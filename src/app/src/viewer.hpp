// Interactive scene window (SDL3). Static chunks come from
// render::FrameRenderer, built by background workers on half the cores;
// frames are composed on the GPU (render::gpu::GpuFrame over wgpu-native,
// presented with vsync; built with ANNO_BUILD_GPU) or on the CPU on all
// cores, presented through an SDL streaming texture with vsync.
// Renderer::Auto uses the GPU when one is available, as the Rust viewer.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/cli.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// Read by the window thread. The functions must not touch the sim; they
/// read the published snapshot (docs/app/game-shell.md).
struct ViewerHooks {
    std::function<std::string()> title;
    std::function<void(bool paused)> on_pause;
};

/// The play picture the window follows (docs/app/gpu-handoff.md). At the start of every frame,
/// on the window thread, the window takes the sim's building batches (`take_batches`, the
/// PlayRunner's mailbox: immutable batches, never the World), applies them to `picture` and
/// commits what is ready; then it takes one owning handle on the picture's current generation and
/// draws the whole frame from it. The window calls into `picture` only from its own thread and
/// not after run_viewer returns; the caller owns both and outlives the call.
struct ViewerPicture {
    ScenePicture* picture = nullptr;
    std::function<std::vector<std::shared_ptr<const BuildingBatch>>()> take_batches;
};

/// Opens the window and runs until it is closed; an empty string on
/// success, else the error. `hooks` is null for --view. `scene` is an owning handle (a scene made
/// by render::Scene::create, released through the reclaimer); with `play` it is the picture's
/// generation 0 and is used only to set the window up (the frames draw the picture's
/// generations, so the window does not keep generation 0 alive).
std::string run_viewer(std::shared_ptr<const render::Scene> scene, std::size_t group,
                       Renderer renderer,
                       render::RiverFlowMode river_flow = render::RiverFlowMode::Flow,
                       render::WaterStyle water = render::WaterStyle::Original,
                       std::size_t lookahead_chunks = 3, ZoomOptions zoom = {},
                       const ViewerHooks* hooks = nullptr, const ViewerPicture* play = nullptr);

} // namespace anno::app
