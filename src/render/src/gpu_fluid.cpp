#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <anno/render/gpu_fluid.hpp>
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
    FluidGpu f;
    f.ref_ = std::make_unique<DeviceRef>(dev);
    const WGPUDevice d = dev.device.get();
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
    WGPULimits limits = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(d, &limits);
    f.align_ = std::max<std::uint64_t>(limits.minUniformBufferOffsetAlignment, kStepBytes);
    f.steps_ = uniform_buffer(d, "fluid steps", f.align_ * std::max(cfg.max_steps, 1U));
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
    const fluid::FluidWindow window = *window_;
    const auto now = static_cast<float>(sim_t_);
    for (const auto& r : new_rects) {
        fill(window, r, depth, now, threads);
    }
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

void FluidGpu::fill(const fluid::FluidWindow& window, fluid::CellRect r,
                    const fluid::DepthFn& depth, float t, std::size_t threads) const {
    const auto w = static_cast<std::size_t>(r.x1 - r.x0);
    const auto h = static_cast<std::size_t>(r.y1 - r.y0);
    std::vector<float> d(w * h, 0.0F);
    std::vector<fluid::Cell> s(w * h, fluid::Cell{});
    const std::size_t rows = std::max<std::size_t>(
        (h + std::max<std::size_t>(threads, 1) - 1) / std::max<std::size_t>(threads, 1), 1);
    {
        std::vector<std::jthread> pool;
        for (std::size_t y0 = 0; y0 < h; y0 += rows) {
            pool.emplace_back([&, y0] {
                const std::size_t y1 = std::min(h, y0 + rows);
                for (std::size_t i = y0 * w; i < y1 * w; ++i) {
                    const std::int64_t x = r.x0 + static_cast<std::int64_t>(i % w);
                    const std::int64_t y = r.y0 + static_cast<std::int64_t>(i / w);
                    const auto [px, py] = window.cell_centre(x, y);
                    d[i] = depth(px, py);
                    s[i] = fluid::reset_cell(cfg_, comps_, window, {x, y}, t, d[i]);
                }
            });
        }
    }
    const std::int64_t n = window.size;
    // Split the rectangle where it wraps around the storage.
    auto split = [n](std::int64_t a0, std::int64_t a1) {
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
    };
    for (const auto& [x0, x1] : split(r.x0, r.x1)) {
        for (const auto& [y0, y1] : split(r.y0, r.y1)) {
            const auto pw = static_cast<std::size_t>(x1 - x0);
            const auto ph = static_cast<std::size_t>(y1 - y0);
            std::vector<std::uint8_t> db, sb;
            db.reserve(pw * ph * 4);
            sb.reserve(pw * ph * 16);
            for (std::int64_t y = y0; y < y1; ++y) {
                const std::size_t row = static_cast<std::size_t>(y - r.y0) * w;
                const auto col = static_cast<std::size_t>(x0 - r.x0);
                for (std::size_t i = row + col; i < row + col + pw; ++i) {
                    put_f32(db, d[i]);
                    for (const float c : s[i]) {
                        put_f32(sb, c);
                    }
                }
            }
            const auto [sx, sy] = window.storage(x0, y0);
            auto write = [&](WGPUTexture tex, const std::vector<std::uint8_t>& data,
                             std::uint32_t bpp) {
                WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
                dst.texture = tex;
                dst.origin = {static_cast<std::uint32_t>(sx), static_cast<std::uint32_t>(sy), 0};
                WGPUTexelCopyBufferLayout lay = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
                lay.bytesPerRow = static_cast<std::uint32_t>(pw) * bpp;
                lay.rowsPerImage = static_cast<std::uint32_t>(ph);
                const WGPUExtent3D ext = {static_cast<std::uint32_t>(pw),
                                          static_cast<std::uint32_t>(ph), 1};
                wgpuQueueWriteTexture(ref_->queue.get(), &dst, data.data(), data.size(), &lay,
                                      &ext);
            };
            write(grid_.bathy.get(), db, 4);
            write(grid_.states[0].get(), sb, 16);
        }
    }
}

} // namespace anno::render::gpu
