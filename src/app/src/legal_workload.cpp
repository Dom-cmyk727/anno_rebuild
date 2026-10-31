// The legal city workload (see the header; docs/perf/README.md, "Legal city workload").
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>

#include <anno/app/legal_workload.hpp>
#include <anno/app/loader.hpp>
#include <anno/render/iso.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/replay.hpp>

namespace anno::app {

namespace {

/// The type flag bits the D17 stand-in does not read (placement.hpp; I): a type that needs a
/// coast, a mountain, or that is a road or bridge.
constexpr std::uint32_t kCoastFlag = 0x2000, kMountainFlag = 0x1000, kRoadFlag = 0x4000;

/// The turn the play runner runs: ten steps of 10 ms (PlayRunner::step_ms_, steps_).
constexpr std::uint32_t kStepMs = 10, kSteps = 10;

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const std::uint8_t b : bytes) {
        h = (h ^ b) * 0x100000001b3ULL;
    }
    return h;
}

/// Whether any building of the island covers tile `idx` (the sim's own overlap check, F32).
bool tile_has_building(const sim::Island& is, std::uint64_t idx) {
    for (const std::vector<std::uint32_t>& grid : is.cache.grids) {
        if (idx < grid.size() && grid[static_cast<std::size_t>(idx)] != 0) {
            return true;
        }
    }
    return false;
}

std::size_t city_buildings(const sim::City& city) {
    std::size_t n = 0;
    for (const sim::BuildingList& list : city.index.lists) {
        for (const auto& slot : list.slots) {
            n += slot.has_value() ? 1U : 0U;
        }
    }
    return n;
}

std::uint64_t stock_sum(const sim::City& city) {
    std::uint64_t n = 0;
    for (const std::uint32_t v : city.stock.stock) {
        n += v;
    }
    return n;
}

} // namespace

std::uint64_t LegalWorkload::next_random(std::size_t c) {
    // xorshift64*: the same seed gives the same workload on every host.
    std::uint64_t& r = rng_[c];
    r ^= r >> 12;
    r ^= r << 25;
    r ^= r >> 27;
    return r * 0x2545F4914F6CDD1DULL;
}

