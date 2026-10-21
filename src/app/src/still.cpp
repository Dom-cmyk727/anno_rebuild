#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

#include <anno/app/still.hpp>

namespace anno::app {

std::string set_derived_levels(render::Scene& scene, std::size_t group, std::size_t max_derived) {
    if (group < render::kGroups) {
        return {};
    }
    // Group 2 + n is the n-th derived level, so the levels 0 .. group need n = group - 2.
    const std::size_t derived = group - render::kGroups + 1;
    if (derived > max_derived) {
        return "--group " + std::to_string(group) + " needs --max-derived-levels " +
               std::to_string(derived) + " or more";
    }
    scene.set_levels(render::ZoomLevels::with_derived(derived));
    return {};
}

std::optional<render::Canvas> derived_view(const render::Scene& scene, render::Rect view,
                                           std::size_t group, const render::Layers& layers,
                                           render::FrameStats& frames) {
    render::FrameRenderer renderer(std::max(1U, std::thread::hardware_concurrency()));
    renderer.river_flow = layers.river_flow;
    // Without workers the chunks are built inside the calls, a plan's worth per call (512
    // keys, iso.md "Busy frames"); a derived chunk waits for its children, so a view takes
    // a few calls. The bound only guards against a chunk that never completes.
    constexpr std::size_t kRounds = 4096;
    std::size_t missing = 1;
    render::FrameStats built;
    for (std::size_t round = 0; missing != 0; ++round) {
        if (round == kRounds) {
            return std::nullopt;
        }
        const auto [n, ms, left] = renderer.update_chunks(scene, view, group);
        built.chunks_built += n;
        built.chunks_ms += ms;
        missing = left;
    }
    const auto w = static_cast<std::size_t>(view.w), h = static_cast<std::size_t>(view.h);
    std::vector<std::uint32_t> zero_rgb(w * h);
    frames =
        renderer.render(scene, view, group, layers.time_ms, layers.water_style, zero_rgb.data());
    // The chunks built by the calls above are the picture's (the draw found them cached).
    frames.chunks_built += built.chunks_built;
    frames.chunks_ms += built.chunks_ms;
    render::Canvas canvas;
    canvas.width = w;
    canvas.height = h;
    canvas.pixels.resize(w * h * 4);
    for (std::size_t i = 0; i < zero_rgb.size(); ++i) {
        const std::uint32_t c = zero_rgb[i];
        canvas.pixels[4 * i] = static_cast<std::uint8_t>(c >> 16);
        canvas.pixels[4 * i + 1] = static_cast<std::uint8_t>(c >> 8);
        canvas.pixels[4 * i + 2] = static_cast<std::uint8_t>(c);
        canvas.pixels[4 * i + 3] = 255;
    }
    return canvas;
}

} // namespace anno::app
