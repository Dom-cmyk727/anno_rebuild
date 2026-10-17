//! The sea surface, drawn per pixel from a pre-rendered world pattern
//! (spec: `docs/render/iso.md`, "Sea surface").
//!
//! The original draws the sea as sprites of entry [`SEA_ANLAGE`] on a
//! staggered lattice (`y` even, `x ≡ 2·((y/2) mod 2) (mod 4)`), raised to
//! [`SEA_LEVEL`], with animation phase `(5x + 3y) mod 32` (D: `AnnoWelt.dll`
//! `0x1009f4c0`). Lattice and phase repeat after [`PERIOD_TILES`] tiles in
//! world `x` and `y`, so the whole sea at one animation step is one
//! periodic image. [`SeaPattern`] renders that image once per step; a frame
//! then only looks pixels up. Where the sea may appear, and how much of the
//! sea floor shows through, is static per pixel ([`WaterField`]).
//!
//! Lattice and phase use world tile coordinates, so the sea continues
//! seamlessly between islands. Whether the original uses world or
//! island-local coordinates is not traced (OQ-026 b).

use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel::{GroundCell, Island};

use crate::iso::{
    self, Canvas, LAND_AVERAGE, Lift, SEA_ANLAGE, SEA_LEVEL, SpriteCache, TILE, WaterStyle, over,
};
use crate::scene::{Placement, Rect};

/// Period of the sea lattice and its animation phase in world tiles, along
/// `x` and `y`. A shift by `(p, p)` changes the phase `(5x + 3y) mod 32` by
/// `8p`, a shift by `(p, −p)` by `2p`; both vanish mod 32 for `p = 16`, and
/// 16 is a multiple of the lattice spacing 4. On screen the period is
/// `16·tw × 16·th` (shifts `(16, −16)` and `(16, 16)` of [`iso::tile_top`]).
pub const PERIOD_TILES: i32 = 16;

/// Shore distances in [`WaterField::shore`] are stored in 1/32 tiles (fine
/// enough that crest lines do not step at zoom group 0); 255 = 7.97 tiles
/// or more (open sea: the coastal waves end at `Waves::surf_zone`, 3 tiles).
/// `gpu.wgsl` uses the same scale.
pub const SHORE_SCALE: f32 = 32.0;

/// Water depths in [`WaterField::depth`] are stored in 1/8 `HEIGHT` units
/// (255 = 31.9 units or more; sea level is 32). `gpu.wgsl` uses the same
/// scale.
pub const DEPTH_SCALE: f32 = 8.0;

/// Passes of a 3 × 3 box blur over the shore distance field: the chamfer
/// distance has octagonal contours with kinks at the tile corners, which
/// showed as angular bands along the coast. Land stays 0.
const SHORE_SMOOTHING: usize = 2;

/// [`iso::shore_distance`] smoothed by [`SHORE_SMOOTHING`] box-blur passes.
fn smooth_shore(heights: &[u8], w: usize, h: usize) -> Vec<f32> {
    let mut d = iso::shore_distance(heights, w, h);
    for _ in 0..SHORE_SMOOTHING {
        let prev = d.clone();
        for y in 0..h {
            for x in 0..w {
                let i = y * w + x;
                if prev[i] == 0.0 {
                    continue;
                }
                let (mut sum, mut n) = (0.0, 0.0);
                for yy in y.saturating_sub(1)..=(y + 1).min(h - 1) {
                    for xx in x.saturating_sub(1)..=(x + 1).min(w - 1) {
                        let v = prev[yy * w + xx];
                        // Open sea beyond the island keeps growing.
                        if v.is_finite() {
                            sum += v;
                            n += 1.0;
                        }
                    }
                }
                if n > 0.0 {
                    d[i] = sum / n;
                }
            }
        }
    }
    d
}

/// One animation step of the sea, `width × height` pixels, row-major.
struct PatternStep {
    /// Sea sprites only (transparent between them): drawn over a sea floor.
    sprite: Vec<[u8; 4]>,
    /// Sea sprites over the open-water colour: open water, where no sea
    /// floor texture is drawn.
    filled: Vec<[u8; 4]>,
}

