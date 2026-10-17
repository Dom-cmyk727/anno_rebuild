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

// Waves::DEFAULT
const SURF_ZONE: f32 = 5.5;
const BREAK_ZONE: f32 = 2.6;
const WAVELENGTH: f32 = 1.6;
const PERIOD_MS: f32 = 2600.0;
const WHITECAPS: f32 = 0.18;

// (light, foam) at shore distance `shore` (tiles), plane position w (tiles).
fn wave_shade(shore: f32, w: vec2<f32>, t: f32) -> vec2<f32> {
    let wobble = 0.08 * sin(w.x * 0.45) + 0.06 * sin(w.y * 0.65 + 1.7);
    let phase = shore / WAVELENGTH + t / PERIOD_MS + wobble;
    let u = phase - floor(phase);
    let height = max(exp(-u * 5.0), sstep(0.86, 1.0, u));
    let segment = sstep(0.25, 0.6, noise3(w.x * 0.35, w.y * 0.35, t * 0.00012));
    let grow = sstep(SURF_ZONE, BREAK_ZONE, shore) * (0.45 + 0.55 * segment);
    let face = sstep(0.86, 0.98, u) * (1.0 - sstep(0.98, 1.0, u));
    var light = -0.32 * grow * face + 0.2 * grow * exp(-u * 12.0);
    let calm = sstep(BREAK_ZONE, SURF_ZONE * 1.5, shore);
    light += calm * 0.12 * (0.5 * sin(0.55 * w.x + 0.35 * w.y - t * 0.0018)
        + 0.5 * sin(0.23 * w.x - 0.41 * w.y - t * 0.0011));
    let churn = noise3(w.x * 2.6, w.y * 2.6, t * 0.0016);
    var foam = 0.0;
    let breaking = sstep(BREAK_ZONE, BREAK_ZONE * 0.45, shore) * (0.5 + 0.5 * segment);
    if breaking > 0.0 {
        let crest_foam = pow(height, 1.2) * (0.6 + 0.6 * churn);
        let trail = exp(-u * 1.6) * sstep(0.35, 0.7, churn) * 0.75;
        foam = max(foam, breaking * (crest_foam + trail));
    }
    if shore < 1.3 {
        let at_shore = phase - shore / WAVELENGTH;
        let since = at_shore - floor(at_shore);
        let burst = sstep(0.0, 0.04, since) * exp(-since * 2.4);
        let streaks = sstep(0.25, 0.65, noise3(w.x * 4.0, w.y * 4.0, t * 0.0009));
        let near = pow(1.0 - shore / 1.3, 0.7);
        foam = max(foam, near * burst * (0.65 + 0.35 * streaks));
    }
    if calm > 0.0 {
        let swell_crest = sin(0.55 * w.x + 0.35 * w.y - t * 0.0018);
        let on_crest = sstep(0.55, 0.95, swell_crest);
        let cell = noise3(w.x * 0.6, w.y * 0.6, t * 0.00016);
        let threshold = 0.75 - 0.25 * WHITECAPS;
        let cap = sstep(threshold - 0.04, threshold + 0.06, cell) * on_crest;
        let detail = noise3(w.x * 6.0, w.y * 6.0, t * 0.0012);
        foam = max(foam, calm * cap * (0.3 + 0.7 * detail) * 0.75);
    }
    return vec2<f32>(light, clamp(foam, 0.0, 1.0));
}

// iso::shade_apply
fn shade_apply(c: vec3<f32>, light: f32, foam: f32) -> vec3<f32> {
    let v = c * (1.0 + light);
    let white = vec3<f32>(236.0, 244.0, 250.0) / 255.0;
    return clamp(v + (white - v) * foam, vec3<f32>(0.0), vec3<f32>(1.0));
}

fn wrap(a: i32, m: i32) -> i32 {
    return ((a % m) + m) % m;
}

// The sea over the pass-1 result: sea::shade_sea.
@fragment
fn sea_fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
    let px = scene_px(pos);
    let under = textureLoad(under_tex, vec2<i32>(floor(pos.xy)), 0).rgb;
    var cover = 255u;
    var shore = 255u;
    if (chunk.flags & 4u) != 0u {
        let f = textureLoad(field_tex, px - chunk.origin, 0);
        cover = f.r;
        shore = f.g;
    }
    if cover == 0u {
        return vec4<f32>(under, 1.0);
    }
    let pi = vec2<i32>(wrap(px.x, frame.pattern_size.x), wrap(px.y, frame.pattern_size.y));
    var sprite = textureLoad(sea_sprite, pi, frame.steps.x, 0);
    var filled = textureLoad(sea_filled, pi, frame.steps.x, 0);
    let enhanced = frame.enhanced != 0u;
    if enhanced {
        sprite = mix(sprite, textureLoad(sea_sprite, pi, frame.steps.y, 0), frame.frac);
        filled = mix(filled, textureLoad(sea_filled, pi, frame.steps.y, 0), frame.frac);
    }
    var c = under * (1.0 - sprite.a) + sprite.rgb * sprite.a;
    if cover > 1u {
        c = mix(c, filled.rgb, f32(cover - 1u) / 254.0);
    }
    if enhanced {
        let p = vec2<f32>(f32(px.x), f32(px.y) + frame.raise);
        let w = vec2<f32>(p.x / frame.tile_w + p.y / frame.tile_h, p.y / frame.tile_h - p.x / frame.tile_w);
        let s = wave_shade(f32(shore) / 16.0, w, frame.time_ms);
        c = shade_apply(c, s.x, s.y);
    }
    return vec4<f32>(c, 1.0);
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
