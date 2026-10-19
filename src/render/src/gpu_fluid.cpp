#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <anno/render/gpu_fluid.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/shaders.hpp>

namespace anno::render::gpu {

namespace {

/// Bytes of the per-step uniform (Step in fluid.wgsl).
constexpr std::uint64_t kStepBytes = 32;
/// Bytes of Params in fluid.wgsl: 5 vec4 + 16 vec4 swell components.
constexpr std::uint64_t kParamsBytes = 80 + 16 * 16;

void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void put_f32(std::vector<std::uint8_t>& b, float v) {
    std::uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    put_u32(b, u);
}

/// A world cell or size as the shader's i32 (the window lies within ±2³¹
/// cells: world tiles × cells per tile).
std::int32_t i32_of(std::int64_t v) {
    return static_cast<std::int32_t>(v);
}

/// Params of fluid.wgsl.
std::vector<std::uint8_t> params_bytes(const fluid::FluidConfig& cfg, const fluid::Swell& comps) {
    const std::array v = {cfg.gravity(),         cfg.depth_cap,
                          cfg.min_depth,         cfg.dt,
                          cfg.damping,           cfg.friction,
                          cfg.break_ratio,       cfg.break_rate,
                          cfg.foam_gain,         std::exp(-cfg.dt / cfg.foam_decay),
                          cfg.force_depth.first, cfg.force_depth.second,
                          cfg.force_rate,        static_cast<float>(cfg.sponge_cells),
                          cfg.infiltration,      cfg.deep_speed / cfg.depth_cap,
                          cfg.max_speed,         cfg.swash_foam,
                          cfg.break_min_depth,   0.0F};
    std::vector<std::uint8_t> out;
    out.reserve(kParamsBytes);
    for (const float f : v) {
        put_f32(out, f);
    }
    for (const auto& c : comps) {
        for (const float f : {c.k[0], c.k[1], c.omega, c.phase, c.amplitude, 0.0F, 0.0F, 0.0F}) {
            put_f32(out, f);
        }
    }
    return out;
}

/// Step of fluid.wgsl.
std::vector<std::uint8_t> step_bytes(const fluid::FluidWindow& w, float t) {
    std::vector<std::uint8_t> out;
    out.reserve(kStepBytes);
    for (const std::int64_t v : {w.origin.first, w.origin.second, w.size}) {
        put_u32(out, static_cast<std::uint32_t>(i32_of(v)));
    }
    put_u32(out, 0);
    put_f32(out, static_cast<float>(w.cells_per_tile));
    put_f32(out, t);
    put_u32(out, 0);
    put_u32(out, 0);
    return out;
}

WGPUBindGroupLayoutEntry tex_entry(std::uint32_t binding) {
    WGPUBindGroupLayoutEntry e = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    e.binding = binding;
    e.visibility = WGPUShaderStage_Compute;
    e.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    e.texture.viewDimension = WGPUTextureViewDimension_2D;
    e.texture.multisampled = 0;
    return e;
}

WGPUBindGroupLayoutEntry uniform_entry(std::uint32_t binding, bool dynamic) {
    WGPUBindGroupLayoutEntry e = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    e.binding = binding;
    e.visibility = WGPUShaderStage_Compute;
    e.buffer.type = WGPUBufferBindingType_Uniform;
    e.buffer.hasDynamicOffset = dynamic ? 1U : 0U;
    return e;
}

Texture grid_texture(WGPUDevice d, const char* label, WGPUTextureFormat format, std::uint32_t n,
                     WGPUTextureUsage usage) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.usage = usage;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {n, n, 1};
    desc.format = format;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    return Texture(wgpuDeviceCreateTexture(d, &desc));
}

TextureView view_of(WGPUTexture t) {
    WGPUTextureViewDescriptor desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    return TextureView(wgpuTextureCreateView(t, &desc));
}

Buffer uniform_buffer(WGPUDevice d, const char* label, std::uint64_t size) {
    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.size = size;
    desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    return Buffer(wgpuDeviceCreateBuffer(d, &desc));
}

} // namespace

formats::Result<FluidGpu, std::string> FluidGpu::create(const Context& c, fluid::FluidConfig cfg) {
    return create(DeviceRef(c), std::move(cfg));
}

