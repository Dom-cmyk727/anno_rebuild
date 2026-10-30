#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

#include <anno/app/commands_file.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/replay.hpp>

namespace anno::app {

namespace {

// Reads a whole decimal integer from `token`; nullopt when it is not one.
template <class T> std::optional<T> number(std::string_view token) {
    T value{};
    const auto* first = token.data();
    const auto* last = token.data() + token.size();
    const auto [end, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string_view> split(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) {
            ++i;
        }
        const std::size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') {
            ++i;
        }
        if (i > start) {
            out.push_back(line.substr(start, i - start));
        }
    }
    return out;
}

// The island (slot) whose rectangle holds the tile, if any.
std::optional<std::uint32_t> island_at(const sim::World& world, std::int32_t x, std::int32_t y) {
    const auto& islands = world.islands();
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i]) {
            continue;
        }
        const auto& t = islands[i]->tiles;
        if (x >= t.x0() && y >= t.y0() && x < t.x0() + static_cast<std::int32_t>(t.width()) &&
            y < t.y0() + static_cast<std::int32_t>(t.height())) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return std::nullopt;
}

// The city of `player` on island `slot`, if it has one.
std::optional<sim::CityRef> city_of(const sim::World& world, std::uint32_t slot,
                                    sim::PlayerId player) {
    const auto& island = world.islands()[slot];
    for (const std::optional<sim::City>& city : island->cities) {
        if (city && city->owner == player) {
            return sim::CityRef{sim::IslandId{slot}, city->number};
        }
    }
    return std::nullopt;
}

// One line. Returns the scripted command, an error, or nothing for a blank or comment line.
formats::Result<std::optional<ScriptedCommand>, std::string> parse_line(std::string_view line,
                                                                        std::size_t number_1based,
                                                                        const sim::World& world,
                                                                        sim::PlayerId player) {
    const std::string where = "line " + std::to_string(number_1based) + ": ";
    const auto hash = line.find('#');
    if (hash != std::string_view::npos) {
        line = line.substr(0, hash);
    }
    const std::vector<std::string_view> tok = split(line);
    if (tok.empty()) {
        return std::optional<ScriptedCommand>{};
    }
    if (tok.size() < 4 || tok[0] != "turn") {
        return where + "expected `turn <n> build|demolish ...`";
    }
    const auto turn = number<std::uint64_t>(tok[1]);
    if (!turn) {
        return where + "the turn is not a number";
    }
    ScriptedCommand out;
    out.turn = *turn;
    out.command.issuer = player;

    if (tok[2] == "build") {
        // turn <n> build <type> <x> <y> <rotation> [<x> <y> <rotation>]...
        // Tokens after <type>: the triples. Index 3 is the type.
        const std::size_t rest = tok.size() - 4;
        if (rest < 3 || rest % 3 != 0) {
            return where + "build takes <type> then triples <x> <y> <rotation>";
        }
        const auto type = number<std::uint32_t>(tok[3]);
        if (!type) {
            return where + "the type is not a number";
        }
        sim::BuildCmd build;
        std::optional<std::uint32_t> island;
        for (std::size_t k = 4; k < tok.size(); k += 3) {
            const auto x = number<std::int32_t>(tok[k]);
            const auto y = number<std::int32_t>(tok[k + 1]);
            const auto rotation = number<std::uint8_t>(tok[k + 2]);
            if (!x || !y || !rotation) {
                return where + "a placement is <x> <y> <rotation> with numbers";
            }
            if (*rotation > 3) {
                return where + "the rotation is 0..3";
            }
            if (!island) {
                island = island_at(world, *x, *y);
                if (!island) {
                    return where + "the first placement is on no island";
                }
            }
            sim::PlacementWire placement;
            placement.type = sim::TypeId{*type};
            placement.x = *x;
            placement.y = *y;
            placement.rotation = *rotation;
            build.placements.push_back(placement);
        }
        const auto city = city_of(world, *island, player);
        if (!city) {
            return where + "player " + std::to_string(player.v) + " has no city on island " +
                   std::to_string(*island);
        }
        build.city = *city;
        out.command.body = std::move(build);
    } else if (tok[2] == "demolish") {
        // turn <n> demolish <x> <y> [<x> <y>]...
        const std::size_t rest = tok.size() - 3;
        if (rest < 2 || rest % 2 != 0) {
            return where + "demolish takes pairs <x> <y>";
        }
        sim::DemolishCmd demolish;
        std::optional<std::uint32_t> island;
        for (std::size_t k = 3; k < tok.size(); k += 2) {
            const auto x = number<std::int32_t>(tok[k]);
            const auto y = number<std::int32_t>(tok[k + 1]);
            if (!x || !y) {
                return where + "a tile is <x> <y> with numbers";
            }
            if (!island) {
                island = island_at(world, *x, *y);
                if (!island) {
                    return where + "the first tile is on no island";
                }
            }
            demolish.tiles.push_back(sim::TileXY{*x, *y});
        }
        const auto city = city_of(world, *island, player);
        if (!city) {
            return where + "player " + std::to_string(player.v) + " has no city on island " +
                   std::to_string(*island);
        }
        demolish.city = *city;
        out.command.body = std::move(demolish);
    } else {
        return where + "unknown command `" + std::string(tok[2]) + "`";
    }

    // The wire limits (command.hpp): a line that has no wire form is refused here,
    // because the session drivers would skip it without a word.
    if (!sim::has_wire_form(out.command.body)) {
        return where + "more placements or tiles than one command holds (4095 / 8190)";
    }
    return std::optional<ScriptedCommand>{std::move(out)};
}

} // namespace

sim::PlayerId developer_player() {
    return sim::PlayerId{1};
}

formats::Result<std::vector<ScriptedCommand>, std::string>
parse_commands(std::string_view text, const sim::World& world, sim::PlayerId player) {
    std::vector<ScriptedCommand> out;
    std::size_t line_no = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        ++line_no;
        auto parsed = parse_line(text.substr(start, end - start), line_no, world, player);
        if (!parsed.ok()) {
            return parsed.error();
        }
        if (parsed.value()) {
            if (!out.empty() && parsed.value()->turn < out.back().turn) {
                return "line " + std::to_string(line_no) + ": turns must not decrease (turn " +
                       std::to_string(parsed.value()->turn) + " after turn " +
                       std::to_string(out.back().turn) + ")";
            }
            out.push_back(std::move(*parsed.value()));
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }
    return out;
}

formats::Result<std::vector<ScriptedCommand>, std::string>
load_commands(const std::filesystem::path& file, const sim::World& world, sim::PlayerId player) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::string("cannot read the commands file");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse_commands(buffer.str(), world, player);
}

} // namespace anno::app
