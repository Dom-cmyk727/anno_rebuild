// Coastal wave simulation (enhanced water, render-only). Same arithmetic
// as the CPU reference `fluid.rs` (`FluidGrid::step`); model and units are
// documented there and in docs/render/iso.md, "Wave simulation".
//
// Storage is a ring buffer: world cell (x, y) lives at texel
// (x mod size, y mod size). Each thread handles one texel and derives its
// world cell from the window origin.

struct Params {
    // g, depth_cap, min_depth, dt
    a: vec4<f32>,
    // damping, friction, break_ratio, break_rate
    b: vec4<f32>,
    // foam_gain, foam decay factor per step, force_depth.0, force_depth.1
    c: vec4<f32>,
    // force_rate, sponge_cells, infiltration, deep_speed / depth_cap
    d: vec4<f32>,
    // max_speed, swash_foam, break_min_depth, 0
    e: vec4<f32>,
    // per swell component: (kx, ky, omega, phase), (amplitude, 0, 0, 0)
    comps: array<vec4<f32>, 16>,
}

struct Step {
    origin: vec2<i32>,
    size: i32,
    _p: i32,
    cpt: f32,
    t: f32,
    _q: vec2<f32>,
}

@group(0) @binding(0) var<uniform> P: Params;
@group(0) @binding(1) var<uniform> S: Step;
@group(0) @binding(2) var src: texture_2d<f32>;
@group(0) @binding(3) var dst: texture_storage_2d<rgba32float, write>;
@group(0) @binding(4) var bathy: texture_2d<f32>;

// Euclidean remainder (n > 0) with non-negative operands only: WGSL defines
// % for negative operands, but backends that translate to GLSL (wgpu's GL
// backend) inherit GLSL's undefined result, which llvmpipe got wrong for all
// but powers of two (CI, 2026-10-18).
fn wrapi(v: i32, n: i32) -> i32 {
    if v >= 0 {
        return v % n;
    }
    return n - 1 - (-(v + 1)) % n;
}

fn texel(x: i32, y: i32) -> vec2<i32> {
    return vec2<i32>(wrapi(x, S.size), wrapi(y, S.size));
}

fn depth_at(x: i32, y: i32) -> f32 {
    return textureLoad(bathy, texel(x, y), 0).r;
}

fn state_at(x: i32, y: i32) -> vec4<f32> {
    return textureLoad(src, texel(x, y), 0);
}

fn cell_centre(x: i32, y: i32) -> vec2<f32> {
    return (vec2<f32>(f32(x), f32(y)) + 0.5) / S.cpt;
}