formats::Result<FluidGpu, std::string> FluidGpu::create(const DeviceRef& dev,
                                                        fluid::FluidConfig cfg) {
    const WGPUDevice d = dev.device.get();
    WGPULimits limits = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(d, &limits);
    // The window texture, the per-step uniform ring and the zoom groups
    // must fit the device and the renderer (FluidConfig is data, PLAN §0.1).
    if (cfg.size < 64 || cfg.size > limits.maxTextureDimension2D) {
        return "fluid window size " + std::to_string(cfg.size) + " outside 64.." +
               std::to_string(limits.maxTextureDimension2D);
    }
    if (cfg.cells_per_tile.size() < kGroups ||
        std::find(cfg.cells_per_tile.begin(), cfg.cells_per_tile.end(), 0U) !=
            cfg.cells_per_tile.end()) {
        return std::string("fluid cells_per_tile needs a non-zero entry per zoom group");
    }
    const std::uint64_t align_bytes =
        std::max<std::uint64_t>(limits.minUniformBufferOffsetAlignment, kStepBytes);
    const std::uint64_t ring = align_bytes * std::max(cfg.max_steps, 1U);
    if (ring > limits.maxBufferSize || ring > UINT32_MAX) {
        return "fluid max_steps " + std::to_string(cfg.max_steps) + " too large for the device";
    }
    FluidGpu f;
    f.ref_ = std::make_unique<DeviceRef>(dev);
    auto module = create_shader(dev.instance.get(), d, shaders::fluid(), "fluid.wgsl");
    if (!module) {
        return module.error();
    }
    WGPUBindGroupLayoutEntry storage = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    storage.binding = 3;
    storage.visibility = WGPUShaderStage_Compute;
    storage.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
    storage.storageTexture.format = WGPUTextureFormat_RGBA32Float;
    storage.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
    const std::array entries = {uniform_entry(0, false), uniform_entry(1, true), tex_entry(2),
                                storage, tex_entry(4)};
    WGPUBindGroupLayoutDescriptor ld = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    ld.label = sv("fluid");
    ld.entryCount = entries.size();
    ld.entries = entries.data();
    f.layout_ = BindGroupLayout(wgpuDeviceCreateBindGroupLayout(d, &ld));
    const WGPUBindGroupLayout layouts[] = {f.layout_.get()};
    WGPUPipelineLayoutDescriptor pd = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pd.label = sv("fluid");
    pd.bindGroupLayoutCount = 1;
    pd.bindGroupLayouts = layouts;
    const PipelineLayout pl(wgpuDeviceCreatePipelineLayout(d, &pd));
    auto pipe = [&](const char* entry) {
        WGPUComputePipelineDescriptor cd = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
        cd.label = sv(entry);
        cd.layout = pl.get();
        cd.compute.module = module.value().get();
        cd.compute.entryPoint = sv(entry);
        return ComputePipeline(wgpuDeviceCreateComputePipeline(d, &cd));
    };
    wgpuDevicePushErrorScope(d, WGPUErrorFilter_Validation);
    f.vel_pipe_ = pipe("vel_step");
    f.eta_pipe_ = pipe("eta_step");
    if (const std::string err = pop_error_scope(dev.instance.get(), d); !err.empty()) {
        return "fluid pipelines: " + err;
    }
    f.comps_ = fluid::swell_components(cfg);
    f.params_ = uniform_buffer(d, "fluid params", kParamsBytes);
    const auto pb = params_bytes(cfg, f.comps_);
    wgpuQueueWriteBuffer(dev.queue.get(), f.params_.get(), 0, pb.data(), pb.size());
    f.align_ = align_bytes;
    f.steps_ = uniform_buffer(d, "fluid steps", ring);
    f.cfg_ = std::move(cfg);
    // Small until the first update sizes it to the view.
    f.grid_ = f.make_grid(64);
    return f;
}

