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
#include <anno/app/dev_script.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/opening_ui.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/publication_hold.hpp>
#include <anno/app/ship_layer.hpp>
#include <anno/app/window_trace.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// The ship-start opening in the window (W4-OPENING-1; docs/app/ship-opening.md). The window
/// feeds `ui` its input and the published OpeningView, posts the commands it makes to the runner
/// and draws `ships`; the caller owns all three and outlives run_viewer.
struct OpeningHooks {
    OpeningUi* ui = nullptr;
    std::shared_ptr<const ShipLayer> ships;
    /// Asks the session for a quick-save at the next turn boundary; the text is shown (a request
    /// that could not be made says why). Called on the window thread; must not block.
    std::function<std::string()> quick_save;
    /// The newest save result line ("" when there is none), polled for the title.
    std::function<std::string()> save_status;
    /// The tile the camera opens on (the local participant's ship).
    double look_x = 0, look_y = 0;
};

/// Read by the window thread. The functions must not touch the sim; they
/// read the published snapshot (docs/app/game-shell.md).
struct ViewerHooks {
    /// The ship-start opening (null: an ordinary --play window).
    OpeningHooks* opening = nullptr;
    std::function<std::string()> title;
    std::function<void(bool paused)> on_pause;
    /// The window trace (docs/app/window-trace.md; opt-in, GPU renderer only): every attempted GPU
    /// frame, the readiness of each commit's visible keys, and present-call results. Outlives the
    /// call; the window records nothing after run_viewer returns.
    WindowTrace* trace = nullptr;
    /// The developer building tool (dev_tool.hpp; --play only): the window feeds it its SDL
    /// events (sdl_input.hpp) with the last drawn frame and posts its commands to the runner. The
    /// caller owns it; null leaves the tool out.
    DevTool* dev = nullptr;
    /// A scripted session for the tool (dev_script.hpp; ANNO_DEV_SCRIPT): with `dev`, the window
    /// pushes the script's events into SDL's queue and closes when it ends or fails. The caller
    /// owns it and reads its log after run_viewer returns.
    DevScript* script = nullptr;
    /// The publication hold a scripted session arms (publication_hold.hpp; installed in the
    /// runner by the caller before it starts). Null leaves `hold publication` out: the script
    /// then fails at that step. The caller owns it.
    PublicationHold* hold = nullptr;
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
