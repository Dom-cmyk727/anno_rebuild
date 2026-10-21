#include "viewer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <anno/app/zoom.hpp>
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
/// Frames that start this long (ms) after the previous one are reported.
/// Overridable with ANNO_VIEW_HITCH_MS, for looking at smaller hitches.
double hitch_ms() {
    static const double v = [] {
        double ms = 30.0;
        if (const char* e = std::getenv("ANNO_VIEW_HITCH_MS"); e != nullptr) {
            std::sscanf(e, "%lf", &ms);
        }
        return ms;
    }();
    return v;
}
/// One step of a frame on the window thread: the steps of a frame, laid end to
/// end, add up to its busy time (see Viewer::measure).
struct Step {
    const char* name;
    double ms;
};

/// "name ms, name ms, ...; sum S ms, busy minus sum D ms" (steps below 0.05 ms
/// left out of the list, not of the sum).
std::string describe(const std::vector<Step>& steps, double busy) {
    std::string out;
    double sum = 0;
    char buf[96];
    for (const Step& st : steps) {
        sum += st.ms;
        if (st.ms >= 0.05) {
            std::snprintf(buf, sizeof buf, "%s%s %.2f", out.empty() ? "" : ", ", st.name, st.ms);
            out += buf;
        }
    }
    std::snprintf(buf, sizeof buf, "; sum %.2f ms, busy minus sum %.2f ms", sum, busy - sum);
    return out + buf;
}
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
    // Interactive limits: chunk uploads and the wave simulation's window are
    // spread over frames, the window thread never waits for content.
    // (ANNO_VIEW_LIMITS=default: the library defaults, for comparing frame times.)
    const char* limits_env = std::getenv("ANNO_VIEW_LIMITS");
    const bool library_defaults = limits_env != nullptr && std::string(limits_env) == "default";
    gpu::GpuLimits limits = library_defaults ? gpu::GpuLimits{} : gpu::GpuLimits::interactive();
    // Chunks not built yet show the nearest zoom group's chunks, scaled
    // (OQ-077); ANNO_VIEW_STAND_INS=off shows open sea there, for comparing.
    if (const char* s = std::getenv("ANNO_VIEW_STAND_INS"); s != nullptr && *s != 0) {
        const std::string v(s);
        if (v == "off") {
            limits.stand_ins = false;
        } else if (v != "on") {
            std::fprintf(stderr, "ANNO_VIEW_STAND_INS=%s not known (on, off)\n", s);
        }
    }
    // The pre-created planes cover a view's worth of chunks: the window's size in
    // chunks (the same number at every zoom group, a chunk being kChunk screen
    // pixels), never fewer than the interactive default. A later resize is not
    // followed (OQ-093).
    if (limits.prewarm_chunks > 0) {
        limits.prewarm_chunks = std::max(
            limits.prewarm_chunks,
            render::visible_chunks({0, 0, static_cast<int>(w), static_cast<int>(h)}).size());
    }
    // The background pre-creation of chunk texture planes (GpuLimits::prewarm_chunks);
    // ANNO_VIEW_PREWARM=off creates them in the frame as before, for comparing.
    if (const char* s = std::getenv("ANNO_VIEW_PREWARM"); s != nullptr && *s != 0) {
        const std::string v(s);
        if (v == "off") {
            limits.prewarm_chunks = 0;
        } else if (v != "on") {
            std::fprintf(stderr, "ANNO_VIEW_PREWARM=%s not known (on, off)\n", s);
        }
    }
    // The cost tier of the river flow shading: automatic (High on a discrete GPU,
    // measured on others) unless ANNO_RIVER_QUALITY says low, medium or high.
    if (const char* q = std::getenv("ANNO_RIVER_QUALITY"); q != nullptr && *q != 0) {
        const std::string v(q);
        if (v == "low") {
            limits.river_quality = render::RiverQuality::Low;
        } else if (v == "medium") {
            limits.river_quality = render::RiverQuality::Medium;
        } else if (v == "high") {
            limits.river_quality = render::RiverQuality::High;
        } else if (v != "auto") {
            std::fprintf(stderr, "ANNO_RIVER_QUALITY=%s not known (auto, low, medium, high)\n", q);
        }
    }
    auto frame = gpu::GpuFrame::create(b->ctx, b->view_format, limits);
    if (!frame) {
        return frame.error();
    }
    b->frame = std::move(frame).value();
    return b;
}
#endif

