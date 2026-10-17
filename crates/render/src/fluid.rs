//! Coastal wave simulation of the enhanced water (not original, opt-in).
//!
//! **Render-only.** It reads the terrain, never game state, and nothing
//! reads it back: it is outside the deterministic game simulation, so it
//! cannot affect lockstep multiplayer. The GPU runs it (`fluid.wgsl`); this
//! module holds the parameters ([`FluidConfig`]), the incoming swell
//! ([`swell_components`], [`forcing`]), the camera-local window
//! ([`FluidWindow`]) and a CPU reference solver ([`FluidGrid`]) with the
//! same arithmetic, which tests compare with the GPU.
//!
//! **Scales with the map:** the grid covers a fixed number of cells around
//! the view ([`FluidConfig::size`]) and scrolls with the camera as a ring
//! buffer (storage index = world cell index mod size), so only newly
//! visible strips are initialised. Cost is independent of map size and
//! island count.
//!
//! **Model** (spec: `docs/render/iso.md`, "Wave simulation"): linear
//! shallow-water equations on a staggered grid, units `HEIGHT` units for
//! surface elevation `η` and depth `h`, tiles for distance, seconds for
//! time:
//!
//! ```text
//! ∂u/∂t = −g ∂η/∂x − damping(h) u        (u on x faces, v on y faces)
//! ∂η/∂t = −∂(H u)/∂x − ∂(H v)/∂y          H = clamp(face depth, min, cap)
//! ```
//!
//! with wave speed `√(g H)` (deep `deep_speed` at `depth_cap`), so waves
//! slow down, steepen and turn towards the shore over shallows (shoaling,
//! refraction), bend around islands (diffraction) and reflect. Where the
//! elevation exceeds `break_ratio · h` the wave breaks: the excess is
//! dissipated and becomes foam, which decays. Deep water and a sponge band
//! at the window border are relaxed towards an incoming swell that is a sum
//! of [`SWELL_COMPONENTS`] plane waves with seeded random directions,
//! periods and phases; their interference makes wave groups and an
//! irregular sea. Wetting and drying: the flux uses the upwind total
//! depth `η + h` (≥ 0), a face carries flow only where the higher surface
//! lies above the higher bed, and the surface never drops below the bed —
//! so bores run up the beach, slow down by friction and drain back (the
//! run-up is simulated, not added). Swash flow churns foam. Integration is symplectic Euler (velocities, then elevation),
//! explicit, stable for `dt < dx / (√2 · deep_speed)`.

use std::f32::consts::TAU;

/// Number of plane waves in the incoming swell (also the shader's array
/// size).
pub const SWELL_COMPONENTS: usize = 8;

/// Parameters of the simulation. Tuning values, not from the original;
/// kept together so they can become configuration (PLAN §0.1, §0.2).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FluidConfig {
    /// Cells per side of the camera-local grid.
    pub size: u32,
    /// Cells per tile per zoom group (finer when zoomed in).
    pub cells_per_tile: [u32; 3],
    /// Depth (`HEIGHT` units) above which water counts as deep: the flux
    /// depth is capped here, so deep waves travel at `deep_speed`.
    pub depth_cap: f32,
    /// Speed of deep-water waves (tiles per second).
    pub deep_speed: f32,
    /// Smallest flux depth (stability over very shallow water).
    pub min_depth: f32,
    /// Time step (seconds) and most steps per frame (the simulation skips
    /// ahead when a frame takes longer).
    pub dt: f32,
    pub max_steps: u32,
    /// Velocity damping per second, plus bottom friction scaled by 1 / H.
    pub damping: f32,
    pub friction: f32,
    /// Waves break where η exceeds `break_ratio · h`; the excess decays at
    /// `break_rate` per second and turns into foam (`foam_gain` per unit),
    /// which decays with time constant `foam_decay` seconds.
    pub break_ratio: f32,
    pub break_rate: f32,
    /// Breaking applies where the still depth exceeds this; shallower, in
    /// the swash zone, the broken wave runs on as a bore and only friction
    /// slows it (clipping there would stop all run-up).
    pub break_min_depth: f32,
    pub foam_gain: f32,
    pub foam_decay: f32,
    /// Deep water (depth `force_depth.0` → `.1`) relaxes towards the
    /// incoming swell at `force_rate` per second; within `sponge_cells` of
    /// the window border fully.
    pub force_depth: (f32, f32),
    pub force_rate: f32,
    pub sponge_cells: u32,
    /// Largest flow speed (tiles per second; keeps thin swash stable).
    pub max_speed: f32,
    /// Foam churned by swash flow per tile of flow.
    pub swash_foam: f32,
    /// Swash water seeping into the beach above the waterline (`HEIGHT`
    /// units per second), so thin films fall dry instead of draining only
    /// asymptotically.
    pub infiltration: f32,
    /// Incoming swell: main direction (radians in tile axes), largest
    /// component amplitude (`HEIGHT` units), periods (seconds), seed.
    pub swell_dir: f32,
    pub swell_amplitude: f32,
    pub swell_periods: (f32, f32),
    pub seed: u32,
    /// The window recentres when the view centre moves this many cells.
    pub recentre_cells: u32,
}

