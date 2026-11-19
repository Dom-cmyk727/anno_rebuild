// FIRST-call leaf only; AnnoWelt 0x100928a0–0x10092b32. UNWIRED.
#pragma once

#include <memory>
#include <utility>

#include <anno/sim_load/wildlife_prefix.hpp>

namespace anno::sim_load {

struct WildlifeFillRoles {
    // Original selectors at 0x100928ae/0x100928bc, not compiled descriptor data.
    sim::TypeId grass{2}, fish{11};
};
struct WildlifeFillResult {
    PreparedIsland island;
    std::uint64_t fish = 0, grass = 0, consumed = 0, prefix_consumed = 0;
};

class PrecomputedWildlifeFill {
  public:
    PrecomputedWildlifeFill(PrecomputedWildlifeFill&&) = default;
    PrecomputedWildlifeFill& operator=(PrecomputedWildlifeFill&&) = default;
    PrecomputedWildlifeFill(const PrecomputedWildlifeFill&) = delete;
    PrecomputedWildlifeFill& operator=(const PrecomputedWildlifeFill&) = delete;

  private:
    struct Role {
        sim::TypeId id;
        std::uint8_t code = 0;
    };
    PrecomputedWildlifeFill(std::unique_ptr<PreparedIsland> source, Role grass, Role fish,
                            std::uint64_t consumed)
        : source_(std::move(source)), grass_(grass), fish_(fish), prefix_consumed_(consumed) {}
    std::unique_ptr<PreparedIsland> source_; // One owned copy, moved into the result.
    Role grass_, fish_;
    std::uint64_t prefix_consumed_ = 0;
    friend formats::Result<PrecomputedWildlifeFill, std::string>
    prepare_wildlife_postload_fill(const PreparedPrefix&, std::uint64_t, formats::Bytes,
                                   std::span<const formats::figuren::Entry>,
                                   const sim::TypeRegistry&, WildlifeFillRoles);
    friend formats::Result<WildlifeFillResult, std::string>
        apply_wildlife_postload_fill(PrecomputedWildlifeFill, sim::TileRect);
};

// Verifies typed completion against the exact source scene, matching raw terrain/
// last FRUCHTBAR and effective low nibbles. Complete, nonoverlapping source HEIGHT/
// GFX blocks are the supported proof domain (existing format accessors).
// Registry/Entry buffers need live only during validation; facts are copied.
// Retains immutable Terrain, clones owned island data ONCE after validation.
[[nodiscard]] formats::Result<PrecomputedWildlifeFill, std::string> prepare_wildlife_postload_fill(
    const PreparedPrefix&, std::uint64_t ordinal, formats::Bytes source_scene,
    std::span<const formats::figuren::Entry>, const sim::TypeRegistry&, WildlifeFillRoles = {});

// Consumes the validated snapshot. Half-open rectangle, widened clipping. No RNG
// input/global cursor, preprocessing or navigation/raster/graphics composition.
[[nodiscard]] formats::Result<WildlifeFillResult, std::string>
    apply_wildlife_postload_fill(PrecomputedWildlifeFill, sim::TileRect);

} // namespace anno::sim_load
