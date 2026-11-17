// RENDER-DIRECT-1c: see atlas_upload.hpp. A standalone unit: it opens no device of its own (the
// caller's Context), and touches no GpuFrame state.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <anno/render/atlas_upload.hpp>
#include <anno/render/shaders.hpp>

namespace anno::render::gpu {

namespace {

Texture make_texture_2mip(WGPUDevice d, std::string_view label, WGPUTextureFormat format,
                          std::uint32_t size, std::uint32_t layers, WGPUTextureUsage usage) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.usage = usage;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {size, size, layers};
    desc.format = format;
    desc.mipLevelCount = 2;
    desc.sampleCount = 1;
    return Texture(wgpuDeviceCreateTexture(d, &desc));
}

Texture make_texture_1mip(WGPUDevice d, std::string_view label, WGPUTextureFormat format,
                          std::uint32_t w, std::uint32_t h, WGPUTextureUsage usage) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.label = sv(label);
    desc.usage = usage;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {w, h, 1};
    desc.format = format;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    return Texture(wgpuDeviceCreateTexture(d, &desc));
}

TextureView make_view_array(WGPUTexture t) {
    WGPUTextureViewDescriptor desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    desc.dimension = WGPUTextureViewDimension_2DArray;
    return TextureView(wgpuTextureCreateView(t, &desc));
}

TextureView make_view_2d(WGPUTexture t) {
    WGPUTextureViewDescriptor desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    desc.dimension = WGPUTextureViewDimension_2D;
    return TextureView(wgpuTextureCreateView(t, &desc));
}

/// Finishes and submits `enc`; false (and the error printed) if the device rejected it
/// (gpu.cpp's own `submit`, copied here: this unit has no GpuFrame::Impl to call it on).
bool submit(const Context& ctx, WGPUCommandEncoder enc, const char* what) {
    wgpuDevicePushErrorScope(ctx.device.get(), WGPUErrorFilter_Validation);
    WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    CommandBuffer cmd(wgpuCommandEncoderFinish(enc, &cd));
    if (const std::string err = pop_error_scope(ctx.instance.get(), ctx.device.get());
        !err.empty()) {
        std::fprintf(stderr, "%s not submitted: %s\n", what, err.c_str());
        ctx.errors->fetch_add(1);
        return false;
    }
    const WGPUCommandBuffer raw = cmd.get();
    wgpuQueueSubmit(ctx.queue.get(), 1, &raw);
    return true;
}

/// A level's own side length: `page_size` at level 0, `page_size / 2` at level 1
/// (AtlasPages::level1's own layout, atlas_pages.hpp).
std::uint32_t level_size(std::uint32_t page_size, std::uint32_t level) {
    return level == 0 ? page_size : page_size / 2;
}

} // namespace

std::vector<std::uint32_t> resident_atlas_groups(WGPUAdapterType adapter_type,
                                                 std::uint32_t zoom_group,
                                                 std::uint32_t group_count) {
    std::vector<std::uint32_t> out;
    if (group_count == 0) {
        return out;
    }
    if (adapter_type == WGPUAdapterType_DiscreteGPU) {
        out.reserve(group_count);
        for (std::uint32_t g = 0; g < group_count; ++g) {
            out.push_back(g);
        }
        return out;
    }
    if (group_count == 1) {
        out.push_back(0);
        return out;
    }
    const std::uint32_t lo = std::min(zoom_group, group_count - 2);
    out.push_back(lo);
    out.push_back(lo + 1);
    return out;
}

std::uint64_t atlas_pages_bytes(const AtlasPages& pages) {
    const std::uint64_t l0 = std::uint64_t{pages.page_size} * pages.page_size * 4;
    const std::uint64_t half = pages.page_size / 2;
    const std::uint64_t l1 = half * half * 4;
    return (l0 + l1) * pages.level0.size();
}

std::optional<std::string> atlas_upload_refusal(std::uint32_t layers, std::uint32_t page_size,
                                                std::uint32_t max_array_layers,
                                                std::uint32_t max_dimension_2d,
                                                std::uint64_t needed_bytes,
                                                std::uint64_t budget_bytes) {
    if (layers == 0) {
        return std::string("upload_atlas_pages: no pages to upload");
    }
    if (layers > max_array_layers) {
        return "upload_atlas_pages: " + std::to_string(layers) + " layers exceed the device's " +
               std::to_string(max_array_layers) + " maxTextureArrayLayers";
    }
    if (page_size == 0 || page_size > max_dimension_2d) {
        return "upload_atlas_pages: page size " + std::to_string(page_size) +
               " exceeds the device's " + std::to_string(max_dimension_2d) +
               " maxTextureDimension2D";
    }
    if (needed_bytes > budget_bytes) {
        return "upload_atlas_pages: needs " + std::to_string(needed_bytes) +
               " bytes, budget allows " + std::to_string(budget_bytes);
    }
    return std::nullopt;
}