impl FluidConfig {
    pub const DEFAULT: FluidConfig = FluidConfig {
        size: 1024,
        cells_per_tile: [8, 4, 2],
        depth_cap: 16.0,
        deep_speed: 3.0,
        min_depth: 0.5,
        dt: 1.0 / 60.0,
        max_steps: 6,
        damping: 0.05,
        friction: 0.04,
        break_ratio: 0.8,
        break_rate: 8.0,
        break_min_depth: 3.0,
        foam_gain: 1.5,
        foam_decay: 2.5,
        force_depth: (10.0, 15.0),
        force_rate: 1.5,
        sponge_cells: 32,
        max_speed: 2.5,
        swash_foam: 0.6,
        infiltration: 0.4,
        swell_dir: 0.28,
        swell_amplitude: 1.2,
        swell_periods: (2.6, 4.6),
        seed: 1503,
        recentre_cells: 64,
    };

    /// Gravity in tiles² / (s² · `HEIGHT` unit), from `deep_speed` at
    /// `depth_cap`.
    pub fn gravity(&self) -> f32 {
        self.deep_speed * self.deep_speed / self.depth_cap
    }
}

/// One plane wave of the incoming swell: wave vector `k` (radians per
/// tile), angular frequency `omega` (radians per second), phase, amplitude.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SwellComponent {
    pub k: [f32; 2],
    pub omega: f32,
    pub phase: f32,
    pub amplitude: f32,
}

/// The incoming swell of `cfg`: directions spread around the full circle
/// with the main direction strongest (so every coast gets waves, the
/// windward side most), seeded random periods and phases.
pub fn swell_components(cfg: &FluidConfig) -> [SwellComponent; SWELL_COMPONENTS] {
    let mut state = cfg
        .seed
        .wrapping_mul(747_796_405)
        .wrapping_add(2_891_336_453);
    let mut rand = || {
        state = state.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
        #[allow(clippy::cast_precision_loss)]
        let v = (state >> 8) as f32 / 16_777_216.0;
        v
    };
    std::array::from_fn(|n| {
        #[allow(clippy::cast_precision_loss)]
        let base = n as f32 / SWELL_COMPONENTS as f32 * TAU;
        let theta = cfg.swell_dir + base + (rand() - 0.5) * 0.6;
        let off = theta - cfg.swell_dir;
        let weight = 0.3 + 0.7 * (off / 2.0).cos().powi(2);
        let amplitude = cfg.swell_amplitude * weight * (0.7 + 0.3 * rand());
        let period = cfg.swell_periods.0 + (cfg.swell_periods.1 - cfg.swell_periods.0) * rand();
        let omega = TAU / period;
        let k = omega / cfg.deep_speed;
        SwellComponent {
            k: [k * theta.cos(), k * theta.sin()],
            omega,
            phase: rand() * TAU,
            amplitude,
        }
    })
}

