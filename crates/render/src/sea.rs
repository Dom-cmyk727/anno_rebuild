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
use crate::scene::Rect;

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

/// Water depths in [`WaterField::depth`] are stored in 1/4 `HEIGHT` units
/// around [`DEPTH_BIAS`] (−32 … +31.75 units; sea level is 32). `gpu.wgsl`
/// uses the same scale.
pub const DEPTH_SCALE: f32 = 4.0;

/// Passes of a 3 × 3 box blur over the shore distance field: the chamfer
/// distance has octagonal contours with kinks at the tile corners, and it
/// traces every bulge and shoal of the coast. Wave crests follow this
/// field, so it is smoothed strongly (radius ≈ 3 tiles); breaking and tint
/// follow the water depth instead. Land stays 0.
const SHORE_SMOOTHING: usize = 10;

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

    /// Premultiplied-alpha average (0–1) of the 2 × 2 pattern pixels at
    /// `(sx, sy)` .. `(sx + 1, sy + 1)` of step image `img`.
    fn smoothed(&self, img: &[[u8; 4]], sx: i32, sy: i32) -> [f32; 4] {
        let mut sum = [0.0; 4];
        for (dx, dy) in [(0, 0), (1, 0), (0, 1), (1, 1)] {
            let p = img[self.index(sx + dx, sy + dy)];
            let a = f32::from(p[3]) / 255.0;
            for c in 0..3 {
                sum[c] += f32::from(p[c]) / 255.0 * a;
            }
            sum[3] += a;
        }
        sum.map(|v| v / 4.0)
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
    /// Signed water depth over the smooth terrain of the enhanced water
    /// ([`smooth_depth`]), `128 + depth · DEPTH_SCALE`, clamped; 255 = deep
    /// or open sea.
    pub depth: Vec<u8>,
    /// Bit 0: water in the original (terrain under the pixel, interpolated
    /// as the original's triangles, below sea level). Bit 1: enhanced river
    /// water (`depth` is then the river depth).
    pub flags: Vec<u8>,
}

/// Bias of [`WaterField::depth`]: the byte value of depth 0.
pub const DEPTH_BIAS: f32 = 128.0;

/// Terrain height (`HEIGHT` units) of the enhanced waterline at tile
/// position `(x, y)`: a cubic B-spline through the corner heights (smooth,
/// no overshoot; the original's triangles make the waterline straight
/// between tile corners) plus fixed value noise of amplitude
/// `Waves::shore_noise`, so beaches get bays and bulges.
fn smooth_height(heights: &[u8], w: usize, h: usize, x: f32, y: f32) -> f32 {
    let weights = |t: f32| {
        let t2 = t * t;
        let t3 = t2 * t;
        [
            (1.0 - t).powi(3) / 6.0,
            (3.0 * t3 - 6.0 * t2 + 4.0) / 6.0,
            (-3.0 * t3 + 3.0 * t2 + 3.0 * t + 1.0) / 6.0,
            t3 / 6.0,
        ]
    };
    let (fx, fy) = (x.floor(), y.floor());
    let (wx, wy) = (weights(x - fx), weights(y - fy));
    #[allow(clippy::cast_possible_truncation)]
    let (ix, iy) = (fx as i64, fy as i64);
    let at = |cx: i64, cy: i64| {
        let cx = usize::try_from(cx.max(0)).expect("non-negative").min(w - 1);
        let cy = usize::try_from(cy.max(0)).expect("non-negative").min(h - 1);
        f32::from(heights[cy * w + cx])
    };
    let mut v = 0.0;
    for (j, wyj) in wy.iter().enumerate() {
        for (i, wxi) in wx.iter().enumerate() {
            let (di, dj) = (
                i64::try_from(i).expect("< 4") - 1,
                i64::try_from(j).expect("< 4") - 1,
            );
            v += wxi * wyj * at(ix + di, iy + dj);
        }
    }
    let amp = iso::Waves::DEFAULT.shore_noise;
    let n = (iso::noise3(x * 0.9, y * 0.9, 11.0) - 0.5) * 2.0 * amp
        + (iso::noise3(x * 2.7, y * 2.7, 23.0) - 0.5) * 0.6 * amp;
    v + n
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
    /// River surface level per height corner (`HEIGHT` units), `NaN` away
    /// from rivers (enhanced water only, [`IslandWater::new`]).
    river: Vec<f32>,
}

