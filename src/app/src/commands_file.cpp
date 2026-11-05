#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>
#include <variant>

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
        return where +
               "expected `turn <n> build|demolish|buildc|demolishc|found|move|stop|unload ...`";
    }
    const auto turn = number<std::uint64_t>(tok[1]);
    if (!turn) {
        return where + "the turn is not a number";
    }
    ScriptedCommand out;
    out.turn = *turn;
    out.command.issuer = player;
    if (tok[2] == "buildc" || tok[2] == "demolishc" || tok[2] == "found" || tok[2] == "move" ||
        tok[2] == "stop" || tok[2] == "unload") {
        // The ship-start forms: the issuer is on the line and nothing is looked up in the world.
        auto issuer = number<std::uint32_t>(tok[3]);
        if (!issuer) {
            return where + "the issuer is not a number";
        }
        out.command.issuer = sim::PlayerId{*issuer};
        auto num = [&](std::size_t k) { return number<std::int64_t>(tok[k]); };
        if (tok[2] == "buildc") {
            // buildc <issuer> <island> <city> <type> <x> <y> <rot> ...
            const std::size_t rest = tok.size() >= 7 ? tok.size() - 7 : 1;
            if (tok.size() < 7 || rest % 3 != 0) {
                return where + "buildc takes <issuer> <island> <city> <type> then <x> <y> "
                               "<rotation> triples";
            }
            const auto island = number<std::uint32_t>(tok[4]);
            const auto city = number<std::uint32_t>(tok[5]);
            const auto type = number<std::uint32_t>(tok[6]);
            if (!island || !city || !type) {
                return where + "island, city and type are numbers";
            }
            sim::BuildCmd build;
            build.city = sim::CityRef{sim::IslandId{*island}, sim::CityNo{*city}};
            for (std::size_t k = 7; k < tok.size(); k += 3) {
                const auto x = number<std::int32_t>(tok[k]);
                const auto y = number<std::int32_t>(tok[k + 1]);
                const auto rotation = number<std::uint8_t>(tok[k + 2]);
                if (!x || !y || !rotation || *rotation > 3) {
                    return where + "a placement is <x> <y> <rotation 0..3> with numbers";
                }
                build.placements.push_back(
                    sim::PlacementWire{sim::TypeId{*type}, *x, *y, *rotation, 0, 0});
            }
            out.command.body = std::move(build);
        } else if (tok[2] == "demolishc") {
            if (tok.size() < 8 || (tok.size() - 6) % 2 != 0) {
                return where + "demolishc takes <issuer> <island> <city> then <x> <y> pairs";
            }
            const auto island = number<std::uint32_t>(tok[4]);
            const auto city = number<std::uint32_t>(tok[5]);
            if (!island || !city) {
                return where + "island and city are numbers";
            }
            sim::DemolishCmd demolish;
            demolish.city = sim::CityRef{sim::IslandId{*island}, sim::CityNo{*city}};
            for (std::size_t k = 6; k < tok.size(); k += 2) {
                const auto x = number<std::int32_t>(tok[k]);
                const auto y = number<std::int32_t>(tok[k + 1]);
                if (!x || !y) {
                    return where + "a tile is <x> <y> with numbers";
                }
                demolish.tiles.push_back(sim::TileXY{*x, *y});
            }
            out.command.body = std::move(demolish);
        } else if (tok[2] == "found") {
            if (tok.size() != 11) {
                return where +
                       "found takes <issuer> <ship> <type> <rotation> <x> <y> <click x> <click y>";
            }
            const auto ship = number<std::uint64_t>(tok[4]);
            const auto type = number<std::uint32_t>(tok[5]);
            const auto rotation = number<std::uint8_t>(tok[6]);
            const auto x = num(7), y = num(8), cx = num(9), cy = num(10);
            if (!ship || !type || !rotation || *rotation > 3 || !x || !y || !cx || !cy ||
                *x != static_cast<std::int32_t>(*x) || *y != static_cast<std::int32_t>(*y) ||
                *cx != static_cast<std::int32_t>(*cx) || *cy != static_cast<std::int32_t>(*cy)) {
                return where + "found: numbers expected (rotation 0..3, 32-bit tiles)";
            }
            sim::FoundCmd found;
            found.ship = sim::FigureId{*ship};
            found.placement.type = sim::TypeId{*type};
            found.placement.rotation = *rotation;
            found.placement.x = static_cast<std::int32_t>(*x);
            found.placement.y = static_cast<std::int32_t>(*y);
            found.click_x = static_cast<std::int32_t>(*cx);
            found.click_y = static_cast<std::int32_t>(*cy);
            out.command.body = found;
        } else if (tok[2] == "move") {
            if (tok.size() != 7) {
                return where + "move takes <issuer> <ship> <target x> <target y> (Q24.8)";
            }
            const auto ship = number<std::uint64_t>(tok[4]);
            const auto x = number<std::int32_t>(tok[5]);
            const auto y = number<std::int32_t>(tok[6]);
            if (!ship || !x || !y) {
                return where + "move: numbers expected";
            }
            out.command.body = sim::MoveCmd{sim::FigureId{*ship}, *x, *y};
        } else if (tok[2] == "stop") {
            if (tok.size() != 5) {
                return where + "stop takes <issuer> <ship>";
            }
            const auto ship = number<std::uint64_t>(tok[4]);
            if (!ship) {
                return where + "stop: the ship is not a number";
            }
            out.command.body = sim::StopCmd{sim::FigureId{*ship}};
        } else {
            if (tok.size() != 9) {
                return where + "unload takes <issuer> <ship> <warehouse> <hold> <goods> <tons>";
            }
            const auto ship = number<std::uint64_t>(tok[4]);
            const auto warehouse = number<std::uint64_t>(tok[5]);
            const auto hold = number<std::uint32_t>(tok[6]);
            const auto goods = number<std::uint32_t>(tok[7]);
            const auto tons = number<std::uint32_t>(tok[8]);
            if (!ship || !warehouse || !hold || !goods || !tons) {
                return where + "unload: numbers expected";
            }
            sim::UnloadCmd unload;
            unload.ship = sim::FigureId{*ship};
            unload.warehouse = sim::BuildingId{*warehouse};
            unload.hold = *hold;
            unload.goods = *goods;
            unload.tons = *tons;
            out.command.body = unload;
        }
        if (!sim::has_wire_form(out.command.body)) {
            return where + "more placements or tiles than one command holds (4095 / 8190)";
        }
        return std::optional<ScriptedCommand>{std::move(out)};
    }

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

