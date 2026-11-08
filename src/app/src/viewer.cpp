#include "viewer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_script.hpp>
#include <anno/app/frame_projection.hpp>
#include <anno/app/png.hpp>
#include <anno/app/sdl_input.hpp>
#include <anno/app/ship_motion.hpp>
#include <anno/app/ui_mock.hpp>
#include <anno/app/window_capture.hpp>
#include <anno/app/zoom.hpp>
#include <anno/render/frame.hpp>
#include <anno/sim/production.hpp>
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
    /// The idle plane budget of the pre-creation when it is on (0 = off): a resize
    /// recomputes the number of chunks to keep ready from the window's size.
    std::size_t prewarm_budget_mb = 0;
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

// The window trace names the statuses by the values webgpu.h and wgpu.h give them
// (window_trace.hpp).
static_assert(kAcquireSuccessOptimal ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal));
static_assert(kAcquireSuccessSuboptimal ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal));
static_assert(kAcquireTimeout ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Timeout));
static_assert(kAcquireOutdated ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Outdated));
static_assert(kAcquireLost == static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Lost));
static_assert(kAcquireError ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Error));
static_assert(kAcquireOccluded ==
              static_cast<std::uint32_t>(WGPUSurfaceGetCurrentTextureStatus_Occluded));
static_assert(kPresentSuccess == static_cast<std::uint32_t>(WGPUStatus_Success));
static_assert(kPresentError == static_cast<std::uint32_t>(WGPUStatus_Error));

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
    // The pre-created planes cover the largest composed grid of the window and a ring
    // around it (gpu::prewarm_plan, OQ-093), bounded by the idle plane budget; a resize
    // recomputes the number (draw_gpu).
    if (limits.prewarm_chunks > 0) {
        // The idle planes' budget (GpuLimits::idle_plane_budget_mb) is the VRAM the
        // pre-creation may take: a discrete adapter has memory of its own (4 GiB and more:
        // 1 GiB holds the 260 chunks a 3840 x 2160 window wants), an integrated one shares
        // the system's, where 256 MiB stay.
        if (b->ctx.adapter_type == WGPUAdapterType_DiscreteGPU) {
            limits.idle_plane_budget_mb = std::max<std::size_t>(limits.idle_plane_budget_mb, 1024);
        }
        limits.prewarm_chunks = gpu::prewarm_plan(w, h, limits.idle_plane_budget_mb).chunks;
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
    // Test hook: ANNO_VIEW_FADE_MS=<ms> sets the chunk dissolve (GpuLimits::chunk_fade_ms), so a
    // scripted session can observe a change while its chunk is still dissolving in.
    if (const char* s = std::getenv("ANNO_VIEW_FADE_MS"); s != nullptr && *s != 0) {
        char* end = nullptr;
        const unsigned long long ms = std::strtoull(s, &end, 10);
        if (end != s && *end == 0 && s[0] != '-') {
            limits.chunk_fade_ms = ms;
        } else {
            std::fprintf(stderr, "ANNO_VIEW_FADE_MS=%s not understood (milliseconds)\n", s);
        }
    }
    b->prewarm_budget_mb = limits.prewarm_chunks > 0 ? limits.idle_plane_budget_mb : 0;
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
/// How the resample pass reads the composed grid at scale `s` (gpu::resample_filter_for):
/// the mean of the covered pixels when reducing, bilinear when magnifying (the owner's
/// choice at the stage 3 fix 1 look; nearest doubles every second to seventh row and
/// column at a fractional scale). ANNO_VIEW_RESAMPLE=nearest|bilinear|box|area forces
/// one, for the owner's A/B (area below scale 1 still becomes box).
gpu::ResampleFilter resample_filter(double s) {
    static const char* env = std::getenv("ANNO_VIEW_RESAMPLE");
    std::optional<gpu::ResampleFilter> forced;
    if (env != nullptr) {
        const std::string f = env;
        if (f == "nearest") {
            forced = gpu::ResampleFilter::Nearest;
        } else if (f == "bilinear") {
            forced = gpu::ResampleFilter::Bilinear;
        } else if (f == "box") {
            forced = gpu::ResampleFilter::Box;
        } else if (f == "area") {
            forced = gpu::ResampleFilter::Area;
        }
    }
    return gpu::resample_filter_for(s, forced);
}
#endif

class Viewer {
  public:
    ~Viewer() {
        if (hold_ != nullptr) {
            hold_->release(); // a session that ends inside a held publication frees the sim thread
        }
        if (chunk_hold_) {
            chunk_hold_->release(); // and the builders (also released before they are joined)
        }
        if (dev_ != nullptr && play_ != nullptr) {
            play_->set_batch_observer({}); // it points into knowledge_
        }
    }
    Viewer(const Viewer&) = delete;
    Viewer& operator=(const Viewer&) = delete;
    Viewer(std::shared_ptr<const render::Scene> scene, std::size_t group, std::size_t threads,
           Renderer renderer, render::RiverFlowMode river_flow, render::WaterStyle water,
           std::size_t lookahead_chunks, ZoomOptions zoom, const ViewerHooks* hooks,
           PlayFollower* play)
        : play_(play), levels_(scene->levels()), name_(scene->name), group_(group), zoom_opt_(zoom),
          frames_(threads), renderer_choice_(renderer), water_(water), hooks_(hooks),
          trace_(hooks != nullptr ? hooks->trace : nullptr),
          dev_(hooks != nullptr && play != nullptr ? hooks->dev : nullptr),
          construction_menu_(hooks != nullptr && play != nullptr ? hooks->construction_menu
                                                                 : nullptr),
          construction_game_dir_(construction_menu_ != nullptr ? hooks->construction_game_dir
                                                               : std::filesystem::path{}),
          script_(dev_ != nullptr ? hooks->script : nullptr),
          hold_(dev_ != nullptr && hooks->script != nullptr ? hooks->hold : nullptr),
          opening_(hooks != nullptr && play != nullptr ? hooks->opening : nullptr) {
        if (dev_ != nullptr) {
            knowledge_.emplace(dev_->setup());
            attach_occupancy(*play_, *knowledge_, *dev_); // follows the takes; ensure() seeds
        }
        if (construction_menu_ != nullptr) {
            construction_tex_cache_.emplace(construction_game_dir_);
            if (dev_ != nullptr && dev_->setup().registry) {
                const auto& registry = *dev_->setup().registry;
                goods_tools_ = registry.goods_by_name("WERKZEUG");
                goods_wood_ = registry.goods_by_name("HOLZ");
                goods_bricks_ = registry.goods_by_name("ZIEGEL");
                goods_marble_ = registry.goods_by_name("MAMOR");
            }
        }
        frames_.river_flow = river_flow;
        frames_.max_lookahead_chunks = lookahead_chunks;
        const auto b = *scene->bounds(group);
        cam_x_ = b.x + b.w / 2.0;
        cam_y_ = b.y + b.h / 2.0;
        zoom_now_ = zoom_target_ = levels().tile_w(group);
        if (play_ == nullptr) {
            fixed_ = std::move(scene);
        }
    }

    /// The zoom levels, the islands' placement and the sea are the same in every generation of
    /// the scene (a generation changes object lists only): the levels are copied once.
    const render::ZoomLevels& levels() const { return levels_; }
    /// The generation shown now: the picture's current one in play, else the scene of --view.
    /// An owning handle; a frame takes one at its start and draws all of it from that one.
    [[nodiscard]] std::shared_ptr<const render::Scene> current() const {
        return play_ != nullptr ? play_->picture().scene() : fixed_;
    }

    /// The smallest tile width the continuous zoom may show: --min-zoom, else the
    /// width at which the whole scene fits the window, but never above group 2's
    /// (the original's farthest view stays reachable on a small scene).
    [[nodiscard]] double zoom_min_tile() const {
        if (zoom_opt_.min_zoom > 0.0) {
            return zoom_opt_.min_zoom;
        }
        const auto b = current()->bounds(levels().built_count() - 1);
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
    /// The frame boundary of play (PlayFollower::follow): takes the sim's batches, applies them and
    /// commits what is ready, before the frame takes its scene handle. Never waits for a build.
    void follow_picture();
    /// What the loop needs from this iteration (FrameDemand, play_follow.hpp).
    [[nodiscard]] FrameDemand demand(bool can_draw) const;
    /// `gen`: the frame's scene handle, the only scene the frame reads.
    void draw_cpu(const std::shared_ptr<const render::Scene>& gen, render::Rect view);
#ifdef ANNO_VIEWER_GPU
    /// `view`: the scene grid composed (the window's rectangle, or with
    /// `resample` the larger one resampled onto the window); `out_w` x `out_h`:
    /// the window.
    void draw_gpu(const std::shared_ptr<const render::Scene>& gen, render::Rect view,
                  const gpu::Resample* resample, int out_w, int out_h);
#endif
    [[nodiscard]] bool on_gpu() const;
    void update_title();
    /// Where the change of `latest` (an applied command) is on its way to the screen
    /// (ChangeVisibility); NoVisualChange for anything else. Window thread.
    Visibility command_visibility(const std::optional<CommandOutcome>& latest) const {
        if (!latest || latest->state != CommandOutcome::State::Applied || play_ == nullptr) {
            return Visibility::NoVisualChange;
        }
        return play_->visibility().classify(latest->turn);
    }
    Visibility title_visibility_ = Visibility::NoVisualChange;
    /// Refreshes the title when the latest command's change moved to another stage (a frame
    /// presented it, or its object list committed).
    void refresh_visibility_title();
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
    /// Only the GPU path steps the continuous zoom; a viewer built without wgpu leaves it unused.
    [[maybe_unused]] void zoom_tick();
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

    /// The scene of --view (null in play: the frames draw the picture's generations).
    std::shared_ptr<const render::Scene> fixed_;
    PlayFollower* play_ = nullptr;
    /// The SDL event type the runner's batch listener posts (0: none registered).
    std::uint32_t wake_event_ = 0;
    render::ZoomLevels levels_;
    std::string name_;
    /// The frame boundary's work in the frame being drawn (follow_picture), and its end (the
    /// draw prologue is measured from there). Islands committed since the last report.
    double picture_ms_ = 0;
    Clock::time_point picture_end_ = Clock::now();
    std::size_t committed_ = 0;
    std::size_t group_;
    /// Scene screen position (at group_) shown in the window centre.
    double cam_x_ = 0, cam_y_ = 0;
    /// The part of the camera the continuous zoom's anchoring moved (ZoomState::shift_x).
    double zoom_shift_x_ = 0, zoom_shift_y_ = 0;
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
    const ViewerHooks* hooks_ = nullptr;
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
#ifdef ANNO_VIEWER_GPU
    /// Consecutive frames whose surface texture was outdated or lost.
    int surface_failures_ = 0;
    /// Present the next frame on a reconfigured surface (it was suboptimal).
    bool reconfigure_ = false;
    bool present_error_shown_ = false;
#endif
    /// The window trace (ViewerHooks::trace; null when off) and, on the GPU, its bookkeeping of
    /// the attempted frames (FrameTracer).
    WindowTrace* trace_ = nullptr;
#ifdef ANNO_VIEWER_GPU
    std::optional<FrameTracer> tracer_;
#endif
    /// The developer tool (ViewerHooks::dev), the pointer frame of the last frame drawn (what the
    /// pointer is over: its screen map, zoom geometry and scene), what the preview knows from the
    /// published picture and snapshot, and the receipt of the tool's latest command (round ee).
    DevTool* dev_ = nullptr;
    std::optional<PointerFrame> pointer_frame_;
    std::optional<PlacementKnowledge> knowledge_;
    std::optional<CommandId> latest_receipt_;
    /// The construction menu (ViewerHooks::construction_menu; --construction-menu on, W4-UI-2
    /// C4): whether the menu screen itself is showing (data/ui/construction.txt's own `open_key
    /// B` / `leave_key ESC`, checked ahead of the developer tool's own B-toggle and the window's
    /// Esc-quits-the-app binding - a real collision on both keys, resolved by state: B/Esc are
    /// the menu's own while it is open or being opened, the developer tool's and the window's
    /// otherwise), the open category (set to "public" on open; empty while closed), and whether a
    /// cell is currently armed (the placement panel is "showing", for the pickaxe/cancel-X
    /// overlap - ConstructionMenu::click's own doc comment).
    ConstructionMenu* construction_menu_ = nullptr;
    std::filesystem::path construction_game_dir_;
    /// Persists decoded `.tex` files and fonts across frames (ADR 0004 "Atlases": no re-decode
    /// per frame - the first H1/C1 slice built one fresh every call, re-reading every HUD/menu
    /// texture off disk each frame; this is the fix). Constructed once, in the constructor body,
    /// only when construction_menu_ is set (needs construction_game_dir_, itself conditional).
    std::optional<UiTexCache> construction_tex_cache_;
    /// H2: the four build-material goods, resolved by name once (registry lookup, never a
    /// hardcoded number - synthetic.cpp's own "never hardcode the goods number" convention) so
    /// draw_cpu's HudCounters can read the right slot of PlaySnapshot::goods every frame. Null
    /// when construction_menu_ is null, or when dev_'s registry lacks one of the four names
    /// (never happens for the real game files; HudCounters then just stays nullopt for it).
    std::optional<sim::GoodsId> goods_tools_, goods_wood_, goods_bricks_, goods_marble_;
    bool construction_menu_open_ = false;
    std::string construction_open_category_;
    bool construction_placement_open_ = false;
    /// The construction menu's share of handle(), checked before dev_ (ADR 0004 R1): returns
    /// whether the event was taken. Only resolves clicks while the window's drawable size is
    /// exactly 1024 x 768 (R3's scale is still open; a mismatched size leaves every click to
    /// fall through unchanged, never a guessed scale).
    bool construction_handle(const SDL_Event& e);
    /// H2's `HudCounters` and the construction screen's `UiMockSpec`/eligibility, read from the
    /// current snapshot - shared by draw_cpu and draw_gpu so the "has a city yet"/goods-unit
    /// logic lives in exactly one place. `eligibility` outlives the call (it is the snapshot's
    /// own `shared_ptr`'s pointee, kept alive by `snapshot` below); callers only read it within
    /// the same draw call, matching `draw_cpu`'s own existing contract.
    struct ConstructionHudInputs {
        UiMockSpec spec;
        HudCounters counters;
        std::shared_ptr<const PlaySnapshot> snapshot;
    };
    [[nodiscard]] ConstructionHudInputs construction_hud_inputs() const;
    /// The scripted session (ViewerHooks::script), the frames drawn and the loop's start for it.
    DevScript* script_ = nullptr;
    PublicationHold* hold_ = nullptr;
    /// A scripted GPU session's stage holds (hold chunks|uploads|presents): the builders' gate,
    /// the compositor's upload hold (set on gpu_->frame), and presents counted as failed.
    std::unique_ptr<StageHold> chunk_hold_;
    bool uploads_held_ = false, presents_failing_ = false;
    /// The CPU delivery seam's stages that report failure (script: fail update|submit|present;
    /// bit 0 SDL_UpdateTexture, 1 SDL_RenderTexture, 2 SDL_RenderPresent) and how many frames
    /// failed.
    std::uint8_t cpu_fail_ = 0;
    std::uint64_t cpu_stage_failures_[3] = {0, 0, 0}; // per stage, for the diagnostic's rate limit
    std::uint64_t surface_epoch_ = 0;
    std::uint64_t drawn_frames_ = 0;
    Clock::time_point started_ = Clock::now();
    void script_tick();
    // ---- the ship-start opening (ViewerHooks::opening) ----
    OpeningHooks* opening_ = nullptr;
    /// The view of the newest snapshot the frame took, the ships drawn from it, and the tick of
    /// the view the last DELIVERED frame (CPU: every stage returned true; GPU: the present
    /// succeeded) was drawn from.
    std::shared_ptr<const PlaySnapshot> opening_snapshot_;
    std::vector<render::UnitSprite> live_units_;
    /// What is drawn between two published turns (ships moved along their segment, turning in
    /// steps) and the wall clock it is drawn at; the sim's own positions stay in opening_snapshot_.
    ShipMotion motion_;
    OpeningView display_view_;
    /// The opening on the GPU starts with enhanced water (ships stir the simulated waves) unless
    /// --water chose the water (OpeningHooks::enhanced_water); E toggles.
    bool opening_water_set_ = false;
    /// The founding question last asked of the sim (PlayRunner::request_found_probe).
    std::optional<sim::FoundCmd> asked_probe_;
    /// The wave simulation's windows so far (GpuStats::fluid_windows), for the scripts.
    std::uint64_t fluid_windows_ = 0, fluid_resamples_ = 0;
    /// The tile the last frame's wave window was centred on (GpuStats::fluid_centre): the ships
    /// nearest to it stir the waves when there are more than fluid::kMaxShipWakes.
    std::pair<double, double> wave_centre_{0.0, 0.0};
    [[nodiscard]] static double wall_seconds() {
        static const Clock::time_point epoch = Clock::now();
        return std::chrono::duration<double>(Clock::now() - epoch).count();
    }
    std::uint64_t frame_view_tick_ = 0;
    std::optional<std::uint64_t> shown_tick_;
    std::size_t ships_missing_ = 0;
    /// The founding cursor's preview of the frame being drawn.
    std::optional<Preview> found_preview_;
    /// Takes the newest snapshot, revalidates the controls against its view, retargets the building
    /// tool at a city that appeared, and builds the ships to draw. Frame boundary; window thread.
    void opening_refresh();
    [[nodiscard]] UiFeed opening_feed() const;
    /// The named facts a scripted session waits on and expects (DevScriptState::facts).
    [[nodiscard]] std::map<std::string, std::string> opening_facts() const;
    /// Applies a UiResult: posts the commands, records their receipts, asks for a save.
    void opening_apply(const UiResult& result);
    std::string save_note_;
    /// The opening's share of handle(): returns whether the event was taken.
    bool opening_handle(const SDL_Event& e);
    /// Test hook (ANNO_VIEW_QUIT_S=<seconds>): the loop ends at this time.
    std::optional<Clock::time_point> quit_at_;
    /// Test hook (ANNO_VIEW_RESIZE=<seconds>:<w>x<h>): one resize at this time, to this drawable.
    std::optional<Clock::time_point> resize_at_;
    std::pair<int, int> resize_to_{0, 0};
    /// Chunks received since the last periodic report. A console write on the
    /// window thread is synchronous and can cost tens of milliseconds on a busy
    /// machine, so nothing is printed per frame (hitch lines excepted: they are
    /// printed in the frame after the hitch, outside its own busy time).
    std::size_t chunks_received_ = 0;
    /// The report's view of what was not ready (GPU): frames of the interval that drew a visible
    /// chunk not built yet (GpuStats::chunks_missing), the most at once and at which zoom level,
    /// and the wave windows made and carried over since the last report (owner playtest
    /// 2026-11-03, round 5: the overview "needs a moment" and islands flicker at the farthest
    /// zoom).
    std::size_t missing_frames_ = 0, missing_most_ = 0, missing_most_group_ = 0;
    /// Frames of the interval that drew chunks dissolving in over their stand-in (built, then
    /// uploaded: GpuStats::chunks_fading, GpuLimits::chunk_fade_ms) and the most at once (round 6:
    /// a zoom back in showed the coarser level magnified until the real chunks had come in).
    std::size_t fading_frames_ = 0, fading_most_ = 0;
    /// The wave windows made and carried over since the last report.
    std::uint64_t windows_new_ = 0, resamples_new_ = 0;
#ifdef ANNO_VIEWER_GPU
    /// The compositor's counters as last seen while the simulation ran (they read 0 while it is
    /// off and restart with a new compositor).
    std::uint64_t windows_seen_ = 0, resamples_seen_ = 0;
#endif
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
    // Sea patterns of all zoom levels, built off the window thread, the level shown first
    // and from the very start: opening the window and the GPU context take hundreds of
    // milliseconds, so the first frame finds the pattern it draws built (it used to build
    // it itself: 20-28 ms in the owner's log, "sea 27.96").
    std::vector<std::thread> seas;
    struct JoinAtExit {
        std::vector<std::thread>& threads;
        ~JoinAtExit() {
            for (auto& t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }
    } join_seas{seas};
    // Each thread holds the generation it builds from (the patterns are shared by every
    // generation of the scene), so a commit meanwhile cannot free what it reads.
    seas.emplace_back([s = current(), g = group_] { s->sea(g); });
    for (std::size_t g = 0; g < levels().size(); ++g) {
        if (g != group_) {
            seas.emplace_back([s = current(), g] { s->sea(g); });
        }
    }
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
    // Test hook (not documented for players): ANNO_VIEW_SIZE=<w>x<h> asks for a drawable of that
    // many pixels (the window's size in points is that over the pixel density). The size the
    // window gets is the one reported: a screen smaller than the request gives a smaller one.
    if (const char* sz = std::getenv("ANNO_VIEW_SIZE"); sz != nullptr && *sz != 0) {
        int want_w = 0, want_h = 0;
        if (std::sscanf(sz, "%dx%d", &want_w, &want_h) == 2 && want_w > 0 && want_h > 0) {
            float density = SDL_GetWindowPixelDensity(window_);
            if (!(density > 0.0F)) {
                density = 1.0F;
            }
            SDL_SetWindowSize(window_,
                              static_cast<int>(std::lround(want_w / static_cast<double>(density))),
                              static_cast<int>(std::lround(want_h / static_cast<double>(density))));
            SDL_SyncWindow(window_);
            const auto [got_w, got_h] = view_size();
            std::fprintf(stderr, "window: drawable %dx%d px (asked for %dx%d, density %.2f)\n",
                         got_w, got_h, want_w, want_h, static_cast<double>(density));
            if (trace_ != nullptr) {
                trace_->note("drawable_asked " + std::to_string(want_w) + "x" +
                             std::to_string(want_h));
            }
        } else {
            std::fprintf(stderr, "ANNO_VIEW_SIZE=%s not understood (<w>x<h>)\n", sz);
        }
    }
    // Test hook: ANNO_VIEW_AT=<x>,<y> centres the camera on that tile.
    if (const char* at = std::getenv("ANNO_VIEW_AT"); at != nullptr && *at != 0) {
        int tx = 0, ty = 0;
        if (std::sscanf(at, "%d,%d", &tx, &ty) == 2) {
            const auto [sx, sy] = render::tile_top(tx, ty, levels().ancestor(group_));
            cam_x_ = sx;
            cam_y_ = sy;
            clamp();
        } else {
            std::fprintf(stderr, "ANNO_VIEW_AT=%s not understood (<x>,<y>)\n", at);
        }
    }
    if (opening_ != nullptr) {
        // The opening starts on the participant's ship (on the sea surface, as the ship is drawn).
        const auto g = render::pick_geometry_of(levels().tile_w(group_), levels().tile_h(group_));
        const auto [sx, sy] = render::corner_point(opening_->look_x, opening_->look_y,
                                                   static_cast<double>(render::kSeaLevel), g);
        cam_x_ = sx;
        cam_y_ = sy;
        clamp();
    }
    // Test hook: ANNO_VIEW_RESIZE=<seconds>:<w>x<h> resizes the window once, that long after the
    // start, to a drawable of that many pixels (the surface is reconfigured: a new epoch).
    if (const char* rz = std::getenv("ANNO_VIEW_RESIZE"); rz != nullptr && *rz != 0) {
        double seconds = 0;
        int rw = 0, rh = 0;
        if (std::sscanf(rz, "%lf:%dx%d", &seconds, &rw, &rh) == 3 && seconds > 0 && rw > 0 &&
            rh > 0) {
            resize_at_ = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                            std::chrono::duration<double>(seconds));
            resize_to_ = {rw, rh};
        } else {
            std::fprintf(stderr, "ANNO_VIEW_RESIZE=%s not understood (<seconds>:<w>x<h>)\n", rz);
        }
    }
    if (const char* q = std::getenv("ANNO_VIEW_QUIT_S"); q != nullptr && *q != 0) {
        double seconds = 0;
        if (std::sscanf(q, "%lf", &seconds) == 1 && seconds > 0) {
            quit_at_ = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                          std::chrono::duration<double>(seconds));
        }
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
            // GPU-built chunks (T11 stage 2a): off by default; the interim switch
            // ANNO_VIEW_GPU_CHUNKS=on makes the workers produce draw lists that
            // the compositor's compute pass turns into the objects and base
            // planes (the owner check; C4 of docs/render/gpu-chunks.md).
            if (const char* s = std::getenv("ANNO_VIEW_GPU_CHUNKS"); s != nullptr && *s != 0) {
                const std::string v(s);
                if (v == "on") {
                    frames_.chunk_builder = render::ChunkBuilder::GpuList;
                    std::fprintf(stderr, "GPU chunk build: on\n");
                } else if (v != "off") {
                    std::fprintf(stderr, "ANNO_VIEW_GPU_CHUNKS=%s not known (on, off)\n", s);
                }
            }
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
    if (trace_ != nullptr) {
#ifdef ANNO_VIEWER_GPU
        if (gpu_) {
            trace_->event_now(TraceKind::GpuMode, 0, 1);
            tracer_.emplace(*trace_);
            const auto [dw, dh] = view_size();
            trace_->note("adapter " + gpu_->ctx.name);
            trace_->note("drawable " + std::to_string(dw) + "x" + std::to_string(dh) + " density " +
                         std::to_string(SDL_GetWindowPixelDensity(window_)));
            trace_->note(
                std::string("chunk_builder ") +
                (frames_.chunk_builder == render::ChunkBuilder::GpuList ? "gpu_list" : "cpu"));
        }
#endif
        if (!on_gpu()) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            return "the window trace (ANNO_VIEW_TRACE) needs the GPU renderer";
        }
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
    // Chunk builders use half the cores; frames use all of them.
    // Queue the river flows now (low priority threads): chunks that are built
    // before a flow is ready show the legacy river look and are rebuilt.
    // A batch published while the loop waits for events wakes it (the runner's sim thread posts
    // an SDL event; SDL_PushEvent is thread-safe). Cleared before the window goes, below, and on
    // every other way out of run().
    struct ClearListener {
        PlayFollower* play;
        explicit ClearListener(PlayFollower* p) : play(p) {}
        ClearListener(const ClearListener&) = delete;
        ClearListener& operator=(const ClearListener&) = delete;
        ~ClearListener() {
            if (play != nullptr) {
                play->runner().set_wake_listener({});
            }
        }
    } clear_listener{play_};
    if (play_ != nullptr) {
        wake_event_ = SDL_RegisterEvents(1);
        if (wake_event_ != 0) {
            play_->runner().set_wake_listener([type = wake_event_] {
                SDL_Event e{};
                e.type = type;
                SDL_PushEvent(&e);
            });
        }
    }
    // (The flow builders hold the generation that asked: Scene::river_flow.)
    current()->prefetch_river_flow();
    // No default scene: every chunk job holds the generation of the frame that planned it.
    {
        // Three quarters of the cores (they run below normal priority): a zoom out to the whole map
        // asks for thousands of chunks and was waiting on half of them.
        auto workers = std::make_unique<render::ChunkWorkers>(std::max<std::size_t>(1, hw * 3 / 4));
        if (script_ != nullptr && on_gpu()) {
            // A scripted GPU session may hold the chunk builders (hold chunks): the gate does
            // nothing until the script arms it.
            chunk_hold_ = std::make_unique<StageHold>();
            workers->set_test_gate(
                [g = chunk_hold_->gate()](const render::ChunkKey&, std::uint64_t) { g(); });
        }
        frames_.set_workers(std::move(workers));
        // The ship-start opening sails over the whole map and zooms out to see it: its overview
        // levels are built while the player is busy with something else.
        frames_.prewarm_far_levels = opening_ != nullptr;
        if (opening_ != nullptr) {
            // The levels from the whole map down to level 4 stay built (zoom.hpp).
            frames_.resident_level_chunks = kOpeningResidentLevelChunks;
        }
    }
    while (running_) {
        SDL_Event e;
        if (quit_at_ && Clock::now() >= *quit_at_) {
            running_ = false;
            break;
        }
        if (resize_at_ && Clock::now() >= *resize_at_) {
            resize_at_.reset();
            float density = SDL_GetWindowPixelDensity(window_);
            if (!(density > 0.0F)) {
                density = 1.0F;
            }
            SDL_SetWindowSize(
                window_,
                static_cast<int>(std::lround(resize_to_.first / static_cast<double>(density))),
                static_cast<int>(std::lround(resize_to_.second / static_cast<double>(density))));
            dirty_ = true;
        }
        if (retry_at_ && Clock::now() >= *retry_at_) {
            retry_at_.reset();
        }
        // Nothing is drawn while hidden or backing off; dirty_ stays set.
        const bool can_draw = !hidden_ && !retry_at_;
        // A batch waiting in the mailbox, a change not committed yet, chunk builds and the
        // compositor's uploads and dissolves keep the frames coming, also while paused (round eb,
        // 1A): they finish without a new turn or an input event.
        const bool animating = demand(can_draw).animating();
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
        script_tick();
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
    // Teardown (docs/app/gpu-handoff.md): the window stops calling into the picture here (its
    // owner destroys it after run_viewer returns). The chunk workers are joined (at most one
    // build per worker in progress); the generations their jobs held are released there, through
    // the reclaimer. The GPU finishes its submissions before the compositor is destroyed; the
    // wave simulation's background preparations release the generation they hold as they end.
    if (play_ != nullptr) {
        play_->runner().set_wake_listener({}); // no SDL event after the window is gone
    }
    if (chunk_hold_) {
        // A session that ends inside `hold chunks` (quit, a failed step, the window closed): the
        // workers parked in the gate must go before they are joined, or the exit waits for the
        // hold's watchdog (W4-WIN-CLOSE C2 review, B1).
        chunk_hold_->release();
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
    if (script_ != nullptr && foreign_input(e)) {
        return; // a scripted session takes only its own input (dev_script.hpp, kScriptDevice)
    }
    if (wake_event_ != 0 && e.type == wake_event_) {
        dirty_ = true; // a building batch or a command outcome (the runner's wake): draw it now
        update_title();
        return;
    }
    if (construction_menu_ != nullptr && construction_handle(e)) {
        // The construction menu first (ADR 0004 R1: "SDL3 events go to the UI first, then to the
        // world"). Only a genuine hit on one of its own rectangles returns true; everything else
        // (including every click on the map) falls through to dev_ unchanged.
        update_title();
        dirty_ = true;
        return;
    }
    if (dev_ != nullptr) {
        // The developer tool first (sdl_input.hpp: B toggles it; with it on, the left button,
        // N, P, T, X and Esc with something to cancel are its). The frame is the one drawn, so
        // the pointer picks what is on screen. Its commands go to the runner's queue; the
        // receipts are kept for the feedback.
        const bool was_on = dev_->enabled();
        const auto hovered = dev_->hover();
        ToolStep step = dev_tool_step(*dev_, e, SDL_GetWindowPixelDensity(window_),
                                      pointer_frame_ ? &*pointer_frame_ : nullptr);
        for (sim::Command& command : step.commands) {
            latest_receipt_ = play_->runner().post_command(std::move(command));
        }
        if (step.taken && step.commands.empty() && e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            e.button.button == SDL_BUTTON_LEFT) {
            // A click that sent nothing (off the island, refused locally): the title shows the
            // tool's reason, not the previous command's outcome (round ee review of C, P3).
            latest_receipt_.reset();
        }
        if (step.taken) {
            if (dev_->enabled() != was_on) {
                drag_ = false; // a left drag that began as a pan ends with it
                if (opening_ != nullptr && dev_->enabled()) {
                    opening_->ui->leave_modes(); // the pointer is the building tool's now
                }
            }
            // A motion within the same tile changes nothing shown: no redraw, no new title.
            if (e.type != SDL_EVENT_MOUSE_MOTION || dev_->hover() != hovered) {
                update_title();
                dirty_ = true;
            }
            // The window keeps its own handling of motion (the cursor, a right-button pan) and
            // of focus and visibility; the rest was the tool's.
            switch (e.type) {
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            case SDL_EVENT_WINDOW_HIDDEN:
            case SDL_EVENT_WINDOW_MINIMIZED:
                break;
            default:
                return;
            }
        }
    }
    if (opening_ != nullptr && opening_->ui != nullptr && opening_handle(e)) {
        update_title();
        dirty_ = true;
        return;
    }
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
        // The left button pans (with the tool on it is the tool's and never reaches this); the
        // right button pans too (round ee: the tool's pan).
        if (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT) {
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

UiFeed Viewer::opening_feed() const {
    UiFeed feed;
    if (opening_snapshot_ && opening_snapshot_->opening) {
        feed.view = opening_snapshot_->opening.get();
        feed.outcome = [snap = opening_snapshot_, this](CommandId id) {
            return play_->runner().outcome_at(id, *snap);
        };
    }
    feed.shown_tick = shown_tick_;
    // The terrain's height bytes at the tile corners of the scene being drawn (the island files'
    // own HEIGHT grids, what the sim's coast rule reads), for the founding preview.
    feed.vertex_height = [scene = current()](std::int64_t x,
                                             std::int64_t y) -> std::optional<std::uint8_t> {
        return scene ? scene_vertex_height(*scene, x, y) : std::nullopt;
    };
    return feed;
}

void Viewer::opening_refresh() {
    if (opening_ == nullptr || opening_->ui == nullptr || play_ == nullptr) {
        return;
    }
    std::shared_ptr<const PlaySnapshot> snap = play_->runner().snapshot();
    if (!snap || !snap->opening) {
        live_units_.clear();
        return;
    }
    const bool changed = !opening_snapshot_ || opening_snapshot_->opening != snap->opening;
    opening_snapshot_ = std::move(snap);
    const OpeningView& view = *opening_snapshot_->opening;
    const double now = wall_seconds();
    if (!opening_water_set_ && on_gpu()) {
        opening_water_set_ = true;
        if (opening_->enhanced_water) {
            water_ = render::WaterStyle::Enhanced;
            update_title();
        }
    }
    if (changed) {
        motion_.observe(view, now);
        opening_->ui->on_view(opening_feed());
        // The participant's first city exists (founded after launch): the building tool builds
        // there from now on (the city is derived from the published view, never assumed).
        if (dev_ != nullptr && !view.cities.empty()) {
            const CityView& city = view.cities.front();
            if (!dev_->setup().city || *dev_->setup().city != city.ref) {
                for (const IslandRect& r : opening_->ui->setup().islands) {
                    if (r.slot == city.ref.island.v) {
                        dev_->retarget(city.ref, TileRect{r.x0, r.y0, r.w, r.h});
                        if (knowledge_) {
                            knowledge_->retarget(dev_->setup());
                        }
                        break;
                    }
                }
            }
        }
        update_title();
    }
    const auto selected = opening_->ui->selected();
    const ShipView* ship = selected ? view.ship(*selected) : nullptr;
    ships_missing_ = 0;
    display_view_ = motion_.display(view, now);
    live_units_ = opening_->ships->units(
        display_view_, selected, ship != nullptr && ship->owner == opening_->ui->setup().local,
        &ships_missing_);
    frame_view_tick_ = view.tick;
    if (motion_.animating(now)) {
        dirty_ = true; // the next frame draws the ships a little further along
    }
    // The founding cursor asks the sim what it would answer (a turn later, in the view).
    if (const auto want = opening_->ui->probe_wanted(opening_feed());
        want && want != asked_probe_) {
        play_->runner().request_found_probe(*want);
        asked_probe_ = want;
    }
}

void Viewer::opening_apply(const UiResult& result) {
    const std::uint64_t tick =
        opening_snapshot_ && opening_snapshot_->opening ? opening_snapshot_->opening->tick : 0;
    for (std::size_t i = 0; i < result.commands.size(); ++i) {
        const CommandId id = play_->runner().post_command(result.commands[i]);
        opening_->ui->submitted(i, id, tick);
    }
    if (result.save) {
        save_note_ = opening_->quick_save ? opening_->quick_save() : "saving is not available";
    }
    update_title();
    dirty_ = true;
}

bool Viewer::construction_handle(const SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN) {
        // data/ui/construction.txt: `open_key B`, `leave_key ESC`. Both collide with an existing
        // binding (the developer tool's own B-toggle; the window's Esc-quits-the-app) - resolved
        // by state, not by picking a winner once and for all: B is the menu's own key only to
        // open it (closed -> open; already open, a category click or ESC is how it's used, so B
        // while open is left to the developer tool, consistent with ADR 0004 R1 "the developer
        // tool keeps its own key"); Esc is the menu's only while it is open (closes it, consuming
        // the event before the window's own Esc-quits binding sees it).
        if (e.key.key == SDLK_B && !construction_menu_open_) {
            construction_menu_open_ = true;
            construction_open_category_ = "public";
            construction_placement_open_ = false;
            return true;
        }
        if ((e.key.key == SDLK_ESCAPE) && construction_menu_open_) {
            construction_menu_open_ = false;
            construction_open_category_.clear();
            construction_placement_open_ = false;
            return true;
        }
        return false;
    }
    if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN || e.button.button != SDL_BUTTON_LEFT) {
        return false; // only a left click resolves against the menu; everything else is dev_'s
    }
    if (!construction_menu_open_) {
        return false; // the menu screen is not showing: every click is the map's/dev_'s
    }
    const auto [dw, dh] = view_size();
    if (dw != 1024 || dh != 768) {
        return false; // R3 (the UI scale at other sizes) is still open: never a guessed scale
    }
    if (play_ == nullptr) {
        return false;
    }
    const float density = SDL_GetWindowPixelDensity(window_);
    const double px = static_cast<double>(e.button.x) * static_cast<double>(density);
    const double py = static_cast<double>(e.button.y) * static_cast<double>(density);
    const auto snapshot = play_->runner().snapshot();
    const auto result =
        construction_menu_->click(px, py, construction_open_category_, construction_placement_open_,
                                  snapshot ? snapshot->eligibility.get() : nullptr);
    switch (result.kind) {
    case ConstructionClickKind::None:
        return false;
    case ConstructionClickKind::SwitchCategory:
        construction_open_category_ = result.category;
        return true;
    case ConstructionClickKind::Empty:
    case ConstructionClickKind::Locked:
        return true;
    case ConstructionClickKind::Arm:
        if (dev_ != nullptr && dev_->arm(result.type)) {
            construction_placement_open_ = true;
        }
        return true;
    case ConstructionClickKind::ToggleDemolish:
        if (dev_ != nullptr) {
            for (sim::Command& c : dev_->handle({ToolInput::Kind::ToggleDemolish, 0, 0},
                                                pointer_frame_ ? &*pointer_frame_ : nullptr)) {
                latest_receipt_ = play_->runner().post_command(std::move(c));
            }
        }
        return true;
    case ConstructionClickKind::Cancel:
        if (dev_ != nullptr) {
            for (sim::Command& c : dev_->handle({ToolInput::Kind::Cancel, 0, 0},
                                                pointer_frame_ ? &*pointer_frame_ : nullptr)) {
                latest_receipt_ = play_->runner().post_command(std::move(c));
            }
        }
        construction_placement_open_ = false;
        return true;
    }
    return false;
}

bool Viewer::opening_handle(const SDL_Event& e) {
    OpeningUi& ui = *opening_->ui;
    const float density = SDL_GetWindowPixelDensity(window_);
    const double d = density > 0.0F ? static_cast<double>(density) : 1.0;
    const bool tool_on = dev_ != nullptr && dev_->enabled();
    std::optional<FrameProjection> projection;
    if (pointer_frame_) {
        projection.emplace(*pointer_frame_, *opening_->ships, &display_view_);
    }
    const ScreenProjection* frame = projection ? &*projection : nullptr;
    const UiFeed feed = opening_feed();
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: {
        const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
        std::optional<OpeningKey> key = opening_key_from_sdl(e.key.key);
        if (e.key.key == SDLK_ESCAPE) {
            if (e.key.repeat) {
                return true; // a held Esc is never the window's quit, whatever the first one
                             // cancelled
            }
            key = ui.has_pending() ? std::optional<OpeningKey>(OpeningKey::Escape) : std::nullopt;
        }
        if (!key) {
            return false;
        }
        if (e.key.repeat && *key != OpeningKey::QtyDown && *key != OpeningKey::QtyUp) {
            // A held key does nothing twice; a held Esc after a cancel must not quit either.
            return *key == OpeningKey::Escape || ui.mode() != OpeningMode::Select;
        }
        if (tool_on &&
            (*key == OpeningKey::Found || *key == OpeningKey::Unload || *key == OpeningKey::Load)) {
            return false; // the building tool owns the pointer: B turns it off first
        }
        const UiResult r = ui.key(*key, shift, feed);
        opening_apply(r);
        return r.taken;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (tool_on ||
            (e.button.button != SDL_BUTTON_LEFT && e.button.button != SDL_BUTTON_RIGHT)) {
            return false;
        }
        const PointerButton button =
            e.button.button == SDL_BUTTON_LEFT ? PointerButton::Left : PointerButton::Right;
        const double px = static_cast<double>(e.button.x) * d,
                     py = static_cast<double>(e.button.y) * d;
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            ui.pointer_down(button, px, py);
            return false; // the window's own pan follows the same press
        }
        opening_apply(ui.pointer_up(button, px, py, frame, feed));
        return false;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (!tool_on) {
            ui.pointer_move(static_cast<double>(e.motion.x) * d,
                            static_cast<double>(e.motion.y) * d, frame, feed);
            if (ui.mode() == OpeningMode::Found) {
                dirty_ = true;
            }
            if (ui.mode() == OpeningMode::Found || ui.take_hover_changed()) {
                update_title();
            }
        }
        return false;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
        ui.abort_gestures();
        return false;
    default:
        return false;
    }
}

std::map<std::string, std::string> Viewer::opening_facts() const {
    std::map<std::string, std::string> f;
    if (opening_ == nullptr || !opening_snapshot_ || !opening_snapshot_->opening) {
        return f;
    }
    const OpeningView& v = *opening_snapshot_->opening;
    const OpeningUi& ui = *opening_->ui;
    const UiFeed feed = opening_feed();
    f["tick"] = std::to_string(v.tick);
    // The picture's work: the scene generation of the last drawn frame and the islands committed
    // and results discarded since the start. A sailing ship changes none of them.
    f["generation"] = pointer_frame_ && pointer_frame_->scene
                          ? std::to_string(pointer_frame_->scene->generation())
                          : "0";
    if (play_ != nullptr) {
        const ScenePicture::Stats ps = play_->picture().stats();
        f["committed"] = std::to_string(ps.committed);
        f["discarded"] = std::to_string(ps.discarded);
        f["pending_builds"] = std::to_string(play_->picture().pending());
    }
    f["gold"] = std::to_string(v.gold);
    f["cities"] = std::to_string(v.cities.size());
    f["mode"] = ui.mode() == OpeningMode::Found    ? "found"
                : ui.mode() == OpeningMode::Unload ? "unload"
                : ui.mode() == OpeningMode::Load   ? "load"
                                                   : "select";
    f["selected"] = ui.selected() ? std::to_string(ui.selected()->v) : "none";
    {
        std::optional<std::uint32_t> tons;
        if (ui.mode() == OpeningMode::Unload) {
            if (const auto plan = ui.unload_plan(opening_feed())) {
                tons = plan->tons;
            }
        } else if (ui.mode() == OpeningMode::Load) {
            if (const auto plan = ui.load_plan(opening_feed())) {
                tons = plan->tons;
            }
        }
        f["request"] = tons ? std::to_string(*tons) : "none";
    }
    {
        const auto fp =
            ui.mode() == OpeningMode::Found ? ui.found_preview(opening_feed()) : std::nullopt;
        f["found.coast"] = fp ? (fp->coast_ok ? "yes" : "no") : "none";
        f["found.rotation"] = fp ? std::to_string(fp->rotation) : "none";
        f["found.cursor"] = fp ? std::to_string(fp->cursor_rotation) : "none";
        f["found.sim"] = !fp               ? "none"
                         : !fp->sim_answer ? "pending"
                                           : std::string(sim::reject_name(*fp->sim_answer));
    }
    f["fluid.windows"] = std::to_string(fluid_windows_);
    f["fluid.resamples"] = std::to_string(fluid_resamples_);
    f["chunks.cached"] = std::to_string(frames_.cached());
    {
        // The chunks the whole scene covers at each derived level (what prewarm and the resident
        // rule count), "3:12 4:6 ...", counted as the renderer does (render::visible_chunks: a
        // negative x or y rounds down; the division here truncated, review 2026-11-03).
        std::string levels;
        const auto gen = current();
        for (std::size_t g = gen->levels().built_count(); g < gen->levels().size(); ++g) {
            if (const auto b = gen->bounds(g)) {
                levels += (levels.empty() ? "" : " ") + std::to_string(g) + ":" +
                          std::to_string(render::visible_chunks(*b).size());
            }
        }
        f["levels.chunks"] = levels;
    }
    f["tracked"] = std::to_string(ui.tracked().size());
    f["save"] = opening_->save_status ? opening_->save_status() : "";
    if (f["save"].empty()) {
        f["save"] = "none";
    }
    f["tool"] = dev_ != nullptr && dev_->enabled() ? "on" : "off";
    f["tool_city"] = dev_ != nullptr && dev_->setup().city
                         ? std::to_string(dev_->setup().city->island.v) + ":" +
                               std::to_string(dev_->setup().city->city.v)
                         : "none";
    f["latest"] = "none";
    f["latest_kind"] = "none";
    f["latest_reason"] = "none";
    f["latest_moved"] = "0";
    if (!ui.tracked().empty()) {
        const Tracked& t = ui.tracked().back();
        std::string s = track_state_name(ui.state_of(t, feed));
        for (char& c : s) {
            if (c == ' ' || c == ',' || c == '(' || c == ')') {
                c = '-';
            }
        }
        f["latest"] = s;
        f["latest_kind"] = t.kind == TrackKind::Move    ? "move"
                           : t.kind == TrackKind::Stop  ? "stop"
                           : t.kind == TrackKind::Found ? "found"
                                                        : "unload";
        if (feed.outcome) {
            const CommandOutcome out = feed.outcome(t.receipt);
            if (out.state == CommandOutcome::State::Refused) {
                f["latest_reason"] = std::string(sim::reject_name(out.status));
            }
            f["latest_moved"] = std::to_string(out.transferred_88);
        }
    }
    // Every tracked command by its index (oldest first, the bounded list): its kind and its own
    // state.
    for (std::size_t i = 0; i < ui.tracked().size(); ++i) {
        const Tracked& t = ui.tracked()[i];
        std::string s = track_state_name(ui.state_of(t, feed));
        for (char& c : s) {
            if (c == ' ' || c == ',' || c == '(' || c == ')') {
                c = '-';
            }
        }
        f["tracked." + std::to_string(i) + ".state"] = s;
        f["tracked." + std::to_string(i) + ".kind"] = t.kind == TrackKind::Move    ? "move"
                                                      : t.kind == TrackKind::Stop  ? "stop"
                                                      : t.kind == TrackKind::Found ? "found"
                                                                                   : "unload";
    }
    for (const ShipView& s : v.ships) {
        const std::string p = "ship." + std::to_string(s.id.v) + ".";
        f[p + "x"] = std::to_string(s.pos.x >> 8);
        f[p + "y"] = std::to_string(s.pos.y >> 8);
        f[p + "order"] = std::to_string(s.order_id ? *s.order_id : 0);
        f[p + "dir"] = std::to_string(s.dir16);
        f[p + "holds"] = std::to_string(s.holds.size());
        for (std::size_t i = 0; i < s.holds.size(); ++i) {
            f[p + "hold" + std::to_string(i)] = std::to_string(s.holds[i].amount_88);
        }
    }
    if (!v.cities.empty()) {
        const CityView& c = v.cities.front();
        for (std::size_t g = 0; g < c.stock_88.size(); ++g) {
            f["store." + std::to_string(g)] = std::to_string(c.stock_88[g]);
        }
    }
    return f;
}

void Viewer::toggle_pause() {
    if (paused_at_) {
        clock_ = Clock::now() - *paused_at_;
        paused_at_.reset();
    } else {
        paused_at_ = Clock::now() - clock_;
    }
    dirty_ = true;
    if (hooks_ != nullptr && hooks_->on_pause) {
        hooks_->on_pause(paused_at_.has_value());
    }
    update_title();
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
    if (opening_ != nullptr && opening_->ui != nullptr) {
        // A ship sails over the whole world, not only over the islands: the camera is kept inside
        // the world's rectangle (its four corners at the sea surface), not inside the islands'
        // bounds.
        const OpeningSetup& s = opening_->ui->setup();
        const auto g = render::pick_geometry_of(levels().tile_w(group_), levels().tile_h(group_));
        const double sea = static_cast<double>(render::kSeaLevel);
        const auto a = render::corner_point(0, 0, sea, g);
        const auto b = render::corner_point(s.world_w, 0, sea, g);
        const auto c = render::corner_point(0, s.world_h, sea, g);
        const auto d = render::corner_point(s.world_w, s.world_h, sea, g);
        cam_x_ = std::clamp(cam_x_, std::min({a.first, b.first, c.first, d.first}),
                            std::max({a.first, b.first, c.first, d.first}));
        cam_y_ = std::clamp(cam_y_, std::min({a.second, b.second, c.second, d.second}),
                            std::max({a.second, b.second, c.second, d.second}));
        return;
    }
    if (const auto b = current()->bounds(group_)) {
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
    zoom_shift_x_ *= rw;
    zoom_shift_y_ *= rh;
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
    s.shift_x = zoom_shift_x_;
    s.shift_y = zoom_shift_y_;
    s.group = group_;
    const std::size_t before = group_;
    s = zoom_advance(
        s, dt_ms, zoom_opt_.ease_ms, [this](std::size_t g) { return group_ready(g); }, levels(),
        zoom_opt_.max_upscale);
    zoom_now_ = s.now;
    cam_x_ = s.cam_x;
    cam_y_ = s.cam_y;
    zoom_shift_x_ = s.shift_x;
    zoom_shift_y_ = s.shift_y;
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
    // The query reads the sea patterns only (shared by all generations); the handle is held
    // for the call.
    const std::shared_ptr<const render::Scene> scene = current();
#ifdef ANNO_VIEWER_GPU
    if (gpu_) {
        return gpu_->frame->group_ready(*scene, g);
    }
#endif
    return scene->sea_built(g);
}

FrameDemand Viewer::demand(bool can_draw) const {
    FrameDemand d;
    d.can_draw = can_draw;
    d.paused = paused_at_.has_value();
    d.chunks_busy = frames_.busy();
    d.zoom_animating = zoom_animating();
    d.play_work = play_ != nullptr && play_->has_work();
#ifdef ANNO_VIEWER_GPU
    d.gpu_work = gpu_ && gpu_->frame->work_pending();
#endif
    d.dirty = dirty_;
    return d;
}

void Viewer::refresh_visibility_title() {
    if (dev_ == nullptr || play_ == nullptr || !dev_->enabled() || !latest_receipt_) {
        return;
    }
    const Visibility now = command_visibility(play_->runner().feedback(latest_receipt_).latest);
    if (now != title_visibility_) {
        title_visibility_ = now;
        update_title();
    }
}

void Viewer::update_title() {
    // Whatever stage the title is about to show is the stage refresh_visibility_title compares to.
    if (dev_ != nullptr && play_ != nullptr && latest_receipt_) {
        title_visibility_ = command_visibility(play_->runner().feedback(latest_receipt_).latest);
    }
    std::string title = "anno1503 — " + name_ + " — zoom group " + std::to_string(group_);
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
    const bool dev_on = dev_ != nullptr && dev_->enabled() && knowledge_;
    if (opening_ != nullptr && opening_->ui != nullptr) {
        // The opening's own line comes first: a caption shows about 150 characters of a 1280 px
        // window, and the command state, the advisory and the pointer are what the owner reads.
        std::string lead = opening_->ui->status_line(opening_feed());
        if (paused_at_) {
            // Space pauses the sim and the water: say so where it cannot be missed.
            lead = "PAUSED (Space resumes) | " + lead;
        }
        if (!save_note_.empty()) {
            lead += " | " + save_note_;
        }
        if (opening_->save_status) {
            if (const std::string s = opening_->save_status(); !s.empty()) {
                lead += " | " + s;
            }
        }
        if (ships_missing_ > 0) {
            lead += " | " + std::to_string(ships_missing_) + " ship(s) without sprites";
        }
        title = lead + " — " + title;
    }
    if (hooks_ != nullptr && hooks_->title && !dev_on) {
        const std::string extra = hooks_->title();
        if (!extra.empty()) {
            title += " — ";
            title += extra;
        }
    }
    if (dev_on) {
        // The runner's line and the feedback from one snapshot (dev_title), published data only
        // (dev_feedback.hpp): the snapshot, the picture's buildings, and the outcome of the
        // tool's latest command by its receipt.
        // One consistent read (PlayRunner::feedback): the snapshot first, the outcome as of its
        // publication, so "Applied" never stands next to money from before it. The runner
        // records a turn's outcomes before it swaps the snapshot, so reading the outcome first
        // does not give that (round ee review, R1).
        const PlayFeedback read = play_->runner().feedback(latest_receipt_);
        const std::optional<CommandOutcome>& latest = read.latest;
        const auto& snap = read.snapshot;
        // The developer inspection UI (W4-SETTLEMENT-1 W4): ask for the hovered tile's building
        // now, shown from `snap->inspection` next time (same one-turn-lag pattern as the founding
        // probe below — this frame's title still reads what the previous request answered).
        play_->runner().request_inspect(dev_->hover());
        // Why there is no preview, from the frame the pointer is over (its scene and zoom level).
        std::string note = pointer_frame_ && pointer_frame_->scene
                               ? preview_note(*dev_, *pointer_frame_->scene, pointer_frame_->group)
                               : std::string();
        // Whether the picture shows the latest command's change yet (round ee, E4).
        if (const std::string drawn = drawn_note(latest, command_visibility(latest));
            !drawn.empty()) {
            note += (note.empty() ? "" : " | ") + drawn;
        }
        title +=
            " — " + dev_title(*snap, *dev_, knowledge_->check(*dev_, snap.get()), latest, note);
        // Against the hover read just now (dev_->hover()), not the one `inspection` answered: a
        // hover that has moved since, or landed on empty ground, must not show the old building's
        // line (W4-SETTLEMENT-1 W7 review, finding 3).
        if (const BuildingInspection* insp = live_inspection(*snap, dev_on, dev_->hover())) {
            title += " — " + insp->kind;
            if (!insp->status.empty()) {
                title += " " + insp->status;
            }
        }
    }
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

void Viewer::follow_picture() {
    const auto start = Clock::now();
    if (play_ != nullptr) {
        const auto followed = play_->followed_turn();
        const std::size_t n = play_->follow(frames_, trace_);
        committed_ += n;
        if ((n > 0 || play_->followed_turn() != followed) && dev_ != nullptr && dev_->enabled()) {
            update_title(); // a commit or a newer followed turn can move "not drawn yet" on
        }
#ifdef ANNO_VIEWER_GPU
        if (tracer_ && n > 0) {
            const std::uint64_t g = play_->picture().scene()->generation();
            for (const ScenePicture::CommittedIsland& c : play_->picture().last_commit()) {
                tracer_->committed(static_cast<std::uint32_t>(c.index), g, c.keys);
            }
        }
#endif
    }
    picture_end_ = Clock::now();
    picture_ms_ = std::chrono::duration<double, std::milli>(picture_end_ - start).count();
}

void Viewer::script_tick() {
    if (script_ == nullptr || script_->done()) {
        return;
    }
    if (!pointer_frame_) {
        // The script starts once a frame was drawn; a window that never draws one fails it
        // within the script's wait limit instead of running forever (review of E3).
        const double waited =
            std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
        if (waited > DevScript::kWaitLimitMs) {
            script_->abort("no frame was drawn within " +
                           std::to_string(static_cast<int>(DevScript::kWaitLimitMs)) + " ms");
            running_ = false;
        }
        return;
    }
    DevScriptState st;
    st.frames = drawn_frames_;
    st.now_ms = std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
    {
        PlayFeedback read = play_->runner().feedback(latest_receipt_); // one consistent read (R1)
        st.snapshot = std::move(read.snapshot);
        st.latest = std::move(read.latest);
    }
    st.visibility = command_visibility(st.latest);
    // Set alongside st.facts below (opening_facts() reads opening_snapshot_, refreshed only once
    // per drawn frame in draw()/opening_refresh(), never here): this is the turn opening_snapshot_
    // belongs to, not the live one above, so `wait turns` cannot resolve ahead of what the facts
    // it gates (`expect fact ...`) can actually show (W4-OPENING-CONTROLS-FLAKE). In a session
    // without an opening, opening_snapshot_ is null and facts_turn_for falls back to the live
    // turn, as dev_script.hpp documents; the inspection.* facts below are built from that read.
    st.facts_turn = facts_turn_for(opening_snapshot_.get(), st.snapshot.get());
    st.tool = dev_;
    st.knowledge = knowledge_ ? &*knowledge_ : nullptr;
    if (chunk_hold_) {
        st.stage_holds_available = true;
        st.chunks_blocked = chunk_hold_->blocked();
        st.chunks_armed = chunk_hold_->armed();
        st.uploads_held = uploads_held_;
        st.presents_failing = presents_failing_;
    }
    st.scene_generation =
        pointer_frame_ && pointer_frame_->scene ? pointer_frame_->scene->generation() : 0;
    st.surface_epoch = surface_epoch_;
    st.cpu_fail_available = !on_gpu();
    st.cpu_failing = cpu_fail_;
    if (hold_ != nullptr) {
        st.hold_available = true;
        st.held = hold_->held();
        st.hold_expired = hold_->expired();
    }
    st.frame = &*pointer_frame_;
    st.density = SDL_GetWindowPixelDensity(window_);
    if (!(st.density > 0.0F)) {
        st.density = 1.0F;
    }
    SDL_GetWindowSize(window_, &st.window_w, &st.window_h);
    if (const char* t = SDL_GetWindowTitle(window_); t != nullptr) {
        st.title = t;
    }
    // The developer inspection UI's facts (W4-SETTLEMENT-1 W5): available in any session, not
    // only an opening one — Proof B (an existing-town session with no ship-start opening) needs
    // these to assert production/worker activity, which opening_facts() below never reports.
    // Against the live hover, not just whatever `inspection` last answered (W4-SETTLEMENT-1 W7
    // review, finding 3): while paused no new turn publishes to retract a stale answer once the
    // hover moves, lands on empty ground, or the tool is switched off.
    if (st.snapshot != nullptr) {
        if (const BuildingInspection* insp =
                live_inspection(*st.snapshot, dev_ != nullptr && dev_->enabled(),
                                dev_ != nullptr ? dev_->hover() : std::nullopt)) {
            st.facts["inspection.kind"] = insp->kind;
            st.facts["inspection.status"] = insp->status;
            st.facts["inspection.type"] = std::to_string(insp->type.v);
            st.facts["inspection.prodtype"] = std::to_string(insp->prodtype);
            st.facts["inspection.tile.x"] = std::to_string(insp->tile.x);
            st.facts["inspection.tile.y"] = std::to_string(insp->tile.y);
        }
    }
    if (opening_ != nullptr) {
        for (auto& [key, value] : opening_facts()) {
            st.facts[key] = std::move(value);
        }
        st.ship_point = [this](std::uint64_t id) -> std::optional<std::pair<float, float>> {
            if (!opening_snapshot_ || !opening_snapshot_->opening || !pointer_frame_) {
                return std::nullopt;
            }
            const ShipView* ship = display_view_.ship(sim::FigureId{id});
            const float density = SDL_GetWindowPixelDensity(window_);
            if (ship == nullptr || !(density > 0.0F)) {
                return std::nullopt;
            }
            const auto box = opening_->ships->box(*ship, *pointer_frame_);
            if (!box) {
                return std::nullopt;
            }
            return std::pair{static_cast<float>(box->cx / density),
                             static_cast<float>(box->cy / density)};
        };
    }
    // W4-OPENING-CONTROLS-FLAKE step 1: an opt-in trace (no cost unless set) of the two readings a
    // scripted "wait turns" + "expect fact tracked..." pair can race between: `st.snapshot` is a
    // fresh feedback() read taken every call, but the facts built above from opening_snapshot_
    // (opening_facts()) are only as new as the last drawn frame (opening_refresh(), draw()-only).
    // Under frame-rate collapse the live turn can run far ahead of the frame-gated one.
    const char* trace_env = std::getenv("ANNO_DEV_SCRIPT_TRACE");
    const std::uint64_t trace_live_turn = st.snapshot ? st.snapshot->turn : 0;
    const std::uint64_t trace_opening_turn = opening_snapshot_ ? opening_snapshot_->turn : 0;
    DevScriptAction act = script_->step(st);
    if (trace_env != nullptr) {
        std::fprintf(stderr,
                     "[dev-script-trace] drawn_frames=%llu live_turn=%llu opening_turn=%llu "
                     "gap=%lld tracked.1.state=%s log=%s\n",
                     static_cast<unsigned long long>(drawn_frames_),
                     static_cast<unsigned long long>(trace_live_turn),
                     static_cast<unsigned long long>(trace_opening_turn),
                     static_cast<long long>(trace_live_turn) -
                         static_cast<long long>(trace_opening_turn),
                     st.facts.count("tracked.1.state") != 0 ? st.facts.at("tracked.1.state").c_str()
                                                            : "<none>",
                     script_->log().empty() ? "" : script_->log().back().c_str());
    }
    if (act.look_world) {
        const auto g = render::pick_geometry_of(levels().tile_w(group_), levels().tile_h(group_));
        const auto [sx, sy] = render::corner_point(act.look_world->first, act.look_world->second,
                                                   static_cast<double>(render::kSeaLevel), g);
        cam_x_ = sx;
        cam_y_ = sy;
        clamp();
        dirty_ = true;
    }
    if (act.foreign != DevScriptAction::Foreign::None && dev_ != nullptr && dev_->setup().foreign) {
        const ForeignSpot& f = *dev_->setup().foreign;
        sim::Command c;
        c.issuer = f.owner;
        if (act.foreign == DevScriptAction::Foreign::Build) {
            sim::BuildCmd b;
            b.city = f.city;
            b.placements.push_back(sim::PlacementWire{f.type, f.at.x, f.at.y, 0, 0, 0});
            c.body = std::move(b);
        } else {
            c.body = sim::DemolishCmd{f.city, {f.at}};
        }
        static_cast<void>(play_->runner().post_command(std::move(c)));
    }
    if (act.local != DevScriptAction::Local::None && dev_ != nullptr && dev_->setup().city) {
        sim::Command c;
        c.issuer = dev_->setup().issuer;
        if (act.local == DevScriptAction::Local::Build) {
            sim::BuildCmd b;
            b.city = *dev_->setup().city;
            b.placements.push_back(
                sim::PlacementWire{act.local_type, act.local_at.x, act.local_at.y, 0, 0, 0});
            c.body = std::move(b);
        } else {
            c.body = sim::DemolishCmd{*dev_->setup().city, {act.local_at}};
        }
        static_cast<void>(play_->runner().post_command(std::move(c)));
    }
    if (act.stage != DevScriptAction::Stage::None && chunk_hold_) {
        using Stage = DevScriptAction::Stage;
        switch (act.stage) {
        case Stage::ArmChunks:
            chunk_hold_->arm();
            break;
        case Stage::ReleaseChunks:
            chunk_hold_->release();
            break;
        case Stage::ArmUploads:
        case Stage::ReleaseUploads:
            uploads_held_ = act.stage == Stage::ArmUploads;
#ifdef ANNO_VIEWER_GPU
            if (gpu_) {
                gpu_->frame->set_test_hold_uploads(uploads_held_);
            }
#endif
            break;
        case Stage::FailPresents:
        case Stage::PassPresents:
            presents_failing_ = act.stage == Stage::FailPresents;
            dirty_ = true;
            break;
        case Stage::None:
            break;
        }
    }
    if (act.cpu_fail_set != 0 || act.cpu_fail_clear != 0) {
        cpu_fail_ = static_cast<std::uint8_t>((cpu_fail_ | act.cpu_fail_set) & ~act.cpu_fail_clear);
        dirty_ = true;
    }
    if (act.resize) {
        SDL_SetWindowSize(window_, act.resize->first, act.resize->second);
        dirty_ = true;
    }
    if (hold_ != nullptr && act.hold != DevScriptAction::Hold::None) {
        if (act.hold == DevScriptAction::Hold::Arm) {
            hold_->arm();
        } else {
            hold_->release();
        }
    }
    for (SDL_Event& e : act.events) {
        e.common.timestamp = SDL_GetTicksNS();
        if (!SDL_PushEvent(&e)) {
            std::fprintf(stderr, "dev script: SDL_PushEvent: %s\n", SDL_GetError());
        }
    }
    if (act.look) {
        // The camera on the tile's drawn centre at the zoom level last drawn (the forward rule
        // of the pick, DevScript::scene_point).
        if (const auto p = DevScript::scene_point(*pointer_frame_, *dev_, *act.look)) {
            cam_x_ = p->first;
            cam_y_ = p->second;
            clamp();
        }
    }
    if (act.screenshot) {
        // The window's own rectangle only (never the rest of the screen), into the session
        // directory (ANNO_DEV_SESSION): macOS's screencapture. Elsewhere: logged as not run.
        const char* dir = std::getenv("ANNO_DEV_SESSION");
#ifdef __APPLE__
        int x = 0, y = 0, w = 0, h = 0;
        SDL_GetWindowPosition(window_, &x, &y);
        SDL_GetWindowSize(window_, &w, &h);
        if (dir != nullptr && *dir != 0) {
            const std::string file =
                (std::filesystem::path(dir) / (*act.screenshot + ".png")).string();
            const std::string cmd = "screencapture -x -R" + std::to_string(x) + "," +
                                    std::to_string(y) + "," + std::to_string(w) + "," +
                                    std::to_string(h) + " '" + file + "'";
            const int rc = std::system(cmd.c_str());
            script_->note("screenshot " + *act.screenshot + ": " + file + " (" + std::to_string(w) +
                          "x" + std::to_string(h) + " points at " + std::to_string(x) + "," +
                          std::to_string(y) + ", exit " + std::to_string(rc) + ")");
        } else {
            script_->note("screenshot " + *act.screenshot + ": NOT_RUN (no ANNO_DEV_SESSION)");
        }
#elif defined(_WIN32)
        // Windows: the CPU renderer's own frame buffer, else a copy of the window's client area
        // from the desktop (the swapchain image of a GPU window). The size actually captured is
        // logged.
        if (dir != nullptr && *dir != 0) {
            const std::filesystem::path file =
                std::filesystem::path(dir) / (*act.screenshot + ".png");
            std::string error, how;
            int w = 0, h = 0;
            if (!on_gpu() && !buffer_.empty() && tex_w_ > 0 && tex_h_ > 0) {
                w = tex_w_;
                h = tex_h_;
                std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4);
                for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
                    const std::uint32_t px = buffer_[i];
                    rgba[i * 4] = static_cast<std::uint8_t>(px >> 16);
                    rgba[i * 4 + 1] = static_cast<std::uint8_t>(px >> 8);
                    rgba[i * 4 + 2] = static_cast<std::uint8_t>(px);
                    rgba[i * 4 + 3] = 255;
                }
                error = write_png(file, static_cast<std::uint32_t>(w),
                                  static_cast<std::uint32_t>(h), rgba);
                how = "CPU frame buffer";
            } else {
                error = capture_window_screen(window_, file, w, h);
                how = "desktop copy of the window";
            }
            script_->note(error.empty()
                              ? "screenshot " + *act.screenshot + ": " + file.string() + " (" +
                                    std::to_string(w) + "x" + std::to_string(h) + " drawable px, " +
                                    how + ")"
                              : "screenshot " + *act.screenshot + ": NOT_RUN (" + error + ")");
        } else {
            script_->note("screenshot " + *act.screenshot + ": NOT_RUN (no ANNO_DEV_SESSION)");
        }
#else
        static_cast<void>(dir);
        script_->note("screenshot " + *act.screenshot + ": NOT_RUN (no capture on this platform)");
#endif
    }
    if (!act.events.empty() || act.look) {
        dirty_ = true;
    }
    if (act.quit || script_->done()) {
        running_ = false;
    }
}

void Viewer::draw() {
    ++drawn_frames_;
    draw_start_ = Clock::now();
    display_wait_ms_ = 0;
    // The frame boundary: the picture first, then the one scene handle of this frame.
    follow_picture();
    opening_refresh(); // the ships of the newest snapshot (an opening only)
    const std::shared_ptr<const render::Scene> gen = current();
    if (knowledge_ && play_ != nullptr) {
        // The first seed, and the reseed after a spell with the tool off, from the picture's
        // per-island list (O(the city), once); the takes keep it current otherwise (R4).
        knowledge_->ensure(play_->picture(), *gen, dev_->enabled());
        if (knowledge_->take_changed()) {
            update_title(); // the occupancy the preview knows changed
        }
    }
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
    // The point a wheel zoom would keep fixed: the one under the pointer (FrameRenderer::
    // zoom_anchor builds the neighbouring groups' chunks around it).
    if (cursor_) {
        const double s = scale();
        frames_.zoom_anchor = std::pair{cam_x_ + (cursor_->first - w / 2.0) / s,
                                        cam_y_ + (cursor_->second - h / 2.0) / s};
    } else {
        frames_.zoom_anchor.reset();
    }
#ifdef ANNO_VIEWER_GPU
    if (gpu_) {
        if (continuous_) {
            zoom_tick();
            // The margin's velocity follows the pan, not the zoom's own camera move.
            frames_.motion_centre = std::pair{cam_x_ - zoom_shift_x_, cam_y_ - zoom_shift_y_};
            const double s = scale();
            const auto rv =
                gpu::resampled_view(cam_x_, cam_y_, static_cast<std::uint32_t>(w),
                                    static_cast<std::uint32_t>(h), s, resample_filter(s));
            draw_gpu(gen, rv.view, &rv.resample, w, h);
            // A zoom still on its way draws the next frame at once.
            dirty_ = dirty_ || zoom_animating();
            return;
        }
        draw_gpu(gen, view, nullptr, w, h);
        return;
    }
#endif
    draw_cpu(gen, view);
}

#ifdef ANNO_VIEWER_GPU
void Viewer::draw_gpu(const std::shared_ptr<const render::Scene>& gen, render::Rect view,
                      const gpu::Resample* resample, int out_w, int out_h) {
    GpuBackend& g = *gpu_;
    const auto w = static_cast<std::uint32_t>(out_w), h = static_cast<std::uint32_t>(out_h);
    steps_.clear();
    auto since_ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    // The window trace's row of this attempt, recorded on every way out (docs/app/window-trace.md).
    if (play_ != nullptr) {
        // Decides for each change committed since the last attempt whether this attempt's view
        // holds it (ChangeVisibility), with or without a trace.
        play_->visibility().begin_frame(view, group_);
    }
    if (tracer_) {
        FrameRecord rec;
        rec.generation = gen->generation();
        rec.view_x = view.x;
        rec.view_y = view.y;
        rec.view_w = view.w;
        rec.view_h = view.h;
        rec.group = group_;
        rec.scale = scale();
        rec.drawable_w = w;
        rec.drawable_h = h;
        rec.t_start_ms = trace_->ms_at(draw_start_);
        rec.picture_ms = picture_ms_;
        tracer_->begin(std::move(rec), view, group_);
    }
    // A failed attempt: frame_failed at the acquire's end, and its row.
    auto attempt_failed = [this](const char* outcome) {
        if (tracer_) {
            tracer_->failed(outcome);
        }
    };
    auto reconfigured = [this] {
        ++surface_epoch_;
        if (tracer_) {
            tracer_->reconfigured();
        }
    };
    steps_.push_back({"picture", picture_ms_});
    // The prologue: window size queries, the view rectangle and, on a resize, the surface
    // (re)configuration, reported as its own step ("surface configure").
    auto prologue_from = picture_end_;
    if (g.width != w || g.height != h || reconfigure_) {
        const auto configure_start = Clock::now();
        steps_.push_back({"draw prologue", since_ms(picture_end_, configure_start)});
        g.configure(w, h);
        reconfigured();
        reconfigure_ = false;
        if (g.prewarm_budget_mb > 0 && g.frame) {
            g.frame->set_prewarm_chunks(
                gpu::prewarm_plan(static_cast<int>(w), static_cast<int>(h), g.prewarm_budget_mb)
                    .chunks);
        }
        prologue_from = Clock::now();
        steps_.push_back({"surface configure", since_ms(configure_start, prologue_from)});
    }
    WGPUSurfaceTexture st = WGPU_SURFACE_TEXTURE_INIT;
    const auto acquire_start = Clock::now();
    steps_.push_back({"draw prologue", since_ms(prologue_from, acquire_start)});
    wgpuSurfaceGetCurrentTexture(g.surface.get(), &st);
    const auto acquired = Clock::now();
    display_wait_ms_ = since_ms(acquire_start, acquired);
    const gpu::Texture texture(st.texture);
    // The status as webgpu.h and wgpu.h define it; wgpu-native's Occluded (the window is not
    // visible; macOS reports it until the window is first shown) is not in webgpu.h's enum, so
    // the action is chosen from the number (classify_acquire, window_trace.hpp).
    const auto status = static_cast<std::uint32_t>(st.status);
    const AcquireAction action = classify_acquire(status);
    if (tracer_) {
        tracer_->acquired(trace_->ms_at(acquire_start), trace_->ms_at(acquired), status);
    }
    switch (action) {
    case AcquireAction::Draw:
        surface_failures_ = 0;
        break;
    case AcquireAction::DrawThenReconfigure:
        surface_failures_ = 0;
        reconfigure_ = true;
        break;
    case AcquireAction::Reconfigure:
        // Reconfigure and try again (as the Rust viewer); back off when it
        // keeps happening, give up when it never recovers.
        if (++surface_failures_ > kAcquireGiveUpAfter) {
            attempt_failed("fatal");
            error_ = "the window surface stays outdated or lost";
            running_ = false;
            return;
        }
        attempt_failed("acquire_retry");
        g.configure(w, h);
        reconfigured();
        if (surface_failures_ > kAcquireBackOffAfter) {
            back_off();
        } else {
            dirty_ = true;
        }
        return;
    case AcquireAction::BackOff:
        attempt_failed("acquire_skipped");
        back_off();
        return;
    case AcquireAction::Fatal:
        attempt_failed("fatal");
        error_ = "the window surface failed (status " + std::to_string(status) + ")";
        running_ = false;
        return;
    }
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    vd.format = g.view_format;
    vd.dimension = WGPUTextureViewDimension_2D;
    const gpu::TextureView target(wgpuTextureCreateView(texture.get(), &vd));
    const auto render_start = Clock::now();
    steps_.push_back({"surface view", since_ms(acquired, render_start)});
    // The developer tool's preview: its sprites, drawn last (dev_feedback.hpp).
    std::vector<render::SpriteDraw> overlay;
    if (dev_ != nullptr) {
        if (auto p = make_preview(*dev_, *gen, group_, time_ms())) {
            overlay = std::move(p->sprites);
        }
    }
    if (opening_ != nullptr) {
        // The founding cursor: the warehouse sprite at the pointer, over the picture.
        // Drawn where the warehouse can stand: the sim's answer, or the coast rule until it comes.
        if (const auto fp = opening_->ui->found_preview(opening_feed());
            fp && (fp->sim_answer ? *fp->sim_answer == sim::Reject::None : fp->coast_ok)) {
            if (auto p = make_type_preview(*gen, fp->type, fp->rotation, fp->origin, group_,
                                           time_ms())) {
                overlay.insert(overlay.end(), p->sprites.begin(), p->sprites.end());
            }
        }
    }
    // The ships stir the simulated waves (enhanced water): a bow wave and a wake.
    g.frame->set_wakes(opening_ != nullptr ? motion_.wakes(wall_seconds(), wave_centre_)
                                           : std::vector<render::fluid::ShipWake>{});
    // ADR 0004 R1, the GPU path: the same HUD/construction-menu overlay draw_cpu composes into
    // its own buffer, composed here instead into a transparent-background RGBA image
    // (ui_mock.hpp's draw_live_screen_rgba) and alpha-blended over the whole frame by the
    // compositor's own "ui overlay" pass (gpu.cpp/gpu.wgsl) - drawn only at the same exact
    // 1024 x 768 the CPU path requires (R3 still open).
    std::optional<assets::Rgba> ui_overlay_image;
    if (construction_menu_ != nullptr && w == 1024 && h == 768) {
        const auto in = construction_hud_inputs();
        auto drawn = draw_live_screen_rgba(
            static_cast<int>(w), static_cast<int>(h), construction_menu_->hud(),
            construction_menu_->construction(), in.spec, *construction_tex_cache_,
            in.snapshot ? in.snapshot->eligibility.get() : nullptr, in.counters,
            construction_placement_open_);
        if (drawn.ok()) {
            ui_overlay_image = std::move(drawn).value();
        }
    }
    const auto s =
        g.frame->render(frames_, gen, view, group_, time_ms(), water_, target.get(), resample,
                        overlay, live_units_, ui_overlay_image ? &*ui_overlay_image : nullptr);
    fluid_windows_ = s.fluid_windows;
    fluid_resamples_ = s.fluid_resamples;
    if (s.wave_simulation) {
        wave_centre_ = {s.fluid_centre.first, s.fluid_centre.second};
        const auto grown = [](std::uint64_t now, std::uint64_t seen) {
            return now >= seen ? now - seen : now; // smaller: a new compositor's count
        };
        windows_new_ += grown(s.fluid_windows, windows_seen_);
        resamples_new_ += grown(s.fluid_resamples, resamples_seen_);
        windows_seen_ = s.fluid_windows;
        resamples_seen_ = s.fluid_resamples;
    }
    if (s.chunks_fading > 0) {
        ++fading_frames_;
        fading_most_ = std::max(fading_most_, s.chunks_fading);
    }
    if (s.chunks_missing > 0) {
        ++missing_frames_;
        if (s.chunks_missing > missing_most_) {
            missing_most_ = s.chunks_missing;
            missing_most_group_ = group_;
        }
    }
    // What the pointer is over from now on: this frame's map, zoom level and scene.
    pointer_frame_ = pointer_frame(view, resample, group_, levels().tile_w(group_),
                                   levels().tile_h(group_), gen, out_w, out_h);
    const auto p = Clock::now();
    for (const auto& step : s.steps()) {
        steps_.push_back({step.name, step.ms});
    }
    // Locals of render() destroyed on the way out (command encoder, bind groups).
    steps_.push_back({"render exit", std::max(0.0, since_ms(render_start, p) - s.total_ms)});
    const auto present_start = Clock::now();
    WGPUStatus presented = wgpuSurfacePresent(g.surface.get());
    if (presents_failing_) {
        // Test seam (hold presents): the consumers see a failed present, the one a failing surface
        // gives (the frame itself was presented).
        presented = WGPUStatus_Error;
    }
    if (presented != WGPUStatus_Success && !present_error_shown_) {
        std::fprintf(stderr, "presenting the frame failed\n");
        present_error_shown_ = true;
    }
    if (presented != WGPUStatus_Success && play_ != nullptr && play_->visibility().busy()) {
        // A change waits for a presented frame (ChangeVisibility): draw again shortly, so an idle
        // window does not stay NotPresented until some input arrives, and a surface that keeps
        // failing is retried at the usual back-off, not at the loop's rate.
        back_off();
    }
    steps_end_ = Clock::now();
    const double present = since_ms(p, steps_end_);
    steps_.push_back({"present", present});
    // After the present call, so the trace adds nothing between submission and present; the
    // events of the submission carry its time. CPU-ready: cached and not stale (bench_picture's
    // boundary); GPU-usable: also the compositor's copy is that chunk's, whole, and not
    // dissolving. The chunk state changes only inside render, so after the present call it is
    // the render's. The window's own readiness (ChangeVisibility) uses the same two predicates.
    auto fresh = [this](const render::ChunkKey& k) {
        return frames_.chunk(k) != nullptr && frames_.stale_generation(k) == 0;
    };
    auto usable = [this, &fresh](const render::ChunkKey& k) {
        if (!fresh(k)) {
            return false;
        }
        const auto cs = gpu_->frame->chunk_state(k);
        return cs.serial == frames_.chunk(k)->serial && !cs.fading;
    };
    if (play_ != nullptr) {
        play_->visibility().frame_done(presented == WGPUStatus_Success, fresh, usable);
        if (presented == WGPUStatus_Success && opening_ != nullptr) {
            shown_tick_ = frame_view_tick_;
        }
        refresh_visibility_title();
    }
    if (tracer_) {
        const bool picture_idle = play_ != nullptr && play_->picture().pending() == 0 &&
                                  !play_->runner().batches_waiting();
        tracer_->presented(trace_->ms_at(p), trace_->ms_at(present_start),
                           trace_->ms_at(steps_end_), static_cast<std::uint32_t>(presented), fresh,
                           usable, picture_idle);
    }
    chunks_received_ += s.chunks_built; // reported with the periodic line, not per frame
    tex_w_ = out_w;
    tex_h_ = out_h;
    measure(s.chunks_ms, {s.upload_ms, s.sprites_ms, s.encode_ms, present});
}
#endif

Viewer::ConstructionHudInputs Viewer::construction_hud_inputs() const {
    const UiMockSpec spec = construction_menu_open_ ? UiMockSpec{UiMockScreen::Construction,
                                                                 construction_open_category_}
                                                    : UiMockSpec{UiMockScreen::Hud, ""};
    const auto snapshot = play_ != nullptr ? play_->runner().snapshot() : nullptr;
    // H2: the live numbers (goods_tools_ etc. resolved once, constructor; this file's own member
    // comment). Goods are 8.8 fixed-point tons (PlaySnapshot::goods's own doc comment) - sim::kTon
    // (production.hpp) is the 256 that separates the whole-ton bits from the fractional ones; a
    // display counter only ever needs the whole tons. eligibility is null exactly when there is
    // no city yet (its own doc comment says so, "matching money/goods above") - money/goods
    // themselves are plain fields (0, not nullopt) in that case, so eligibility's nullness is the
    // real "has a city" signal.
    const bool has_city = snapshot != nullptr && snapshot->eligibility != nullptr;
    auto whole_tons = [&](std::optional<sim::GoodsId> id) -> std::optional<std::uint32_t> {
        if (!has_city || !id || *id >= snapshot->goods.size())
            return std::nullopt;
        return snapshot->goods[*id] / sim::kTon;
    };
    HudCounters counters{has_city ? std::optional<std::int64_t>(snapshot->money) : std::nullopt,
                         whole_tons(goods_tools_),
                         whole_tons(goods_wood_),
                         whole_tons(goods_bricks_),
                         whole_tons(goods_marble_),
                         has_city ? snapshot->population : std::nullopt};
    return {spec, std::move(counters), snapshot};
}

void Viewer::draw_cpu(const std::shared_ptr<const render::Scene>& gen, render::Rect view) {
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
    steps_.push_back({"picture", picture_ms_});
    steps_.push_back({"draw prologue", since_ms(picture_end_, render_start)});
    if (play_ != nullptr) {
        play_->visibility().begin_frame(view, group_); // the CPU path's frame, as draw_gpu's
    }
    const auto s = frames_.render(gen, view, group_, time_ms(), water_, buffer_.data());
    if (opening_ != nullptr) {
        // The live ships after every object of the scene (the scene's own units are drawn last
        // too), then the founding cursor over them.
        blend_units(live_units_, *gen, group_, view, buffer_.data());
        if (const auto fp = opening_->ui->found_preview(opening_feed());
            fp && (fp->sim_answer ? *fp->sim_answer == sim::Reject::None : fp->coast_ok)) {
            if (const auto p = make_type_preview(*gen, fp->type, fp->rotation, fp->origin, group_,
                                                 time_ms())) {
                blend_preview(*gen, *p, group_, time_ms(), view, buffer_.data());
            }
        }
    }
    if (dev_ != nullptr) {
        if (const auto p = make_preview(*dev_, *gen, group_, time_ms())) {
            blend_preview(*gen, *p, group_, time_ms(), view, buffer_.data());
        }
    }
    double construction_ui_ms = 0.0;
    if (construction_menu_ != nullptr && w == 1024 && h == 768) {
        // H1/C1, second slice: the menu's own chrome, drawn over the world (ADR 0004 R1's
        // "Rendering" - the CPU path; the GPU path is later work), only while the window is at
        // the UI's 1024 x 768 reference (R3's scale is still open). construction_tex_cache_
        // persists decoded textures across frames (ADR 0004 "Atlases"; see its own comment).
        // Timed on its own (not folded into "render other") so the cache's effect is measurable
        // per CLAUDE.md's "measure every performance change".
        const auto ui_start = Clock::now();
        const auto in = construction_hud_inputs();
        static_cast<void>(draw_live_screen_xrgb(
            buffer_.data(), w, h, construction_menu_->hud(), construction_menu_->construction(),
            in.spec, *construction_tex_cache_,
            in.snapshot ? in.snapshot->eligibility.get() : nullptr, in.counters,
            construction_placement_open_));
        construction_ui_ms = since_ms(ui_start, Clock::now());
    }
    pointer_frame_ = pointer_frame(view, nullptr, group_, levels().tile_w(group_),
                                   levels().tile_h(group_), gen, w, h);
    const auto rendered = Clock::now();
    const double known = s.chunks_ms + s.ground_ms + s.water_objects_ms + s.sea_ms + s.objects_ms +
                         construction_ui_ms;
    steps_.push_back({"chunks", s.chunks_ms});
    steps_.push_back({"ground", s.ground_ms});
    steps_.push_back({"water objects", s.water_objects_ms});
    steps_.push_back({"sea", s.sea_ms});
    steps_.push_back({"objects", s.objects_ms});
    if (construction_menu_ != nullptr) {
        steps_.push_back({"construction ui", construction_ui_ms});
    }
    steps_.push_back({"render other", std::max(0.0, since_ms(render_start, rendered) - known)});
    // The delivery chain: texture update, render submission, present. The frame counts as delivered
    // only if every call succeeded (SDL3 returns bool); after a failed stage the later ones are not
    // attempted, so an old texture is never presented as this frame. The script seam (fail STAGE)
    // makes the window see a failure for a stage whose real call was made.
    bool delivered = true;
    const char* failed_at = nullptr;
    std::size_t failed_idx = 0;
    const auto stage = [&](std::uint8_t bit, const char* name, bool ok) {
        const std::size_t idx = bit == 1 ? 0 : bit == 2 ? 1 : 2;
        if (!ok) {
            delivered = false;
            failed_at = name;
            failed_idx = idx;
        }
    };
    // The script seam (fail STAGE) hands the stage's real SDL call an invalid object (a null
    // texture or renderer), so SDL itself reports the failure and the window reads it like any
    // other.
    const bool bad_update = (cpu_fail_ & 1) != 0, bad_submit = (cpu_fail_ & 2) != 0,
               bad_present = (cpu_fail_ & 4) != 0;
    stage(1, "SDL_UpdateTexture",
          SDL_UpdateTexture(bad_update ? nullptr : texture_, nullptr, buffer_.data(), w * 4));
    if (delivered) {
        stage(2, "SDL_RenderTexture",
              SDL_RenderTexture(renderer_, bad_submit ? nullptr : texture_, nullptr, nullptr));
    }
    const auto present_start = Clock::now();
    steps_.push_back({"texture copy", since_ms(rendered, present_start)});
    if (delivered) {
        stage(4, "SDL_RenderPresent", SDL_RenderPresent(bad_present ? nullptr : renderer_));
    }
    steps_end_ = Clock::now();
    if (!delivered) {
        // The change (if any) stays pending (frame_done below gets false); draw again shortly, at
        // the usual 100 ms back-off, and say why: the first failure and every hundredth after it.
        std::uint64_t& at_stage = cpu_stage_failures_[failed_idx];
        if (at_stage++ % 100 == 0) {
            std::fprintf(
                stderr,
                "CPU frame delivery failed (%s: %s); the picture's pending changes wait for a "
                "delivered frame, retrying (failure %llu of this stage)\n",
                failed_at, SDL_GetError(), static_cast<unsigned long long>(at_stage));
        }
        back_off();
    } else {
        cpu_stage_failures_[0] = cpu_stage_failures_[1] = cpu_stage_failures_[2] =
            0; // a new episode says why again
    }
    if (play_ != nullptr) {
        // The CPU path has no GPU copy: a fresh chunk was drawn into this frame's buffer.
        auto fresh = [this](const render::ChunkKey& k) {
            return frames_.chunk(k) != nullptr && frames_.stale_generation(k) == 0;
        };
        play_->visibility().frame_done(delivered, fresh, fresh);
        if (delivered && opening_ != nullptr) {
            shown_tick_ = frame_view_tick_;
        }
        refresh_visibility_title();
    }
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
    if (hooks_ != nullptr && hooks_->title) {
        const std::string extra = hooks_->title();
        if (!extra.empty()) {
            std::fprintf(stderr, "  play: %s\n", extra.c_str());
        }
    }
    std::fprintf(stderr, "  steps of the worst frame (%.1f ms busy): %s\n", worst_ms_,
                 describe(worst_steps_, worst_ms_).c_str());
    if (chunks_received_ > 0) {
        std::fprintf(stderr, "  chunk(s) received in this interval: %zu\n", chunks_received_);
        chunks_received_ = 0;
    }
    if (on_gpu()) {
        std::fprintf(
            stderr,
            "  not ready: %zu of %zu frames drew a visible chunk not built yet (at most %zu "
            "at once, zoom group %zu), %zu dissolved chunks in over a stand-in (at most %zu); "
            "wave windows %llu new, %llu carried over\n",
            missing_frames_, frames_n_, missing_most_, missing_most_group_, fading_frames_,
            fading_most_, static_cast<unsigned long long>(windows_new_),
            static_cast<unsigned long long>(resamples_new_));
        missing_frames_ = missing_most_ = missing_most_group_ = 0;
        fading_frames_ = fading_most_ = 0;
        windows_new_ = resamples_new_ = 0;
    }
    if (play_ != nullptr) {
        const std::string failed = play_->picture().error();
        std::fprintf(stderr,
                     "  picture: generation %llu, %zu island(s) committed in this interval, "
                     "%zu pending%s%s\n",
                     static_cast<unsigned long long>(play_->picture().scene()->generation()),
                     committed_, play_->picture().pending(),
                     failed.empty() ? "" : "; failed: ", failed.c_str());
        committed_ = 0;
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

std::string run_viewer(std::shared_ptr<const render::Scene> scene, std::size_t group,
                       Renderer renderer, render::RiverFlowMode river_flow,
                       render::WaterStyle water, std::size_t lookahead_chunks, ZoomOptions zoom,
                       const ViewerHooks* hooks, PlayFollower* play) {
    if (!scene || !scene->bounds(group)) {
        return "no islands";
    }
    const std::size_t threads =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    Viewer v(std::move(scene), group, threads, renderer, river_flow, water, lookahead_chunks, zoom,
             hooks, play);
    return v.run();
}

} // namespace anno::app
