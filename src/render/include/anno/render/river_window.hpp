// The two river stages of Scene::water_field_from that run on a window around
// the river sprites instead of the whole chunk (T1f item 2b; the proof is in
// docs/render/iso.md, "Bounded blur and chamfer"). They are exposed so that a
// test can compare the bounded result with the unbounded one on synthetic
// canvases, which CI can run without the game files.
#pragma once

#include <cstddef>
#include <vector>

#include <anno/render/iso.hpp>
#include <anno/render/sea.hpp>

namespace anno::render::detail {

struct WindowOptions {
    /// false: the window is the whole canvas / rect (the reference).
    bool bounded = true;
    /// Pixels taken off the window margin; tests set 1 to show the margin is
    /// tight (the result must then differ on a suitable input).
    int shrink = 0;
};

/// Box blur of the alpha channel of `c` (radius r, two passes of a horizontal
/// and a vertical box; edge pixels repeat), see scene.cpp. `close`: every pixel
/// with any alpha in its 3 x 3 neighbourhood counts as fully covered first.
std::vector<float> blurred_alpha(const Canvas& c, int r, bool close, WindowOptions options = {});

/// Scene::blend_rivers_into_sea on a field: river pixels near sea water take the
/// sea's look over kRiverBlend pixels, and sea pixels in the narrow gap between
/// river and sea join the river.
void blend_rivers_into_sea(sea::WaterField& f, std::size_t group, WindowOptions options = {});

} // namespace anno::render::detail
