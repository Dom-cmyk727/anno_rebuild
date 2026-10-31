// The ship-start loader. See ship_start.hpp.
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include <anno/formats/chunk.hpp>
#include <anno/formats/insel.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/sim_load/ship_start.hpp>

namespace anno::sim_load {
namespace {

namespace fs = std::filesystem;
namespace sm = formats::ship_manifest;

std::optional<std::vector<std::uint8_t>> read_bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

/// INFO[0] of the template's INSEL5 (the climate), or nullopt. Same rule as fixed_map.cpp.
std::optional<std::uint8_t> template_climate(formats::Bytes scp, std::size_t climates) {
    auto parsed = formats::chunk::parse_file(scp);
    if (!parsed || parsed.value().chunks.empty()) {
        return std::nullopt;
    }
    const formats::chunk::Chunk* insel5 = nullptr;
    for (const formats::chunk::Chunk& child : parsed.value().chunks.front().children) {
        if (child.name() == "INSEL5") {
            insel5 = &child;
            break;
        }
    }
    if (insel5 == nullptr) {
        return std::nullopt;
    }
    auto island = formats::insel::Island::from_insel5(insel5->payload);
    if (!island) {
        return std::nullopt;
    }
    const std::optional<formats::Bytes> info = island.value().record("INFO");
    if (!info || info->empty() || (*info)[0] >= climates) {
        return std::nullopt;
    }
    return (*info)[0];
}

bool ranges_overlap(std::int64_t a, std::uint32_t aw, std::int64_t b, std::uint32_t bw) {
    return a < b + static_cast<std::int64_t>(bw) && b < a + static_cast<std::int64_t>(aw);
}

constexpr std::uint32_t kShip = 0, kScout = 1, kDonkey = 2; // order within a seat

} // namespace

formats::Result<std::int32_t, std::string> milli_tile_to_fixed(std::int64_t milli) {
    // |m| * 32 must fit int64 before it is divided; the quotient must fit int32.
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max() / 32;
    if (milli > kMax || milli < -kMax) {
        return std::string("overflows the 64-bit intermediate of m * 32");
    }
    const std::int64_t scaled = milli * 32;
    if (scaled % 125 != 0) {
        return std::string(
            "is not an exact multiple of 1/256 tile (m * 32 is not divisible by 125)");
    }
    const std::int64_t q = scaled / 125;
    if (q > std::numeric_limits<std::int32_t>::max() ||
        q < std::numeric_limits<std::int32_t>::min()) {
        return std::string("overflows int32 in Q24.8");
    }
    return static_cast<std::int32_t>(q);
}

std::optional<std::uint8_t> facing_to_dir16(std::int64_t facing) {
    static constexpr std::uint8_t kTable[4] = {12, 8, 4, 0};
    if (facing < 0 || facing > 3) {
        return std::nullopt;
    }
    return kTable[facing];
}

std::optional<sim::GoodsId> manifest_goods(const sim::TypeRegistry& types,
                                           const std::string& name) {
    static const std::pair<const char*, const char*> kNames[] = {{"wood", "HOLZ"},
                                                                 {"food", "NAHRUNG"},
                                                                 {"tools", "WERKZEUG"},
                                                                 {"bricks", "ZIEGEL"},
                                                                 {"spices", "GEWUERZE"}};
    for (const auto& [manifest_name, goods_name] : kNames) {
        if (name == manifest_name) {
            return types.goods_by_name(goods_name);
        }
    }
    return std::nullopt;
}

formats::Result<fs::path, std::string> resolve_content_path(const fs::path& content_root,
                                                            const std::string& relative) {
    // 1. The spelling, checked on the string before any file system call (a NUL would end a
    //    C string early: "a.sze\0b" must not open "a.sze").
    if (relative.empty()) {
        return std::string("is empty");
    }
    for (std::size_t i = 0; i < relative.size(); ++i) {
        const auto c = static_cast<unsigned char>(relative[i]);
        const std::string at = " at byte " + std::to_string(i);
        if (c == 0) {
            return "contains a NUL character" + at;
        }
        if (c < 0x20 || c == 0x7F) {
            return "contains a control character" + at;
        }
        if (c == '\\') {
            return "contains a backslash" + at + " (a Windows separator; use '/')";
        }
        if (c == ':') {
            return "contains ':'" + at + " (a Windows drive or stream)";
        }
    }
    if (relative.front() == '/') {
        return std::string("is a rooted path; it must be relative to the content directory");
    }
    std::vector<std::string> segments;
    for (std::size_t start = 0; start <= relative.size();) {
        const std::size_t end = std::min(relative.find('/', start), relative.size());
        segments.push_back(relative.substr(start, end - start));
        start = end + 1;
    }
    for (const std::string& segment : segments) {
        if (segment.empty()) {
            return std::string("has an empty segment (\"//\" or a trailing '/')");
        }
        if (segment == "." || segment == "..") {
            return "has a '" + segment + "' segment; paths are not normalised";
        }
        if (segment.back() == ' ' || segment.back() == '.') {
            return std::string("has a segment ending in a space or a dot (Windows drops it)");
        }
    }
    // 2. The real file. The root is resolved normally (it may be a link); the file is resolved
    //    through every link on the way and must still lie inside the real root.
    std::error_code error;
    const fs::path real_root = fs::canonical(content_root, error);
    if (error) {
        return "cannot be resolved: the content directory " + content_root.string() + " (" +
               error.message() + ")";
    }
    fs::path joined = real_root;
    for (const std::string& segment : segments) {
        joined /= segment;
    }
    const fs::path real = fs::canonical(joined, error);
    if (error) {
        return "does not exist below the content directory (" + error.message() + ")";
    }
    const auto [root_end, real_at] =
        std::mismatch(real_root.begin(), real_root.end(), real.begin(), real.end());
    if (root_end != real_root.end()) {
        return "resolves outside the content directory (through a link) to " + real.string();
    }
    if (!fs::is_regular_file(real, error)) {
        return std::string("is not a regular file");
    }
    return real;
}

formats::Result<sim::World, std::string>
load_ship_start(const sm::Manifest& manifest, const fs::path& game_dir,
                std::shared_ptr<const sim::TypeRegistry> types, const sim::SimConfig& config,
                const sim::Rules& rules, const sim::Executor& ex) {
    if (!types) {
        return std::string("registry: is null");
    }
    // 1. Support: this loader takes the proposed baseline only (a schema-valid alternative
    //    loadout is a valid manifest this adapter rejects).
    if (!sm::baseline_ok(manifest)) {
        return std::string("loadout: not the supported baseline (status proposed-baseline, gold "
                           "30000, figures 1502 / 717 / 718, holds wood 50, food 10, tools 50, "
                           "tools 30)");
    }
    const sm::Loadout& loadout = manifest.loadout;
    if (manifest.spawns.size() != sm::kSeatCount) {
        return std::string("spawns: this loader takes exactly five seats");
    }
    for (std::size_t i = 0; i < manifest.spawns.size(); ++i) {
        if (manifest.spawns[i].seat != static_cast<std::int64_t>(i)) {
            return "spawns[" + std::to_string(i) + "].seat: seats must be 0..4 in order";
        }
    }

    // 2. Content: figure types and goods through the registry.
    const std::int64_t figure_kinds[3] = {loadout.ship_figure, loadout.scout_figure,
                                          loadout.donkey_figure};
    const char* figure_fields[3] = {"loadout.ship_figure", "loadout.scout_figure",
                                    "loadout.donkey_figure"};
    for (int i = 0; i < 3; ++i) {
        if (figure_kinds[i] < 0 || figure_kinds[i] > std::numeric_limits<std::uint32_t>::max() ||
            types->find_figure_type(static_cast<std::uint32_t>(figure_kinds[i])) == nullptr) {
            return std::string(figure_fields[i]) + ": " + std::to_string(figure_kinds[i]) +
                   " is not a figure type of the registry";
        }
    }
    std::vector<sim::ShipHold> holds;
    for (std::size_t i = 0; i < loadout.holds.size(); ++i) {
        const auto goods = manifest_goods(*types, loadout.holds[i].goods);
        if (!goods) {
            return "loadout.holds[" + std::to_string(i) + "].goods: '" + loadout.holds[i].goods +
                   "' is not a goods of the registry";
        }
        holds.push_back(sim::ShipHold{*goods, static_cast<std::uint32_t>(loadout.holds[i].tons)});
    }

    // 3. The base scenario supplies settings and catalogues; its players, units and cities
    //    are the `excluded` list and are dropped.
    // The base is content-relative (resolve_content_path): refused before anything is opened.
    const auto base_path = resolve_content_path(game_dir, manifest.base);
    if (!base_path) {
        return "base: " + base_path.error();
    }
    const std::optional<std::vector<std::uint8_t>> base_bytes = read_bytes(base_path.value());
    if (!base_bytes) {
        return "base: cannot read " + base_path.value().string();
    }
    auto loaded = sim::load_scenario(*base_bytes, *types, config);
    if (!loaded) {
        return "base: " + loaded.error();
    }
    sim::LoadedScenario out = std::move(loaded).value();
    out.spec.settings.world_width = static_cast<std::uint32_t>(manifest.edge_w);
    out.spec.settings.world_height = static_cast<std::uint32_t>(manifest.edge_h);
    out.spec.seed = static_cast<std::uint32_t>(manifest.seed);
    if (const std::string problem = out.spec.settings.validate(); !problem.empty()) {
        return problem;
    }
    out.spec.islands.clear();
    out.spec.players.clear();
    out.spec.figures.clear();
    out.links.clear();
    out.link_inputs = {};
    out.links_total = 0;
    out.links_placed = 0;

    // 4. Five players, the baseline gold, no cities.
    for (std::uint32_t seat = 0; seat < sm::kSeatCount; ++seat) {
        sim::ScenarioSpec::PlayerSpec player;
        player.id = sim::PlayerId{seat};
        player.kind = 8;
        player.volk = 1;
        player.money = loadout.gold;
        out.spec.players.push_back(std::move(player));
    }

    // 5. The island templates, placed without ownership.
    const IslandTemplates templates = scan_island_templates(game_dir);
    std::map<std::string, std::size_t> by_name;
    for (std::size_t i = 0; i < templates.summaries.size(); ++i) {
        by_name.emplace(templates.summaries[i].name, i);
    }
    struct Placed {
        const sm::Island* layout = nullptr;
        std::int32_t x = 0, y = 0;
        std::uint32_t w = 0, h = 0;
        std::uint8_t climate = 0;
        std::size_t template_index = 0;
    };
    std::vector<Placed> placed;
    placed.reserve(manifest.islands.size());
    for (std::size_t i = 0; i < manifest.islands.size(); ++i) {
        const sm::Island& island = manifest.islands[i];
        const std::string at = "islands[" + std::to_string(i) + "]";
        if (island.id < 0 || static_cast<std::uint64_t>(island.id) >= config.island_slots) {
            return at + ".id: " + std::to_string(island.id) + " is not below island_slots " +
                   std::to_string(config.island_slots);
        }
        const auto found = by_name.find(sim::ascii_upper(island.key));
        if (found == by_name.end()) {
            return at + ".key: unknown template '" + island.key + "'";
        }
        const sim::TemplateSummary& summary = templates.summaries[found->second];
        if (island.x < 0 || island.y < 0 || island.x > manifest.edge_w ||
            island.y > manifest.edge_h ||
            island.x + static_cast<std::int64_t>(summary.w) > manifest.edge_w ||
            island.y + static_cast<std::int64_t>(summary.h) > manifest.edge_h) {
            return at + ": template '" + island.key + "' at (" + std::to_string(island.x) + ", " +
                   std::to_string(island.y) + ") is outside the " +
                   std::to_string(manifest.edge_w) + " x " + std::to_string(manifest.edge_h) +
                   " world";
        }
        for (const Placed& earlier : placed) {
            if (ranges_overlap(island.x, summary.w, earlier.x, earlier.w) &&
                ranges_overlap(island.y, summary.h, earlier.y, earlier.h)) {
                return at + ": overlaps island " + std::to_string(earlier.layout->id);
            }
        }
        const std::optional<std::vector<std::uint8_t>> bytes =
            read_bytes(templates.files[found->second]);
        if (!bytes) {
            return at + ".key: cannot read " + templates.files[found->second].string();
        }
        const std::optional<std::uint8_t> climate =
            template_climate(*bytes, config.monopoly_climate_mask.size());
        if (!climate) {
            return at + ".key: template '" + island.key + "' has no INFO or an unknown climate";
        }
        placed.push_back(Placed{&island, static_cast<std::int32_t>(island.x),
                                static_cast<std::int32_t>(island.y), summary.w, summary.h, *climate,
                                found->second});
    }
    std::vector<std::vector<std::uint8_t>> bytes(placed.size());
    std::vector<formats::Bytes> views(placed.size());
    sim::LinkResolution resolution;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const Placed& item = placed[i];
        auto file = read_bytes(templates.files[item.template_index]);
        if (!file) {
            return "islands[" + std::to_string(i) + "].key: cannot read the template";
        }
        bytes[i] = std::move(*file);
        views[i] = bytes[i];
        sim::IslandLinkSpec link;
        link.number = static_cast<std::uint32_t>(item.layout->id);
        link.x = static_cast<std::uint32_t>(item.x);
        link.y = static_cast<std::uint32_t>(item.y);
        link.w = item.w;
        link.h = item.h;
        link.climate = item.climate;
        link.path = item.layout->key;
        link.where = "manifest.islands[" + std::to_string(i) + "]";
        out.links.push_back(std::move(link));
        sim::ResolvedLink resolved;
        resolved.candidate = 0;
        resolution.links.push_back(std::move(resolved));
    }
    out.links_total = out.links.size();
    if (auto error = sim::place_island_links(out, resolution, views, *types, config)) {
        return *error;
    }
    // place_island_links gives each island its content and no owner: every tile stays neutral
    // and no city is added (the developer start is not applied).

