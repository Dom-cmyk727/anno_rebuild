#include "viewer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <anno/render/frame.hpp>
#ifdef ANNO_VIEWER_GPU
#include <anno/render/gpu.hpp>

#include "gpu_surface.hpp"
#endif

namespace anno::app {

namespace {

using Clock = std::chrono::steady_clock;
/// Interval of the FPS display and the stderr breakdown.
constexpr auto kReport = std::chrono::seconds(2);
constexpr std::array<const char*, 4> kStagesCpu = {"ground", "water objects", "sea", "objects"};
constexpr std::array<const char*, 4> kStagesGpu = {"uploads", "sprites", "encode", "present"};

#ifdef ANNO_VIEWER_GPU
namespace gpu = render::gpu;

/// The GPU backend: device, window surface and compositor (presented with
/// vsync). Members are declared so that they are destroyed in reverse
/// order of use: the compositor before the device, the surface before the
/// Metal view it draws into.
struct GpuBackend {
    SDL_MetalView metal_view = nullptr;
    gpu::Context ctx;
    gpu::Surface surface;
    WGPUTextureFormat format = WGPUTextureFormat_Undefined;
    /// Written as stored: a non-sRGB view of the surface texture.
    WGPUTextureFormat view_format = WGPUTextureFormat_Undefined;
    std::uint32_t width = 0, height = 0;
    std::unique_ptr<gpu::GpuFrame> frame;

    GpuBackend() = default;
    GpuBackend(const GpuBackend&) = delete;
    GpuBackend& operator=(const GpuBackend&) = delete;
    ~GpuBackend() {
        frame.reset();
        if (surface) {
            wgpuSurfaceUnconfigure(surface.get());
        }
        surface.reset();
        ctx = gpu::Context{};
        if (metal_view != nullptr) {
            SDL_Metal_DestroyView(metal_view);
        }
    }