/// A river piece for [`IslandWater::new`]: tile `(x, y)`, footprint
/// `(w, h)` in tiles, height byte.
pub type RiverPiece = (u16, u16, u8, u8, u8);

/// River surface above the river objects' height byte (`HEIGHT` units):
/// river items lie ≈ 9 units below the surrounding terrain, and this level
/// fills the painted riverbed (tuned visually, as `scene::RIVER_TOLERANCE`;
/// OQ-026 b).
pub const RIVER_LEVEL: f32 = 9.0;

/// Openness (of 254) of enhanced river water: how much of the open-water
/// colour covers the riverbed. Tuning value (not original).
pub const RIVER_OPENNESS: u8 = 100;

impl IslandWater {
    /// `None` if the island's `HEIGHT` or `GFXID` grid cannot be read.
    /// `rivers`: the island's river pieces, whose footprints get a river
    /// surface level (item height + [`RIVER_LEVEL`]) that is smoothed along
    /// the river and spread one corner beyond it.
    pub fn new(island: &Island<'_>, rivers: &[RiverPiece]) -> Option<Self> {
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
        // River level: average over the pieces covering each corner, then
        // two passes of averaging with defined neighbours (smooth along
        // the river, one corner beyond its footprints).
        let (cw1, ch1) = (w, h);
        let mut sum = vec![0.0f32; cw1 * ch1];
        let mut n = vec![0.0f32; cw1 * ch1];
        for &(x, y, fw, fh, z) in rivers {
            for dy in 0..=usize::from(fh) {
                for dx in 0..=usize::from(fw) {
                    let (cx, cy) = (usize::from(x) + dx, usize::from(y) + dy);
                    if cx < cw1 && cy < ch1 {
                        sum[cy * cw1 + cx] += f32::from(z) + RIVER_LEVEL;
                        n[cy * cw1 + cx] += 1.0;
                    }
                }
            }
        }
        let mut river: Vec<f32> = sum
            .iter()
            .zip(&n)
            .map(|(&s, &k)| if k > 0.0 { s / k } else { f32::NAN })
            .collect();
        for _ in 0..2 {
            let prev = river.clone();
            for y in 0..ch1 {
                for x in 0..cw1 {
                    let (mut s, mut k) = (0.0, 0.0);
                    for yy in y.saturating_sub(1)..=(y + 1).min(ch1 - 1) {
                        for xx in x.saturating_sub(1)..=(x + 1).min(cw1 - 1) {
                            let v = prev[yy * cw1 + xx];
                            if !v.is_nan() {
                                s += v;
                                k += 1.0;
                            }
                        }
                    }
                    if k > 0.0 {
                        river[y * cw1 + x] = s / k;
                    }
                }
            }
        }
        Some(Self {
            w,
            h,
            river,
            shore: smooth_shore(&heights, w, h),
            heights,
            sea_chunk,
            open,
        })
    }

