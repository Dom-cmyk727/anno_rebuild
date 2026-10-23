// The scenario adapter (docs/sim/state.md §10, stage S14). Evidence for every
// mapping line is cited next to it: D = a disassembled function (module and
// address), P = the corpus (the spec section that states it).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim/scenario_formats.hpp>

namespace anno::sim {

namespace sze = formats::sze;
namespace insel = formats::insel;
namespace tagged = formats::tagged;
using formats::Bytes;

std::string_view warning_kind_name(ScenarioWarningKind kind) {
    switch (kind) {
    case ScenarioWarningKind::UnknownRecord:
        return "UnknownRecord";
    case ScenarioWarningKind::UndecodedCity:
        return "UndecodedCity";
    case ScenarioWarningKind::IslandLink:
        return "IslandLink";
    case ScenarioWarningKind::UnknownObject:
        return "UnknownObject";
    case ScenarioWarningKind::BuildingWithoutCity:
        return "BuildingWithoutCity";
    case ScenarioWarningKind::TileOwnersUndecoded:
        return "TileOwnersUndecoded";
    case ScenarioWarningKind::UndecodedDeposits:
        return "UndecodedDeposits";
    case ScenarioWarningKind::ObjectTileTaken:
        return "ObjectTileTaken";
    case ScenarioWarningKind::DuplicateObject:
        return "DuplicateObject";
    case ScenarioWarningKind::CityOffIsland:
        return "CityOffIsland";
    case ScenarioWarningKind::UndecodedPeople:
        return "UndecodedPeople";
    case ScenarioWarningKind::ComputerNameNotDrawn:
        return "ComputerNameNotDrawn";
    case ScenarioWarningKind::UnknownClimate:
        return "UnknownClimate";
    case ScenarioWarningKind::UnalignedIsland:
        return "UnalignedIsland";
    }
    return "?";
}

std::vector<ScenarioWarningKind> all_warning_kinds() {
    return {ScenarioWarningKind::UnknownRecord,       ScenarioWarningKind::UndecodedCity,
            ScenarioWarningKind::IslandLink,          ScenarioWarningKind::UnknownObject,
            ScenarioWarningKind::BuildingWithoutCity, ScenarioWarningKind::TileOwnersUndecoded,
            ScenarioWarningKind::UndecodedDeposits,   ScenarioWarningKind::ObjectTileTaken,
            ScenarioWarningKind::DuplicateObject,     ScenarioWarningKind::CityOffIsland,
            ScenarioWarningKind::UndecodedPeople,     ScenarioWarningKind::ComputerNameNotDrawn,
            ScenarioWarningKind::UnknownClimate,      ScenarioWarningKind::UnalignedIsland};
}

ScenarioCounts count(const LoadedScenario& s) {
    ScenarioCounts c;
    c.players = s.spec.players.size();
    c.islands = s.spec.islands.size();
    for (const ScenarioSpec::IslandSpec& is : s.spec.islands) {
        c.tile_objects += is.objects.size();
        c.deposits += is.deposits.size();
        c.cities += is.cities.size();
        for (const ScenarioSpec::CitySpec& city : is.cities) {
            c.buildings += city.buildings.size();
        }
    }
    for (const ScenarioWarning& w : s.warnings) {
        ++c.warnings[w.kind];
        if (w.kind == ScenarioWarningKind::IslandLink) {
            ++c.links;
        }
    }
    return c;
}

namespace {

/// The warnings of one load, with the "once per scope and name" rule.
struct Warnings {
    std::vector<ScenarioWarning>& out;
    std::set<std::pair<std::string, std::string>> seen_once;