    void configure(std::uint32_t w, std::uint32_t h) {
        WGPUSurfaceConfiguration c = WGPU_SURFACE_CONFIGURATION_INIT;
        c.device = ctx.device.get();
        c.format = format;
        c.usage = WGPUTextureUsage_RenderAttachment;
        c.width = w;
        c.height = h;
        c.viewFormatCount = view_format != format ? 1 : 0;
        c.viewFormats = &view_format;
        // Vsync (wgpu's AutoVsync on the Rust viewer); always supported.
        c.presentMode = WGPUPresentMode_Fifo;
        wgpuSurfaceConfigure(surface.get(), &c);
        width = w;
        height = h;
    }
};

/// The non-sRGB variant of an 8-bit colour format (colours are written as
/// stored, as the Rust viewer's remove_srgb_suffix).
WGPUTextureFormat linear(WGPUTextureFormat f) {
    switch (f) {
    case WGPUTextureFormat_BGRA8UnormSrgb:
        return WGPUTextureFormat_BGRA8Unorm;
    case WGPUTextureFormat_RGBA8UnormSrgb:
        return WGPUTextureFormat_RGBA8Unorm;
    default:
        return f;
    }
}

/// Opens the high-performance GPU for `window`.
formats::Result<std::unique_ptr<GpuBackend>, std::string> create_gpu(SDL_Window* window, int w,
                                                                     int h) {
    auto b = std::make_unique<GpuBackend>();
    gpu::Instance instance(wgpuCreateInstance(nullptr));
    if (!instance) {
        return std::string("wgpuCreateInstance failed");
    }
    auto surface = create_surface(instance.get(), window, b->metal_view);
    if (!surface) {
        return surface.error();
    }
    b->surface = std::move(surface).value();
    auto ctx = gpu::context_for(std::move(instance), b->surface.get());
    if (!ctx) {
        return ctx.error();
    }
    b->ctx = std::move(ctx).value();
    WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(b->surface.get(), b->ctx.adapter.get(), &caps) !=
            WGPUStatus_Success ||
        caps.formatCount == 0) {
        return std::string("the GPU cannot present to this window");
    }
    // The first 8-bit BGRA/RGBA format in the surface's order of preference
    // (wgpu's get_default_config takes the first; adapters may list 10-bit
    // or float formats first, which the compositor does not write).
    for (std::size_t i = 0; i < caps.formatCount; ++i) {
        const WGPUTextureFormat lf = linear(caps.formats[i]);
        if (lf == WGPUTextureFormat_BGRA8Unorm || lf == WGPUTextureFormat_RGBA8Unorm) {
            b->format = caps.formats[i];
            b->view_format = lf;
            break;
        }
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    if (b->format == WGPUTextureFormat_Undefined) {
        return std::string("no 8-bit RGBA/BGRA surface format");
    }
    // Configuration errors only reach the uncaptured-error callback: catch
    // them here so --renderer auto can fall back to the CPU.
    wgpuDevicePushErrorScope(b->ctx.device.get(), WGPUErrorFilter_Validation);
    b->configure(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    if (const std::string err = gpu::pop_error_scope(b->ctx.instance.get(), b->ctx.device.get());
        !err.empty()) {
        return "surface configuration: " + err;
    }
    auto frame = gpu::GpuFrame::create(b->ctx, b->view_format);
    if (!frame) {
        return frame.error();
    }
    b->frame = std::move(frame).value();
    return b;
}
#endif

class Viewer {
  public:
    Viewer(const render::Scene& scene, std::size_t group, std::size_t threads, Renderer renderer)
        : scene_(scene), group_(group), frames_(threads), renderer_choice_(renderer) {
        const auto b = *scene.bounds(group);
        cam_x_ = b.x + b.w / 2.0;
        cam_y_ = b.y + b.h / 2.0;
    }

    std::string run();

  private:
    void handle(const SDL_Event& e);
    void draw();
    void draw_cpu(render::Rect view);
#ifdef ANNO_VIEWER_GPU
    void draw_gpu(render::Rect view);
#endif
    [[nodiscard]] bool on_gpu() const;
    void update_title();
    void pan(double dx, double dy);
    void clamp();
    void zoom(int step, std::optional<std::pair<double, double>> anchor);
    void toggle_pause();
    [[nodiscard]] std::uint64_t time_ms() const;
    [[nodiscard]] std::pair<int, int> view_size() const;
    void measure(double total_ms, double chunks_ms, const std::array<double, 4>& stages);

    const render::Scene& scene_;
    std::size_t group_;
    /// Scene screen position (at group_) shown in the window centre.
    double cam_x_ = 0, cam_y_ = 0;
    render::FrameRenderer frames_;
    Renderer renderer_choice_;
#ifdef ANNO_VIEWER_GPU
    std::unique_ptr<GpuBackend> gpu_;
#endif
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int tex_w_ = 0, tex_h_ = 0;
    std::vector<std::uint32_t> buffer_;
    Clock::time_point clock_ = Clock::now();
    std::optional<Clock::duration> paused_at_;
    render::WaterStyle water_ = render::WaterStyle::Original;
    bool running_ = true, dirty_ = true, drag_ = false;
    /// No frame before this time: the surface is occluded or timed out, or
    /// the window has no pixels (the Rust viewer's occluded_until).
    std::optional<Clock::time_point> retry_at_;
    /// Consecutive frames whose surface texture was outdated or lost.
    int surface_failures_ = 0;
    /// Present the next frame on a reconfigured surface (it was suboptimal).
    bool reconfigure_ = false;
    bool present_error_shown_ = false;
    void back_off() {
        retry_at_ = Clock::now() + std::chrono::milliseconds(100);
        dirty_ = true;
    }
    /// Minimised or covered: no frames are composed.
    bool hidden_ = false;
    /// A fatal error that ended the loop.
    std::string error_;
    std::optional<std::pair<double, double>> cursor_;
    double scroll_ = 0;
    // Statistics since the last report.
    std::size_t frames_n_ = 0;
    double total_ms_ = 0, worst_ms_ = 0, chunks_ms_ = 0;
    std::array<double, 4> stages_{};
    Clock::time_point meter_since_ = Clock::now();
    std::optional<std::pair<double, double>> fps_;
};

std::string Viewer::run() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return std::string("SDL_Init: ") + SDL_GetError();
    }
    // 1280 × 800 logical size, as the Rust viewer: window sizes are pixels on
    // Windows and X11 but points on macOS and Wayland, so scale by the
    // display's content scale (1.0 where sizes are points; SDL
    // docs/README-highdpi.md).
    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (!(scale > 0.0F)) {
        scale = 1.0F;
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(ANNO_VIEWER_GPU) && defined(__APPLE__)
    if (renderer_choice_ != Renderer::Cpu) {
        flags |= SDL_WINDOW_METAL;
    }
#endif
    window_ = SDL_CreateWindow("anno1503", static_cast<int>(1280 * scale),
                               static_cast<int>(800 * scale), flags);
    if (window_ == nullptr) {
        const std::string err = std::string("SDL_CreateWindow: ") + SDL_GetError();
        SDL_Quit();
        return err;
    }
#ifdef ANNO_VIEWER_GPU
    if (renderer_choice_ != Renderer::Cpu) {
        const auto [w, h] = view_size();
        auto g = create_gpu(window_, w, h);
        if (g) {
            gpu_ = std::move(g).value();
            std::fprintf(stderr, "GPU renderer: %s\n", gpu_->ctx.name.c_str());
        } else if (renderer_choice_ == Renderer::Gpu) {
            const std::string err = "GPU renderer: " + g.error();
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return err;
        } else {
            std::fprintf(stderr, "GPU renderer unavailable (%s); using the CPU renderer\n",
                         g.error().c_str());
        }
    }
#else
    if (renderer_choice_ == Renderer::Gpu) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return "--renderer gpu: built without the GPU compositor (ANNO_BUILD_GPU)";
    }
#endif
    if (!on_gpu()) {
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            const std::string err = std::string("SDL_CreateRenderer: ") + SDL_GetError();
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return err;
        }
        // Vsync paces the frames (SDL falls back to a simulated vsync where
        // the backend has none, and while the window is hidden).
        if (!SDL_SetRenderVSync(renderer_, 1)) {
            std::fprintf(stderr, "SDL_SetRenderVSync: %s\n", SDL_GetError());
        }
    }
    update_title();
    const unsigned hw =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    // Sea patterns of all zoom groups, built off the window thread.
    std::vector<std::thread> seas;
    for (std::size_t g = 0; g < render::kGroups; ++g) {
        seas.emplace_back([this, g] { scene_.sea(g); });
    }
    // Chunk builders use half the cores; frames use all of them.
    frames_.set_workers(std::make_unique<render::ChunkWorkers>(scene_, (hw + 1) / 2));
    while (running_) {
        SDL_Event e;
        if (retry_at_ && Clock::now() >= *retry_at_) {
            retry_at_.reset();
        }
        // Nothing is drawn while hidden or backing off; dirty_ stays set.
        const bool can_draw = !hidden_ && !retry_at_;
        const bool animating = (!paused_at_ || frames_.busy()) && can_draw;
        if (!animating && !(dirty_ && can_draw)) {
            int wait_ms = 100;
            if (retry_at_) {
                const auto left =
                    std::chrono::duration_cast<std::chrono::milliseconds>(*retry_at_ - Clock::now())
                        .count();
                wait_ms = static_cast<int>(std::clamp<long long>(left, 1, 100));
            }
            if (SDL_WaitEventTimeout(&e, wait_ms)) {
                handle(e);
            }
        }
        while (running_ && SDL_PollEvent(&e)) {
            handle(e);
        }
        if (running_ && can_draw && (animating || dirty_)) {
            dirty_ = false;
            draw();
        }
    }
    frames_.set_workers(nullptr);
    for (auto& t : seas) {
        t.join();
    }
#ifdef ANNO_VIEWER_GPU
    if (gpu_) {
        gpu_->frame->wait();
    }
    gpu_.reset();
#endif
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    SDL_DestroyWindow(window_);
    SDL_Quit();
    return error_;
}

