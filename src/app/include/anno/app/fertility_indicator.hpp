// The fertility indicator over a farm's placement ghost (UI-PRODBAR-1 stage B1; the rule in
// docs/re/ui.md 4.1, GOG AnnoFrame.dll 0x10047940 and its caller 0x10048450). The original draws
// frame trunc(10 * good / N) of FigurenHeader figure 1557 over the ghost, where N is the number of
// tiles of a midpoint-circle disc around the footprint's centre cell(s) and "good" those where the
// type's crop object would not be dry (island slot +0x140, 0x100817c0) and could be placed
// (island slot +0x6c, AnnoWelt 0x1007c790).
//
// This file holds the app-side parts that read only data the app already has (the type registry
// and the island's rectangle): the draw gates, the building check, the disc, the scaling and the
// sprite. The two per-tile tests are sim rules the rebuild does not have yet (OQ-715); they come
// in through FertilityTileSource. Without a source nothing is drawn: the named placeholder
// kFertilityNeedsSim. Read-only: nothing here touches the World.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/dev_tool.hpp>
#include <anno/assets/texture.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// The FigurenHeader.dat entry of the bar sprite: registry slot +0x6c(0x615) at 0x10047d29.
/// Palette Farbtabellen\Symbole\Fruchtbarkeit.PCX, 11 frames (0..10 segments), 54 x 12 in each of
/// the three zoom groups (P: anno-dump figuren --full). Data, not a limit: a mod may name another.
inline constexpr std::uint16_t kFertilityFigure = 1557;
/// Frames of the sprite: the value runs 0..10, so 11 (frame = value).
inline constexpr std::uint32_t kFertilityFrames = 11;
/// The goods range input 0 must lie in (0x1004799f-0x100479ab: the crops and resources 56..75,
/// docs/re/production.md section 7).
inline constexpr sim::GoodsId kFertilityGoodsFirst = 0x38;
inline constexpr sim::GoodsId kFertilityGoodsLast = 0x4b;
/// HEAD offset of the u16 crop object (type +0x82, slot +0xf8(2); loader 0x1000948e/0x1000949f).
inline constexpr std::size_t kFertilityCropHeadOffset = 0x28;
/// The named placeholder while the sim cannot answer the two tile tests (OQ-715).
inline constexpr std::string_view kFertilityNeedsSim = "fertility-indicator-needs-sim";

/// The original's draw gates, in the order it tests them. All must hold for the bar.
struct FertilityGates {
    /// Byte B+0x2e4: the cursor's preview test passed (stored by 0x10047470 at 0x100477a2 /
    /// 0x100478a7); clear means the yellow rectangle and no bar (0x10048464).
    bool preview_valid = false;
    /// The record the bar is computed for is the last ghost (0x1004795e-0x1004797b: index
    /// count - 1 when B+0x2e5 is set, else 0).
    bool last_ghost = false;
    /// [B+0x48] < [B+0x50] (0x10048491-0x1004849d): the cursor's rectangle is not empty.
    bool rect_nonempty = false;
    /// The registry lookup [B+0xc]->+0x70(type) is non-null (0x100484e3-0x100484e8).
    bool type_found = false;
    /// Figure 1557's projection slot +0x7c returned non-zero (0x10047de0-0x10047de2).
    bool projected = false;
};

/// The name of the first gate that is closed, "" when all hold (the projection is tested last,
/// after the count, as the original does).
[[nodiscard]] std::string fertility_closed_gate(const FertilityGates& gates);

/// The type's crop object when it shows the bar: input 0 (ROHSTOFF[0], type slot +0xb0(0)) in
/// kFertilityGoodsFirst..kFertilityGoodsLast and HEAD u16 [0x28] != 0 (0x10047a0f-0x10047a21).
/// nullopt otherwise: 810 fisher (input FISCH in range, object 0), 805 hunter (input out of range).
[[nodiscard]] std::optional<std::uint32_t> fertility_crop_object(const sim::BuildingType& type);

/// The half-width of rows k = 0..r of the disc (0x10047a5b-0x10047ac7): an integer midpoint-circle
/// loop in the original's unsigned 32-bit arithmetic, then, for r <= 3, +1 on rows
/// k >= r - trunc((r + 1) / 4). r 3: 3,3,3,2; r 4: 4,4,3,3,1; r 5: 5,5,5,4,3,2.
[[nodiscard]] std::vector<std::uint32_t> fertility_row_half_widths(std::uint32_t r);

