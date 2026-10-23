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
#include <variant>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/rng.hpp>
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
    case ScenarioWarningKind::UnknownObject:
        return "UnknownObject";
    case ScenarioWarningKind::BuildingItemWithoutEntry:
        return "BuildingItemWithoutEntry";
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
    case ScenarioWarningKind::EntryWithoutBuildingItem:
        return "EntryWithoutBuildingItem";
    case ScenarioWarningKind::BuildingRefused:
        return "BuildingRefused";
    case ScenarioWarningKind::UndecodedBuildingRecord:
        return "UndecodedBuildingRecord";
    case ScenarioWarningKind::UnknownGoods:
        return "UnknownGoods";
    case ScenarioWarningKind::OwnerTileOutsideIsland:
        return "OwnerTileOutsideIsland";
    case ScenarioWarningKind::LinkNamesTemplate:
        return "LinkNamesTemplate";
    case ScenarioWarningKind::LinkUnresolved:
        return "LinkUnresolved";
    case ScenarioWarningKind::LinkOresUnplaced:
        return "LinkOresUnplaced";
    case ScenarioWarningKind::LinkRawMaterials:
        return "LinkRawMaterials";
    case ScenarioWarningKind::LinkTreasureUnplaced:
        return "LinkTreasureUnplaced";
    case ScenarioWarningKind::LinkTreasuresCapped:
        return "LinkTreasuresCapped";
    case ScenarioWarningKind::LinkTemplatePlayers:
        return "LinkTemplatePlayers";
    }
    return "?";
}

