#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/chunk.hpp>
#include <anno/formats/insel.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/scenario_load.hpp>

namespace anno::sim_load {
namespace {

namespace fs = std::filesystem;

std::optional<std::vector<std::uint8_t>> read_bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

bool ranges_overlap(std::int64_t a, std::uint32_t aw, std::int64_t b, std::uint32_t bw) {
    return a < b + static_cast<std::int64_t>(bw) && b < a + static_cast<std::int64_t>(aw);
}

/// INFO[0] of the template's INSEL5, or nullopt when the record is missing or
/// the byte is not a climate. template_summary zero-fills a missing INFO, which
/// would look like polar; the layout must not get that by accident.
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

} // namespace

formats::Result<sim::LoadedScenario, std::string>
load_fixed_map(const formats::fixed_map::FixedMap& map, std::span<const sim::Seat> seats,
               const fs::path& game_dir, const sim::TypeRegistry& types,
               const sim::SimConfig& config, FixedMapOptions options) {
    const std::size_t regions = map.starts.size();
    if (seats.size() < 2 || seats.size() > regions) {
        return "seats: " + std::to_string(seats.size()) + " is not from 2 to " +
               std::to_string(regions) + " start regions";
    }
    for (std::size_t i = 0; i < map.starts.size(); ++i) {
        if (map.starts[i].seat != i) {
            return "start seats must be 0.." + std::to_string(map.starts.size() - 1) +
                   " without gaps";
        }
    }

    std::map<std::uint32_t, const formats::fixed_map::Island*> by_slot;
    for (const formats::fixed_map::Island& island : map.islands) {
        by_slot.emplace(island.slot, &island);
    }
    for (const formats::fixed_map::Start& start : map.starts) {
        if (by_slot.find(start.island) == by_slot.end()) {
            return "start seat " + std::to_string(start.seat) + ": island slot " +
                   std::to_string(start.island) + " is not in the layout";
        }
    }
    if (options.developer_start) {
        std::map<std::uint32_t, std::uint32_t> claimed;
        for (const formats::fixed_map::Start& start : map.starts) {
            if (!claimed.emplace(start.island, start.seat).second) {
                return "start island slot " + std::to_string(start.island) + " is used twice";
            }
        }
        for (std::size_t seat = 0; seat < seats.size(); ++seat) {
            if (seat == config.neutral_owner || seat >= config.city_slots) {
                return "seat " + std::to_string(seat) +
                       ": city number is neutral_owner or not below city_slots";
            }
        }
    }

    const fs::path base_path = game_dir / fs::path(map.base);
    const std::optional<std::vector<std::uint8_t>> base_bytes = read_bytes(base_path);
    if (!base_bytes) {
        return "base: cannot read " + base_path.string();
    }
    auto loaded = sim::load_scenario(*base_bytes, types, config);
    if (!loaded) {
        return "base: " + loaded.error();
    }
    sim::LoadedScenario out = std::move(loaded).value();

    std::optional<std::int64_t> human_money;
    for (const sim::ScenarioSpec::PlayerSpec& player : out.spec.players) {
        // Kind 8 and kind 11 are both human (docs/formats/sze.md, players).
        // 03_1024_med_256.sze has a kind-11 human and no kind 8.
        if (player.kind == 8 || player.kind == 11) {
            human_money = player.money;
            break;
        }
    }
    if (!map.money && !human_money) {
        return std::string("base: no human player");
    }

    out.spec.settings.world_width = map.width;
    out.spec.settings.world_height = map.height;
    out.spec.seed = map.seed;
    if (const std::string problem = out.spec.settings.validate(); !problem.empty()) {
        return problem;
    }
    out.spec.islands.clear();
    out.spec.players.clear();
    out.links.clear();
    out.link_inputs = {};
    out.links_total = 0;
    out.links_placed = 0;

    const std::int64_t money = map.money ? static_cast<std::int64_t>(*map.money) : *human_money;
    for (std::size_t i = 0; i < seats.size(); ++i) {
        if (seats[i].colour > 0xff) {
            return "seat " + std::to_string(i) + ": colour does not fit a scenario colour";
        }
        sim::ScenarioSpec::PlayerSpec player;
        player.id = sim::PlayerId{static_cast<std::uint32_t>(i)};
        player.kind = 8;
        player.volk = 1;
        player.money = money;
        player.colour = static_cast<std::uint8_t>(seats[i].colour);
        player.name.assign(seats[i].name.begin(), seats[i].name.end());
        out.spec.players.push_back(std::move(player));
    }

    const IslandTemplates templates = scan_island_templates(game_dir);
    std::map<std::string, std::size_t> by_name;
    for (std::size_t i = 0; i < templates.summaries.size(); ++i) {
        by_name.emplace(templates.summaries[i].name, i);
    }

    struct Placed {
        const formats::fixed_map::Island* layout = nullptr;
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        std::uint8_t climate = 0;
    };
    std::vector<Placed> placed;
    placed.reserve(map.islands.size());
    for (const formats::fixed_map::Island& island : map.islands) {
        if (island.slot >= config.island_slots) {
            return "island slot " + std::to_string(island.slot) + ": not below island_slots";
        }
        const auto found = by_name.find(island.key);
        if (found == by_name.end()) {
            return "unknown template '" + island.key + "'";
        }
        const sim::TemplateSummary& summary = templates.summaries[found->second];
        const std::int64_t x1 = static_cast<std::int64_t>(island.x) + summary.w;
        const std::int64_t y1 = static_cast<std::int64_t>(island.y) + summary.h;
        if (island.x < 0 || island.y < 0 || x1 > map.width || y1 > map.height) {
            return "island slot " + std::to_string(island.slot) + ": outside the world";
        }
        for (const Placed& earlier : placed) {
            if (ranges_overlap(island.x, summary.w, earlier.layout->x, earlier.w) &&
                ranges_overlap(island.y, summary.h, earlier.layout->y, earlier.h)) {
                const std::uint32_t lo = std::min(island.slot, earlier.layout->slot);
                const std::uint32_t hi = std::max(island.slot, earlier.layout->slot);
                return "island slot " + std::to_string(hi) + ": overlaps island slot " +
                       std::to_string(lo);
            }
        }
        const std::optional<std::vector<std::uint8_t>> bytes =
            read_bytes(templates.files[found->second]);
        if (!bytes) {
            return "unknown template '" + island.key + "'";
        }
        const std::optional<std::uint8_t> climate =
            template_climate(*bytes, config.monopoly_climate_mask.size());
        if (!climate) {
            return "template '" + island.key + "': no INFO or an unknown climate";
        }
        placed.push_back(Placed{&island, summary.w, summary.h, *climate});
    }

    const sim::BuildingType* kontor = nullptr;
    if (options.developer_start) {
        kontor = types.find(sim::TypeId{options.kontor_type});
        if (kontor == nullptr) {
            return "kontor type " + std::to_string(options.kontor_type) + " is not in the registry";
        }
        for (const formats::fixed_map::Start& start : map.starts) {
            if (start.seat >= seats.size()) {
                continue;
            }
            const Placed* home = nullptr;
            for (const Placed& item : placed) {
                if (item.layout->slot == start.island) {
                    home = &item;
                    break;
                }
            }
            if (home == nullptr) {
                return "start seat " + std::to_string(start.seat) + ": island slot " +
                       std::to_string(start.island) + " was not placed";
            }
            const auto [fw, fh] =
                sim::footprint(*kontor, static_cast<std::uint8_t>(start.rotation));
            const std::int64_t lx = static_cast<std::int64_t>(start.x) - home->layout->x;
            const std::int64_t ly = static_cast<std::int64_t>(start.y) - home->layout->y;
            if (lx < 0 || ly < 0 || lx + fw > home->w || ly + fh > home->h) {
                return "start seat " + std::to_string(start.seat) +
                       ": the kontor footprint is outside island slot " +
                       std::to_string(start.island);
            }
        }
    }

    std::vector<std::vector<std::uint8_t>> bytes(placed.size());
    std::vector<formats::Bytes> views(placed.size());
    sim::LinkResolution resolution;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const Placed& item = placed[i];
        const auto found = by_name.find(item.layout->key);
        auto file = read_bytes(templates.files[found->second]);
        if (!file) {
            return "unknown template '" + item.layout->key + "'";
        }
        bytes[i] = std::move(*file);
        views[i] = bytes[i];
        sim::IslandLinkSpec link;
        link.number = item.layout->slot;
        link.x = static_cast<std::uint32_t>(item.layout->x);
        link.y = static_cast<std::uint32_t>(item.layout->y);
        link.w = item.w;
        link.h = item.h;
        link.climate = item.climate;
        link.path = item.layout->key;
        link.where = "fixed-map.island[" + std::to_string(i) + "]";
        out.links.push_back(std::move(link));
        sim::ResolvedLink resolved;
        resolved.candidate = 0;
        resolution.links.push_back(std::move(resolved));
    }
    out.links_total = out.links.size();
    if (auto error = sim::place_island_links(out, resolution, views, types, config)) {
        return *error;
    }

