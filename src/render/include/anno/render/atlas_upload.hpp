// RENDER-DIRECT-1c: the resident atlas pages (RENDER-DIRECT-1b, atlas_pages.hpp) uploaded into
// one GPU texture array and proven exact, both by a byte-for-byte readback and by sampling every
// placement through the same placement-to-UV rule a later draw pass will reuse. A new, standalone
// unit: it touches no GpuFrame state and nothing here is read by GpuFrame::render yet (stage 2-3
// of ADR 0007 add the draw passes).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>

#include <anno/formats/result.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/wgpu.hpp>

namespace anno::render::gpu {

/// The zoom groups whose static+water pages should be resident on the GPU at once
/// (docs/render/atlas.md section 7, OQ-096/OQ-096b, owner decision 2026-11-17): a discrete
/// adapter holds every one of `group_count` groups at once (ADR 0007 decision 2: the 4090's own
/// case, the RTX run below); any other adapter holds only the adjacent pair a continuous zoom is
/// between (the owner's seamless-zoom rule: nothing may be uploaded mid-zoom, so both groups of
/// the crossed pair must already be resident - the iGPU's own case, atlas.md's own "adjacent
/// pair" language, section 4 and 7). `zoom_group` names the lower of that pair (the group the
/// continuous zoom position has not yet passed): the pair is {zoom_group, zoom_group + 1},
/// clamped so `zoom_group` never exceeds `group_count - 2` (a zoom at the very top of the range is
/// still between the last two groups, not past them) - and a single group when `group_count <=
/// 1`, where no pair exists. Pure and total: no device, no adapter needed, so a budget refusal
/// case and this one are both testable without a GPU (the packet's own acceptance item).
[[nodiscard]] std::vector<std::uint32_t> resident_atlas_groups(WGPUAdapterType adapter_type,
                                                               std::uint32_t zoom_group,
                                                               std::uint32_t group_count);

/// One page set uploaded into a 2D texture array: RGBA8Unorm (the format the existing sprite
/// atlas uses, `new_atlas`/`gpu.cpp` - so a later pass blends the resident pages exactly as it
/// already blends the sprite atlas; never an *Srgb variant, since the pages hold colours as
/// stored, GpuFrame::create's own "a sRGB target would convert twice" rule applies equally to a
/// sampled source), `page_size` x `page_size` x `layers`, 2 mip levels (level 0 and the one mip
/// level `build_atlas_pages` already produced on the CPU - no GPU-generated mips: the pages hold
/// premultiplied alpha already written field by field, and letting the GPU regenerate a mip would
/// silently redo RENDER-DIRECT-1b's own box filter, untested against it).
struct AtlasTexture {
    Texture texture;
    TextureView view;
    std::uint32_t page_size = 0;
    std::uint32_t layers = 0;
};

/// Device bytes `pages` needs once uploaded: level 0 (`page_size`^2 * 4) plus the one mip level
/// (`(page_size / 2)`^2 * 4), per layer, for every layer `pages.level0` actually holds (not a
/// fixed layer count - the caller's own page set).
[[nodiscard]] std::uint64_t atlas_pages_bytes(const AtlasPages& pages);

/// The refusal `upload_atlas_pages` would give for a page set of `layers` layers at `page_size`
/// needing `needed_bytes`, against a device whose limits are `max_array_layers` /
/// `max_dimension_2d`, and `budget_bytes` allowed: empty when none of the three refusal
/// conditions holds (atlas_upload.hpp's own doc on `upload_atlas_pages`). A pure function with no
/// device, so it is unit-tested without a GPU (the packet's own acceptance item) - `upload_atlas_
/// pages` itself calls this after reading the real device's limits.
[[nodiscard]] std::optional<std::string>
atlas_upload_refusal(std::uint32_t layers, std::uint32_t page_size, std::uint32_t max_array_layers,
                     std::uint32_t max_dimension_2d, std::uint64_t needed_bytes,
                     std::uint64_t budget_bytes);

/// Uploads `pages` into one new 2D texture array (`queue.writeTexture` per layer and level; no
/// sRGB conversion - the data is written as stored, straight bytes). Refused (never truncated)
/// naming which limit or budget failed:
/// - `pages.level0.size()` (the layer count) > the device's `maxTextureArrayLayers`;
/// - `pages.page_size` > the device's `maxTextureDimension2D`;
/// - `atlas_pages_bytes(pages)` > `budget_bytes` (the caller's own
///   `atlas_budget_bytes_before_sprite(...)` minus the existing sprite atlas's current footprint,
///   gpu.hpp/`cb_atlas_budget_bytes` - computed by the caller, since this unit holds no GpuFrame
///   state to subtract from).
/// The texture usage is TextureBinding | CopyDst | CopySrc (CopySrc for the readback proof below).
[[nodiscard]] formats::Result<AtlasTexture, std::string>
upload_atlas_pages(const Context& ctx, const AtlasPages& pages, std::uint64_t budget_bytes);

/// Reads one layer's one mip level back from `tex` (tightly packed RGBA8, the copy's row padding
/// removed): the exact layout of `AtlasPages::level0[layer]` (`level == 0`) or `level1[layer]`
/// (`level == 1`), for the byte-exact proof (step 2). Empty on failure (mapping failed, or `layer`
/// / `level` out of range).
[[nodiscard]] std::vector<std::uint8_t> read_back_level(const Context& ctx, const AtlasTexture& tex,
                                                        std::uint32_t layer, std::uint32_t level);

/// Draws one placement's own rectangle (level 0, `place.width` x `place.height`) or its mip
/// rectangle (level 1, `place.mip_width` x `place.mip_height`) from `tex` into a new, exactly
/// sized RGBA8Unorm offscreen target, read back and returned tightly packed: the placement-to-UV
/// rule every later draw pass will reuse, proven once here and shared, not re-derived
/// (RENDER-DIRECT-1c step 3). No sampler and no blending - `textureLoad` at the exact destination
/// texel (the atlas's own origin plus the output pixel), the project's one rule for every pass
/// that reads a resident atlas (gpu.wgsl's own "textureLoad, never filtered, so cached chunks and
/// sprites stay exact"): there is no rounding to get wrong, so this is strictly the nearest-sampler
/// rule the packet asks for, realized without a sampler object. Empty on failure.
[[nodiscard]] std::vector<std::uint8_t> sample_placement(const Context& ctx,
                                                         const AtlasTexture& tex,
                                                         const AtlasFramePlacement& place,
                                                         std::uint32_t level);

} // namespace anno::render::gpu