void Viewer::handle(const SDL_Event& e) {
    const float density = SDL_GetWindowPixelDensity(window_);
    switch (e.type) {
    case SDL_EVENT_QUIT:
        running_ = false;
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        dirty_ = true;
        break;
    // No frames while nobody can see them (minimised or covered).
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_OCCLUDED:
    case SDL_EVENT_WINDOW_HIDDEN:
        hidden_ = true;
        break;
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_EXPOSED:
        hidden_ = false;
        dirty_ = true;
        break;
    case SDL_EVENT_KEY_DOWN: {
        const auto [w, h] = view_size();
        const double dx = w / 4.0, dy = h / 4.0;
        switch (e.key.key) {
        case SDLK_ESCAPE:
        case SDLK_Q:
            running_ = false;
            break;
        case SDLK_LEFT:
        case SDLK_A:
            pan(-dx, 0);
            break;
        case SDLK_RIGHT:
        case SDLK_D:
            pan(dx, 0);
            break;
        case SDLK_UP:
        case SDLK_W:
            pan(0, -dy);
            break;
        case SDLK_DOWN:
        case SDLK_S:
            pan(0, dy);
            break;
        case SDLK_SPACE:
            toggle_pause();
            break;
        case SDLK_E:
            water_ = water_ == render::WaterStyle::Original ? render::WaterStyle::Enhanced
                                                            : render::WaterStyle::Original;
            update_title();
            dirty_ = true;
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            zoom(-1, std::nullopt);
            break;
        case SDLK_MINUS:
        case SDLK_UNDERSCORE:
        case SDLK_KP_MINUS:
            zoom(1, std::nullopt);
            break;
        default:
            break;
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
            drag_ = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        const std::pair<double, double> now{e.motion.x * density, e.motion.y * density};
        if (drag_ && cursor_) {
            pan(cursor_->first - now.first, cursor_->second - now.second);
        }
        cursor_ = now;
        break;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        cursor_.reset();
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        scroll_ += e.wheel.y;
        if (std::abs(scroll_) >= 1.0) {
            // Wheel up = closer = lower group.
            const int step = scroll_ > 0 ? -1 : 1;
            scroll_ = 0;
            zoom(step,
                 std::pair<double, double>{e.wheel.mouse_x * density, e.wheel.mouse_y * density});
        }
        break;
    default:
        break;
    }
}

std::pair<int, int> Viewer::view_size() const {
    int w = 1, h = 1;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    return {std::max(w, 1), std::max(h, 1)};
}

std::uint64_t Viewer::time_ms() const {
    const auto t = paused_at_ ? *paused_at_ : Clock::now() - clock_;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t).count());
}

