// GPU host of the coastal wave simulation (fluid.wgsl): textures, compute
// pipelines, the camera-local ring-buffer window and time stepping. Port of
// crates/render/src/gpu_fluid.rs. Model, parameters and the CPU reference:
// fluid.hpp; spec: docs/render/iso.md, "Wave simulation".
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/render/fluid.hpp>
#include <anno/render/wgpu.hpp>

namespace anno::render::gpu {

class FluidGpu {
  public:
    ~FluidGpu();
    FluidGpu(FluidGpu&&) noexcept;
    FluidGpu& operator=(FluidGpu&&) noexcept;

    /// Pipelines and buffers for `cfg` on the device of `c` (the simulation
    /// keeps its own references to it).
    static formats::Result<FluidGpu, std::string> create(const Context& c, fluid::FluidConfig cfg);
    static formats::Result<FluidGpu, std::string> create(const DeviceRef& d,
                                                         fluid::FluidConfig cfg);

    /// The current state (η, u, v, foam) (ring-buffer storage, RGBA32Float).
    [[nodiscard]] WGPUTextureView state_view() const { return grid_.views[0].get(); }
    /// Still-water depth per cell (ring-buffer storage, R32Float).
    [[nodiscard]] WGPUTextureView depth_view() const { return grid_.bathy_view.get(); }
    /// The current state texture (for read-back in tests).
    [[nodiscard]] WGPUTexture state_texture() const { return grid_.states[0].get(); }
    /// The window, once update() has run.
    [[nodiscard]] std::optional<fluid::FluidWindow> window() const { return window_; }
    /// Counts the windows the simulation has switched to (the first one, a new zoom group
    /// or size; not a recentring of the same group): the compositor starts the cross-fade
    /// from the analytic waves when it changes (GpuLimits::wave_fade_ms).
    [[nodiscard]] std::uint64_t window_serial() const { return window_serial_; }
    /// Simulation time in seconds.
    [[nodiscard]] double sim_time() const { return sim_t_; }
    [[nodiscard]] const fluid::FluidConfig& config() const { return cfg_; }

    /// Background setup: update() no longer fills a new or moved window in
    /// the frame. The cells' depth and initial state are computed on a
    /// background thread and uploaded over several frames (at most
    /// upload_bytes_per_frame each); meanwhile the simulation keeps running
    /// on the previous window (window() is empty before the first one is
    /// ready, and renderers then fall back to the analytic waves). A window
    /// for another zoom group or size replaces the one being prepared, so
    /// only the last request is ever finished. Without it (the default)
    /// update() blocks until the window is filled, which tests and headless
    /// renders rely on.
    void set_background(bool on);
    [[nodiscard]] bool background() const { return background_; }
    /// Whether a window or strip is being prepared in the background.
    [[nodiscard]] bool setting_up() const { return job_ != nullptr; }
    /// Limits for uploading a new window's data in the background, per frame:
    /// at most this many bytes (a 1024² window is 20 MiB) and, once this many
    /// milliseconds are spent, no further slice (0 = no time limit); at
    /// least one slice always goes.
    std::size_t upload_bytes_per_frame = std::size_t{16} << 20;
    double upload_budget_ms = 2.0;

    /// Brings the simulation to `time_ms` for a view at zoom `group` centred
    /// on tile position `centre` and spanning `span_tiles` tiles (sizes the
    /// window, FluidConfig::window_size): recentres the window (new cells get
    /// depth `depth(x, y)` and the incoming swell, computed on `threads` CPU
    /// threads) and records the time steps into `encoder`. `depth` is called
    /// from several threads at once.
    void update(WGPUCommandEncoder encoder, std::size_t group, std::pair<float, float> centre,
                float span_tiles, std::uint64_t time_ms, const fluid::DepthFn& depth,
                std::size_t threads);

  private:
    /// Textures of a grid of n × n cells and their bind groups; recreated
    /// when the window size changes (viewport resize).
    struct Grid {
        std::uint32_t n = 0;
        /// State (η, u, v, foam): texture 0 holds the current state after
        /// every step; texture 1 is the intermediate of pass A.
        std::array<Texture, 2> states;
        std::array<TextureView, 2> views;
        Texture bathy;
        TextureView bathy_view;
        /// Pass A reads 0 writes 1; pass B reads 1 writes 0.
        BindGroup bind_a, bind_b;
    };

    struct Job;

    FluidGpu();
    [[nodiscard]] Grid make_grid(std::uint32_t n) const;
    /// Fills the cells of `r` of the active grid now (blocking).
    void fill(const fluid::FluidWindow& window, fluid::CellRect r, const fluid::DepthFn& depth,
              float t, std::size_t threads) const;
    /// Background mode of update(): retires a stale job, uploads a finished
    /// one, starts the next when the window has to change.
    void prepare_window(std::size_t group, std::pair<float, float> centre, std::uint32_t size,
                        double target, const fluid::DepthFn& depth, std::size_t threads);
    /// Uploads (part of) a finished job; true when the window was switched.
    bool apply_job(Job& j);
    /// Cancels the job in flight; its thread is joined later (retired_).
    void retire_job();

    std::unique_ptr<DeviceRef> ref_;
    fluid::FluidConfig cfg_;
    fluid::Swell comps_{};
    std::optional<fluid::FluidWindow> window_;
    std::uint64_t window_serial_ = 0;
    std::size_t group_ = SIZE_MAX;
    BindGroupLayout layout_;
    Buffer params_, steps_;
    std::uint64_t align_ = 0;
    ComputePipeline vel_pipe_, eta_pipe_;
    Grid grid_;
    /// The grid replaced by the last window switch, kept for the next one of
    /// the same size (zoom changes mostly keep it: a window covers about the
    /// same number of cells at every group), which saves creating the
    /// textures — tens of megabytes — on the window thread.
    std::optional<Grid> spare_;
    double sim_t_ = 0.0;
    bool background_ = false;
    std::unique_ptr<Job> job_;
    /// Cancelled jobs whose threads may still be running.
    std::vector<std::unique_ptr<Job>> retired_;
};

} // namespace anno::render::gpu
