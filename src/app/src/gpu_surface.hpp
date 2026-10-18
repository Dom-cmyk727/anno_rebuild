// A WebGPU surface for an SDL3 window (wgpu-native): the native window
// handle per platform (Metal layer on macOS, HWND on Windows, X11 or
// Wayland on Linux), as wgpu's raw-window-handle path does in the Rust viewer.
#pragma once

#include <SDL3/SDL.h>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/render/wgpu.hpp>

namespace anno::app {

/// The surface of `window` on `instance`. On macOS the window gets a Metal
/// view (SDL_Metal_CreateView) that the caller destroys
/// (SDL_Metal_DestroyView) after the surface, also when this fails;
/// elsewhere `metal_view` stays null.
formats::Result<render::gpu::Surface, std::string>
create_surface(WGPUInstance instance, SDL_Window* window, SDL_MetalView& metal_view);

} // namespace anno::app
