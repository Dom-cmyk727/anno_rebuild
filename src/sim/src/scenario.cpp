// World::create: the scenario loader (docs/sim/state.md §10, stage S5).
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/create.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

using Spec = ScenarioSpec;

std::string idx_path(const char* what, std::size_t i) {
    return std::string(what) + "[" + std::to_string(i) + "]";
}

/// The checks of one island of the scenario, with the scenario's own paths
/// ("islands[0].deposits[1]: ..."). The rectangle against the world and against
/// the other islands is checked by the caller on the rectangles alone
/// (validate_island_rects), before anything is built. `player_present[s]` is 1
/// for a player slot the scenario fills. The footprints of the buildings (inside
/// the island, free of each other) are create_building's: the build reports them
/// with the same kind of path.
std::string check_island_spec(const Spec::IslandSpec& is, std::size_t i, const SimConfig& cfg,
                              const TypeRegistry& types,
                              const std::vector<std::uint8_t>& player_present) {
    const std::string p = idx_path("islands", i);
    if (is.w == 0 || is.h == 0) {
        return p + ": the island rectangle is empty (" + std::to_string(is.w) + " x " +
               std::to_string(is.h) + ")";
    }
    const std::uint64_t tiles = std::uint64_t{is.w} * is.h;
    if (!island_tiles_within_bound(is.w, is.h)) {
        return p + ": " + std::to_string(tiles) + " tiles, more than " +
               std::to_string(kIslandTileBound);
    }
    if (is.climate >= cfg.monopoly_climate_mask.size()) {
        return p + ".climate: " + std::to_string(is.climate) +
               " has no entry in SimConfig.monopoly_climate_mask";
    }
    if (is.terrain && !terrain_matches(*is.terrain)) {
        return p + ".terrain: the digest does not match the columns (use make_terrain)";
    }
    std::vector<std::uint8_t> city_present(cfg.city_slots, 0);
    for (std::size_t k = 0; k < is.cities.size(); ++k) {
        const Spec::CitySpec& cs = is.cities[k];
        const std::string cp = p + ".cities[" + std::to_string(k) + "]";
        if (!cs.no.valid() || cs.no.v >= cfg.city_slots) {
            return cp + ".no: is not a slot below SimConfig.city_slots (" +
                   std::to_string(cfg.city_slots) + ")";
        }
        if (cs.no.v == cfg.neutral_owner) {
            return cp + ".no: " + std::to_string(cs.no.v) +
                   " is SimConfig.neutral_owner, the owner value of nobody";
        }
        if (city_present[cs.no.v] != 0) {
            return cp + ".no: city " + std::to_string(cs.no.v) + " is used twice";
        }
        city_present[cs.no.v] = 1;
        if (!cs.owner.valid() || cs.owner.v >= player_present.size() ||
            player_present[cs.owner.v] == 0) {
            return cp + ".owner: there is no player in slot " +
                   (cs.owner.valid() ? std::to_string(cs.owner.v) : std::string("(invalid)"));
        }
        if (cs.stock.has_value() && cs.stock->size() != cfg.warehouse_goods) {
            return cp + ".stock: " + std::to_string(cs.stock->size()) +
                   " entries, SimConfig.warehouse_goods is " + std::to_string(cfg.warehouse_goods);
        }
        for (std::size_t b = 0; b < cs.buildings.size(); ++b) {
            const Spec::BuildingSpec& bs = cs.buildings[b];
            const std::string bp = cp + ".buildings[" + std::to_string(b) + "]";
            const BuildingType* ty = types.find(bs.type);
            if (ty == nullptr || bs.type.v == 0) {
                return bp + ".type: " + std::to_string(bs.type.v) +
                       " is not a type of the registry";
            }
            if (bs.rotation > 3) {
                return bp + ".rotation: " + std::to_string(bs.rotation) + " is not below 4";
            }
            if (!cfg.layout.list_for(ty->prodtype).has_value()) {
                return bp + ".type: " + std::to_string(bs.type.v) + " (PRODTYPE " +
                       std::to_string(ty->prodtype) +
                       ") is not a building: it has no list in SimConfig.layout";
            }
            if (bs.payload.has_value()) {
                const std::string problem =
                    payload_problem(*bs.payload, *ty, types.class_info(ty->cls), cfg);
                if (!problem.empty()) {
                    return bp + ".payload: " + problem;
                }
            }
        }
    }
    std::uint64_t covered = 0;
    for (std::size_t r = 0; r < is.owner_runs.size(); ++r) {
        const auto& [owner, length] = is.owner_runs[r];
        if (owner > cfg.neutral_owner) {
            return p + ".owner_runs[" + std::to_string(r) + "]: owner " + std::to_string(owner) +
                   " is above neutral_owner " + std::to_string(cfg.neutral_owner);
        }
        covered += length;
        if (covered > tiles) {
            return p + ".owner_runs[" + std::to_string(r) + "]: the runs cover more than the " +
                   std::to_string(tiles) + " tiles of the island";
        }
        // S6: an owner other than nobody is a city of the island (§9.6).
        if (owner != cfg.neutral_owner && length > 0 &&
            (owner >= city_present.size() || city_present[owner] == 0)) {
            return p + ".owner_runs[" + std::to_string(r) + "]: owner " + std::to_string(owner) +
                   " is not a city of the island (" + p + ".cities)";
        }
    }
    std::vector<std::uint32_t> seen;
    seen.reserve(is.objects.size());
    for (std::size_t o = 0; o < is.objects.size(); ++o) {
        if (is.objects[o].index >= tiles) {
            return p + ".objects[" + std::to_string(o) + "]: tile index " +
                   std::to_string(is.objects[o].index) + " is outside the island";
        }
        seen.push_back(is.objects[o].index);
    }
    std::sort(seen.begin(), seen.end());
    if (std::adjacent_find(seen.begin(), seen.end()) != seen.end()) {
        return p + ".objects: a tile index is given twice";
    }
    const std::int64_t x1 = std::int64_t{is.x0} + is.w;
    const std::int64_t y1 = std::int64_t{is.y0} + is.h;
    for (std::size_t d = 0; d < is.deposits.size(); ++d) {
        const Spec::DepositSpec& dep = is.deposits[d];
        const std::string dp = p + ".deposits[" + std::to_string(d) + "]";
        if (dep.x < is.x0 || dep.y < is.y0 || std::int64_t{dep.x} >= x1 ||
            std::int64_t{dep.y} >= y1) {
            return dp + ": the position is outside the island";
        }
        if (!types.goods_by_name(dep.goods).has_value()) {
            return dp + ".goods: unknown goods name '" + dep.goods + "'";
        }
    }
    return {};
}