#ifdef ANNO_VIEWER_GPU
/// How the resample pass reads the composed grid at scale `s`: the mean of the
/// covered pixels when reducing (nearest would skip pixels and shimmer), the
/// coverage-weighted mean (Area) when magnifying: equal-sized crisp pixels, where
/// nearest doubles every second to seventh row and column at a fractional scale.
/// ANNO_VIEW_RESAMPLE=nearest|bilinear|box|area forces one, for the owner's A/B.
gpu::ResampleFilter resample_filter(double s) {
    static const char* forced = std::getenv("ANNO_VIEW_RESAMPLE");
    if (forced != nullptr) {
        const std::string f = forced;
        if (f == "nearest") {
            return gpu::ResampleFilter::Nearest;
        }
        if (f == "bilinear") {
            return gpu::ResampleFilter::Bilinear;
        }
        if (f == "box") {
            return gpu::ResampleFilter::Box;
        }
        if (f == "area") {
            return gpu::ResampleFilter::Area;
        }
    }
    return s < 1.0 ? gpu::ResampleFilter::Box : gpu::ResampleFilter::Area;
}
#endif

class Viewer {
  public:
    Viewer(const render::Scene& scene, std::size_t group, std::size_t threads, Renderer renderer,
           render::RiverFlowMode river_flow, render::WaterStyle water, std::size_t lookahead_chunks,
           ZoomOptions zoom)
        : scene_(scene), group_(group), zoom_opt_(zoom), frames_(threads),
          renderer_choice_(renderer), water_(water) {
        frames_.river_flow = river_flow;
        frames_.max_lookahead_chunks = lookahead_chunks;
        const auto b = *scene.bounds(group);
        cam_x_ = b.x + b.w / 2.0;
        cam_y_ = b.y + b.h / 2.0;
        zoom_now_ = zoom_target_ = levels().tile_w(group);
    }

    const render::ZoomLevels& levels() const { return scene_.levels(); }

    /// The smallest tile width the continuous zoom may show: --min-zoom, else the
    /// width at which the whole scene fits the window, but never above group 2's
    /// (the original's farthest view stays reachable on a small scene).
    [[nodiscard]] double zoom_min_tile() const {
        if (zoom_opt_.min_zoom > 0.0) {
            return zoom_opt_.min_zoom;
        }
        const auto b = scene_.bounds(levels().built_count() - 1);
        if (!b || b->w <= 0 || b->h <= 0) {
            return 0.0;
        }
        const auto [w, h] = view_size();
        const double last = levels().tile_w(levels().built_count() - 1);
        return std::min(
            last, last * std::min(w / static_cast<double>(b->w), h / static_cast<double>(b->h)));
    }

    std::string run();