    /// River surface level at tile `(tx, ty)` + `(fx, fy)`: bilinear over
    /// the corners that have one; `None` away from rivers.
    fn river_level(&self, tx: usize, ty: usize, fx: f32, fy: f32) -> Option<f32> {
        let at = |x: usize, y: usize| self.river[y.min(self.h - 1) * self.w + x.min(self.w - 1)];
        let (mut s, mut k) = (0.0, 0.0);
        for (v, wt) in [
            (at(tx, ty), (1.0 - fx) * (1.0 - fy)),
            (at(tx + 1, ty), fx * (1.0 - fy)),
            (at(tx, ty + 1), (1.0 - fx) * fy),
            (at(tx + 1, ty + 1), fx * fy),
        ] {
            if !v.is_nan() {
                s += v * wt;
                k += wt;
            }
        }
        (k > 0.25).then(|| s / k)
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

/// The ground seen at each pixel of a scene rectangle: island index
/// ([`NO_ISLAND`] where no ground was drawn) and the tile position of the
/// visible terrain point (`iso::GroundProbe`).
pub struct SceneProbe {
    pub island: Vec<u32>,
    pub pos: Vec<[f32; 2]>,
}

/// [`SceneProbe::island`] where no island ground is drawn.
pub const NO_ISLAND: u32 = u32::MAX;

/// The water field of `rect` (scene screen coordinates) from the ground seen
/// at each pixel (`probe`): open sea where no island ground is drawn; else
/// the original's water where the visible terrain point lies below
/// [`iso::WATERLINE`] — the original's depth test of the sea sprites against
/// the terrain — plus the smooth waterline of the enhanced water
/// ([`smooth_height`]). `water(i)` gives the [`IslandWater`] of island `i`.
pub fn water_field<'w>(
    rect: Rect,
    probe: &SceneProbe,
    water: impl Fn(usize) -> Option<&'w IslandWater>,
) -> WaterField {
    let n = usize::try_from(rect.w * rect.h).expect("non-negative");
    let mut cover = vec![255; n];
    let mut shore = vec![255; n];
    let mut depth = vec![255; n];
    let mut flags = vec![1; n];
    #[allow(clippy::cast_precision_loss)]
    let waterline = iso::WATERLINE as f32;
    let edge = iso::Waves::DEFAULT.shore_edge;
    for i in 0..n {
        let island = probe.island[i];
        if island == NO_ISLAND {
            continue;
        }
        let Some(iw) = water(usize::try_from(island).expect("fits")) else {
            continue;
        };
        let [x, y] = probe.pos[i];
        let under = iso::terrain_height(&iw.heights, iw.w, iw.h, x, y);
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let (tx, ty) = (
            (x.max(0.0) as usize).min(iw.w - 1),
            (y.max(0.0) as usize).min(iw.h - 1),
        );
        #[allow(clippy::cast_precision_loss)]
        let (fx, fy) = (
            (x - tx as f32).clamp(0.0, 1.0),
            (y - ty as f32).clamp(0.0, 1.0),
        );
        let sea_chunk = iw.sea_chunk[(ty / 8) * iw.w.div_ceil(8) + tx / 8];
        let original_water = under < waterline && sea_chunk;
        let smooth = smooth_height(&iw.heights, iw.w, iw.h, x, y);
        let sd = waterline - smooth;
        // Enhanced river water (flag bit 1): river level over the smooth
        // terrain, where no sea is.
        if !original_water && sd <= 0.0 {
            let level = iw.river_level(tx, ty, fx, fy);
            if let Some(level) = level {
                let rd = level - smooth;
                if rd > -iso::Waves::DEFAULT.shore_edge {
                    #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                    {
                        // River water shows its bed less than the sea
                        // shallows (openness ≈ 0.4).
                        cover[i] = 1 + RIVER_OPENNESS;
                        shore[i] = 255;
                        depth[i] = (DEPTH_BIAS + rd * DEPTH_SCALE).clamp(0.0, 255.0) as u8;
                        flags[i] = 2;
                    }
                    continue;
                }
            }
        }
        // Enhanced water runs up to `runup` above the waterline.
        if !sea_chunk || (!original_water && sd <= -(edge + iso::Waves::DEFAULT.runup)) {
            cover[i] = 0;
            shore[i] = 0;
            depth[i] = 0;
            flags[i] = 0;
            continue;
        }
        let o = iw.bilinear(&iw.open, tx, ty, fx, fy);
        let d = iw.bilinear(&iw.shore, tx, ty, fx, fy);
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        {
            cover[i] = 1 + (o.clamp(0.0, 1.0) * 254.0).round() as u8;
            shore[i] = (d * SHORE_SCALE).clamp(0.0, 255.0) as u8;
            depth[i] = (DEPTH_BIAS + sd * DEPTH_SCALE).clamp(0.0, 255.0) as u8;
            flags[i] = u8::from(original_water);
        }
    }
    WaterField {
        rect,
        cover,
        shore,
        depth,
        flags,
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

/// Straight-alpha RGBA from premultiplied 0–1 values.
fn unpremultiply(p: [f32; 4]) -> [u8; 4] {
    let a = p[3];
    let channel = |v: f32| {
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let out = (v * 255.0).round().clamp(0.0, 255.0) as u8;
        out
    };
    if a <= 0.0 {
        return [0; 4];
    }
    [
        channel(p[0] / a),
        channel(p[1] / a),
        channel(p[2] / a),
        channel(a),
    ]
}

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
    field: impl Fn(usize) -> (u8, u8, u8, u8),
    pattern: &SeaPattern,
    time_ms: u64,
    style: WaterStyle,
    group: usize,
    lift: Lift,
) {
    let (k0, k1, frac) = pattern.steps_at(time_ms);
    let (a, b) = (&pattern.steps[k0], &pattern.steps[k1]);
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
                    let (_, shore, depth, _) = field(cy * rw + cx);
                    #[allow(clippy::cast_precision_loss)]
                    let (px, py) = (sx as f32, (sy + raise) as f32);
                    let pos = (px / twf + py / thf, py / thf - px / twf);
                    let depth = (f32::from(depth) - DEPTH_BIAS) / DEPTH_SCALE;
                    iso::wave_shade(f32::from(shore) / SHORE_SCALE, depth, pos, time_ms, detail)
                })
                .collect()
        }
    };
    #[allow(clippy::cast_precision_loss)]
    let stepf = step as f32;
    let raise_px = lift.scaled(SEA_LEVEL, 1, group);
    #[allow(clippy::cast_precision_loss)]
    let (twf, thf) = (TILE[group].0 as f32, TILE[group].1 as f32);
    for row in 0..rh {
        let sy = region.y + i32::try_from(row).expect("fits");
        for col in 0..rw {
            let (cover, _, depth, flags) = field(row * rw + col);
            if cover == 0 {
                continue;
            }
            // Original: the original's waterline. Enhanced: the smooth
            // waterline with a soft edge (`smooth_height`).
            let sd = (f32::from(depth) - DEPTH_BIAS) / DEPTH_SCALE;
            let alpha = match style {
                WaterStyle::Original if flags & 1 == 0 => continue,
                WaterStyle::Original => 1.0,
                WaterStyle::Enhanced => {
                    let w = iso::Waves::DEFAULT;
                    // The waterline moves with the swash (not on rivers).
                    let run = if sd < w.swash_depth && flags & 2 == 0 {
                        let (_, shore, _, _) = field(row * rw + col);
                        let sx = region.x + i32::try_from(col).expect("fits");
                        #[allow(clippy::cast_precision_loss)]
                        let (px, py) = (sx as f32, (sy + raise_px) as f32);
                        let pos = (px / twf + py / thf, py / thf - px / twf);
                        iso::swash_runup(f32::from(shore) / SHORE_SCALE, pos, time_ms)
                    } else {
                        0.0
                    };
                    iso::smoothstep_pub(-w.shore_edge, w.shore_edge, sd + run)
                }
            };
            if alpha <= 0.0 {
                continue;
            }
            let sx = region.x + i32::try_from(col).expect("fits");
            let i = pattern.index(sx, sy);
            let (mut sprite, mut filled) = (a.sprite[i], a.filled[i]);
            if style == WaterStyle::Enhanced {
                // 2 × 2 average of the pattern: the checkerboard of the sea
                // sprite becomes a continuous surface; blended into the next
                // animation step.
                let smooth = |img: &[[u8; 4]]| pattern.smoothed(img, sx, sy);
                let mix = |p: [f32; 4], q: [f32; 4]| -> [f32; 4] {
                    std::array::from_fn(|c| p[c] + (q[c] - p[c]) * frac)
                };
                sprite = unpremultiply(mix(smooth(&a.sprite), smooth(&b.sprite)));
                filled = unpremultiply(mix(smooth(&a.filled), smooth(&b.filled)));
            }
            let dst = &mut out[row * stride + col];
            let below = dst.rgba();
            let mut c = below;
            over(&mut c, &sprite);
            // Openness 0..=254; enhanced water also hides the floor where it
            // is deep (smooth across tiles, unlike the floor textures).
            let mut open = u32::from(cover - 1);
            if style == WaterStyle::Enhanced {
                #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                let deep = (iso::deep_water(sd.max(0.0)) * 254.0) as u32;
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
                let shade = if flags & 2 == 0 {
                    (mix(|p| p.0), mix(|p| p.1), mix(|p| p.2))
                } else {
                    #[allow(clippy::cast_precision_loss)]
                    let (px, py) = (sx as f32, (sy + raise_px) as f32);
                    iso::river_shade((px / twf + py / thf, py / thf - px / twf), sd, time_ms)
                };
                c = iso::shade_apply(c, shade);
            }
            if alpha < 1.0 {
                #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
                let t = (alpha * 256.0) as u32;
                c = lerp4(below, c, t, 256);
            }
            *dst = P::from_rgba(c);
        }
    }
}