/// The sea at every animation step as a periodic screen image. Pixel
/// `(sx, sy)` of the scene screen (at the pattern's zoom group, sea raised)
/// is pattern pixel `(sx mod width, sy mod height)`.
pub struct SeaPattern {
    pub width: i32,
    pub height: i32,
    /// Milliseconds per animation step (sprite `ANIM`; 0 = static).
    pub step_ms: u64,
    steps: Vec<PatternStep>,
}

impl SeaPattern {
    /// Renders the pattern of `group`; `water` is the open-water colour.
    /// `None` if the sea entry or its frames of `group` are missing.
    pub fn new(
        anlagen: &AnlagenFile<'_>,
        group: usize,
        lift: Lift,
        water: [u8; 4],
        cache: &SpriteCache,
    ) -> Option<Self> {
        let entry = anlagen.entry(SEA_ANLAGE)?;
        let set = entry.gfx.first()?;
        let g = u32::try_from(group).ok()?;
        let frames = set.frame_sets.iter().find(|fs| fs.group == g)?;
        let anim = iso::animation(set);
        let steps = if anim.1 == 0 { 1 } else { anim.0 };
        let (tw, th) = TILE[group];
        let (width, height) = (PERIOD_TILES * tw, PERIOD_TILES * th);
        let raise = lift.scaled(SEA_LEVEL, 1, group);
        // Lattice points whose sprites can reach the pattern rectangle: its
        // tile range (x ∈ [0, 2p], y ∈ [−p, p]) plus a margin of 8 tiles,
        // more than a sea sprite and the sea-level raise span.
        let p = PERIOD_TILES;
        let (xs, ys) = (-8..2 * p + 8, -p - 8..p + 8);
        let to_usize = |v: i32| usize::try_from(v).expect("positive");
        let mut out = Vec::with_capacity(steps);
        for step in 0..steps {
            let t = u64::try_from(step).expect("fits") * anim.1;
            let blank = |c: [u8; 4]| Canvas {
                width: to_usize(width),
                height: to_usize(height),
                origin: (0, 0),
                pixels: c.repeat(to_usize(width * height)),
            };
            let (mut sprite, mut filled) = (blank([0; 4]), blank(water));
            for y in ys.clone().filter(|y| y.rem_euclid(2) == 0) {
                let x0 = 2 * (y / 2).rem_euclid(2);
                for x in xs.clone().filter(|x| (x - x0).rem_euclid(4) == 0) {
                    let phase = usize::try_from((5 * x + 3 * y).rem_euclid(32)).expect("< 32");
                    let k = iso::frame_index(frames.frames.len(), anim, 0, t, phase);
                    let Some(frame) = frames.frames.get(k) else {
                        continue;
                    };
                    let Some(img) = cache.get(anlagen, (SEA_ANLAGE, 0, false, g, k), frame) else {
                        continue;
                    };
                    let (sx, sy) = iso::tile_top(x, y, group);
                    let at = (sx, sy - raise);
                    iso::blit_sprite(&mut sprite, &img, frame, at, false, |_, _, c| Some(c));
                    iso::blit_sprite(&mut filled, &img, frame, at, false, |_, _, c| Some(c));
                }
            }
            let crop = |c: &Canvas| -> Vec<[u8; 4]> { c.pixels.as_chunks::<4>().0.to_vec() };
            out.push(PatternStep {
                sprite: crop(&sprite),
                filled: crop(&filled),
            });
        }
        Some(Self {
            width,
            height,
            step_ms: anim.1,
            steps: out,
        })
    }

    /// Number of animation steps.
    pub fn step_count(&self) -> usize {
        self.steps.len()
    }