  private:
    void handle(const SDL_Event& e);
    void draw();
    void draw_cpu(render::Rect view);
#ifdef ANNO_VIEWER_GPU
    /// `view`: the scene grid composed (the window's rectangle, or with
    /// `resample` the larger one resampled onto the window); `out_w` x `out_h`:
    /// the window.
    void draw_gpu(render::Rect view, const gpu::Resample* resample, int out_w, int out_h);
#endif
    [[nodiscard]] bool on_gpu() const;
    void update_title();
    void pan(double dx, double dy);
    void clamp();
    void zoom(int step, std::optional<std::pair<double, double>> anchor);
    /// Continuous zoom (docs/render/iso.md "Continuous zoom"): the wanted tile
    /// width is multiplied by `factor` (clamped to the range), the point under
    /// `anchor` (window pixels; none = the window centre) stays fixed.
    void zoom_by(double factor, std::optional<std::pair<double, double>> anchor);
    /// The wanted tile width set to `tile_w` (the reset key).
    void zoom_to(double tile_w);
    /// Moves the shown zoom towards the wanted one by the time since the last
    /// call (ease-out), keeps the anchor fixed and switches the zoom group when
    /// the zoom has moved closer to another group's tile width.
    void zoom_tick();
    [[nodiscard]] bool zoom_animating() const { return continuous_ && zoom_now_ != zoom_target_; }
    /// Screen pixels per scene pixel of the group shown (1 when stepped).
    [[nodiscard]] double scale() const {
        return continuous_ ? zoom_now_ / levels().tile_w(group_) : 1.0;
    }
    /// Whether zoom group `g` can be shown without waiting for content.
    [[nodiscard]] bool group_ready(std::size_t g) const;
    void toggle_pause();
    [[nodiscard]] std::uint64_t time_ms() const;
    [[nodiscard]] std::pair<int, int> view_size() const;
    void measure(double chunks_ms, const std::array<double, 4>& stages);
    /// The steps of the frame being drawn (filled by draw_gpu / draw_cpu), the
    /// end of the last one, and the previous frame's.
    std::vector<Step> steps_, last_steps_, worst_steps_;
    Clock::time_point steps_end_ = Clock::now();

