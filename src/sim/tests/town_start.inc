// TOWN-START-1: a measured player opening, not an automatic gameplay policy.
// Keep helpers internal, like the Origin and public-command helpers they compose.
namespace {
namespace town_start_test {
constexpr std::uint64_t budget_ticks = 900000; // Owner's 30 game minutes at 500 ticks/second.
constexpr std::uint64_t sample_ticks = 15000;  // Owner's 30 game-second checkpoints.

struct Projection {
    std::string first;
    sim::UpgradeScores scores;
    bool at_cap = false;
};

Projection project(const sim::World& w, const sim::City& city, const sim::Building& b,
                   const sim::House& house) {
    const auto* type = w.types().find(b.type);
    REQUIRE(type);
    const auto group = sim::house_group(w.config(), type->prodtype);
    REQUIRE(group);
    const auto cur = w.types().population_view(*group, w.config());
    REQUIRE(cur);
    Projection p;
    p.at_cap = house.residents_64 == static_cast<std::int32_t>(cur.value().group->wohnmax * 64);
    if (*group + 1 >= w.types().house_groups().size()) {
        p.first = "next_type";
        return p;
    }
    const auto next = w.types().population_view(*group + 1, w.config());
    REQUIRE(next);
    p.scores = sim::upgrade_scores(cur.value(), next.value(), house.e, house.stock);
    if (p.scores.fault != sim::HouseFault::None)
        p.first = std::string("fault:") + sim::house_fault_name(p.scores.fault);
    else if (p.scores.f <= 0)
        p.first = "food";
    else if (p.scores.b <= 0)
        p.first = "goods";
    else if (p.scores.i <= 0)
        p.first = "infrastructure";
    else {
        const auto ladder = sim::house_type_ladder(w.config(), w.types(), *type);
        if (!ladder.next)
            p.first = "next_type";
        else if (!sim::house_materials_available(
                     w.config(), city, sim::house_material_amounts(w.config(), *ladder.next)))
            p.first = "materials";
        else if (sim::house_successor(w.config(), w.types(), *type) != sim::HouseSuccessor::Ladder)
            p.first = "next_type";
        else
            p.first = "eligible";
    }
    return p;
}

struct Result {
    std::string trace;
    std::vector<std::uint64_t> hashes;
    std::uint64_t food_failures = 0, houses_built = 0, first_settler = 0;
    bool diagnostic_distinguished = false;
};

bool touches(int x, int y, const sim::Building& b, const sim::BuildingType& t) {
    const auto [width, height] = sim::footprint(t, b.rotation);
    const auto dx = static_cast<int>(width), dy = static_cast<int>(height);
    return ((x == b.x - 1 || x == b.x + dx) && y >= b.y && y < b.y + dy) ||
           ((y == b.y - 1 || y == b.y + dy) && x >= b.x && x < b.x + dx);
}

struct Opening {
    sim::World& w;
    sim::CityRef ref;
    sim::BuildingId warehouse;
    std::ostringstream log;
    std::uint64_t founded = 0;
    std::vector<sim::BuildingId> houses;
    std::map<std::uint32_t, sim::BuildingId> chain{};
    std::set<std::uint32_t> no_site{};

    sim::ApplyResult apply(const std::string& label, const sim::Command& cmd) {
        const auto answer = w.apply(cmd);
        log << "COMMAND\t" << w.clock().tick() << '\t' << label << '\t'
            << sim::reject_name(answer.status) << '\n';
        return answer;
    }

    bool owned(int x, int y, std::uint32_t width, std::uint32_t height) const {
        const auto& is = *w.islands()[ref.island.v];
        for (std::uint32_t dy = 0; dy < height; ++dy)
            for (std::uint32_t dx = 0; dx < width; ++dx) {
                const auto i =
                    is.tiles.index_of(x + static_cast<int>(dx), y + static_cast<int>(dy));
                if (i == sim::kNoSlot || is.tiles.owner(i) != ref.city.v)
                    return false;
            }
        return true;
    }

