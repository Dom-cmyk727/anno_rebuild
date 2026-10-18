#include "gpu_surface.hpp"

#include <cstdint>
#include <string_view>

namespace anno::app {

namespace gpu = render::gpu;

formats::Result<gpu::Surface, std::string> create_surface(WGPUInstance instance, SDL_Window* window,
                                                          SDL_MetalView& metal_view) {
    metal_view = nullptr;
    WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
    desc.label = gpu::sv("anno1503");
#if defined(__APPLE__)
    metal_view = SDL_Metal_CreateView(window);
    if (metal_view == nullptr) {
        return std::string("SDL_Metal_CreateView: ") + SDL_GetError();
    }
    WGPUSurfaceSourceMetalLayer src = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
    src.layer = SDL_Metal_GetLayer(metal_view);
    if (src.layer == nullptr) {
        return std::string("SDL_Metal_GetLayer: ") + SDL_GetError();
    }
    desc.nextInChain = &src.chain;
#elif defined(_WIN32)
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
    WGPUSurfaceSourceWindowsHWND src = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
    src.hwnd = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    src.hinstance = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    if (src.hwnd == nullptr) {
        return std::string("no Win32 window handle");
    }
    desc.nextInChain = &src.chain;
#else
    const SDL_PropertiesID p = SDL_GetWindowProperties(window);
    const char* driver = SDL_GetCurrentVideoDriver();
    const std::string_view name = driver != nullptr ? driver : "";
    WGPUSurfaceSourceWaylandSurface wl = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
    WGPUSurfaceSourceXlibWindow x11 = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
    if (name == "wayland") {
        wl.display = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        wl.surface = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        if (wl.display == nullptr || wl.surface == nullptr) {
            return std::string("no Wayland surface");
        }
        desc.nextInChain = &wl.chain;
    } else if (name == "x11") {
        x11.display = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        x11.window = static_cast<std::uint64_t>(
            SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
        if (x11.display == nullptr || x11.window == 0) {
            return std::string("no X11 window");
        }
        desc.nextInChain = &x11.chain;
    } else {
        return "video driver \"" + std::string(name) + "\" has no WebGPU surface";
    }
#endif
    gpu::Surface s(wgpuInstanceCreateSurface(instance, &desc));
    if (!s) {
        return std::string("wgpuInstanceCreateSurface failed");
    }
    return s;
}

} // namespace anno::app