/// Incoming swell at tile position `(x, y)` and time `t` (s): elevation
/// and the depth-averaged velocity of a progressive wave in deep water
/// (`η · deep_speed / depth_cap` along its direction).
pub fn forcing(
    cfg: &FluidConfig,
    comps: &[SwellComponent; SWELL_COMPONENTS],
    (x, y): (f32, f32),
    t: f32,
) -> (f32, f32, f32) {
    let (mut eta, mut u, mut v) = (0.0, 0.0, 0.0);
    let s = cfg.deep_speed / cfg.depth_cap;
    for c in comps {
        let e = c.amplitude * (c.k[0] * x + c.k[1] * y - c.omega * t + c.phase).cos();
        let kn = (c.k[0] * c.k[0] + c.k[1] * c.k[1]).sqrt().max(1e-6);
        eta += e;
        u += e * s * c.k[0] / kn;
        v += e * s * c.k[1] / kn;
    }
    (eta, u, v)
}

/// A rectangle of world cells `[x0, x1) × [y0, y1)`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CellRect {
    pub x0: i64,
    pub y0: i64,
    pub x1: i64,
    pub y1: i64,
}

impl CellRect {
    pub const EMPTY: CellRect = CellRect {
        x0: 0,
        y0: 0,
        x1: 0,
        y1: 0,
    };

    pub fn is_empty(&self) -> bool {
        self.x1 <= self.x0 || self.y1 <= self.y0
    }

    pub fn contains(&self, x: i64, y: i64) -> bool {
        x >= self.x0 && x < self.x1 && y >= self.y0 && y < self.y1
    }
}

/// The camera-local window: `size × size` world cells starting at
/// `origin`, `cells_per_tile` cells per tile.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FluidWindow {
    pub size: i64,
    pub cells_per_tile: u32,
    pub origin: (i64, i64),
}

impl FluidWindow {
    /// A window of `cfg` at zoom `group` centred on tile position `centre`.
    pub fn new(cfg: &FluidConfig, group: usize, centre: (f32, f32)) -> Self {
        let mut w = Self {
            size: i64::from(cfg.size),
            cells_per_tile: cfg.cells_per_tile[group],
            origin: (0, 0),
        };
        w.origin = w.origin_for(centre);
        w
    }

    fn origin_for(&self, (cx, cy): (f32, f32)) -> (i64, i64) {
        let cpt = f64::from(self.cells_per_tile);
        #[allow(clippy::cast_possible_truncation)]
        let c = |v: f32| (f64::from(v) * cpt).floor() as i64 - self.size / 2;
        (c(cx), c(cy))
    }

    /// The whole window as a cell rectangle.
    pub fn rect(&self) -> CellRect {
        CellRect {
            x0: self.origin.0,
            y0: self.origin.1,
            x1: self.origin.0 + self.size,
            y1: self.origin.1 + self.size,
        }
    }

    /// Recentres on `centre` if it moved at least `threshold` cells from
    /// the window centre; returns the cell rectangles that are new (at most
    /// two; the whole window after a jump).
    pub fn recentre(&mut self, centre: (f32, f32), threshold: u32) -> Vec<CellRect> {
        let new = self.origin_for(centre);
        let (dx, dy) = (new.0 - self.origin.0, new.1 - self.origin.1);
        let t = i64::from(threshold);
        if dx.abs() < t && dy.abs() < t {
            return Vec::new();
        }
        let old = self.rect();
        self.origin = new;
        let now = self.rect();
        if dx.abs() >= self.size || dy.abs() >= self.size {
            return vec![now];
        }
        let mut out = Vec::new();
        // Columns that are new (full height of the new window) ...
        let xs = if dx > 0 {
            (old.x1, now.x1)
        } else {
            (now.x0, old.x0)
        };
        if xs.1 > xs.0 {
            out.push(CellRect {
                x0: xs.0,
                y0: now.y0,
                x1: xs.1,
                y1: now.y1,
            });
        }
        // ... and rows that are new within the old columns.
        let (cx0, cx1) = (now.x0.max(old.x0), now.x1.min(old.x1));
        let ys = if dy > 0 {
            (old.y1, now.y1)
        } else {
            (now.y0, old.y0)
        };
        if ys.1 > ys.0 && cx1 > cx0 {
            out.push(CellRect {
                x0: cx0,
                y0: ys.0,
                x1: cx1,
                y1: ys.1,
            });
        }
        out
    }

