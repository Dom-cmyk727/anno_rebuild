// Interactive scene window (SDL3). Static chunks come from
// render::FrameRenderer, built by background workers on half the cores;
// frames are composed on the CPU on all cores and presented through an SDL
// streaming texture with vsync. The GPU compositor follows (ADR 0001).
#pragma once

#include <cstddef>
#include <string>

#include <anno/render/scene.hpp>

namespace anno::app {

/// Opens the window and runs until it is closed; an empty string on
/// success, else the error.
std::string run_viewer(const render::Scene& scene, std::size_t group);

} // namespace anno::app
