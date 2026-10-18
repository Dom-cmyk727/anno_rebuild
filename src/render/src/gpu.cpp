#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>

#include <anno/render/gpu.hpp>
#include <anno/render/gpu_fluid.hpp>
#include <anno/render/shaders.hpp>

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
    // tile_w, tile_h, raise, then _pad (vec2, 8-aligned: offsets 56, 60).
    for (const float v : {f.tile_w, f.tile_h, f.raise, 0.0F, 0.0F}) {
        u.f32(v);
    }
    u.i32(f.fluid_origin.first);
    u.i32(f.fluid_origin.second);
    u.i32(f.fluid_size);
    u.u32(f.fluid_on ? 1U : 0U);
    for (const float v : {f.fluid_cpt, 0.0F, 0.0F, 0.0F}) {
        u.f32(v);
    }
    return std::move(u.b);
}

struct GpuFrame::Impl {
    /// Own references to the device (the Context may move or go away).
    std::unique_ptr<DeviceRef> ref;
    GpuLimits limits;
    /// The device's maxTextureArrayLayers (atlas growth stops there).
    std::uint32_t max_layers = 1;
    bool atlas_full_reported = false;
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
    ShaderModule module;
    BindGroupLayout frame_layout, chunk_layout, atlas_layout, empty_layout;
    /// Bound at group 1 for the sprite draws, which use no chunk data.
    BindGroup empty_bind;
    PipelineLayout chunk_pl, sprite_pl;
    RenderPipeline ground_pipe, terrain_pipe, objects_pipe, sea_pipe, sprite_pipe;
    Buffer frame_uniform;
    /// Offscreen texture of pass 1 and its size; terrain height per pixel.
    Texture under_t, terrain_t;
    TextureView under_v, terrain_v;
    std::pair<std::uint32_t, std::uint32_t> under_size{0, 0};
    /// 1 × 1 stand-ins for unused bindings.
    std::pair<Texture, TextureView> dummy_color, dummy_field, dummy_terrain, dummy_depth,
        dummy_fluid, dummy_array;

    struct Sea {
        std::pair<int, int> size{0, 0};
        std::size_t steps = 0, uploaded = 0;
        std::uint64_t step_ms = 0;
        Texture sprite_t, filled_t;
        TextureView sprite_v, filled_v;
    };
    std::map<std::size_t, Sea> seas;
    std::optional<std::size_t> sea_group;

    struct Chunk {
        BindGroup bind;
        bool real = false;
        std::vector<Texture> textures;
        std::vector<TextureView> views;
        Buffer uniform;
    };
    std::map<ChunkKey, Chunk> chunks;

    /// (layer, x, y, width, height) of a frame image in the atlas.
    using Place = std::array<std::uint32_t, 5>;
    struct Atlas {
        Texture texture;
        TextureView view;
        BindGroup bind;
        std::uint32_t layers = 0;
        std::map<SpriteKey, std::optional<Place>> place;
        /// Packing cursor: layer, x, y, height of the current shelf.
        std::array<std::uint32_t, 4> cursor{};
    };
    std::optional<Atlas> atlas;
    Buffer instances;
    std::uint64_t instances_size = 0;
    /// The wave simulation, created on the first enhanced frame.
    std::optional<FluidGpu> fluid;
    bool fluid_failed = false;

    WGPUDevice dev() const { return ref->device.get(); }
    WGPUQueue queue() const { return ref->queue.get(); }