void Viewer::toggle_pause() {
    if (paused_at_) {
        clock_ = Clock::now() - *paused_at_;
        paused_at_.reset();
    } else {
        paused_at_ = Clock::now() - clock_;
    }
    dirty_ = true;
}

void Viewer::pan(double dx, double dy) {
    cam_x_ += dx;
    cam_y_ += dy;
    clamp();
    dirty_ = true;
}

void Viewer::clamp() {
    if (const auto b = scene_.bounds(group_)) {
        cam_x_ = std::clamp(cam_x_, static_cast<double>(b->x), static_cast<double>(b->x) + b->w);
        cam_y_ = std::clamp(cam_y_, static_cast<double>(b->y), static_cast<double>(b->y) + b->h);
    }
}

void Viewer::zoom(int step, std::optional<std::pair<double, double>> anchor) {
    const auto next = static_cast<std::ptrdiff_t>(group_) + step;
    if (next < 0 || next >= static_cast<std::ptrdiff_t>(render::kGroups)) {
        return;
    }
    const auto g = static_cast<std::size_t>(next);
    const auto [w, h] = view_size();
    const double dx = anchor ? anchor->first - w / 2.0 : 0.0;
    const double dy = anchor ? anchor->second - h / 2.0 : 0.0;
    const auto ot = render::kTile[group_], nt = render::kTile[g];
    // Screen coordinates are linear in tile coordinates per group; keep the
    // scene point under the anchor fixed.
    const double px = (cam_x_ + dx) * nt.w / ot.w, py = (cam_y_ + dy) * nt.h / ot.h;
    cam_x_ = px - dx;
    cam_y_ = py - dy;
    group_ = g;
    clamp();
    update_title();
    dirty_ = true;
}

