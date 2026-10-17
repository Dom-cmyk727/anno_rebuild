// GPU frame compositor (`gpu.rs`). Same rules as the CPU path in
// `frame.rs` / `sea.rs` (spec: docs/render/iso.md, "Animation and layers").
// All positions are integer scene pixels; textures are read with
// textureLoad, never filtered, so cached chunks and sprites stay exact.

struct Frame {
    // Scene pixel of the target's top-left corner, and target size.
    view_origin: vec2<i32>,
    view_size: vec2<u32>,
    // Sea pattern size and animation steps (current, next).
    pattern_size: vec2<i32>,
    steps: vec2<u32>,
    // Fraction of the way to the next step (enhanced only), time in ms
    // modulo one hour, 1 = enhanced water.
    frac: f32,
    time_ms: f32,
    enhanced: u32,
    // Tile size of the zoom group and the sea-level raise in pixels.
    tile_w: f32,
    tile_h: f32,
    raise: f32,
    _pad: vec2<f32>,
}

@group(0) @binding(0) var<uniform> frame: Frame;
// Ground and water objects below the sea, composed in pass 1.
@group(0) @binding(1) var under_tex: texture_2d<f32>;
@group(0) @binding(2) var sea_sprite: texture_2d_array<f32>;
@group(0) @binding(3) var sea_filled: texture_2d_array<f32>;

struct Chunk {
    origin: vec2<i32>,
    size: i32,
    // bit 0: ground, bit 1: objects, bit 2: water field
    flags: u32,
}

@group(1) @binding(0) var<uniform> chunk: Chunk;
@group(1) @binding(1) var ground_tex: texture_2d<f32>;
@group(1) @binding(2) var objects_tex: texture_2d<f32>;
@group(1) @binding(3) var field_tex: texture_2d<u32>;

// Scene pixel rectangle → clip space.
fn to_clip(p: vec2<f32>) -> vec4<f32> {
    let size = vec2<f32>(frame.view_size);
    let rel = (p - vec2<f32>(frame.view_origin)) / size;
    return vec4<f32>(rel.x * 2.0 - 1.0, 1.0 - rel.y * 2.0, 0.0, 1.0);
}

fn quad_corner(i: u32) -> vec2<f32> {
    // Two triangles: (0,0) (1,0) (0,1), (0,1) (1,0) (1,1).
    let c = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
    );
    return c[i];
}

@vertex
fn chunk_vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4<f32> {
    let p = vec2<f32>(chunk.origin) + quad_corner(i) * f32(chunk.size);
    return to_clip(p);
}

// Scene pixel of a fragment.
fn scene_px(pos: vec4<f32>) -> vec2<i32> {
    return frame.view_origin + vec2<i32>(floor(pos.xy));
}

@fragment
fn ground_fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    if (chunk.flags & 1u) == 0u {
        discard;
    }
    let local = scene_px(pos) - chunk.origin;
    return vec4<f32>(textureLoad(ground_tex, local, 0).rgb, 1.0);
}

@fragment
fn objects_fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    if (chunk.flags & 2u) == 0u {
        discard;
    }
    let local = scene_px(pos) - chunk.origin;
    let c = textureLoad(objects_tex, local, 0);
    if c.a == 0.0 {
        discard;
    }
    return c;
}

// ---- Enhanced waves: port of iso::wave_shade (not original, opt-in) ----

fn hash3(x: i32, y: i32, z: i32) -> f32 {
    var h = (bitcast<u32>(x) * 0x8DA6B343u) ^ (bitcast<u32>(y) * 0xD8163841u)
        ^ (bitcast<u32>(z) * 0xCB1AB31Fu);
    h = h ^ (h >> 13u);
    h = h * 0x5BD1E995u;
    h = h ^ (h >> 15u);
    return f32(h & 0x00FFFFFFu) / 16777216.0;
}

