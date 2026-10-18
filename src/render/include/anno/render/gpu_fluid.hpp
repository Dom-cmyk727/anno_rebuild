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

#include <anno/formats/result.hpp>
#include <anno/render/fluid.hpp>
#include <anno/render/wgpu.hpp>

namespace anno::render::gpu {

class FluidGpu {
  public:
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
    /// Simulation time in seconds.
    [[nodiscard]] double sim_time() const { return sim_t_; }
    [[nodiscard]] const fluid::FluidConfig& config() const { return cfg_; }

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

    FluidGpu() = default;
    [[nodiscard]] Grid make_grid(std::uint32_t n) const;
    void fill(const fluid::FluidWindow& window, fluid::CellRect r, const fluid::DepthFn& depth,
              float t, std::size_t threads) const;

    std::unique_ptr<DeviceRef> ref_;
    fluid::FluidConfig cfg_;
    fluid::Swell comps_{};
    std::optional<fluid::FluidWindow> window_;
    std::size_t group_ = SIZE_MAX;
    BindGroupLayout layout_;
    Buffer params_, steps_;
    std::uint64_t align_ = 0;
    ComputePipeline vel_pipe_, eta_pipe_;
    Grid grid_;
    double sim_t_ = 0.0;
};

} // namespace anno::render::gpu
