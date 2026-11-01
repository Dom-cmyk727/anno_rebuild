// Scratch generator (round ec, D2; from the round-ea smoke generator): finds placements the sim
// accepts for the developer city of a fixed map, each tried in turn in a trial world, and writes a
// --commands file with the chosen schedule. Acceptance in the measured run is read from the trace
// (sim.tsv), not assumed from this trial.
//   workload_commands <game_dir> <map> <out> one|burst|sustained <count> <first_turn> <every>
// one: <count> = 1; burst: <count> builds in turn <first_turn>; sustained: one every <every> turns.
// Prints the city's centre tile (for ANNO_VIEW_AT) on stdout.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

int main(int argc, char** argv) {
    if (argc != 8) {
        std::fprintf(stderr, "usage: workload_commands <game_dir> <map> <out> one|burst|sustained "
                             "<count> <first_turn> <every>\n");
        return 2;
    }
    const std::string mode = argv[4];
    const std::size_t count = std::strtoul(argv[5], nullptr, 10);
    const std::uint64_t first = std::strtoull(argv[6], nullptr, 10);
    const std::uint64_t every = std::strtoull(argv[7], nullptr, 10);
    auto load = app::load_play(argv[1], argv[2]);
    if (!load) {
        std::fprintf(stderr, "%s\n", load.error().c_str());
        return 1;
    }
    sim::World& world = load.value().world;
    std::optional<sim::CityRef> city;
    std::optional<sim::TileXY> origin;
    std::uint32_t w = 0, h = 0;
    for (std::size_t i = 0; i < world.islands().size() && !city; ++i) {
        if (!world.islands()[i]) {
            continue;
        }
        for (const auto& c : world.islands()[i]->cities) {
            if (c && c->owner == app::developer_player() && !city) {
                city = sim::CityRef{world.islands()[i]->index, c->number};
                origin = sim::TileXY{world.islands()[i]->tiles.x0(), world.islands()[i]->tiles.y0()};
                w = world.islands()[i]->tiles.width();
                h = world.islands()[i]->tiles.height();
            }
        }
    }
    if (!city) {
        std::fprintf(stderr, "no developer city\n");
        return 1;
    }
    std::optional<std::uint32_t> cheap;
    std::int64_t cheap_gold = 0;
    for (std::uint32_t id = 1; id <= 0xFFFFu; ++id) {
        const sim::BuildingType* type = world.types().find(sim::TypeId{id});
        if (type == nullptr || !type->cost_goods.empty() ||
            !app::anlage_is_building(world, static_cast<std::uint16_t>(id))) {
            continue;
        }
        if (!cheap || static_cast<std::int64_t>(type->cost_gold) < cheap_gold) {
            cheap = id;
            cheap_gold = static_cast<std::int64_t>(type->cost_gold);
        }
    }
    if (!cheap) {
        std::fprintf(stderr, "no cheap type\n");
        return 1;
    }
    app::PlayRunner runner(std::move(world));
    std::vector<sim::Command> found;
    std::uint64_t trials = 0;
    for (std::uint32_t y = 4; y + 4 < h && found.size() < count; y += 3) {
        for (std::uint32_t x = 4; x + 4 < w && found.size() < count; x += 3) {
            sim::Command cmd;
            cmd.issuer = app::developer_player();
            sim::BuildCmd build;
            build.city = *city;
            sim::PlacementWire p;
            p.type = sim::TypeId{*cheap};
            p.x = origin->x + static_cast<std::int32_t>(x);
            p.y = origin->y + static_cast<std::int32_t>(y);
            p.rotation = 0;
            build.placements.push_back(p);
            cmd.body = build;
            runner.post_command(cmd);
            runner.run_n(1);
            ++trials;
            const bool ok = !runner.take_building_batches().empty();
            (void)runner.take_rejections();
            if (ok) {
                found.push_back(cmd);
            }
        }
    }
    if (found.size() < count) {
        std::fprintf(stderr, "found %zu of %zu accepted placements\n", found.size(), count);
        return 1;
    }
    std::vector<app::ScriptedCommand> script;
    for (std::size_t i = 0; i < found.size(); ++i) {
        const std::uint64_t turn = mode == "burst" ? first : first + every * i;
        script.push_back({turn, found[i]});
    }
    const auto text = app::format_commands(script);
    if (!text) {
        std::fprintf(stderr, "%s\n", text.error().message.c_str());
        return 1;
    }
    std::ofstream(argv[3]) << text.value();
    std::fprintf(stderr, "type %u (gold %lld), %zu builds, trial turns %llu\n", *cheap,
                 static_cast<long long>(cheap_gold), found.size(),
                 static_cast<unsigned long long>(trials));
    std::printf("%d,%d\n", origin->x + static_cast<std::int32_t>(w / 2),
                origin->y + static_cast<std::int32_t>(h / 2));
    return 0;
}
