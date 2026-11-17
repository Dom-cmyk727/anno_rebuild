// RENDER-DIRECT-1c step 3: draws one placement's own rectangle (or its mip rectangle) out of the
// resident atlas texture array, one texel at a time, with no sampler and no blending -
// `textureLoad` only, as every other pass in gpu.wgsl ("textureLoad, never filtered, so cached
// chunks and sprites stay exact"). This is a separate shader module from gpu.wgsl: a new unit,
// not a change to the chunk compositor.

struct AtlasSampleParams {
    origin_x: i32,
    origin_y: i32,
    layer: i32,
    level: i32,
};
@group(0) @binding(0) var<uniform> p: AtlasSampleParams;
@group(0) @binding(1) var atlas_tex: texture_2d_array<f32>;

@vertex
fn atlas_sample_vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4<f32> {
    // One triangle over the whole target (scale_vs in gpu.wgsl, the same trick): (0, 0), (2, 0),
    // (0, 2) in unit space, covering every pixel of a target sized exactly to the rectangle drawn.
    let q = vec2<f32>(f32((i << 1u) & 2u), f32(i & 2u));
    return vec4<f32>(q.x * 2.0 - 1.0, 1.0 - q.y * 2.0, 0.0, 1.0);
}

@fragment
fn atlas_sample_fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    // pos.xy is the target pixel's centre (x.5, y.5); floor gives its integer pixel index, the
    // same one `read_back_level`'s row-major layout expects at that position.
    let texel = vec2<i32>(p.origin_x, p.origin_y) + vec2<i32>(i32(pos.x), i32(pos.y));
    return textureLoad(atlas_tex, texel, p.layer, p.level);
}