/// What create_building's refusal means for a scenario building, in words.
std::string building_refusal(Reject r) {
    switch (r) {
    case Reject::OutOfIsland:
        return "the footprint is not inside the island";
    case Reject::Occupied:
        return "the footprint overlaps another building";
    case Reject::SerialExhausted:
        return "the island has no building serial left";
    default:
        return "cannot be created (" + std::string(reject_name(r)) + ")";
    }
}

/// The island's content (§10 steps 3 and 4): tiles from the owner runs and
/// objects, deposits in file order, then the cities in ascending number and the
/// buildings of each in file order through create_building at tick 0 (no cost
/// charged, so the list slots follow file order, D 0x100264a0). Touches only
/// `out`. Returns the first refusal as a message ("" when the island was built).
std::string build_island(const Spec::IslandSpec& is, std::size_t island_index, const SimConfig& cfg,
                         const Rules& rules, const TypeRegistry& types, Island& out) {
    out.index = is.slot;
    static_cast<void>(out.tiles.reset(is.x0, is.y0, is.w, is.h, cfg.chunk_edge));
    out.climate = is.climate;
    out.terrain = is.terrain;
    const std::uint64_t tiles = out.tiles.tile_count();
    // Tiles after the last run are neutral.
    for (std::uint64_t t = 0; t < tiles; ++t) {
        out.tiles.set_owner(t, cfg.neutral_owner);
    }
    std::uint64_t at = 0;
    for (const auto& [owner, length] : is.owner_runs) {
        for (std::uint32_t k = 0; k < length; ++k) {
            out.tiles.set_owner(at++, owner);
        }
    }
    for (const Spec::TileObjectSpec& o : is.objects) {
        out.tiles.set_object(o.index, o.object);
        out.tiles.set_phase(o.index, o.phase);
    }
    out.deposits.reserve(is.deposits.size());
    for (const Spec::DepositSpec& d : is.deposits) {
        Deposit dep;
        dep.goods = d.goods;
        dep.x = d.x;
        dep.y = d.y;
        dep.remaining = d.remaining;
        dep.reserve = d.reserve;
        out.deposits.push_back(std::move(dep));
    }
    // sweep.stamp = the world tick at island init = 0 (D 0x1008c2e6); every other
    // stamp, cursor and counter starts at its default.

    // Cities by number (the table slot is the number), then their buildings.
    out.cities.assign(cfg.city_slots, std::nullopt);
    std::vector<std::size_t> order(is.cities.size());
    for (std::size_t k = 0; k < order.size(); ++k) {
        order[k] = k;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return is.cities[a].no < is.cities[b].no; });
    for (const std::size_t k : order) {
        const Spec::CitySpec& cs = is.cities[k];
        City city = make_city(cs.no, cs.owner, cfg);
        if (cs.stock.has_value()) {
            city.stock.stock = *cs.stock;
        }
        out.cities[cs.no.v] = std::move(city);
    }
    for (const std::size_t k : order) {
        const Spec::CitySpec& cs = is.cities[k];
        City& city = *out.cities[cs.no.v];
        for (std::size_t b = 0; b < cs.buildings.size(); ++b) {
            const Spec::BuildingSpec& bs = cs.buildings[b];
            PlacementRecord rec;
            rec.type = bs.type;
            rec.x = bs.x;
            rec.y = bs.y;
            rec.rotation = bs.rotation;
            rec.byte0f = bs.byte0f;
            const auto made = create_building(cfg, rules, types, out, city, rec, 0,
                                              bs.payload.has_value() ? &*bs.payload : nullptr);
            if (!made.ok()) {
                return idx_path("islands", island_index) + ".cities[" + std::to_string(k) +
                       "].buildings[" + std::to_string(b) + "]: " + building_refusal(made.error());
            }
        }
    }
    return {};
}

} // namespace

