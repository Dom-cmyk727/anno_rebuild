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

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
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
    /// The window-thread steps of render(), laid end to end: steps() lists them in
    /// order and their sum is total_ms (the time between two steps is
    /// charged to the step before it), so a hitch that shows in total_ms is
    /// always in one of them. The coarse fields above are sums of these.
    /// ensure_under (the offscreen sea targets) and visible_chunks.
    double under_ms = 0, visible_ms = 0;
    /// Of chunk_sync_ms: building the key set and deciding what stays, the GPU
    /// chunks dropped (their planes go to the plane pool, which keeps a bounded
    /// number; the rest is freed on this thread: chunks_dropped,
    /// planes_freed), the real uploads (textures, texel writes, bind group;
    /// chunks_uploaded) and the placeholder chunks made for chunks that are not
    /// built or not uploaded yet (a uniform buffer and a bind group each;
    /// placeholders).
    double sync_scan_ms = 0, sync_drop_ms = 0, sync_real_ms = 0, sync_placeholder_ms = 0;
    std::size_t chunks_dropped = 0, planes_freed = 0, placeholders = 0;
    /// Planes of the real uploads that came from the background pre-creation
    /// (GpuLimits::prewarm_chunks) instead of being created in the frame.
    std::size_t planes_warm = 0;
    /// Of encode_ms: creating the command encoder; the frame and river uniforms
    /// with the two frame bind groups; choosing the chunk draws (stand_in_ms of
    /// it: the stand-in cover and the piece records); writing the piece buffer
    /// (pieces_realloc: it grew, a new buffer and bind group were made);
    /// recording the three passes; finishing the command buffer, popping the
    /// validation error scope (it polls the device), wgpuQueueSubmit, and
    /// releasing the command buffer.
    double encoder_ms = 0, uniforms_ms = 0, draws_ms = 0, stand_in_ms = 0, pieces_ms = 0;
    bool pieces_realloc = false;
    double record_ms = 0, finish_ms = 0, scope_ms = 0, queue_ms = 0, release_ms = 0;
    /// A step of render(): a name and its time on the window thread.
    struct Step {
        const char* name;
        double ms;
    };
    /// The steps in the order they ran. Their sum is total_ms up to the clock
    /// reads between two steps (below a microsecond).
    [[nodiscard]] std::vector<Step> steps() const {
        // chunk_sync_ms is not in the list: its parts are.
        return {{"chunks", chunks_ms},
                {"under", under_ms},
                {"sea", sea_ms},
                {"visible", visible_ms},
                {"sync scan", sync_scan_ms},
                {"sync drops", sync_drop_ms},
                {"sync uploads", sync_real_ms},
                {"sync placeholders", sync_placeholder_ms},
                {"sprites", sprites_ms},
                {"encoder", encoder_ms},
                {"fluid", fluid_ms},
                {"uniforms", uniforms_ms},
                {"draws", draws_ms},
                {"pieces", pieces_ms},
                {"record", record_ms},
                {"finish", finish_ms},
                {"error scope", scope_ms},
                {"queue submit", queue_ms},
                {"cmd release", release_ms}};
    }
    /// Animated sprites drawn (water objects below and above the waterline).
    std::size_t sprites = 0;
    /// Ships drawn by the compositor (FrameRenderer without ships in its
    /// chunks; their shadows included).
    std::size_t ships = 0;
    /// The coastal wave simulation ran for this frame (enhanced water).
    bool wave_simulation = false;
    /// Visible chunks drawn from another zoom group's chunks because their own
    /// are not built or not uploaded yet (GpuLimits::stand_ins), and the parts
    /// (draws) they took.
    std::size_t stand_in_chunks = 0, stand_in_parts = 0;
    /// Visible chunks that show open sea because they are not on the GPU and no
    /// zoom group has a chunk to stand in for them (or stand-ins are off): the
    /// pop-in a pan during a zoom can show.
    std::size_t open_sea_chunks = 0;
    /// Of open_sea_chunks, the ones the CPU has built (waiting for their upload).
    std::size_t open_sea_built = 0;
    /// Chunks of the neighbouring zoom groups uploaded ahead (GpuLimits::prefetch_chunks).
    std::size_t chunks_prefetched = 0;
    /// Chunks drawn fading in (GpuLimits::chunk_fade_ms).
    std::size_t chunks_fading = 0;
    /// A wave-window job (new group, size or strips) is being prepared in the
    /// background: the shader shows the previous window or, where it does not
    /// reach, the analytic waves (OQ-076).
    bool fluid_setting_up = false;
    /// Enhanced rivers: the visible chunks that carry a flow lattice on the GPU,
    /// and the cost tier the river shading ran at this frame.
    std::size_t river_chunks = 0;
    RiverQuality river_quality = RiverQuality::High;
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
    /// Chunks' worth of texture planes (five 512 x 512 textures, about 4 MB
    /// per chunk) that a background thread keeps ready: it creates them right
    /// after the compositor is made and makes up for the ones uploads take.
    /// Creating textures costs the window thread 1-18 ms for 2-3 chunks (the
    /// driver allocates device memory), which a zoom step into a group with
    /// nothing uploaded yet paid for. The default covers the visible chunks of
    /// a 2560 x 1369 view at one group (24); a larger view or more groups held
    /// at once take more. Memory use is this times 4 MB, on top of the chunks
    /// uploaded; 0 = none (headless renders, tests).
    std::size_t prewarm_chunks = 0;
    /// Device memory in MiB that idle chunk planes may hold at most: the pooled
    /// planes of dropped chunks (bounded by their own count) and the pre-created
    /// ones together; the pre-creation fills what the pool leaves. The default
    /// is about 64 chunks' worth and covers prewarm_chunks (24 chunks, about
    /// 94 MB) plus a pool of a view's chunks; raise it with prewarm_chunks.
    /// Uploaded chunks are not counted (they are the content, bounded by the CPU
    /// cache). 0 = no idle planes are pre-created.
    std::size_t idle_plane_budget_mb = 256;
    /// Chunks of the neighbouring zoom groups (the view a zoom step would show,
    /// FrameRenderer::adjacent_keys) that may be on the GPU at once, uploaded
    /// ahead when no visible chunk waits, prefetch_per_frame per frame. A pan
    /// during a zoom then lands on real chunks instead of open sea. About 4 MB
    /// of device memory each: the default 96 covers the two neighbours of a
    /// 2560 x 1440 view with a ring (2 x 7 x 5 = 70) and some more; 0 = off
    /// (headless renders, tests).
    std::size_t prefetch_chunks = 0;
    std::size_t prefetch_per_frame = 1;
    /// A chunk that replaces its stand-in or the open sea (it was uploaded while
    /// it was in view) is drawn over it with rising opacity for this many
    /// milliseconds of the animation clock (render()'s time_ms), so a swap is a
    /// short dissolve, not a pop. About the time an eye needs to follow a change
    /// without it reading as a jump; 0 = swap at once (headless renders, tests).
    std::uint64_t chunk_fade_ms = 0;
    /// The limits for an interactive window: uploads and wave simulation
    /// setup are spread over frames (upload_budget_ms 1.0, background_fluid).
    [[nodiscard]] static GpuLimits interactive() {
        GpuLimits l;
        l.upload_budget_ms = 1.0;
        l.background_fluid = true;
        l.prewarm_chunks = 24;
        l.prefetch_chunks = 96;
        l.chunk_fade_ms = 150;
        return l;
    }
    /// A visible chunk that is not on the GPU yet (still being built, or waiting
    /// for its upload) shows the nearest zoom group's chunks that are, scaled by
    /// the compositor's shader to the tile size shown (frame.hpp, "Stand-in
    /// chunks"; open-questions OQ-077), instead of open sea. false: open sea.
    bool stand_ins = true;
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
    /// The cost tier of the river flow shading (docs/render/rivers.md 4 and 16).
    /// nullopt = automatic: High on a discrete adapter; on any other adapter the
    /// highest tier whose measured added cost over the legacy shimmer
    /// (GpuFrame::measure_river_cost, run once in GpuFrame::create) is at most
    /// river_gate_ms_per_mpx, else Low; a
    /// software adapter gets Low unmeasured.
    std::optional<RiverQuality> river_quality;
    /// The gate of the automatic tier: milliseconds of GPU time the flow shading
    /// adds to the legacy shimmer per million river pixels. 1.0 keeps the river
    /// pass of a view with a million river pixels (a third of a 4K screen; real
    /// views have a few percent) within half the 2 ms frame of a 500 Hz monitor.
    double river_gate_ms_per_mpx = 1.0;
    /// Wall-clock budget (milliseconds) of the automatic tier's measurement in
    /// GpuFrame::create, which blocks the calling thread once at startup. When it runs
    /// out the tiers not finished are unmeasured and the choice is Low (the floor), so
    /// a slow adapter never waits more than about this plus one pass. 0 = unlimited.
    double river_measure_budget_ms = 500.0;
    /// Time the sea pass with GPU timestamps (GpuFrame::sea_pass_ms) when the
    /// device supports them. Costs two query writes and a 16-byte copy per
    /// frame; for benchmarks.
    bool gpu_timing = false;
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
    /// Enhanced rivers are shaded by the flow lattice (else the legacy shimmer),
    /// and the pixels per HEIGHT unit of the lift (RiverView::lambda).
    bool river_flow = false;
    float river_lambda = 0;
};
/// Bytes of the frame uniform (96, little-endian, WGSL uniform layout).
std::vector<std::uint8_t> frame_uniform_bytes(const FrameUniform& f);