FluidGpu::Grid FluidGpu::make_grid(std::uint32_t n) const {
    const WGPUDevice d = ref_->device.get();
    Grid g;
    g.n = n;
    for (std::size_t i = 0; i < 2; ++i) {
        g.states[i] =
            grid_texture(d, "fluid state", WGPUTextureFormat_RGBA32Float, n,
                         WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding |
                             WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
        g.views[i] = view_of(g.states[i].get());
    }
    g.bathy = grid_texture(d, "fluid depth", WGPUTextureFormat_R32Float, n,
                           WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    g.bathy_view = view_of(g.bathy.get());
    auto bind = [&](WGPUTextureView src, WGPUTextureView dst) {
        std::array<WGPUBindGroupEntry, 5> e{};
        for (std::size_t i = 0; i < e.size(); ++i) {
            e[i] = WGPU_BIND_GROUP_ENTRY_INIT;
            e[i].binding = static_cast<std::uint32_t>(i);
        }
        e[0].buffer = params_.get();
        e[0].size = kParamsBytes;
        e[1].buffer = steps_.get();
        e[1].size = kStepBytes;
        e[2].textureView = src;
        e[3].textureView = dst;
        e[4].textureView = g.bathy_view.get();
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.label = sv("fluid");
        desc.layout = layout_.get();
        desc.entryCount = e.size();
        desc.entries = e.data();
        return BindGroup(wgpuDeviceCreateBindGroup(d, &desc));
    };
    g.bind_a = bind(g.views[0].get(), g.views[1].get());
    g.bind_b = bind(g.views[1].get(), g.views[0].get());
    return g;
}

void FluidGpu::update(WGPUCommandEncoder encoder, std::size_t group, std::pair<float, float> centre,
                      float span_tiles, std::uint64_t time_ms, const fluid::DepthFn& depth,
                      std::size_t threads) {
    const double target = static_cast<double>(time_ms) / 1000.0;
    const std::uint32_t size = cfg_.window_size(group, span_tiles);
    if (background_) {
        prepare_window(group, centre, size, target, depth, threads);
        if (!window_) {
            // No window yet: nothing to step.
            return;
        }
    } else {
        if (size != grid_.n) {
            grid_ = make_grid(size);
            window_.reset();
        }
        std::vector<fluid::CellRect> new_rects;
        if (window_ && group_ == group) {
            new_rects = window_->recentre(centre, cfg_.recentre_cells);
        } else {
            window_ = fluid::FluidWindow::with_size(cfg_, group, centre, size);
            group_ = group;
            sim_t_ = target;
            new_rects = {window_->rect()};
        }
        const auto now = static_cast<float>(sim_t_);
        for (const auto& r : new_rects) {
            fill(*window_, r, depth, now, threads);
        }
    }
    const fluid::FluidWindow window = *window_;
    const auto dt = static_cast<double>(cfg_.dt);
    if (target < sim_t_) {
        // Time went back (pause, clock wrap): restart from here.
        sim_t_ = target;
    }
    // Rust `as u64` of a non-negative floor: saturating (the difference is
    // never negative here, and NaN cannot occur).
    const double due_f = std::floor((target - sim_t_) / dt);
    const std::uint64_t due =
        due_f >= 18446744073709551615.0 ? UINT64_MAX : static_cast<std::uint64_t>(due_f);
    const std::uint64_t n = std::min<std::uint64_t>(due, cfg_.max_steps);
    if (due > n) {
        // Too far behind: skip ahead rather than stall the frame.
        sim_t_ += static_cast<double>(due - n) * dt;
    }
    if (n == 0) {
        return;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(align_ * n), 0);
    for (std::uint64_t k = 0; k < n; ++k) {
        const double t = sim_t_ + dt * static_cast<double>(k + 1);
        const auto step = step_bytes(window, static_cast<float>(t));
        std::memcpy(bytes.data() + align_ * k, step.data(), step.size());
    }
    wgpuQueueWriteBuffer(ref_->queue.get(), steps_.get(), 0, bytes.data(), bytes.size());
    sim_t_ += dt * static_cast<double>(n);
    const std::uint32_t groups = (grid_.n + 7) / 8;
    WGPUComputePassDescriptor cd = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
    cd.label = sv("fluid");
    const WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &cd);
    for (std::uint64_t k = 0; k < n; ++k) {
        const auto off = static_cast<std::uint32_t>(align_ * k);
        wgpuComputePassEncoderSetPipeline(pass, vel_pipe_.get());
        wgpuComputePassEncoderSetBindGroup(pass, 0, grid_.bind_a.get(), 1, &off);
        wgpuComputePassEncoderDispatchWorkgroups(pass, groups, groups, 1);
        wgpuComputePassEncoderSetPipeline(pass, eta_pipe_.get());
        wgpuComputePassEncoderSetBindGroup(pass, 0, grid_.bind_b.get(), 1, &off);
        wgpuComputePassEncoderDispatchWorkgroups(pass, groups, groups, 1);
    }
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);
}