formats::Result<std::unique_ptr<LegalWorkload>, std::string>
LegalWorkload::build(const std::filesystem::path& game_dir, const std::filesystem::path& map_file,
                     const LegalWorkloadOptions& options) {
    std::unique_ptr<LegalWorkload> w(new LegalWorkload());
    w->options_ = options;
    {
        std::ifstream in(map_file, std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
        w->map_digest_ = fnv1a(bytes);
    }
    FixedPlayOptions fixed;
    fixed.seats = options.cities;
    fixed.gold = options.gold;
    fixed.goods_tons = options.goods_tons;
    auto loaded = load_play(game_dir, map_file, fixed);
    if (!loaded) {
        return "legal workload: " + loaded.error();
    }
    w->load_ = std::move(loaded).value();
    if (!w->load_.fixed_map) {
        return std::string("legal workload: ") + map_file.string() + " is not a fixed map";
    }
    const sim::World& world = w->load_.world;
    // The islands' terrain, from the scene without buildings: the land classifier of the original
    // (AnnoWelt.dll 0x100900c0, render::kLandAverage) over each island's HEIGHT grid.
    std::map<std::uint32_t, std::vector<bool>> land_by_slot;
    {
        auto scene = load_placed_scene(game_dir, w->load_.name, w->load_.placed, nullptr);
        if (!scene) {
            return "legal workload: the scene: " + scene.error();
        }
        for (const formats::insel::Island& island : scene.value().scene->islands) {
            const auto heights = island.heights();
            if (!heights) {
                continue;
            }
            std::vector<bool> land(std::size_t{island.width} * island.height, false);
            for (std::size_t y = 0; y < island.height; ++y) {
                for (std::size_t x = 0; x < island.width; ++x) {
                    const auto c =
                        render::corners(heights.value(), island.width, island.height, x, y);
                    land[y * island.width + x] =
                        c[0] + c[1] + c[2] + c[3] > 4 * render::kLandAverage;
                }
            }
            land_by_slot.emplace(island.number, std::move(land));
        }
    }
    // The cities of the developer start: one per seat, each on its own island, each holding
    // its Kontor (the only building at tick 0).
    for (const auto& island : world.islands()) {
        if (!island) {
            continue;
        }
        for (const auto& city : island->cities) {
            if (!city) {
                continue;
            }
            LegalCity c;
            c.island_slot = island->index.v;
            c.city = city->number.v;
            c.player = city->owner.v;
            c.island_tiles = island->tiles.tile_count();
            std::vector<std::uint64_t> owned;
            for (std::uint64_t idx = 0; idx < island->tiles.tile_count(); ++idx) {
                if (island->tiles.owner(idx) == city->number.v) {
                    owned.push_back(idx);
                }
            }
            c.owned_tiles = owned.size();
            const auto land = land_by_slot.find(island->index.v);
            if (land == land_by_slot.end() || land->second.size() != island->tiles.tile_count()) {
                return "legal workload: no terrain of island " + std::to_string(island->index.v) +
                       " the size of its tile layer";
            }
            // The search scans the owned land tiles only (a footprint can still reach water at a
            // shore; the candidate filter refuses it).
            std::erase_if(owned, [&land](std::uint64_t idx) {
                return !land->second[static_cast<std::size_t>(idx)];
            });
            c.land_tiles = owned.size();
            for (const std::uint64_t idx : owned) {
                c.free_land_tiles += island->tiles.object(idx) == 0 ? 1U : 0U;
            }
            w->land_.push_back(land->second);
            for (const sim::BuildingList& list : city->index.lists) {
                for (const auto& b : list.slots) {
                    if (b) {
                        w->kontor_type_ = b->type.v;
                        ++c.by_type[b->type.v];
                    }
                }
            }
            w->cities_.push_back(std::move(c));
            w->owned_.push_back(std::move(owned));
        }
    }
    if (w->cities_.size() != options.cities) {
        return "legal workload: " + std::to_string(w->cities_.size()) + " cities, " +
               std::to_string(options.cities) + " seats";
    }
    std::set<std::uint32_t> islands;
    for (const LegalCity& c : w->cities_) {
        islands.insert(c.island_slot);
    }
    if (islands.size() != w->cities_.size()) {
        return std::string("legal workload: two cities share an island");
    }
    // One stream per city (the seed mixed with the city's place in the list), so a city's
    // construction does not depend on the others.
    for (std::size_t c = 0; c < w->cities_.size(); ++c) {
        const std::uint64_t mixed =
            (options.seed + 0x9E3779B97F4A7C15ULL * (c + 1)) ^ 0xD1B54A32D192ED03ULL;
        w->rng_.push_back(mixed == 0 ? 0x9E3779B97F4A7C15ULL : mixed);
    }
    // The search order: each city's owned land tiles shuffled by its stream (Fisher-Yates).
    for (std::size_t c = 0; c < w->owned_.size(); ++c) {
        auto& owned = w->owned_[c];
        for (std::size_t i = owned.size(); i > 1; --i) {
            std::swap(owned[i - 1], owned[static_cast<std::size_t>(w->next_random(c) % i)]);
        }
    }
    w->cursor_.assign(w->owned_.size(), 0);
    // The candidate types: every type the sim can place (a list for its PRODTYPE), but the Kontor,
    // the types whose placement needs a check the stand-in does not make (coast, mountain, road
    // flags; shipyards, whalers, mines and quarries, which stand on a coast or a mountain), by
    // kind.
    const sim::SimConfig& cfg = world.config();
    for (const sim::BuildingType& t : world.types().types()) {
        const bool listed = cfg.layout.list_for(t.prodtype).has_value();
        if (!listed) {
            if ((t.flags_word & kRoadFlag) != 0 && w->road_type_ == 0) {
                w->road_type_ = t.id.v;
            }
            continue;
        }
        if (t.id.v == w->kontor_type_ || (t.flags_word & (kCoastFlag | kMountainFlag)) != 0 ||
            t.width == 0 || t.height == 0) {
            continue;
        }
        switch (t.cls) {
        case sim::RuntimeClass::Shipyard:
        case sim::RuntimeClass::Whaler:
        case sim::RuntimeClass::Mine:
        case sim::RuntimeClass::Quarry:
            continue;
        case sim::RuntimeClass::House:
            w->houses_.push_back(t.id.v);
            break;
        default:
            (t.has_prod ? w->producers_ : w->others_).push_back(t.id.v);
            break;
        }
    }
    if (w->houses_.empty() || w->producers_.empty()) {
        return std::string("legal workload: the registry has no house or producer type to place");
    }
    // Construction: each turn places up to placements_per_turn buildings of one city, so the
    // turn's refusals belong to that city.
    std::vector<bool> done(w->cities_.size(), false);
    for (bool any = true; any;) {
        any = false;
        for (std::size_t c = 0; c < w->cities_.size(); ++c) {
            if (done[c]) {
                continue;
            }
            LegalCity& city = w->cities_[c];
            const std::size_t placed = city.accepted;
            const std::size_t wanted =
                options.buildings_per_city + (c == 0 ? 0 : options.unrelated_extra);
            if (placed >= wanted) {
                done[c] = true;
                continue;
            }
            const std::size_t want = std::min(options.placements_per_turn, wanted - placed);
            std::vector<sim::Command> commands;
            for (std::size_t k = 0; k < want; ++k) {
                sim::PlacementWire p;
                const std::size_t left = options.attempts_per_city > city.candidates
                                             ? options.attempts_per_city - city.candidates
                                             : 0;
                if (left == 0 || !w->next_candidate(c, left, p)) {
                    break;
                }
                commands.push_back(
                    sim::Command{sim::PlayerId{city.player},
                                 sim::BuildCmd{sim::CityRef{sim::IslandId{city.island_slot},
                                                            sim::CityNo{city.city}},
                                               {p}}});
            }
            if (commands.empty()) {
                city.capacity_reached = true;
                done[c] = true;
                continue;
            }
            any = true;
            const sim::City* before =
                world.islands()[city.island_slot]->city(sim::CityNo{city.city});
            const std::size_t count_before = before != nullptr ? city_buildings(*before) : 0;
            city.issued += commands.size();
            const auto reasons = w->run(std::move(commands));
            for (const std::string& r : reasons) {
                ++city.rejected[r];
            }
            const sim::City* after =
                world.islands()[city.island_slot]->city(sim::CityNo{city.city});
            const std::size_t count_after = after != nullptr ? city_buildings(*after) : 0;
            city.accepted += count_after > count_before ? count_after - count_before : 0;
        }
    }
    // Roads: issued as any other placement; the current sim refuses them (Unsupported, O-4).
    if (w->road_type_ != 0) {
        for (LegalCity& city : w->cities_) {
            const std::size_t ci = static_cast<std::size_t>(&city - w->cities_.data());
            std::vector<sim::Command> commands;
            for (std::size_t k = 0; k < options.roads_per_city && k < w->owned_[ci].size(); ++k) {
                const sim::Island& is = *world.islands()[city.island_slot];
                const std::uint64_t idx =
                    w->owned_[ci][(w->cursor_[ci] + k) % w->owned_[ci].size()];
                const auto x = static_cast<std::int32_t>(idx % is.tiles.width()) + is.tiles.x0();
                const auto y = static_cast<std::int32_t>(idx / is.tiles.width()) + is.tiles.y0();
                commands.push_back(sim::Command{
                    sim::PlayerId{city.player},
                    sim::BuildCmd{
                        sim::CityRef{sim::IslandId{city.island_slot}, sim::CityNo{city.city}},
                        {sim::PlacementWire{sim::TypeId{w->road_type_}, x, y, 0, 0, 0}}}});
            }
            w->roads_issued_ += commands.size();
            for (const std::string& r : w->run(std::move(commands))) {
                ++w->road_rejected_[r];
            }
        }
    }
    for (LegalCity& city : w->cities_) {
        w->count_city(city);
        const sim::City* c = world.islands()[city.island_slot]->city(sim::CityNo{city.city});
        city.stock_after_build = c != nullptr ? stock_sum(*c) : 0;
    }
    if (options.settle_turns > 0) {
        for (std::size_t t = 0; t < options.settle_turns; ++t) {
            static_cast<void>(w->run({}));
        }
    }
    for (LegalCity& city : w->cities_) {
        const sim::City* c = world.islands()[city.island_slot]->city(sim::CityNo{city.city});
        city.stock_after_settle = c != nullptr ? stock_sum(*c) : 0;
    }
    w->invariants_ = world.check_invariants();
    if (!w->invariants_.empty()) {
        return "legal workload: invariants after construction: " + w->invariants_;
    }
    w->sorted_ = play_buildings(world);
    sort_buildings(w->sorted_);
    return w;
}

bool LegalWorkload::next_candidate(std::size_t c, std::size_t attempts, sim::PlacementWire& out) {
    LegalCity& city = cities_[c];
    const sim::World& world = load_.world;
    const sim::Island& is = *world.islands()[city.island_slot];
    const sim::City* owner_city = is.city(sim::CityNo{city.city});
    const sim::Player* owner = world.players().find(sim::PlayerId{city.player});
    const std::vector<std::uint64_t>& owned = owned_[c];
    if (owner_city == nullptr || owner == nullptr || owned.empty()) {
        return false;
    }
    // The kind schedule: six houses, three producers and one other in every ten candidates.
    static constexpr char kSchedule[] = "HHHPHHPOHP";
    for (std::size_t a = 0; a < attempts; ++a) {
        ++city.candidates;
        const char kind = kSchedule[city.candidates % 10];
        const std::vector<std::uint32_t>& pool =
            kind == 'H' ? houses_ : (kind == 'P' || others_.empty() ? producers_ : others_);
        const sim::BuildingType* type = world.types().find(
            sim::TypeId{pool[static_cast<std::size_t>(next_random(c) % pool.size())]});
        const auto rotation = static_cast<std::uint8_t>(next_random(c) % 4);
        const std::uint64_t idx = owned[cursor_[c] % owned.size()];
        cursor_[c] = (cursor_[c] + 1) % owned.size();
        const sim::PlacementRecord rec{
            type->id, static_cast<std::int32_t>(idx % is.tiles.width()) + is.tiles.x0(),
            static_cast<std::int32_t>(idx / is.tiles.width()) + is.tiles.y0(), rotation, 0};
        // The sim's own rules, read-only: the footprint owner rule, the placement rule, the
        // overlap with buildings, and affordability (Build steps 2, 3, F32 and 6).
        if (sim::footprint_owner_rule(is, world.config(), *type, rec, sim::CityNo{city.city}) !=
            sim::Reject::None) {
            ++city.prefiltered;
            ++city.refused_owner;
            continue;
        }
        if (sim::stand_in_placement_rule(is, *type, rec) != sim::Reject::None) {
            ++city.prefiltered;
            ++city.refused_object;
            continue;
        }
        const auto [fw, fh] = sim::footprint(*type, rotation);
        bool taken = false, on_land = true;
        for (std::uint32_t dy = 0; dy < fh && !taken; ++dy) {
            for (std::uint32_t dx = 0; dx < fw && !taken; ++dx) {
                // On the island (footprint_owner_rule held), so both are non-negative.
                const std::int64_t tx = std::int64_t{rec.x} - is.tiles.x0() + std::int64_t{dx};
                const std::int64_t ty = std::int64_t{rec.y} - is.tiles.y0() + std::int64_t{dy};
                const auto tile =
                    static_cast<std::uint64_t>(ty * std::int64_t{is.tiles.width()} + tx);
                taken = tile_has_building(is, tile) || claimed_.count(tile) != 0;
                on_land = on_land && land_[c][static_cast<std::size_t>(tile)];
            }
        }
        if (!on_land) {
            ++city.not_land;
            continue;
        }
        bool affordable = owner->money >= pending_gold_ + std::int64_t{type->cost_gold};
        for (const sim::CostEntry& cost : type->cost_goods) {
            const auto spent = pending_goods_.find(cost.goods);
            const std::uint64_t used = spent == pending_goods_.end() ? 0 : spent->second;
            affordable = affordable && cost.goods < owner_city->stock.stock.size() &&
                         owner_city->stock.stock[cost.goods] >= used + cost.amount_88;
        }
        if (taken || !affordable) {
            ++city.prefiltered;
            ++(taken ? city.refused_overlap : city.refused_cost);
            continue;
        }
        for (std::uint32_t dy = 0; dy < fh; ++dy) {
            for (std::uint32_t dx = 0; dx < fw; ++dx) {
                const std::int64_t tx = std::int64_t{rec.x} - is.tiles.x0() + std::int64_t{dx};
                const std::int64_t ty = std::int64_t{rec.y} - is.tiles.y0() + std::int64_t{dy};
                claimed_.insert(
                    static_cast<std::uint64_t>(ty * std::int64_t{is.tiles.width()} + tx));
            }
        }
        pending_gold_ += std::int64_t{type->cost_gold};
        for (const sim::CostEntry& cost : type->cost_goods) {
            pending_goods_[cost.goods] += cost.amount_88;
        }
        out = sim::PlacementWire{type->id, rec.x, rec.y, rotation, 0, 0};
        return true;
    }
    return false;
}

std::vector<std::string> LegalWorkload::run(std::vector<sim::Command> commands,
                                            std::uint32_t steps) {
    claimed_.clear();
    pending_gold_ = 0;
    pending_goods_.clear();
    sim::Turn turn;
    turn.index = turn_++;
    turn.step_ms = kStepMs;
    turn.commands = std::move(commands);
    sim::run_turn(load_.world, nullptr, turn, steps == 0 ? kSteps : steps * kSteps);
    std::vector<std::string> reasons;
    for (const sim::SimEvent& event : load_.world.drain_events()) {
        if (event.kind == sim::SimEventKind::CommandRejected) {
            reasons.emplace_back(sim::reject_name(static_cast<sim::Reject>(event.code)));
        }
    }
    return reasons;
}

std::shared_ptr<const BuildingBatch> LegalWorkload::take_batch() {
    std::vector<SceneBuilding> cur = play_buildings(load_.world);
    sort_buildings(cur);
    std::vector<BuildingDelta> deltas = diff_sorted_buildings(sorted_, cur);
    sorted_ = std::move(cur);
    if (deltas.empty()) {
        return nullptr;
    }
    auto batch = std::make_shared<BuildingBatch>();
    batch->turn = turn_;
    batch->deltas = std::move(deltas);
    return batch;
}

void LegalWorkload::count_city(LegalCity& c) const {
    const sim::Island& is = *load_.world.islands()[c.island_slot];
    c.by_type.clear();
    c.occupied_tiles = 0;
    for (std::uint64_t idx = 0; idx < is.tiles.tile_count(); ++idx) {
        if (is.tiles.owner(idx) == c.city && tile_has_building(is, idx)) {
            ++c.occupied_tiles;
        }
    }
    if (const sim::City* city = is.city(sim::CityNo{c.city})) {
        for (const sim::BuildingList& list : city->index.lists) {
            for (const auto& b : list.slots) {
                if (b) {
                    ++c.by_type[b->type.v];
                }
            }
        }
    }
}

LegalEdit LegalWorkload::build_one(std::size_t c, std::size_t attempts) {
    LegalEdit edit;
    edit.what = "build";
    LegalCity& city = cities_[c];
    sim::PlacementWire p;
    if (!next_candidate(c, attempts, p)) {
        edit.reason = "no candidate";
        return edit;
    }
    const sim::Island& is = *load_.world.islands()[city.island_slot];
    edit.island = city.island_slot;
    edit.x = p.x - is.tiles.x0();
    edit.y = p.y - is.tiles.y0();
    edit.type = p.type.v;
    const auto reasons = run({sim::Command{
        sim::PlayerId{city.player},
        sim::BuildCmd{sim::CityRef{sim::IslandId{city.island_slot}, sim::CityNo{city.city}},
                      {p}}}});
    edit.accepted = reasons.empty();
    edit.reason = reasons.empty() ? std::string() : reasons.front();
    edit.batch = take_batch();
    invariants_ = load_.world.check_invariants();
    count_city(city);
    return edit;
}

LegalEdit LegalWorkload::demolish_one(std::size_t c) {
    LegalEdit edit;
    edit.what = "demolish";
    LegalCity& city = cities_[c];
    const sim::Island& is = *load_.world.islands()[city.island_slot];
    const sim::City* owner_city = is.city(sim::CityNo{city.city});
    std::vector<const sim::Building*> candidates;
    if (owner_city != nullptr) {
        for (const sim::BuildingList& list : owner_city->index.lists) {
            for (const auto& b : list.slots) {
                if (b && b->type.v != kontor_type_) {
                    candidates.push_back(&*b);
                }
            }
        }
    }
    if (candidates.empty()) {
        edit.reason = "no building";
        return edit;
    }
    const sim::Building& b =
        *candidates[static_cast<std::size_t>(next_random(c) % candidates.size())];
    edit.island = city.island_slot;
    edit.x = b.x - is.tiles.x0();
    edit.y = b.y - is.tiles.y0();
    edit.type = b.type.v;
    const sim::TileXY tile{b.x, b.y};
    const auto reasons = run({sim::Command{
        sim::PlayerId{city.player},
        sim::DemolishCmd{sim::CityRef{sim::IslandId{city.island_slot}, sim::CityNo{city.city}},
                         {tile}}}});
    edit.accepted = reasons.empty();
    edit.reason = reasons.empty() ? std::string() : reasons.front();
    edit.batch = take_batch();
    invariants_ = load_.world.check_invariants();
    count_city(city);
    return edit;
}

LegalEdit LegalWorkload::advance(std::size_t turns) {
    LegalEdit edit;
    edit.what = "advance";
    edit.accepted = true;
    for (std::size_t t = 0; t < turns; ++t) {
        static_cast<void>(run({}));
    }
    edit.batch = take_batch();
    invariants_ = load_.world.check_invariants();
    return edit;
}

std::string LegalWorkload::manifest() const {
    std::ostringstream o;
    o << "legal_workload=1\n";
    o << "label=legal (sim-validated placements; see docs/perf/README.md for its limits)\n";
    o << "seed=" << options_.seed << "\n";
    o << "cities=" << cities_.size() << " buildings_per_city=" << options_.buildings_per_city
      << " unrelated_extra=" << options_.unrelated_extra
      << " attempts_per_city=" << options_.attempts_per_city
      << " placements_per_turn=" << options_.placements_per_turn
      << " settle_turns=" << options_.settle_turns << "\n";
    o << "resources gold=" << options_.gold << " goods_tons=" << options_.goods_tons << "\n";
    o << "map_fnv1a64=" << std::hex << map_digest_ << " registry_digest=" << registry_digest()
      << " state_hash=" << state_hash() << std::dec << "\n";
    o << "kontor_type=" << kontor_type_ << " road_type=" << road_type_
      << " candidate_types houses=" << houses_.size() << " producers=" << producers_.size()
      << " others=" << others_.size() << "\n";
    o << "turns=" << turn_ << "\n";
    std::size_t total = 0, issued = 0, rejected = 0;
    for (const LegalCity& c : cities_) {
        std::size_t placed = 0;
        for (const auto& [type, n] : c.by_type) {
            placed += n;
        }
        total += placed;
        issued += c.issued;
        std::size_t refused = 0;
        for (const auto& [why, n] : c.rejected) {
            refused += n;
        }
        rejected += refused;
        o << "city island=" << c.island_slot << " city=" << c.city << " player=" << c.player
          << " island_tiles=" << c.island_tiles << " owned_tiles=" << c.owned_tiles
          << " land_tiles=" << c.land_tiles << " free_land_tiles=" << c.free_land_tiles
          << " occupied_tiles=" << c.occupied_tiles << " buildings=" << placed
          << " accepted=" << c.accepted << " issued=" << c.issued << " rejected=" << refused
          << " candidates=" << c.candidates << " prefiltered=" << c.prefiltered
          << " not_land=" << c.not_land << " refused_owner=" << c.refused_owner
          << " refused_object=" << c.refused_object << " refused_overlap=" << c.refused_overlap
          << " refused_cost=" << c.refused_cost
          << " capacity_reached=" << (c.capacity_reached ? 1 : 0)
          << " stock_after_build=" << c.stock_after_build
          << " stock_after_settle=" << c.stock_after_settle << "\n";
        o << "city_types island=" << c.island_slot;
        for (const auto& [type, n] : c.by_type) {
            o << " " << type << ":" << n;
        }
        o << "\n";
        if (!c.rejected.empty()) {
            o << "city_rejected island=" << c.island_slot;
            for (const auto& [why, n] : c.rejected) {
                o << " " << why << ":" << n;
            }
            o << "\n";
        }
    }
    o << "roads issued=" << roads_issued_;
    for (const auto& [why, n] : road_rejected_) {
        o << " rejected_" << why << "=" << n;
    }
    o << "\n";
    o << "totals buildings=" << total << " issued=" << issued << " rejected=" << rejected << "\n";
    o << "invariants=" << (invariants_.empty() ? "ok" : invariants_) << "\n";
    return o.str();
}

} // namespace anno::app