std::vector<ScenarioWarningKind> all_warning_kinds() {
    return {ScenarioWarningKind::UnknownRecord,
            ScenarioWarningKind::UnknownObject,
            ScenarioWarningKind::BuildingItemWithoutEntry,
            ScenarioWarningKind::TileOwnersUndecoded,
            ScenarioWarningKind::UndecodedDeposits,
            ScenarioWarningKind::ObjectTileTaken,
            ScenarioWarningKind::DuplicateObject,
            ScenarioWarningKind::CityOffIsland,
            ScenarioWarningKind::UndecodedPeople,
            ScenarioWarningKind::ComputerNameNotDrawn,
            ScenarioWarningKind::UnknownClimate,
            ScenarioWarningKind::UnalignedIsland,
            ScenarioWarningKind::EntryWithoutBuildingItem,
            ScenarioWarningKind::BuildingRefused,
            ScenarioWarningKind::UndecodedBuildingRecord,
            ScenarioWarningKind::UnknownGoods,
            ScenarioWarningKind::OwnerTileOutsideIsland,
            ScenarioWarningKind::LinkNamesTemplate,
            ScenarioWarningKind::LinkUnresolved,
            ScenarioWarningKind::LinkOresUnplaced,
            ScenarioWarningKind::LinkRawMaterials,
            ScenarioWarningKind::LinkTreasureUnplaced,
            ScenarioWarningKind::LinkTreasuresCapped,
            ScenarioWarningKind::LinkTemplatePlayers};
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
    c.links = s.links_total;
    c.links_resolved = s.links_placed;
    for (const ScenarioWarning& w : s.warnings) {
        ++c.warnings[w.kind];
        if (w.kind == ScenarioWarningKind::LinkUnresolved) {
            ++c.links_unresolved;
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

/// A PROD2 item of a building type: the tile layer's copy of a building, which a
/// city's PROD/ENTRY turns into the building (docs/re/annowelt.md "Scenario cities",
/// (b) and (e)).
struct BuildingItem {
    std::uint16_t id = 0;
    std::uint8_t rotation = 0, height = 0;
    std::uint32_t x = 0, y = 0; ///< island-relative origin
    std::uint32_t w = 1, h = 1; ///< the footprint on the island (rotated)
    std::string where;
    std::uint64_t at = 0;
    ItemKey key; ///< the item as the record holds it (the duplicate test)
    bool used = false;
};

/// What the city records of one island need from the island pass and write back.
struct IslandCities {
    std::vector<BuildingItem> items;                  ///< file order
    std::map<std::uint32_t, std::uint64_t> by_origin; ///< origin tile -> first item there
    /// The tile owners as the city records write them (row-major, w · h); empty until
    /// a city of the island writes one (then filled with the neutral owner first).
    std::vector<std::uint32_t> owner;
    /// Tiles a placed building covers (create_building's Occupied rule).
    std::vector<std::uint8_t> occupied;
    /// Origin tile -> the PRODTYPE of the first PROD2 item there whose type has no
    /// list in the layout (a monument, a wall, ...): only for the warning of an
    /// ENTRY that names it.
    std::map<std::uint32_t, std::uint32_t> other_prod2;
};

/// The tile a PROD/ENTRY at island tile (x, y) resolves to: island +0x84 =
/// 0x10083a90 returns the placement of the building whose footprint holds the tile
/// (the origin from the tile's in-footprint offset). Here: the item whose origin is
/// (x, y), else the first item in file order whose footprint holds it.
BuildingItem* item_at(IslandCities& ic, std::uint32_t x, std::uint32_t y, std::uint32_t w) {
    if (const auto it = ic.by_origin.find(y * w + x); it != ic.by_origin.end()) {
        return &ic.items[it->second];
    }
    for (BuildingItem& b : ic.items) {
        if (x >= b.x && y >= b.y && x - b.x < b.w && y - b.y < b.h) {
            return &b;
        }
    }
    return nullptr;
}

/// The bytes of `rec` copied into a zero-filled buffer of `n` bytes (the class Loads
/// read the record into such a buffer).
template <std::size_t N> std::array<std::uint8_t, N> zero_filled(Bytes rec) {
    std::array<std::uint8_t, N> b{};
    std::copy_n(rec.begin(), std::min(rec.size(), N), b.begin());
    return b;
}

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

/// A building's own record after its 4-byte position, read as the class's Load does
/// (slot +0x7c; docs/re/annowelt.md "Scenario cities" (b)). Returns false when the
/// adapter does not map the bytes (a class whose Load is not read, or a record that
/// would overrun the original's buffer): the make_* default stays. The switch: each
/// Load passes the result of slot +0xac of the object at building +0x14, called with
/// (0, the switch bit), to slot +0xc8 (workshop 0x100686aa / 0x100686b3, farm
/// 0x1006efd1 / 0x1006efda); that +0xac returns the bit unchanged is likely (not read,
/// OQ-145).
bool read_class_record(Payload& payload, RuntimeClass cls, Bytes rec) {
    if (rec.empty()) {
        return true;
    }
    if (auto* w = std::get_if<Workshop>(&payload); w != nullptr && cls == RuntimeClass::Workshop) {
        // 0x10068640: length <= 10, else the record is skipped whole (0x100686c6).
        if (rec.size() > 10) {
            return true;
        }
        const auto b = zero_filled<10>(rec);
        w->switched_on = (b[1] & 1) != 0; // slot +0xc8 stores it at +0x24 (0x100684d4)
        if (!w->out.empty()) {
            w->out[0] = le16(&b[2]); // +0x18
        }
        for (std::size_t i = 0; i < w->in.size() && i < 2; ++i) {
            w->in[i] = le16(&b[4 + 2 * i]); // +0x1c, +0x1e
        }
        if (!w->activity.empty()) {
            w->activity[0] = le16(&b[8]); // +0x20
        }
        return true;
    }
    if (auto* f = std::get_if<Farm>(&payload);
        f != nullptr && (cls == RuntimeClass::Farm || cls == RuntimeClass::FurTrapper)) {
        // 0x1006ef70: exactly 10 bytes, else the record is skipped whole (0x1006ef7e).
        if (rec.size() != 10) {
            return true;
        }
        f->switched_on = (rec[1] & 1) != 0;
        f->out = le16(&rec[2]);      // +0x18
        f->raw = le16(&rec[4]);      // +0x1c
        f->activity = le16(&rec[8]); // +0x1e ([6] is not read, 0x1006efad)
        return true;
    }
    if (auto* m = std::get_if<Mine>(&payload); m != nullptr && cls == RuntimeClass::Mine) {
        // 0x1006e310: the record's own length into a 10-byte buffer (no test).
        if (rec.size() > 10) {
            return false;
        }
        const auto b = zero_filled<10>(rec);
        m->switched_on = (b[1] & 1) != 0;
        m->out = le16(&b[2]);      // +0x18
        m->activity = le16(&b[8]); // +0x1c
        return true;
    }
    if (auto* s = std::get_if<SmallFarm>(&payload);
        s != nullptr && cls == RuntimeClass::SmallFarm) {
        // 0x1006fc80: own length into a 14-byte buffer (no test); per line i.
        if (rec.size() > 14) {
            return false;
        }
        const auto b = zero_filled<14>(rec);
        for (std::size_t i = 0; i < 2; ++i) {
            if (i < s->out.size()) {
                s->out[i] = le16(&b[2 + 2 * i]); // +0x18 + 2i
            }
            if (i < s->raw.size()) {
                s->raw[i] = le16(&b[6 + 2 * i]); // +0x24 + 2i
            }
            if (i < s->line_on.size()) {
                s->line_on[i] = ((b[1] >> i) & 1) != 0;
            }
            if (i < s->activity.size()) {
                s->activity[i] = le16(&b[10 + 2 * i]); // +0x1c + 2i
            }
        }
        return true;
    }
    if (auto* x = std::get_if<WeaponsWorkshop>(&payload);
        x != nullptr && cls == RuntimeClass::WeaponsWorkshop) {
        // 0x10069140: own length into a 14-byte buffer (no test); per line i.
        if (rec.size() > 14) {
            return false;
        }
        const auto b = zero_filled<14>(rec);
        for (std::size_t i = 0; i < 2; ++i) {
            if (i < x->out.size()) {
                x->out[i] = le16(&b[2 + 2 * i]); // +0x18 + 2i
            }
            if (i < x->in.size()) {
                x->in[i] = le16(&b[6 + 2 * i]); // +0x20 + 2i
            }
            if (i < x->activity.size()) {
                x->activity[i] = le16(&b[10 + 2 * i]); // +0x24 + 2i
            }
            if (i < x->line_on.size()) {
                x->line_on[i] = (b[1] & (1U << i)) != 0;
            }
        }
        return true;
    }
    return false;
}

/// WARETYPE (P: 56 × (u16 number, NUL-terminated name), sze.md "SCENE children") →
/// the file's goods number -> the registry's goods id, by name as the original maps
/// it (D AnnoWelt 0x100a18d0). nullopt for a number without a mappable name.
std::map<std::uint32_t, std::optional<GoodsId>> goods_remap(const sze::Scene& scene,
                                                            const TypeRegistry& types) {
    std::map<std::uint32_t, std::optional<GoodsId>> out;
    for (const formats::chunk::Chunk* c : scene.chunks("WARETYPE")) {
        const Bytes p = c->payload;
        std::size_t o = 0;
        while (o + 2 < p.size()) {
            const std::uint32_t number = formats::u16_le(p, o);
            o += 2;
            const auto end =
                std::find(p.begin() + static_cast<std::ptrdiff_t>(o), p.end(), std::uint8_t{0});
            const std::string name(p.begin() + static_cast<std::ptrdiff_t>(o), end);
            o = static_cast<std::size_t>(end - p.begin()) + 1;
            out[number] = types.goods_by_name(name);
        }
    }
    return out;
}

/// The owner grid of an island as row-major runs {owner, length}, the trailing
/// neutral tiles left out (World::create makes them neutral).
std::vector<std::pair<std::uint32_t, std::uint32_t>> owner_runs(const std::vector<std::uint32_t>& g,
                                                                std::uint32_t neutral) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> runs;
    for (const std::uint32_t o : g) {
        if (!runs.empty() && runs.back().first == o) {
            ++runs.back().second;
        } else {
            runs.emplace_back(o, 1);
        }
    }
    if (!runs.empty() && runs.back().first == neutral) {
        runs.pop_back();
    }
    return runs;
}

/// A STADT2 city's records (D AnnoWelt city reader 0x1002e170, docs/re/annowelt.md
/// "Scenario cities"): HEAD2 → capacity, WARE → stock, ID3 / ID2 → the island's tile
/// owners, PROD/ENTRY → the buildings (with the footprint claim); every other record
/// an UnknownRecord. The records in file order, as the reader takes them. An error
/// only for a record that cannot be read at all.
std::optional<std::string> read_city(const tagged::Record& r, const std::string& where,
                                     ScenarioSpec::IslandSpec& is, IslandCities& ic,
                                     ScenarioSpec::CitySpec& city, const TypeRegistry& types,
                                     const SimConfig& config, Warnings& warn,
                                     const std::map<std::uint32_t, std::optional<GoodsId>>& goods) {
    const std::size_t header = r.long_form ? tagged::kLongHeaderLen : tagged::kHeaderLen;
    auto kids = tagged::parse_records(r.payload.subspan(6), r.offset + header + 6);
    if (!kids) {
        return where + ": " + kids.error();
    }
    const std::uint32_t no = city.no.v;
    const std::uint32_t tiles = is.w * is.h;
    auto owners = [&]() -> std::vector<std::uint32_t>& {
        if (ic.owner.empty()) {
            ic.owner.assign(tiles, config.neutral_owner);
        }
        return ic.owner;
    };
    for (const tagged::Record& k : kids.value()) {
        const std::string kp = where + "." + std::string(k.name);
        if (k.name == "HEAD2") {
            // 0x1002e383–0x1002e3e5: a zero-filled 64-byte buffer, u16 [0x24] << 8 is the
            // capacity, 0 -> 0x3200 (50 t).
            const auto b = zero_filled<64>(k.payload);
            const std::uint32_t cap = le16(&b[0x24]);
            city.capacity = cap != 0 ? cap << 8 : CityStock{}.capacity;
        } else if (k.name == "WARE") {
            // 0x1002e7ca–0x1002e84b: length / 8 entries {u16 goods (file numbering),
            // u16 tons}; the goods mapped by WARETYPE name, 0 -> skipped; stock = tons << 8.
            if (!city.stock.has_value()) {
                city.stock = std::vector<std::uint32_t>(config.warehouse_goods, 0);
            }
            for (std::size_t o = 0; o + 8 <= k.payload.size(); o += 8) {
                const std::uint32_t file_goods = formats::u16_le(k.payload, o);
                const std::uint32_t tons = formats::u16_le(k.payload, o + 2);
                const auto it = goods.find(file_goods);
                if (it == goods.end() || !it->second.has_value() ||
                    *it->second >= city.stock->size()) {
                    warn.add(ScenarioWarningKind::UnknownGoods, kp, k.name, k.offset,
                             "goods number " + std::to_string(file_goods) +
                                 " maps to no goods of the registry's table");
                    continue;
                }
                if (*it->second == 0) {
                    continue; // UNUSED: the original skips a mapped 0
                }
                (*city.stock)[*it->second] = tons << 8;
            }
        } else if (k.name == "ID3" || k.name == "ID2") {
            // ID3 (0x1002e709–0x1002e7c5): u32 entries, x = v & 0x7fff, y = (v >> 15) &
            // 0x7fff, island-relative (ID2, 0x1002e65c: world tiles); bit 30 -> 8 mask
            // bytes follow. Island +0x100 = 0x10085620 sets the owner of the 8 × 8 chunk
            // at (x, y) (all 64 tiles, or tile (x + i, y + j) where mask byte j has bit
            // i), unconditionally and unclipped.
            std::vector<std::uint32_t>& g = owners();
            std::uint64_t outside = 0;
            std::size_t o = 0;
            while (o < k.payload.size()) {
                if (o + 4 > k.payload.size()) {
                    return kp + ": a truncated entry at byte " + std::to_string(o);
                }
                const std::uint32_t v = formats::u32_le(k.payload, o);
                o += 4;
                std::int64_t x = v & 0x7fffU;
                std::int64_t y = (v >> 15) & 0x7fffU;
                if (k.name == "ID2") {
                    x -= is.x0;
                    y -= is.y0;
                }
                const std::uint8_t* mask = nullptr;
                if ((v & 0x40000000U) != 0) {
                    if (o + 8 > k.payload.size()) {
                        return kp + ": a truncated mask at byte " + std::to_string(o);
                    }
                    mask = &k.payload[o];
                    o += 8;
                }
                for (std::int64_t j = 0; j < 8; ++j) {
                    for (std::int64_t i = 0; i < 8; ++i) {
                        if (mask != nullptr && ((mask[j] >> i) & 1) == 0) {
                            continue;
                        }
                        const std::int64_t tx = x + i;
                        const std::int64_t ty = y + j;
                        if (tx < 0 || ty < 0 || tx >= is.w || ty >= is.h) {
                            ++outside;
                            continue;
                        }
                        g[static_cast<std::size_t>(ty * is.w + tx)] = no;
                    }
                }
            }
            if (outside > 0) {
                warn.add(ScenarioWarningKind::OwnerTileOutsideIsland, kp, k.name, k.offset,
                         std::to_string(outside) +
                             " tiles outside the island (the original writes them unclipped)");
            }
        } else if (k.name == "ID") {
            warn.add(ScenarioWarningKind::TileOwnersUndecoded, kp, k.name, k.offset,
                     "radius claims (island +0xe8) are not modelled: their tiles stay as they are");
        } else if (k.name == "PROD") {
            const std::size_t kh = k.long_form ? tagged::kLongHeaderLen : tagged::kHeaderLen;
            auto entries = tagged::parse_records(k.payload, k.offset + kh);
            if (!entries) {
                return kp + ": " + entries.error();
            }
            std::size_t e_no = 0;
            for (const tagged::Record& e : entries.value()) {
                if (e.name != "ENTRY") {
                    // 0x1002671d: another record name is skipped.
                    warn.unknown_record("cities", kp + "." + std::string(e.name), e.name, e.offset);
                    continue;
                }
                const std::string ep = kp + ".ENTRY[" + std::to_string(e_no++) + "]";
                if (e.payload.size() < 4) {
                    return ep + ": the 4-byte position is missing";
                }
                // 0x10026524–0x1002655c: u16 x, y island-relative; island +0x84 resolves
                // the tile to the building's placement.
                const std::uint32_t ex = formats::u16_le(e.payload, 0);
                const std::uint32_t ey = formats::u16_le(e.payload, 2);
                BuildingItem* bi = ex < is.w && ey < is.h ? item_at(ic, ex, ey, is.w) : nullptr;
                if (bi == nullptr) {
                    const auto other = ex < is.w && ey < is.h ? ic.other_prod2.find(ey * is.w + ex)
                                                              : ic.other_prod2.end();
                    warn.add(ScenarioWarningKind::EntryWithoutBuildingItem, ep, e.name, e.offset,
                             "tile (" + std::to_string(ex) + ", " + std::to_string(ey) +
                                 ") holds no PROD2 building item" +
                                 (other != ic.other_prod2.end()
                                      ? " (a PROD2 item of PRODTYPE " +
                                            std::to_string(other->second) +
                                            ", no list in the layout: OQ-144)"
                                      : "") +
                                 ": skipped");
                    continue;
                }
                bi->used = true; // named by an ENTRY, created or refused below
                const BuildingType* type = types.find(TypeId{bi->id});
                if (type == nullptr) {
                    continue; // the island pass kept only known types
                }
                // create_building's rules (F32: the original creates blindly).
                if (bi->x + bi->w > is.w || bi->y + bi->h > is.h) {
                    warn.add(ScenarioWarningKind::BuildingRefused, ep, e.name, e.offset,
                             "building " + std::to_string(bi->id) +
                                 ": the footprint is not inside the island");
                    continue;
                }
                if (ic.occupied.empty()) {
                    ic.occupied.assign(tiles, 0);
                }
                bool free = true;
                for (std::uint32_t j = 0; j < bi->h && free; ++j) {
                    for (std::uint32_t i = 0; i < bi->w; ++i) {
                        if (ic.occupied[(bi->y + j) * is.w + bi->x + i] != 0) {
                            free = false;
                            break;
                        }
                    }
                }
                if (!free) {
                    warn.add(ScenarioWarningKind::BuildingRefused, ep, e.name, e.offset,
                             "building " + std::to_string(bi->id) +
                                 ": the footprint overlaps another building");
                    continue;
                }
                // Create (0x10024010) builds the class's default, then the Load reads
                // the bytes after the position over it. No bytes: World::create's own
                // default (nullopt). With bytes: the default with the type attach's
                // deep flag of a mine (create_building step 2-3; the stamp of step 5 is
                // 0 at tick 0, the default), then the Load's fields.
                std::optional<Payload> payload;
                if (e.payload.size() > 4) {
                    const ClassInfo& cls = types.class_info(type->cls);
                    payload = make_payload(*type, cls, config);
                    if (auto* mine = std::get_if<Mine>(&*payload)) {
                        mine->deep = mine_is_deep(type->prod, type->research_id);
                    }
                    if (!read_class_record(*payload, type->cls, e.payload.subspan(4)) &&
                        warn.seen_once
                            .emplace("building record", std::to_string(static_cast<int>(type->cls)))
                            .second) {
                        warn.add(ScenarioWarningKind::UndecodedBuildingRecord, ep, e.name, e.offset,
                                 "the record of class " +
                                     std::to_string(static_cast<int>(type->cls)) +
                                     " is not mapped: the make_* default stays");
                    }
                    if (const std::string p = payload_problem(*payload, *type, cls, config);
                        !p.empty()) {
                        warn.add(ScenarioWarningKind::BuildingRefused, ep, e.name, e.offset,
                                 "building " + std::to_string(bi->id) + ": " + p);
                        continue;
                    }
                }
                std::vector<std::uint32_t>& g = owners();
                for (std::uint32_t j = 0; j < bi->h; ++j) {
                    for (std::uint32_t i = 0; i < bi->w; ++i) {
                        const std::uint32_t t = (bi->y + j) * is.w + bi->x + i;
                        ic.occupied[t] = 1;
                        // The footprint claim (0x10026685: island +0xe8, radius 0, no
                        // force): a tile still neutral gets the city.
                        if (g[t] == config.neutral_owner) {
                            g[t] = no;
                        }
                    }
                }
                ScenarioSpec::BuildingSpec bs;
                bs.type = TypeId{bi->id};
                bs.x = is.x0 + static_cast<std::int32_t>(bi->x); // world tile (+0x50)
                bs.y = is.y0 + static_cast<std::int32_t>(bi->y);
                bs.rotation = bi->rotation; // +0x68
                bs.byte0f = bi->height;     // +0x70 stores +0x0f (0x10071c40)
                bs.payload = std::move(payload);
                city.buildings.push_back(std::move(bs));
            }
        } else {
            warn.unknown_record("cities", kp, k.name, k.offset);
        }
    }
    return std::nullopt;
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

/// Whether an INSELLNK path names a dummy: its last 9 characters are "DUMMY.scp" in
/// any case (D AnnoScene 0x10009eac–0x10009ec0, _stricmp at 0x10027310).
bool is_dummy_path(const std::string& path) {
    return path.size() >= 9 &&
           ascii_upper(std::string_view(path).substr(path.size() - 9)) == "DUMMY.SCP";
}

/// A dummy link's climate: the file name after the last backslash (the whole path when
/// there is none) against the prefixes of the table at 0x1003b328, case-insensitive,
/// first match wins; no match keeps 0 (D 0x10009ec6–0x10009f73).
std::uint32_t link_climate(const std::string& path) {
    const std::size_t slash = path.rfind('\\');
    const std::string file =
        ascii_upper(slash == std::string::npos ? path : path.substr(slash + 1));
    constexpr std::array<std::string_view, 7> kPrefix = {"PO_", "T_", "N_", "PR_",
                                                         "D_",  "S_", "W_"};
    for (std::size_t i = 0; i < kPrefix.size(); ++i) {
        if (file.compare(0, kPrefix[i].size(), kPrefix[i]) == 0) {
            return static_cast<std::uint32_t>(i);
        }
    }
    return 0;
}

/// One island record (INSEL5) → IslandSpec at (x0, y0) in table slot `slot`: the
/// climate, the terrain columns and the placed items, with the warnings of their
/// records (`base` is the file offset of the island's first record, `head_offset` the
/// INSEL5's own; an unknown record is reported once per `record_scope` and name). Used
/// for the scenario's INSEL5 islands and for the template a link resolves to (stage 2,
/// whose offsets are in the template file). `out` stays empty when the island is skipped
/// (UnknownClimate); the return value is a refusal. The PROD2 building items are not
/// placed: they go to `ic` for the city pass (T23).
std::optional<std::string> island_from_insel(const insel::Island& isl, const std::string& path,
                                             std::uint64_t base, std::uint64_t head_offset,
                                             IslandId slot, std::uint32_t x0, std::uint32_t y0,
                                             const TypeRegistry& types, const SimConfig& config,
                                             Warnings& warn, std::string_view record_scope,
                                             std::optional<ScenarioSpec::IslandSpec>& out,
                                             IslandCities& ic) {
    // Climate = INFO[0] (P over 444 templates: 0 polar … 5 steppe, matching the
    // folder; the link selection compares it, D AnnoScene 0x10009550,
    // docs/re/annoscene.md §2, §5 step 2).
    const std::optional<Bytes> info = isl.record("INFO");
    if (!info || info->empty() || (*info)[0] >= config.monopoly_climate_mask.size()) {
        warn.add(ScenarioWarningKind::UnknownClimate, path, "INFO", base,
                 info && !info->empty()
                     ? "INFO[0] = " + std::to_string((*info)[0]) + " has no climate mask"
                     : std::string("no INFO record"));
        return std::nullopt;
    }
    if (x0 % 8 != 0 || y0 % 8 != 0) {
        warn.add(ScenarioWarningKind::UnalignedIsland, path, "INSEL5", head_offset,
                 "position (" + std::to_string(x0) + ", " + std::to_string(y0) +
                     ") is not a multiple of 8; the original snaps it down "
                     "(0x100a253e–0x100a2586), here it stays");
    }
    ScenarioSpec::IslandSpec is;
    is.slot = slot;
    is.x0 = static_cast<std::int32_t>(x0); // head u16 x, y (or the link's), width, height
    is.y0 = static_cast<std::int32_t>(y0); // (P, insel.md "Head")
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
            warn.unknown_record(std::string(record_scope), path + "." + std::string(name), name,
                                at);
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
            // A PROD2 item of a type with a list in the layout is the tile layer's
            // copy of a building; the building is created from its city's
            // PROD/ENTRY with the type, rotation and height of the tile (D AnnoWelt
            // 0x100264a0 through island +0x84 = 0x10083a90; docs/re/annowelt.md
            // "Scenario cities" (b), (e)). It is kept here for the city pass.
            if (name == "PROD2" && config.layout.list_for(type->prodtype).has_value()) {
                // A second item on a building item's origin is dropped as the tile
                // objects are (DuplicateObject for an equal item, else
                // ObjectTileTaken; OQ-142): an entry names one building there.
                const std::uint32_t origin = static_cast<std::uint32_t>(o.y) * is.w + o.x;
                if (const auto f = ic.by_origin.find(origin); f != ic.by_origin.end()) {
                    const bool same = ic.items[f->second].key == key_of(o);
                    warn.add(same ? ScenarioWarningKind::DuplicateObject
                                  : ScenarioWarningKind::ObjectTileTaken,
                             where, name, item_at,
                             same ? std::string("the same building item again (OQ-024)")
                                  : "tile (" + std::to_string(o.x) + ", " + std::to_string(o.y) +
                                        ") already holds a building item; the first "
                                        "stays (OQ-142)");
                    continue;
                }
                BuildingItem bi;
                bi.key = key_of(o);
                bi.id = o.anlage;
                bi.rotation = o.rotation;
                bi.height = o.height();
                bi.x = o.x;
                bi.y = o.y;
                const auto [fw, fh] = o.footprint(static_cast<std::uint8_t>(type->width),
                                                  static_cast<std::uint8_t>(type->height));
                bi.w = fw;
                bi.h = fh;
                bi.where = where;
                bi.at = item_at;
                ic.by_origin.emplace(origin, ic.items.size());
                ic.items.push_back(std::move(bi));
                continue;
            }
            const std::uint32_t tile = static_cast<std::uint32_t>(o.y) * is.w + o.x;
            if (name == "PROD2") {
                ic.other_prod2.try_emplace(tile, type->prodtype);
            }
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
    out = std::move(is);
    return std::nullopt;
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
    std::vector<IslandCities> cities_of;         // parallel to spec.islands (T23)
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
        std::optional<ScenarioSpec::IslandSpec> built;
        IslandCities ic;
        if (auto e = island_from_insel(isl, path, base, child.offset, IslandId{slot}, isl.x, isl.y,
                                       types, config, warn, "islands", built, ic)) {
            return *e;
        }
        if (!built) {
            continue;
        }
        spec.islands.push_back(std::move(*built));
        island_at_offset.push_back(child.offset);
        cities_of.push_back(std::move(ic));
    }

    // Players (SPIELER): the header series (with HEAD) makes the players, the state
    // series adds their cities (P, sze.md "Players": two series, same indices).
    auto players = scene.players();
    if (!players) {
        return "players: " + players.error();
    }
    const std::map<std::uint32_t, std::optional<GoodsId>> goods = goods_remap(scene, types);
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
            // The scenario path's load origin is (0, 0) and the reader's flag 0 (D
            // 0x100ae2a9 / 0x100ae2ad, 0x100ae286); the prefix position is always
            // added: the context bit that would drop it (0x1001e2c4–0x1001e2fd) is
            // likely clear (OQ-145).
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
            const std::size_t island_index = static_cast<std::size_t>(is - spec.islands.data());
            if (auto e = read_city(r, where, *is, cities_of[island_index], city, types, config,
                                   warn, goods)) {
                return *e;
            }
            is->cities.push_back(std::move(city));
        }
    }

    // The tile owners the cities wrote, and the PROD2 buildings no city created.
    for (std::size_t i = 0; i < spec.islands.size(); ++i) {
        IslandCities& ic = cities_of[i];
        if (!ic.owner.empty()) {
            spec.islands[i].owner_runs = owner_runs(ic.owner, config.neutral_owner);
        }
        for (const BuildingItem& b : ic.items) {
            if (!b.used) {
                warn.add(ScenarioWarningKind::BuildingItemWithoutEntry, b.where, "PROD2", b.at,
                         "building " + std::to_string(b.id) + " at (" + std::to_string(b.x) + ", " +
                             std::to_string(b.y) +
                             ") is named by no city's PROD/ENTRY: not placed (likely a tile "
                             "item without a building in the original, OQ-144)");
            }
        }
    }

    // The other SCENE children, in file order: the ROHSTOFF, SCHATZ and INSELLNK
    // records before ERZE feed the link selection, which the ERZE record runs (D
    // AnnoScene 0x1000a25c; docs/re/annoscene.md §3, §5).
    bool erze_seen = false;
    std::vector<std::uint16_t> raw(config.links.raw_kinds, 0);
    std::uint16_t treasures = 0;
    for (std::size_t c = 0; c < scene.children.size(); ++c) {
        const formats::chunk::Chunk& child = scene.children[c];
        const std::string_view name = child.name();
        const std::string where = "scene[" + std::to_string(c) + "]";
        if (name == "HEAD" || name == "INSEL5" || name == "SPIELER" || name == "WARETYPE") {
            continue; // WARETYPE: the goods remap of the city WARE records
        }
        if (name == "INSELLNK") {
            auto parsed_link = sze::IslandLink::parse(child.payload);
            if (!parsed_link) {
                return where + ".INSELLNK: " + parsed_link.error();
            }
            const sze::IslandLink& l = parsed_link.value();
            ++out.links_total;
            const std::string path(l.path.begin(), l.path.end());
            if (!is_dummy_path(path)) {
                warn.add(ScenarioWarningKind::LinkNamesTemplate, where, name, child.offset,
                         "island " + std::to_string(l.number) + " names " + path +
                             ": the original places that file as it is (0x10009fe9), not read "
                             "here");
                continue;
            }
            IslandLinkSpec link;
            link.number = l.number; // the head (D 0x10009e2e–0x10009e63, sze.md)
            link.x = l.x;
            link.y = l.y;
            link.w = l.width;
            link.h = l.height;
            link.climate = link_climate(path);
            link.path = path;
            link.where = where;
            link.offset = child.offset;
            if (erze_seen) {
                warn.add(ScenarioWarningKind::LinkUnresolved, where, name, child.offset,
                         "island " + std::to_string(l.number) +
                             " comes after ERZE, which has run the selection (0x1000a25c)");
                continue;
            }
            out.links.push_back(std::move(link));
            continue;
        }
        if (name == "ROHSTOFF") {
            // D 0x1000a076–0x1000a140: a zero-filled 16-byte buffer, bytes 0..9 to the
            // kinds 1, 2, 4..11 of this+0x51DC (bytes 10..15 not read). A longer record
            // would be read past the buffer: refused.
            if (child.payload.size() > 16) {
                return where + ".ROHSTOFF: " + std::to_string(child.payload.size()) +
                       " bytes, longer than the reader's 16-byte buffer";
            }
            if (config.links.raw_kinds < 12) {
                return where + ".ROHSTOFF: LinkTables.raw_kinds is below the 12 the record fills";
            }
            std::array<std::uint8_t, 16> buf{};
            std::copy(child.payload.begin(), child.payload.end(), buf.begin());
            constexpr std::array<std::uint32_t, 10> kKind = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
            for (std::size_t b = 0; b < kKind.size(); ++b) {
                raw[kKind[b]] = buf[b];
            }
            continue;
        }
        if (name == "SCHATZ") {
            // D 0x1000a158–0x1000a19c: the record is read into the u16 at this+0x150.
            if (child.payload.size() > 2) {
                return where + ".SCHATZ: " + std::to_string(child.payload.size()) +
                       " bytes, longer than the u16 it is read into";
            }
            std::array<std::uint8_t, 2> buf = {static_cast<std::uint8_t>(treasures & 0xffU),
                                               static_cast<std::uint8_t>(treasures >> 8)};
            std::copy(child.payload.begin(), child.payload.end(), buf.begin());
            treasures = static_cast<std::uint16_t>(buf[0] | (buf[1] << 8));
            continue;
        }
        if (name == "ERZE") {
            // D 0x1000a19f–0x1000a25c: a zero-filled 10-byte buffer; bytes 0..3 → ore
            // kinds 0..3, 4 → kind 5, 5 → kind 6, 6 → the special count; bytes 7 and 8
            // go to this+0x528C / +0x528E (not read by the selection), byte 9 is not read.
            // Then the selection runs (0x1000a25c).
            if (child.payload.size() > 10) {
                return where + ".ERZE: " + std::to_string(child.payload.size()) +
                       " bytes, longer than the reader's 10-byte buffer";
            }
            if (erze_seen) {
                return where + ".ERZE: a second ERZE would run the selection again";
            }
            if (config.links.ore_kinds < 7) {
                return where + ".ERZE: LinkTables.ore_kinds is below the 7 the record fills";
            }
            std::array<std::uint8_t, 10> buf{};
            std::copy(child.payload.begin(), child.payload.end(), buf.begin());
            LinkInputs& in = out.link_inputs;
            in.raw_materials = raw;
            in.ores.assign(config.links.ore_kinds, 0);
            constexpr std::array<std::uint32_t, 6> kKind = {0, 1, 2, 3, 5, 6};
            for (std::size_t b = 0; b < kKind.size(); ++b) {
                in.ores[kKind[b]] = buf[b];
            }
            in.special_count = buf[6];
            in.treasures = treasures;
            erze_seen = true;
            continue;
        }
        warn.unknown_record("scene", "scene." + std::string(name), name, child.offset);
    }
    if (!erze_seen) {
        // Without ERZE the selection never runs (it is called from the ERZE case only).
        for (const IslandLinkSpec& l : out.links) {
            warn.add(ScenarioWarningKind::LinkUnresolved, l.where, "INSELLNK", l.offset,
                     "island " + std::to_string(l.number) +
                         ": the scenario has no ERZE record, which runs the selection");
        }
        out.links.clear();
    }
    out.link_inputs.raw_materials.resize(config.links.raw_kinds, 0);
    out.link_inputs.ores.resize(config.links.ore_kinds, 0);
    return out;
}