namespace {

/// Depth and initial state of a rectangle of cells, in the storage layout of
/// the textures (4 and 16 bytes per cell, little-endian floats, rows of
/// `pw` cells).
struct Piece {
    /// Storage column and row of the first cell.
    std::size_t sx = 0, sy = 0;
    std::uint32_t pw = 0, ph = 0;
    std::vector<std::uint8_t> depth, state;
};

/// [a0, a1) cut where it wraps around the ring storage of `n` cells.
std::vector<std::pair<std::int64_t, std::int64_t>> ring_split(std::int64_t a0, std::int64_t a1,
                                                              std::int64_t n) {
    const std::int64_t s0 = ((a0 % n) + n) % n;
    const std::int64_t len = a1 - a0;
    std::vector<std::pair<std::int64_t, std::int64_t>> out;
    if (s0 + len <= n) {
        out.emplace_back(a0, a1);
    } else {
        out.emplace_back(a0, a0 + (n - s0));
        out.emplace_back(a0 + (n - s0), a1);
    }
    return out;
}

/// The cells of `r` (new cells of `window`) as texture-ready pieces: depth
/// from `depth` and the initial state at time `t`, computed on `threads`
/// threads; the pieces stop early (and are incomplete) when `cancel` is set.
std::vector<Piece> compute_pieces(const fluid::FluidConfig& cfg, const fluid::Swell& comps,
                                  const fluid::FluidWindow& window, fluid::CellRect r,
                                  const fluid::DepthFn& depth, float t, std::size_t threads,
                                  const std::atomic<bool>* cancel) {
    // The floats are written as they are in memory.
    static_assert(std::endian::native == std::endian::little);
    std::vector<Piece> out;
    for (const auto& [x0, x1] : ring_split(r.x0, r.x1, window.size)) {
        for (const auto& [y0, y1] : ring_split(r.y0, r.y1, window.size)) {
            if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
                return out;
            }
            Piece p;
            p.pw = static_cast<std::uint32_t>(x1 - x0);
            p.ph = static_cast<std::uint32_t>(y1 - y0);
            std::tie(p.sx, p.sy) = window.storage(x0, y0);
            p.depth.resize(std::size_t{p.pw} * p.ph * 4);
            p.state.resize(std::size_t{p.pw} * p.ph * 16);
            const std::size_t rows = p.ph;
            const std::size_t per = std::max<std::size_t>(
                (rows + std::max<std::size_t>(threads, 1) - 1) / std::max<std::size_t>(threads, 1),
                1);
            std::vector<std::thread> pool;
            const std::int64_t bx = x0, by = y0;
            auto work = [&](std::size_t ra, std::size_t rb) {
                for (std::size_t row = ra; row < rb; ++row) {
                    if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
                        return;
                    }
                    const std::int64_t y = by + static_cast<std::int64_t>(row);
                    for (std::size_t col = 0; col < p.pw; ++col) {
                        const std::int64_t x = bx + static_cast<std::int64_t>(col);
                        const auto [px, py] = window.cell_centre(x, y);
                        const float d = depth(px, py);
                        const fluid::Cell s = fluid::reset_cell(cfg, comps, window, {x, y}, t, d);
                        const std::size_t i = row * p.pw + col;
                        std::memcpy(p.depth.data() + i * 4, &d, 4);
                        std::memcpy(p.state.data() + i * 16, s.data(), 16);
                    }
                }
            };
            for (std::size_t ra = 0; ra < rows; ra += per) {
                const std::size_t rb = std::min(rows, ra + per);
                if (ra + per >= rows) {
                    // The last band on this thread.
                    work(ra, rb);
                } else {
                    pool.emplace_back(work, ra, rb);
                }
            }
            for (auto& th : pool) {
                th.join();
            }
            out.push_back(std::move(p));
        }
    }
    return out;
}