formats::Result<AtlasTexture, std::string>
upload_atlas_pages(const Context& ctx, const AtlasPages& pages, std::uint64_t budget_bytes) {
    const auto layers = static_cast<std::uint32_t>(pages.level0.size());
    WGPULimits limits = WGPU_LIMITS_INIT;
    wgpuDeviceGetLimits(ctx.device.get(), &limits);
    if (const auto refusal = atlas_upload_refusal(
            layers, pages.page_size, limits.maxTextureArrayLayers, limits.maxTextureDimension2D,
            atlas_pages_bytes(pages), budget_bytes)) {
        return *refusal;
    }

    // The device can still refuse the allocation even though it fits the calculated budget (a
    // single large resource can exceed a real allocator's own ceiling below the device's total
    // reported memory - found exercising RENDER-DIRECT-1c review F1's real worst case on the
    // Radeon iGPU: `wgpuDeviceCreateTexture` returned a non-null but device-invalidated handle,
    // "Not enough memory left", silently accepted by every call below before this error-scope
    // check existed - a correctness gap, not merely a reporting one, since the caller would have
    // gone on to read back an invalid texture and seen a byte mismatch instead of a named
    // refusal). Every device call that can fail is made inside one scope so any of them is
    // caught, matching `submit`'s own pattern above.
    AtlasTexture out;
    out.page_size = pages.page_size;
    out.layers = layers;
    wgpuDevicePushErrorScope(ctx.device.get(), WGPUErrorFilter_OutOfMemory);
    wgpuDevicePushErrorScope(ctx.device.get(), WGPUErrorFilter_Validation);
    out.texture = make_texture_2mip(
        ctx.device.get(), "resident-atlas", WGPUTextureFormat_RGBA8Unorm, pages.page_size, layers,
        WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc);
    out.view = make_view_array(out.texture.get());

    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        for (std::uint32_t level = 0; level < 2; ++level) {
            const auto& src = level == 0 ? pages.level0[layer] : pages.level1[layer];
            const std::uint32_t size = level_size(pages.page_size, level);
            if (src.size() != std::size_t{size} * size * 4) {
                continue; // malformed page (should not happen; refuse nothing silently wrong)
            }
            WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
            dst.texture = out.texture.get();
            dst.mipLevel = level;
            dst.origin = {0, 0, layer};
            dst.aspect = WGPUTextureAspect_All;
            WGPUTexelCopyBufferLayout lay = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
            lay.offset = 0;
            lay.bytesPerRow = size * 4;
            lay.rowsPerImage = size;
            const WGPUExtent3D ext = {size, size, 1};
            wgpuQueueWriteTexture(ctx.queue.get(), &dst, src.data(), src.size(), &lay, &ext);
        }
    }
    const std::string validation_err = pop_error_scope(ctx.instance.get(), ctx.device.get());
    const std::string oom_err = pop_error_scope(ctx.instance.get(), ctx.device.get());
    if (!validation_err.empty() || !oom_err.empty()) {
        return "upload_atlas_pages: device refused the allocation (" + std::to_string(layers) +
               " layers, " + std::to_string(atlas_pages_bytes(pages)) + " bytes needed, " +
               std::to_string(budget_bytes) +
               " bytes budgeted - the real device, not the "
               "calculated budget, refused this one): " +
               (oom_err.empty() ? validation_err : oom_err);
    }
    return out;
}

