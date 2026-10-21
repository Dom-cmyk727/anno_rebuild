#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>

#include <anno/render/gpu.hpp>
#include <anno/render/gpu_fluid.hpp>
#include <anno/render/shaders.hpp>

#include "thread_priority.hpp"

namespace anno::render::gpu {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

/// Format of the terrain-height textures.
constexpr WGPUTextureFormat kTerrainFormat = WGPUTextureFormat_R16Uint;
/// Bytes of one sprite instance (see sprite_vs in gpu.wgsl).
constexpr std::uint64_t kInstanceBytes = 32;
/// Bytes of the frame uniform (see Frame in gpu.wgsl).
constexpr std::uint64_t kFrameUniformBytes = 96;

/// Little-endian byte writer for uniforms and instances.
struct Bytes {
    std::vector<std::uint8_t> b;
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void f32(float v) {
        std::uint32_t u = 0;
        std::memcpy(&u, &v, 4);
        u32(u);
    }
};

WGPUBindGroupLayoutEntry texture_entry(std::uint32_t binding, WGPUTextureSampleType sample,
                                       WGPUTextureViewDimension dim) {
    WGPUBindGroupLayoutEntry e = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    e.binding = binding;
    e.visibility = WGPUShaderStage_Fragment;
    e.texture.sampleType = sample;
    e.texture.viewDimension = dim;
    e.texture.multisampled = 0;
    return e;
}

WGPUBindGroupLayoutEntry uniform_entry(std::uint32_t binding) {
    WGPUBindGroupLayoutEntry e = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    e.binding = binding;
    e.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    e.buffer.type = WGPUBufferBindingType_Uniform;
    return e;
}

constexpr WGPUTextureSampleType kFloat = WGPUTextureSampleType_UnfilterableFloat;

BindGroupLayout layout(WGPUDevice d, std::string_view label,
                       const std::vector<WGPUBindGroupLayoutEntry>& entries) {
    WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    return BindGroupLayout(wgpuDeviceCreateBindGroupLayout(d, &desc));
}

WGPUBindGroupEntry view_entry(std::uint32_t binding, WGPUTextureView v) {
    WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
    e.binding = binding;
    e.textureView = v;
    return e;
}

WGPUBindGroupEntry buffer_entry(std::uint32_t binding, WGPUBuffer b, std::uint64_t size) {
    WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
    e.binding = binding;
    e.buffer = b;
    e.offset = 0;
    e.size = size;
    return e;
}

Texture make_texture(WGPUDevice d, std::string_view label, WGPUTextureFormat format,
                     std::uint32_t w, std::uint32_t h, std::uint32_t layers,
                     WGPUTextureUsage usage) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.usage = usage;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {w, h, layers};
    desc.format = format;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    return Texture(wgpuDeviceCreateTexture(d, &desc));
}

TextureView make_view(WGPUTexture t, bool array) {
    WGPUTextureViewDescriptor desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    desc.dimension = array ? WGPUTextureViewDimension_2DArray : WGPUTextureViewDimension_2D;
    return TextureView(wgpuTextureCreateView(t, &desc));
}

void write_layer(WGPUQueue q, WGPUTexture t, std::uint32_t layer, std::uint32_t x, std::uint32_t y,
                 std::uint32_t w, std::uint32_t h, std::uint32_t bpp, const void* data,
                 std::size_t size) {
    WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    dst.texture = t;
    dst.mipLevel = 0;
    dst.origin = {x, y, layer};
    dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout lay = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    lay.offset = 0;
    lay.bytesPerRow = w * bpp;
    lay.rowsPerImage = h;
    const WGPUExtent3D ext = {w, h, 1};
    wgpuQueueWriteTexture(q, &dst, data, size, &lay, &ext);
}

/// Layers of a texture bound as a 2D array: at least 2. wgpu's GL backend
/// creates a 1-layer texture as a plain 2D texture, which then does not
/// match the shader's texture_2d_array (wrong sprites on llvmpipe/GL, CI
/// 2026-10-18); the extra layer is never read.
constexpr std::uint32_t array_layers(std::uint32_t n) {
    return std::max(n, 2U);
}

/// A 2D texture (or array with `array`) filled from `data`.
std::pair<Texture, TextureView> upload_texture(WGPUDevice d, WGPUQueue q, WGPUTextureFormat format,
                                               std::uint32_t w, std::uint32_t h, std::uint32_t bpp,
                                               const std::vector<std::uint8_t>& data, bool array) {
    Texture t = make_texture(d, "upload", format, w, h, array ? array_layers(1) : 1,
                             WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst |
                                 WGPUTextureUsage_CopySrc);
    write_layer(q, t.get(), 0, 0, 0, w, h, bpp, data.data(), data.size());
    TextureView v = make_view(t.get(), array);
    return {std::move(t), std::move(v)};
}

Buffer make_buffer(WGPUDevice d, std::string_view label, std::uint64_t size,
                   WGPUBufferUsage usage) {
    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.size = size;
    desc.usage = usage;
    return Buffer(wgpuDeviceCreateBuffer(d, &desc));
}

/// SeaPattern::steps_at for an uploaded pattern.
std::tuple<std::size_t, std::size_t, float> steps_at(std::size_t steps, std::uint64_t step_ms,
                                                     std::uint64_t time_ms) {
    if (step_ms == 0 || steps < 2) {
        return {0, 0, 0.0F};
    }
    const auto k = static_cast<std::size_t>((time_ms / step_ms) % steps);
    const float frac = static_cast<float>(time_ms % step_ms) / static_cast<float>(step_ms);
    return {k, (k + 1) % steps, frac};
}

} // namespace

std::vector<std::uint8_t> frame_uniform_bytes(const FrameUniform& f) {
    Bytes u;
    u.i32(f.view.x);
    u.i32(f.view.y);
    u.u32(static_cast<std::uint32_t>(std::max(f.view.w, 1)));
    u.u32(static_cast<std::uint32_t>(std::max(f.view.h, 1)));
    u.i32(f.pattern_size.first);
    u.i32(f.pattern_size.second);
    u.u32(f.step0);
    u.u32(f.step1);
    u.f32(f.frac);
    u.f32(f.time_ms);
    u.u32(f.enhanced ? 1U : 0U);
    // tile_w, tile_h, raise, ship_water, objects_water (offsets 44 to 63).
    for (const float v :
         {f.tile_w, f.tile_h, f.raise, f.ship_water ? 1.0F : 0.0F, f.objects_water ? 1.0F : 0.0F}) {
        u.f32(v);
    }
    u.i32(f.fluid_origin.first);
    u.i32(f.fluid_origin.second);
    u.i32(f.fluid_size);
    u.u32(f.fluid_on ? 1U : 0U);
    for (const float v : {f.fluid_cpt, f.river_flow ? 1.0F : 0.0F, f.river_lambda, 0.0F}) {
        u.f32(v);
    }
    return std::move(u.b);
}

RiverQuality choose_river_quality(const std::vector<RiverCost>& costs, double gate_ms_per_mpx) {
    // The gate is on what the flow shading adds to the legacy shimmer's pass (the
    // measurement without a tier): the sea pass's own cost is there either way.
    // A driver that reports no time gives a zero: that must not read as "free", so
    // an unmeasured reference or tier never qualifies (Low is the answer).
    double legacy_ms = 0.0;
    bool have_legacy = false;
    for (const RiverCost& rc : costs) {
        if (!rc.quality && rc.measured()) {
            legacy_ms = rc.ms_per_mpx;
            have_legacy = true;
        }
    }
    RiverQuality best = RiverQuality::Low;
    if (!have_legacy) {
        return best;
    }
    for (const RiverCost& rc : costs) {
        if (rc.quality && rc.measured() && rc.ms_per_mpx - legacy_ms <= gate_ms_per_mpx &&
            static_cast<std::uint32_t>(*rc.quality) > static_cast<std::uint32_t>(best)) {
            best = *rc.quality;
        }
    }
    return best;
}