    /// Finishes `enc` and submits it, unless recording raised a validation
    /// error: wgpu-native 29 aborts the process when an invalid command
    /// buffer is submitted, so the error is counted (errors()) and the work
    /// dropped instead. False if nothing was submitted.
    bool submit(WGPUCommandEncoder enc, const char* what) const {
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        wgpuDevicePushErrorScope(dev(), WGPUErrorFilter_Validation);
        CommandBuffer cmd(wgpuCommandEncoderFinish(enc, &cd));
        if (const std::string err = pop_error_scope(ref->instance.get(), dev()); !err.empty()) {
            std::fprintf(stderr, "%s not submitted: %s\n", what, err.c_str());
            ref->errors->fetch_add(1);
            return false;
        }
        const WGPUCommandBuffer raw = cmd.get();
        wgpuQueueSubmit(queue(), 1, &raw);
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

    /// Uploads up to `max` missing animation steps of `p` as the pattern of
    /// `group`; returns the number uploaded.
    std::size_t upload_sea_steps(std::size_t group, const sea::SeaPattern& p, std::size_t max) {
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
    /// are already rendered, so a zoom change finds them on the GPU.
    void ensure_sea(const Scene& scene, std::size_t group) {
        sea_group.reset();
        if (const sea::SeaPattern* p = scene.sea(group)) {
            upload_sea_steps(group, *p, SIZE_MAX);
            sea_group = group;
        }
        std::size_t budget = limits.sea_steps_per_frame;
        for (std::size_t g = 0; g < kGroups && budget > 0; ++g) {
            if (g == group) {
                continue;
            }
            if (const sea::SeaPattern* p = scene.sea_if_ready(g)) {
                budget -= upload_sea_steps(g, *p, budget);
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
        auto view = [&](WGPUTextureFormat fmt, std::uint32_t bpp,
                        const std::optional<std::vector<std::uint8_t>>& data, std::uint32_t bit,
                        WGPUTextureView dummy) -> WGPUTextureView {
            if (!data) {
                return dummy;
            }
            auto [t, v] = upload_texture(dev(), queue(), fmt, n, n, bpp, *data, false);
            flags |= bit;
            const WGPUTextureView raw = v.get();
            out.textures.push_back(std::move(t));
            out.views.push_back(std::move(v));
            return raw;
        };
        // Texel bytes written in place (a chunk is 512² texels: per-byte
        // push_back cost milliseconds per chunk).
        std::optional<std::vector<std::uint8_t>> ground, objects, field, terrain;
        if (chunk != nullptr && !chunk->ground.empty()) {
            ground.emplace(chunk->ground.size() * 4);
            std::uint8_t* o = ground->data();
            for (const std::uint32_t p : chunk->ground) {
                *o++ = static_cast<std::uint8_t>(p);
                *o++ = static_cast<std::uint8_t>(p >> 8);
                *o++ = static_cast<std::uint8_t>(p >> 16);
                *o++ = static_cast<std::uint8_t>(p >> 24);
            }
        }
        if (chunk != nullptr && !chunk->objects.empty()) {
            objects.emplace(chunk->objects.size() * 4);
            std::uint8_t* o = objects->data();
            for (const Px& p : chunk->objects) {
                o = std::copy(p.begin(), p.end(), o);
            }
        }
        if (chunk != nullptr && !chunk->cover.empty()) {
            // Rust zips the four planes (shortest wins); they have one
            // entry per texel each.
            const std::size_t k = std::min({chunk->cover.size(), chunk->shore.size(),
                                            chunk->depth.size(), chunk->flags.size()});
            field.emplace(k * 4);
            std::uint8_t* o = field->data();
            for (std::size_t i = 0; i < k; ++i) {
                *o++ = chunk->cover[i];
                *o++ = chunk->shore[i];
                *o++ = chunk->depth[i];
                *o++ = chunk->flags[i];
            }
        }
        if (chunk != nullptr && !chunk->terrain.empty()) {
            terrain.emplace(chunk->terrain.size() * 2);
            std::uint8_t* o = terrain->data();
            for (const std::uint16_t v : chunk->terrain) {
                *o++ = static_cast<std::uint8_t>(v & 0xFF);
                *o++ = static_cast<std::uint8_t>(v >> 8);
            }
        }
        // 0RGB little-endian = B, G, R, 0.
        const WGPUTextureView gv =
            view(WGPUTextureFormat_BGRA8Unorm, 4, ground, 1, dummy_color.second.get());
        const WGPUTextureView ov =
            view(WGPUTextureFormat_RGBA8Unorm, 4, objects, 2, dummy_color.second.get());
        const WGPUTextureView fv =
            view(WGPUTextureFormat_RGBA8Uint, 4, field, 4, dummy_field.second.get());
        const WGPUTextureView tv = view(kTerrainFormat, 2, terrain, 8, dummy_terrain.second.get());
        out.uniform =
            make_buffer(dev(), "chunk", 16, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
        Bytes u;
        u.i32(r.x);
        u.i32(r.y);
        u.i32(kChunk);
        u.u32(flags);
        wgpuQueueWriteBuffer(queue(), out.uniform.get(), 0, u.b.data(), u.b.size());
        const std::array entries = {buffer_entry(0, out.uniform.get(), 16), view_entry(1, gv),
                                    view_entry(2, ov), view_entry(3, fv), view_entry(4, tv)};
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.label = sv("chunk");
        desc.layout = chunk_layout.get();
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        out.bind = BindGroup(wgpuDeviceCreateBindGroup(dev(), &desc));
        out.real = chunk != nullptr;
        return out;
    }

    /// Uploads newly built visible chunks (a few per frame), adds stand-ins
    /// for missing ones, and drops GPU chunks the CPU cache no longer holds.
    void sync_chunks(const FrameRenderer& frames, std::size_t group,
                     const std::vector<std::pair<int, int>>& visible) {
        std::set<ChunkKey> keys;
        for (const auto& [cx, cy] : visible) {
            keys.insert({group, cx, cy});
        }
        for (auto it = chunks.begin(); it != chunks.end();) {
            const bool keep =
                it->second.real ? frames.chunk(it->first) != nullptr : keys.contains(it->first);
            it = keep ? std::next(it) : chunks.erase(it);
        }
        std::size_t uploads = 0;
        for (const auto& key : keys) {
            const auto it = chunks.find(key);
            if (it != chunks.end() && it->second.real) {
                continue;
            }
            const render::Chunk* c = frames.chunk(key);
            if (c != nullptr && uploads < limits.uploads_per_frame) {
                ++uploads;
                chunks.insert_or_assign(key, upload_chunk(key, c));
            } else if (it == chunks.end()) {
                chunks.emplace(key, upload_chunk(key, nullptr));
            }
        }
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

    /// The atlas position of a sprite frame, packing it on first use.
    std::optional<Place> atlas_place(const Scene& scene, const SpriteDraw& d) {
        if (atlas) {
            if (const auto it = atlas->place.find(d.key); it != atlas->place.end()) {
                return it->second;
            }
        }
        std::optional<Place> place;
        if (const auto img = scene.sprites.get(scene.anlagen, d.key, *d.frame)) {
            const std::uint32_t w = img->width, h = img->height;
            if (w != 0 && h != 0 && w <= limits.atlas_size && h <= limits.atlas_size) {
                if (const auto at = atlas_alloc(w, h)) {
                    const auto [layer, x, y] = *at;
                    write_layer(queue(), atlas->texture.get(), layer, x, y, w, h, 4,
                                img->pixels.data(), img->pixels.size());
                    place = Place{layer, x, y, w, h};
                }
            }
        }
        if (atlas) {
            atlas->place.insert_or_assign(d.key, place);
        }
        return place;
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
                             const Scene& scene, bool fluid_on) {
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

WGPURenderPassEncoder begin(WGPUCommandEncoder enc, WGPUTextureView view, WGPUColor clear) {
    WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    ca.view = view;
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = clear;
    WGPURenderPassDescriptor desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &ca;
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
                texture_entry(6, kFloat, WGPUTextureViewDimension_2D)});
    m.chunk_layout =
        layout(d, "chunk",
               {uniform_entry(0), texture_entry(1, kFloat, WGPUTextureViewDimension_2D),
                texture_entry(2, kFloat, WGPUTextureViewDimension_2D),
                texture_entry(3, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D),
                texture_entry(4, WGPUTextureSampleType_Uint, WGPUTextureViewDimension_2D)});
    m.atlas_layout =
        layout(d, "atlas", {texture_entry(0, kFloat, WGPUTextureViewDimension_2DArray)});
    {
        const std::array layouts = {m.frame_layout.get(), m.chunk_layout.get()};
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
    m.ground_pipe = m.pipeline("ground", m.chunk_pl.get(), "chunk_vs", "ground_fs", nullptr, 0,
                               format, nullptr);
    m.terrain_pipe = m.pipeline("terrain", m.chunk_pl.get(), "chunk_vs", "terrain_fs", nullptr, 0,
                                kTerrainFormat, nullptr);
    m.sea_pipe =
        m.pipeline("sea", m.chunk_pl.get(), "chunk_vs", "sea_fs", nullptr, 0, format, nullptr);
    m.objects_pipe = m.pipeline("objects", m.chunk_pl.get(), "chunk_vs", "objects_fs", nullptr, 0,
                                format, &alpha);
    m.sprite_pipe =
        m.pipeline("sprite", m.sprite_pl.get(), "sprite_vs", "sprite_fs", &vbl, 1, format, &alpha);
    if (const std::string err = pop_error_scope(c.instance.get(), d); !err.empty()) {
        return "pipelines: " + err;
    }
    m.frame_uniform = make_buffer(d, "frame", kFrameUniformBytes,
                                  WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst);
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
    m.dummy_fluid = upload_texture(d, q, WGPUTextureFormat_RGBA32Float, 1, 1, 16,
                                   std::vector<std::uint8_t>(16, 0), false);
    m.dummy_array = upload_texture(d, q, WGPUTextureFormat_RGBA8Unorm, 1, 1, 4, {0, 0, 0, 0}, true);
    return f;
}

GpuFrame::~GpuFrame() = default;

WGPUTextureFormat GpuFrame::format() const {
    return impl_->format;
}

std::uint64_t GpuFrame::errors() const {
    return impl_->ref->errors->load();
}

GpuStats GpuFrame::render(FrameRenderer& frames, const Scene& scene, Rect view, std::size_t group,
                          std::uint64_t time_ms, WaterStyle style, WGPUTextureView target) {
    Impl& m = *impl_;
    const auto start = Clock::now();
    GpuStats stats;
    std::tie(stats.chunks_built, stats.chunks_ms, stats.chunks_missing) =
        frames.update_chunks(scene, view, group);
    const auto w = static_cast<std::uint32_t>(std::max(view.w, 1));
    const auto h = static_cast<std::uint32_t>(std::max(view.h, 1));

    auto t = Clock::now();
    m.ensure_under(w, h);
    m.ensure_sea(scene, group);
    const auto visible = visible_chunks(view);
    m.sync_chunks(frames, group, visible);
    stats.upload_ms = ms_since(t);

    t = Clock::now();
    const auto below = scene.sprites_in(view, group, time_ms, SpriteLayer::WaterBelow, style);
    const auto above = scene.sprites_in(view, group, time_ms, SpriteLayer::WaterAbove, style);
    Bytes bytes;
    bytes.b.reserve((below.size() + above.size()) * kInstanceBytes);
    const std::uint32_t n_below = m.pack_sprites(scene, below, false, bytes);
    const std::uint32_t n_above = m.pack_sprites(scene, above, true, bytes);
    m.write_instances(bytes.b);
    stats.sprites = std::size_t{n_below} + n_above;
    stats.sprites_ms = ms_since(t);

    t = Clock::now();
    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    ed.label = sv("frame");
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(m.dev(), &ed));
    // Coastal wave simulation (enhanced water only).
    const bool fluid_on = style == WaterStyle::Enhanced && m.limits.wave_simulation &&
                          m.update_fluid(enc.get(), scene, view, group, time_ms);
    stats.wave_simulation = fluid_on;
    m.write_frame_uniform(view, group, time_ms, style, scene, fluid_on);
    const BindGroup pass1 = m.frame_bind(nullptr, nullptr);
    const BindGroup pass2 = m.frame_bind(m.under_v.get(), m.terrain_v.get());
    const Px water = scene.textures.water();
    const WGPUColor clear = {water[0] / 255.0, water[1] / 255.0, water[2] / 255.0, 1.0};
    std::vector<WGPUBindGroup> binds;
    for (const auto& [cx, cy] : visible) {
        if (const auto it = m.chunks.find({group, cx, cy}); it != m.chunks.end()) {
            binds.push_back(it->second.bind.get());
        }
    }
    const WGPUBindGroup atlas = m.atlas ? m.atlas->bind.get() : nullptr;
    auto draw_chunks = [&](WGPURenderPassEncoder p) {
        for (const WGPUBindGroup b : binds) {
            wgpuRenderPassEncoderSetBindGroup(p, 1, b, 0, nullptr);
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
        draw_chunks(p);
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
    {
        WGPURenderPassEncoder p = begin(enc.get(), target, clear);
        wgpuRenderPassEncoderSetBindGroup(p, 0, pass2.get(), 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(p, m.sea_pipe.get());
        draw_chunks(p);
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
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    m.submit(enc.get(), "frame");
    stats.encode_ms = ms_since(t);
    stats.total_ms = ms_since(start);
    return stats;
}

void GpuFrame::wait() const {
    wait_idle(impl_->ref->instance.get(), impl_->dev(), impl_->queue());
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