/// The tiles the bar counts (N = size), in the original's order (0x10047b44-0x10047d1a): for each
/// row k, the up row cy0 - k, then the down row cy1 + k unless it is the up row; columns
/// cx0 - w[k] .. cx1 + w[k]. cx0 = x0 + trunc((W - 1) / 2), cx1 = cx0 + (W - 1) mod 2, and the same
/// for y with H. `w` x `h` is the footprint at the placement's rotation (swapped at 1 and 3).
/// Rows outside `island` are skipped, columns clamped to it, footprint tiles left out.
[[nodiscard]] std::vector<sim::TileXY> fertility_area(sim::TileXY origin, std::uint32_t w,
                                                      std::uint32_t h, std::uint32_t r,
                                                      const TileRect& island);

/// trunc(10 * good / n), 0 when n is 0 (0x10047de4-0x10047dfa).
[[nodiscard]] std::uint32_t fertility_value(std::uint64_t good, std::uint64_t n);

/// The two per-tile tests the bar needs from the sim (not ported yet, OQ-715). A tile is good when
/// the crop is not dry there and, only then, the crop object could be placed there (the original's
/// order, 0x10047c08-0x10047c34).
class FertilityTileSource {
  public:
    FertilityTileSource() = default;
    FertilityTileSource(const FertilityTileSource&) = default;
    FertilityTileSource& operator=(const FertilityTileSource&) = default;
    virtual ~FertilityTileSource() = default;
    /// The crop test (island slot +0x140, 0x100817c0) for `crop_object` at `tile`: true = dry.
    [[nodiscard]] virtual bool crop_dry(sim::TileXY tile, std::uint32_t crop_object) const = 0;
    /// The island placement predicate (slot +0x6c, 0x1007c790) for `crop_object` at `tile`.
    [[nodiscard]] virtual bool crop_placeable(sim::TileXY tile,
                                              std::uint32_t crop_object) const = 0;
};

/// What the bar shows for one ghost.
struct FertilityDecision {
    enum class Outcome : std::uint8_t {
        /// A gate is closed (`reason` names it): no bar.
        GateClosed,
        /// The type shows no bar.
        NotEligible,
        /// No tile source yet: the named placeholder kFertilityNeedsSim, no bar.
        NeedsSim,
        /// Draw frame `frame` of kFertilityFigure.
        Draw,
    };
    Outcome outcome = Outcome::GateClosed;
    std::uint32_t frame = 0;
    std::uint64_t good = 0, n = 0;
    std::string reason;
};

/// The decision for `type` placed at `origin` with `rotation` on `island`: gates, the building
/// check, then (with a source) the count and the projection gate.
[[nodiscard]] FertilityDecision fertility_decision(const sim::BuildingType* type,
                                                   sim::TileXY origin, std::uint8_t rotation,
                                                   const TileRect& island,
                                                   const FertilityGates& gates,
                                                   const FertilityTileSource* source);

/// A short label for a decision (the viewer's `tool.fertility` fact): "gate:<name>",
/// "not-eligible", the placeholder name, or "frame <k>".
[[nodiscard]] std::string fertility_label(const FertilityDecision& decision);

/// The sprite's frames: frames[k] is frame k of each zoom group (index = group), decoded from
/// `game_dir`'s FigurenHeader.dat (load_figure_facings with kFertilityFrames facings, uncoloured).
[[nodiscard]] formats::Result<std::vector<assets::unit::UnitFrames>, std::string>
load_fertility_frames(const std::filesystem::path& game_dir);

/// Blends `image` (RGBA, straight alpha) onto `xrgb` (0x00RRGGBB, `view_w` x `view_h`) with the
/// image pixel (anchor_x, anchor_y) at view pixel (px, py); the original's anchor is the frame's
/// (27, 12), its bottom centre. Pixels outside the view are skipped.
void blend_fertility_frame(const assets::Rgba& image, int anchor_x, int anchor_y, int px, int py,
                           int view_w, int view_h, std::uint32_t* xrgb);

} // namespace anno::app
