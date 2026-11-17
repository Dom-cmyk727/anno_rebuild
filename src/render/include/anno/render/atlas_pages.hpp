// The resident atlas pages the direct renderer (ADR 0007) will upload, built on the CPU
// (RENDER-DIRECT-1b, docs/render/atlas.md section 7 and "The pages"). Nothing here is read by
// any render path yet: RENDER-DIRECT-1c uploads and draws from these pages.
//
// A page is `page_size` x `page_size` texels, RGBA8, premultiplied. Every frame is placed by
// `AtlasResidency` (residency.hpp) at a slot `2 * padding` texels larger than the frame in each
// dimension (padding on every side), with the frame itself at the slot's (padding, padding)
// offset; the padding is dilated (each padding texel copies the nearest frame texel, clamped to
// the edge, after premultiplying). One mip level per page (half resolution, box-filtered) is
// built alongside level 0. A slot's own width and height are rounded up to even numbers before
// packing (the dilation absorbs the one extra texel when needed): this keeps every slot's origin
// and size even, so the page-wide mip grid never has to average texels from two different frames'
// slots at a boundary - a requirement for building pages in parallel with no shared lock on the
// per-texel path (RENDER-DIRECT-1b step 3), since a slot's own mip region then depends only on
// its own texels.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/render/frame_table.hpp>

namespace anno::render {

/// One frame ready to be placed into a page: its identity (FrameId, frame_table.hpp - the
/// caller's own dedup key) and its decoded, straight-alpha image (assets::Rgba; `building::
/// to_rgba`, `DecodedFrame::image`, or a smoothed water copy all produce this form).
struct AtlasFrameInput {
    FrameId id = 0;
    std::shared_ptr<const assets::Rgba> image;
};

/// Where one frame's own content (not its padding) lands, at level 0 and at the one mip level.
/// The mip rectangle is the frame's padding/2-inset sub-rectangle of its slot's own mip region
/// (the same relationship padding has to the frame at level 0): a renderer samples slightly
/// outside it, into the dilated padding, exactly as at level 0.
struct AtlasFramePlacement {
    FrameId id = 0;
    std::uint32_t layer = 0;
    std::uint32_t x = 0, y = 0, width = 0, height = 0;
    std::uint32_t mip_x = 0, mip_y = 0, mip_width = 0, mip_height = 0;
};

/// `premultiply_pixel`'s rounding rule, applied to a whole premultiplied page: `page_size`^2 * 4
/// bytes per layer at level 0, `(page_size / 2)`^2 * 4 bytes per layer at the one mip level,
/// zero-initialized (texels outside every slot are 0, RENDER-DIRECT-1b step 4's own proof point).
struct AtlasPages {
    std::uint32_t page_size = 0;
    std::uint32_t padding = 0;
    std::vector<std::vector<std::uint8_t>> level0;
    std::vector<std::vector<std::uint8_t>> level1;
    std::vector<AtlasFramePlacement> placements;
    /// Frames `AtlasResidency::ensure` could not place at all (too large for one page even
    /// alone, Overflow on its own): named, not silently dropped.
    std::vector<FrameId> oversized;
};

/// A straight-alpha pixel premultiplied by its own alpha: `c' = (c * a + 127) / 255` per colour
/// channel (integer, half up), alpha unchanged. RENDER-DIRECT-1b step 2's own rule: no existing
/// integer premultiply rounding rule was found elsewhere in the codebase to reuse instead
/// (`sea::unpremultiply`, `sea.cpp`, is float-valued and the reverse operation).
[[nodiscard]] inline std::array<std::uint8_t, 4>
premultiply_pixel(std::array<std::uint8_t, 4> rgba) {
    const unsigned a = rgba[3];
    auto channel = [a](std::uint8_t c) {
        return static_cast<std::uint8_t>((unsigned{c} * a + 127U) / 255U);
    };
    return {channel(rgba[0]), channel(rgba[1]), channel(rgba[2]), rgba[3]};
}

/// Builds the pages of `frames` at `page_size` with `padding` texels on every side of every
/// frame, on `threads` worker threads (clamped to at least 1; each frame's own slot is written,
/// dilated and mipped independently, with no shared lock on the per-texel path, so the result
/// does not depend on how many threads did the work - proven by a hash-equality test, not just
/// asserted). Packing itself (`AtlasResidency::ensure`, one call, a budget sized so it never has
/// to evict) is single-threaded; only the per-frame texel work is split.
[[nodiscard]] AtlasPages build_atlas_pages(std::span<const AtlasFrameInput> frames,
                                           std::uint32_t page_size, std::uint32_t padding,
                                           std::size_t threads);

} // namespace anno::render