    // 6. The expedition: ship, scout, donkey per seat, in that order.
    const std::uint32_t kinds[3] = {static_cast<std::uint32_t>(loadout.ship_figure),
                                    static_cast<std::uint32_t>(loadout.scout_figure),
                                    static_cast<std::uint32_t>(loadout.donkey_figure)};
    const std::int64_t world_w = manifest.edge_w * 256, world_h = manifest.edge_h * 256;
    for (std::size_t seat = 0; seat < manifest.spawns.size(); ++seat) {
        const sm::Spawn& spawn = manifest.spawns[seat];
        const std::string at = "spawns[" + std::to_string(seat) + "]";
        const auto x = milli_tile_to_fixed(spawn.x);
        if (!x) {
            return at + ".x: " + std::to_string(spawn.x) + " " + x.error();
        }
        const auto y = milli_tile_to_fixed(spawn.y);
        if (!y) {
            return at + ".y: " + std::to_string(spawn.y) + " " + y.error();
        }
        if (x.value() < 0 || y.value() < 0 || x.value() >= world_w || y.value() >= world_h) {
            return at + ": (" + std::to_string(spawn.x) + ", " + std::to_string(spawn.y) +
                   ") is outside the world";
        }
        const std::optional<std::uint8_t> dir16 = facing_to_dir16(spawn.facing);
        if (!dir16) {
            return at + ".facing: " + std::to_string(spawn.facing) + " is outside 0..3";
        }
        const auto base_index = static_cast<std::uint32_t>(out.spec.figures.size());
        for (std::uint32_t member = kShip; member <= kDonkey; ++member) {
            sim::ScenarioSpec::FigureSpec f;
            f.kind = kinds[member];
            f.owner = sim::PlayerId{static_cast<std::uint32_t>(seat)};
            f.pos = sim::FixedPos{x.value(), y.value()};
            f.dir16 = *dir16;
            if (member == kShip) {
                f.holds = holds;
            } else if (member == kScout) {
                f.parent = base_index; // the ship
                f.crew_slot = 0;
            } else {
                f.parent = base_index + 1; // the scout, not the ship
            }
            out.spec.figures.push_back(std::move(f));
        }
    }