fn sstep(a: f32, b: f32, x: f32) -> f32 {
    let t = clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// fluid::forcing: (eta, u, v) of the incoming swell.
fn forcing(p: vec2<f32>, t: f32) -> vec3<f32> {
    var r = vec3<f32>(0.0);
    for (var n = 0; n < 8; n++) {
        let k = P.comps[2 * n];
        let amp = P.comps[2 * n + 1].x;
        let e = amp * cos(k.x * p.x + k.y * p.y - k.z * t + k.w);
        let kn = max(sqrt(k.x * k.x + k.y * k.y), 1e-6);
        r += vec3<f32>(e, e * P.d.w * k.x / kn, e * P.d.w * k.y / kn);
    }
    return r;
}

fn face_h(a: f32, b: f32) -> f32 {
    return clamp(0.5 * (a + b), P.a.z, P.a.y);
}

fn force_factor(x: i32, y: i32, h: f32) -> f32 {
    let lx = x - S.origin.x;
    let ly = y - S.origin.y;
    let edge = min(min(lx, ly), min(S.size - 1 - lx, S.size - 1 - ly));
    let sponge = sstep(P.d.y, 0.0, f32(edge));
    let deep = sstep(P.c.z, P.c.w, h);
    return max(min(deep * P.d.x * P.a.w, 1.0), sponge);
}

// World cell of the thread's texel.
fn world_cell(id: vec2<u32>) -> vec2<i32> {
    let g = vec2<i32>(id);
    return vec2<i32>(
        S.origin.x + wrapi(g.x - S.origin.x, S.size),
        S.origin.y + wrapi(g.y - S.origin.y, S.size),
    );
}

const WET: f32 = 0.02;

// A face from cell (x, y) towards neighbour (nx, ny): new velocity after
// the pressure step; zero where the higher surface does not reach above
// the higher bed (wet/dry).
fn face_vel(vel: f32, eta: f32, h: f32, nx: i32, ny: i32) -> f32 {
    let s_r = state_at(nx, ny).x;
    let b = max(-h, -depth_at(nx, ny));
    let d = max(eta, s_r) - b;
    if d <= WET {
        return 0.0;
    }
    let dt = P.a.w;
    let dx = 1.0 / S.cpt;
    var v = vel - P.a.x * dt * (s_r - eta) / dx;
    v /= 1.0 + dt * (P.b.x + P.b.y / max(d, P.a.z));
    return clamp(v, -P.e.x, P.e.x);
}

// Pass A: velocities (FluidGrid::step, first loop).
@compute @workgroup_size(8, 8)
fn vel_step(@builtin(global_invocation_id) id: vec3<u32>) {
    if i32(id.x) >= S.size || i32(id.y) >= S.size {
        return;
    }
    let w = world_cell(id.xy);
    let x = w.x;
    let y = w.y;
    let dt = P.a.w;
    let dx = 1.0 / S.cpt;
    let h = depth_at(x, y);
    let s = state_at(x, y);
    var u = face_vel(s.y, s.x, h, x + 1, y);
    var v = face_vel(s.z, s.x, h, x, y + 1);
    let f = force_factor(x, y, h);
    if f > 0.0 && h > WET {
        let p = cell_centre(x, y);
        let half = 0.5 * dx;
        let fu = forcing(vec2<f32>(p.x + half, p.y), S.t).y;
        let fv = forcing(vec2<f32>(p.x, p.y + half), S.t).z;
        u += (fu - u) * f;
        v += (fv - v) * f;
    }
    textureStore(dst, vec2<i32>(id.xy), vec4<f32>(s.x, u, v, s.w));
}

fn total(x: i32, y: i32) -> f32 {
    return clamp(state_at(x, y).x + depth_at(x, y), 0.0, P.a.y);
}

fn flux(vel: f32, fx: i32, fy: i32, tx: i32, ty: i32) -> f32 {
    if vel > 0.0 {
        return vel * total(fx, fy);
    }
    return vel * total(tx, ty);
}

// Pass B: elevation from upwind fluxes, dry floor, breaking, foam.
@compute @workgroup_size(8, 8)
fn eta_step(@builtin(global_invocation_id) id: vec3<u32>) {
    if i32(id.x) >= S.size || i32(id.y) >= S.size {
        return;
    }
    let w = world_cell(id.xy);
    let x = w.x;
    let y = w.y;
    let dt = P.a.w;
    let h = depth_at(x, y);
    let s = state_at(x, y);
    var eta = s.x;
    let ul = state_at(x - 1, y).y;
    let vu = state_at(x, y - 1).z;
    // Foam drifts with the flow (fluid.rs: semi-Lagrangian).
    let bx = f32(x) - 0.5 * (s.y + ul) * dt * S.cpt;
    let by = f32(y) - 0.5 * (s.z + vu) * dt * S.cpt;
    let fx0 = floor(bx);
    let fy0 = floor(by);
    let ffx = bx - fx0;
    let ffy = by - fy0;
    let ix = i32(fx0);
    let iy = i32(fy0);
    var foam = (state_at(ix, iy).w * (1.0 - ffx) + state_at(ix + 1, iy).w * ffx) * (1.0 - ffy)
        + (state_at(ix, iy + 1).w * (1.0 - ffx) + state_at(ix + 1, iy + 1).w * ffx) * ffy;
    let div = (flux(s.y, x, y, x + 1, y) - flux(ul, x - 1, y, x, y))
        + (flux(s.z, x, y, x, y + 1) - flux(vu, x, y - 1, x, y));
    eta -= dt * div / (1.0 / S.cpt);
    if h <= 0.0 {
        eta -= dt * P.d.z;
    }
    eta = max(eta, -h);
    let d = eta + h;
    if h > P.e.z {
        let lim = P.b.z * h;
        if eta > lim {
            let ex = (eta - lim) * min(P.b.w * dt, 1.0);
            eta -= ex;
            foam += ex * P.c.x;
        }
    } else if d > WET && h <= P.a.z {
        let speed = 0.5 * (abs(s.y) + abs(ul) + abs(s.z) + abs(vu));
        foam += dt * P.e.y * speed;
    }
    foam *= P.c.y;
    foam = max(foam, 0.0);
    let f = force_factor(x, y, h);
    if f > 0.0 && h > WET {
        let fe = forcing(cell_centre(x, y), S.t).x;
        eta += (max(fe, -h) - eta) * f;
    }
    textureStore(dst, vec2<i32>(id.xy), vec4<f32>(eta, s.y, s.z, foam));
}