    /// Step `k`: the sprite-only and the open-water image, row-major
    /// `width × height` straight-alpha RGBA.
    pub fn step_images(&self, k: usize) -> (&[[u8; 4]], &[[u8; 4]]) {
        let s = &self.steps[k];
        (&s.sprite, &s.filled)
    }

    /// Animation steps `(current, next)` at `time_ms` and the fraction of
    /// the way to the next one.
    pub fn steps_at(&self, time_ms: u64) -> (usize, usize, f32) {
        let n = self.steps.len();
        if self.step_ms == 0 || n < 2 {
            return (0, 0, 0.0);
        }
        let ticks = time_ms / self.step_ms;
        let n64 = u64::try_from(n).expect("fits");
        let k = usize::try_from(ticks % n64).expect("< n");
        #[allow(clippy::cast_precision_loss)]
        let frac = (time_ms % self.step_ms) as f32 / self.step_ms as f32;
        (k, (k + 1) % n, frac)
    }

    fn index(&self, sx: i32, sy: i32) -> usize {
        usize::try_from(sy.rem_euclid(self.height) * self.width + sx.rem_euclid(self.width))
            .expect("positive")
    }
}

/// Static per-pixel sea data of a scene rectangle.
pub struct WaterField {
    pub rect: Rect,
    /// 0 = no sea at this pixel (terrain at or above sea level, or a land
    /// chunk); `1 + 254·o` = sea with openness `o` ∈ [0, 1]: how much the
    /// pixel shows open water (no sea floor) instead of the sea floor.
    pub cover: Vec<u8>,
    /// Distance to the shore in 1/[`SHORE_SCALE`] tiles, 255 = far.
    pub shore: Vec<u8>,
    /// Water depth (sea level minus terrain height, `HEIGHT` units) in
    /// 1/[`DEPTH_SCALE`] units, 255 = deep or open sea, 0 = no sea.
    pub depth: Vec<u8>,
}

/// Width in tiles over which an island's sea floor fades into open water
/// towards the island border. Islands use different floor textures; the
/// fade lets each continue into the shared open sea without a seam. Tuning
/// value, not from the original (OQ-026 b).
pub const BORDER_FADE_TILES: f32 = 8.0;

/// Per-island inputs of [`water_field`]; computed once per island.
pub struct IslandWater {
    w: usize,
    h: usize,
    heights: Vec<u8>,
    /// Per 8 × 8 tile chunk: contains a non-land tile (the original's
    /// per-chunk sea flag is not traced; OQ-026 b).
    sea_chunk: Vec<bool>,
    /// Openness per height corner: share of open-water tiles (texture 0)
    /// among the 4 × 4 tiles around it, outside the island counting as open,
    /// raised towards the island border ([`BORDER_FADE_TILES`]).
    /// Smooths the switch between the sea floor texture and the flat
    /// open-water colour over about two tiles (approximation, OQ-026 b).
    open: Vec<f32>,
    /// Shore distance per height corner in tiles.
    shore: Vec<f32>,
}

impl IslandWater {
    /// `None` if the island's `HEIGHT` or `GFXID` grid cannot be read.
    pub fn new(island: &Island<'_>) -> Option<Self> {
        let (heights, gfx) = (island.heights().ok()?, island.gfx_ids().ok()?);
        let (w, h) = (usize::from(island.width), usize::from(island.height));
        let cw = w.div_ceil(8);
        let mut sea_chunk = vec![false; cw * h.div_ceil(8)];
        for y in 0..h {
            for x in 0..w {
                let c = iso::corners(&heights, w, h, x, y);
                if c.iter().sum::<i64>() <= 4 * LAND_AVERAGE {
                    sea_chunk[(y / 8) * cw + x / 8] = true;
                }
            }
        }
        let open_tile = |x: isize, y: isize| -> f32 {
            match (usize::try_from(x), usize::try_from(y)) {
                (Ok(x), Ok(y)) if x < w && y < h => {
                    if GroundCell::from_gfx_id(gfx[y * w + x]).texture == 0 {
                        1.0
                    } else {
                        0.0
                    }
                }
                _ => 1.0,
            }
        };
        let mut open = vec![0.0; w * h];
        for y in 0..h {
            for x in 0..w {
                let (xi, yi) = (
                    isize::try_from(x).expect("fits"),
                    isize::try_from(y).expect("fits"),
                );
                let mut sum = 0.0;
                for dy in -2..2 {
                    for dx in -2..2 {
                        sum += open_tile(xi + dx, yi + dy);
                    }
                }
                // Fade the floor out towards the island border, where the
                // open sea takes over.
                let edge = x.min(y).min(w - 1 - x).min(h - 1 - y);
                #[allow(clippy::cast_precision_loss)]
                let border = 1.0 - (edge as f32 / BORDER_FADE_TILES).min(1.0);
                open[y * w + x] = (sum / 16.0).max(border);
            }
        }
        Some(Self {
            w,
            h,
            shore: smooth_shore(&heights, w, h),
            heights,
            sea_chunk,
            open,
        })
    }