/// The binding contract of the river flow with gpu.wgsl (a test reads the
/// shader's declarations and compares): chunk flag bit 5 = the chunk has a
/// lattice, bits 8-15 its stride in pixels; group 1 bindings 6 (velocity,
/// RG16Sint), 7 (valid / turbulence / wake / level, RGBA8Uint) and 8 (shear,
/// R8Uint); group 0 binding 7 = the river parameters.
inline constexpr std::uint32_t kChunkFlagFlow = 1U << 5;
/// The stride field is 8 bits wide: river::kMaxLatticeStride (255) is its limit, enforced
/// by RiverConfig::validate; widening it means moving to the free bits 16-31.
inline constexpr std::uint32_t kChunkStrideShift = 8;
inline constexpr std::uint32_t kFlowVelocityBinding = 6, kFlowAuxBinding = 7, kFlowShearBinding = 8;
inline constexpr std::uint32_t kRiverParamsBinding = 7;
/// Bytes of the river parameter uniform (struct RiverParams in gpu.wgsl).
inline constexpr std::size_t kRiverParamsBytes = 32 + 32 * kMaxRiverLayers;
/// The river parameter uniform of `p` (layers beyond kMaxRiverLayers dropped);
/// `min_valid` is RiverConfig::min_valid.
std::vector<std::uint8_t> river_params_bytes(const RiverParams& p, float min_valid);

