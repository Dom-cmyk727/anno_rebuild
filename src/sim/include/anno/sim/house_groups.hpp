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

/// The goods roles the population rules read (SimConfig::population_*; docs/sim/
/// population-contract.md section 3). A derived group table is bound to exactly these.
struct PopulationRoles {
    GoodsId infra_first = 0x4c, infra_last = 0x5a, tavern_goods = 0x4c, tavern_alias_goods = 0x1d;

    friend constexpr bool operator==(const PopulationRoles&, const PopulationRoles&) = default;
};

/// The data of one group the original's loader derives after parsing (MAP-POP-IMPL-1 C;
/// docs/sim/population-inputs.md 2.6-2.10, section 3). A pure function of the group's needs, the
/// goods `id_sec` and the roles; computed once when the registry is built, never serialised and not
/// hashed (its inputs are).
struct HouseGroupDerived {
    /// The need count n (the original's byte, here the vector length).
    std::uint32_t need_count = 0;
    /// D `+0xcc`: written per Ware by the loader, Force is not read.
    std::uint32_t goods_end = 0;
    /// D `+0xcd`: the position after the last infrastructure need (forced or not), then max with
    /// `goods_end`.
    std::uint32_t infra_end = 0;
    /// One entry per goods id of the registry: the need slot that serves the goods, `need_count` =
    /// none (the sentinel). The last of duplicate needs wins; the tavern alias and the one-pass
    /// IdSec substitution are applied.
    std::vector<std::uint32_t> lookup;
    /// D `+0xc4`, widened from a u32 to a set over the configured infrastructure range: entry i is
    /// 1 when the need for goods `infra_first + i` is forced. Size infra_last - infra_first + 1.
    std::vector<std::uint8_t> forced;

    bool operator==(const HouseGroupDerived&) const = default;
};

/// Derives the table of one group (the loader's per-Ware update of +0xcc, the post-pass of
/// AnnoGame 0x10014afc-0x10014bd9). `goods` is the registry's goods table. Returns an empty string,
/// or the reason for a refusal: a need on goods 0 (UNUSED), a need on a goods id outside the table,
/// a need with Menge 0 (a zero demand), or roles outside the goods domain. These refusals are
/// approved adaptations (the original's handling is untraced), not fidelity claims.
[[nodiscard]] std::string derive_group(const HouseGroup& group, std::span<const GoodsInfo> goods,
                                       const PopulationRoles& roles, HouseGroupDerived& out);

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