    // Layout preference: leave room for roads; not a new placement predicate.
    bool spaced(int x, int y, const sim::BuildingType& type) const {
        const auto& city = command_test::city(w, ref);
        for (const auto& list : city.index.lists)
            for (const auto& slot : list.slots) {
                if (!slot)
                    continue;
                const auto* other = w.types().find(slot->type);
                REQUIRE(other);
                if (other->prodtype == 193) // STRASSE; roads may adjoin buildings.
                    continue;
                const auto [ow, oh] = sim::footprint(*other, slot->rotation);
                const int gap = type.cls == sim::RuntimeClass::House ? 0 : 1;
                if (x < slot->x + static_cast<int>(ow) + gap &&
                    x + static_cast<int>(type.width) + gap > slot->x &&
                    y < slot->y + static_cast<int>(oh) + gap &&
                    y + static_cast<int>(type.height) + gap > slot->y)
                    return false;
            }
        return true;
    }

    void road_to(sim::BuildingId target_id) {
        const auto& is = *w.islands()[ref.island.v];
        const auto* target = w.find_building(target_id);
        const auto* wh = w.find_building(warehouse);
        REQUIRE(target);
        REQUIRE(wh);
        const auto& road = *w.types().find(sim::TypeId{kRoadDirt});
        std::set<std::pair<int, int>> legal;
        for (const auto& p : legal_origins(is, w.types(), road))
            if (owned(p.x, p.y, 1, 1) &&
                site::footprint_occupancy(is.tiles, w.types(), road, {p.x, p.y}, 1, 1) ==
                    site::Compat::Compatible)
                legal.emplace(p.x, p.y);
        // Already built streets are usable nodes but need no second command.
        for (std::uint32_t y = 0; y < is.tiles.height(); ++y)
            for (std::uint32_t x = 0; x < is.tiles.width(); ++x) {
                const int wx = is.tiles.x0() + static_cast<int>(x);
                const int wy = is.tiles.y0() + static_cast<int>(y);
                if (is.tiles.object(is.tiles.index_of(wx, wy)) == kRoadDirt && owned(wx, wy, 1, 1))
                    legal.emplace(wx, wy);
            }
        std::vector<std::pair<int, int>> queue;
        std::map<std::pair<int, int>, std::pair<int, int>> parent;
        for (const auto& xy : legal)
            if (touches(xy.first, xy.second, *wh, *w.types().find(wh->type))) {
                queue.push_back(xy);
                parent[xy] = xy;
            }
        for (std::size_t i = 0; i < queue.size(); ++i) {
            const auto xy = queue[i];
            if (touches(xy.first, xy.second, *target, *w.types().find(target->type))) {
                std::vector<Origin> path;
                for (auto at = xy;; at = parent.at(at)) {
                    if (is.tiles.object(is.tiles.index_of(at.first, at.second)) != kRoadDirt)
                        path.push_back({at.first, at.second});
                    if (parent.at(at) == at)
                        break;
                }
                if (!path.empty())
                    CHECK(apply("roads_to_" + std::to_string(target_id.v), road_drag(0, ref, path))
                              .ok());
                return;
            }
            for (const auto& [dx, dy] : {std::pair{0, -1}, {-1, 0}, {1, 0}, {0, 1}}) {
                const std::pair<int, int> next{xy.first + dx, xy.second + dy};
                if (legal.contains(next) && !parent.contains(next)) {
                    parent[next] = xy;
                    queue.push_back(next);
                }
            }
        }
        log << "NO_ROAD\t" << w.clock().tick() << '\t' << target_id.v << '\n';
    }