fn noise3(x: f32, y: f32, z: f32) -> f32 {
    let f = floor(vec3<f32>(x, y, z));
    let i = vec3<i32>(f);
    let t = vec3<f32>(x, y, z) - f;
    let u = t * t * (3.0 - 2.0 * t);
    let c000 = hash3(i.x, i.y, i.z);
    let c100 = hash3(i.x + 1, i.y, i.z);
    let c010 = hash3(i.x, i.y + 1, i.z);
    let c110 = hash3(i.x + 1, i.y + 1, i.z);
    let c001 = hash3(i.x, i.y, i.z + 1);
    let c101 = hash3(i.x + 1, i.y, i.z + 1);
    let c011 = hash3(i.x, i.y + 1, i.z + 1);
    let c111 = hash3(i.x + 1, i.y + 1, i.z + 1);
    let p0 = mix(mix(c000, c100, u.x), mix(c010, c110, u.x), u.y);
    let p1 = mix(mix(c001, c101, u.x), mix(c011, c111, u.x), u.y);
    return mix(p0, p1, u.z);
}

// 0 below a, 1 above b, smooth in between (also for a > b).
fn sstep(a: f32, b: f32, x: f32) -> f32 {
    let t = clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Waves::DEFAULT (iso.rs)
const SURF_ZONE: f32 = 4.0;
const SHOAL_DEPTH: f32 = 12.0;
const BREAK_DEPTH: f32 = 4.0;
const SWASH_DEPTH: f32 = 1.5;
const SWELL_DIR: vec2<f32> = vec2<f32>(0.96, 0.28);
const TINT_DEPTH: f32 = 8.0;
const WAVELENGTH: f32 = 2.6;
const PERIOD_MS: f32 = 3400.0;
const SWELL: f32 = 0.07;
const SHALLOW_TINT: f32 = 0.3;
const SHALLOW_COLOUR: vec3<f32> = vec3<f32>(0.36, 0.74, 0.76);
const DEEP_FROM: f32 = 4.0;
const DEEP_TO: f32 = 14.0;
// sea::SHORE_SCALE, sea::DEPTH_SCALE
const SHORE_SCALE: f32 = 32.0;
const DEPTH_SCALE: f32 = 4.0;
const DEPTH_BIAS: f32 = 128.0;
const SHORE_EDGE: f32 = 0.35;

// iso::wave_shade (profile)
fn profile(u: f32) -> f32 {
    let d = u - 0.9;
    if d < 0.0 {
        return exp(-(d / 0.3) * (d / 0.3));
    }
    return exp(-(d / 0.05) * (d / 0.05));
}

// iso::wave_shade: (light, foam, tint).
fn wave_shade(dist: f32, depth: f32, w: vec2<f32>, t: f32, detail: f32) -> vec3<f32> {
    let s1 = noise3(w.x * 0.22 + t * 0.00011, w.y * 0.22 - t * 0.00004, t * 0.00006);
    let s2 = noise3(w.x * 0.55 - t * 0.00019, w.y * 0.55 + t * 0.00013, t * 0.00011);
    var light = SWELL * 2.0 * (0.65 * s1 + 0.35 * s2 - 0.5);
    var foam = 0.0;
    let near = sstep(SURF_ZONE, SURF_ZONE * 0.5, dist);
    if near > 0.0 {
        let bend = 1.4 * noise3(w.x * 0.12, w.y * 0.12, t * 0.00003)
            + 0.5 * noise3(w.x * 0.35 + 5.0, w.y * 0.35, t * 0.00005);
        let along = -dot(w, SWELL_DIR);
        let phase = along / WAVELENGTH + t / PERIOD_MS + bend;
        let u = phase - floor(phase);
        let height = profile(u);
        let slope = (profile(min(u + 0.01, 1.0)) - profile(max(u - 0.01, 0.0))) * 2.5;
        let segment = 0.55 + 0.45 * sstep(0.3, 0.7, noise3(w.x * 0.25 + 3.1, w.y * 0.25, t * 0.00005));
        let shoal = sstep(SHOAL_DEPTH, BREAK_DEPTH, depth);
        let breaking = sstep(BREAK_DEPTH * 1.4, BREAK_DEPTH * 0.7, depth);
        let bore = 1.0 - 0.7 * sstep(BREAK_DEPTH * 0.7, 0.3, depth);
        let amp = near * segment * (0.35 + 0.65 * shoal) * (1.0 - 0.5 * breaking);
        light += amp * (0.10 * slope + 0.10 * height);
        let fine = noise3(w.x * 2.0, w.y * 2.0, t * 0.0009);
        let mottle = 0.55 + detail * (fine - 0.55);
        if breaking > 0.0 {
            let pieces = sstep(0.35, 0.6, noise3(w.x * 0.5 + 9.0, w.y * 0.5, t * 0.00007));
            let surge = breaking * bore * pieces;
            let dl = (u - 0.9) / 0.035;
            let line = exp(-dl * dl);
            let behind = max(0.9 - u, 0.0);
            let trail = exp(-behind * 14.0) * sstep(0.25, 0.7, mottle);
            foam = surge * near * segment * max(0.8 * line, 0.3 * trail);
        }
        if depth < SWASH_DEPTH {
            let at_shore = phase;
            let since = at_shore - floor(at_shore);
            let burst = sstep(0.0, 0.05, since) * exp(-since * 3.0);
            let edge = sstep(SWASH_DEPTH, 0.0, depth);
            foam = max(foam, edge * burst * (0.35 + 0.4 * mottle));
        }
    }
    let tint = SHALLOW_TINT * sstep(TINT_DEPTH, 0.0, depth);
    return vec3<f32>(light, clamp(foam, 0.0, 1.0), tint);
}

// iso::shade_apply
fn shade_apply(c: vec3<f32>, s: vec3<f32>) -> vec3<f32> {
    let v = mix(c, SHALLOW_COLOUR, s.z) * (1.0 + s.x);
    let white = vec3<f32>(236.0, 244.0, 250.0) / 255.0;
    return clamp(v + (white - v) * s.y, vec3<f32>(0.0), vec3<f32>(1.0));
}

fn wrap(a: i32, m: i32) -> i32 {
    return ((a % m) + m) % m;
}

fn pattern_px(p: vec2<i32>) -> vec2<i32> {
    return vec2<i32>(wrap(p.x, frame.pattern_size.x), wrap(p.y, frame.pattern_size.y));
}

fn premul(c: vec4<f32>) -> vec4<f32> {
    return vec4<f32>(c.rgb * c.a, c.a);
}

fn smoothed_sprite(p: vec2<i32>, layer: u32) -> vec4<f32> {
    return (premul(textureLoad(sea_sprite, pattern_px(p), layer, 0))
        + premul(textureLoad(sea_sprite, pattern_px(p + vec2<i32>(1, 0)), layer, 0))
        + premul(textureLoad(sea_sprite, pattern_px(p + vec2<i32>(0, 1)), layer, 0))
        + premul(textureLoad(sea_sprite, pattern_px(p + vec2<i32>(1, 1)), layer, 0))) / 4.0;
}

fn smoothed_filled(p: vec2<i32>, layer: u32) -> vec4<f32> {
    return (premul(textureLoad(sea_filled, pattern_px(p), layer, 0))
        + premul(textureLoad(sea_filled, pattern_px(p + vec2<i32>(1, 0)), layer, 0))
        + premul(textureLoad(sea_filled, pattern_px(p + vec2<i32>(0, 1)), layer, 0))
        + premul(textureLoad(sea_filled, pattern_px(p + vec2<i32>(1, 1)), layer, 0))) / 4.0;
}

fn unpremultiply(p: vec4<f32>) -> vec4<f32> {
    if p.a <= 0.0 {
        return vec4<f32>(0.0);
    }
    return vec4<f32>(p.rgb / p.a, p.a);
}

// The sea over the pass-1 result: sea::shade_sea.
@fragment
fn sea_fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    let px = scene_px(pos);
    let under = textureLoad(under_tex, vec2<i32>(floor(pos.xy)), 0).rgb;
    var cover = 255u;
    var shore = 255u;
    var depth = 255u;
    var flags = 1u;
    if (chunk.flags & 4u) != 0u {
        let f = textureLoad(field_tex, px - chunk.origin, 0);
        cover = f.r;
        shore = f.g;
        depth = f.b;
        flags = f.a;
    }
    if cover == 0u {
        return vec4<f32>(under, 1.0);
    }
    let enhanced = frame.enhanced != 0u;
    // Original: the original's waterline; enhanced: the smooth waterline
    // with a soft edge (sea::smooth_height).
    let sd = (f32(depth) - DEPTH_BIAS) / DEPTH_SCALE;
    var alpha = 1.0;
    if enhanced {
        alpha = sstep(-SHORE_EDGE, SHORE_EDGE, sd);
    } else if (flags & 1u) == 0u {
        alpha = 0.0;
    }
    if alpha <= 0.0 {
        return vec4<f32>(under, 1.0);
    }
    let pi = vec2<i32>(wrap(px.x, frame.pattern_size.x), wrap(px.y, frame.pattern_size.y));
    var sprite = textureLoad(sea_sprite, pi, frame.steps.x, 0);
    var filled = textureLoad(sea_filled, pi, frame.steps.x, 0);
    if enhanced {
        // sea::SeaPattern::smoothed: 2 x 2 premultiplied average (removes the
        // checkerboard), blended into the next step.
        let s = mix(smoothed_sprite(px, frame.steps.x), smoothed_sprite(px, frame.steps.y), frame.frac);
        let f = mix(smoothed_filled(px, frame.steps.x), smoothed_filled(px, frame.steps.y), frame.frac);
        sprite = unpremultiply(s);
        filled = unpremultiply(f);
    }
    var c = under * (1.0 - sprite.a) + sprite.rgb * sprite.a;
    var open = f32(cover - 1u) / 254.0;
    if enhanced {
        // iso::deep_water
        open = max(open, sstep(DEEP_FROM, DEEP_TO, max(sd, 0.0)));
    }
    if open > 0.0 {
        c = mix(c, filled.rgb, open);
    }
    if enhanced {
        let p = vec2<f32>(f32(px.x), f32(px.y) + frame.raise);
        let w = vec2<f32>(p.x / frame.tile_w + p.y / frame.tile_h, p.y / frame.tile_h - p.x / frame.tile_w);
        let detail = sstep(24.0, 64.0, frame.tile_w);
        c = shade_apply(c, wave_shade(f32(shore) / SHORE_SCALE, sd, w, frame.time_ms, detail));
    }
    return vec4<f32>(mix(under, c, alpha), 1.0);
}

