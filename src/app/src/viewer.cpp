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

namespace anno::app {

namespace {

using Clock = std::chrono::steady_clock;
/// Interval of the FPS display and the stderr breakdown.
constexpr auto kReport = std::chrono::seconds(2);

class Viewer {
  public:
    Viewer(const render::Scene& scene, std::size_t group, std::size_t threads)
        : scene_(scene), group_(group), frames_(threads) {
        const auto b = *scene.bounds(group);
        cam_x_ = b.x + b.w / 2.0;
        cam_y_ = b.y + b.h / 2.0;
    }

    std::string run();

  private:
    void handle(const SDL_Event& e);
    void draw();
    void update_title();
    void pan(double dx, double dy);
    void clamp();
    void zoom(int step, std::optional<std::pair<double, double>> anchor);
    void toggle_pause();
    [[nodiscard]] std::uint64_t time_ms() const;
    [[nodiscard]] std::pair<int, int> view_size() const;
    void measure(const render::FrameStats& s);

    const render::Scene& scene_;
    std::size_t group_;
    /// Scene screen position (at group_) shown in the window centre.
    double cam_x_ = 0, cam_y_ = 0;
    render::FrameRenderer frames_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int tex_w_ = 0, tex_h_ = 0;
    std::vector<std::uint32_t> buffer_;
    Clock::time_point clock_ = Clock::now();
    std::optional<Clock::duration> paused_at_;
    render::WaterStyle water_ = render::WaterStyle::Original;
    bool running_ = true, dirty_ = true, drag_ = false;
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
    window_ =
        SDL_CreateWindow("anno1503", static_cast<int>(1280 * scale), static_cast<int>(800 * scale),
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        const std::string err = std::string("SDL_CreateWindow: ") + SDL_GetError();
        SDL_Quit();
        return err;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        const std::string err = std::string("SDL_CreateRenderer: ") + SDL_GetError();
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return err;
    }
    // Vsync paces the frames (SDL falls back to a simulated vsync where the
    // backend has none, and while the window is hidden).
    if (!SDL_SetRenderVSync(renderer_, 1)) {
        std::fprintf(stderr, "SDL_SetRenderVSync: %s\n", SDL_GetError());
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
        const bool animating = (!paused_at_ || frames_.busy()) && !hidden_;
        if (!animating && !dirty_) {
            if (SDL_WaitEventTimeout(&e, 100)) {
                handle(e);
            }
        }
        while (running_ && SDL_PollEvent(&e)) {
            handle(e);
        }
        if (running_ && (animating || dirty_)) {
            dirty_ = false;
            draw();
        }
    }
    frames_.set_workers(nullptr);
    for (auto& t : seas) {
        t.join();
    }
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
    SDL_DestroyRenderer(renderer_);
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
    title += " — CPU";
    if (fps_) {
        char buf[64];
        std::snprintf(buf, sizeof buf, " — %.0f fps (%.1f ms/frame)", fps_->first, fps_->second);
        title += buf;
    }
    SDL_SetWindowTitle(window_, title.c_str());
}

void Viewer::draw() {
    const auto [w, h] = view_size();
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
    // Scene screen position of the window's top-left pixel.
    const render::Rect view{static_cast<int>(std::lround(cam_x_ - w / 2.0)),
                            static_cast<int>(std::lround(cam_y_ - h / 2.0)), w, h};
    const auto s = frames_.render(scene_, view, group_, time_ms(), water_, buffer_.data());
    SDL_UpdateTexture(texture_, nullptr, buffer_.data(), w * 4);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    if (s.chunks_built > 0) {
        std::fprintf(stderr, "zoom group %zu: %zu chunk(s) received\n", group_, s.chunks_built);
    }
    measure(s);
}

void Viewer::measure(const render::FrameStats& s) {
    ++frames_n_;
    worst_ms_ = std::max(worst_ms_, s.total_ms);
    total_ms_ += s.total_ms;
    chunks_ms_ += s.chunks_ms;
    const std::array<double, 4> v = {s.ground_ms, s.water_objects_ms, s.sea_ms, s.objects_ms};
    for (std::size_t i = 0; i < 4; ++i) {
        stages_[i] += v[i];
    }
    const auto elapsed = Clock::now() - meter_since_;
    if (elapsed < kReport) {
        return;
    }
    const double n = static_cast<double>(frames_n_);
    const double fps = n / std::chrono::duration<double>(elapsed).count();
    std::fprintf(stderr,
                 "zoom group %zu, %dx%d px: %.1f fps, frame avg %.1f ms / worst %.1f ms (chunks "
                 "%.1f, ground %.1f, water objects %.1f, sea %.1f, objects %.1f; %zu chunks "
                 "cached)\n",
                 group_, tex_w_, tex_h_, fps, total_ms_ / n, worst_ms_, chunks_ms_ / n,
                 stages_[0] / n, stages_[1] / n, stages_[2] / n, stages_[3] / n, frames_.cached());
    fps_ = std::pair{fps, total_ms_ / n};
    frames_n_ = 0;
    total_ms_ = worst_ms_ = chunks_ms_ = 0;
    stages_ = {};
    meter_since_ = Clock::now();
    update_title();
}

} // namespace

std::string run_viewer(const render::Scene& scene, std::size_t group) {
    if (!scene.bounds(group)) {
        return "no islands";
    }
    const std::size_t threads =
        (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4U);
    Viewer v(scene, group, threads);
    return v.run();
}

} // namespace anno::app