void Viewer::update_title() {
    std::string title = "anno1503 — " + scene_.name + " — zoom group " + std::to_string(group_);
    if (water_ == render::WaterStyle::Enhanced) {
        title += " — enhanced water (not original)";
    }
#ifdef ANNO_VIEWER_GPU
    title += gpu_ ? " — GPU " + gpu_->ctx.name : std::string(" — CPU");
#else
    title += " — CPU";
#endif
    if (fps_) {
        char buf[64];
        std::snprintf(buf, sizeof buf, " — %.0f fps (%.1f ms/frame)", fps_->first, fps_->second);
        title += buf;
    }
    SDL_SetWindowTitle(window_, title.c_str());
}

bool Viewer::on_gpu() const {
#ifdef ANNO_VIEWER_GPU
    return gpu_ != nullptr;
#else
    return false;
#endif
}

void Viewer::draw() {
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(window_, &pw, &ph);
    if (pw <= 0 || ph <= 0) {
        // Minimised on some platforms without a minimise event: never
        // configure a 0-sized surface; look again shortly.
        back_off();
        return;
    }
    const auto [w, h] = view_size();
    // Scene screen position of the window's top-left pixel.
    const render::Rect view{static_cast<int>(std::lround(cam_x_ - w / 2.0)),
                            static_cast<int>(std::lround(cam_y_ - h / 2.0)), w, h};
#ifdef ANNO_VIEWER_GPU
    if (gpu_) {
        draw_gpu(view);
        return;
    }
#endif
    draw_cpu(view);
}

#ifdef ANNO_VIEWER_GPU
void Viewer::draw_gpu(render::Rect view) {
    GpuBackend& g = *gpu_;
    const auto w = static_cast<std::uint32_t>(view.w), h = static_cast<std::uint32_t>(view.h);
    if (g.width != w || g.height != h || reconfigure_) {
        g.configure(w, h);
        reconfigure_ = false;
    }
    WGPUSurfaceTexture st = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(g.surface.get(), &st);
    const gpu::Texture texture(st.texture);
    // wgpu-native's own status (wgpu.h): the window is not visible (macOS
    // reports it until the window is first shown). Not in webgpu.h's enum,
    // so compared before the switch.
    if (static_cast<std::uint32_t>(st.status) ==
        static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Occluded)) {
        back_off();
        return;
    }
    switch (st.status) {
    case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        surface_failures_ = 0;
        break;
    case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
        surface_failures_ = 0;
        reconfigure_ = true;
        break;
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
        // Reconfigure and try again (as the Rust viewer); back off when it
        // keeps happening, give up when it never recovers.
        if (++surface_failures_ > 50) {
            error_ = "the window surface stays outdated or lost";
            running_ = false;
            return;
        }
        g.configure(w, h);
        if (surface_failures_ > 2) {
            back_off();
        } else {
            dirty_ = true;
        }
        return;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        back_off();
        return;
    default:
        error_ = "the window surface failed (status " + std::to_string(st.status) + ")";
        running_ = false;
        return;
    }
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    vd.format = g.view_format;
    vd.dimension = WGPUTextureViewDimension_2D;
    const gpu::TextureView target(wgpuTextureCreateView(texture.get(), &vd));
    const auto s = g.frame->render(frames_, scene_, view, group_, time_ms(), water_, target.get());
    const auto p = Clock::now();
    if (wgpuSurfacePresent(g.surface.get()) != WGPUStatus_Success && !present_error_shown_) {
        std::fprintf(stderr, "presenting the frame failed\n");
        present_error_shown_ = true;
    }
    const double present = std::chrono::duration<double, std::milli>(Clock::now() - p).count();
    if (s.chunks_built > 0) {
        std::fprintf(stderr, "zoom group %zu: %zu chunk(s) received\n", group_, s.chunks_built);
    }
    tex_w_ = view.w;
    tex_h_ = view.h;
    measure(s.total_ms + present, s.chunks_ms, {s.upload_ms, s.sprites_ms, s.encode_ms, present});
}
#endif

