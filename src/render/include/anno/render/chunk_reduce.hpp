// The CPU reference of a derived zoom level's chunk (docs/render/iso.md
// "Continuous zoom, stage 3 design"): four chunks of the level above, each
// reduced 2x by an integer box filter into one quadrant of one chunk. The GPU
// reduce pass equals this exactly. Derived levels are not part of the original
// game; the rules below are our own (a plain mean, no gamma).
#pragma once

#include <array>

#include <anno/render/frame.hpp>

namespace anno::render {

/// Chunk (2 cx + qx, 2 cy + qy) of the source level is quadrant (qx, qy) of
/// chunk (cx, cy) of the derived level; index `qy * 2 + qx`.
using ChunkQuad = std::array<const Chunk*, 4>;

/// One chunk of the derived level. A null source, and a plane a source leaves
/// empty, mean "nothing here" (open sea: no ground, no objects, the field's
/// open-sea texel {255, 255, 255, 1}, no terrain). A plane is empty in the
/// result when it is empty in every source. Per plane, over each 2 x 2 block of
/// source texels, all means rounded to nearest with ties up:
///   terrain: mean of the texels that have ground (kNoTerrain if none);
///   ground:  mean per channel of the texels that have ground, `open_water`
///            where none does (the stand-in fill); a texel has ground when its
///            terrain is not kNoTerrain, or, with no terrain plane, whenever the
///            chunk has a ground plane;
///   field:   cover, shore, depth: mean; flags: bit 0 and bit 1 OR-ed (original
///            water, river water), the slope in bits 2-7 the mean of the slopes
///            of the texels with cover > 0 (0 if none): the slope belongs to the
///            water surface, so land and no-sea texels must not dilute it, and a
///            mean (not a maximum) keeps one steep texel from setting the swash
///            of a whole coast pixel;
///   objects: straight RGBA; alpha the mean of the four, colour the alpha-weighted
///            mean (the mean of the premultiplied colour, un-premultiplied);
///   water_below / water_above: OR; `ships`: true only if every source has them.
/// Not derived, empty in the result: `base` (the compositor then draws objects
/// over the water as pictures, as the original does), `flow`, `flow_waiting`.
[[nodiscard]] Chunk reduce_chunks_2x(const ChunkQuad& quad, Px open_water);

} // namespace anno::render