    /// Bilinear value of a per-corner field at tile `(tx, ty)` + `(fx, fy)`.
    fn bilinear(&self, field: &[f32], tx: usize, ty: usize, fx: f32, fy: f32) -> f32 {
        let at = |x: usize, y: usize| field[y.min(self.h - 1) * self.w + x.min(self.w - 1)];
        at(tx, ty) * (1.0 - fx) * (1.0 - fy)
            + at(tx + 1, ty) * fx * (1.0 - fy)
            + at(tx, ty + 1) * (1.0 - fx) * fy
            + at(tx + 1, ty + 1) * fx * fy
    }
}

/// The water field of `rect` (scene screen coordinates at `group`): open
/// sea everywhere, except over the tiles of the islands in `placements`,
/// where the terrain under each pixel (projected back onto the sea plane)
/// decides, as the original's depth test would (approximation, OQ-026 b).
/// `water(i)` gives the [`IslandWater`] of island `i`.
pub fn water_field<'w>(
    rect: Rect,
    water: impl Fn(usize) -> Option<&'w IslandWater>,
    placements: &[Placement],
    group: usize,
    lift: Lift,
) -> WaterField {
    let n = usize::try_from(rect.w * rect.h).expect("non-negative");
    let mut cover = vec![255; n];
    let mut shore = vec![255; n];
    let mut depth = vec![255; n];
    let (tw, th) = TILE[group];
    let den = i64::from(tw) * i64::from(th);
    let raise = lift.scaled(SEA_LEVEL, 1, group);
    for p in placements {
        let Some(r) = p.bounds.intersect(rect) else {
            continue;
        };
        let Some(iw) = water(p.index) else {
            continue;
        };
        for sy in r.y..r.y + r.h {
            for sx in r.x..r.x + r.w {
                // Island-local screen point on the sea plane → tile
                // coordinates (times den): x = px/tw + py/th, y = py/th − px/tw.
                let (lx, ly) = (
                    i64::from(sx - p.offset.0),
                    i64::from(sy - p.offset.1 + raise),
                );
                let xn = lx * i64::from(th) + ly * i64::from(tw);
                let yn = ly * i64::from(tw) - lx * i64::from(th);
                let (Ok(tx), Ok(ty)) = (
                    usize::try_from(xn.div_euclid(den)),
                    usize::try_from(yn.div_euclid(den)),
                ) else {
                    continue;
                };
                if tx >= iw.w || ty >= iw.h {
                    continue;
                }
                let i = usize::try_from((sy - rect.y) * rect.w + (sx - rect.x)).expect("inside");
                let (ix, iy) = (xn.rem_euclid(den), yn.rem_euclid(den));
                let under =
                    iso::interpolate(iso::corners(&iw.heights, iw.w, iw.h, tx, ty), ix, iy, den);
                if under >= SEA_LEVEL * den || !iw.sea_chunk[(ty / 8) * iw.w.div_ceil(8) + tx / 8] {
                    cover[i] = 0;
                    shore[i] = 0;
                    depth[i] = 0;
                    continue;
                }
                #[allow(clippy::cast_precision_loss)]
                let (fx, fy) = (ix as f32 / den as f32, iy as f32 / den as f32);
                let o = iw.bilinear(&iw.open, tx, ty, fx, fy);
                let d = iw.bilinear(&iw.shore, tx, ty, fx, fy);
                #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                {
                    cover[i] = 1 + (o.clamp(0.0, 1.0) * 254.0).round() as u8;
                    shore[i] = (d * SHORE_SCALE).clamp(0.0, 255.0) as u8;
                    let units = (SEA_LEVEL * den - under) as f32 / den as f32;
                    depth[i] = (units * DEPTH_SCALE).clamp(0.0, 255.0) as u8;
                }
            }
        }
    }
    WaterField {
        rect,
        cover,
        shore,
        depth,
    }
}

