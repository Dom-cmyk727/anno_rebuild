// The population group table: the load-time data of Data/BGruppen.dat that the
// house needs (docs/sim/state.md §4.7c "Group table", docs/re/population.md §1 and
// §12.1). Registry data like the type table: immutable, parsed once, in the
// registry digest, never state.
//
// Parsed in the simulation library from the keyed text of BGruppen.dat with the
// formats::text helpers; nothing of the original file is kept in the source tree.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {

struct GoodsInfo; // types.hpp

/// One need of a group (D: group record +0x64+4j, population.md §1): the goods id
/// (a goods table index, resolved through the name), the forced flag and the
/// quantity `Menge`, an integer: trunc(Menge * 40.96 + 0.5) as the loader stores it.
struct HouseNeed {
    GoodsId ware = 0;
    bool force = false;
    std::uint16_t menge = 0;

    bool operator==(const HouseNeed&) const = default;
};

/// One [BGRUPPE] record. Omitted on purpose: Priority (no effect, population.md
/// §1), BauFirst, BauLast, Hiscore and Wissen (unread), and the derived tables
/// whose first reader is P2 (the id -> need slot lookup with its substitution
/// pass, the ends of the goods and infrastructure needs, the forced-infrastructure
/// bit set); they come with that reader.
struct HouseGroup {
    /// The slot BGruppeNr names (PIONIER 0 ... ARISTOKRATEN 4): the index in the table.
    std::uint32_t number = 0;
    bool no_upgrade = false;
    bool no_downgrade = false;
    /// Residents per house at this level (D: group +0xc8, u16).
    std::uint16_t wohnmax = 0;
    /// The needs in file order: the goods needs, then the infrastructure needs.
    std::vector<HouseNeed> needs;
    /// NahrungTab / BedarfTab / InfraTab: 21 i8 scores per satisfaction step.
    std::array<std::int8_t, 21> nahrung{}, bedarf{}, infra{};
    /// VermehrTab: growth per update for mood -2..+2 in 1/64 resident,
    /// trunc(v * 64) toward zero (population.md §1).
    std::array<std::int8_t, 5> vermehr{};

    bool operator==(const HouseGroup&) const = default;
};

/// Parses the [BGRUPPE] section of Data/BGruppen.dat. `goods` resolves the names of
/// the `Ware` lines (the goods table the registry is built with). The groups are
/// returned in slot order; a name other than the five original ones, a repeated or
/// skipped slot, a missing required key (BGruppeNr, Wohnmax, the four tables), a
/// value that is not the integer or decimal it must be, a table of another length,
/// a goods name the table does not know and a need without Menge or Force are
/// errors naming the line. The count is not checked here (the registry checks it
/// against SimConfig::house_groups).
[[nodiscard]] formats::Result<std::vector<HouseGroup>, std::string>
parse_house_groups(formats::Bytes file, std::span<const GoodsInfo> goods);

} // namespace anno::sim