    std::optional<sim::BuildingId> build(std::uint32_t id) {
        const auto& is = *w.islands()[ref.island.v];
        const auto* type = w.types().find(sim::TypeId{id});
        REQUIRE(type);
        struct Candidate {
            Origin p;
            std::uint8_t rotation;
            std::uint64_t inputs;
        };
        std::vector<Candidate> origins;
        const auto& city = command_test::city(w, ref);
        if (type->construction.stage != 0 &&
            (city.stage_mask & (std::uint32_t{1} << type->construction.stage)) == 0) {
            log << "WAIT_STAGE\t" << w.clock().tick() << '\t' << id << '\t'
                << type->construction.stage << '\t' << city.stage_mask << '\n';
            return {};
        }
        for (const auto& cost : type->cost_goods) {
            if (city.stock.stock.at(cost.goods) < cost.amount_88) {
                const auto holds = ship_of(w, 1).holds;
                for (std::size_t h = 0; h < holds.size(); ++h) {
                    if (holds[h].goods != cost.goods)
                        continue;
                    const auto have = command_test::city(w, ref).stock.stock.at(cost.goods);
                    if (have >= cost.amount_88)
                        break;
                    const auto tons =
                        std::min<std::uint64_t>(holds[h].amount_88 / 256,
                                                (std::uint64_t{cost.amount_88 - have} + 255) / 256);
                    if (tons == 0)
                        continue;
                    REQUIRE(h <= std::numeric_limits<std::uint32_t>::max());
                    REQUIRE(tons <= std::numeric_limits<std::uint32_t>::max());
                    (void)apply(
                        "resupply_for_" + std::to_string(id),
                        {sim::PlayerId{0},
                         sim::UnloadCmd{sim::FigureId{1}, warehouse, static_cast<std::uint32_t>(h),
                                        cost.goods, static_cast<std::uint32_t>(tons)}});
                }
            }
            if (command_test::city(w, ref).stock.stock.at(cost.goods) < cost.amount_88) {
                log << "WAIT_GOODS\t" << w.clock().tick() << '\t' << id << '\t' << cost.goods
                    << '\t' << command_test::city(w, ref).stock.stock.at(cost.goods) << '/'
                    << cost.amount_88 << '\n';
                return {};
            }
        }
        if (w.players().slots.at(0)->money < type->cost_gold) {
            log << "WAIT_MONEY\t" << w.clock().tick() << '\t' << id << '\n';
            return {};
        }
        if (no_site.contains(id))
            return {};
        for (std::uint8_t rotation = 0; rotation < (id == 810 ? 4 : 1); ++rotation) {
            const auto [fw, fh] = sim::footprint(*type, rotation);
            for (std::int32_t y = is.tiles.y0();
                 y < is.tiles.y0() + static_cast<int>(is.tiles.height()); ++y)
                for (std::int32_t x = is.tiles.x0();
                     x < is.tiles.x0() + static_cast<int>(is.tiles.width()); ++x) {
                    if (!owned(x, y, fw, fh) || !spaced(x, y, *type))
                        continue;
                    if (id == 801 && (x != 238 || y != 128))
                        continue; // Previously reviewed site.
                    bool road_overlap = false;
                    for (std::uint32_t dy = 0; dy < fh; ++dy)
                        for (std::uint32_t dx = 0; dx < fw; ++dx)
                            road_overlap |=
                                is.tiles.object(is.tiles.index_of(x + static_cast<int>(dx),
                                                                  y + static_cast<int>(dy))) ==
                                kRoadDirt;
                    if (road_overlap)
                        continue;
                    sim::PlacementRecord rec;
                    rec.type = type->id;
                    rec.x = x;
                    rec.y = y;
                    rec.rotation = rotation;
                    if (sim::placement_rule(is, w.types(), *type, rec).reject != sim::Reject::None)
                        continue;
                    std::uint64_t inputs = 0;
                    if (id == 810) {
                        sim::Building probe;
                        probe.type = type->id;
                        probe.x = x;
                        probe.y = y;
                        probe.rotation = rotation;
                        for (std::size_t k = 0; k < type->entrances.size(); ++k) {
                            const auto at = sim::entrance_position(probe, *type, k);
                            const auto lx = (at.x >> 8) - is.tiles.x0(),
                                       ly = (at.y >> 8) - is.tiles.y0();
                            if (lx < 0 || ly < 0)
                                continue;
                            const auto cell = sim::fisher_cell_at(
                                is.terrain->height, is.tiles.width(), is.tiles.height(),
                                static_cast<std::uint32_t>(lx), static_cast<std::uint32_t>(ly));
                            inputs += cell && cell->walkable_water;
                        }
                        if (inputs == 0)
                            continue;
                    } else if ((type->cls == sim::RuntimeClass::SmallFarm ||
                                type->cls == sim::RuntimeClass::Farm) &&
                               id != 801) {
                        const auto radius = static_cast<int>(type->action_radius);
                        const auto cx = x + static_cast<int>((fw - 1) / 2),
                                   cy = y + static_cast<int>((fh - 1) / 2);
                        for (int sy = cy - radius; sy <= cy + radius; ++sy)
                            for (int sx = cx - radius; sx <= cx + radius; ++sx) {
                                const auto obj = is.tiles.object(is.tiles.index_of(sx, sy));
                                const auto* raw = w.types().find(sim::TypeId{obj});
                                if (!raw)
                                    continue;
                                for (auto input : type->input_goods)
                                    inputs += std::find(raw->output_goods.begin(),
                                                        raw->output_goods.end(),
                                                        input) != raw->output_goods.end();
                            }
                    }
                    origins.push_back({{x, y}, rotation, inputs});
                }
        }
        const auto* wh = w.find_building(warehouse);
        REQUIRE(wh);
        std::sort(origins.begin(), origins.end(), [&](const auto& a, const auto& b) {
            return std::tuple{std::numeric_limits<std::uint64_t>::max() - a.inputs,
                              std::abs(a.p.x - wh->x) + std::abs(a.p.y - wh->y), a.p.y, a.p.x,
                              a.rotation} <
                   std::tuple{std::numeric_limits<std::uint64_t>::max() - b.inputs,
                              std::abs(b.p.x - wh->x) + std::abs(b.p.y - wh->y), b.p.y, b.p.x,
                              b.rotation};
        });
        for (const auto& candidate : origins) {
            const auto& p = candidate.p;
            log << "BUILD_SITE\t" << id << '\t' << p.x << '\t' << p.y << '\t'
                << static_cast<unsigned>(candidate.rotation) << "\tinput_cells=" << candidate.inputs
                << '\n';
            const auto a =
                apply("build_" + std::to_string(id),
                      command_test::build(
                          0, ref, {command_test::at(type->id, p.x, p.y, candidate.rotation)}));
            if (!a.ok()) {
                log << "BUILD_REFUSAL_STOCK\t" << id;
                const auto& c = command_test::city(w, ref);
                for (const auto& cost : type->cost_goods)
                    log << '\t' << cost.goods << ':' << c.stock.stock.at(cost.goods) << '/'
                        << cost.amount_88;
                log << '\n';
                return {};
            }
            REQUIRE(a.records.size() == 1);
            const auto result = a.records[0].building;
            road_to(result);
            return result;
        }
        no_site.insert(id);
        log << "NO_SITE\t" << w.clock().tick() << '\t' << id << '\n';
        return {};
    }