std::vector<std::uint8_t> river_params_bytes(const RiverParams& p, float min_valid) {
    Bytes u;
    const auto n = static_cast<std::uint32_t>(std::min<std::size_t>(p.count, kMaxRiverLayers));
    u.u32(n);
    u.u32(static_cast<std::uint32_t>(p.quality));
    for (const float v : {min_valid, p.rapids_base, p.shear_from, p.shear_to, kLowTapGain, 0.0F}) {
        u.f32(v);
    }
    for (std::size_t i = 0; i < kMaxRiverLayers; ++i) {
        const RiverLayer l = i < n ? p.layers[i] : RiverLayer{0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        for (const float v : {l.scale, l.period_ms, l.amp, l.fades, l.rapids, 0.0F, 0.0F, 0.0F}) {
            u.f32(v);
        }
    }
    return std::move(u.b);
}

/// struct Piece in gpu.wgsl: 4 x vec2<i32>, a u32 and padding.
constexpr std::size_t kPieceBytes = 48;
/// Distance of two pieces in the piece buffer: the dynamic offset alignment every
/// WebGPU device guarantees (minUniformBufferOffsetAlignment is at most 256).
constexpr std::size_t kPieceStride = 256;
/// Bytes of the resample uniform (struct Scale in gpu.wgsl): four 4-byte words
/// of the scale, the offset and the filter, then the grid size, taps and padding.
constexpr std::size_t kScaleBytes = 32;
/// Most samples per axis of a Box resample: a scale down to 1/8 averages 64
/// samples per output pixel (8 x 8); below that the picture would skip grid
/// pixels again, and a smaller scale needs mips of the grid (T3 stage 3).
constexpr std::uint32_t kMaxBoxTaps = 8;

struct GpuFrame::Impl {
    /// Own references to the device (the Context may move or go away).
    std::unique_ptr<DeviceRef> ref;
    GpuLimits limits;
    /// The device's maxTextureArrayLayers (atlas growth stops there).
    std::uint32_t max_layers = 1;
    bool atlas_full_reported = false;
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
    ShaderModule module;
    BindGroupLayout frame_layout, chunk_layout, atlas_layout, empty_layout, piece_layout,
        scale_layout;
    /// The resample pass of a continuous zoom (struct Scale in gpu.wgsl): its
    /// pipeline, uniform, and the offscreen scene grid it reads.
    PipelineLayout scale_pl;
    RenderPipeline scale_pipe;
    Buffer scale_uniform;
    Texture scene_t;
    TextureView scene_v;
    std::pair<std::uint32_t, std::uint32_t> scene_size{0, 0};
    /// Bound at group 1 for the sprite draws, which use no chunk data.
    BindGroup empty_bind;
    PipelineLayout chunk_pl, sprite_pl;
    RenderPipeline ground_pipe, terrain_pipe, objects_pipe, sea_pipe, sprite_pipe;
    /// Ground and sea with blending, for the draws of chunks fading in.
    RenderPipeline ground_fade_pipe, sea_fade_pipe;
    Buffer frame_uniform;
    /// What each draw of a chunk shows (struct Piece in gpu.wgsl): slot 0 is the
    /// chunk itself (kind 0), the others are the parts of stand-ins drawn this
    /// frame, kPieceStride bytes apart (the dynamic offset's alignment). Grown
    /// when a frame needs more; bound as group 2 of the chunk pipelines.
    Buffer piece_buffer;
    std::size_t piece_slots = 0;
    BindGroup piece_bind;
    /// The river flow shading's parameters (struct RiverParams in gpu.wgsl).
    Buffer river_uniform;
    /// The tier of the river flow shading, and the measurements behind an
    /// automatic choice.
    RiverQuality quality = RiverQuality::High;
    std::vector<RiverCost> costs;
    std::string tier_note;
    /// Timestamp queries (the device has them and a timed pass asked): the
    /// sea pass of render() or a measured pass writes two ticks.
    bool timestamps = false;
    float timestamp_period_ns = 0.0F;
    QuerySet ts_set;
    Buffer ts_resolve, ts_read;
    WGPUPassTimestampWrites ts_writes = WGPU_PASS_TIMESTAMP_WRITES_INIT;
    bool ts_recorded = false;
    /// Offscreen texture of pass 1 and its size; terrain height per pixel.
    Texture under_t, terrain_t;
    TextureView under_v, terrain_v;
    std::pair<std::uint32_t, std::uint32_t> under_size{0, 0};
    /// 1 × 1 stand-ins for unused bindings.
    std::pair<Texture, TextureView> dummy_color, dummy_field, dummy_terrain, dummy_base,
        dummy_depth, dummy_fluid, dummy_array, dummy_flow_uv, dummy_flow_aux, dummy_flow_shear;

    struct Sea {
        std::pair<int, int> size{0, 0};
        std::size_t steps = 0, uploaded = 0;
        std::uint64_t step_ms = 0;
        Texture sprite_t, filled_t;
        TextureView sprite_v, filled_v;
    };
    std::map<std::size_t, Sea> seas;
    std::optional<std::size_t> sea_group;

    /// A chunk plane's texture (512² texels of `format`, or a flow lattice's
    /// size) and its view.
    struct Plane {
        WGPUTextureFormat format = WGPUTextureFormat_Undefined;
        std::uint32_t w = 0, h = 0;
        Texture texture;
        TextureView view;
    };
    struct Chunk {
        BindGroup bind;
        bool real = false;
        /// Chunk::serial of the CPU chunk uploaded (0 = a stand-in): another
        /// serial under the same key means the chunk was rebuilt.
        std::uint64_t serial = 0;
        /// The chunk has a flow lattice on the GPU.
        bool flow = false;
        std::vector<Plane> planes;
        Buffer uniform;
    };
    std::map<ChunkKey, Chunk> chunks;
    /// Planes of dropped chunks, reused by the next uploads: creating and
    /// freeing textures of several megabytes makes the driver allocate and
    /// free device memory blocks, which showed as sporadic stalls of
    /// 10-20 ms on the window thread. Bounded (kPoolPlanes); the textures
    /// are overwritten completely on reuse, and GPU work still reading the
    /// old contents is ordered before those writes.
    std::vector<Plane> plane_pool;
    /// Chunks that replaced a stand-in or open sea and are still fading in: the
    /// time of fade_clock (ms) their fade started at (GpuLimits::chunk_fade_ms).
    std::map<ChunkKey, std::uint64_t> fading;
    /// The clock of `fading` (GpuFrame::set_fade_clock): steady, not the
    /// animation time.
    std::function<std::uint64_t()> fade_clock = [] {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
                .count());
    };
    /// The placeholders the chunks in `fading` replaced, drawn under them until
    /// their fade ends (the open sea they showed before the chunk arrived).
    std::map<ChunkKey, Chunk> fade_under;
    static constexpr std::size_t kPoolPlanes = 8 * 24;
    /// plane_pool.size(), readable from the pre-creation thread (which keeps
    /// the idle planes, pooled and pre-created, within GpuLimits::idle_plane_
    /// budget_mb: the pool counts first, the thread fills the rest).
    std::atomic<std::size_t> pool_planes{0};

    /// Chunk planes created ahead of time by a background thread
    /// (GpuLimits::prewarm_chunks): the first texture creations after start
    /// cost 1-13 ms each on the window thread (the driver allocates device
    /// memory blocks), so a zoom step that uploads its first chunks stalled for
    /// that long. The window thread takes from here when the plane pool has
    /// no match (the lock is held only for a push or a pop); the thread keeps
    /// prewarm_chunks' worth ready, refilling as planes are taken.
    ///
    /// Texture creation takes wgpu-hal's device allocator lock, which the window
    /// thread needs for every upload and every buffer it makes. So the thread
    /// runs at normal priority (a lowered one would let it be descheduled while
    /// holding the lock: a priority inversion on the window thread) and creates
    /// one plane per quiet frame only: render() hands it a token at its end when
    /// the frame uploaded nothing and made no placeholder. Until the first
    /// render() it is unpaced, to fill the pool at start (and for headless users
    /// that wait for warm_planes_ready() before rendering).
    std::mutex warm_mutex;
    std::atomic<bool> warm_token{false};
    std::atomic<bool> warm_paced{false};
    std::vector<Plane> warm_planes;
    /// warm_planes per chunk-plane format (index of kChunkPlanes), kept at push
    /// and pop so that choosing the scarcest format needs no scan under the lock.
    std::array<std::size_t, 5> warm_count{};
    std::thread warm_thread;
    std::condition_variable warm_cv;
    bool warm_stop = false;
    std::size_t planes_warm_taken = 0;

    ~Impl() {
        {
            const std::lock_guard lock(warm_mutex);
            warm_stop = true;
        }
        warm_cv.notify_one();
        if (warm_thread.joinable()) {
            warm_thread.join();
        }
    }

    /// The five full-size chunk planes (formats of upload_chunk).
    static constexpr std::array<std::pair<WGPUTextureFormat, int>, 5> kChunkPlanes = {{
        {WGPUTextureFormat_BGRA8Unorm, 4},
        {WGPUTextureFormat_RGBA8Unorm, 4},
        {WGPUTextureFormat_RGBA8Uint, 4},
        {kTerrainFormat, 2},
        {WGPUTextureFormat_R8Uint, 1},
    }};

    /// Starts the thread that creates `limits.prewarm_chunks` chunks' planes.
    void start_prewarm() {
        if (limits.prewarm_chunks == 0) {
            return;
        }
        warm_thread = std::thread([this] {
            const auto n = static_cast<std::uint32_t>(kChunk);
            // A plane of the five formats is n x n x 3 bytes on average
            // (4 + 4 + 4 + 2 + 1 bytes per texel over five planes).
            const std::size_t plane_bytes = std::size_t{n} * n * 3;
            const std::size_t budget_planes =
                (limits.idle_plane_budget_mb << 20) / std::max<std::size_t>(plane_bytes, 1);
            auto target_now = [&] {
                const std::size_t pooled = pool_planes.load();
                return std::min(limits.prewarm_chunks * kChunkPlanes.size(),
                                budget_planes > pooled ? budget_planes - pooled : 0);
            };
            std::unique_lock lock(warm_mutex);
            while (true) {
                // Refills as the window thread takes planes: the first uploads of
                // each zoom group need a view's worth of chunks at once.
                warm_cv.wait(lock, [&] {
                    return warm_stop || (warm_planes.size() < target_now() &&
                                         (!warm_paced.load() || warm_token.load()));
                });
                if (warm_stop) {
                    return;
                }
                warm_token.store(false);
                // The format that is the scarcest now (uploads take the five of
                // a chunk alike, but a chunk without a plane takes fewer).
                std::size_t pick = 0;
                for (std::size_t k = 1; k < kChunkPlanes.size(); ++k) {
                    if (warm_count[k] < warm_count[pick]) {
                        pick = k;
                    }
                }
                lock.unlock();
                Plane p;
                p.format = kChunkPlanes[pick].first;
                p.w = n;
                p.h = n;
                // wgpu-native returns a non-null handle with an error id when the
                // device refuses a texture, so a null check cannot see it. The
                // uncaptured-error counter is what changes (no error scope here:
                // the scope stack is per device and the window thread's
                // validation scopes would see this thread's). Any error counted
                // meanwhile, from either thread, ends the pre-creation: the plane
                // is dropped and the window thread creates textures itself, as
                // before this thread existed.
                const std::uint64_t errors_before = ref->errors->load();
                p.texture = make_texture(dev(), "upload", p.format, n, n, 1,
                                         WGPUTextureUsage_TextureBinding |
                                             WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
                p.view = make_view(p.texture.get(), false);
                const bool refused = !p.texture || !p.view || ref->errors->load() != errors_before;
                lock.lock();
                if (refused) {
                    return;
                }
                ++warm_count[pick];
                warm_planes.push_back(std::move(p));
            }
        });
    }

    /// Takes a pre-created plane of this format and size, if one is ready.
    bool take_warm(WGPUTextureFormat fmt, std::uint32_t w, std::uint32_t h, Plane& out) {
        // The lock is held for a push or a pop only (the texture is created outside
        // it), so waiting for it is microseconds; a try_lock lost to the thread's
        // bookkeeping and sent the upload to create its own texture.
        std::unique_lock lock(warm_mutex);
        const auto it = std::find_if(warm_planes.begin(), warm_planes.end(), [&](const Plane& p) {
            return p.format == fmt && p.w == w && p.h == h;
        });
        if (it == warm_planes.end()) {
            return false;
        }
        out = std::move(*it);
        for (std::size_t k = 0; k < kChunkPlanes.size(); ++k) {
            if (kChunkPlanes[k].first == fmt) {
                --warm_count[k];
                break;
            }
        }
        warm_planes.erase(it);
        ++planes_warm_taken;
        lock.unlock();
        warm_cv.notify_one();
        return true;
    }

    /// Drops the GPU chunk at `it`, keeping its planes for reuse.
    std::map<ChunkKey, Chunk>::iterator drop_chunk(std::map<ChunkKey, Chunk>::iterator it) {
        for (Plane& p : it->second.planes) {
            if (plane_pool.size() < kPoolPlanes) {
                plane_pool.push_back(std::move(p));
            }
        }
        pool_planes.store(plane_pool.size());
        return chunks.erase(it);
    }

    /// (layer, x, y, width, height) of a frame image in the atlas.
    using Place = std::array<std::uint32_t, 5>;
    struct Atlas {
        Texture texture;
        TextureView view;
        BindGroup bind;
        std::uint32_t layers = 0;
        std::map<SpriteKey, std::optional<Place>> place;
        /// Unit frames (ships), keyed by their decoded image (shared by
        /// the scene's units; only a cache key, never iterated).
        std::map<const Rgba*, std::optional<Place>> unit_place;
        /// Packing cursor: layer, x, y, height of the current shelf.
        std::array<std::uint32_t, 4> cursor{};
    };
    std::optional<Atlas> atlas;
    /// Per-frame phase timers of the sprite path (reset by render()).
    double decode_ms = 0, atlas_ms = 0;
    std::size_t sprites_new = 0;
    /// Chunk upload phases: texture creation (or reuse from the pool), texel writes.
    double up_create_ms = 0, up_write_ms = 0;
    Buffer instances;
    std::uint64_t instances_size = 0;
    /// The wave simulation, created on the first enhanced frame.
    std::optional<FluidGpu> fluid;
    bool fluid_failed = false;

    WGPUDevice dev() const { return ref->device.get(); }
    WGPUQueue queue() const { return ref->queue.get(); }

    /// Makes the piece buffer hold `slots` slots (slot 0 = the chunk itself) and
    /// writes `pieces` (slots * kPieceStride bytes) to it.
    /// True when the buffer (and its bind group) had to be made or grown.
    bool write_pieces(const std::vector<std::uint8_t>& pieces) {
        const std::size_t slots = pieces.size() / kPieceStride;
        const bool grown = !piece_buffer || slots > piece_slots;
        if (grown) {
            std::size_t cap = std::max<std::size_t>(piece_slots, 16);
            while (cap < slots) {
                cap *= 2;
            }
            piece_buffer = make_buffer(dev(), "pieces", cap * kPieceStride,
                                       WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
            piece_slots = cap;
            const std::array entries = {buffer_entry(1, piece_buffer.get(), kPieceBytes)};
            WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
            desc.label = sv("pieces");
            desc.layout = piece_layout.get();
            desc.entryCount = entries.size();
            desc.entries = entries.data();
            piece_bind = BindGroup(wgpuDeviceCreateBindGroup(dev(), &desc));
        }
        wgpuQueueWriteBuffer(queue(), piece_buffer.get(), 0, pieces.data(), pieces.size());
        return grown;
    }

    /// Of a frame's submit: command buffer finish, error scope pop, wgpuQueueSubmit
    /// (milliseconds; the command buffer's release follows it).
    struct SubmitTimes {
        double finish = 0, scope = 0, queue = 0;
    };
    /// Finishes `enc` and submits it, unless recording raised a validation
    /// error: wgpu-native 29 aborts the process when an invalid command
    /// buffer is submitted, so the error is counted (errors()) and the work
    /// dropped instead. False if nothing was submitted.
    bool submit(WGPUCommandEncoder enc, const char* what, SubmitTimes* times = nullptr) const {
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        auto t = Clock::now();
        wgpuDevicePushErrorScope(dev(), WGPUErrorFilter_Validation);
        CommandBuffer cmd(wgpuCommandEncoderFinish(enc, &cd));
        if (times != nullptr) {
            times->finish = ms_since(t);
            t = Clock::now();
        }
        if (const std::string err = pop_error_scope(ref->instance.get(), dev()); !err.empty()) {
            std::fprintf(stderr, "%s not submitted: %s\n", what, err.c_str());
            ref->errors->fetch_add(1);
            return false;
        }
        if (times != nullptr) {
            times->scope = ms_since(t);
            t = Clock::now();
        }
        const WGPUCommandBuffer raw = cmd.get();
        wgpuQueueSubmit(queue(), 1, &raw);
        if (times != nullptr) {
            times->queue = ms_since(t);
        }
        return true;
    }

    RenderPipeline pipeline(std::string_view label, WGPUPipelineLayout pl, std::string_view vs,
                            std::string_view fs, const WGPUVertexBufferLayout* buffers,
                            std::size_t buffer_count, WGPUTextureFormat target,
                            const WGPUBlendState* blend) const {
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = target;
        ct.blend = blend;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState frag = WGPU_FRAGMENT_STATE_INIT;
        frag.module = module.get();
        frag.entryPoint = sv(fs);
        frag.targetCount = 1;
        frag.targets = &ct;
        WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        desc.label = sv(label);
        desc.layout = pl;
        desc.vertex.module = module.get();
        desc.vertex.entryPoint = sv(vs);
        desc.vertex.bufferCount = buffer_count;
        desc.vertex.buffers = buffers;
        desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        desc.multisample.count = 1;
        desc.multisample.mask = 0xFFFFFFFFU;
        desc.fragment = &frag;
        return RenderPipeline(wgpuDeviceCreateRenderPipeline(dev(), &desc));
    }

    void ensure_under(std::uint32_t w, std::uint32_t h) {
        if (under_t && under_size == std::pair{w, h}) {
            return;
        }
        under_t = make_texture(dev(), "under", format, w, h, 1,
                               WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);
        under_v = make_view(under_t.get(), false);
        terrain_t =
            make_texture(dev(), "terrain", kTerrainFormat, w, h, 1,
                         WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);
        terrain_v = make_view(terrain_t.get(), false);
        under_size = {w, h};
    }

    /// The offscreen scene grid of a resampled frame, `w` x `h` in the frame's
    /// format (the compositor's last pass writes it, the resample pass reads it).
    void ensure_scene(std::uint32_t w, std::uint32_t h) {
        if (scene_t && scene_size == std::pair{w, h}) {
            return;
        }
        scene_t = make_texture(dev(), "scene grid", format, w, h, 1,
                               WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);
        scene_v = make_view(scene_t.get(), false);
        scene_size = {w, h};
    }

    /// Uploads up to `max` missing animation steps of `p` as the pattern of
    /// `group` (stopping once `budget_ms` > 0 are spent, after at least one
    /// step); returns the number uploaded.
    std::size_t upload_sea_steps(std::size_t group, const sea::SeaPattern& p, std::size_t max,
                                 double budget_ms = 0.0) {
        const auto begin = Clock::now();
        const auto w = static_cast<std::uint32_t>(p.width),
                   h = static_cast<std::uint32_t>(p.height);
        auto it = seas.find(group);
        if (it == seas.end()) {
            Sea s;
            s.size = {p.width, p.height};
            s.steps = p.step_count();
            s.step_ms = p.step_ms;
            const auto layers = array_layers(static_cast<std::uint32_t>(p.step_count()));
            const auto usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
            s.sprite_t =
                make_texture(dev(), "sea", WGPUTextureFormat_RGBA8Unorm, w, h, layers, usage);
            s.filled_t =
                make_texture(dev(), "sea", WGPUTextureFormat_RGBA8Unorm, w, h, layers, usage);
            s.sprite_v = make_view(s.sprite_t.get(), true);
            s.filled_v = make_view(s.filled_t.get(), true);
            it = seas.emplace(group, std::move(s)).first;
        }
        Sea& s = it->second;
        std::size_t n = 0;
        while (s.uploaded < s.steps && n < max) {
            if (n > 0 && budget_ms > 0.0 && ms_since(begin) >= budget_ms) {
                break;
            }
            const auto k = s.uploaded;
            const auto [sprite, filled] = p.step_images(k);
            write_layer(queue(), s.sprite_t.get(), static_cast<std::uint32_t>(k), 0, 0, w, h, 4,
                        sprite.data(), sprite.size() * 4);
            write_layer(queue(), s.filled_t.get(), static_cast<std::uint32_t>(k), 0, 0, w, h, 4,
                        filled.data(), filled.size() * 4);
            ++s.uploaded;
            ++n;
        }
        return n;
    }

    /// Makes the sea pattern of `group` usable (uploading what is missing,
    /// blocking) and uploads a few steps of the other groups' patterns that
    /// are already rendered (within the frame's upload time budget), so a
    /// zoom change finds them on the GPU.
    void ensure_sea(const Scene& scene, std::size_t group) {
        sea_group.reset();
        if (const sea::SeaPattern* p = scene.sea(group)) {
            upload_sea_steps(group, *p, SIZE_MAX);
            sea_group = group;
        }
        std::size_t budget = limits.sea_steps_per_frame;
        const auto begin = Clock::now();
        for (std::size_t g = 0; g < kGroups && budget > 0; ++g) {
            if (g == group) {
                continue;
            }
            if (const sea::SeaPattern* p = scene.sea_if_ready(g)) {
                double left = 0.0;
                if (limits.upload_budget_ms > 0.0) {
                    left = limits.upload_budget_ms - ms_since(begin);
                    if (left <= 0.0 && budget < limits.sea_steps_per_frame) {
                        break;
                    }
                    left = std::max(left, 1e-6);
                }
                budget -= upload_sea_steps(g, *p, budget, left);
            }
        }
    }

    Chunk upload_chunk(const ChunkKey& key, const render::Chunk* chunk) {
        const auto [g, cx, cy] = key;
        (void)g;
        const Rect r = chunk_rect(cx, cy);
        const auto n = static_cast<std::uint32_t>(kChunk);
        std::uint32_t flags = 0;
        Chunk out;
        // The chunk's vectors are uploaded as they are: their element bytes
        // are the texel bytes on a little-endian host (0RGB = B, G, R, 0;
        // Px = R, G, B, A; u16 = low, high), so the window thread converts
        // nothing (per-texel loops here cost ~0.8 ms per chunk).
        static_assert(std::endian::native == std::endian::little);
        auto view = [&](WGPUTextureFormat fmt, std::uint32_t bpp, const void* data,
                        std::size_t bytes, std::uint32_t bit, WGPUTextureView dummy,
                        std::uint32_t pw, std::uint32_t ph) -> WGPUTextureView {
            // A plane that does not cover the chunk (pw x ph texels, the chunk's
            // size unless a flow lattice) is left out (a short write would be a
            // validation error).
            if (data == nullptr || bytes != std::size_t{pw} * ph * bpp) {
                return dummy;
            }
            const auto c0 = Clock::now();
            Plane plane;
            const auto pooled =
                std::find_if(plane_pool.begin(), plane_pool.end(), [&](const Plane& p) {
                    return p.format == fmt && p.w == pw && p.h == ph;
                });
            if (pooled != plane_pool.end()) {
                plane = std::move(*pooled);
                plane_pool.erase(pooled);
                pool_planes.store(plane_pool.size());
            } else if (!take_warm(fmt, pw, ph, plane)) {
                plane.format = fmt;
                plane.w = pw;
                plane.h = ph;
                plane.texture =
                    make_texture(dev(), "upload", fmt, pw, ph, 1,
                                 WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst |
                                     WGPUTextureUsage_CopySrc);
                plane.view = make_view(plane.texture.get(), false);
            }
            const auto c1 = Clock::now();
            write_layer(queue(), plane.texture.get(), 0, 0, 0, pw, ph, bpp, data, bytes);
            up_create_ms += std::chrono::duration<double, std::milli>(c1 - c0).count();
            up_write_ms += ms_since(c1);
            flags |= bit;
            const WGPUTextureView raw = plane.view.get();
            out.planes.push_back(std::move(plane));
            return raw;
        };
        auto plane = [&](const auto& vec) {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            return std::pair<const void*, std::size_t>{
                chunk != nullptr && !vec.empty() ? static_cast<const void*>(vec.data()) : nullptr,
                vec.size() * sizeof(T)};
        };
        static const render::Chunk kNone;
        const render::Chunk& c = chunk != nullptr ? *chunk : kNone;
        const auto [gp, gb] = plane(c.ground);
        const auto [op, ob] = plane(c.objects);
        const auto [fp, fb] = plane(c.field);
        const auto [tp, tb] = plane(c.terrain);
        const auto [bp, bb] = plane(c.base);
        const WGPUTextureView gv =
            view(WGPUTextureFormat_BGRA8Unorm, 4, gp, gb, 1, dummy_color.second.get(), n, n);
        const WGPUTextureView ov =
            view(WGPUTextureFormat_RGBA8Unorm, 4, op, ob, 2, dummy_color.second.get(), n, n);
        const WGPUTextureView fv =
            view(WGPUTextureFormat_RGBA8Uint, 4, fp, fb, 4, dummy_field.second.get(), n, n);
        const WGPUTextureView tv =
            view(kTerrainFormat, 2, tp, tb, 8, dummy_terrain.second.get(), n, n);
        const WGPUTextureView bv =
            view(WGPUTextureFormat_R8Uint, 1, bp, bb, 16, dummy_base.second.get(), n, n);
        // The river flow lattice (rivers.md 3.3): three planes of the lattice's
        // size, valid only when they are consistent and start at the chunk's
        // corner (the shader reads texel (i, j) at chunk pixel (i, j) * stride).
        const river::FlowPlane& fl = c.flow;
        const std::size_t texels = std::size_t{fl.nx} * fl.ny;
        const bool has_flow = chunk != nullptr && fl.nx >= 2 && fl.ny >= 2 && fl.stride_px >= 1 &&
                              fl.stride_px <= river::kMaxLatticeStride && fl.x0 == r.x &&
                              fl.y0 == r.y && fl.uv.size() == texels * 2 &&
                              fl.aux.size() == texels * 4 && fl.shear.size() == texels;
        WGPUTextureView fuv = dummy_flow_uv.second.get(), fax = dummy_flow_aux.second.get(),
                        fsh = dummy_flow_shear.second.get();
        if (has_flow) {
            fuv = view(WGPUTextureFormat_RG16Sint, 4, fl.uv.data(), fl.uv.size() * 2, 0, fuv, fl.nx,
                       fl.ny);
            fax = view(WGPUTextureFormat_RGBA8Uint, 4, fl.aux.data(), fl.aux.size(), 0, fax, fl.nx,
                       fl.ny);
            fsh = view(WGPUTextureFormat_R8Uint, 1, fl.shear.data(), fl.shear.size(), 0, fsh, fl.nx,
                       fl.ny);
            flags |= kChunkFlagFlow | fl.stride_px << kChunkStrideShift;
        }
        out.flow = has_flow;
        out.uniform =
            make_buffer(dev(), "chunk", 16, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
        Bytes u;
        u.i32(r.x);
        u.i32(r.y);
        u.i32(kChunk);
        u.u32(flags);
        wgpuQueueWriteBuffer(queue(), out.uniform.get(), 0, u.b.data(), u.b.size());
        const std::array entries = {buffer_entry(0, out.uniform.get(), 16),
                                    view_entry(1, gv),
                                    view_entry(2, ov),
                                    view_entry(3, fv),
                                    view_entry(4, tv),
                                    view_entry(5, bv),
                                    view_entry(kFlowVelocityBinding, fuv),
                                    view_entry(kFlowAuxBinding, fax),
                                    view_entry(kFlowShearBinding, fsh)};
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.label = sv("chunk");
        desc.layout = chunk_layout.get();
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        out.bind = BindGroup(wgpuDeviceCreateBindGroup(dev(), &desc));
        out.real = chunk != nullptr;
        out.serial = chunk != nullptr ? chunk->serial : 0;
        return out;
    }

    /// Puts `fresh` in the place of `slot`. The replaced chunk's planes are reused
    /// by later uploads (its bind group goes with it; queued GPU work keeps what
    /// it reads alive).
    void replace_chunk(Chunk& slot, Chunk fresh) {
        Chunk old = std::move(slot);
        slot = std::move(fresh);
        for (Plane& p : old.planes) {
            if (plane_pool.size() < kPoolPlanes) {
                plane_pool.push_back(std::move(p));
            }
        }
        pool_planes.store(plane_pool.size());
    }

    /// Uploads newly built visible chunks (a few per frame), adds stand-ins
    /// for missing ones, and drops GPU chunks the CPU cache no longer holds.
    /// Fills the drop, upload and placeholder timers and counters of `stats`
    /// (the rest of the sync time is the scan).
    std::size_t sync_chunks(const FrameRenderer& frames, std::size_t group,
                            const std::vector<std::pair<int, int>>& visible,
                            const std::vector<ChunkKey>& margin,
                            const std::vector<ChunkKey>& adjacent, std::uint64_t fade_now,
                            GpuStats& stats) {
        std::set<ChunkKey> keys;
        for (const auto& [cx, cy] : visible) {
            keys.insert({group, cx, cy});
        }
        for (auto it = chunks.begin(); it != chunks.end();) {
            const bool keep =
                it->second.real ? frames.chunk(it->first) != nullptr : keys.contains(it->first);
            if (keep) {
                ++it;
                continue;
            }
            const auto d0 = Clock::now();
            const std::size_t kept = plane_pool.size();
            const std::size_t planes = it->second.planes.size();
            it = drop_chunk(it);
            // Planes the pool had no room for were freed with the chunk, here.
            stats.planes_freed += planes - (plane_pool.size() - kept);
            ++stats.chunks_dropped;
            stats.sync_drop_ms += ms_since(d0);
        }
        std::size_t uploads = 0;
        bool waiting = false; // a visible chunk is not (yet) on the GPU
        const auto begin = Clock::now();
        for (const auto& key : keys) {
            const auto it = chunks.find(key);
            const render::Chunk* c = frames.chunk(key);
            // A chunk on the GPU stays unless the CPU rebuilt it (another
            // serial: its river flow arrived, rivers.md 15.6).
            if (it != chunks.end() && it->second.real &&
                (c == nullptr || it->second.serial == c->serial)) {
                continue;
            }
            waiting = true;
            // Count and time budget: the first chunk always goes, so a
            // slow device still makes progress.
            const bool room = uploads < limits.uploads_per_frame &&
                              (uploads == 0 || limits.upload_budget_ms <= 0.0 ||
                               ms_since(begin) < limits.upload_budget_ms);
            if (c != nullptr && room) {
                ++uploads;
                const auto u0 = Clock::now();
                Chunk fresh = upload_chunk(key, c);
                // A chunk dissolves in only over what an earlier frame showed in its
                // place: its placeholder (a stand-in or the open sea). A chunk that
                // reaches the GPU in the frame it comes into view (the edge of a
                // plain pan) and a rebuild of a real chunk (its river flow) swap at
                // once: nothing else was on screen there.
                if (limits.chunk_fade_ms > 0 && it != chunks.end() && !it->second.real) {
                    fading[key] = fade_now;
                    // The placeholder (it has no planes, only its chunk uniform and
                    // bind group) keeps drawing the open sea under the dissolve, as
                    // it did in the frames before.
                    fade_under.insert_or_assign(key, std::move(it->second));
                    it->second = std::move(fresh);
                } else if (it != chunks.end()) {
                    replace_chunk(it->second, std::move(fresh));
                } else {
                    chunks.emplace(key, std::move(fresh));
                }
                stats.sync_real_ms += ms_since(u0);
            } else if (it == chunks.end()) {
                const auto p0 = Clock::now();
                chunks.emplace(key, upload_chunk(key, nullptr));
                ++stats.placeholders;
                stats.sync_placeholder_ms += ms_since(p0);
            }
        }
        // Nothing visible is waiting: upload built chunks ahead, a few per frame,
        // first the margin of the group shown (what a pan brings into view next:
        // on the GPU before it is visible, so it appears at once, no placeholder,
        // no dissolve), then the neighbouring zoom groups' views (after a zoom
        // step they are real chunks and the stand-ins have sources even for the
        // part of the view that was off screen before it). Bounded by
        // GpuLimits::prefetch_chunks of these resident at once.
        if (!waiting && uploads == 0 && limits.prefetch_chunks > 0) {
            const std::array<const std::vector<ChunkKey>*, 2> lists = {&margin, &adjacent};
            // Counted over the two lists (their length, not every GPU chunk).
            std::size_t resident = 0;
            for (const auto* list : lists) {
                for (const auto& key : *list) {
                    const auto it = chunks.find(key);
                    resident += it != chunks.end() && it->second.real ? 1U : 0U;
                }
            }
            std::size_t done = 0;
            for (const auto* list : lists) {
                for (const auto& key : *list) {
                    if (resident >= limits.prefetch_chunks || done >= limits.prefetch_per_frame) {
                        break;
                    }
                    const render::Chunk* c = frames.chunk(key);
                    if (c == nullptr) {
                        continue;
                    }
                    const auto it = chunks.find(key);
                    if (it != chunks.end() && it->second.real && it->second.serial == c->serial) {
                        continue;
                    }
                    const auto u0 = Clock::now();
                    Chunk fresh = upload_chunk(key, c);
                    if (it != chunks.end()) {
                        replace_chunk(it->second, std::move(fresh));
                    } else {
                        chunks.emplace(key, std::move(fresh));
                        ++resident;
                    }
                    ++done;
                    ++stats.chunks_prefetched;
                    stats.sync_real_ms += ms_since(u0);
                }
            }
        }
        return uploads;
    }

    Atlas new_atlas(std::uint32_t layers, std::optional<Atlas> old) {
        Atlas a;
        layers = array_layers(layers);
        a.texture = make_texture(dev(), "atlas", WGPUTextureFormat_RGBA8Unorm, limits.atlas_size,
                                 limits.atlas_size, layers,
                                 WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst |
                                     WGPUTextureUsage_CopySrc);
        if (old) {
            WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
            CommandEncoder enc(wgpuDeviceCreateCommandEncoder(dev(), &ed));
            WGPUTexelCopyTextureInfo src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            src.texture = old->texture.get();
            WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            dst.texture = a.texture.get();
            const WGPUExtent3D ext = {limits.atlas_size, limits.atlas_size, old->layers};
            wgpuCommandEncoderCopyTextureToTexture(enc.get(), &src, &dst, &ext);
            submit(enc.get(), "atlas copy");
            a.place = std::move(old->place);
            a.unit_place = std::move(old->unit_place);
            a.cursor = old->cursor;
        }
        a.layers = layers;
        a.view = make_view(a.texture.get(), true);
        const std::array entries = {view_entry(0, a.view.get())};
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.label = sv("atlas");
        desc.layout = atlas_layout.get();
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        a.bind = BindGroup(wgpuDeviceCreateBindGroup(dev(), &desc));
        return a;
    }

    /// Room for w × h texels: shelf packing, growing the array by one layer
    /// (copying the old layers) when full; nullopt when the device's layer
    /// limit is reached (no eviction yet, as the Rust compositor).
    std::optional<std::array<std::uint32_t, 3>> atlas_alloc(std::uint32_t w, std::uint32_t h) {
        if (!atlas) {
            atlas = new_atlas(1, std::nullopt);
        }
        auto [layer, x, y, shelf] = atlas->cursor;
        const std::uint32_t size = limits.atlas_size;
        if (x + w > size) {
            y += shelf;
            x = 0;
            shelf = 0;
        }
        if (y + h > size) {
            ++layer;
            x = y = shelf = 0;
        }
        if (layer >= max_layers) {
            if (!atlas_full_reported) {
                std::fprintf(stderr, "sprite atlas full (%u layers of %u²): sprites dropped\n",
                             max_layers, size);
                atlas_full_reported = true;
            }
            return std::nullopt;
        }
        atlas->cursor = {layer, x + w, y, std::max(shelf, h)};
        if (layer >= atlas->layers) {
            std::optional<Atlas> old = std::move(atlas);
            atlas.reset();
            atlas = new_atlas(layer + 1, std::move(old));
        }
        return std::array<std::uint32_t, 3>{layer, x, y};
    }

    /// Packs `img` into the atlas; nullopt if it is empty, larger than a
    /// layer, or the atlas is full.
    std::optional<Place> pack_image(const Rgba& img) {
        const std::uint32_t w = img.width, h = img.height;
        if (w == 0 || h == 0 || w > limits.atlas_size || h > limits.atlas_size) {
            return std::nullopt;
        }
        const auto at = atlas_alloc(w, h);
        if (!at) {
            return std::nullopt;
        }
        const auto [layer, x, y] = *at;
        write_layer(queue(), atlas->texture.get(), layer, x, y, w, h, 4, img.pixels.data(),
                    img.pixels.size());
        return Place{layer, x, y, w, h};
    }

    /// The atlas position of a sprite frame, packing it on first use.
    std::optional<Place> atlas_place(const Scene& scene, const SpriteDraw& d) {
        if (atlas) {
            if (const auto it = atlas->place.find(d.key); it != atlas->place.end()) {
                return it->second;
            }
        }
        std::optional<Place> place;
        const auto t0 = Clock::now();
        const auto img = scene.sprites.get(scene.anlagen, d.key, *d.frame);
        const auto t1 = Clock::now();
        decode_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (img) {
            place = pack_image(*img);
            atlas_ms += ms_since(t1);
        }
        ++sprites_new;
        if (atlas) {
            atlas->place.insert_or_assign(d.key, place);
        }
        return place;
    }

    /// The atlas position of a unit frame image, packing it on first use.
    std::optional<Place> unit_place(const Rgba& img) {
        if (atlas) {
            if (const auto it = atlas->unit_place.find(&img); it != atlas->unit_place.end()) {
                return it->second;
            }
        }
        std::optional<Place> place = pack_image(img);
        if (atlas) {
            atlas->unit_place.insert_or_assign(&img, place);
        }
        return place;
    }

    /// Appends instances for the ships in `view` (shadow, then frame; in
    /// Scene::draw_units order) and returns their number. Bit 17 marks a
    /// ship's frame, bits 18-31 hold its anchor row (the enhanced waterline,
    /// ship_in_water in gpu.wgsl).
    std::uint32_t pack_ships(const Scene& scene, Rect view, std::size_t group, Bytes& bytes) {
        std::vector<const UnitSprite*> order;
        for (const auto& u : scene.units) {
            if (!u.z) {
                order.push_back(&u);
            }
        }
        std::stable_sort(order.begin(), order.end(), [](const UnitSprite* a, const UnitSprite* b) {
            return total_less(a->x + a->y, b->x + b->y);
        });
        std::uint32_t n = 0;
        auto add = [&](const UnitFrame& f, std::pair<int, int> at, std::uint32_t flags) {
            const auto p = unit_place(*f.image);
            if (!p) {
                return;
            }
            const auto [layer, ax, ay, w, h] = *p;
            const int left = at.first - f.frame.anchor_x, top = at.second - f.frame.anchor_y;
            if (!Rect{left, top, static_cast<int>(w), static_cast<int>(h)}.intersect(view)) {
                return;
            }
            bytes.i32(left);
            bytes.i32(top);
            for (const std::uint32_t v : {w, h, ax, ay, layer, flags}) {
                bytes.u32(v);
            }
            ++n;
        };
        for (const UnitSprite* u : order) {
            if (group >= u->frames.size() || !u->frames[group] || !u->frames[group]->image) {
                continue;
            }
            const auto at = u->anchor(group, scene.lift);
            if (group < u->shadows.size() && u->shadows[group] && u->shadows[group]->image) {
                add(*u->shadows[group], at, 1U);
            }
            // Bits 18-31: the frame's anchor row (ship_in_water in gpu.wgsl).
            const auto anchor_y = static_cast<std::uint32_t>(
                std::clamp<int>(u->frames[group]->frame.anchor_y, 0, (1 << 14) - 1));
            add(*u->frames[group], at, 1U << 17 | anchor_y << 18);
        }
        return n;
    }

    /// Appends one instance per drawable sprite of `draws` (packing new
    /// frames into the atlas); returns their number.
    std::uint32_t pack_sprites(const Scene& scene, const std::vector<SpriteDraw>& draws,
                               bool tested, Bytes& bytes) {
        std::uint32_t n = 0;
        for (const auto& d : draws) {
            const auto p = atlas_place(scene, d);
            if (!p) {
                continue;
            }
            const auto [layer, ax, ay, w, h] = *p;
            // Placed by the frame box, sized by the decoded image.
            const auto r = d.rect();
            bytes.i32(r[0]);
            bytes.i32(r[1]);
            // Bit 0 shadow, bits 8–15 object height, bit 16 depth-tested.
            const std::uint32_t flags =
                (d.is_shadow() ? 1U : 0U) | std::uint32_t{d.height} << 8 | (tested ? 1U : 0U) << 16;
            for (const std::uint32_t v : {w, h, ax, ay, layer, flags}) {
                bytes.u32(v);
            }
            ++n;
        }
        return n;
    }

    void write_instances(const std::vector<std::uint8_t>& bytes) {
        if (bytes.empty()) {
            return;
        }
        if (!instances || instances_size < bytes.size()) {
            std::uint64_t size = 4096;
            while (size < bytes.size()) {
                size *= 2;
            }
            instances = make_buffer(dev(), "sprites", size,
                                    WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst);
            instances_size = size;
        }
        wgpuQueueWriteBuffer(queue(), instances.get(), 0, bytes.data(), bytes.size());
    }

    const Sea* current_sea() const {
        if (!sea_group) {
            return nullptr;
        }
        const auto it = seas.find(*sea_group);
        return it == seas.end() ? nullptr : &it->second;
    }

    void write_frame_uniform(Rect view, std::size_t group, std::uint64_t time_ms, WaterStyle style,
                             const Scene& scene, bool fluid_on, bool river_flow) {
        FrameUniform f;
        f.view = view;
        if (const Sea* s = current_sea()) {
            std::size_t k0 = 0, k1 = 0;
            std::tie(k0, k1, f.frac) = steps_at(s->steps, s->step_ms, time_ms);
            f.step0 = static_cast<std::uint32_t>(k0);
            f.step1 = static_cast<std::uint32_t>(k1);
            f.pattern_size = s->size;
        }
        f.tile_w = static_cast<float>(kTile[group].w);
        f.tile_h = static_cast<float>(kTile[group].h);
        f.raise = static_cast<float>(scene.lift.scaled(kSeaLevel, 1, group));
        f.time_ms = static_cast<float>(time_ms % 3600000);
        f.enhanced = style == WaterStyle::Enhanced;
        f.ship_water = limits.ship_waterline;
        f.objects_water = limits.object_waterline;
        f.river_flow = river_flow;
        f.river_lambda = sea::river_view(group, scene.lift).lambda;
        // Fluid window: origin (world cells), size, on, cells per tile.
        if (fluid && fluid_on) {
            if (const auto w = fluid->window()) {
                f.fluid_origin = {static_cast<std::int32_t>(w->origin.first),
                                  static_cast<std::int32_t>(w->origin.second)};
                f.fluid_size = static_cast<std::int32_t>(w->size);
                f.fluid_on = true;
                f.fluid_cpt = static_cast<float>(w->cells_per_tile);
            }
        }
        const auto u = frame_uniform_bytes(f);
        wgpuQueueWriteBuffer(queue(), frame_uniform.get(), 0, u.data(), u.size());
    }

    /// Brings the wave simulation to `time_ms` for `view` (enhanced water),
    /// recording its steps into `enc`; false if it cannot run.
    bool update_fluid(WGPUCommandEncoder enc, const Scene& scene, Rect view, std::size_t group,
                      std::uint64_t time_ms) {
        if (!fluid && !fluid_failed) {
            auto made = FluidGpu::create(*ref, fluid::FluidConfig{});
            if (made) {
                fluid.emplace(std::move(made).value());
                fluid->set_background(limits.background_fluid);
            } else {
                std::fprintf(stderr, "wave simulation unavailable: %s\n", made.error().c_str());
                fluid_failed = true;
            }
        }
        if (!fluid) {
            return false;
        }
        const float tw = static_cast<float>(kTile[group].w);
        const float th = static_cast<float>(kTile[group].h);
        const float raise = static_cast<float>(scene.lift.scaled(kSeaLevel, 1, group));
        const float px = static_cast<float>(view.x) + static_cast<float>(view.w) / 2.0F;
        const float py = static_cast<float>(view.y) + static_cast<float>(view.h) / 2.0F + raise;
        const std::pair<float, float> centre{px / tw + py / th, py / th - px / tw};
        const unsigned hw = std::thread::hardware_concurrency();
        const std::size_t threads = hw > 0 ? hw : 4;
        const fluid::DepthFn depth = [&scene](float x, float y) { return scene.fluid_depth(x, y); };
        const float span = static_cast<float>(view.w) / tw + static_cast<float>(view.h) / th;
        fluid->update(enc, group, centre, span, time_ms, depth, threads);
        return true;
    }

    /// Creates the query set and buffers of the timed passes on first use;
    /// false when the device has no timestamp queries.
    bool ensure_timestamps() {
        if (!timestamps) {
            return false;
        }
        if (!ts_set) {
            WGPUQuerySetDescriptor qd = WGPU_QUERY_SET_DESCRIPTOR_INIT;
            qd.label = sv("timestamps");
            qd.type = WGPUQueryType_Timestamp;
            qd.count = 2;
            ts_set = QuerySet(wgpuDeviceCreateQuerySet(dev(), &qd));
            ts_resolve = make_buffer(dev(), "timestamps", 16,
                                     WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc);
            ts_read = make_buffer(dev(), "timestamps read", 16,
                                  WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead);
            ts_writes.querySet = ts_set.get();
            ts_writes.beginningOfPassWriteIndex = 0;
            ts_writes.endOfPassWriteIndex = 1;
        }
        return static_cast<bool>(ts_set);
    }

    /// After the timed pass: resolves the two ticks and copies them to the
    /// read-back buffer.
    void resolve_timestamps(WGPUCommandEncoder enc) {
        wgpuCommandEncoderResolveQuerySet(enc, ts_set.get(), 0, 2, ts_resolve.get(), 0);
        wgpuCommandEncoderCopyBufferToBuffer(enc, ts_resolve.get(), 0, ts_read.get(), 0, 16);
        ts_recorded = true;
    }

    /// The duration (ms) of the last timed pass; waits for the GPU.
    std::optional<double> read_timestamps() {
        if (!ts_recorded || !ts_read) {
            return std::nullopt;
        }
        ts_recorded = false;
        wait_idle(ref->instance.get(), dev(), queue());
        if (!map_read(ref->instance.get(), dev(), ts_read.get(), 16)) {
            return std::nullopt;
        }
        const auto* t =
            static_cast<const std::uint64_t*>(wgpuBufferGetConstMappedRange(ts_read.get(), 0, 16));
        std::optional<double> ms;
        if (t != nullptr && t[1] > t[0]) {
            ms = static_cast<double>(t[1] - t[0]) * static_cast<double>(timestamp_period_ns) * 1e-6;
        }
        wgpuBufferUnmap(ts_read.get());
        return ms;
    }

    BindGroup frame_bind(WGPUTextureView under, WGPUTextureView terrain) const {
        const Sea* s = current_sea();
        const WGPUTextureView sprite = s != nullptr ? s->sprite_v.get() : dummy_array.second.get();
        const WGPUTextureView filled = s != nullptr ? s->filled_v.get() : dummy_array.second.get();
        const std::array entries = {
            buffer_entry(0, frame_uniform.get(), kFrameUniformBytes),
            view_entry(1, under != nullptr ? under : dummy_color.second.get()),
            view_entry(2, sprite),
            view_entry(3, filled),
            view_entry(4, terrain != nullptr ? terrain : dummy_terrain.second.get()),
            view_entry(5, fluid ? fluid->state_view() : dummy_fluid.second.get()),
            view_entry(6, fluid ? fluid->depth_view() : dummy_depth.second.get()),
            buffer_entry(kRiverParamsBinding, river_uniform.get(), kRiverParamsBytes),
        };
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.label = sv("frame");
        desc.layout = frame_layout.get();
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        return BindGroup(wgpuDeviceCreateBindGroup(dev(), &desc));
    }
};

namespace {

WGPURenderPassEncoder begin(WGPUCommandEncoder enc, WGPUTextureView view, WGPUColor clear,
                            const WGPUPassTimestampWrites* timestamps = nullptr) {
    WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    ca.view = view;
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = clear;
    WGPURenderPassDescriptor desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &ca;
    desc.timestampWrites = timestamps;
    return wgpuCommandEncoderBeginRenderPass(enc, &desc);
}

} // namespace

formats::Result<std::unique_ptr<GpuFrame>, std::string>
GpuFrame::create(const Context& c, WGPUTextureFormat format, GpuLimits limits) {
    if (format == WGPUTextureFormat_BGRA8UnormSrgb || format == WGPUTextureFormat_RGBA8UnormSrgb) {
        // Colours are written as stored: an sRGB target would convert twice.
        return std::string("sRGB target format: use a non-sRGB view of the surface");
    }
    WGPULimits device_limits = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(c.device.get(), &device_limits);
    if (limits.atlas_size == 0 || limits.atlas_size > device_limits.maxTextureDimension2D) {
        return "atlas size " + std::to_string(limits.atlas_size) + " outside 1.." +
               std::to_string(device_limits.maxTextureDimension2D);
    }
    if (limits.uploads_per_frame == 0) {
        return std::string("uploads_per_frame must be at least 1");
    }
    std::unique_ptr<GpuFrame> f(new GpuFrame());
    f->impl_ = std::make_unique<Impl>();
    Impl& m = *f->impl_;
    m.ref = std::make_unique<DeviceRef>(c);
    m.limits = limits;
    m.max_layers = std::max(device_limits.maxTextureArrayLayers, 1U);
    if (limits.max_atlas_layers != 0) {
        m.max_layers = std::min(m.max_layers, limits.max_atlas_layers);
    }
    m.format = format;
    auto module = create_shader(c, shaders::compositor(), "gpu.wgsl");
    if (!module) {
        return module.error();
    }
    m.module = std::move(module).value();
    const WGPUDevice d = c.device.get();
    m.frame_layout =
        layout(d, "frame",
               {uniform_entry(0), texture_entry(1, kFloat, WGPUTextureViewDimension_2D),
                texture_entry(2, kFloat, WGPUTextureViewDimension_2DArray),
                texture_entry(3, kFloat, WGPUTextureViewDimension_2DArray),
                texture_entry(4, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
                texture_entry(5, kFloat, WGPUTextureViewDimension_2D),
                texture_entry(6, kFloat, WGPUTextureViewDimension_2D),
                uniform_entry(kRiverParamsBinding)});
    m.chunk_layout = layout(
        d, "chunk",
        {uniform_entry(0), texture_entry(1, kFloat, WGPUTextureViewDimension_2D),
         texture_entry(2, kFloat, WGPUTextureViewDimension_2D),
         texture_entry(3, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
         texture_entry(4, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
         texture_entry(5, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
         texture_entry(kFlowVelocityBinding, WGPUTextureSampleType_Sint,
                       WGPUTextureViewDimension_2D),
         texture_entry(kFlowAuxBinding, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
         texture_entry(kFlowShearBinding, WGPUTextureSampleType_Uint,
                       WGPUTextureViewDimension_2D)});
    m.atlas_layout =
        layout(d, "atlas", {texture_entry(0, kFloat, WGPUTextureViewDimension_2DArray)});
    {
        // Group 2 of the chunk pipelines: the piece (struct Piece in gpu.wgsl) of
        // the draw, at a dynamic offset. Binding 1, so that its declaration does
        // not share a (group, binding) pair with the sprite pipelines' atlas.
        WGPUBindGroupLayoutEntry pe = uniform_entry(1);
        pe.buffer.hasDynamicOffset = 1;
        pe.buffer.minBindingSize = kPieceBytes;
        m.piece_layout = layout(d, "piece", {pe});
        const std::array layouts = {m.frame_layout.get(), m.chunk_layout.get(),
                                    m.piece_layout.get()};
        WGPUPipelineLayoutDescriptor pd = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        pd.label = sv("chunk");
        pd.bindGroupLayoutCount = layouts.size();
        pd.bindGroupLayouts = layouts.data();
        m.chunk_pl = PipelineLayout(wgpuDeviceCreatePipelineLayout(d, &pd));
        // Group 1 is unused by the sprite shaders. Rust passes None there;
        // wgpu-native v29 aborts on a NULL layout, so it gets an empty one
        // (and an empty bind group at draw time).
        m.empty_layout = layout(d, "empty", {});
        WGPUBindGroupDescriptor ed = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        ed.label = sv("empty");
        ed.layout = m.empty_layout.get();
        m.empty_bind = BindGroup(wgpuDeviceCreateBindGroup(d, &ed));
        const std::array sl = {m.frame_layout.get(), m.empty_layout.get(), m.atlas_layout.get()};
        pd.label = sv("sprite");
        pd.bindGroupLayoutCount = sl.size();
        pd.bindGroupLayouts = sl.data();
        m.sprite_pl = PipelineLayout(wgpuDeviceCreatePipelineLayout(d, &pd));
        // The resample pass (group 3 of the module): uniform and the grid.
        m.scale_layout = layout(
            d, "scale", {uniform_entry(0), texture_entry(1, kFloat, WGPUTextureViewDimension_2D)});
        const std::array scl = {m.empty_layout.get(), m.empty_layout.get(), m.empty_layout.get(),
                                m.scale_layout.get()};
        pd.label = sv("scale");
        pd.bindGroupLayoutCount = scl.size();
        pd.bindGroupLayouts = scl.data();
        m.scale_pl = PipelineLayout(wgpuDeviceCreatePipelineLayout(d, &pd));
    }
    WGPUBlendState alpha{};
    alpha.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha,
                   WGPUBlendFactor_OneMinusSrcAlpha};
    alpha.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    const std::array attrs = {
        WGPUVertexAttribute{nullptr, WGPUVertexFormat_Sint32x2, 0, 0},
        WGPUVertexAttribute{nullptr, WGPUVertexFormat_Uint32x2, 8, 1},
        WGPUVertexAttribute{nullptr, WGPUVertexFormat_Uint32x2, 16, 2},
        WGPUVertexAttribute{nullptr, WGPUVertexFormat_Uint32x2, 24, 3},
    };
    WGPUVertexBufferLayout vbl = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vbl.stepMode = WGPUVertexStepMode_Instance;
    vbl.arrayStride = kInstanceBytes;
    vbl.attributeCount = attrs.size();
    vbl.attributes = attrs.data();
    wgpuDevicePushErrorScope(d, WGPUErrorFilter_Validation);
    // Ground and sea are opaque; their variants for a chunk fading in blend by
    // the draw's opacity (piece.transparent) over what it replaces. Only the
    // fading draws use them: blending every draw would add two full-screen
    // read-modify-write passes for a few chunks a second.
    m.ground_pipe = m.pipeline("ground", m.chunk_pl.get(), "chunk_vs", "ground_fs", nullptr, 0,
                               format, nullptr);
    m.ground_fade_pipe = m.pipeline("ground fade", m.chunk_pl.get(), "chunk_vs", "ground_fs",
                                    nullptr, 0, format, &alpha);
    m.terrain_pipe = m.pipeline("terrain", m.chunk_pl.get(), "chunk_vs", "terrain_fs", nullptr, 0,
                                kTerrainFormat, nullptr);
    m.sea_pipe =
        m.pipeline("sea", m.chunk_pl.get(), "chunk_vs", "sea_fs", nullptr, 0, format, nullptr);
    m.sea_fade_pipe =
        m.pipeline("sea fade", m.chunk_pl.get(), "chunk_vs", "sea_fs", nullptr, 0, format, &alpha);
    m.objects_pipe = m.pipeline("objects", m.chunk_pl.get(), "chunk_vs", "objects_fs", nullptr, 0,
                                format, &alpha);
    m.sprite_pipe =
        m.pipeline("sprite", m.sprite_pl.get(), "sprite_vs", "sprite_fs", &vbl, 1, format, &alpha);
    m.scale_pipe =
        m.pipeline("scale", m.scale_pl.get(), "scale_vs", "scale_fs", nullptr, 0, format, nullptr);
    if (const std::string err = pop_error_scope(c.instance.get(), d); !err.empty()) {
        return "pipelines: " + err;
    }
    m.frame_uniform = make_buffer(d, "frame", kFrameUniformBytes,
                                  WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    m.scale_uniform =
        make_buffer(d, "scale", kScaleBytes, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    // Slot 0 of the pieces: the chunk itself (all zero = kind 0).
    m.write_pieces(std::vector<std::uint8_t>(kPieceStride, 0));
    m.river_uniform = make_buffer(d, "river", kRiverParamsBytes,
                                  WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
    m.timestamps = c.timestamps;
    m.timestamp_period_ns = c.timestamp_period_ns;
    const WGPUQueue q = c.queue.get();
    m.dummy_color =
        upload_texture(d, q, WGPUTextureFormat_RGBA8Unorm, 1, 1, 4, {0, 0, 0, 0}, false);
    m.dummy_field =
        upload_texture(d, q, WGPUTextureFormat_RGBA8Uint, 1, 1, 4, {255, 255, 255, 1}, false);
    m.dummy_terrain = upload_texture(
        d, q, kTerrainFormat, 1, 1, 2,
        {static_cast<std::uint8_t>(kNoTerrain & 0xFF), static_cast<std::uint8_t>(kNoTerrain >> 8)},
        false);
    m.dummy_depth = upload_texture(d, q, WGPUTextureFormat_R32Float, 1, 1, 4, {0, 0, 0, 0}, false);
    m.dummy_base = upload_texture(d, q, WGPUTextureFormat_R8Uint, 1, 1, 1, {255}, false);
    m.dummy_fluid = upload_texture(d, q, WGPUTextureFormat_RGBA32Float, 1, 1, 16,
                                   std::vector<std::uint8_t>(16, 0), false);
    m.dummy_array = upload_texture(d, q, WGPUTextureFormat_RGBA8Unorm, 1, 1, 4, {0, 0, 0, 0}, true);
    m.dummy_flow_uv =
        upload_texture(d, q, WGPUTextureFormat_RG16Sint, 1, 1, 4, {0, 0, 0, 0}, false);
    m.dummy_flow_aux =
        upload_texture(d, q, WGPUTextureFormat_RGBA8Uint, 1, 1, 4, {0, 0, 0, 0}, false);
    m.dummy_flow_shear = upload_texture(d, q, WGPUTextureFormat_R8Uint, 1, 1, 1, {0}, false);
    {
        const auto fill = river_params_bytes(RiverParams{}, 0.02F);
        wgpuQueueWriteBuffer(q, m.river_uniform.get(), 0, fill.data(), fill.size());
    }
    // The tier of the river flow shading: given, or the highest the adapter
    // affords (rivers.md 16: discrete adapters are High unmeasured, software
    // adapters Low unmeasured, the rest measured against the gate).
    if (limits.river_quality) {
        m.quality = *limits.river_quality;
    } else if (c.adapter_type == WGPUAdapterType_DiscreteGPU) {
        m.quality = RiverQuality::High;
    } else if (c.adapter_type == WGPUAdapterType_CPU) {
        m.quality = RiverQuality::Low;
    } else {
        m.costs =
            f->measure_river_cost(3, limits.river_gate_ms_per_mpx, limits.river_measure_budget_ms);
        m.quality = choose_river_quality(m.costs, limits.river_gate_ms_per_mpx);
        const auto tier_name = [](RiverQuality tier) {
            return tier == RiverQuality::High     ? "high"
                   : tier == RiverQuality::Medium ? "medium"
                                                  : "low";
        };
        double legacy_ms = 0.0;
        for (const RiverCost& rc : m.costs) {
            if (!rc.quality && rc.measured()) {
                legacy_ms = rc.ms_per_mpx;
            }
        }
        m.tier_note = "river shading tier on " + c.name + ": " + tier_name(m.quality) + " (gate " +
                      std::to_string(limits.river_gate_ms_per_mpx) + " ms per million river pixels";
        bool unmeasured = legacy_ms <= 0.0;
        for (const RiverCost& rc : m.costs) {
            if (rc.quality && rc.measured() && legacy_ms > 0.0) {
                char buf[64];
                std::snprintf(buf, sizeof buf, "; %s +%.2f", tier_name(*rc.quality),
                              rc.ms_per_mpx - legacy_ms);
                m.tier_note += buf;
            } else if (rc.quality) {
                unmeasured = true;
            }
        }
        if (unmeasured || m.costs.size() < 2) {
            m.tier_note += "; not (fully) measured: budget spent or no time reported";
        }
        m.tier_note += ")";
    }
    m.start_prewarm();
    return f;
}

ResampledView resampled_view(double cx, double cy, std::uint32_t out_w, std::uint32_t out_h,
                             double scale, ResampleFilter filter, std::uint32_t max_grid) {
    // The grid's size is out / scale plus the filters' reach plus the rounding up to
    // kResampleGridStep (at most 2 + kResampleGridStep): the smallest scale that
    // fits max_grid.
    constexpr double kSlack = 2.0 + kResampleGridStep;
    const double fit = static_cast<double>(std::max(out_w, out_h)) /
                       std::max(static_cast<double>(max_grid) - kSlack, 1.0);
    scale = std::clamp(std::max(scale, fit), 1.0 / 64.0, 64.0);
    // The output's left/top edge in grid pixels, and its far edge.
    const double x0 = cx - static_cast<double>(out_w) / (2.0 * scale);
    const double y0 = cy - static_cast<double>(out_h) / (2.0 * scale);
    const double x1 = x0 + static_cast<double>(out_w) / scale;
    const double y1 = y0 + static_cast<double>(out_h) / scale;
    ResampledView r;
    r.view.x = static_cast<int>(std::floor(x0));
    r.view.y = static_cast<int>(std::floor(y0));
    // One more pixel than the far edge needs: the bilinear filter reads the next
    // pixel, and the box filter's footprint reaches half an output pixel past.
    auto round_up = [](int v) {
        return (v + kResampleGridStep - 1) / kResampleGridStep * kResampleGridStep;
    };
    r.view.w = round_up(static_cast<int>(std::ceil(x1)) - r.view.x + 1);
    r.view.h = round_up(static_cast<int>(std::ceil(y1)) - r.view.y + 1);
    r.resample.scale = scale;
    r.resample.offset_x = x0 - static_cast<double>(r.view.x);
    r.resample.offset_y = y0 - static_cast<double>(r.view.y);
    r.resample.filter = filter;
    return r;
}

GpuFrame::~GpuFrame() = default;

void GpuFrame::set_fade_clock(std::function<std::uint64_t()> clock) {
    impl_->fade_clock = std::move(clock);
}

std::size_t GpuFrame::warm_planes_ready() const {
    const std::lock_guard lock(impl_->warm_mutex);
    return impl_->warm_planes.size();
}

WGPUTextureFormat GpuFrame::format() const {
    return impl_->format;
}

std::uint64_t GpuFrame::errors() const {
    return impl_->ref->errors->load();
}

GpuStats GpuFrame::render(FrameRenderer& frames, const Scene& scene, Rect view, std::size_t group,
                          std::uint64_t time_ms, WaterStyle style, WGPUTextureView target,
                          const Resample* resample) {
    Impl& m = *impl_;
    // From the first frame on the pre-creation is paced by quiet frames (a
    // stricter condition for the thread, so no wake-up is needed).
    m.warm_paced.store(true);
    const auto start = Clock::now();
    GpuStats stats;
    // Every step below ends with a lap: the time since the previous lap goes to
    // the step, so the steps add up to total_ms (GpuStats::steps).
    auto last = start;
    auto lap = [&last]() {
        const auto now = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        return ms;
    };
    frames.retain_stand_in_sources = m.limits.stand_ins;
    // The visible chunks this compositor still draws over a stand-in (as of the
    // last frame: not uploaded, or dissolving in over it): the eviction in
    // update_chunks keeps their stand-ins' sources until they are shown alone.
    frames.shown_over_stand_in.clear();
    if (m.limits.stand_ins) {
        for (const auto& [cx, cy] : visible_chunks(view)) {
            const ChunkKey key{group, cx, cy};
            const auto it = m.chunks.find(key);
            if (it == m.chunks.end() || !it->second.real || m.fading.contains(key)) {
                frames.shown_over_stand_in.insert(key);
            }
        }
    }
    std::tie(stats.chunks_built, stats.chunks_ms, stats.chunks_missing) =
        frames.update_chunks(scene, view, group);
    stats.chunks_ms = lap();
    const auto w = static_cast<std::uint32_t>(std::max(view.w, 1));
    const auto h = static_cast<std::uint32_t>(std::max(view.h, 1));

    m.decode_ms = m.atlas_ms = 0;
    m.up_create_ms = m.up_write_ms = 0;
    m.sprites_new = 0;
    auto t = Clock::now();
    m.ensure_under(w, h);
    const bool resampling = resample != nullptr && !resample->identity();
    if (resampling) {
        m.ensure_scene(w, h);
    }
    stats.under_ms = lap();
    m.ensure_sea(scene, group);
    stats.sea_ms = lap();
    const auto visible = visible_chunks(view);
    stats.visible_ms = lap();
    const std::size_t warm_before = m.planes_warm_taken;
    const std::uint64_t fade_now = m.fade_clock();
    stats.chunks_uploaded = m.sync_chunks(frames, group, visible, frames.margin_keys(),
                                          frames.adjacent_keys(), fade_now, stats);
    stats.planes_warm = m.planes_warm_taken - warm_before;
    stats.chunk_sync_ms = lap();
    stats.sync_scan_ms = std::max(0.0, stats.chunk_sync_ms - stats.sync_drop_ms -
                                           stats.sync_real_ms - stats.sync_placeholder_ms);
    stats.upload_ms = ms_since(t);

    t = Clock::now();
    const auto below = scene.sprites_in(view, group, time_ms, SpriteLayer::WaterBelow, style);
    const auto above = scene.sprites_in(view, group, time_ms, SpriteLayer::WaterAbove, style);
    Bytes bytes;
    bytes.b.reserve((below.size() + above.size()) * kInstanceBytes);
    const std::uint32_t n_below = m.pack_sprites(scene, below, false, bytes);
    const std::uint32_t n_above = m.pack_sprites(scene, above, true, bytes);
    // Ships are drawn here, after the cached objects, unless the chunks
    // already hold them (a FrameRenderer with ships in its chunks).
    const std::uint32_t n_ships =
        frames.ships_in_chunks() ? 0 : m.pack_ships(scene, view, group, bytes);
    m.write_instances(bytes.b);
    stats.upload_create_ms = m.up_create_ms;
    stats.upload_write_ms = m.up_write_ms;
    stats.decode_ms = m.decode_ms;
    stats.atlas_ms = m.atlas_ms;
    stats.sprites_new = m.sprites_new;
    stats.sprites = std::size_t{n_below} + n_above;
    stats.ships = n_ships;
    stats.sprites_ms = lap();

    t = Clock::now();
    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    ed.label = sv("frame");
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(m.dev(), &ed));
    stats.encoder_ms = lap();
    // Coastal wave simulation (enhanced water only).
    const bool fluid_on = style == WaterStyle::Enhanced && m.limits.wave_simulation &&
                          m.update_fluid(enc.get(), scene, view, group, time_ms);
    stats.fluid_ms = lap();
    stats.wave_simulation = fluid_on;
    stats.fluid_setting_up = fluid_on && m.fluid && m.fluid->setting_up();
    const bool river_flow = style == WaterStyle::Enhanced &&
                            frames.river_flow == RiverFlowMode::Flow && scene.river_active();
    m.write_frame_uniform(view, group, time_ms, style, scene, fluid_on, river_flow);
    {
        RiverParams rp = scene.river_cfg.params();
        rp.quality = m.quality;
        const auto river_bytes = river_params_bytes(rp, scene.river_cfg.min_valid);
        wgpuQueueWriteBuffer(m.queue(), m.river_uniform.get(), 0, river_bytes.data(),
                             river_bytes.size());
    }
    stats.river_quality = m.quality;
    for (const auto& [cx, cy] : visible) {
        if (const auto it = m.chunks.find({group, cx, cy});
            it != m.chunks.end() && it->second.flow) {
            ++stats.river_chunks;
        }
    }
    const BindGroup pass1 = m.frame_bind(nullptr, nullptr);
    const BindGroup pass2 = m.frame_bind(m.under_v.get(), m.terrain_v.get());
    const Px water = scene.textures.water();
    const WGPUColor clear = {water[0] / 255.0, water[1] / 255.0, water[2] / 255.0, 1.0};
    stats.uniforms_ms = lap();
    // What to draw per chunk pass: every visible chunk (a stand-in chunk that is
    // not on the GPU yet draws open sea), then the parts of the other zoom
    // groups' chunks that stand in for the ones that are not (kind 1 pieces).
    struct Draw {
        WGPUBindGroup bind;
        std::uint32_t piece_offset;
        /// A chunk fading in: the blending variants of the ground and sea pipelines.
        bool fading = false;
    };
    std::vector<Draw> draws;
    std::vector<std::uint8_t> pieces(kPieceStride, 0);
    // Chunks that replaced their stand-in or the open sea dissolve in: how
    // transparent each still is (0.5 of the way, smoothstep). They are drawn
    // after the stand-in (or the open sea) they replace, with that opacity.
    std::map<ChunkKey, float> transparent;
    if (!m.fading.empty()) {
        std::set<ChunkKey> shown;
        for (const auto& [cx, cy] : visible) {
            shown.insert({group, cx, cy});
        }
        for (auto it = m.fading.begin(); it != m.fading.end();) {
            const auto c = m.chunks.find(it->first);
            const double progress =
                fade_now <= it->second || m.limits.chunk_fade_ms == 0
                    ? 0.0
                    : std::min(1.0, static_cast<double>(fade_now - it->second) /
                                        static_cast<double>(m.limits.chunk_fade_ms));
            if (!shown.contains(it->first) || c == m.chunks.end() || !c->second.real ||
                progress >= 1.0) {
                it = m.fading.erase(it);
                continue;
            }
            transparent[it->first] =
                static_cast<float>(1.0 - progress * progress * (3.0 - 2.0 * progress));
            ++it;
        }
        stats.chunks_fading = transparent.size();
    }
    for (auto it = m.fade_under.begin(); it != m.fade_under.end();) {
        it = m.fading.contains(it->first) ? std::next(it) : m.fade_under.erase(it);
    }
    auto piece_record = [&](Bytes& b) {
        b.b.resize(kPieceStride, 0);
        const auto offset = static_cast<std::uint32_t>(pieces.size());
        pieces.insert(pieces.end(), b.b.begin(), b.b.end());
        return offset;
    };
    for (const auto& [cx, cy] : visible) {
        const ChunkKey key{group, cx, cy};
        if (!transparent.contains(key)) {
            if (const auto it = m.chunks.find(key); it != m.chunks.end()) {
                draws.push_back({it->second.bind.get(), 0});
            }
        } else if (const auto under = m.fade_under.find(key); under != m.fade_under.end()) {
            draws.push_back({under->second.bind.get(), 0});
        }
    }
    // A visible chunk not on the GPU of which `covered` pixels inside the view
    // are drawn from other groups: the rest of its visible area is open sea.
    auto count_open_sea = [&](const ChunkKey& key, std::uint64_t covered) {
        const auto in = chunk_rect(std::get<1>(key), std::get<2>(key)).intersect(view);
        const std::uint64_t area =
            in ? static_cast<std::uint64_t>(in->w) * static_cast<std::uint64_t>(in->h) : 0;
        if (area > covered) {
            ++stats.open_sea_chunks;
            stats.open_sea_px += area - covered;
            stats.open_sea_built += frames.chunk(key) != nullptr ? 1U : 0U;
        }
    };
    if (m.limits.stand_ins) {
        for (const auto& [cx, cy] : visible) {
            const auto own = m.chunks.find({group, cx, cy});
            const bool real = own != m.chunks.end() && own->second.real;
            const bool dissolving = real && transparent.contains({group, cx, cy});
            if (real && !dissolving) {
                continue;
            }
            const auto c0 = Clock::now();
            const auto cover = stand_in_cover(cx, cy, group, [&](const ChunkKey& k) {
                const auto src = m.chunks.find(k);
                return src != m.chunks.end() && src->second.real;
            });
            stats.stand_in_ms += ms_since(c0);
            for (const StandInPart& part : cover) {
                const std::size_t from = part.from;
                const StandInSpan& sp = part.span;
                const auto src = m.chunks.find({from, sp.src_cx, sp.src_cy});
                Bytes b;
                b.i32(sp.dst.x);
                b.i32(sp.dst.y);
                b.i32(sp.dst.x + sp.dst.w);
                b.i32(sp.dst.y + sp.dst.h);
                b.i32(kTile[from].w);
                b.i32(kTile[from].h);
                b.i32(kTile[group].w);
                b.i32(kTile[group].h);
                b.u32(1);
                draws.push_back({src->second.bind.get(), piece_record(b)});
            }
            if (!cover.empty()) {
                ++stats.stand_in_chunks;
                stats.stand_in_parts += cover.size();
            }
            {
                // The parts are disjoint: what of the chunk's visible area they
                // leave shows open sea (a hole in every group's cover, or all;
                // under a dissolving chunk it fades out over chunk_fade_ms).
                std::uint64_t covered = 0;
                for (const StandInPart& part : cover) {
                    if (const auto in = part.span.dst.intersect(view)) {
                        covered +=
                            static_cast<std::uint64_t>(in->w) * static_cast<std::uint64_t>(in->h);
                    }
                }
                count_open_sea({group, cx, cy}, covered);
            }
        }
    } else {
        for (const auto& [cx, cy] : visible) {
            const auto own = m.chunks.find({group, cx, cy});
            if (own == m.chunks.end() || !own->second.real) {
                count_open_sea({group, cx, cy}, 0);
            }
        }
    }
    // The dissolving chunks last: over the stand-in or open sea they replace.
    for (const auto& [cx, cy] : visible) {
        if (const auto tr = transparent.find({group, cx, cy}); tr != transparent.end()) {
            Bytes b;
            for (int i = 0; i < 8; ++i) {
                b.i32(0);
            }
            b.u32(0);
            b.f32(tr->second);
            draws.push_back({m.chunks.find(tr->first)->second.bind.get(), piece_record(b), true});
        }
    }
    stats.draws_ms = lap();
    stats.pieces_realloc = m.write_pieces(pieces);
    stats.pieces_ms = lap();
    const WGPUBindGroup atlas = m.atlas ? m.atlas->bind.get() : nullptr;
    // `fade` (the ground and sea passes): the pipeline the fading draws switch to
    // (they come last, so it is set once); nullptr = one pipeline for all.
    auto draw_chunks = [&](WGPURenderPassEncoder p, WGPURenderPipeline fade = nullptr) {
        bool fading = false;
        for (const Draw& d : draws) {
            if (fade != nullptr && d.fading && !fading) {
                wgpuRenderPassEncoderSetPipeline(p, fade);
                fading = true;
            }
            wgpuRenderPassEncoderSetBindGroup(p, 1, d.bind, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(p, 2, m.piece_bind.get(), 1, &d.piece_offset);
            wgpuRenderPassEncoderDraw(p, 6, 1, 0, 0);
        }
    };
    {
        WGPURenderPassEncoder p =
            begin(enc.get(), m.terrain_v.get(), {static_cast<double>(kNoTerrain), 0, 0, 0});
        wgpuRenderPassEncoderSetBindGroup(p, 0, pass1.get(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(p, m.terrain_pipe.get());
        draw_chunks(p);
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    {
        WGPURenderPassEncoder p = begin(enc.get(), m.under_v.get(), clear);
        wgpuRenderPassEncoderSetBindGroup(p, 0, pass1.get(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(p, m.ground_pipe.get());
        draw_chunks(p, m.ground_fade_pipe.get());
        if (m.instances && atlas != nullptr && n_below > 0) {
            wgpuRenderPassEncoderSetPipeline(p, m.sprite_pipe.get());
            wgpuRenderPassEncoderSetBindGroup(p, 1, m.empty_bind.get(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(p, 2, atlas, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(p, 0, m.instances.get(), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDraw(p, 6, n_below, 0, 0);
        }
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    const bool timed = m.limits.gpu_timing && m.ensure_timestamps();
    {
        // Pass 2 writes the target itself, or the scene grid a resample reads.
        WGPURenderPassEncoder p = begin(enc.get(), resampling ? m.scene_v.get() : target, clear,
                                        timed ? &m.ts_writes : nullptr);
        wgpuRenderPassEncoderSetBindGroup(p, 0, pass2.get(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(p, m.sea_pipe.get());
        draw_chunks(p, m.sea_fade_pipe.get());
        if (m.instances && atlas != nullptr && n_above > 0) {
            wgpuRenderPassEncoderSetPipeline(p, m.sprite_pipe.get());
            wgpuRenderPassEncoderSetBindGroup(p, 1, m.empty_bind.get(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(p, 2, atlas, 0, nullptr);
            // The above-water instances follow the below-water ones: bound
            // at their offset, drawn from instance 0. A first instance of
            // n_below (gpu.rs) needs base-instance support, which wgpu's GL
            // backend may lack (the instances then start at 0: wrong sprites).
            wgpuRenderPassEncoderSetVertexBuffer(
                p, 0, m.instances.get(), std::uint64_t{n_below} * kInstanceBytes, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDraw(p, 6, n_above, 0, 0);
        }
        wgpuRenderPassEncoderSetPipeline(p, m.objects_pipe.get());
        draw_chunks(p);
        // Ships last, as Scene::draw_units draws them after all objects.
        if (m.instances && atlas != nullptr && n_ships > 0) {
            wgpuRenderPassEncoderSetPipeline(p, m.sprite_pipe.get());
            wgpuRenderPassEncoderSetBindGroup(p, 1, m.empty_bind.get(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(p, 2, atlas, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(p, 0, m.instances.get(),
                                                 std::uint64_t{n_below + n_above} * kInstanceBytes,
                                                 WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDraw(p, 6, n_ships, 0, 0);
        }
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    if (resampling) {
        // The composed grid onto the target (struct Scale in gpu.wgsl).
        Bytes u;
        u.f32(static_cast<float>(resample->scale));
        u.f32(static_cast<float>(resample->offset_x));
        u.f32(static_cast<float>(resample->offset_y));
        u.u32(static_cast<std::uint32_t>(resample->filter));
        u.u32(w);
        u.u32(h);
        u.u32(std::clamp(static_cast<std::uint32_t>(std::ceil(1.0 / resample->scale)), 1U,
                         kMaxBoxTaps));
        u.u32(0);
        wgpuQueueWriteBuffer(m.queue(), m.scale_uniform.get(), 0, u.b.data(), u.b.size());
        const std::array entries = {buffer_entry(0, m.scale_uniform.get(), kScaleBytes),
                                    view_entry(1, m.scene_v.get())};
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.label = sv("scale");
        bd.layout = m.scale_layout.get();
        bd.entryCount = entries.size();
        bd.entries = entries.data();
        const BindGroup scale_bind(wgpuDeviceCreateBindGroup(m.dev(), &bd));
        WGPURenderPassEncoder p = begin(enc.get(), target, clear);
        wgpuRenderPassEncoderSetPipeline(p, m.scale_pipe.get());
        wgpuRenderPassEncoderSetBindGroup(p, 0, m.empty_bind.get(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(p, 1, m.empty_bind.get(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(p, 2, m.empty_bind.get(), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(p, 3, scale_bind.get(), 0, nullptr);
        wgpuRenderPassEncoderDraw(p, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    if (timed) {
        m.resolve_timestamps(enc.get());
    }
    stats.record_ms = lap();
    Impl::SubmitTimes times;
    m.submit(enc.get(), "frame", &times);
    stats.finish_ms = times.finish;
    stats.scope_ms = times.scope;
    stats.queue_ms = times.queue;
    stats.release_ms = std::max(0.0, lap() - times.finish - times.scope - times.queue);
    stats.encode_ms = ms_since(t);
    stats.total_ms = ms_since(start);
    // A frame without uploads or placeholders lets the pre-creation thread make
    // one plane (Impl::warm_mutex); a busy frame leaves the allocator alone.
    if (m.limits.prewarm_chunks > 0) {
        {
            // Under the mutex, or the wake-up can be lost between the thread's
            // predicate check and its wait; the thread holds it only briefly.
            const std::lock_guard lock(m.warm_mutex);
            if (stats.chunks_uploaded == 0 && stats.placeholders == 0) {
                m.warm_token.store(true);
                m.warm_cv.notify_one();
            }
        }
    }
    return stats;
}

bool GpuFrame::group_ready(const Scene& scene, std::size_t group) const {
    if (!scene.sea_built(group)) {
        return false;
    }
    if (scene.sea_if_ready(group) == nullptr) {
        // No sea pattern for this group: nothing to upload.
        return true;
    }
    const auto it = impl_->seas.find(group);
    return it != impl_->seas.end() && it->second.uploaded >= it->second.steps;
}

void GpuFrame::wait() const {
    wait_idle(impl_->ref->instance.get(), impl_->dev(), impl_->queue());
}

RiverQuality GpuFrame::river_quality() const {
    return impl_->quality;
}

const std::vector<RiverCost>& GpuFrame::river_costs() const {
    return impl_->costs;
}

const std::string& GpuFrame::river_tier_note() const {
    return impl_->tier_note;
}

std::optional<double> GpuFrame::sea_pass_ms() {
    return impl_->read_timestamps();
}

std::vector<RiverCost> GpuFrame::measure_river_cost(std::size_t repeats,
                                                    std::optional<double> stop_gate,
                                                    double budget_ms) {
    Impl& m = *impl_;
    constexpr std::uint32_t kSide = 1024;
    constexpr std::size_t kGroup = 1;
    const auto n = static_cast<std::size_t>(kChunk);
    // Four chunks of pure river pixels (cover 255, far from any shore, 5 units
    // deep, no slope: no rapids) with a flow lattice of stride 4: a speed of
    // 0.6 tiles/s turning slowly around, level 32 units, valid, no shear (the
    // shear fade removes layers: this is the worst case of the shading).
    render::Chunk syn;
    syn.field.resize(n * n * 4);
    for (std::size_t i = 0; i < n * n; ++i) {
        syn.field[i * 4] = 255;
        syn.field[i * 4 + 1] = 255;
        syn.field[i * 4 + 2] = static_cast<std::uint8_t>(sea::kDepthBias + 20.0F);
        syn.field[i * 4 + 3] = 2;
    }
    const std::uint32_t stride = 4;
    river::FlowPlane& fp = syn.flow;
    fp.stride_px = stride;
    fp.nx = fp.ny = static_cast<std::uint32_t>(kChunk) / stride + 1;
    fp.tile_w = kTile[kGroup].w;
    fp.tile_h = kTile[kGroup].h;
    fp.uv.assign(std::size_t{fp.nx} * fp.ny * 2, 0);
    fp.aux.assign(std::size_t{fp.nx} * fp.ny * 4, 0);
    fp.shear.assign(std::size_t{fp.nx} * fp.ny, 0);
    for (std::uint32_t j = 0; j < fp.ny; ++j) {
        for (std::uint32_t i = 0; i < fp.nx; ++i) {
            const float a = 0.05F * static_cast<float>(i) + 0.03F * static_cast<float>(j);
            river::set_texel(fp, i, j, 0.6F * std::cos(a), 0.6F * std::sin(a), 1.0F, 0.0F, 0.0F,
                             32.0F, 8.0F, 0.0F);
        }
    }
    std::vector<Impl::Chunk> quad;
    for (int cy = 0; cy < 2; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            fp.x0 = cx * kChunk;
            fp.y0 = cy * kChunk;
            quad.push_back(m.upload_chunk({kGroup, cx, cy}, &syn));
        }
    }
    m.ensure_under(kSide, kSide);
    const auto target = offscreen_target(kSide, kSide);
    const BindGroup frame_bind = m.frame_bind(m.under_v.get(), m.terrain_v.get());
    const bool stamps = m.ensure_timestamps();
    FrameUniform fu;
    fu.view = {0, 0, static_cast<int>(kSide), static_cast<int>(kSide)};
    fu.enhanced = true;
    fu.tile_w = static_cast<float>(kTile[kGroup].w);
    fu.tile_h = static_cast<float>(kTile[kGroup].h);
    fu.raise = static_cast<float>(kHeightLift.scaled(kSeaLevel, 1, kGroup));
    fu.river_lambda = sea::river_view(kGroup, kHeightLift).lambda;
    const double mpx = static_cast<double>(kSide) * kSide / 1e6;
    const WGPUColor clear = {0, 0, 0, 1};
    const auto started = Clock::now();
    const auto over_budget = [&] { return budget_ms > 0.0 && ms_since(started) > budget_ms; };
    // Set when a pass wanted GPU timestamps and had to use the wall clock.
    bool fell_back = false;
    // One pass; its GPU milliseconds (timestamps; the wall clock around submit
    // and wait when the device has none or the reading gave nothing: a driver that
    // reports no time is never "0 ms"). nullopt = the submit failed.
    auto pass = [&](std::optional<RiverQuality> quality) -> std::optional<double> {
        fu.river_flow = quality.has_value();
        const auto u = frame_uniform_bytes(fu);
        wgpuQueueWriteBuffer(m.queue(), m.frame_uniform.get(), 0, u.data(), u.size());
        RiverParams rp = river::RiverConfig{}.params();
        rp.quality = quality.value_or(RiverQuality::High);
        const auto rb = river_params_bytes(rp, 0.02F);
        wgpuQueueWriteBuffer(m.queue(), m.river_uniform.get(), 0, rb.data(), rb.size());
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        CommandEncoder enc(wgpuDeviceCreateCommandEncoder(m.dev(), &ed));
        WGPURenderPassEncoder p =
            begin(enc.get(), target.second.get(), clear, stamps ? &m.ts_writes : nullptr);
        wgpuRenderPassEncoderSetBindGroup(p, 0, frame_bind.get(), 0, nullptr);
        const std::uint32_t whole = 0;
        wgpuRenderPassEncoderSetBindGroup(p, 2, m.piece_bind.get(), 1, &whole);
        wgpuRenderPassEncoderSetPipeline(p, m.sea_pipe.get());
        for (const Impl::Chunk& c : quad) {
            wgpuRenderPassEncoderSetBindGroup(p, 1, c.bind.get(), 0, nullptr);
            wgpuRenderPassEncoderDraw(p, 6, 1, 0, 0);
        }
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
        if (stamps) {
            m.resolve_timestamps(enc.get());
        }
        wait();
        const auto t0 = Clock::now();
        if (!m.submit(enc.get(), "river cost")) {
            return std::nullopt;
        }
        if (stamps) {
            if (const auto gpu_ms = m.read_timestamps()) {
                return *gpu_ms;
            }
            fell_back = true;
        }
        wait();
        return ms_since(t0);
    };
    std::vector<RiverCost> out;
    // All tiers from the cheapest up; with a gate from the dearest down, stopping at
    // the first that fits (and after Medium: Low is the floor whatever it costs),
    // so the automatic choice on a slow adapter measures as little as possible.
    std::array<std::optional<RiverQuality>, 4> tiers = {std::nullopt, RiverQuality::Low,
                                                        RiverQuality::Medium, RiverQuality::High};
    if (stop_gate) {
        tiers = {std::nullopt, RiverQuality::High, RiverQuality::Medium, RiverQuality::Low};
    }
    for (const auto& tier : tiers) {
        // Warm-up passes: an integrated GPU idles at a low clock and needs a few
        // milliseconds of work to leave it. The wall-clock budget is checked before
        // every pass; a tier it cuts short is dropped (unmeasured, never "free").
        bool complete = true;
        for (int i = 0; i < 3 && complete; ++i) {
            complete = !over_budget() && pass(tier).has_value();
        }
        std::vector<double> times;
        fell_back = false;
        for (std::size_t i = 0; i < std::max<std::size_t>(repeats, 1) && complete; ++i) {
            complete = !over_budget();
            if (complete) {
                const auto t = pass(tier);
                complete = t.has_value();
                if (t) {
                    times.push_back(*t);
                }
            }
        }
        if (!complete || times.empty()) {
            break;
        }
        std::sort(times.begin(), times.end());
        RiverCost rc;
        rc.quality = tier;
        rc.ms = times[times.size() / 2];
        rc.megapixels = mpx;
        rc.ms_per_mpx = rc.ms / mpx;
        rc.gpu_timestamps = stamps && !fell_back;
        out.push_back(rc);
        if (stop_gate && tier &&
            (rc.ms_per_mpx - out.front().ms_per_mpx <= *stop_gate ||
             *tier == RiverQuality::Medium)) {
            break;
        }
    }
    return out;
}

std::pair<Texture, TextureView> GpuFrame::offscreen_target(std::uint32_t w, std::uint32_t h) const {
    Texture t = make_texture(impl_->dev(), "offscreen", impl_->format, w, h, 1,
                             WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc);
    TextureView v = make_view(t.get(), false);
    return {std::move(t), std::move(v)};
}

std::vector<std::uint8_t> GpuFrame::read_back(WGPUTexture texture, std::uint32_t w, std::uint32_t h,
                                              std::uint32_t bpp) const {
    const std::uint32_t row = (bpp * w + 255) / 256 * 256;
    Buffer buf = make_buffer(impl_->dev(), "read back", std::uint64_t{row} * h,
                             WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead);
    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(impl_->dev(), &ed));
    WGPUTexelCopyTextureInfo src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    src.texture = texture;
    WGPUTexelCopyBufferInfo dst = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    dst.buffer = buf.get();
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = row;
    dst.layout.rowsPerImage = h;
    const WGPUExtent3D ext = {w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc.get(), &src, &dst, &ext);
    if (!impl_->submit(enc.get(), "read-back")) {
        return {};
    }
    if (!map_read(impl_->ref->instance.get(), impl_->dev(), buf.get(), std::size_t{row} * h)) {
        return {};
    }
    std::vector<std::uint8_t> out;
    out.reserve(std::size_t{bpp} * w * h);
    const auto* data = static_cast<const std::uint8_t*>(
        wgpuBufferGetConstMappedRange(buf.get(), 0, std::size_t{row} * h));
    if (data != nullptr) {
        for (std::uint32_t y = 0; y < h; ++y) {
            out.insert(out.end(), data + std::size_t{y} * row,
                       data + std::size_t{y} * row + std::size_t{bpp} * w);
        }
    }
    wgpuBufferUnmap(buf.get());
    return out;
}

} // namespace anno::render::gpu
