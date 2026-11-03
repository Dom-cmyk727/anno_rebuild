// The per-goods population metadata of Data/BGruppen.dat [WAREN] (MAP-POP-IMPL-1 B,
// docs/sim/population-inputs.md sections 2.2-2.3): BauFirst (the stage index that enables a need)
// and IdSec (the substitute goods id). Registry data like the group table: parsed once, in the
// registry digest, never state. Other [WAREN] keys (Preis, Bonus, Forsch, BauLast, ...) are not
// read.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <anno/formats/bytes.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The number of stage names the 32-bit player mask can hold (indices 0..31). A stage index is a
/// shift count into `Player::flags` (u32); widening the mask is a state-format change.
inline constexpr std::uint32_t kStageIndexCount = 32;

/// The stage index of an INFRA_* name, or nullopt for anything else, GOTT and SPIELER included.
/// D: the name-pointer table at AnnoGame 0x10024bd0 (INFRA_EVER 0, INFRA_STUFE_1A.. 1, ..,
/// INFRA_STUFE_FORSCH 31; entries 32 and 33 are GOTT and SPIELER, not stages). The table is
/// compiled in, like the route-name mapping: a mapping to supported behaviour, not a goods
/// allowlist.
[[nodiscard]] std::optional<std::uint32_t> stage_index_of(std::string_view name);

/// Reads the [WAREN] section and sets `bau_first` and `id_sec` of the goods the records name; every
/// other entry is left unchanged (absent keys keep 0). Returns an empty string, or one message
/// naming the line: an `Id` that is not in the goods table, a `BauFirst` that is not a stage name
/// of 0..31 (unknown names, GOTT and SPIELER are refused: an adaptation, the original's handling is
/// untraced), an `IdSec` naming a goods the table does not have. A missing [WAREN] section changes
/// nothing. `goods` is the table the registry will be built with.
[[nodiscard]] std::string apply_goods_metadata(formats::Bytes file, std::span<GoodsInfo> goods);

/// Reads the [BAUSTUFEN] section: `Stufe: <stage name>, <group>, <count>` lines and `Wissen:
/// <stage name>, <n>` lines (a Wissen line needs an earlier Stufe line of its stage). Returns the
/// records ascending by stage. A stage name that is not INFRA_* (stage_index_of), a repeated
/// Stufe/Wissen, a field that is not a non-negative integer that fits u32, a Stufe without group
/// and count are errors naming the line. A missing section gives an empty table.
[[nodiscard]] formats::Result<std::vector<StageRecord>, std::string>
parse_stage_table(formats::Bytes file);

} // namespace anno::sim