    /// Storage index of world cell `(x, y)` (ring buffer).
    pub fn storage(&self, x: i64, y: i64) -> (usize, usize) {
        let s = |v: i64| usize::try_from(v.rem_euclid(self.size)).expect("non-negative");
        (s(x), s(y))
    }

    /// Tile position of the centre of world cell `(x, y)`.
    pub fn cell_centre(&self, x: i64, y: i64) -> (f32, f32) {
        let cpt = f64::from(self.cells_per_tile);
        #[allow(clippy::cast_precision_loss, clippy::cast_possible_truncation)]
        let p = |v: i64| ((v as f64 + 0.5) / cpt) as f32;
        (p(x), p(y))
    }
}

/// CPU reference solver: the same arithmetic as `fluid.wgsl`, storage in
/// ring-buffer order. Used by tests (and small enough grids only).
pub struct FluidGrid {
    pub cfg: FluidConfig,
    pub comps: [SwellComponent; SWELL_COMPONENTS],
    pub window: FluidWindow,
    pub depth: Vec<f32>,
    /// `(η, u, v, foam)` per storage cell.
    pub state: Vec<[f32; 4]>,
}

/// Elevation, velocity and foam of a freshly initialised cell.
/// Water deeper than this (`HEIGHT` units) counts as wet.
pub const WET: f32 = 0.02;

/// `h` is the cell's still-water depth (negative on land): dry land gets
/// the surface at its bed (`η = −h`) and no flow.
pub fn reset_cell(
    cfg: &FluidConfig,
    comps: &[SwellComponent; SWELL_COMPONENTS],
    window: &FluidWindow,
    (x, y): (i64, i64),
    t: f32,
    h: f32,
) -> [f32; 4] {
    if h <= WET {
        return [-h, 0.0, 0.0, 0.0];
    }
    let (eta, u, v) = forcing(cfg, comps, window.cell_centre(x, y), t);
    [eta.max(-h), u, v, 0.0]
}

impl FluidGrid {
    /// A grid over `window` with depth `depth(x, y)` (tile position →
    /// depth below the waterline in `HEIGHT` units, ≤ 0 on land),
    /// initialised to the incoming swell at time `t`.
    pub fn new(
        cfg: FluidConfig,
        window: FluidWindow,
        depth: impl Fn(f32, f32) -> f32,
        t: f32,
    ) -> Self {
        let n = usize::try_from(window.size).expect("positive");
        let comps = swell_components(&cfg);
        let mut g = Self {
            cfg,
            comps,
            window,
            depth: vec![0.0; n * n],
            state: vec![[0.0; 4]; n * n],
        };
        let r = window.rect();
        g.fill(r, &depth, t);
        g
    }

    /// Sets depth and state of the cells of `r` (new cells).
    pub fn fill(&mut self, r: CellRect, depth: &impl Fn(f32, f32) -> f32, t: f32) {
        let n = usize::try_from(self.window.size).expect("positive");
        for y in r.y0..r.y1 {
            for x in r.x0..r.x1 {
                let (sx, sy) = self.window.storage(x, y);
                let (px, py) = self.window.cell_centre(x, y);
                let h = depth(px, py);
                self.depth[sy * n + sx] = h;
                self.state[sy * n + sx] =
                    reset_cell(&self.cfg, &self.comps, &self.window, (x, y), t, h);
            }
        }
    }