    if (options.developer_start) {
        for (const formats::fixed_map::Start& start : map.starts) {
            if (start.seat >= seats.size()) {
                continue;
            }
            sim::ScenarioSpec::IslandSpec* island = nullptr;
            for (sim::ScenarioSpec::IslandSpec& spec : out.spec.islands) {
                if (spec.slot.v == start.island) {
                    island = &spec;
                    break;
                }
            }
            if (island == nullptr) {
                return "start seat " + std::to_string(start.seat) + ": island slot " +
                       std::to_string(start.island) + " was not placed";
            }
            const std::uint64_t tiles = std::uint64_t{island->w} * island->h;
            if (tiles > std::numeric_limits<std::uint32_t>::max()) {
                return "island slot " + std::to_string(start.island) +
                       ": too many tiles for one owner run";
            }
            sim::ScenarioSpec::CitySpec city;
            city.no = sim::CityNo{start.seat};
            city.owner = sim::PlayerId{start.seat};
            sim::ScenarioSpec::BuildingSpec building;
            building.type = sim::TypeId{options.kontor_type};
            building.x = start.x;
            building.y = start.y;
            building.rotation = static_cast<std::uint8_t>(start.rotation);
            city.buildings.push_back(building);
            if (options.resources.enabled) {
                // Test resources (DeveloperResources): the same goods stock in every goods
                // number of the warehouse, goods 0 (UNUSED) empty, and the capacity raised
                // to hold it. Money is set on the seat's player below.
                const std::uint32_t units = options.resources.goods_tons * sim::kTon;
                std::vector<std::uint32_t> stock(config.warehouse_goods, units);
                if (!stock.empty()) {
                    stock[0] = 0;
                }
                city.stock = std::move(stock);
                city.capacity = units;
                for (sim::ScenarioSpec::PlayerSpec& player : out.spec.players) {
                    if (player.id.v == start.seat) {
                        player.money = options.resources.gold;
                    }
                }
            }
            island->cities.push_back(std::move(city));
            island->owner_runs.push_back({start.seat, static_cast<std::uint32_t>(tiles)});
        }
    }
    return out;
}

} // namespace anno::sim_load