/// A pixel the sea can be drawn onto.
pub trait SeaTarget: Copy {
    fn rgba(self) -> [u8; 4];
    fn from_rgba(c: [u8; 4]) -> Self;
}

/// Straight-alpha RGBA.
impl SeaTarget for [u8; 4] {
    fn rgba(self) -> [u8; 4] {
        self
    }
    fn from_rgba(c: [u8; 4]) -> Self {
        c
    }
}

/// Opaque `0RGB` (window buffers).
impl SeaTarget for u32 {
    fn rgba(self) -> [u8; 4] {
        let [_, r, g, b] = self.to_be_bytes();
        [r, g, b, 255]
    }
    fn from_rgba(c: [u8; 4]) -> Self {
        u32::from(c[0]) << 16 | u32::from(c[1]) << 8 | u32::from(c[2])
    }
}

/// Pixel step of the coarse grid on which the enhanced waves are evaluated
/// and then interpolated, per zoom group. The wave features are a tile or
/// larger, the finest foam detail a sixth of a tile. Tuning value, not from
/// the original (PLAN §0.1, §0.2).
pub const SHADING_STEP: [i32; 3] = [4, 2, 2];

fn lerp4(a: [u8; 4], b: [u8; 4], t: u32, scale: u32) -> [u8; 4] {
    let mix = |x: u8, y: u8| {
        let (x, y) = (u32::from(x), u32::from(y));
        u8::try_from((x * (scale - t) + y * t) / scale).expect("<= 255")
    };
    [
        mix(a[0], b[0]),
        mix(a[1], b[1]),
        mix(a[2], b[2]),
        mix(a[3], b[3]),
    ]
}