formats::Result<std::string, FormatError>
format_commands(const std::vector<ScriptedCommand>& commands, bool ship_start) {
    std::string out;
    for (const ScriptedCommand& scripted : commands) {
        const std::string turn = "turn " + std::to_string(scripted.turn) + " ";
        const std::string who = std::to_string(scripted.command.issuer.v);
        if (ship_start) {
            // The ship-start session form: every line carries its issuer.
            if (const auto* f = std::get_if<sim::FoundCmd>(&scripted.command.body)) {
                if (f->placement.byte7 != 0 || f->placement.byte8 != 0) {
                    return FormatError{"a founding with byte7/byte8 set has no line form"};
                }
                out += turn + "found " + who + " " + std::to_string(f->ship.v) + " " +
                       std::to_string(f->placement.type.v) + " " +
                       std::to_string(f->placement.rotation) + " " +
                       std::to_string(f->placement.x) + " " + std::to_string(f->placement.y) + " " +
                       std::to_string(f->click_x) + " " + std::to_string(f->click_y) + "\n";
                continue;
            }
            if (const auto* m = std::get_if<sim::MoveCmd>(&scripted.command.body)) {
                out += turn + "move " + who + " " + std::to_string(m->ship.v) + " " +
                       std::to_string(m->target_x_q8) + " " + std::to_string(m->target_y_q8) + "\n";
                continue;
            }
            if (const auto* s = std::get_if<sim::StopCmd>(&scripted.command.body)) {
                out += turn + "stop " + who + " " + std::to_string(s->ship.v) + "\n";
                continue;
            }
            if (const auto* u = std::get_if<sim::UnloadCmd>(&scripted.command.body)) {
                out += turn + "unload " + who + " " + std::to_string(u->ship.v) + " " +
                       std::to_string(u->warehouse.v) + " " + std::to_string(u->hold) + " " +
                       std::to_string(u->goods) + " " + std::to_string(u->tons) + "\n";
                continue;
            }
            if (const auto* b = std::get_if<sim::BuildCmd>(&scripted.command.body)) {
                if (b->placements.empty()) {
                    return FormatError{"a build without placements has no line form"};
                }
                const sim::TypeId type = b->placements.front().type;
                out += turn + "buildc " + who + " " + std::to_string(b->city.island.v) + " " +
                       std::to_string(b->city.city.v) + " " + std::to_string(type.v);
                for (const sim::PlacementWire& p : b->placements) {
                    if (p.type != type) {
                        return FormatError{"a build with more than one type has no line form"};
                    }
                    if (p.byte7 != 0 || p.byte8 != 0) {
                        return FormatError{"a placement with byte7/byte8 set has no line form"};
                    }
                    out += ' ' + std::to_string(p.x) + ' ' + std::to_string(p.y) + ' ' +
                           std::to_string(p.rotation);
                }
                out += '\n';
                continue;
            }
            if (const auto* d = std::get_if<sim::DemolishCmd>(&scripted.command.body)) {
                out += turn + "demolishc " + who + " " + std::to_string(d->city.island.v) + " " +
                       std::to_string(d->city.city.v);
                for (const sim::TileXY& t : d->tiles) {
                    out += ' ' + std::to_string(t.x) + ' ' + std::to_string(t.y);
                }
                out += '\n';
                continue;
            }
            return FormatError{"a command of this kind has no ship-start line form"};
        }
        if (const auto* build = std::get_if<sim::BuildCmd>(&scripted.command.body)) {
            // The line names one type, then the placements as <x> <y> <rotation>.
            if (build->placements.empty()) {
                return FormatError{"a build without placements has no line form"};
            }
            const sim::TypeId type = build->placements.front().type;
            out += turn + "build " + std::to_string(type.v);
            for (const sim::PlacementWire& p : build->placements) {
                if (p.type != type) {
                    return FormatError{"a build with more than one type has no line form"};
                }
                if (p.byte7 != 0 || p.byte8 != 0) {
                    return FormatError{"a placement with byte7/byte8 set has no line form"};
                }
                out += ' ' + std::to_string(p.x) + ' ' + std::to_string(p.y) + ' ' +
                       std::to_string(p.rotation);
            }
            out += '\n';
            continue;
        }
        if (const auto* demolish = std::get_if<sim::DemolishCmd>(&scripted.command.body)) {
            out += turn + "demolish";
            for (const sim::TileXY& t : demolish->tiles) {
                out += ' ' + std::to_string(t.x) + ' ' + std::to_string(t.y);
            }
            out += '\n';
            continue;
        }
        return FormatError{"a command other than Build or Demolish has no line form"};
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
