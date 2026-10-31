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
#include <anno/app/play_follow.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// Read by the window thread. The functions must not touch the sim; they
/// read the published snapshot (docs/app/game-shell.md).
struct ViewerHooks {
    std::function<std::string()> title;
    std::function<void(bool paused)> on_pause;
};

/// Opens the window and runs until it is closed; an empty string on
/// success, else the error. `hooks` is null for --view. `scene` is an owning handle (a scene made
/// by render::Scene::create, released through the reclaimer); with `play` it is the picture's
/// generation 0 and is used only to set the window up (the frames draw the picture's
/// generations, so the window does not keep generation 0 alive). With `play` the window follows
/// the sim (docs/app/gpu-handoff.md): at each frame start PlayFollower::follow, then one owning
/// handle of the picture's current generation for the whole frame; while paused it keeps drawing
/// while PlayFollower::has_work or the compositor's work is pending, and the runner's batch
/// listener wakes it when a batch arrives. The window calls into `play` only from its own thread
/// and not after run_viewer returns; the caller owns it and outlives the call.
std::string run_viewer(std::shared_ptr<const render::Scene> scene, std::size_t group,
                       Renderer renderer,
                       render::RiverFlowMode river_flow = render::RiverFlowMode::Flow,
                       render::WaterStyle water = render::WaterStyle::Original,
                       std::size_t lookahead_chunks = 3, ZoomOptions zoom = {},
                       const ViewerHooks* hooks = nullptr, PlayFollower* play = nullptr);

} // namespace anno::app