std::vector<std::uint8_t> read_back_level(const Context& ctx, const AtlasTexture& tex,
                                          std::uint32_t layer, std::uint32_t level) {
    if (layer >= tex.layers || level > 1 || tex.texture.get() == nullptr) {
        return {};
    }
    const std::uint32_t size = level_size(tex.page_size, level);
    const std::uint32_t row = (4 * size + 255) / 256 * 256;
    Buffer buf;
    {
        WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
        desc.label = sv("atlas read back");
        desc.size = std::uint64_t{row} * size;
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        buf = Buffer(wgpuDeviceCreateBuffer(ctx.device.get(), &desc));
    }
    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(ctx.device.get(), &ed));
    WGPUTexelCopyTextureInfo src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    src.texture = tex.texture.get();
    src.mipLevel = level;
    src.origin = {0, 0, layer};
    src.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo dst = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    dst.buffer = buf.get();
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = row;
    dst.layout.rowsPerImage = size;
    const WGPUExtent3D ext = {size, size, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc.get(), &src, &dst, &ext);
    if (!submit(ctx, enc.get(), "atlas read-back")) {
        return {};
    }
    if (!map_read(ctx.instance.get(), ctx.device.get(), buf.get(), std::size_t{row} * size)) {
        return {};
    }
    std::vector<std::uint8_t> out;
    out.reserve(std::size_t{4} * size * size);
    const auto* data = static_cast<const std::uint8_t*>(
        wgpuBufferGetConstMappedRange(buf.get(), 0, std::size_t{row} * size));
    if (data != nullptr) {
        for (std::uint32_t y = 0; y < size; ++y) {
            out.insert(out.end(), data + std::size_t{y} * row,
                       data + std::size_t{y} * row + std::size_t{4} * size);
        }
    }
    wgpuBufferUnmap(buf.get());
    return out;
}

