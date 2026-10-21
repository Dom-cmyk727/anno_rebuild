// Interactive scene window (SDL3). Static chunks come from
// render::FrameRenderer, built by background workers on half the cores;
// frames are composed on the GPU (render::gpu::GpuFrame over wgpu-native,
// presented with vsync; built with ANNO_BUILD_GPU) or on the CPU on all
// cores, presented through an SDL streaming texture with vsync.
// Renderer::Auto uses the GPU when one is available, as the Rust viewer.
#pragma once

#include <cstddef>
#include <string>

#include <anno/app/cli.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// Opens the window and runs until it is closed; an empty string on
/// success, else the error.
std::string run_viewer(const render::Scene& scene, std::size_t group, Renderer renderer,
                       render::RiverFlowMode river_flow = render::RiverFlowMode::Flow,
                       render::WaterStyle water = render::WaterStyle::Original,
                       std::size_t lookahead_chunks = 3, ZoomOptions zoom = {});

} // namespace anno::app
