// GPU frame compositor (wgpu-native): composes the same frame as
// FrameRenderer::render on the graphics card. Port of crates/render/src/gpu.rs.
//
// The static data stays on the CPU side and is shared: chunks (ground,
// objects, sea field) are built by FrameRenderer (in the frame or by its
// background workers) and uploaded once as textures; the sea pattern of a
// zoom group is uploaded as two texture arrays (one layer per animation
// step). Per frame only the animated parts run, in three render passes:
// 0. the terrain height seen at each pixel (river depth test);
// 1. into an offscreen texture: cached ground, water objects below the
//    waterline (sprites from an atlas);
// 2. into the target: the sea per pixel (sea_fs in gpu.wgsl, the rules of
//    sea::shade_sea, reading pass 1), water objects above the waterline,
//    cached objects.
// Positions are integer scene pixels and textures are read with
// textureLoad, so the result matches the CPU path up to blending rounding.
// Shader and rules: src/render/shaders/gpu.wgsl; spec: docs/render/iso.md.
//
// Enhanced water runs the coastal wave simulation (gpu_fluid.hpp) in a
// compute pass before the render passes, as gpu.rs does.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/wgpu.hpp>

namespace anno::render::gpu {

/// Time spent per stage of one GPU frame on the CPU, in milliseconds.
struct GpuStats {
    std::size_t chunks_built = 0;
    double chunks_ms = 0;
    std::size_t chunks_missing = 0;
    /// Chunks, sea pattern and atlas uploads (sea_ms + chunk_sync_ms + the
    /// ensure_under check).
    double upload_ms = 0;
    /// Of upload_ms: making the sea pattern of the group usable on the GPU.
    double sea_ms = 0;
    /// Of upload_ms: uploading chunks and dropping GPU chunks.
    double chunk_sync_ms = 0;
    /// Of chunk_sync_ms: creating textures, writing texels.
    double upload_create_ms = 0, upload_write_ms = 0;
    /// Chunks uploaded to the GPU this frame (real ones).
    std::size_t chunks_uploaded = 0;
    /// Of sprites_ms: decoding sprite frames not seen before (window thread).
    double decode_ms = 0;
    /// Of sprites_ms: packing frames into the atlas (writes, growth).
    double atlas_ms = 0;
    /// Sprite frames decoded or packed for the first time this frame.
    std::size_t sprites_new = 0;
    /// Of encode_ms: bringing the wave simulation up to date (window
    /// (re)initialisation, depth sampling, uploads, step recording).
    double fluid_ms = 0;
    /// Placing and packing the animated sprites.
    double sprites_ms = 0;
    /// Recording and submitting the commands.
    double encode_ms = 0;
    double total_ms = 0;
    /// Animated sprites drawn (water objects below and above the waterline).
    std::size_t sprites = 0;
    /// Ships drawn by the compositor (FrameRenderer without ships in its
    /// chunks; their shadows included).
    std::size_t ships = 0;
    /// The coastal wave simulation ran for this frame (enhanced water).
    bool wave_simulation = false;
};

/// Tuning of the compositor (Rust constants; data, PLAN §0.1).
struct GpuLimits {
    /// Edge length of an atlas layer for sprite frames.
    std::uint32_t atlas_size = 2048;
    /// Chunks uploaded per frame at most; the rest show open sea until a
    /// later frame, so zooming never stalls on uploads.
    std::size_t uploads_per_frame = 6;
    /// Time budget in milliseconds for the chunk uploads of one frame: once
    /// it is spent, further chunks wait for the next frame (the first one
    /// always goes). 0 = no time limit, only uploads_per_frame applies.
    double upload_budget_ms = 0.0;
    /// The coastal wave simulation sets up its window (depth sampling, the
    /// initial state) on a background thread and uploads it over several
    /// frames, so a zoom group change or a window jump never stalls the
    /// window thread; until the new window is ready the previous one (or
    /// the shader's analytic waves outside it) is shown. false: set up in
    /// the frame (headless renders, tests).
    bool background_fluid = false;
    /// The limits for an interactive window: uploads and wave simulation
    /// setup are spread over frames (upload_budget_ms 1.0, background_fluid).
    [[nodiscard]] static GpuLimits interactive() {
        GpuLimits l;
        l.upload_budget_ms = 1.0;
        l.background_fluid = true;
        return l;
    }
    /// Animation steps of other zoom groups' sea patterns uploaded per frame.
    std::size_t sea_steps_per_frame = 2;
    /// Atlas layers at most (0 = the device's maxTextureArrayLayers; larger
    /// values are clamped to it). When full, new sprite frames are dropped.
    std::uint32_t max_atlas_layers = 0;
    /// Enhanced water: ships sink into the water with a moving waterline
    /// and foam (not in the original; gpu.wgsl ship_in_water).
    bool ship_waterline = true;
    /// Enhanced water: objects standing in water (rocks, cliffs, piers...)
    /// sink in at their base, with foam at the sea (gpu.wgsl
    /// object_in_water; needs chunks with Chunk::base).
    bool object_waterline = true;
    /// Run the coastal wave simulation for enhanced water (false: the
    /// shader's analytic waves, as the Rust compositor without it).
    bool wave_simulation = true;
};

/// Values of the frame uniform (struct Frame in gpu.wgsl).
struct FrameUniform {
    Rect view;
    std::pair<std::int32_t, std::int32_t> pattern_size{1, 1};
    std::uint32_t step0 = 0, step1 = 0;
    float frac = 0, time_ms = 0;
    bool enhanced = false;
    float tile_w = 0, tile_h = 0, raise = 0;
    /// Ships sit in the enhanced water (GpuLimits::ship_waterline).
    bool ship_water = false;
    /// Water meets objects standing in it (GpuLimits::object_waterline).
    bool objects_water = false;
    std::pair<std::int32_t, std::int32_t> fluid_origin{0, 0};
    std::int32_t fluid_size = 1;
    bool fluid_on = false;
    float fluid_cpt = 1;
};
/// Bytes of the frame uniform (96, little-endian, WGSL uniform layout).
std::vector<std::uint8_t> frame_uniform_bytes(const FrameUniform& f);

class GpuFrame {
  public:
    /// Pipelines for targets of `format` (a non-sRGB format: colours are
    /// written as stored; sRGB formats are refused). The frame keeps its own
    /// references to the device of `c`. Limits are checked against the
    /// device (atlas size ≤ maxTextureDimension2D, ≥ 1 upload per frame).
    /// A frame caches the
    /// GPU copies of one scene (sprite atlas, sea patterns, chunks): use a
    /// new one for another scene.
    static formats::Result<std::unique_ptr<GpuFrame>, std::string>
    create(const Context& c, WGPUTextureFormat format, GpuLimits limits = {});
    ~GpuFrame();
    GpuFrame(const GpuFrame&) = delete;
    GpuFrame& operator=(const GpuFrame&) = delete;