formats::Result<World, std::string> World::create(const ScenarioSpec& spec,
                                                  std::shared_ptr<const TypeRegistry> types,
                                                  SimConfig config, Rules rules, Seams seams,
                                                  const Executor& ex) {
    // 1. Validate (§10): configuration first, before any state is built.
    if (!types) {
        return std::string("registry: is null");
    }
    std::string problem = config.validate();
    if (problem.empty()) {
        problem = rules.validate();
    }
    if (problem.empty()) {
        problem = validate_seams(seams, config);
    }
    if (problem.empty()) {
        problem = types->validate_against(config);
        if (!problem.empty()) {
            problem = "registry does not fit the configuration: " + problem;
        }
    }
    if (problem.empty()) {
        problem = spec.settings.validate();
    }
    if (!problem.empty()) {
        return problem;
    }
    if (spec.rng_table.has_value() && spec.rng_table->empty()) {
        return std::string("rng_table: must not be empty");
    }

    std::vector<std::uint8_t> player_taken(config.player_slots, 0);
    for (std::size_t i = 0; i < spec.players.size(); ++i) {
        const Spec::PlayerSpec& ps = spec.players[i];
        const std::string p = idx_path("players", i);
        if (!ps.id.valid() || ps.id.v >= config.player_slots) {
            return p + ".id: is not a slot below SimConfig.player_slots (" +
                   std::to_string(config.player_slots) + ")";
        }
        if (player_taken[ps.id.v] != 0) {
            return p + ".id: slot " + std::to_string(ps.id.v) + " is used twice";
        }
        player_taken[ps.id.v] = 1;
        if (!ps.research.empty() && ps.research.size() != config.research_entries) {
            return p + ".research: " + std::to_string(ps.research.size()) +
                   " entries, SimConfig.research_entries is " +
                   std::to_string(config.research_entries);
        }
    }
    std::vector<std::uint8_t> island_taken(config.island_slots, 0);
    std::uint64_t table_size = 0;
    for (std::size_t i = 0; i < spec.islands.size(); ++i) {
        const Spec::IslandSpec& is = spec.islands[i];
        if (!is.slot.valid() || is.slot.v >= config.island_slots) {
            return idx_path("islands", i) + ".slot: is not a slot below SimConfig.island_slots (" +
                   std::to_string(config.island_slots) + ")";
        }
        if (island_taken[is.slot.v] != 0) {
            return idx_path("islands", i) + ".slot: " + std::to_string(is.slot.v) +
                   " is used twice";
        }
        island_taken[is.slot.v] = 1;
        table_size = std::max<std::uint64_t>(table_size, std::uint64_t{is.slot.v} + 1);
    }
    for (std::size_t i = 0; i < spec.islands.size(); ++i) {
        problem = check_island_spec(spec.islands[i], i, config, *types, player_taken);
        if (!problem.empty()) {
            return problem;
        }
    }
    // The rectangles inside the world, covering at most its area, pairwise
    // disjoint: checked on the rectangles alone, BEFORE any tile column is
    // allocated, so that what a scenario can ask for is bounded by the world's
    // area (at most kWorldEdgeBound squared tiles) and not by the sum of the
    // per-island bound (F30). validate_islands repeats the check on the built table.
    std::vector<SlotRect> rects;
    rects.reserve(spec.islands.size());
    for (const Spec::IslandSpec& is : spec.islands) {
        rects.push_back(SlotRect{is.slot.v, is.x0, is.y0, is.w, is.h});
    }
    problem = validate_island_rects(rects, spec.settings);
    if (!problem.empty()) {
        return problem;
    }

    // 2. Configuration, rules, settings, seams, the PRNG and the players.
    World w;
    w.config_ = std::move(config);
    w.rules_ = rules;
    w.settings_ = spec.settings;
    w.seams_ = std::move(seams);
    w.types_ = RegistryRef(std::move(types));
    std::optional<WorldRng> rng = scenario_rng(spec);
    if (!rng.has_value()) {
        return std::string("rng_table: must not be empty");
    }
    w.rng_ = std::move(*rng);
    w.players_ = make_player_table(w.config_);
    w.figures_ = make_figure_registry(w.config_);
    // §10 step 2: the monopoly entries sized from the configuration and empty, the
    // script timers stopped at 0.
    w.timers_ = make_script_timers(w.config_);
    w.monopoly_ = make_monopoly_table(w.config_);
    for (const Spec::PlayerSpec& ps : spec.players) {
        Player p = make_player(ps.id, w.config_);
        p.kind = ps.kind;
        p.volk = ps.volk;
        p.money = ps.money;
        p.flags_a0 = ps.flags_a0;
        p.default_colour = ps.colour == 0xff ? kNoColour : static_cast<std::uint32_t>(ps.colour);
        p.name = ps.name;
        if (!ps.research.empty()) {
            p.research.state = ps.research;
        }
        w.players_.slots[ps.id.v] = std::move(p);
    }

    // 3. Islands by slot (the rectangles were checked above). The content of each
    // island is built on `ex` (one island per job, each writing only its own entry)
    // and placed in table order.
    std::vector<Island> built(spec.islands.size());
    std::vector<std::string> refused(spec.islands.size());
    const SimConfig& cfg = w.config_;
    const Rules& build_rules = w.rules_;
    const TypeRegistry& registry = *w.types_;
    parallel_for(ex, spec.islands.size(), [&](std::uint64_t i) {
        const auto k = static_cast<std::size_t>(i);
        refused[k] = build_island(spec.islands[k], k, cfg, build_rules, registry, built[k]);
    });
    for (const std::string& message : refused) {
        if (!message.empty()) {
            return message; // the first island in scenario order
        }
    }
    w.islands_.assign(static_cast<std::size_t>(table_size), std::nullopt);
    for (std::size_t i = 0; i < spec.islands.size(); ++i) {
        w.islands_[spec.islands[i].slot.v] = std::move(built[i]);
    }

    // 5. The structural facts (§9.6) hold for what was built: the whole of
    // check_invariants (players, islands, cities and buildings, footprints, the
    // derived indexes).
    problem = w.check_invariants();
    if (!problem.empty()) {
        return problem;
    }
    return w;
}

std::optional<WorldRng> scenario_rng(const ScenarioSpec& spec) {
    if (spec.rng_table.has_value()) {
        return WorldRng::from_table(*spec.rng_table, spec.seed);
    }
    WorldRng rng;
    rng.set_cursor(spec.seed);
    return rng;
}

} // namespace anno::sim