void Viewer::draw_cpu(render::Rect view) {
    const int w = view.w, h = view.h;
    if (texture_ == nullptr || tex_w_ != w || tex_h_ != h) {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
        }
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, w, h);
        if (texture_ == nullptr) {
            error_ = std::string("SDL_CreateTexture: ") + SDL_GetError();
            running_ = false;
            return;
        }
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        tex_w_ = w;
        tex_h_ = h;
        buffer_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    }
    const auto s = frames_.render(scene_, view, group_, time_ms(), water_, buffer_.data());
    SDL_UpdateTexture(texture_, nullptr, buffer_.data(), w * 4);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    if (s.chunks_built > 0) {
        std::fprintf(stderr, "zoom group %zu: %zu chunk(s) received\n", group_, s.chunks_built);
    }
    measure(s.total_ms, s.chunks_ms, {s.ground_ms, s.water_objects_ms, s.sea_ms, s.objects_ms});
}

void Viewer::measure(double total_ms, double chunks_ms, const std::array<double, 4>& stages) {
    ++frames_n_;
    worst_ms_ = std::max(worst_ms_, total_ms);
    total_ms_ += total_ms;
    chunks_ms_ += chunks_ms;
    for (std::size_t i = 0; i < 4; ++i) {
        stages_[i] += stages[i];
    }
    const auto elapsed = Clock::now() - meter_since_;
    if (elapsed < kReport) {
        return;
    }
    const double n = static_cast<double>(frames_n_);
    const double fps = n / std::chrono::duration<double>(elapsed).count();
    const auto& names = on_gpu() ? kStagesGpu : kStagesCpu;
    std::fprintf(stderr,
                 "zoom group %zu, %dx%d px: %.1f fps, frame avg %.1f ms / worst %.1f ms (chunks "
                 "%.1f, %s %.1f, %s %.1f, %s %.1f, %s %.1f; %zu chunks cached)\n",
                 group_, tex_w_, tex_h_, fps, total_ms_ / n, worst_ms_, chunks_ms_ / n, names[0],
                 stages_[0] / n, names[1], stages_[1] / n, names[2], stages_[2] / n, names[3],
                 stages_[3] / n, frames_.cached());
    fps_ = std::pair{fps, total_ms_ / n};
    frames_n_ = 0;
    total_ms_ = worst_ms_ = chunks_ms_ = 0;
    stages_ = {};
    meter_since_ = Clock::now();
    update_title();
}

} // namespace

std::string run_viewer(const render::Scene& scene, std::size_t group, Renderer renderer) {
    if (!scene.bounds(group)) {
        return "no islands";
    }
    const std::size_t threads =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    Viewer v(scene, group, threads, renderer);
    return v.run();
}

} // namespace anno::app