    /// Composes `view` at `group` and `time_ms` into `target` (view.w ×
    /// view.h, in the frame's format) and submits the commands.
    GpuStats render(FrameRenderer& frames, const Scene& scene, Rect view, std::size_t group,
                    std::uint64_t time_ms, WaterStyle style, WGPUTextureView target);

    /// Whether `group` can be shown without the window thread waiting for
    /// content: its sea pattern is rendered and fully on the GPU (render()
    /// uploads the patterns of the other groups a few steps per frame once
    /// they are rendered). Interactive callers delay a zoom change until it
    /// is true.
    [[nodiscard]] bool group_ready(const Scene& scene, std::size_t group) const;
    /// Waits until the GPU has finished all submitted work.
    void wait() const;
    /// An offscreen target of `w × h` in the frame's format that can be
    /// read back.
    std::pair<Texture, TextureView> offscreen_target(std::uint32_t w, std::uint32_t h) const;
    /// The bytes of `texture` (w × h × bpp, rows packed), waiting for the
    /// GPU; empty if the read-back failed (an invalid copy is counted in
    /// errors(), not submitted).
    std::vector<std::uint8_t> read_back(WGPUTexture texture, std::uint32_t w, std::uint32_t h,
                                        std::uint32_t bpp = 4) const;
    [[nodiscard]] WGPUTextureFormat format() const;
    /// Errors of the device so far (Context::errors): uncaptured ones and
    /// command buffers dropped because recording them failed (render()
    /// then draws nothing).
    [[nodiscard]] std::uint64_t errors() const;

  private:
    GpuFrame() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace anno::render::gpu