/// GPU cost of the river flow shading: one cost tier (nullopt = the legacy
/// shimmer, for reference), the GPU milliseconds for `megapixels` of pure river
/// pixels, per million, and how the time was taken. gpu_timestamps is false when
/// the device has none or a reading failed (that pass then took the wall clock).
/// A cost that is not a positive finite number is "unmeasured" (a driver that
/// reports no time): it can never make a tier look cheap.
struct RiverCost {
    std::optional<RiverQuality> quality;
    double ms = 0, megapixels = 0, ms_per_mpx = 0;
    bool gpu_timestamps = false;
    [[nodiscard]] bool measured() const { return std::isfinite(ms_per_mpx) && ms_per_mpx > 0.0; }
};

/// The tier the automatic choice takes from measurements (GpuFrame::measure_river_cost):
/// the highest tier whose cost above the legacy shimmer's (the entry without a
/// tier) is at most `gate_ms_per_mpx`; Low when none is (the floor). Unmeasured
/// costs (RiverCost::measured) never qualify, and without a measured legacy
/// reference nothing does: the answer is Low, never the dearest tier.
RiverQuality choose_river_quality(const std::vector<RiverCost>& costs, double gate_ms_per_mpx);

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
    /// The tier the river flow shading runs at (GpuLimits::river_quality, or
    /// the automatic choice made in create()).
    [[nodiscard]] RiverQuality river_quality() const;
    /// One line describing an automatic choice that was measured (adapter, gate,
    /// the added cost per tier, or that the budget ran out); empty when the tier
    /// was given or decided by the adapter type. For the caller to print.
    [[nodiscard]] const std::string& river_tier_note() const;
    /// The measurements behind an automatic choice (empty when the tier was
    /// given or the adapter is discrete).
    [[nodiscard]] const std::vector<RiverCost>& river_costs() const;
    /// Renders a synthetic view of 1024 x 1024 pure river pixels (four chunks
    /// with a flow lattice of full-amplitude, shear-free flow, zoom group 1,
    /// the worst case of the shading) a few times per tier and returns the GPU
    /// time per million river pixels: the legacy shimmer first, then Low,
    /// Medium and High. GPU timestamps when the device has them, else the wall
    /// clock around submit and wait. Blocks (tools and the automatic choice in
    /// create(); never per frame).
    /// With `stop_gate` (the automatic choice of create()) only the tiers needed to
    /// choose are measured: the legacy shimmer, then High, Medium, Low until one
    /// adds at most `stop_gate` ms per Mpx (Low is not measured after Medium
    /// failed: it is the floor).
    /// `budget_ms` (0 = none) bounds the wall-clock time: when it is spent the tier
    /// in progress is dropped and the rest are not measured.
    std::vector<RiverCost> measure_river_cost(std::size_t repeats = 5,
                                              std::optional<double> stop_gate = std::nullopt,
                                              double budget_ms = 0.0);
    /// GPU time of the sea pass of the last render() in milliseconds (needs
    /// GpuLimits::gpu_timing and a device with timestamp queries; waits for
    /// the GPU); nullopt otherwise.
    std::optional<double> sea_pass_ms();
    /// An offscreen target of `w × h` in the frame's format that can be
    /// read back.
    std::pair<Texture, TextureView> offscreen_target(std::uint32_t w, std::uint32_t h) const;
    /// The bytes of `texture` (w × h × bpp, rows packed), waiting for the
    /// GPU; empty if the read-back failed (an invalid copy is counted in
    /// errors(), not submitted).
    std::vector<std::uint8_t> read_back(WGPUTexture texture, std::uint32_t w, std::uint32_t h,
                                        std::uint32_t bpp = 4) const;
    [[nodiscard]] WGPUTextureFormat format() const;
    /// Pre-created chunk planes waiting for uploads (GpuLimits::prewarm_chunks).
    [[nodiscard]] std::size_t warm_planes_ready() const;
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
