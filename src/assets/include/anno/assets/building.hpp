// Anlagen.dat frames (buildings, objects) as 8-bit RGBA images.
//
// Mapping (spec: docs/formats/anlagen.md): as figure.hpp, except that the
// colour table is the global Anlagen.dat table with id
// entry.palette_ids[frame.palette] (id 0 never matches), and that pixels a
// row writes past the frame's width are kept: the image is widened to them
// (decode_frame_extended), as the original draws them to the right of the
// frame box (OQ-025). Rows and anchor are unchanged.
#pragma once

#include <cstdint>

#include <anno/assets/texture.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>

namespace anno::assets::building {

/// Decodes `frame` of `entry` from the volume of zoom group `group`.
Result<Rgba, Error> to_rgba(const formats::anlagen::AnlagenFile& file,
                            const formats::figuren::Entry& entry, std::uint32_t group,
                            const formats::figuren::Frame& frame);

} // namespace anno::assets::building
