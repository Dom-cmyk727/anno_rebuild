// Included in founding_assets_test.cpp to reuse the accepted unchanged-original recipe.
namespace freight_original {
void publish(const char* name, const std::vector<std::uint8_t>& bytes) {
    if (const char* dir = std::getenv("ANNO_FREIGHT_OUT_DIR"); dir && *dir) {
        const auto path = fs::path(dir) / name;
        const auto error = export_log_if_requested(path.string().c_str(), bytes);
        REQUIRE_MESSAGE(error.empty(), error);
    }
}
void text(const char* name, const std::string& s) {
    publish(name, std::vector<std::uint8_t>(s.begin(), s.end()));
}
} // namespace freight_original

TEST_CASE("freight original voyage: produced wood funds two automatic cycles and receiving build") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    using D = sim::FreightDirection;
    using P = sim::FreightPhase;
    using S = sim::FreightSignal;
    const auto started = std::chrono::steady_clock::now();
    auto rs = real_start(*dir);
    rs.rules.city_booking = true;
    rs.rules.save_billed_goods = true;
    auto made = sl::load_ship_start(rs.manifest, *dir, rs.types, {}, rs.rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string{} : made.error()));
    auto w = std::move(made).value();
    REQUIRE(sl::expedition_baseline_problem(w).empty());
    REQUIRE_FALSE(w.rules().house_updates);
    const auto recipe = expansion_test::selected_recipe();
    const auto wood = *rs.types->goods_by_name("HOLZ");
    const auto tools = *rs.types->goods_by_name("WERKZEUG");
    const auto initial_holds = ship_of(w, 1).holds;
    const auto initial_money = expansion_test::balances(w);
    const auto cargo_city_totals = [](const sim::World& world) {
        std::map<std::pair<std::uint32_t, sim::GoodsId>, std::uint64_t> result;
        for (const auto& figure : world.figures().list.slots)
            if (figure)
                for (const auto& hold : figure->holds)
                    if (hold.amount_88)
                        result[{figure->owner.v, hold.goods}] += hold.amount_88;
        for (const auto& island : world.islands())
            if (island)
                for (const auto& city : island->cities)
                    if (city)
                        for (std::size_t g = 0; g < city->stock.stock.size(); ++g)
                            if (city->stock.stock[g])
                                result[{city->owner.v, static_cast<sim::GoodsId>(g)}] +=
                                    city->stock.stock[g];
        return result;
    };
    const auto initial_goods = cargo_city_totals(w);
    const auto wood_key = std::pair{0U, wood}, tools_key = std::pair{0U, tools};
    auto recording = sim::Recorder::start(w);
    REQUIRE(recording.ok());
    auto rec = std::move(recording).value();
    expansion_test::Run run{w, rec, {}, {}, {}, {}, {}, 0};
    run.sink = freight_original::publish;
    expansion_test::sail(run, recipe.source);
    REQUIRE(ship_of(w, 1).holds == initial_holds);
    auto first_found = run.apply(found_command(recipe.source), 350);
    REQUIRE(first_found.ok());
    const auto source_wh = first_found.records.at(0).building;
    const auto source = expansion_test::city_of_warehouse(w, source_wh);
    REQUIRE(run.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, source_wh, 2, tools, 2}})
                .ok());
    const auto forest_result =
        run.apply(command_test::build(0, source,
                                      {command_test::at(sim::TypeId{801}, recipe.forest.site.x,
                                                        recipe.forest.site.y)}),
                  150);
    REQUIRE(forest_result.ok());
    const auto forest = forest_result.records.at(0).building;
    const auto road =
        road_path(w, source, *rs.types->find(sim::TypeId{2520}), *w.find_building(source_wh),
                  *rs.types->find(sim::TypeId{1302}), recipe.forest.site);
    REQUIRE(road);
    REQUIRE(road->size() == 20);
    REQUIRE(run.apply(road_drag(0, source, *road), 100).ok());
    const auto& empty_farm = std::get<sim::Farm>(w.find_building(forest)->payload);
    REQUIRE(empty_farm.raw == 0);
    REQUIRE(empty_farm.out == 0);
    REQUIRE(empty_farm.workers_out == 0);
    const auto preproduction = w.save();
    const auto control_start_entry = rec.log().entries.size();
    freight_original::publish("pre-production.save", preproduction);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    std::uint64_t exported = 0, received = 0, wood_spent = 12 * 256, tools_spent = 7 * 256;
    std::optional<sim::CityRef> destination;
    sim::BuildingId destination_wh;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> loads, unloads;
    const auto square =
        sim::search_area(*w.find_building(forest), *rs.types->find(sim::TypeId{801})).square();
    std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> objects;
    for (std::int32_t y = square.y0; y < square.y0 + static_cast<std::int32_t>(square.h); ++y)
        for (std::int32_t x = square.x0; x < square.x0 + static_cast<std::int32_t>(square.w); ++x) {
            const auto& island = *w.islands()[source.island.v];
            const auto i = island.tile_index(x, y);
            if (i != sim::kNoSlot)
                objects[{x, y}] = island.tiles.object(i);
        }
    std::uint64_t observed_cuts = 0, observed_growth = 0;
    std::ostringstream trace;
    trace << "tick signal generation order warehouse stop action direction goods actual phase "
             "cycles\n";
    const auto made_wood = [&]() -> std::uint64_t {
        const auto it = stats.forestry.farms.find(forest);
        return it == stats.forestry.farms.end() ? 0 : it->second.made_88;
    };
    const auto ledger = [&] {
        expansion_test::source_ledger(w, source, source_wh, forest, wood, stats, exported);
        for (auto& [xy, before] : objects) {
            const auto& island = *w.islands()[source.island.v];
            const auto now = island.tiles.object(island.tile_index(xy.first, xy.second));
            if (now == before)
                continue;
            if (now + 1 == before && sim::harvest_cuttable(w.types(), before))
                ++observed_cuts;
            else if (now == before + 1 && w.types().find_growing(before))
                ++observed_growth;
            else
                FAIL("unexplained original source-tree transition");
            before = now;
        }
        REQUIRE(observed_cuts == stats.forestry.cuts);
        auto held_and_stored = cargo_city_totals(w);
        for (const auto& island : w.islands())
            if (island)
                for (const auto& city : island->cities)
                    if (city) {
                        REQUIRE(city->accounts.military_cost == 0);
                        REQUIRE(std::all_of(city->billed.begin(), city->billed.end(),
                                            [](auto x) { return x == 0; }));
                        REQUIRE(city->accounts.sales == 0);
                        REQUIRE(city->accounts.trade_sales == 0);
                        REQUIRE(city->accounts.trade_purchases == 0);
                    }
        const auto& farm = std::get<sim::Farm>(w.find_building(forest)->payload);
        held_and_stored[wood_key] += farm.out + expansion_test::cart_wood(w, source_wh, wood);
        auto expected_goods = initial_goods;
        expected_goods[wood_key] += made_wood();
        REQUIRE(expected_goods[wood_key] >= wood_spent);
        expected_goods[wood_key] -= wood_spent;
        REQUIRE(expected_goods[tools_key] >= tools_spent);
        expected_goods[tools_key] -= tools_spent;
        std::erase_if(held_and_stored, [](const auto& row) { return row.second == 0; });
        std::erase_if(expected_goods, [](const auto& row) { return row.second == 0; });
        REQUIRE(held_and_stored == expected_goods);
        if (destination)
            REQUIRE(expansion_test::stock(w, *destination, wood) ==
                    received - (wood_spent == 25 * 256 ? 256 : 0));
        for (const auto& [owner, cash] : initial_money)
            REQUIRE(expansion_test::balances(w).at(owner) ==
                    cash - static_cast<std::int64_t>(run.construction[owner] + run.upkeep[owner]) +
                        static_cast<std::int64_t>(run.income[owner]));
    };
    run.source_check = [&] {
        for (const auto& e : run.events)
            if (e.freight) {
                const auto& f = *e.freight;
                trace << e.tick << " " << static_cast<unsigned>(f.signal) << " " << f.generation
                      << " " << e.order_id << " " << f.warehouse.v << " " << f.stop << " "
                      << f.action << " " << static_cast<unsigned>(f.direction) << " " << f.goods
                      << " " << f.amount_88 << " " << static_cast<unsigned>(f.phase) << " "
                      << f.completed_cycles << "\n";
            }
        for (const auto& e : run.events)
            if (e.freight && e.freight->signal == S::Transferred) {
                const auto& f = *e.freight;
                REQUIRE(e.figure == sim::FigureId{1});
                REQUIRE(f.goods == wood);
                if (f.direction == D::LoadTarget) {
                    REQUIRE(f.warehouse == source_wh);
                    exported += f.amount_88;
                    loads.emplace_back(w.clock().tick(), f.amount_88);
                } else {
                    REQUIRE(f.warehouse == destination_wh);
                    received += f.amount_88;
                    unloads.emplace_back(w.clock().tick(), f.amount_88);
                }
            }
        ledger();
    };
    unsigned initial_waits = 0;
    for (; initial_waits < 400; ++initial_waits) {
        run.advance();
        const auto amount = expansion_test::stock(w, source, wood);
        if (amount >= 256 && amount % 256) {
            ++initial_waits;
            break;
        }
    }
    REQUIRE(initial_waits < 400);
    REQUIRE(expansion_test::stock(w, source, wood) >= 256);
    REQUIRE(expansion_test::stock(w, source, wood) % 256 != 0);
    const sim::Command off{
        sim::PlayerId{0},
        sim::SetProductionLineCmd{{recipe.forest.site.x, recipe.forest.site.y}, wood, false}};
    auto on = off;
    std::get<sim::SetProductionLineCmd>(on.body).on = true;
    REQUIRE(command_test::city(w, source).accounts.civil_cost == 27);
    REQUIRE(run.apply(off, 0, source).ok());
    REQUIRE(command_test::city(w, source).accounts.civil_cost == 19);
    expansion_test::sail(run, recipe.destination);
    const auto second_found = run.apply(found_command(recipe.destination), 350);
    REQUIRE(second_found.ok());
    wood_spent += 12 * 256;
    tools_spent += 5 * 256;
    destination_wh = second_found.records.at(0).building;
    destination = expansion_test::city_of_warehouse(w, destination_wh);
    REQUIRE(destination->island != source.island);
    REQUIRE(
        run.apply({sim::PlayerId{0}, sim::UnloadCmd{sim::FigureId{1}, destination_wh, 2, tools, 1}})
            .ok());
    const auto receiver = command_test::build(
        0, *destination,
        {command_test::at(sim::TypeId{1310}, recipe.receiver.x, recipe.receiver.y)});
    const auto refused = w.save();
    REQUIRE(run.apply(receiver).status == sim::Reject::CannotAfford);
    REQUIRE(w.save() == refused);
    expansion_test::sail(run, recipe.source);
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) == 26 * 256);
    REQUIRE(run.apply(on, 0, source).ok());
    REQUIRE(command_test::city(w, source).accounts.civil_cost == 27);
    unsigned collection_waits = 0;
    for (; collection_waits < 60 && expansion_test::stock(w, source, wood) < 768;
         ++collection_waits)
        run.advance();
    REQUIRE(collection_waits < 60);
    REQUIRE(expansion_test::stock(w, source, wood) >= 768);
    REQUIRE(made_wood() < 1536);
    const auto pre_assign = w.save();
    const auto assign_entry = rec.log().entries.size();
    sim::AssignFreightRouteCmd route{sim::FigureId{1},
                                     {{source_wh,
                                       recipe.source.goal_x * 256 + 128,
                                       recipe.source.goal_y * 256 + 128,
                                       {{D::LoadTarget, wood, 29}}},
                                      {destination_wh,
                                       recipe.destination.goal_x * 256 + 128,
                                       recipe.destination.goal_y * 256 + 128,
                                       {{D::UnloadAmount, wood, 3}}}}};
    REQUIRE(run.apply({sim::PlayerId{0}, route}).ok());
    run.checkpoint();
    const auto assignment_generation = ship_of(w, 1).freight_generation;
    std::uint64_t first_load_made = 0;
    std::set<unsigned> cuts;
    std::optional<std::vector<std::uint8_t>> suspend_start;
    unsigned automatic_advances = 0;
    for (; automatic_advances < 6000; ++automatic_advances) {
        const auto position = ship_of(w, 1).pos;
        const auto stamp = ship_of(w, 1).nav->last;
        const auto before_phase = w.freight_route(sim::FigureId{1})->phase;
        const auto pending = w.freight_route(sim::FigureId{1})->expected_order_id;
        const auto before_loads = loads.size(), before_unloads = unloads.size();
        run.advance(); // No commands until the uninterrupted two-cycle interval ends.
        const auto* r = w.freight_route(sim::FigureId{1});
        REQUIRE(r != nullptr);
        REQUIRE(r->status == sim::FreightStatus::Running);
        REQUIRE(ship_of(w, 1).freight_generation == assignment_generation);
        const auto& ship = ship_of(w, 1);
        const auto elapsed = static_cast<std::uint16_t>(ship.nav->last - stamp);
        const auto speed = rs.types->find_figure_type(ship.kind)->nav->speed_q32;
        const std::uint64_t numerator = std::uint64_t{elapsed} * speed * 256;
        const std::uint64_t denominator = 5ULL * (1ULL << 32);
        const auto maximum =
            static_cast<std::int64_t>((numerator + denominator - 1) / denominator + 1);
        REQUIRE(std::abs(std::int64_t{ship.pos.x} - position.x) <= maximum);
        REQUIRE(std::abs(std::int64_t{ship.pos.y} - position.y) <= maximum);
        REQUIRE(ship_of(w, 2).parent == sim::FigureId{1});
        REQUIRE(ship_of(w, 3).parent == sim::FigureId{2});
        REQUIRE(ship_of(w, 2).pos == ship.pos);
        REQUIRE(ship_of(w, 3).pos == ship.pos);
        bool arrived = false;
        for (const auto& e : run.events)
            if (e.figure == ship.id && e.order_id == pending) {
                REQUIRE(e.kind != sim::SimEventKind::ShipRouteFailed);
                if (e.kind == sim::SimEventKind::ShipArrived)
                    arrived = true;
            }
        if (arrived) {
            REQUIRE(before_phase == P::Sailing);
            REQUIRE(r->phase == P::Wait);
            REQUIRE(r->wait_remaining == rs.types->find_figure_type(ship.kind)->load_ticks);
            REQUIRE(r->credit_ticks == 0);
            REQUIRE(loads.size() == before_loads);
            REQUIRE(unloads.size() == before_unloads);
        }
        if (loads.size() == 1 && first_load_made == 0) {
            REQUIRE(loads[0].second == 768);
            first_load_made = made_wood();
            REQUIRE(first_load_made >= 768);
            REQUIRE(first_load_made < 1536);
        }
        const auto cut = [&](unsigned number, const char* label) {
            if (cuts.insert(number).second)
                run.native_cut(label, rs.types);
        };
        if (cuts.empty() && r->phase == P::Sailing && r->stop_index == 0)
            cut(1, "01-source-sailing");
        if (arrived && r->stop_index == 0 && loads.empty())
            cut(2, "02-source-arrival-wait");
        if (r->phase == P::Sailing && r->stop_index == 1 && loads.size() == 1 && unloads.empty()) {
            REQUIRE(aboard_of(ship.holds, wood) == 29 * 256);
            cut(3, "03-loaded-outbound");
            if (!suspend_start)
                suspend_start = w.save();
        }
        if (arrived && r->stop_index == 1 && unloads.empty())
            cut(4, "04-destination-arrival-wait");
        if (r->phase == P::Sailing && r->stop_index == 0 && unloads.size() == 1) {
            REQUIRE(aboard_of(ship.holds, wood) == 26 * 256);
            cut(5, "05-first-delivery-return");
        }
        if (r->completed_cycles >= 2) {
            ++automatic_advances;
            break;
        }
    }
    REQUIRE(automatic_advances < 6000);
    REQUIRE(cuts.size() == 5);
    REQUIRE(loads.size() == 2);
    REQUIRE(unloads.size() == 2);
    REQUIRE(exported == 1536);
    REQUIRE(received == 1536);
    for (const auto& [tick, amount] : loads) {
        REQUIRE(tick > 0);
        REQUIRE(amount == 768);
    }
    for (const auto& [tick, amount] : unloads) {
        REQUIRE(tick > 0);
        REQUIRE(amount == 768);
    }
    REQUIRE(loads[0].first < unloads[0].first);
    REQUIRE(unloads[0].first < loads[1].first);
    REQUIRE(loads[1].first < unloads[1].first);
    REQUIRE(made_wood() - first_load_made >= 1536 - first_load_made);
    REQUIRE(aboard_of(ship_of(w, 1).holds, wood) == 26 * 256);
    run.checkpoint();
    REQUIRE(run.apply(receiver, 50).ok());
    wood_spent += 256;
    tools_spent += 256;
    ledger();
    auto expected = initial_holds;
    expected[0].amount_88 = 26 * 256;
    expected[2].amount_88 = 37 * 256;
    REQUIRE(ship_of(w, 1).holds == expected);
    REQUIRE(expansion_test::stock(w, *destination, wood) == 1280);
    REQUIRE(expansion_test::stock(w, *destination, tools) == 0);
    REQUIRE(run.construction.at(0) == 1000);
    REQUIRE(command_test::city(w, source).accounts.civil_cost == 27);
    REQUIRE(command_test::city(w, *destination).accounts.civil_cost == 20);
    REQUIRE(w.check_invariants().empty());
    run.checkpoint();
    const auto final = w.save();
    freight_original::publish("two-cycles.save", final);
    auto honest = sim::write_log(rec.log());
    REQUIRE(honest.ok());
    freight_original::publish("freight.log", honest.value());
    auto changed = rec.log();
    auto& changed_route =
        std::get<sim::AssignFreightRouteCmd>(changed.entries.at(assign_entry).commands.at(0).body);
    changed_route.stops[0].actions[0].tons = 28;
    auto changed_world = sim::World::load(pre_assign, rs.types);
    REQUIRE(changed_world.ok());
    REQUIRE(changed_world.value().apply({sim::PlayerId{0}, changed_route}).ok());
    REQUIRE(changed_world.value().state_hash() != rec.log().entries.at(assign_entry + 1).hash);
    REQUIRE(changed.entries.at(assign_entry + 1).kind == sim::ReplayEntry::Kind::Checkpoint);
    auto changed_bytes = sim::write_log(changed);
    REQUIRE(changed_bytes.ok());
    freight_original::publish("freight-changed-command.log", changed_bytes.value());
    std::size_t checkpoints = 0, prior = 0;
    for (std::size_t i = 0; i < rec.log().entries.size(); ++i)
        if (rec.log().entries[i].kind == sim::ReplayEntry::Kind::Checkpoint) {
            ++checkpoints;
            if (i < assign_entry)
                ++prior;
        }
    REQUIRE(checkpoints == 11);
    REQUIRE(prior == 3);
    // State 137's carrier-home endpoint correction reaches the two source thresholds three 500-tick
    // advances sooner in total than the isolated state-136 proof (73 rather than 75 initial advances,
    // then 38 rather than 39 collection advances), so the combined state-138 recording assigns the
    // route three entries earlier. Keep this exact: it is the continuation fixture's deterministic
    // identity fingerprint, not a tolerance.
    REQUIRE(assign_entry == 487);
    std::ostringstream evidence;
    evidence << "state=138 combined=135+136+137 schema=ECONBK1 PRNG=5 replay=4 command=1 save=1\n"
             << "tick=" << w.clock().tick() << " entries=" << rec.log().entries.size()
             << " checkpoints=" << checkpoints << " assign_entry=" << assign_entry
             << " changed_checkpoint=" << assign_entry + 1 << " matched_prior=" << prior
             << " initial_wait_advances=" << initial_waits
             << " collection_wait_advances=" << collection_waits
             << " auto_advances=" << automatic_advances << " first_load_made=" << first_load_made
             << " made=" << made_wood() << " export=" << exported << " received=" << received
             << "\n";
    for (const auto& n : run.natives) {
        REQUIRE(n.advances > 0);
        REQUIRE(n.commands == 1);
        REQUIRE(n.world.save() == final);
        evidence << "native=" << n.label << " suffix_advances=" << n.advances
                 << " suffix_commands=" << n.commands << " final_hash=" << n.world.state_hash()
                 << "\n";
    }
    for (const auto& [owner, cash] : initial_money)
        evidence << "owner=" << owner << " start=" << cash
                 << " construction=" << run.construction[owner] << " income=" << run.income[owner]
                 << " city_upkeep=" << run.upkeep[owner]
                 << " final=" << expansion_test::balances(w).at(owner) << " residual=0\n";
    for (const auto& [ref, paid] : run.city_upkeep)
        evidence << "city=" << ref.island.v << "," << ref.city.v << " upkeep=" << paid << "\n";
    for (std::size_t i = 0; i < run.draws.size(); ++i)
        evidence << "draw=" << sim::draw_site_name(static_cast<sim::DrawSite>(i))
                 << " advances=" << run.draws[i] << "\n";
    const auto& f = std::get<sim::Farm>(w.find_building(forest)->payload);
    evidence << "cuts=" << observed_cuts << " growth=" << observed_growth
             << " raw_credited=" << stats.forestry.raw_credited_88
             << " raw_delivered=" << stats.forestry.raw_delivered_88
             << " consumed=" << stats.forestry.farms.at(forest).consumed_88 << " raw=" << f.raw
             << " farm_out=" << f.out << " cart=" << expansion_test::cart_wood(w, source_wh, wood)
             << " source=" << expansion_test::stock(w, source, wood)
             << " destination=" << expansion_test::stock(w, *destination, wood) << "\n";
    run.natives.clear(); // Subsequent controls are separate from the uninterrupted main proof.
    REQUIRE(suspend_start);
    auto suspended = sim::World::load(*suspend_start, rs.types);
    REQUIRE(suspended.ok());
    auto& branch = suspended.value();
    auto branch_recording = sim::Recorder::start(branch);
    REQUIRE(branch_recording.ok());
    auto branch_rec = std::move(branch_recording).value();
    expansion_test::Run branch_run{branch, branch_rec, {}, {}, {}, {}, {}, 0};
    REQUIRE(
        branch_run
            .apply({sim::PlayerId{0},
                    sim::ControlFreightRouteCmd{sim::FigureId{1}, sim::FreightControl::Suspend}})
            .ok());
    REQUIRE(std::any_of(branch_run.events.begin(), branch_run.events.end(),
                        [](const auto& e) { return e.freight && e.freight->signal == S::Paused; }));
    const auto held = ship_of(branch, 1).holds;
    for (unsigned i = 0; i < 4; ++i)
        branch_run.advance();
    REQUIRE(ship_of(branch, 1).holds == held);
    REQUIRE(branch.freight_route(sim::FigureId{1})->status == sim::FreightStatus::Suspended);
    REQUIRE(branch_run
                .apply({sim::PlayerId{0},
                        sim::ControlFreightRouteCmd{sim::FigureId{1}, sim::FreightControl::Resume}})
                .ok());
    REQUIRE(std::any_of(branch_run.events.begin(), branch_run.events.end(), [](const auto& e) {
        return e.freight && e.freight->signal == S::Resumed;
    }));
    bool delivered = false;
    for (unsigned i = 0; i < 2000 && !delivered; ++i) {
        branch_run.advance();
        REQUIRE_FALSE(branch.faulted());
        for (const auto& e : branch_run.events)
            if (e.freight && e.freight->signal == S::Transferred &&
                e.freight->warehouse == destination_wh) {
                REQUIRE(e.freight->amount_88 == 768);
                delivered = true;
            }
    }
    REQUIRE(delivered);
    REQUIRE(expansion_test::stock(branch, *destination, wood) == 768);
    REQUIRE(aboard_of(ship_of(branch, 1).holds, wood) == 26 * 256);
    REQUIRE(branch_run
                .apply({sim::PlayerId{0},
                        sim::ControlFreightRouteCmd{sim::FigureId{1}, sim::FreightControl::Cancel}})
                .ok());
    REQUIRE(branch.freight_route(sim::FigureId{1}) == nullptr);
    REQUIRE(std::any_of(branch_run.events.begin(), branch_run.events.end(), [](const auto& e) {
        return e.freight && e.freight->signal == S::Cancelled;
    }));
    // The control starts before workers or output exist; replay the same legal setup/route
    // suffix with the sole production switch permanently Off. Inherited cargo may deliver.
    auto no_made = sim::World::load(preproduction, rs.types);
    REQUIRE(no_made.ok());
    auto control = std::move(no_made).value();
    sim::TransportStats control_stats;
    control.set_transport_stats(&control_stats);
    REQUIRE(control.apply(off).ok());
    (void)control.drain_events();
    auto control_recording = sim::Recorder::start(control);
    REQUIRE(control_recording.ok());
    auto control_rec = std::move(control_recording).value();
    expansion_test::Run control_run{control, control_rec, {}, {}, {}, {}, {}, 0};
    std::uint64_t control_export = 0, control_received = 0;
    for (std::size_t i = control_start_entry; i < rec.log().entries.size(); ++i) {
        const auto& entry = rec.log().entries[i];
        control_run.events.clear();
        if (entry.kind == sim::ReplayEntry::Kind::Advance)
            control_run.advance(entry.ms);
        else if (entry.kind == sim::ReplayEntry::Kind::Commands)
            for (const auto& command : entry.commands) {
                if (const auto* line = std::get_if<sim::SetProductionLineCmd>(&command.body);
                    line && line->on)
                    continue;
                const auto charge =
                    std::holds_alternative<sim::FoundCmd>(command.body)
                        ? 350U
                        : (std::holds_alternative<sim::BuildCmd>(command.body) ? 50U : 0U);
                const auto result = control_run.apply(
                    command, charge,
                    std::holds_alternative<sim::SetProductionLineCmd>(command.body)
                        ? std::optional<sim::CityRef>{source}
                        : std::nullopt);
                if (std::holds_alternative<sim::BuildCmd>(command.body) && command == receiver)
                    REQUIRE((result.ok() || result.status == sim::Reject::CannotAfford));
                else
                    REQUIRE(result.ok());
            }
        REQUIRE_FALSE(control.faulted());
        for (const auto& e : control_run.events)
            if (e.freight && e.freight->signal == S::Transferred) {
                if (e.freight->direction == D::LoadTarget)
                    control_export += e.freight->amount_88;
                else
                    control_received += e.freight->amount_88;
            }
    }
    REQUIRE(control_export == 0);
    REQUIRE(control_stats.forestry.cuts == 0);
    for (const auto& [id, value] : control_stats.forestry.farms) {
        REQUIRE(id == forest);
        REQUIRE(value.made_88 == 0);
        REQUIRE(value.consumed_88 == 0);
    }
    REQUIRE(expansion_test::stock(control, source, wood) == 0);
    const auto& off_farm = std::get<sim::Farm>(control.find_building(forest)->payload);
    REQUIRE(off_farm.raw == 0);
    REQUIRE(off_farm.out == 0);
    REQUIRE(off_farm.workers_out == 0);
    REQUIRE(control.check_invariants().empty());
    evidence << "no_production_export=" << control_export
             << " inherited_delivery=" << control_received << " qualified_productive_cycles=0\n"
             << "pause_branch_delivery=768 main_construction=1000 wall_ms="
             << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                          started)
                    .count()
             << "\n";
    freight_original::text("events.tsv", trace.str());
    freight_original::text("continuation.txt", evidence.str());
    MESSAGE(evidence.str());
    w.set_transport_stats(nullptr);
    control.set_transport_stats(nullptr);
}