/// Writes rows [r0, r1) of `p` into the depth and state textures.
void write_rows(WGPUQueue q, WGPUTexture bathy, WGPUTexture state, const Piece& p, std::uint32_t r0,
                std::uint32_t r1) {
    auto write = [&](WGPUTexture tex, const std::vector<std::uint8_t>& data, std::uint32_t bpp) {
        WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
        dst.texture = tex;
        dst.origin = {static_cast<std::uint32_t>(p.sx), static_cast<std::uint32_t>(p.sy) + r0, 0};
        WGPUTexelCopyBufferLayout lay = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
        lay.bytesPerRow = p.pw * bpp;
        lay.rowsPerImage = r1 - r0;
        const WGPUExtent3D ext = {p.pw, r1 - r0, 1};
        const std::size_t offset = std::size_t{r0} * p.pw * bpp;
        wgpuQueueWriteTexture(q, &dst, data.data() + offset, std::size_t{r1 - r0} * p.pw * bpp,
                              &lay, &ext);
    };
    write(bathy, p.depth, 4);
    write(state, p.state, 16);
}

} // namespace

/// A window (or strips of new cells) being prepared off the window thread.
struct FluidGpu::Job {
    /// What the thread needs, copied so it never touches the FluidGpu.
    fluid::FluidConfig cfg;
    fluid::Swell comps{};
    fluid::DepthFn depth;
    /// The window to switch to; its group and size.
    fluid::FluidWindow window;
    std::size_t group = 0;
    std::uint32_t size = 0;
    /// A whole new grid (zoom group or size change, jump) or strips of new
    /// cells written into the grid in use.
    bool whole = false;
    /// Simulation time (s) the cells are initialised at.
    float t = 0.0F;
    std::vector<fluid::CellRect> rects;
    std::vector<Piece> pieces;
    std::atomic<bool> done{false}, cancel{false};
    std::thread thread;
    /// Upload progress of a whole grid: the grid and the next piece / row.
    std::optional<Grid> grid;
    std::size_t next_piece = 0;
    std::uint32_t next_row = 0;

    Job() = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    ~Job() {
        cancel.store(true);
        if (thread.joinable()) {
            thread.join();
        }
    }
};

FluidGpu::FluidGpu() = default;
FluidGpu::~FluidGpu() = default;
FluidGpu::FluidGpu(FluidGpu&&) noexcept = default;
FluidGpu& FluidGpu::operator=(FluidGpu&&) noexcept = default;

void FluidGpu::set_background(bool on) {
    if (!on) {
        // A job in flight belongs to the background mode: drop it (the
        // window in use stays).
        retire_job();
    }
    background_ = on;
}

namespace {

/// Hands the data of a finished job to a short-lived thread that frees it:
/// at 4K the pieces are tens of megabytes, and returning that memory to the
/// system costs milliseconds the window thread must not spend.
void free_in_background(std::vector<Piece>&& pieces) {
    if (!pieces.empty()) {
        std::thread([p = std::move(pieces)] {}).detach();
    }
}

} // namespace

void FluidGpu::retire_job() {
    if (!job_) {
        return;
    }
    // Cancelled, and joined later: a thread may be in the middle of
    // allocating tens of megabytes, which the window thread must not wait for.
    job_->cancel.store(true);
    retired_.push_back(std::move(job_));
}