    void add(ScenarioWarningKind kind, std::string path, std::string_view record,
             std::uint64_t offset, std::string message) {
        out.push_back({kind, std::move(path), std::string(record), offset, std::move(message)});
    }
    /// UnknownRecord once per scenario, `scope` and record name.
    void unknown_record(const std::string& scope, const std::string& path, std::string_view record,
                        std::uint64_t offset) {
        if (!seen_once.emplace(scope, std::string(record)).second) {
            return;
        }
        add(ScenarioWarningKind::UnknownRecord, path, record, offset,
            "record " + std::string(record) + " is not mapped (first at file offset " +
                std::to_string(offset) + ")");
    }
};

/// The bytes up to the first NUL of `raw` (a NUL-terminated char field).
std::vector<std::uint8_t> c_string(Bytes raw) {
    const auto end = std::find(raw.begin(), raw.end(), std::uint8_t{0});
    return {raw.begin(), end};
}

/// The island of the scenario whose rectangle holds the world tile (x, y), as
/// the STADT2 reader finds it (world slot +0x8C, D AnnoWelt 0x1001e294), among the
/// islands whose INSEL5 lies before `before` in the file: the original reads the
/// SCENE children in file order, so a city before its island finds none (P: no such
/// file in the corpus). `at[i]` is the file offset of islands[i].
ScenarioSpec::IslandSpec* island_at(std::vector<ScenarioSpec::IslandSpec>& islands,
                                    const std::vector<std::uint64_t>& at, std::uint64_t before,
                                    std::uint32_t x, std::uint32_t y) {
    for (std::size_t i = 0; i < islands.size(); ++i) {
        ScenarioSpec::IslandSpec& is = islands[i];
        if (at[i] >= before) {
            continue;
        }
        const auto x0 = static_cast<std::uint32_t>(is.x0);
        const auto y0 = static_cast<std::uint32_t>(is.y0);
        if (x >= x0 && y >= y0 && x - x0 < is.w && y - y0 < is.h) {
            return &is;
        }
    }
    return nullptr;
}

/// A placed item as the record holds it (for the duplicate test).
using ItemKey =
    std::tuple<std::uint16_t, std::uint8_t, std::uint16_t, std::uint16_t, std::uint16_t>;

ItemKey key_of(const insel::PlacedObject& o) {
    return {o.anlage, o.rotation, o.state, o.x, o.y};
}

/// SCENE/HEAD → WorldSettings and the world size (D AnnoScene 0x10009cc0, the HEAD
/// case 0x10009cf6–0x10009dfa: a 32-byte buffer is zero-filled (0x10009cfd–0x10009d08)
/// and the record read into it (0x1000aa40), the world is created with u32 [0] and
/// u32 [4], then world properties are set from it).
std::optional<std::string> read_head(Bytes record, sze::Edition edition, WorldSettings& s) {
    // P: 32 bytes in all 50 (sze.md "SCENE children"). A shorter record reads as zeros
    // after its end, as in the original's buffer. A longer one would be read past the
    // original's 32-byte buffer (0x1000aa40 reads the record's own length): refused.
    if (record.size() > 32) {
        return "scene.HEAD: " + std::to_string(record.size()) +
               " bytes, longer than the reader's 32-byte buffer";
    }
    std::array<std::uint8_t, 32> buf{};
    std::copy(record.begin(), record.end(), buf.begin());
    const Bytes head(buf);
    s.world_width = formats::u32_le(head, 0);  // D 0x10009d2a: first argument of world slot +0x60
    s.world_height = formats::u32_le(head, 4); // D 0x10009d24: second argument
    s.prop0 = head[8];                         // D 0x10009d4f: property 0 = [8]
    s.prop1 =
        head[0xa] != 0 ? head[0xa] : 1;    // D 0x10009d63..0x10009d86: property 1 = [0xa], 0 -> 1
    s.prop2 = head[9];                     // D 0x10009d89: property 2 = [9]
    s.prop3 = formats::u16_le(head, 0xc);  // D 0x10009da1: property 3 = u16 [0xc]
    s.prop6 = formats::u16_le(head, 0x10); // D 0x10009db9: property 6 = u16 [0x10]
    s.prop10 = head[0xb];                  // D 0x10009dd1: property 10 = [0xb]
    // D 0x10009de9: property 14 = this+0x168, which the loader sets to 1, | 2 for
    // the root ANNOADDON1 (AnnoScene 0x100025a0, docs/re/annoscene.md §1).
    s.prop14 = edition == sze::Edition::Addon ? 3U : 1U;
    return std::nullopt;
}

/// The player fields of a HEAD record (108 bytes; D AnnoWelt 0x1001d2b9–0x1001d430,
/// docs/re/annoscene.md §6, docs/formats/sze.md "Players").
void read_player_head(Bytes h, ScenarioSpec::PlayerSpec& p) {
    p.kind = formats::u16_le(h, 0x00); // +0x78: a u32 read as u16
    p.money =
        static_cast<std::int32_t>(formats::u32_le(h, 4)); // +0x74, the sim's money (player.hpp)
    p.name = c_string(h.subspan(0x08, 48));               // +0x24, NUL-terminated
    p.colour = h[0x4c];                                   // +0xA8, colour slot
    // +0xA0 = u16 [0x4e] & 0xFFBF, a value >= 0x100 read as 0.
    const std::uint32_t a0 = formats::u16_le(h, 0x4e);
    p.flags_a0 = a0 >= 0x100 ? 0U : (a0 & 0xFFBFU);
}

} // namespace

formats::Result<LoadedScenario, std::string> load_scenario(Bytes file, const TypeRegistry& types,
                                                           const SimConfig& config) {
    auto parsed = sze::Scene::parse(file);
    if (!parsed) {
        return "not a scenario: " + parsed.error();
    }
    const sze::Scene& scene = parsed.value();
    LoadedScenario out;
    Warnings warn{out.warnings, {}};
    ScenarioSpec& spec = out.spec;

    // SCENE/HEAD (P: present in all 50, first child).
    const std::vector<const formats::chunk::Chunk*> heads = scene.chunks("HEAD");
    if (heads.empty()) {
        return std::string("scene.HEAD: missing");
    }
    if (auto e = read_head(heads.front()->payload, scene.edition, spec.settings)) {
        return *e;
    }

    // Islands (INSEL5) first: the STADT2 cities of the state series name a tile of
    // their island.
    // Island numbers are unique across a scenario's INSEL5 and INSELLNK (P, sze.md
    // "Island links"); a link reserves its number for stage 2 (O-35).
    std::set<std::uint32_t> slots;
    std::vector<std::uint64_t> island_at_offset; // file offset of spec.islands[i]
    for (std::size_t c = 0; c < scene.children.size(); ++c) {
        const formats::chunk::Chunk& child = scene.children[c];
        if (child.name() != "INSELLNK") {
            continue;
        }
        auto link = sze::IslandLink::parse(child.payload);
        if (!link) {
            return "scene[" + std::to_string(c) + "].INSELLNK: " + link.error();
        }
        if (!slots.insert(link.value().number).second) {
            return "scene[" + std::to_string(c) + "].INSELLNK: island number " +
                   std::to_string(link.value().number) + " is used twice";
        }
    }
    for (std::size_t c = 0; c < scene.children.size(); ++c) {
        const formats::chunk::Chunk& child = scene.children[c];
        if (child.name() != "INSEL5") {
            continue;
        }
        const std::string path = "islands[" + std::to_string(c) + "]";
        const std::uint64_t base =
            child.offset + formats::chunk::kHeaderLen + insel::kInsel5HeadLen;
        auto parsed_island = insel::Island::from_insel5(child.payload);
        if (!parsed_island) {
            return path + ": " + parsed_island.error();
        }
        const insel::Island& isl = parsed_island.value();
        // The scenario's INSEL5 is read by the world reader 0x100a2ea0 (from 0x100ae270
        // at 0x100ae6cb): head at 0x100a2efd, rectangle (x, y, x + w, y + h) at
        // 0x100a2f12–0x100a2f4d, `number & 0xFFFF` looked up in the island table
        // (0x100a2f38–0x100a2f5c); a missing island is made by world +0x88 =
        // 0x100a24d0(0, &rect, number) through island +0x1c4(&rect, number)
        // (0x100a2623), then 0x100a19a0 stores it at world +0x6530 + 4 · slot with slot
        // = the low byte of island slot +0x40. That +0x40 returns the number given to
        // +0x1c4 is likely (not followed, OQ-141). The original reads a second INSEL5
        // with a number it already has into that island; the adapter refuses it (no
        // scenario does it, P: numbers unique).
        const std::uint32_t slot = isl.number;
        if (slot >= config.island_slots) {
            return path + ": island number " + std::to_string(slot) +
                   " is not below island_slots (" + std::to_string(config.island_slots) + ")";
        }
        if (!slots.insert(slot).second) {
            return path + ": island number " + std::to_string(slot) + " is used twice";
        }
        // Climate = INFO[0] (P over 444 templates: 0 polar … 5 steppe, matching the
        // folder; the link selection compares it, D AnnoScene 0x10009550,
        // docs/re/annoscene.md §2, §5 step 2).
        const std::optional<Bytes> info = isl.record("INFO");
        if (!info || info->empty() || (*info)[0] >= config.monopoly_climate_mask.size()) {
            warn.add(ScenarioWarningKind::UnknownClimate, path, "INFO", base,
                     info && !info->empty()
                         ? "INFO[0] = " + std::to_string((*info)[0]) + " has no climate mask"
                         : std::string("no INFO record"));
            continue;
        }
        if (isl.x % 8 != 0 || isl.y % 8 != 0) {
            warn.add(ScenarioWarningKind::UnalignedIsland, path, "INSEL5", child.offset,
                     "position (" + std::to_string(isl.x) + ", " + std::to_string(isl.y) +
                         ") is not a multiple of 8; the original snaps it down "
                         "(0x100a253e–0x100a2586), here it stays");
        }
        ScenarioSpec::IslandSpec is;
        is.slot = IslandId{slot};
        is.x0 = isl.x; // head u16 x, y, width, height (P, insel.md "Head")
        is.y0 = isl.y;
        is.w = isl.width;
        is.h = isl.height;
        is.climate = (*info)[0];
        // Terrain: the HEIGHT and GFXID grids (P, insel.md "Blocks") and FRUCHTBAR
        // (P, w × h bytes); a missing column stays empty (OQ-060 shape).
        std::vector<std::uint8_t> height;
        std::vector<std::uint16_t> gfx;
        std::vector<std::uint8_t> fertility;
        if (isl.record("HEIGHT")) {
            auto h = isl.heights();
            if (!h) {
                return path + ".HEIGHT: " + h.error();
            }
            height = std::move(h).value();
        }
        if (isl.record("GFXID")) {
            auto g = isl.gfx_ids();
            if (!g) {
                return path + ".GFXID: " + g.error();
            }
            gfx = std::move(g).value();
        }
        if (const std::optional<Bytes> f = isl.fertility()) {
            fertility.assign(f->begin(), f->end());
        }
        is.terrain = make_terrain(std::move(height), std::move(gfx), std::move(fertility));

        // The placed items of GELAENDE, FLUESSE and PROD2 in file order (P,
        // insel.md "Placed objects": ANLAGEN id, rotation, height << 8 | phase, the
        // footprint's top-left tile).
        std::map<std::uint32_t, std::pair<std::string_view, ItemKey>> taken; // tile -> first item
        std::map<std::string, std::uint64_t> item_no;                        // per record name
        for (const formats::chunk::Chunk& rec : isl.records) {
            const std::string_view name = rec.name();
            const std::uint64_t at = base + rec.offset;
            if (name == "INFO" || name == "HEIGHT" || name == "GFXID" || name == "FRUCHTBAR") {
                continue;
            }
            if (name == "BERGWERK2") {
                warn.add(ScenarioWarningKind::UndecodedDeposits, path + ".BERGWERK2", name, at,
                         "BERGWERK2 is undecoded (OQ-024; deposit goods O-37): no deposit");
                continue;
            }
            if (name != "GELAENDE" && name != "FLUESSE" && name != "PROD2") {
                warn.unknown_record("islands", path + "." + std::string(name), name, at);
                continue;
            }
            auto block = insel::Block::parse(rec.payload);
            if (!block) {
                return path + "." + std::string(name) + ": " + block.error();
            }
            // D AnnoWelt 0x1008957d–0x10089676 (the PROD2 case of the island record
            // loader 0x10089130; GELAENDE and FLUESSE are read alike): floor((len - 6) /
            // 6) items (0x100895cf–0x100895d6), so bytes after the last whole item are
            // ignored; per item the id & 0x3FFF is looked up (0x1008960d) and an unknown
            // id skipped (0x10089619), rotation (id >> 14) & 3 (0x1008964e), phase
            // state & 0x3F (0x1008965e), height state >> 8 (0x10089660), tile = block
            // origin + the item's x, y read as signed bytes (movsx, 0x10089634).
            // formats::insel masks the id with 0x0FFF and reads x, y unsigned: equal
            // on the corpus (P, insel.md: bits 12–13 always 0, items inside their block).
            const Bytes items = block.value().data;
            const std::size_t whole = items.size() - items.size() % insel::kItemLen;
            for (std::size_t i = 0; i < whole; i += insel::kItemLen) {
                const insel::PlacedObject o =
                    insel::PlacedObject::parse(items.subspan(i, insel::kItemLen), block.value());
                const std::string where = path + "." + std::string(name) + "[" +
                                          std::to_string(item_no[std::string(name)]++) + "]";
                const std::uint64_t item_at =
                    at + formats::chunk::kHeaderLen + insel::kBlockHeaderLen + i;
                if (o.x >= is.w || o.y >= is.h) {
                    return where + ": the item's tile (" + std::to_string(o.x) + ", " +
                           std::to_string(o.y) + ") is outside the island";
                }
                const BuildingType* type = types.find(TypeId{o.anlage});
                if (type == nullptr) {
                    warn.add(ScenarioWarningKind::UnknownObject, where, name, item_at,
                             "ANLAGEN id " + std::to_string(o.anlage) + " is not in the registry");
                    continue;
                }
                // A PROD2 item of a type with a list in the layout is a building of a
                // city (state.md §10, S6 checks). Its city is the owner of its tile,
                // and no record of the island is decoded as tile owners (OQ-141):
                // the building is not placed.
                if (name == "PROD2" && config.layout.list_for(type->prodtype).has_value()) {
                    warn.add(ScenarioWarningKind::BuildingWithoutCity, where, name, item_at,
                             "building " + std::to_string(o.anlage) + " at (" +
                                 std::to_string(o.x) + ", " + std::to_string(o.y) +
                                 "): its tile has no known city");
                    continue;
                }
                const std::uint32_t tile = static_cast<std::uint32_t>(o.y) * is.w + o.x;
                const auto [it, fresh] = taken.try_emplace(tile, name, key_of(o));
                if (!fresh) {
                    const bool same = it->second.first == name && it->second.second == key_of(o);
                    warn.add(same ? ScenarioWarningKind::DuplicateObject
                                  : ScenarioWarningKind::ObjectTileTaken,
                             where, name, item_at,
                             same ? std::string("the same item again (OQ-024)")
                                  : "tile (" + std::to_string(o.x) + ", " + std::to_string(o.y) +
                                        ") already holds an item; the first stays (OQ-142)");
                    continue;
                }
                ScenarioSpec::TileObjectSpec t;
                t.index = tile;
                t.object = o.anlage;
                t.phase = o.phase(); // low 6 bits of state (D AnnoWelt 0x1007aa40)
                is.objects.push_back(t);
            }
        }
        warn.add(ScenarioWarningKind::TileOwnersUndecoded, path, "INSEL5", child.offset,
                 "no record of the island is decoded as tile owners (OQ-141): every tile neutral");
        spec.islands.push_back(std::move(is));
        island_at_offset.push_back(child.offset);
    }

    // Players (SPIELER): the header series (with HEAD) makes the players, the state
    // series adds their cities (P, sze.md "Players": two series, same indices).
    auto players = scene.players();
    if (!players) {
        return "players: " + players.error();
    }
    std::map<std::uint32_t, std::size_t> by_index; // SPIELER index -> spec.players
    std::size_t block_no = 0;
    for (const sze::Player& pl : players.value()) {
        const std::string path = "players[" + std::to_string(block_no++) + "]";
        const tagged::Record* head = tagged::find(pl.records, "HEAD");
        if (head != nullptr) {
            if (pl.index >= config.player_slots) {
                return path + ": player index " + std::to_string(pl.index) +
                       " is not below player_slots (" + std::to_string(config.player_slots) + ")";
            }
            if (head->payload.size() < sze::kPlayerHeadLen) {
                return path + ".HEAD: " + std::to_string(head->payload.size()) +
                       " bytes, the reader needs 108";
            }
            if (by_index.contains(pl.index)) {
                return path + ": player index " + std::to_string(pl.index) +
                       " has two header blocks";
            }
            ScenarioSpec::PlayerSpec p;
            p.id = PlayerId{pl.index};
            // The records in file order, as the player's reader takes them (D
            // AnnoWelt 0x1001d240).
            for (const tagged::Record& r : pl.records) {
                if (r.name == "HEAD") {
                    read_player_head(r.payload, p);
                } else if (r.name == "VOLK") {
                    // D AnnoWelt 0x1001d657–0x1001d753 (switch 0x1001e8f4): people 1
                    // for types 8, 11, 12; 11 for 15; 12 for 16, which also set
                    // +0xA0 bits 0x19 and colour 0; types 9, 10, 13, 14, 17 and up take
                    // the index of the VOLK name in a 20-name list (people 2..12 also
                    // set the bits and colour 0). The reader first matches the name
                    // against that list (0x1001d69e–0x1001d6f7) and sets nothing when
                    // it does not match; the list is not traced (OQ-143). The names
                    // that occur with types 8/11/12/15/16 are SPIELER, PIRATEN and
                    // VENECIA (P), taken here as matching.
                    if (p.kind == 8 || p.kind == 11 || p.kind == 12) {
                        p.volk = 1;
                    } else if (p.kind == 15 || p.kind == 16) {
                        p.volk = p.kind == 15 ? 11U : 12U;
                        p.flags_a0 |= 0x19U;
                        p.colour = 0;
                    } else {
                        warn.add(ScenarioWarningKind::UndecodedPeople, path + ".VOLK", r.name,
                                 r.offset,
                                 "type " + std::to_string(p.kind) +
                                     ": the VOLK name list is not traced (OQ-143): people stays " +
                                     std::to_string(p.volk) +
                                     ", and the +0xA0 bits 0x19 and colour 0 may be missing");
                    }
                } else {
                    warn.unknown_record("players", path + "." + std::string(r.name), r.name,
                                        r.offset);
                }
            }
            // D AnnoScene 0x100025a0: an unnamed computer gets a COMPSPIELER_NAME
            // drawn with the world generator (docs/re/annoscene.md §1).
            if (p.kind == 12 && p.name.empty()) {
                warn.add(ScenarioWarningKind::ComputerNameNotDrawn, path + ".HEAD", "HEAD",
                         head->offset, "unnamed computer player: the original draws a name");
            }
            by_index.emplace(pl.index, spec.players.size());
            spec.players.push_back(std::move(p));
            continue;
        }
        if (!by_index.contains(pl.index)) {
            return path + ": a state block for player " + std::to_string(pl.index) +
                   ", which has no header block";
        }
        for (const tagged::Record& r : pl.records) {
            // The player reader also takes the older name STADT for the same record (D
            // AnnoWelt 0x1001d240, string test; docs/re/annoscene.md §10).
            if (r.name != "STADT2" && r.name != "STADT") {
                warn.unknown_record("players", path + "." + std::string(r.name), r.name, r.offset);
                continue;
            }
            // STADT2 prefix (D AnnoWelt reader 0x1001e294, writer 0x1001f3fd): u8
            // city number on its island, u8 0, u16 x, u16 y (a tile of the city);
            // the owner is the enclosing block's player.
            const std::string where = path + "." + std::string(r.name);
            if (r.payload.size() < 6) {
                return where + ": the 6-byte prefix is missing";
            }
            const std::uint32_t no = r.payload[0];
            const std::uint32_t x = formats::u16_le(r.payload, 2);
            const std::uint32_t y = formats::u16_le(r.payload, 4);
            if (no >= config.city_slots || no == config.neutral_owner) {
                return where + ": city number " + std::to_string(no) +
                       " is not a city slot (city_slots " + std::to_string(config.city_slots) +
                       ", neutral_owner " + std::to_string(config.neutral_owner) + ")";
            }
            // The scenario path's load origin and the reader's skip flag (0x1001e2c4–
            // 0x1001e2fd, 0x1001d2fa) are not traced; origin 0 and flag 0 are likely.
            ScenarioSpec::IslandSpec* is =
                island_at(spec.islands, island_at_offset, r.offset, x, y);
            if (is == nullptr) {
                warn.add(ScenarioWarningKind::CityOffIsland, where, r.name, r.offset,
                         "city " + std::to_string(no) + " at (" + std::to_string(x) + ", " +
                             std::to_string(y) +
                             ") is on no read island; skipped as the original does");
                continue;
            }
            for (const ScenarioSpec::CitySpec& other : is->cities) {
                if (other.no.v == no) {
                    return where + ": city number " + std::to_string(no) +
                           " is used twice on island " + std::to_string(is->slot.v);
                }
            }
            ScenarioSpec::CitySpec city;
            city.no = CityNo{no};
            city.owner = PlayerId{pl.index};
            is->cities.push_back(std::move(city));
            std::string nested;
            if (auto kids = tagged::parse_records(
                    r.payload.subspan(6),
                    r.offset + (r.long_form ? tagged::kLongHeaderLen : tagged::kHeaderLen) + 6)) {
                for (const tagged::Record& k : kids.value()) {
                    nested += (nested.empty() ? "" : ",") + std::string(k.name);
                }
            }
            warn.add(ScenarioWarningKind::UndecodedCity, where, r.name, r.offset,
                     "city " + std::to_string(no) + " of player " + std::to_string(pl.index) +
                         ": its records (" + nested +
                         ") are undecoded (O-36), the city starts empty");
        }
    }

    // The other SCENE children.
    for (std::size_t c = 0; c < scene.children.size(); ++c) {
        const formats::chunk::Chunk& child = scene.children[c];
        const std::string_view name = child.name();
        if (name == "HEAD" || name == "INSEL5" || name == "SPIELER") {
            continue;
        }
        if (name == "INSELLNK") {
            auto link = sze::IslandLink::parse(child.payload);
            if (!link) {
                return "scene[" + std::to_string(c) + "].INSELLNK: " + link.error();
            }
            warn.add(ScenarioWarningKind::IslandLink, "scene[" + std::to_string(c) + "]", name,
                     child.offset,
                     "island " + std::to_string(link.value().number) + " (" +
                         std::to_string(link.value().width) + " x " +
                         std::to_string(link.value().height) +
                         ") is a link: resolved in stage 2 (O-35), its slot stays empty");
            continue;
        }
        warn.unknown_record("scene", "scene." + std::string(name), name, child.offset);
    }
    return out;
}

} // namespace anno::sim
