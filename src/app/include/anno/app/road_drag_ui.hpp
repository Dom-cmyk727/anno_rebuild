// Deterministic UI-side helpers for ROADS-3 (UI-ROADS-1: press-hold L-drag road building,
// docs/BUILDINGSOBJECTS.md §10.1 items 1, 2, 3, 9, 10). Everything here is pure window/UI state
// (no SDL dependency, no World access, never hashed or persisted in sim state).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/app/placement_ghost.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// Chooses the cosmetic road variant from `base_id` and raw UI random byte `byte` using property
/// 0x32 (`HEAD[11]` via `sim::house_variant_facts`: count = HEAD[11] & 0x3f, step = HEAD[11] >> 6,
/// AnnoFrame `0x10044340` `id + (byte % count) * step`). Falls back to `base_id` if count <= 1 or
/// the candidate id is missing / not a land road.
[[nodiscard]] sim::TypeId road_variant_id(const sim::TypeRegistry& registry, sim::TypeId base_id,
                                          std::uint8_t byte);

/// Deterministic UI-side deck of per-tile variant bytes (`B+0x1f0+i`, AnnoFrame `0x1004af50`).
/// Lazily caches bytes by path index so repeated pointer moves during a drag keep the exact same
/// byte per tile index, and the commit on mouse release reads the exact same variants shown in the
/// preview; `reroll()` clears the deck after commit for the next drag. Never touches `sim::Rng`.
class RoadVariantDeck {
  public:
    explicit RoadVariantDeck(std::uint64_t seed = 0x524F414431353033ULL);

    [[nodiscard]] std::uint8_t byte_at(std::size_t index);
    [[nodiscard]] sim::TypeId variant_for(const sim::TypeRegistry& registry, sim::TypeId base_id,
                                          std::size_t index);
    void reroll();
    [[nodiscard]] std::uint64_t generation() const { return generation_; }

  private:
    std::uint8_t next_byte();

    std::uint64_t state_ = 0;
    std::uint64_t generation_ = 0;
    std::vector<std::uint8_t> bytes_;
};

/// Inputs for `plan_road_ghost`.
struct RoadGhostInputs {
    bool armed_road = false;
    bool pointer_on_chrome = false;
    std::size_t path_len = 0;
    std::optional<sim::RoadPathCheck> verdict;
    GhostState previous = GhostState::Hidden;
    bool blink_hidden = false;
};

/// Render plan derived purely from `RoadGhostInputs` (`BUILDINGSOBJECTS.md` §10.1 item 9):
/// - Affordable prefix is always solid; unaffordable tail blinks (`blink_hidden` switches
///   `draw_count` between `valid_count` and `affordable_count`).
/// - Refused (`valid_count == 0`) draws the yellow `0xffff00` 1x1 empty rectangle on the hovered
///   tile (`draw_refusal_box = true`) and zero ghost sprites.
/// - Never draws a radius outline or fertility indicator.
struct RoadGhostPlan {
    GhostState state = GhostState::Hidden;
    std::size_t valid_count = 0;
    std::size_t affordable_count = 0;
    std::size_t draw_count = 0;
    bool draw_refusal_box = false;
    bool draw_radius = false;
    bool draw_fertility = false;
};

[[nodiscard]] RoadGhostPlan plan_road_ghost(const RoadGhostInputs& in);

} // namespace anno::app