namespace {

/// The INFO bytes of an island into `out` as 0x100b2770 copies them (AnnoWelt
/// 0x100b27d3): each INFO record in order zeroes the 12 bytes and copies its first 12
/// bytes (the last record wins); without a record `out` keeps what it held.
void copy_info(const insel::Island& isl, std::array<std::uint8_t, 12>& out) {
    for (const formats::chunk::Chunk& r : isl.records) {
        if (r.name() == "INFO") {
            out = {};
            std::copy_n(r.payload.begin(), std::min<std::size_t>(r.payload.size(), out.size()),
                        out.begin());
        }
    }
}

/// "0, 3, 12" for the set entries of a mark list.
std::string kinds_list(const std::vector<std::uint8_t>& marks) {
    std::string s;
    for (std::size_t k = 0; k < marks.size(); ++k) {
        if (marks[k] != 0) {
            s += (s.empty() ? "" : ", ") + std::to_string(k);
        }
    }
    return s;
}

} // namespace

std::optional<TemplateSummary> template_summary(Bytes scp, std::string name) {
    // D AnnoWelt 0x100b25a0: root ANNO (0x100b25f5), then every INSEL4 / INSEL5 child
    // (INSEL4 0x100b2645, INSEL5 0x100b26ba) gives the size from its head and the INFO record
    // through 0x100b2770; the last one wins; none: failure.
    auto parsed = formats::chunk::parse_file(scp);
    if (!parsed || parsed.value().chunks.empty()) {
        return std::nullopt;
    }
    const formats::chunk::Chunk& root = parsed.value().chunks.front();
    if (root.name() != "ANNO" || !root.is_container) {
        return std::nullopt;
    }
    std::optional<TemplateSummary> out;
    std::array<std::uint8_t, 12> info{}; // the zero-filled summary (0x10007ed0-0x10007f2a)
    for (const formats::chunk::Chunk& c : root.children) {
        std::optional<formats::Result<insel::Island, std::string>> isl;
        if (c.name() == "INSEL5") {
            isl = insel::Island::from_insel5(c.payload);
        } else if (c.name() == "INSEL4") {
            isl = insel::Island::from_insel4(root.children);
        } else {
            continue;
        }
        if (!*isl) {
            return std::nullopt;
        }
        TemplateSummary t;
        t.name = name;
        t.w = isl->value().width; // INSEL5 head +4, +6 (0x100b26f3–0x100b2717); INSEL4 +0, +2
        t.h = isl->value().height;
        copy_info(isl->value(), info);
        t.info = info;
        out = std::move(t);
    }
    return out;
}