    auto world = sim::World::create(out.spec, std::move(types), config, rules, {}, ex);
    if (!world) {
        return world.error();
    }
    if (const std::string problem = expedition_baseline_problem(world.value()); !problem.empty()) {
        return "expedition: " + problem;
    }
    return world;
}

std::optional<std::string> install_ship_start(sim::World& target, const sm::Manifest& manifest,
                                              const fs::path& game_dir,
                                              std::shared_ptr<const sim::TypeRegistry> types,
                                              const sim::SimConfig& config, const sim::Rules& rules,
                                              const sim::Executor& ex) {
    auto built = load_ship_start(manifest, game_dir, std::move(types), config, rules, ex);
    if (!built) {
        return built.error();
    }
    target = std::move(built).value();
    return std::nullopt;
}

std::string expedition_baseline_problem(const sim::World& world) {
    const sim::FigureRegistry& registry = world.figures();
    const sim::TypeRegistry& types = world.types();
    if (world.ids().next_figure != 16) {
        return "ids.next_figure is " + std::to_string(world.ids().next_figure) + ", not 16";
    }
    for (const std::optional<sim::Island>& island : world.islands()) {
        if (!island) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (city) {
                return "an island holds a city";
            }
        }
    }
    std::size_t live = 0;
    for (const auto& slot : registry.list.slots) {
        live += slot.has_value() ? 1 : 0;
    }
    if (live != 15) {
        return std::to_string(live) + " figures, not 15";
    }
    const sm::Loadout& baseline = sm::baseline_loadout();
    for (std::uint32_t seat = 0; seat < 5; ++seat) {
        const std::uint64_t first = 3ULL * seat + 1;
        const sim::Figure* f[3];
        for (std::uint32_t k = 0; k < 3; ++k) {
            f[k] = sim::find_figure(registry, sim::FigureId{first + k});
            if (f[k] == nullptr) {
                return "figure " + std::to_string(first + k) + " is missing";
            }
        }
        const std::string who = "seat " + std::to_string(seat) + ": ";
        const std::int64_t kinds[3] = {baseline.ship_figure, baseline.scout_figure,
                                       baseline.donkey_figure};
        for (std::uint32_t k = 0; k < 3; ++k) {
            if (f[k]->kind != kinds[k]) {
                return who + "figure " + std::to_string(f[k]->id.v) + " has kind " +
                       std::to_string(f[k]->kind);
            }
            if (f[k]->owner != sim::PlayerId{seat}) {
                return who + "figure " + std::to_string(f[k]->id.v) + " has another owner";
            }
            if (f[k]->home.valid() || f[k]->cargo_goods != 0 || f[k]->cargo_88 != 0) {
                return who + "figure " + std::to_string(f[k]->id.v) +
                       " has a home or a single-cargo load";
            }
            if (f[k]->pos.x != f[0]->pos.x || f[k]->pos.y != f[0]->pos.y ||
                f[k]->dir16 != f[0]->dir16) {
                return who + "figure " + std::to_string(f[k]->id.v) +
                       " is not at the ship's position and facing";
            }
        }
        if (f[0]->parent.valid() || f[0]->crew_slot.has_value()) {
            return who + "the ship has a parent or a crew slot";
        }
        if (f[1]->parent != f[0]->id || f[1]->crew_slot != std::optional<std::uint32_t>(0)) {
            return who + "the scout is not crew slot 0 of the ship";
        }
        if (f[2]->parent != f[1]->id || f[2]->crew_slot.has_value()) {
            return who + "the donkey's parent is not the scout (or it has a crew slot)";
        }
        if (!f[1]->holds.empty() || !f[2]->holds.empty()) {
            return who + "the scout or the donkey has holds";
        }
        if (f[0]->holds.size() != baseline.holds.size()) {
            return who + "the ship has " + std::to_string(f[0]->holds.size()) + " holds";
        }
        for (std::size_t h = 0; h < baseline.holds.size(); ++h) {
            const auto goods = manifest_goods(types, baseline.holds[h].goods);
            if (!goods || f[0]->holds[h].goods != *goods ||
                f[0]->holds[h].tons != baseline.holds[h].tons) {
                return who + "hold " + std::to_string(h) + " is not the baseline's";
            }
        }
        const sim::Player* player = world.players().find(sim::PlayerId{seat});
        if (player == nullptr || player->money != baseline.gold) {
            return who + "the player's money is not " + std::to_string(baseline.gold);
        }
    }
    return {};
}

} // namespace anno::sim_load