    /// Relaxation factor towards the swell for world cell `(x, y)` of
    /// depth `h` (sponge at the window border, deep water).
    fn force_factor(&self, x: i64, y: i64, h: f32, dt: f32) -> f32 {
        let c = &self.cfg;
        let (lx, ly) = (x - self.window.origin.0, y - self.window.origin.1);
        let edge = lx
            .min(ly)
            .min(self.window.size - 1 - lx)
            .min(self.window.size - 1 - ly);
        #[allow(clippy::cast_precision_loss)]
        let sponge = smoothstep(c.sponge_cells as f32, 0.0, edge as f32);
        let deep = smoothstep(c.force_depth.0, c.force_depth.1, h);
        (deep * c.force_rate * dt).min(1.0).max(sponge)
    }

    /// One time step at simulation time `t` (s, after the step).
    pub fn step(&mut self, t: f32) {
        let c = self.cfg;
        let n = usize::try_from(self.window.size).expect("positive");
        let dt = c.dt;
        #[allow(clippy::cast_precision_loss)]
        let dx = 1.0 / self.window.cells_per_tile as f32;
        let g = c.gravity();
        let w = self.window;
        let at = |x: i64, y: i64| {
            let (sx, sy) = w.storage(x, y);
            sy * n + sx
        };
        let r = w.rect();
        // Pass A: velocities on faces; a face carries flow when the higher
        // surface lies above the higher bed (wet/dry).
        let src = self.state.clone();
        let mut mid = src.clone();
        for y in r.y0..r.y1 {
            for x in r.x0..r.x1 {
                let i = at(x, y);
                let h = self.depth[i];
                let [eta, mut u, mut v, foam] = src[i];
                let face = |vel: f32, j: usize| -> f32 {
                    let (s_r, b) = (src[j][0], (-h).max(-self.depth[j]));
                    let d = eta.max(s_r) - b;
                    if d <= WET {
                        return 0.0;
                    }
                    let mut vel = vel - g * dt * (s_r - eta) / dx;
                    vel /= 1.0 + dt * (c.damping + c.friction / d.max(c.min_depth));
                    vel.clamp(-c.max_speed, c.max_speed)
                };
                u = face(u, at(x + 1, y));
                v = face(v, at(x, y + 1));
                let f = self.force_factor(x, y, h, dt);
                if f > 0.0 && h > WET {
                    let (px, py) = w.cell_centre(x, y);
                    let half = 0.5 * dx;
                    let (_, fu, _) = forcing(&c, &self.comps, (px + half, py), t);
                    let (_, _, fv) = forcing(&c, &self.comps, (px, py + half), t);
                    u += (fu - u) * f;
                    v += (fv - v) * f;
                }
                mid[i] = [eta, u, v, foam];
            }
        }
        // Pass B: elevation from upwind fluxes, dry floor, breaking, foam.
        let mut out = mid.clone();
        let decay = (-dt / c.foam_decay).exp();
        let total = |j: usize| (mid[j][0] + self.depth[j]).clamp(0.0, c.depth_cap);
        for y in r.y0..r.y1 {
            for x in r.x0..r.x1 {
                let i = at(x, y);
                let h = self.depth[i];
                let [mut eta, u, v, mut foam] = mid[i];
                let (l, up) = (at(x - 1, y), at(x, y - 1));
                let (ul, vu) = (mid[l][1], mid[up][2]);
                let flux = |vel: f32, from: usize, to: usize| {
                    vel * if vel > 0.0 { total(from) } else { total(to) }
                };
                let div = (flux(u, i, at(x + 1, y)) - flux(ul, l, i))
                    + (flux(v, i, at(x, y + 1)) - flux(vu, up, i));
                eta -= dt * div / dx;
                if h <= 0.0 {
                    eta -= dt * c.infiltration;
                }
                eta = eta.max(-h);
                let d = eta + h;
                if h > c.break_min_depth {
                    let lim = c.break_ratio * h;
                    if eta > lim {
                        let ex = (eta - lim) * (c.break_rate * dt).min(1.0);
                        eta -= ex;
                        foam += ex * c.foam_gain;
                    }
                } else if d > WET && h <= c.min_depth {
                    // Swash: turbulent sheet flow churns foam.
                    let speed = 0.5 * (u.abs() + ul.abs() + v.abs() + vu.abs());
                    foam += dt * c.swash_foam * speed;
                }
                foam *= decay;
                let f = self.force_factor(x, y, h, dt);
                if f > 0.0 && h > WET {
                    let (fe, _, _) = forcing(&c, &self.comps, w.cell_centre(x, y), t);
                    eta += (fe.max(-h) - eta) * f;
                }
                out[i] = [eta, u, v, foam];
            }
        }
        self.state = out;
    }
}