    void sample(Result& result) {
        const auto& city = command_test::city(w, ref);
        std::vector<std::uint64_t> groups(w.types().house_groups().size());
        std::int64_t residents64 = 0;
        for (const auto& list : city.index.lists)
            for (const auto& b : list.slots) {
                const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                if (!h)
                    continue;
                const auto* type = w.types().find(b->type);
                const auto group = sim::house_group(w.config(), type->prodtype);
                REQUIRE(group);
                ++groups.at(*group);
                residents64 += h->residents_64;
                const auto p = project(w, city, *b, *h);
                result.food_failures += p.first == "food";
                if (*group > 0 && result.first_settler == 0)
                    result.first_settler = w.clock().tick();
                log << "HOUSE\t" << w.clock().tick() - founded << '\t' << b->id.v << '\t'
                    << b->type.v << '\t' << *group << '\t' << h->residents_64 << '\t' << p.at_cap
                    << '\t' << p.first << '\t' << p.scores.food << '\t' << p.scores.mean << '\t'
                    << p.scores.infra << '\t';
                const auto view = w.types().population_view(*group, w.config());
                REQUIRE(view);
                for (std::size_t k = 0; k < h->stock.size(); ++k)
                    log << view.value().group->needs.at(k).ware << ':' << h->stock[k] << ',';
                log << '\n';
            }
        for (const auto& list : city.index.lists)
            for (const auto& b : list.slots) {
                if (!b)
                    continue;
                log << "PRODUCER\t" << w.clock().tick() - founded << '\t' << b->id.v << '\t'
                    << b->type.v;
                if (const auto* f = std::get_if<sim::Farm>(&b->payload))
                    log << "\traw=" << f->raw << "\tout=" << f->out
                        << "\tworkers=" << f->workers_out;
                if (const auto* f = std::get_if<sim::SmallFarm>(&b->payload)) {
                    log << "\traw=";
                    for (auto n : f->raw)
                        log << n << ',';
                    log << "\tout=";
                    for (auto n : f->out)
                        log << n << ',';
                }
                if (const auto* f = std::get_if<sim::Workshop>(&b->payload)) {
                    log << "\tin=";
                    for (auto n : f->in)
                        log << n << ',';
                    log << "\tout=";
                    for (auto n : f->out)
                        log << n << ',';
                }
                log << '\n';
            }
        log << "SAMPLE\t" << w.clock().tick() - founded << '\t' << w.clock().tick() << '\t'
            << residents64 << '\t' << w.players().slots.at(0)->money << '\t';
        for (auto n : groups)
            log << n << ',';
        for (const char* name : {"WERKZEUG", "HOLZ", "ZIEGEL", "NAHRUNG"}) {
            const auto g = w.types().goods_by_name(name);
            REQUIRE(g);
            log << '\t' << city.stock.stock.at(*g);
        }
        const auto hash = w.state_hash();
        result.hashes.push_back(hash);
        log << '\t' << hash << '\n';
    }
};
} // namespace town_start_test

namespace town_start_test {
Result run(const RealStart& rs, bool food_chain, const fs::path& target) {
    REQUIRE_FALSE(fs::exists(target));
    sim::Rules rules = rs.rules;
    rules.house_updates = true;
    rules.city_booking = true;
    sim::ExtensionContext extensions;
    extensions.fisher_rules.enabled = 1;
    auto created = sl::load_ship_start(rs.manifest, rs.dir, rs.types, {}, rules,
                                       sim::serial_executor(), {}, extensions);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string{} : created.error()));
    auto w = std::move(created).value();
    REQUIRE(w.fisher_enabled());
    REQUIRE(w.rules().house_updates);
    REQUIRE(w.rules().city_booking);
    REQUIRE(sl::expedition_baseline_problem(w).empty());
    std::ostringstream prefix;
    prefix << "META\tcandidate=lan5-ship-a\tseat=0\tseed=" << rs.manifest.seed
           << "\thouse_updates=1\tcity_booking=1\tfisher_rules=1\tfood_chain=" << food_chain
           << "\tbudget_ticks=" << budget_ticks << '\n';
    for (const auto id : {501U, 521U, 809U, 810U, 801U, 807U, 1001U, 1335U, 1301U, 1201U, 804U,
                          1203U, 1022U, 1310U, 1314U, 1320U, 2520U}) {
        const auto* t = w.types().find(sim::TypeId{id});
        REQUIRE(t);
        prefix << "TYPE\t" << id << "\tclass=" << static_cast<unsigned>(t->cls)
               << "\tstage=" << t->construction.stage << "\tresearch=" << t->research_id
               << "\tgold=" << t->cost_gold << "\tcosts=";
        for (const auto& cost : t->cost_goods)
            prefix << cost.goods << ':' << cost.amount_88 << ',';
        prefix << '\n';
    }
    for (std::uint32_t g = 0; g < 2; ++g) {
        const auto v = w.types().population_view(g, w.config());
        REQUIRE(v);
        for (const auto& n : v.value().group->needs)
            prefix << "NEED\t" << g << '\t' << w.types().goods_table()[n.ware].name << '\t'
                   << n.force << '\t' << n.menge << '\t'
                   << w.types().goods_table()[n.ware].bau_first << '\n';
    }
    std::uint64_t all_fish = 0, all_usable = 0, home_fish = 0, home_usable = 0;
    for (const auto& island : w.islands()) {
        if (!island || !island->terrain)
            continue;
        for (std::uint32_t y = 0; y < island->tiles.height(); ++y)
            for (std::uint32_t x = 0; x < island->tiles.width(); ++x) {
                const auto slot = std::uint64_t{y} * island->tiles.width() + x;
                if (island->tiles.object(slot) != 11)
                    continue;
                const auto cell = sim::fisher_cell_at(
                    island->terrain->height, island->tiles.width(), island->tiles.height(), x, y);
                REQUIRE(cell);
                ++all_fish;
                all_usable += cell->usable_mask != 0;
                if (island->index.v == 0) {
                    ++home_fish;
                    home_usable += cell->usable_mask != 0;
                }
            }
    }
    prefix << "FISH\tall=" << all_fish << "\tusable=" << all_usable << "\tisland0=" << home_fish
           << "\tusable0=" << home_usable << '\n';
    const auto leg = expansion_test::selected_recipe().source;
    const auto sail = w.apply(sail_command(leg));
    prefix << "COMMAND\t" << w.clock().tick() << "\tsail\t" << sim::reject_name(sail.status)
           << '\n';
    REQUIRE_MESSAGE(sim::test::write_evidence_file(target, prefix.str()).empty(), target.string());
    REQUIRE_MESSAGE(sail.ok(), sim::reject_name(sail.status));
    bool arrived = false;
    // Operator's 10-game-minute voyage bound, not a ship/navigation limit.
    while (!arrived && w.clock().tick() < 300000) {
        w.advance(100);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        for (const auto& e : w.drain_events())
            if (e.figure == sim::FigureId{1}) {
                REQUIRE(e.kind != sim::SimEventKind::ShipRouteFailed);
                arrived = arrived || e.kind == sim::SimEventKind::ShipArrived;
            }
    }
    REQUIRE_MESSAGE(arrived, "public voyage did not arrive inside declared opening bound");
    const auto founded = w.apply(found_command(leg));
    prefix << "COMMAND\t" << w.clock().tick() << "\tfound\t" << sim::reject_name(founded.status)
           << '\n';
    REQUIRE_MESSAGE(sim::test::write_evidence_file(target, prefix.str()).empty(), target.string());
    REQUIRE_MESSAGE(founded.ok(), sim::reject_name(founded.status));
    REQUIRE(founded.records.size() == 1);
    const auto wh = founded.records[0].building;
    const auto ref = expansion_test::city_of_warehouse(w, wh);
    Opening opening{w, ref, wh, {}, w.clock().tick(), {}};
    opening.log << prefix.str();
    Result result;
    opening.sample(result);                 // The founding checkpoint, before any manual unloading.
    const auto holds = ship_of(w, 1).holds; // Snapshot of actual post-founding cargo.
    for (std::size_t i = 0; i < holds.size(); ++i) {
        REQUIRE(i <= std::numeric_limits<std::uint32_t>::max());
        const auto tons = holds[i].amount_88 / 256;
        REQUIRE(tons <= std::numeric_limits<std::uint32_t>::max());
        if (tons == 0)
            continue;
        const auto a = opening.apply(
            "unload_" + std::to_string(i) + "_" + std::to_string(tons),
            {sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, wh, static_cast<std::uint32_t>(i),
                                              holds[i].goods, static_cast<std::uint32_t>(tons)}});
        opening.log << "TRANSFER\t" << i << "\trequested_tons=" << tons
                    << "\tstatus=" << sim::reject_name(a.status);
        if (!a.records.empty())
            opening.log << "\traw=" << a.records[0].transferred_88;
        opening.log << '\n';
    }
    const auto* chapel = w.types().find(sim::TypeId{1320});
    REQUIRE(chapel);
    const auto stage =
        std::find_if(w.types().stage_table().begin(), w.types().stage_table().end(),
                     [&](const auto& r) { return r.stage == chapel->construction.stage; });
    REQUIRE(stage != w.types().stage_table().end());
    REQUIRE(stage->group == 0);
    const auto pioneer = w.types().population_view(0, w.config());
    REQUIRE(pioneer);
    const auto capacity = pioneer.value().group->wohnmax;
    REQUIRE(capacity > 0);
    const std::uint64_t target_houses = (std::uint64_t{stage->count} + capacity - 1) / capacity;
    opening.log << "POPULATION_TARGET\tstage=" << stage->stage << "\tpeople=" << stage->count
                << "\tpeople_per_house=" << capacity << "\thouses=" << target_houses
                << "\tunit=whole_residents_from_residents_64_div_64\n";
    const auto maintain = [&] {
        const auto once = [&](std::uint32_t id) {
            if (opening.chain.contains(id))
                return;
            if (auto b = opening.build(id))
                opening.chain[id] = *b;
        };
        once(801);
        once(1310);
        once(1314);
        once(1301);
        if (food_chain) {
            once(809);
            once(810);
        }
        while (opening.houses.size() < target_houses) {
            const auto id = opening.build(501);
            if (!id)
                break;
            opening.houses.push_back(*id);
        }
        for (auto id : {807U, 1001U, 1335U, 1201U, 804U, 1203U, 1022U, 1320U})
            once(id);
    };
    maintain();
    result.houses_built = opening.houses.size();
    if (opening.houses.empty())
        opening.log << "PRE_HOUSE_BLOCKER\tno house was built; no house-growth claim\n";
    // Distinguish the readonly diagnostic on a local House value, never on live world stock.
    if (!opening.houses.empty()) {
        const auto* building = w.find_building(opening.houses.front());
        REQUIRE(building);
        const auto cur = w.types().population_view(0, w.config());
        const auto next = w.types().population_view(1, w.config());
        REQUIRE(cur);
        REQUIRE(next);
        sim::House supplied;
        supplied.stock.resize(cur.value().group->needs.size());
        for (std::size_t i = 0; i < supplied.stock.size(); ++i) {
            const auto menge =
                i == 0 ? next.value().group->needs[0].menge : cur.value().group->needs[i].menge;
            const auto raw = (std::uint64_t{supplied.e} * menge * 128 + 1535) / 1536;
            REQUIRE(raw <= std::numeric_limits<std::uint32_t>::max());
            supplied.stock[i] = static_cast<std::uint32_t>(raw);
        }
        const auto before = w.state_hash();
        const auto fed = project(w, command_test::city(w, ref), *building, supplied);
        supplied.stock[0] = 0; // Pure diagnostic counterexample, no World mutation.
        const auto starved = project(w, command_test::city(w, ref), *building, supplied);
        REQUIRE(fed.scores.f > 0);
        REQUIRE(fed.first != "food");
        REQUIRE(starved.first == "food");
        REQUIRE(w.state_hash() == before);
        result.diagnostic_distinguished = true;
        opening.log << "DIAGNOSTIC_CONTROL\tfood_full=" << fed.scores.food
                    << "\tfood_zero=" << starved.scores.food << "\tfirst_zero=" << starved.first
                    << "\tworld_unchanged=1\n";
    }
    const auto publish = [&] {
        const auto error = sim::test::write_evidence_file(target, opening.log.str());
        REQUIRE_MESSAGE(error.empty(), error);
    };
    publish();
    const auto end = opening.founded + budget_ticks;
    std::uint64_t next_sample = opening.founded + sample_ticks;
    while (w.clock().tick() < end) {
        w.advance(100);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        (void)w.drain_events();
        if (result.first_settler == 0)
            for (const auto id : opening.houses) {
                const auto* b = w.find_building(id);
                if (!b || !std::holds_alternative<sim::House>(b->payload))
                    continue;
                const auto group = sim::house_group(w.config(), w.types().find(b->type)->prodtype);
                if (group && *group > 0)
                    result.first_settler = w.clock().tick();
            }
        if (w.clock().tick() == next_sample) {
            maintain();
            result.houses_built = opening.houses.size();
            opening.sample(result);
            publish();
            next_sample += sample_ticks;
        }
    }
    REQUIRE(w.clock().tick() == end);
    REQUIRE(result.hashes.size() == budget_ticks / sample_ticks + 1);
    REQUIRE(w.check_invariants().empty());
    opening.log << "END\thouses_built=" << result.houses_built
                << "\tfood_failure_house_samples=" << result.food_failures
                << "\tfirst_observed_settler_tick=" << result.first_settler
                << "\tobservation_step_ticks=50\n";
    publish();
    result.trace = opening.log.str();
    return result;
}
} // namespace town_start_test