// ---- Sprites (water objects) from the atlas ----

@group(2) @binding(0) var atlas: texture_2d_array<f32>;

struct SpriteOut {
    @builtin(position) pos: vec4<f32>,
    // Atlas texel of the quad's top-left corner, layer, shadow flag.
    @location(0) @interpolate(flat) src: vec2<i32>,
    @location(1) @interpolate(flat) dst: vec2<i32>,
    @location(2) @interpolate(flat) layer_shadow: vec2<u32>,
}

@vertex
fn sprite_vs(
    @builtin(vertex_index) i: u32,
    @location(0) dst: vec2<i32>,
    @location(1) size: vec2<u32>,
    @location(2) src: vec2<u32>,
    @location(3) layer_shadow: vec2<u32>,
) -> SpriteOut {
    var out: SpriteOut;
    out.pos = to_clip(vec2<f32>(dst) + quad_corner(i) * vec2<f32>(size));
    out.src = vec2<i32>(src);
    out.dst = dst;
    out.layer_shadow = layer_shadow;
    return out;
}

@fragment
fn sprite_fs(in: SpriteOut) -> @location(0) vec4<f32> {
    let local = scene_px(in.pos) - in.dst;
    let c = textureLoad(atlas, in.src + local, in.layer_shadow.x, 0);
    if c.a == 0.0 {
        discard;
    }
    if in.layer_shadow.y != 0u {
        // iso::SHADOW: 37.5 % black.
        return vec4<f32>(0.0, 0.0, 0.0, 96.0 / 255.0);
    }
    return c;
}