std::vector<std::uint8_t> sample_placement(const Context& ctx, const AtlasTexture& tex,
                                           const AtlasFramePlacement& place, std::uint32_t level) {
    if (level > 1 || place.layer >= tex.layers || tex.texture.get() == nullptr) {
        return {};
    }
    const std::uint32_t w = level == 0 ? place.width : place.mip_width;
    const std::uint32_t h = level == 0 ? place.height : place.mip_height;
    const std::int32_t origin_x =
        level == 0 ? static_cast<std::int32_t>(place.x) : static_cast<std::int32_t>(place.mip_x);
    const std::int32_t origin_y =
        level == 0 ? static_cast<std::int32_t>(place.y) : static_cast<std::int32_t>(place.mip_y);
    if (w == 0 || h == 0) {
        return {};
    }

    auto shader = create_shader(ctx, shaders::atlas_upload(), "atlas_upload.wgsl");
    if (!shader) {
        std::fprintf(stderr, "atlas_upload.wgsl: %s\n", shader.error().c_str());
        return {};
    }
    const WGPUDevice d = ctx.device.get();

    WGPUBindGroupLayoutEntry uniform_entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    uniform_entry.binding = 0;
    uniform_entry.visibility = WGPUShaderStage_Fragment;
    uniform_entry.buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutEntry texture_entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    texture_entry.binding = 1;
    texture_entry.visibility = WGPUShaderStage_Fragment;
    texture_entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    texture_entry.texture.viewDimension = WGPUTextureViewDimension_2DArray;
    texture_entry.texture.multisampled = 0;
    const std::array entries = {uniform_entry, texture_entry};
    WGPUBindGroupLayoutDescriptor bgl_desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bgl_desc.label = sv("atlas-sample");
    bgl_desc.entryCount = entries.size();
    bgl_desc.entries = entries.data();
    BindGroupLayout bgl(wgpuDeviceCreateBindGroupLayout(d, &bgl_desc));

    const std::array bgls = {bgl.get()};
    WGPUPipelineLayoutDescriptor pld = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pld.bindGroupLayoutCount = bgls.size();
    pld.bindGroupLayouts = bgls.data();
    PipelineLayout pl(wgpuDeviceCreatePipelineLayout(d, &pld));

    WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
    ct.format = WGPUTextureFormat_RGBA8Unorm;
    ct.blend = nullptr; // no blending: the target receives the sampled texel as written
    ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState frag = WGPU_FRAGMENT_STATE_INIT;
    frag.module = shader.value().get();
    frag.entryPoint = sv("atlas_sample_fs");
    frag.targetCount = 1;
    frag.targets = &ct;
    WGPURenderPipelineDescriptor pipe_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipe_desc.label = sv("atlas-sample");
    pipe_desc.layout = pl.get();
    pipe_desc.vertex.module = shader.value().get();
    pipe_desc.vertex.entryPoint = sv("atlas_sample_vs");
    pipe_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipe_desc.multisample.count = 1;
    pipe_desc.multisample.mask = 0xFFFFFFFFU;
    pipe_desc.fragment = &frag;
    RenderPipeline pipe(wgpuDeviceCreateRenderPipeline(d, &pipe_desc));

    WGPUBufferDescriptor ubuf_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    ubuf_desc.label = sv("atlas-sample params");
    ubuf_desc.size = 16;
    ubuf_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    Buffer ubuf(wgpuDeviceCreateBuffer(d, &ubuf_desc));
    const std::array<std::int32_t, 4> params = {origin_x, origin_y,
                                                static_cast<std::int32_t>(place.layer),
                                                static_cast<std::int32_t>(level)};
    wgpuQueueWriteBuffer(ctx.queue.get(), ubuf.get(), 0, params.data(), params.size() * 4);

    WGPUBindGroupEntry ubuf_entry = WGPU_BIND_GROUP_ENTRY_INIT;
    ubuf_entry.binding = 0;
    ubuf_entry.buffer = ubuf.get();
    ubuf_entry.offset = 0;
    ubuf_entry.size = 16;
    WGPUBindGroupEntry view_entry = WGPU_BIND_GROUP_ENTRY_INIT;
    view_entry.binding = 1;
    view_entry.textureView = tex.view.get();
    const std::array bg_entries = {ubuf_entry, view_entry};
    WGPUBindGroupDescriptor bg_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bg_desc.label = sv("atlas-sample");
    bg_desc.layout = bgl.get();
    bg_desc.entryCount = bg_entries.size();
    bg_desc.entries = bg_entries.data();
    BindGroup bind(wgpuDeviceCreateBindGroup(d, &bg_desc));

    Texture target =
        make_texture_1mip(d, "atlas-sample-target", WGPUTextureFormat_RGBA8Unorm, w, h,
                          WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc);
    TextureView target_view = make_view_2d(target.get());

    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    CommandEncoder enc(wgpuDeviceCreateCommandEncoder(d, &ed));
    WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    ca.view = target_view.get();
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = {0, 0, 0, 0};
    WGPURenderPassDescriptor rp_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    rp_desc.colorAttachmentCount = 1;
    rp_desc.colorAttachments = &ca;
    WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(enc.get(), &rp_desc);
    wgpuRenderPassEncoderSetPipeline(rp, pipe.get());
    wgpuRenderPassEncoderSetBindGroup(rp, 0, bind.get(), 0, nullptr);
    wgpuRenderPassEncoderSetViewport(rp, 0, 0, static_cast<float>(w), static_cast<float>(h), 0, 1);
    wgpuRenderPassEncoderDraw(rp, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(rp);
    wgpuRenderPassEncoderRelease(rp);
    if (!submit(ctx, enc.get(), "atlas sample")) {
        return {};
    }

    const std::uint32_t row = (4 * w + 255) / 256 * 256;
    Buffer readback;
    {
        WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
        desc.label = sv("atlas-sample read back");
        desc.size = std::uint64_t{row} * h;
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
        readback = Buffer(wgpuDeviceCreateBuffer(d, &desc));
    }
    WGPUCommandEncoderDescriptor ed2 = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    CommandEncoder enc2(wgpuDeviceCreateCommandEncoder(d, &ed2));
    WGPUTexelCopyTextureInfo rb_src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    rb_src.texture = target.get();
    WGPUTexelCopyBufferInfo rb_dst = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    rb_dst.buffer = readback.get();
    rb_dst.layout.offset = 0;
    rb_dst.layout.bytesPerRow = row;
    rb_dst.layout.rowsPerImage = h;
    const WGPUExtent3D rb_ext = {w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc2.get(), &rb_src, &rb_dst, &rb_ext);
    if (!submit(ctx, enc2.get(), "atlas sample read-back")) {
        return {};
    }
    if (!map_read(ctx.instance.get(), d, readback.get(), std::size_t{row} * h)) {
        return {};
    }
    std::vector<std::uint8_t> out;
    out.reserve(std::size_t{4} * w * h);
    const auto* data = static_cast<const std::uint8_t*>(
        wgpuBufferGetConstMappedRange(readback.get(), 0, std::size_t{row} * h));
    if (data != nullptr) {
        for (std::uint32_t y = 0; y < h; ++y) {
            out.insert(out.end(), data + std::size_t{y} * row,
                       data + std::size_t{y} * row + std::size_t{4} * w);
        }
    }
    wgpuBufferUnmap(readback.get());
    return out;
}

} // namespace anno::render::gpu