TEST_CASE(
    "town start measurement: public ship opening, same-seed traces and food control (opt-in)" *
    doctest::test_suite("heavy")) {
    const char* enabled = std::getenv("ANNO_TOWN_START");
    if (enabled == nullptr || std::string_view(enabled) != "1") {
        MESSAGE("skipping: opt-in measurement; set ANNO_TOWN_START=1");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto rs = real_start(*dir);
    const char* output = std::getenv("ANNO_TOWN_START_OUT");
    const fs::path root = output ? fs::path(output) : sim::test::make_private_dir("town_start");
    REQUIRE_FALSE(root.empty());
    std::error_code ec;
    fs::create_directories(root, ec);
    REQUIRE_FALSE(ec);
    const auto a = town_start_test::run(rs, true, root / "baseline-a.tsv");
    const auto b = town_start_test::run(rs, true, root / "baseline-b.tsv");
    REQUIRE(a.hashes == b.hashes);
    REQUIRE(a.trace == b.trace);
    const auto control = town_start_test::run(rs, false, root / "without-food-chain.tsv");
    std::ostringstream summary;
    summary << "TOWN_RESULT\tdeterminism=equal\tcheckpoint_hashes=" << a.hashes.size()
            << "\thouses_built=" << a.houses_built
            << "\tfirst_observed_settler_tick=" << a.first_settler
            << "\tfood_failures=" << a.food_failures
            << "\twithout_food_chain_failures=" << control.food_failures << "\tfood_effect="
            << (control.food_failures > a.food_failures ? "observed" : "inconclusive")
            << "\tdiagnostic_distinguishes=" << a.diagnostic_distinguished << '\n';
    const auto error = sim::test::write_evidence_file(root / "summary.tsv", summary.str());
    REQUIRE_MESSAGE(error.empty(), error);
    std::cout << summary.str() << std::flush;
    REQUIRE(std::cout.good());
}

} // namespace