/// Draws the sea over `out`, a block of `region.w × region.h` pixels with
/// row stride `stride` showing scene rectangle `region` at `group`.
/// `field(i)` returns `(cover, shore, depth)` of pixel `i` of the region
/// (row-major, `region.w` per row; see [`WaterField`]). `Original` draws
/// the pattern step of `time_ms`; `Enhanced` also blends into the next
/// step, hides the floor in deep water (`iso::deep_water`) and adds the
/// waves of `iso::wave_shade` (not in the original).
#[allow(clippy::too_many_arguments)]
pub fn shade_sea<P: SeaTarget>(
    out: &mut [P],
    stride: usize,
    region: Rect,
    field: impl Fn(usize) -> (u8, u8, u8),
    pattern: &SeaPattern,
    time_ms: u64,
    style: WaterStyle,
    group: usize,
    lift: Lift,
) {
    let (k0, k1, frac) = pattern.steps_at(time_ms);
    let (a, b) = (&pattern.steps[k0], &pattern.steps[k1]);
    #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
    let blend = (frac * 256.0) as u32;
    let to_usize = |v: i32| usize::try_from(v).expect("non-negative");
    let (rw, rh) = (to_usize(region.w), to_usize(region.h));
    // Enhanced: (light, foam) on a grid aligned to multiples of `step` in
    // scene coordinates, so neighbouring regions agree.
    let step = SHADING_STEP[group];
    let (gx0, gy0) = (
        region.x.div_euclid(step) * step,
        region.y.div_euclid(step) * step,
    );
    let gw = to_usize((region.x + region.w - gx0).div_euclid(step) + 2);
    let gh = to_usize((region.y + region.h - gy0).div_euclid(step) + 2);
    let grid: Vec<(f32, f32, f32)> = match style {
        WaterStyle::Original => Vec::new(),
        WaterStyle::Enhanced => {
            let (tw, th) = TILE[group];
            let raise = lift.scaled(SEA_LEVEL, 1, group);
            #[allow(clippy::cast_precision_loss)]
            let (twf, thf) = (tw as f32, th as f32);
            let detail = iso::wave_detail(twf);
            (0..gh)
                .flat_map(|j| (0..gw).map(move |i| (i, j)))
                .map(|(i, j)| {
                    let sx = gx0 + step * i32::try_from(i).expect("fits");
                    let sy = gy0 + step * i32::try_from(j).expect("fits");
                    let (cx, cy) = (
                        to_usize((sx - region.x).clamp(0, region.w - 1)),
                        to_usize((sy - region.y).clamp(0, region.h - 1)),
                    );
                    // Land points too (shore 0), so foam reaches the beach.
                    let (_, shore, _) = field(cy * rw + cx);
                    #[allow(clippy::cast_precision_loss)]
                    let (px, py) = (sx as f32, (sy + raise) as f32);
                    let pos = (px / twf + py / thf, py / thf - px / twf);
                    iso::wave_shade(f32::from(shore) / SHORE_SCALE, pos, time_ms, detail)
                })
                .collect()
        }
    };
    #[allow(clippy::cast_precision_loss)]
    let stepf = step as f32;
    for row in 0..rh {
        let sy = region.y + i32::try_from(row).expect("fits");
        for col in 0..rw {
            let (cover, _, depth) = field(row * rw + col);
            if cover == 0 {
                continue;
            }
            let sx = region.x + i32::try_from(col).expect("fits");
            let i = pattern.index(sx, sy);
            let (mut sprite, mut filled) = (a.sprite[i], a.filled[i]);
            if style == WaterStyle::Enhanced {
                sprite = lerp4(sprite, b.sprite[i], blend, 256);
                filled = lerp4(filled, b.filled[i], blend, 256);
            }
            let dst = &mut out[row * stride + col];
            let mut c = dst.rgba();
            over(&mut c, &sprite);
            // Openness 0..=254; enhanced water also hides the floor where it
            // is deep (smooth across tiles, unlike the floor textures).
            let mut open = u32::from(cover - 1);
            if style == WaterStyle::Enhanced {
                #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                let deep = (iso::deep_water(f32::from(depth) / DEPTH_SCALE) * 254.0) as u32;
                open = open.max(deep);
            }
            if open > 0 {
                c = lerp4(c, filled, open, 254);
            }
            if style == WaterStyle::Enhanced {
                let (gx, gy) = ((sx - gx0).div_euclid(step), (sy - gy0).div_euclid(step));
                #[allow(clippy::cast_precision_loss)]
                let (fx, fy) = (
                    ((sx - gx0) - gx * step) as f32 / stepf,
                    ((sy - gy0) - gy * step) as f32 / stepf,
                );
                let at = |x: i32, y: i32| grid[to_usize(y) * gw + to_usize(x)];
                let (p00, p10, p01, p11) = (
                    at(gx, gy),
                    at(gx + 1, gy),
                    at(gx, gy + 1),
                    at(gx + 1, gy + 1),
                );
                let mix = |s: fn((f32, f32, f32)) -> f32| {
                    (s(p00) * (1.0 - fx) + s(p10) * fx) * (1.0 - fy)
                        + (s(p01) * (1.0 - fx) + s(p11) * fx) * fy
                };
                c = iso::shade_apply(c, (mix(|p| p.0), mix(|p| p.1), mix(|p| p.2)));
            }
            *dst = P::from_rgba(c);
        }
    }
}