formats::Result<LinkResolution, std::string>
resolve_scenario_links(LoadedScenario& s, std::span<const TemplateSummary> candidates,
                       const SimConfig& config, const Rules& rules) {
    // The generator World::create builds (scenario_rng): the table, then the cursor
    // spec.seed mod its length (world property 8, D AnnoWelt 0x100a3230).
    std::optional<WorldRng> made = scenario_rng(s.spec);
    if (!made) {
        return std::string("rng_table: must not be empty");
    }
    WorldRng rng = std::move(*made);
    GlobalDraw draw(rng);
    auto res = resolve_island_links(s.links, s.link_inputs, candidates, draw, config.links,
                                    rules.island_candidate_order);
    if (!res) {
        return res.error();
    }
    if (!s.links.empty()) {
        s.spec.seed = rng.cursor();
    }
    if (res.value().treasures_capped) {
        Warnings warn{s.warnings, {}};
        warn.add(ScenarioWarningKind::LinkTreasuresCapped, "scene", "SCHATZ", 0,
                 std::to_string(s.link_inputs.treasures) + " treasures for " +
                     std::to_string(s.links.size()) +
                     " links: the original's loop would not end (0x10009c5e–0x10009c9d)");
    }
    return res;
}

std::optional<std::string> place_island_links(LoadedScenario& s, const LinkResolution& res,
                                              std::span<const Bytes> templates,
                                              const TypeRegistry& types, const SimConfig& config) {
    if (res.links.size() != s.links.size() || templates.size() != s.links.size()) {
        return std::string("place_island_links: the resolution or the templates do not match "
                           "the links");
    }
    Warnings warn{s.warnings, {}};
    std::set<std::uint32_t> used;
    for (const ScenarioSpec::IslandSpec& is : s.spec.islands) {
        used.insert(is.slot.v);
    }
    // D 0x10008490: the links in list order (file order).
    for (std::size_t i = 0; i < s.links.size(); ++i) {
        const IslandLinkSpec& l = s.links[i];
        const ResolvedLink& r = res.links[i];
        const std::string path = "links[" + std::to_string(i) + "]";
        const std::string who = path + " (island " + std::to_string(l.number) + ")";
        if (!r.candidate.has_value()) {
            // D 0x100084f4–0x100084fd: an empty chosen name skips the link.
            warn.add(ScenarioWarningKind::LinkUnresolved, l.where, "INSELLNK", l.offset,
                     "island " + std::to_string(l.number) + " (" + std::to_string(l.w) + " x " +
                         std::to_string(l.h) + ", climate " + std::to_string(l.climate) +
                         "): no template left (" + std::to_string(r.matched) + " matched, " +
                         std::to_string(r.kept) + " kept); the original skips it");
            continue;
        }
        auto parsed = formats::chunk::parse_file(templates[i]);
        if (!parsed || parsed.value().chunks.empty() ||
            !parsed.value().chunks.front().is_container) {
            return who + ": the template is not a chunk file";
        }
        const formats::chunk::Chunk& root = parsed.value().chunks.front();
        const formats::chunk::Chunk* insel5 = nullptr;
        const formats::chunk::Chunk* insel4 = nullptr;
        bool players = false;
        for (const formats::chunk::Chunk& c : root.children) {
            const std::string_view cname = c.name();
            if (cname == "INSEL5" && insel5 == nullptr) {
                insel5 = &c;
            } else if (cname == "INSEL4" && insel4 == nullptr) {
                insel4 = &c;
            } else if (cname == "SPIELER") {
                players = true;
            } else if (insel4 == nullptr) {
                // (An INSEL4 template keeps its island records as root siblings after the
                // INSEL4, insel.md: those are read with it, not reported here.)
                warn.unknown_record("link_templates", path + "." + std::string(cname), cname,
                                    c.offset);
            }
        }
        // The template's island as the world loader takes it: an INSEL5, else an INSEL4
        // with its root-level records (insel.md; GetAnnoInselFileData accepts both, so a
        // template of either kind can be a candidate).
        formats::Result<insel::Island, std::string> isl =
            std::string("the template holds no INSEL5 or INSEL4");
        if (insel5 != nullptr) {
            isl = insel::Island::from_insel5(insel5->payload);
        } else if (insel4 != nullptr) {
            isl = insel::Island::from_insel4(root.children);
        }
        if (!isl) {
            return who + ": " + isl.error();
        }
        // D 0x10008503–0x1000850d: world slot +0x88(name, &(x, y), number & 0xFFFF).
        const std::uint32_t slot = l.number & 0xFFFFU;
        if (slot >= config.island_slots) {
            return who + ": island number is not below island_slots (" +
                   std::to_string(config.island_slots) + ")";
        }
        if (!used.insert(slot).second) {
            return who + ": island slot " + std::to_string(slot) + " is used twice";
        }
        std::optional<ScenarioSpec::IslandSpec> built;
        IslandCities ic;
        // Record offsets: inside the INSEL5's payload, or file offsets for an INSEL4's
        // root-level records.
        const std::uint64_t head_at = insel5 != nullptr ? insel5->offset : insel4->offset;
        const std::uint64_t base =
            insel5 != nullptr ? head_at + formats::chunk::kHeaderLen + insel::kInsel5HeadLen : 0;
        if (auto e = island_from_insel(isl.value(), path, base, head_at, IslandId{slot}, l.x, l.y,
                                       types, config, warn, "link_templates", built, ic)) {
            return who + ": " + *e;
        }
        if (!built) {
            continue; // UnknownClimate, warned
        }
        // D 0x10008532–0x10008545: island property 7 = the link's climate (equal to the
        // template's INFO[0] by step 2).
        built->climate = l.climate;
        s.spec.islands.push_back(std::move(*built));
        ++s.links_placed;
        // No city of the file names a link island's tiles (STADT2 is read before the
        // links are placed, and the template's SPIELER blocks are not read: native
        // settlements, OQ-151), so no building item of the template is placed.
        for (const BuildingItem& b : ic.items) {
            warn.add(ScenarioWarningKind::BuildingItemWithoutEntry, b.where, "PROD2", b.at,
                     "building " + std::to_string(b.id) + " at (" + std::to_string(b.x) + ", " +
                         std::to_string(b.y) +
                         ") of the link's template: no city's PROD/ENTRY names it, not placed "
                         "(OQ-151)");
        }
        // D 0x100085a2–0x100085cf: island property 11 = 0xF009 | the marks, also with none.
        const std::string marks_raw = kinds_list(r.raw_marks);
        warn.add(ScenarioWarningKind::LinkRawMaterials, path, "INSELLNK", l.offset,
                 "island property 11 = 0xF009 | raw materials {" + marks_raw +
                     "} (0x100085a2) has no field");
        const std::string marks_ore = kinds_list(r.ore_marks);
        if (!marks_ore.empty()) {
            warn.add(ScenarioWarningKind::LinkOresUnplaced, path, "INSELLNK", l.offset,
                     "ores " + marks_ore +
                         ": not put on mountain objects (0x100085d5, O-37), no deposit, the "
                         "LinkOrePlace draws not made");
        }
        if (r.treasure) {
            warn.add(ScenarioWarningKind::LinkTreasureUnplaced, path, "INSELLNK", l.offset,
                     "the treasure (0x10008b25) is not placed, its draws not made");
        }
        if (players) {
            warn.add(ScenarioWarningKind::LinkTemplatePlayers, path, "SPIELER", 0,
                     "the template's SPIELER blocks are not read (native settlements; the "
                     "LinkNativeName draws not made)");
        }
    }
    return std::nullopt;
}

} // namespace anno::sim