    const render::Scene& scene_;
    std::size_t group_;
    /// Scene screen position (at group_) shown in the window centre.
    double cam_x_ = 0, cam_y_ = 0;
    ZoomOptions zoom_opt_;
    /// Continuous zoom is on (asked for, and the GPU renderer runs).
    bool continuous_ = false;
    /// Tile width in screen pixels shown now and wanted (continuous zoom): the
    /// scale of the group shown is zoom_now_ / kTile[group_].w. The anchor is the
    /// point kept fixed, as an offset from the window centre in pixels.
    double zoom_now_ = 0, zoom_target_ = 0;
    std::pair<double, double> zoom_anchor_{0.0, 0.0};
    Clock::time_point zoom_tick_ = Clock::now();
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
    /// A zoom step waiting for the sea pattern of its group (only while the
    /// patterns are still being rendered after the start).
    struct PendingZoom {
        int step = 0;
        std::optional<std::pair<double, double>> anchor;
    };
    std::optional<PendingZoom> pending_zoom_;
    /// No frame before this time: the surface is occluded or timed out, or
    /// the window has no pixels (the Rust viewer's occluded_until).
    std::optional<Clock::time_point> retry_at_;
    /// Consecutive frames whose surface texture was outdated or lost.
    int surface_failures_ = 0;
    /// Present the next frame on a reconfigured surface (it was suboptimal).
    bool reconfigure_ = false;
    bool present_error_shown_ = false;
    /// Chunks received since the last periodic report. A console write on the
    /// window thread is synchronous and can cost tens of milliseconds on a busy
    /// machine, so nothing is printed per frame (hitch lines excepted: they are
    /// printed in the frame after the hitch, outside its own busy time).
    std::size_t chunks_received_ = 0;
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
    // Test hook (not documented for players): ANNO_VIEW_ZOOM_SCRIPT=<frames>,<seconds>
    // zooms one group step every <frames> drawn frames (2, 1, 0, 1, 2, ... like
    // fast mouse-wheel zooming) and quits after <seconds>; the stderr report shows
    // the frame times of the real window thread.
    std::size_t script_every_ = 0;
    double script_seconds_ = 0;
    std::size_t script_frames_ = 0;
    int script_dir_ = -1;
    Clock::time_point script_start_ = Clock::now();
    // Statistics since the last report.
    std::size_t frames_n_ = 0;
    double total_ms_ = 0, worst_ms_ = 0, chunks_ms_ = 0;
    // The whole of draw(), from its start to the end of the present call,
    // less the wait for the display (acquiring the surface texture, which
    // paces the frames with vsync): the time the window thread is busy.
    Clock::time_point draw_start_ = Clock::now();
    double display_wait_ms_ = 0, wait_sum_ms_ = 0, wait_worst_ms_ = 0;
    /// The loop is animating (frames follow back to back); the gap between
    /// two such frames' starts is a hitch if it is long.
    bool frame_animating_ = false, last_animating_ = false;
    std::optional<Clock::time_point> last_start_;
    double gap_worst_ms_ = 0, last_busy_ms_ = 0, last_wait_ms_ = 0;
    /// Handling the window events of the loop iteration that drew the frame.
    double events_ms_ = 0, last_chunks_ms_ = 0;
    std::array<double, 4> last_stages_{};
    /// The end of the previous frame's draw() (after its measure()).
    Clock::time_point last_end_ = Clock::now();
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
            // The compositor draws the ships each frame: chunks without them.
            const render::RiverFlowMode river_flow = frames_.river_flow;
            const std::size_t lookahead = frames_.max_lookahead_chunks;
            frames_ = render::FrameRenderer(frames_.threads, false);
            frames_.river_flow = river_flow;
            frames_.max_lookahead_chunks = lookahead;
            const render::RiverQuality tier = gpu_->frame->river_quality();
            std::fprintf(stderr, "GPU renderer: %s (river shading: %s)\n", gpu_->ctx.name.c_str(),
                         tier == render::RiverQuality::High     ? "high"
                         : tier == render::RiverQuality::Medium ? "medium"
                                                                : "low");
            if (const std::string& note = gpu_->frame->river_tier_note(); !note.empty()) {
                std::fprintf(stderr, "%s\n", note.c_str());
            }
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
    continuous_ = zoom_opt_.mode == ZoomMode::Continuous && on_gpu();
    if (zoom_opt_.mode == ZoomMode::Continuous && zoom_opt_.mode_explicit && !continuous_) {
        std::fprintf(stderr, "--zoom-mode continuous needs the GPU renderer; zooming in steps\n");
    }
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
    if (const char* script = std::getenv("ANNO_VIEW_ZOOM_SCRIPT")) {
        unsigned long every = 0;
        double seconds = 0;
        const bool enhanced = std::string(script).find("enhanced") != std::string::npos;
        if (std::sscanf(script, "%lu,%lf", &every, &seconds) == 2 && every > 0 && seconds > 0) {
            if (enhanced) {
                water_ = render::WaterStyle::Enhanced;
                update_title();
            }
            script_every_ = every;
            script_seconds_ = seconds;
            script_start_ = Clock::now();
            script_dir_ = group_ > 0 ? -1 : 1;
        }
    }
    const unsigned hw =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    // Sea patterns of all zoom groups, built off the window thread.
    std::vector<std::thread> seas;
    for (std::size_t g = 0; g < levels().size(); ++g) {
        seas.emplace_back([this, g] { scene_.sea(g); });
    }
    // Chunk builders use half the cores; frames use all of them.
    // Queue the river flows now (low priority threads): chunks that are built
    // before a flow is ready show the legacy river look and are rebuilt.
    scene_.prefetch_river_flow();
    frames_.set_workers(std::make_unique<render::ChunkWorkers>(scene_, (hw + 1) / 2));
    while (running_) {
        SDL_Event e;
        if (retry_at_ && Clock::now() >= *retry_at_) {
            retry_at_.reset();
        }
        // Nothing is drawn while hidden or backing off; dirty_ stays set.
        const bool can_draw = !hidden_ && !retry_at_;
        const bool animating = (!paused_at_ || frames_.busy() || zoom_animating()) && can_draw;
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
        const auto events_start = Clock::now();
        while (running_ && SDL_PollEvent(&e)) {
            handle(e);
        }
        events_ms_ = std::chrono::duration<double, std::milli>(Clock::now() - events_start).count();
        if (running_ && can_draw && (animating || dirty_)) {
            dirty_ = false;
            if (pending_zoom_) {
                const PendingZoom p = *pending_zoom_;
                pending_zoom_.reset();
                zoom(p.step, p.anchor);
            }
            frame_animating_ = animating;
            draw();
            last_end_ = Clock::now();
            if (script_every_ > 0) {
                if (++script_frames_ % script_every_ == 0) {
                    const auto next = static_cast<std::ptrdiff_t>(group_) + script_dir_;
                    if (next < 0 || next >= static_cast<std::ptrdiff_t>(levels().size())) {
                        script_dir_ = -script_dir_;
                    }
                    zoom(script_dir_, std::nullopt);
                }
                if (std::chrono::duration<double>(Clock::now() - script_start_).count() >
                    script_seconds_) {
                    running_ = false;
                }
            }
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
        case SDLK_R:
            frames_.river_flow = frames_.river_flow == render::RiverFlowMode::Flow
                                     ? render::RiverFlowMode::Legacy
                                     : render::RiverFlowMode::Flow;
            update_title();
            dirty_ = true;
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            if (continuous_) {
                // A key press is a few wheel notches.
                zoom_by(std::pow(zoom_opt_.step, 3.0), std::nullopt);
            } else {
                zoom(-1, std::nullopt);
            }
            break;
        case SDLK_MINUS:
        case SDLK_UNDERSCORE:
        case SDLK_KP_MINUS:
            if (continuous_) {
                zoom_by(std::pow(zoom_opt_.step, -3.0), std::nullopt);
            } else {
                zoom(1, std::nullopt);
            }
            break;
        case SDLK_HOME:
        case SDLK_0:
        case SDLK_KP_0:
            // Back to the default zoom: group 1's own scale.
            if (continuous_) {
                zoom_to(levels().tile_w(1));
            }
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
        if (continuous_) {
            // Every notch, also a fraction of one (a precision wheel), is a
            // factor of zoom_opt_.step; wheel up = closer.
            zoom_by(
                std::pow(zoom_opt_.step, static_cast<double>(e.wheel.y)),
                std::pair<double, double>{e.wheel.mouse_x * density, e.wheel.mouse_y * density});
            break;
        }
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
    // Screen pixels to scene pixels of the group shown: at scale s a drag of d
    // screen pixels moves the picture by d (the point under the cursor follows it).
    const double s = scale();
    cam_x_ += dx / s;
    cam_y_ += dy / s;
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
    // The stepped zoom is the original's three groups; the derived levels are for
    // the continuous zoom.
    const auto limit = continuous_ ? levels().size() : levels().built_count();
    if (next < 0 || next >= static_cast<std::ptrdiff_t>(limit)) {
        return;
    }
    const auto g = static_cast<std::size_t>(next);
    if (!group_ready(g)) {
        // The group's sea pattern is still being rendered / uploaded (the
        // first seconds after the start): zoom as soon as it is, instead of
        // the frame waiting for it.
        pending_zoom_ = PendingZoom{step, anchor};
        dirty_ = true;
        return;
    }
    const auto [w, h] = view_size();
    const double dx = anchor ? anchor->first - w / 2.0 : 0.0;
    const double dy = anchor ? anchor->second - h / 2.0 : 0.0;
    const double rw = levels().tile_w(g) / levels().tile_w(group_);
    const double rh = levels().tile_h(g) / levels().tile_h(group_);
    // Screen coordinates are linear in tile coordinates per group; keep the
    // scene point under the anchor fixed.
    const double px = (cam_x_ + dx) * rw, py = (cam_y_ + dy) * rh;
    cam_x_ = px - dx;
    cam_y_ = py - dy;
    group_ = g;
    // A stepped zoom in the continuous mode (the test script, a pending step)
    // lands on the group's own scale.
    zoom_now_ = zoom_target_ = levels().tile_w(g);
    clamp();
    update_title();
    dirty_ = true;
}

void Viewer::zoom_by(double factor, std::optional<std::pair<double, double>> anchor) {
    // Easing runs on the time since the last frame: a zoom that starts from rest
    // starts its clock now (while one is under way the clock keeps running).
    if (!zoom_animating()) {
        zoom_tick_ = Clock::now();
    }
    const auto [w, h] = view_size();
    zoom_anchor_ =
        anchor ? std::pair{anchor->first - w / 2.0, anchor->second - h / 2.0} : std::pair{0.0, 0.0};
    zoom_target_ = zoom_target_after(zoom_target_, factor, levels(), zoom_min_tile());
    dirty_ = true;
}

void Viewer::zoom_to(double tile_w) {
    if (!zoom_animating()) {
        zoom_tick_ = Clock::now();
    }
    zoom_anchor_ = {0.0, 0.0};
    zoom_target_ = tile_w;
    dirty_ = true;
}

void Viewer::zoom_tick() {
    const auto now = Clock::now();
    const double dt_ms = std::chrono::duration<double, std::milli>(now - zoom_tick_).count();
    zoom_tick_ = now;
    ZoomState s;
    s.now = zoom_now_;
    s.target = zoom_target_;
    s.anchor_x = zoom_anchor_.first;
    s.anchor_y = zoom_anchor_.second;
    s.cam_x = cam_x_;
    s.cam_y = cam_y_;
    s.group = group_;
    const std::size_t before = group_;
    s = zoom_advance(
        s, dt_ms, zoom_opt_.ease_ms, [this](std::size_t g) { return group_ready(g); }, levels(),
        zoom_opt_.max_upscale);
    zoom_now_ = s.now;
    cam_x_ = s.cam_x;
    cam_y_ = s.cam_y;
    group_ = s.group;
    if (group_ != before) {
        update_title();
    }
    clamp();
    // Keep drawing while a better level is not ready yet (its sea pattern is still
    // being built): it is taken in the first frame it is.
    const std::size_t wanted = zoom_level_for(
        zoom_now_, group_, [](std::size_t) { return true; }, levels(), zoom_opt_.max_upscale);
    dirty_ = true;
    if (zoom_now_ == zoom_target_ && wanted == group_) {
        dirty_ = false;
    }
}
bool Viewer::group_ready(std::size_t g) const {
#ifdef ANNO_VIEWER_GPU
    if (gpu_) {
        return gpu_->frame->group_ready(scene_, g);
    }
#endif
    return scene_.sea_built(g);
}

void Viewer::update_title() {
    std::string title = "anno1503 — " + scene_.name + " — zoom group " + std::to_string(group_);
    if (water_ == render::WaterStyle::Enhanced) {
        title += " — enhanced water (not original)";
        if (frames_.river_flow == render::RiverFlowMode::Legacy) {
            title += " — legacy rivers";
        }
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
    draw_start_ = Clock::now();
    display_wait_ms_ = 0;
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
        if (continuous_) {
            zoom_tick();
            const double s = scale();
            const auto rv =
                gpu::resampled_view(cam_x_, cam_y_, static_cast<std::uint32_t>(w),
                                    static_cast<std::uint32_t>(h), s, resample_filter(s));
            draw_gpu(rv.view, &rv.resample, w, h);
            // A zoom still on its way draws the next frame at once.
            dirty_ = dirty_ || zoom_animating();
            return;
        }
        draw_gpu(view, nullptr, w, h);
        return;
    }
#endif
    draw_cpu(view);
}

#ifdef ANNO_VIEWER_GPU
void Viewer::draw_gpu(render::Rect view, const gpu::Resample* resample, int out_w, int out_h) {
    GpuBackend& g = *gpu_;
    const auto w = static_cast<std::uint32_t>(out_w), h = static_cast<std::uint32_t>(out_h);
    steps_.clear();
    auto since_ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    if (g.width != w || g.height != h || reconfigure_) {
        g.configure(w, h);
        reconfigure_ = false;
    }
    WGPUSurfaceTexture st = WGPU_SURFACE_TEXTURE_INIT;
    const auto acquire_start = Clock::now();
    // Window size queries, the view rectangle and a surface (re)configuration.
    steps_.push_back({"draw prologue", since_ms(draw_start_, acquire_start)});
    wgpuSurfaceGetCurrentTexture(g.surface.get(), &st);
    const auto acquired = Clock::now();
    display_wait_ms_ = since_ms(acquire_start, acquired);
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
    const auto render_start = Clock::now();
    steps_.push_back({"surface view", since_ms(acquired, render_start)});
    const auto s =
        g.frame->render(frames_, scene_, view, group_, time_ms(), water_, target.get(), resample);
    const auto p = Clock::now();
    for (const auto& step : s.steps()) {
        steps_.push_back({step.name, step.ms});
    }
    // Locals of render() destroyed on the way out (command encoder, bind groups).
    steps_.push_back({"render exit", std::max(0.0, since_ms(render_start, p) - s.total_ms)});
    if (wgpuSurfacePresent(g.surface.get()) != WGPUStatus_Success && !present_error_shown_) {
        std::fprintf(stderr, "presenting the frame failed\n");
        present_error_shown_ = true;
    }
    steps_end_ = Clock::now();
    const double present = since_ms(p, steps_end_);
    steps_.push_back({"present", present});
    chunks_received_ += s.chunks_built; // reported with the periodic line, not per frame
    tex_w_ = out_w;
    tex_h_ = out_h;
    measure(s.chunks_ms, {s.upload_ms, s.sprites_ms, s.encode_ms, present});
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
    steps_.clear();
    auto since_ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    const auto render_start = Clock::now();
    steps_.push_back({"draw prologue", since_ms(draw_start_, render_start)});
    const auto s = frames_.render(scene_, view, group_, time_ms(), water_, buffer_.data());
    const auto rendered = Clock::now();
    const double known = s.chunks_ms + s.ground_ms + s.water_objects_ms + s.sea_ms + s.objects_ms;
    steps_.push_back({"chunks", s.chunks_ms});
    steps_.push_back({"ground", s.ground_ms});
    steps_.push_back({"water objects", s.water_objects_ms});
    steps_.push_back({"sea", s.sea_ms});
    steps_.push_back({"objects", s.objects_ms});
    steps_.push_back({"render other", std::max(0.0, since_ms(render_start, rendered) - known)});
    SDL_UpdateTexture(texture_, nullptr, buffer_.data(), w * 4);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    const auto present_start = Clock::now();
    steps_.push_back({"texture copy", since_ms(rendered, present_start)});
    SDL_RenderPresent(renderer_);
    steps_end_ = Clock::now();
    // SDL's present waits for vsync.
    display_wait_ms_ = since_ms(present_start, steps_end_);
    chunks_received_ += s.chunks_built; // reported with the periodic line, not per frame
    measure(s.chunks_ms, {s.ground_ms, s.water_objects_ms, s.sea_ms, s.objects_ms});
}

void Viewer::measure(double chunks_ms, const std::array<double, 4>& stages) {
    // The window thread's busy time: all of draw() (events excluded) from
    // its start to the end of the present call, minus the wait for the
    // display. Not just the compositor's own timer: acquiring the surface
    // texture, creating its view and presenting count as well.
    const auto now = Clock::now();
    const double total_ms =
        std::max(0.0, std::chrono::duration<double, std::milli>(now - draw_start_).count() -
                          display_wait_ms_);
    // What is left after the last step: reporting a chunk count, mostly.
    steps_.push_back({"tail", std::chrono::duration<double, std::milli>(now - steps_end_).count()});
    // The steps of a frame add up to its busy time: the only part of the frame
    // that is in none of them is the wait for the display (the surface texture
    // acquire on the GPU, SDL's present on the CPU), which is not busy time.
    // Gap between the starts of two frames of the continuous animation: the
    // true hitch (the vsync wait is in it, so it is one display interval
    // when all is well).
    if (last_start_ && last_animating_ && frame_animating_) {
        const double gap =
            std::chrono::duration<double, std::milli>(draw_start_ - *last_start_).count();
        gap_worst_ms_ = std::max(gap_worst_ms_, gap);
        if (gap > hitch_ms()) {
            std::fprintf(stderr,
                         "frame hitch: %.1f ms since the previous frame started (this frame: busy "
                         "%.1f ms, waiting for the display %.1f ms; previous frame: busy %.1f ms, "
                         "waiting %.1f ms; %.1f ms from the end of that frame to the start of "
                         "this one, of which %.1f ms handling window events); zoom group %zu\n"
                         "  previous frame steps (ms): %s\n  this frame steps (ms): %s\n",
                         gap, total_ms, display_wait_ms_, last_busy_ms_, last_wait_ms_,
                         std::chrono::duration<double, std::milli>(draw_start_ - last_end_).count(),
                         events_ms_, group_, describe(last_steps_, last_busy_ms_).c_str(),
                         describe(steps_, total_ms).c_str());
        }
    }
    last_busy_ms_ = total_ms;
    last_steps_ = steps_;
    last_wait_ms_ = display_wait_ms_;
    last_chunks_ms_ = chunks_ms;
    last_stages_ = stages;
    last_start_ = draw_start_;
    last_animating_ = frame_animating_;
    wait_sum_ms_ += display_wait_ms_;
    wait_worst_ms_ = std::max(wait_worst_ms_, display_wait_ms_);
    ++frames_n_;
    if (total_ms > worst_ms_) {
        worst_steps_ = steps_;
    }
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
                 "zoom group %zu, %dx%d px: %.1f fps, frame avg %.1f ms / worst %.1f ms (busy time "
                 "of the window thread per frame; waiting for the display: avg %.1f / worst %.1f); "
                 "worst gap between frames %.1f ms (chunks "
                 "%.1f, %s %.1f, %s %.1f, %s %.1f, %s %.1f; %zu chunks cached)\n",
                 group_, tex_w_, tex_h_, fps, total_ms_ / n, worst_ms_, wait_sum_ms_ / n,
                 wait_worst_ms_, gap_worst_ms_, chunks_ms_ / n, names[0], stages_[0] / n, names[1],
                 stages_[1] / n, names[2], stages_[2] / n, names[3], stages_[3] / n,
                 frames_.cached());
    std::fprintf(stderr, "  steps of the worst frame (%.1f ms busy): %s\n", worst_ms_,
                 describe(worst_steps_, worst_ms_).c_str());
    if (chunks_received_ > 0) {
        std::fprintf(stderr, "  chunk(s) received in this interval: %zu\n", chunks_received_);
        chunks_received_ = 0;
    }
    fps_ = std::pair{fps, total_ms_ / n};
    frames_n_ = 0;
    total_ms_ = worst_ms_ = chunks_ms_ = 0;
    wait_sum_ms_ = wait_worst_ms_ = gap_worst_ms_ = 0;
    stages_ = {};
    meter_since_ = Clock::now();
    update_title();
}

} // namespace

std::string run_viewer(const render::Scene& scene, std::size_t group, Renderer renderer,
                       render::RiverFlowMode river_flow, render::WaterStyle water,
                       std::size_t lookahead_chunks, ZoomOptions zoom) {
    if (!scene.bounds(group)) {
        return "no islands";
    }
    const std::size_t threads =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    Viewer v(scene, group, threads, renderer, river_flow, water, lookahead_chunks, zoom);
    return v.run();
}

} // namespace anno::app