void FluidGpu::prepare_window(std::size_t group, std::pair<float, float> centre, std::uint32_t size,
                              double target, const fluid::DepthFn& depth, std::size_t threads) {
    // Cancelled jobs are freed once their threads have stopped.
    std::erase_if(retired_, [](const std::unique_ptr<Job>& j) {
        if (!j->done.load(std::memory_order_acquire)) {
            return false;
        }
        free_in_background(std::move(j->pieces));
        return true;
    });
    // 1. A window for another group or size makes the one being prepared
    //    useless.
    if (job_ && job_->whole && (job_->group != group || job_->size != size)) {
        retire_job();
    }
    // 2. Upload a finished job (a whole grid over several frames).
    if (job_ && job_->done.load(std::memory_order_acquire) && apply_job(*job_)) {
        free_in_background(std::move(job_->pieces));
        job_.reset();
    }
    if (job_) {
        return;
    }
    // 3. Start the next one if the window has to change.
    auto job = std::make_unique<Job>();
    job->cfg = cfg_;
    job->comps = comps_;
    job->depth = depth;
    if (!window_ || group != group_ || size != grid_.n) {
        job->whole = true;
        job->window = fluid::FluidWindow::with_size(cfg_, group, centre, size);
        job->group = group;
        job->size = size;
        job->t = static_cast<float>(target);
        job->rects = {job->window.rect()};
    } else {
        fluid::FluidWindow moved = *window_;
        job->rects = moved.recentre(centre, cfg_.recentre_cells);
        if (job->rects.empty()) {
            return;
        }
        job->window = moved;
        job->group = group_;
        job->size = size;
        // A jump (the whole window is new) still needs fresh cells for all
        // of it: a new grid, so the one in use stays valid meanwhile.
        job->whole = job->rects.size() == 1 && job->rects.front() == moved.rect();
        job->t = static_cast<float>(job->whole ? target : sim_t_);
    }
    const std::size_t workers = std::max<std::size_t>(threads / 2, 1);
    Job* j = job.get();
    j->thread = std::thread([j, workers] {
        for (const auto& r : j->rects) {
            auto pieces =
                compute_pieces(j->cfg, j->comps, j->window, r, j->depth, j->t, workers, &j->cancel);
            for (auto& p : pieces) {
                j->pieces.push_back(std::move(p));
            }
        }
        j->done.store(true, std::memory_order_release);
    });
    job_ = std::move(job);
}

bool FluidGpu::apply_job(Job& j) {
    const WGPUQueue q = ref_->queue.get();
    if (!j.whole) {
        for (const auto& p : j.pieces) {
            write_rows(q, grid_.bathy.get(), grid_.states[0].get(), p, 0, p.ph);
        }
        window_ = j.window;
        return true;
    }
    if (!j.grid) {
        if (spare_ && spare_->n == j.size) {
            // The whole grid is overwritten below; commands still reading the
            // old contents are ordered before these writes.
            j.grid = std::move(spare_);
            spare_.reset();
        } else {
            j.grid = make_grid(j.size);
        }
    }
    // Slices of at most 1 MiB until the byte or time budget of the frame is
    // spent (the first slice always goes).
    constexpr std::size_t kSlice = std::size_t{1} << 20;
    const auto begin = std::chrono::steady_clock::now();
    std::size_t budget = std::max<std::size_t>(upload_bytes_per_frame, 1);
    bool first = true;
    while (j.next_piece < j.pieces.size() && budget > 0) {
        if (!first && upload_budget_ms > 0.0 &&
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                    .count() >= upload_budget_ms) {
            break;
        }
        first = false;
        const Piece& p = j.pieces[j.next_piece];
        const std::size_t row_bytes = std::size_t{p.pw} * 20;
        const auto rows = static_cast<std::uint32_t>(
            std::clamp<std::size_t>(std::min(budget, kSlice) / row_bytes, 1, p.ph - j.next_row));
        write_rows(q, j.grid->bathy.get(), j.grid->states[0].get(), p, j.next_row,
                   j.next_row + rows);
        j.next_row += rows;
        budget -= std::min(budget, std::size_t{rows} * row_bytes);
        if (j.next_row >= p.ph) {
            ++j.next_piece;
            j.next_row = 0;
        }
    }
    if (j.next_piece < j.pieces.size()) {
        return false;
    }
    spare_ = std::move(grid_);
    grid_ = std::move(*j.grid);
    window_ = j.window;
    group_ = j.group;
    sim_t_ = static_cast<double>(j.t);
    return true;
}

void FluidGpu::fill(const fluid::FluidWindow& window, fluid::CellRect r,
                    const fluid::DepthFn& depth, float t, std::size_t threads) const {
    const WGPUQueue q = ref_->queue.get();
    for (const auto& p : compute_pieces(cfg_, comps_, window, r, depth, t, threads, nullptr)) {
        write_rows(q, grid_.bathy.get(), grid_.states[0].get(), p, 0, p.ph);
    }
}

} // namespace anno::render::gpu