/// `0` below `a`, `1` above `b`, smooth in between (also for `a > b`);
/// the shader's `sstep`.
pub fn smoothstep(a: f32, b: f32, x: f32) -> f32 {
    let t = ((x - a) / (b - a)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn window_recentres_in_strips() {
        let cfg = FluidConfig {
            size: 64,
            ..FluidConfig::DEFAULT
        };
        let mut w = FluidWindow::new(&cfg, 0, (10.0, 10.0));
        assert_eq!(w.origin, (80 - 32, 80 - 32));
        assert!(w.recentre((10.5, 10.0), 8).is_empty());
        // 2 tiles = 16 cells right, 1 tile = 8 cells down.
        let new = w.recentre((12.0, 11.0), 8);
        assert_eq!(w.origin, (64, 56));
        assert_eq!(
            new,
            [
                CellRect {
                    x0: 112,
                    y0: 56,
                    x1: 128,
                    y1: 120
                },
                CellRect {
                    x0: 64,
                    y0: 112,
                    x1: 112,
                    y1: 120
                }
            ]
        );
        let cells: i64 = new.iter().map(|r| (r.x1 - r.x0) * (r.y1 - r.y0)).sum();
        assert_eq!(cells, 64 * 64 - 48 * 56);
        assert_eq!(w.recentre((100.0, 100.0), 8), [w.rect()]);
    }

    #[test]
    fn swell_is_seeded_and_bounded() {
        let cfg = FluidConfig::DEFAULT;
        let a = swell_components(&cfg);
        assert_eq!(a, swell_components(&cfg));
        let total: f32 = a.iter().map(|c| c.amplitude).sum();
        assert!(total < 8.0 * cfg.swell_amplitude);
        for c in &a {
            let k = (c.k[0] * c.k[0] + c.k[1] * c.k[1]).sqrt();
            assert!((c.omega / k - cfg.deep_speed).abs() < 1e-4);
        }
    }

    /// A wave reaching a beach: the solver stays bounded (stable), breaks
    /// in shallow water (foam), runs up onto land (cells above the
    /// waterline get wet) and drains back (they also fall dry again).
    #[test]
    fn beach_is_stable_breaks_and_runs_up() {
        let cfg = FluidConfig {
            size: 64,
            cells_per_tile: [4, 4, 4],
            ..FluidConfig::DEFAULT
        };
        let window = FluidWindow::new(&cfg, 0, (8.0, 8.0));
        // Deep in the west, a slope rising to land in the east.
        let depth = |x: f32, _y: f32| 16.0 - 2.0 * (x - 2.0).max(0.0);
        let mut g = FluidGrid::new(cfg, window, depth, 0.0);
        let land: Vec<usize> = (0..g.depth.len())
            .filter(|&i| (-1.5..-0.1).contains(&g.depth[i]))
            .collect();
        let (mut ever_wet, mut wet_then_dry) = (vec![false; g.depth.len()], 0);
        let mut t = 0.0;
        for _ in 0..900 {
            t += cfg.dt;
            g.step(t);
            for &i in &land {
                let wet = g.state[i][0] + g.depth[i] > WET;
                if wet {
                    ever_wet[i] = true;
                } else if ever_wet[i] {
                    wet_then_dry += 1;
                }
            }
        }
        let max_eta = g.state.iter().map(|s| s[0].abs()).fold(0.0, f32::max);
        assert!(max_eta.is_finite() && max_eta < 20.0, "{max_eta}");
        let foam: f32 = g.state.iter().map(|s| s[3]).sum();
        assert!(foam > 0.0);
        assert!(land.iter().any(|&i| ever_wet[i]), "no run-up");
        assert!(wet_then_dry > 0, "no backwash");
    }
}
