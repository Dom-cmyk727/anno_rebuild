//! CPU isometric renderer for one island: ground and placed objects into an
//! RGBA canvas. A reference implementation for the projection rules (and for
//! tests); the interactive renderer will use the same rules.
//!
//! Projection rules (spec: `docs/render/iso.md`; all derived from sprite
//! measurements and visual checks, OQ-026):
//! - Tile `(x, y)` is a diamond `TILE[group]` wide/high whose top vertex is
//!   at screen `((x − y) · w/2, (x + y) · h/2)`.
//! - Ground: the diamond shows its `GroundCell`; the point with tile
//!   coordinates `(s, t)` ∈ [0, 1)² shows `GroundCell::texel` (cells are
//!   1/6 of a 256 × 256 `Felder.tex` texture).
//! - Objects: the sprite's anchor is put on the centre of the footprint
//!   diamond. Frame = `rotation · animation frames` (first animation frame);
//!   sets with a single rotation use frame 0.
//! - Draw order: ground, then objects by the depth of their footprint's far
//!   corner (`x + w + y + h`), then by `x`.
//! - Elevation: `HEIGHT` values are heights at tile corners (D: `AnnoWelt.dll`
//!   per-corner grid; that `HEIGHT[x, y]` is corner `(x, y)` is likely) —
//!   tile `(x, y)` spans the heights of `(x, y)`, `(x+1, y)`, `(x, y+1)`,
//!   `(x+1, y+1)` as two triangles split along the `(x, y+1)`–`(x+1, y)`
//!   diagonal — and raise the ground by `height · lift`
//!   pixels at zoom group 0; [`HEIGHT_LIFT`] is the original's scale.
//! - Alpha levels blend (`anno_assets::figure::LEVEL_ALPHA`); shadow frames
//!   darken to 5/8 before their sprite is drawn.

use std::collections::{BTreeMap, BTreeSet};
use std::sync::{Arc, RwLock};

use anno_assets::building;
use anno_assets::island::GroundTextures;
use anno_assets::texture::Rgba;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{Frame, GfxSet};
use anno_formats::insel::{GroundCell, Island, PlacedObject};

/// Tile diamond `(width, height)` per zoom group (P: flat 1 × 1 `Anlagen.dat`
/// sprites are 84 × 42/43, 42 × 21/22 and 20 × 10).
pub const TILE: [(i32, i32); 3] = [(84, 42), (42, 21), (20, 10)];

/// An RGBA canvas whose pixel `(0, 0)` is screen point `origin`.
pub struct Canvas {
    pub width: usize,
    pub height: usize,
    pub origin: (i32, i32),
    pub pixels: Vec<u8>,
}

/// A shadow pixel: 37.5 % black over the destination, i.e. the original's
/// `dst/2 + dst/8` (5/8 brightness; D: `AnnoDisplay.dll` `0x10019f80`) on an
/// opaque pixel, and a correct translucent pixel on a transparent layer.
/// Overlapping shadows compound, as in `tBrushShadR`.
pub const SHADOW: [u8; 4] = [0, 0, 0, 96];

/// `dst = src over dst` for 8-bit straight-alpha RGBA.
pub fn over(dst: &mut [u8; 4], src: &[u8; 4]) {
    let a = u32::from(src[3]);
    match a {
        0 => {}
        255 => *dst = *src,
        _ => {
            let da = u32::from(dst[3]);
            // Output alpha and colour in units of 1/255², straight alpha.
            let out_a = a * 255 + da * (255 - a);
            for (d, &c) in dst[..3].iter_mut().zip(&src[..3]) {
                let num = u32::from(c) * a * 255 + u32::from(*d) * da * (255 - a);
                *d = u8::try_from(num / out_a).expect("<= 255");
            }
            dst[3] = u8::try_from(out_a / 255).expect("<= 255");
        }
    }
}

impl Canvas {
    /// A canvas covering the whole island at `group`, with a margin for
    /// sprites that rise above the top tile.
    pub fn for_island(island: &Island<'_>, group: usize, margin: i32) -> Self {
        let (tw, th) = TILE[group];
        let (w, h) = (i32::from(island.width), i32::from(island.height));
        let left = -h * tw / 2;
        let right = w * tw / 2;
        let bottom = (w + h) * th / 2;
        let width = usize::try_from(right - left).expect("positive");
        let height = usize::try_from(bottom + margin).expect("positive");
        Self {
            width,
            height,
            origin: (left, -margin),
            pixels: vec![0; width * height * 4],
        }
    }

    fn index(&self, sx: i32, sy: i32) -> Option<usize> {
        let (x, y) = (sx - self.origin.0, sy - self.origin.1);
        match (usize::try_from(x), usize::try_from(y)) {
            (Ok(x), Ok(y)) if x < self.width && y < self.height => Some((y * self.width + x) * 4),
            _ => None,
        }
    }

    /// Draws `rgba` over the pixel (straight alpha, see [`over`]).
    pub(crate) fn put(&mut self, sx: i32, sy: i32, rgba: [u8; 4]) {
        let Some(o) = self.index(sx, sy) else { return };
        let dst: &mut [u8; 4] = (&mut self.pixels[o..o + 4]).try_into().expect("4 bytes");
        over(dst, &rgba);
    }

    /// Whether the screen rectangle `(x, y, w, h)` touches the canvas.
    pub fn overlaps(&self, x: i32, y: i32, w: i32, h: i32) -> bool {
        let (cx, cy) = self.origin;
        let (cw, ch) = (
            i32::try_from(self.width).expect("fits"),
            i32::try_from(self.height).expect("fits"),
        );
        x < cx + cw && x + w > cx && y < cy + ch && y + h > cy
    }

    pub fn into_rgba(self) -> Rgba {
        let dim = |n: usize| u16::try_from(n).expect("canvas fits u16");
        Rgba {
            width: dim(self.width),
            height: dim(self.height),
            pixels: self.pixels,
        }
    }
}

/// Screen position of the top vertex of tile `(x, y)`.
pub fn tile_top(x: i32, y: i32, group: usize) -> (i32, i32) {
    let (tw, th) = TILE[group];
    ((x - y) * tw / 2, (x + y) * th / 2)
}

/// Pixels per `HEIGHT` unit at zoom group 0 used by the original: one unit
/// is 0.04 world units (D: `AnnoWelt.dll` height table `0x100E35E0`) and a
/// world unit of height is 51.477 px at zoom 1.0 (D: `AnnoFrame.dll` camera
/// `0x10071d70`: scale 59.1633, tilt 29.53°), so ≈ 2.059 px.
pub const HEIGHT_LIFT: Lift = Lift {
    num: 2059,
    den: 1000,
};

/// Pixels per `HEIGHT` unit at zoom group 0, as a fraction `num / den`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Lift {
    pub num: i32,
    pub den: i32,
}

impl Lift {
    pub const NONE: Lift = Lift { num: 0, den: 1 };

    /// Screen offset (upwards) of a tile with height `h` at `group`.
    pub fn pixels(self, h: u8, group: usize) -> i32 {
        self.scaled(i64::from(h), 1, group)
    }

    /// Offset for the height `h_num / h_den`.
    pub(crate) fn scaled(self, h_num: i64, h_den: i64, group: usize) -> i32 {
        let num = h_num * i64::from(self.num) * i64::from(TILE[group].0);
        let den = h_den * i64::from(self.den) * i64::from(TILE[0].0);
        i32::try_from(num / den).expect("offset fits i32")
    }
}

/// Corner heights of tile `(x, y)`: `[(x, y), (x+1, y), (x, y+1), (x+1, y+1)]`,
/// clamped at the island edge.
pub(crate) fn corners(heights: &[u8], w: usize, h: usize, x: usize, y: usize) -> [i64; 4] {
    let at = |cx: usize, cy: usize| i64::from(heights[cy.min(h - 1) * w + cx.min(w - 1)]);
    [at(x, y), at(x + 1, y), at(x, y + 1), at(x + 1, y + 1)]
}

/// Height at tile position `(i/n, j/n)` (`i` along tile x, `j` along tile y),
/// times `n`. The tile is two triangles split along the fixed diagonal from
/// corner B `(x, y+1)` to C `(x+1, y)`: A-B-C for `i + j ≤ n`, C-B-D beyond
/// (D: `AnnoWelt.dll` mesh builder `0x1009b920`, indices A,B,C and C,B,D).
pub(crate) fn interpolate(c: [i64; 4], i: i64, j: i64, n: i64) -> i64 {
    let [a, cc, b, d] = c;
    if i + j <= n {
        a * n + i * (cc - a) + j * (b - a)
    } else {
        d * n + (n - i) * (b - d) + (n - j) * (cc - d)
    }
}

/// Draws every ground tile, back to front.
pub fn draw_ground(
    canvas: &mut Canvas,
    island: &Island<'_>,
    textures: &GroundTextures,
    group: usize,
    lift: Lift,
) {
    let (Ok(gfx), Ok(heights)) = (island.gfx_ids(), island.heights()) else {
        return;
    };
    let size = (usize::from(island.width), usize::from(island.height));
    draw_ground_grids(canvas, size, &gfx, &heights, textures, group, lift);
}

/// [`draw_ground`] from an island's decoded `GFXID` and `HEIGHT` grids
/// (`size` = width, height in tiles). Only tiles whose screen rectangle
/// (raised by at most the highest corner) can touch the canvas are drawn,
/// back to front by `(x + y, x)`.
pub fn draw_ground_grids(
    canvas: &mut Canvas,
    (w, hgt): (usize, usize),
    gfx: &[u16],
    heights: &[u8],
    textures: &GroundTextures,
    group: usize,
    lift: Lift,
) {
    if w == 0 || hgt == 0 || gfx.len() < w * hgt || heights.len() < w * hgt {
        return;
    }
    #[allow(clippy::cast_precision_loss)]
    let (tw, th, tw0) = (TILE[group].0 as f32, TILE[group].1 as f32, TILE[0].0 as f32);
    #[allow(clippy::cast_precision_loss)]
    let px_per_unit = lift.num as f32 / lift.den as f32 * tw / tw0;
    // Tile (x, y) with u = x − y, v = x + y covers screen x in
    // [(u − 1)·tw/2, (u + 1)·tw/2] and y in [v·th/2 − raise, (v + 2)·th/2].
    let raise = f32::from(heights.iter().copied().max().unwrap_or(0)) * px_per_unit;
    #[allow(clippy::cast_precision_loss)]
    let (x0, y0) = (canvas.origin.0 as f32, canvas.origin.1 as f32);
    #[allow(clippy::cast_precision_loss)]
    let (x1, y1) = (x0 + canvas.width as f32, y0 + canvas.height as f32);
    #[allow(clippy::cast_possible_truncation)]
    let (umin, umax, vmin, vmax) = (
        (2.0 * x0 / tw).floor() as i64 - 1,
        (2.0 * x1 / tw).ceil() as i64 + 1,
        (2.0 * y0 / th).floor() as i64 - 2,
        (2.0 * (y1 + raise) / th).ceil() as i64 + 1,
    );
    let (wi, hi) = (
        i64::try_from(w).expect("fits"),
        i64::try_from(hgt).expect("fits"),
    );
    let mut order = Vec::new();
    for v in vmin.max(0)..=vmax.min(wi + hi - 2) {
        let xa = (v + umin + 1).div_euclid(2).max(v - hi + 1).max(0);
        let xb = (v + umax).div_euclid(2).min(v).min(wi - 1);
        for x in xa..=xb {
            let t = usize::try_from((v - x) * wi + x).expect("inside");
            order.push(t);
        }
    }
    for t in order {
        let cell = GroundCell::from_gfx_id(gfx[t]);
        let c = corners(heights, w, hgt, t % w, t / w);
        #[allow(clippy::cast_precision_loss)]
        let (x, y) = ((t % w) as f32, (t / w) as f32);
        #[allow(clippy::cast_precision_loss)]
        let screen = |cx: f32, cy: f32, h: i64| {
            (
                (cx - cy) * tw / 2.0,
                (cx + cy) * th / 2.0 - h as f32 * px_per_unit,
            )
        };
        // Corners A (x, y), C (x+1, y), B (x, y+1), D (x+1, y+1).
        let a = screen(x, y, c[0]);
        let cc = screen(x + 1.0, y, c[1]);
        let b = screen(x, y + 1.0, c[2]);
        let d = screen(x + 1.0, y + 1.0, c[3]);
        let texel = |s: f32, tt: f32| {
            const N: usize = 4096;
            #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
            let q = |v: f32| ((v * N as f32) as usize).min(N - 1);
            textures.cell_pixel(cell, q(s), q(tt), N)
        };
        // The two triangles of the tile, split along B–C as the original.
        raster_triangle(
            canvas,
            [a, b, cc],
            [(0.0, 0.0), (0.0, 1.0), (1.0, 0.0)],
            texel,
        );
        raster_triangle(
            canvas,
            [cc, b, d],
            [(1.0, 0.0), (0.0, 1.0), (1.0, 1.0)],
            texel,
        );
    }
}

/// Fills a screen triangle (scene screen coordinates); each covered pixel
/// centre gets `shade(s, t)` with `(s, t)` interpolated from `uv`. A planar
/// triangle projects affinely, so this is exact; every pixel is drawn once.
fn raster_triangle(
    canvas: &mut Canvas,
    v: [(f32, f32); 3],
    uv: [(f32, f32); 3],
    shade: impl Fn(f32, f32) -> [u8; 4],
) {
    let edge = |p: (f32, f32), q: (f32, f32), r: (f32, f32)| {
        (q.0 - p.0) * (r.1 - p.1) - (q.1 - p.1) * (r.0 - p.0)
    };
    let area = edge(v[0], v[1], v[2]);
    if area.abs() < 1e-6 {
        return;
    }
    #[allow(clippy::cast_possible_truncation)]
    let (x0, x1, y0, y1) = (
        v[0].0.min(v[1].0).min(v[2].0).floor() as i32,
        v[0].0.max(v[1].0).max(v[2].0).ceil() as i32,
        v[0].1.min(v[1].1).min(v[2].1).floor() as i32,
        v[0].1.max(v[1].1).max(v[2].1).ceil() as i32,
    );
    if !canvas.overlaps(x0, y0, x1 - x0 + 1, y1 - y0 + 1) {
        return;
    }
    let (cx0, cy0) = canvas.origin;
    let (cw, ch) = (
        i32::try_from(canvas.width).expect("fits"),
        i32::try_from(canvas.height).expect("fits"),
    );
    // Small tolerance so shared edges leave no cracks.
    let eps = -1e-3 * area.abs();
    for py in y0.max(cy0)..=y1.min(cy0 + ch - 1) {
        for px in x0.max(cx0)..=x1.min(cx0 + cw - 1) {
            #[allow(clippy::cast_precision_loss)]
            let p = (px as f32 + 0.5, py as f32 + 0.5);
            let (w0, w1, w2) = (
                edge(v[1], v[2], p) * area.signum(),
                edge(v[2], v[0], p) * area.signum(),
                edge(v[0], v[1], p) * area.signum(),
            );
            if w0 < eps || w1 < eps || w2 < eps {
                continue;
            }
            let n = area.abs();
            let (b0, b1, b2) = (w0 / n, w1 / n, w2 / n);
            let s = (b0 * uv[0].0 + b1 * uv[1].0 + b2 * uv[2].0).clamp(0.0, 1.0);
            let t = (b0 * uv[0].1 + b1 * uv[1].1 + b2 * uv[2].1).clamp(0.0, 1.0);
            canvas.put(px, py, shade(s, t));
        }
    }
}

/// Outcome of [`draw_objects`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct DrawStats {
    pub drawn: usize,
    /// Objects whose `ANLAGEN` entry has no sprite set (ids 1 and 2 on the
    /// corpus islands checked; invisible markers).
    pub invisible: usize,
    /// Objects that should be drawable but are not (missing entry, zoom
    /// group, frame or decode error).
    pub failed: usize,
}

/// `ANLAGEN` entry of the open-sea surface (`MEER`, `Meer\Meer01_001.tga`,
/// 64 `BSH_RGBWZ` frames; D: `AnnoWelt.dll` `0x1009f4c0`). Original value;
/// to become content configuration (PLAN §0.1).
pub const SEA_ANLAGE: u32 = 2801;
/// Sea level in `HEIGHT` units: 1.28 world units / 0.04 (D: `AnnoWelt.dll`
/// `0x100E3A20`, set at `0x1009d010`). Original value (PLAN §0.1).
pub const SEA_LEVEL: i64 = 32;
/// A tile counts as land when the average of its corner heights exceeds
/// this (D: `AnnoWelt.dll` classifier `0x100900c0`).
pub(crate) const LAND_AVERAGE: i64 = 19;

/// Display kind of water objects: byte 2 of the sprite-set prefix is 10
/// (`tHausAnimWasser`) exactly for sea, rivers, waterfalls and surf (D:
/// `AnnoDisplay.dll` factory `0x10025d30`; the prefix byte as its source is
/// likely; P: all `BSH_RGBWZ` entries 2701–2708, 2801, 2802).
pub const KIND_WATER: u8 = 10;

/// `ANLAGEN` entry of surf (`Brandung01`): the only object whose animation
/// phase the original overrides on load, with `(3y + 5x) mod 32` (D:
/// `AnnoWelt.dll` `FLUESSE` loader `0x1007aa40`, test at `0x1007ab58`).
/// Original value; to become content configuration (PLAN §0.1).
pub const SURF_ANLAGE: u32 = 2708;

/// Which placed objects [`draw_objects`] draws.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Objects {
    All,
    /// Everything except water objects (static layer).
    NotWater,
    /// Water objects below sea level (surf on the sea floor), drawn before
    /// the sea surface so it covers them with its checkerboard.
    WaterBelowSea,
    /// Water objects at or above sea level (rivers), drawn after the sea.
    WaterAboveSea,
}

/// Decoded frames, shared between renders and threads. Key: `(ANLAGEN id,
/// sprite set, shadow, zoom group, frame index)`.
#[derive(Default)]
pub struct SpriteCache {
    frames: RwLock<BTreeMap<SpriteKey, Option<Arc<Rgba>>>>,
}

/// `(ANLAGEN id, sprite set, shadow, zoom group, frame index)`.
pub type SpriteKey = (u32, usize, bool, u32, usize);

impl SpriteCache {
    /// The decoded frame `key` (`frame` = its header), decoded on first use.
    pub fn get(
        &self,
        anlagen: &AnlagenFile<'_>,
        key: SpriteKey,
        frame: &Frame,
    ) -> Option<Arc<Rgba>> {
        if let Some(hit) = self.frames.read().expect("no panic").get(&key) {
            return hit.clone();
        }
        let entry = anlagen.entry(key.0)?;
        let img = building::to_rgba(anlagen, entry, key.3, frame)
            .ok()
            .map(Arc::new);
        self.frames
            .write()
            .expect("no panic")
            .insert(key, img.clone());
        img
    }
}

/// Animation of a sprite set from its `ANIM` record: bytes 2–3 = frames per
/// animation, bytes 4–5 = milliseconds per frame (bit 25 of the u32 at 4 is
/// a flag of unknown meaning). P: frames per set = animation frames × 1 or
/// × 4 rotations in every set; durations 50–1000 ms.
pub(crate) fn animation(set: &GfxSet<'_>) -> (usize, u64) {
    let anim = set
        .records
        .iter()
        .find(|r| r.name() == "ANIM")
        .map(|r| r.payload);
    let frames = anim
        .and_then(|a| a.get(2..4))
        .map_or(1, |b| usize::from(u16::from_le_bytes([b[0], b[1]])).max(1));
    let ms = anim
        .and_then(|a| a.get(4..6))
        .map_or(0, |b| u64::from(u16::from_le_bytes([b[0], b[1]])));
    (frames, ms)
}

/// Frame index: rotation block `rotation · anim`, then the animation step
/// `(time / ms + phase) mod anim` (no animation when `ms` is 0).
pub(crate) fn frame_index(
    total: usize,
    (anim, ms): (usize, u64),
    rotation: u8,
    time_ms: u64,
    phase: usize,
) -> usize {
    let rotations = (total / anim).max(1);
    let ticks = time_ms.checked_div(ms).unwrap_or(0);
    let anim64 = u64::try_from(anim).expect("fits u64");
    let phase64 = u64::try_from(phase).expect("fits u64");
    let step = usize::try_from((ticks + phase64) % anim64).expect("< anim");
    (usize::from(rotation) % rotations) * anim + step
}

/// Draws `img` with its anchor at screen point `(sx, sy)`, skipping sprites
/// outside the canvas; `pixel` decides per pixel (screen x, y) whether to draw.
pub(crate) fn blit_sprite(
    canvas: &mut Canvas,
    img: &Rgba,
    frame: &Frame,
    (sx, sy): (i32, i32),
    shadow: bool,
    mut pixel: impl FnMut(i32, i32, [u8; 4]) -> Option<[u8; 4]>,
) {
    let (ox, oy) = (
        sx - i32::from(frame.anchor_x),
        sy - i32::from(frame.anchor_y),
    );
    let (fw, fh) = (i32::from(img.width), i32::from(img.height));
    if !canvas.overlaps(ox, oy, fw, fh) {
        return;
    }
    let w = usize::from(img.width);
    for (i, px) in img.pixels.chunks(4).enumerate() {
        if px[3] == 0 {
            continue;
        }
        let (x, y) = (
            ox + i32::try_from(i % w).expect("fits"),
            oy + i32::try_from(i / w).expect("fits"),
        );
        let Some(rgba) = pixel(x, y, [px[0], px[1], px[2], px[3]]) else {
            continue;
        };
        canvas.put(x, y, if shadow { SHADOW } else { rgba });
    }
}

/// A placed object with its footprint, ready to draw.
pub type Prepared = (PlacedObject, (u8, u8));

/// The placed objects of `records` (e.g. `["GELAENDE", "PROD2",
/// "FLUESSE"]`) selected by `which`, duplicates removed, in draw order
/// (back to front by the far corner of the footprint, then `x`). Objects
/// whose entry is missing or has no sprite set are kept, so that drawing
/// counts them.
pub fn prepare_objects(
    island: &Island<'_>,
    anlagen: &AnlagenFile<'_>,
    records: &[&str],
    which: Objects,
) -> Vec<Prepared> {
    let mut objs = BTreeSet::<PlacedObject>::new();
    for r in records {
        objs.extend(island.objects(r).unwrap_or_default());
    }
    let mut list: Vec<Prepared> = objs
        .into_iter()
        .filter(|o| {
            let set = anlagen
                .entry(u32::from(o.anlage))
                .and_then(|e| e.gfx.first());
            let Some(set) = set else {
                // Counted (failed or invisible) once, with the static objects.
                return matches!(which, Objects::All | Objects::NotWater);
            };
            let water = set.prefix.get(2) == Some(&KIND_WATER);
            let below = i64::from(o.height()) < SEA_LEVEL;
            match which {
                Objects::All => true,
                Objects::NotWater => !water,
                Objects::WaterBelowSea => water && below,
                Objects::WaterAboveSea => water && !below,
            }
        })
        .map(|o| {
            let (fw, fh) = anlagen.footprint(u32::from(o.anlage)).unwrap_or((1, 1));
            (o, o.footprint(fw, fh))
        })
        .collect();
    list.sort_by_key(|(o, (w, h))| (o.x + u16::from(*w) + o.y + u16::from(*h), o.x));
    list
}

/// Draws the placed objects of `records` selected by `which` at animation
/// time `time_ms` ([`prepare_objects`], then [`draw_prepared`]).
#[allow(clippy::too_many_arguments)]
pub fn draw_objects(
    canvas: &mut Canvas,
    island: &Island<'_>,
    anlagen: &AnlagenFile<'_>,
    records: &[&str],
    which: Objects,
    group: usize,
    lift: Lift,
    time_ms: u64,
    cache: &SpriteCache,
) -> DrawStats {
    let list = prepare_objects(island, anlagen, records, which);
    draw_prepared(canvas, &list, anlagen, group, lift, time_ms, cache)
}

/// One sprite to draw: its frame (header), the key of its decoded image
/// in a [`SpriteCache`], and the screen point its anchor lands on.
#[derive(Debug, Clone, Copy)]
pub struct SpriteDraw<'f> {
    pub key: SpriteKey,
    pub frame: &'f Frame,
    pub anchor: (i32, i32),
}

impl SpriteDraw<'_> {
    /// Screen rectangle `(x, y, w, h)` the frame covers.
    pub fn rect(&self) -> (i32, i32, i32, i32) {
        (
            self.anchor.0 - i32::from(self.frame.anchor_x),
            self.anchor.1 - i32::from(self.frame.anchor_y),
            i32::from(self.frame.width),
            i32::from(self.frame.height),
        )
    }

    /// Whether this is a shadow (drawn as [`SHADOW`] where covered).
    pub fn is_shadow(&self) -> bool {
        self.key.2
    }
}

/// The sprites of prepared objects at animation time `time_ms`, in draw
/// order (each object's shadow before the object), keeping only those for
/// which `visible(x, y, w, h)` holds. Each object stands at its own height
/// (`PlacedObject::height`, as the original), with its own animation phase
/// (surf: `(3y + 5x) mod 32`).
#[allow(clippy::too_many_arguments)]
pub fn place_prepared<'a>(
    list: &[Prepared],
    anlagen: &'a AnlagenFile<'_>,
    group: usize,
    lift: Lift,
    time_ms: u64,
    visible: impl Fn(i32, i32, i32, i32) -> bool,
    out: &mut Vec<SpriteDraw<'a>>,
) -> DrawStats {
    let mut stats = DrawStats::default();
    let g = u32::try_from(group).expect("group");
    let (tw, th) = TILE[group];
    for &(o, (w, h)) in list {
        let id = u32::from(o.anlage);
        let Some(entry) = anlagen.entry(id) else {
            stats.failed += 1;
            continue;
        };
        let Some(set) = entry.gfx.first() else {
            stats.invisible += 1;
            continue;
        };
        // Footprint centre: tile (x + w/2, y + h/2), computed in doubled units.
        let (cx2, cy2) = (
            2 * i32::from(o.x) + i32::from(w),
            2 * i32::from(o.y) + i32::from(h),
        );
        // The object's own height (doubled units, as the footprint centre).
        let z2 = 2 * i64::from(o.height());
        let raise = lift.scaled(z2, 2, group);
        let anchor = ((cx2 - cy2) * tw / 4, (cx2 + cy2) * th / 4 - raise);
        let Some(frames) = set.frame_sets.iter().find(|fs| fs.group == g) else {
            stats.failed += 1;
            continue;
        };
        let phase = if id == SURF_ANLAGE {
            (3 * usize::from(o.y) + 5 * usize::from(o.x)) % 32
        } else {
            usize::from(o.phase())
        };
        let k = frame_index(
            frames.frames.len(),
            animation(set),
            o.rotation,
            time_ms,
            phase,
        );
        let Some(frame) = frames.frames.get(k) else {
            stats.failed += 1;
            continue;
        };
        stats.drawn += 1;
        // Shadow first: same frame index of the set's SHADOW, own anchor.
        if let Some(sf) = set.shadow.as_ref().and_then(|s| {
            s.frame_sets
                .iter()
                .find(|fs| fs.group == g)
                .and_then(|fs| fs.frames.get(k))
        }) {
            let d = SpriteDraw {
                key: (id, 0, true, g, k),
                frame: sf,
                anchor,
            };
            let (x, y, w, h) = d.rect();
            if visible(x, y, w, h) {
                out.push(d);
            }
        }
        let d = SpriteDraw {
            key: (id, 0, false, g, k),
            frame,
            anchor,
        };
        let (x, y, w, h) = d.rect();
        if visible(x, y, w, h) {
            out.push(d);
        }
    }
    stats
}

/// Draws prepared objects at animation time `time_ms` ([`place_prepared`]).
pub fn draw_prepared(
    canvas: &mut Canvas,
    list: &[Prepared],
    anlagen: &AnlagenFile<'_>,
    group: usize,
    lift: Lift,
    time_ms: u64,
    cache: &SpriteCache,
) -> DrawStats {
    let mut draws = Vec::new();
    let mut stats = place_prepared(
        list,
        anlagen,
        group,
        lift,
        time_ms,
        |x, y, w, h| canvas.overlaps(x, y, w, h),
        &mut draws,
    );
    for d in draws {
        let Some(img) = cache.get(anlagen, d.key, d.frame) else {
            if !d.is_shadow() {
                stats.drawn -= 1;
                stats.failed += 1;
            }
            continue;
        };
        blit_sprite(canvas, &img, d.frame, d.anchor, d.is_shadow(), |_, _, p| {
            Some(p)
        });
    }
    stats
}

/// Average colour of the opaque pixels of the sea sprite's first frame
/// (zoom group 0), used as the colour of open-water ground (texture 0),
/// which the original does not draw as terrain (approximation, OQ-026 b).
pub fn water_colour(anlagen: &AnlagenFile<'_>) -> Option<[u8; 4]> {
    let entry = anlagen.entry(SEA_ANLAGE)?;
    let frame = entry
        .gfx
        .first()?
        .frame_sets
        .iter()
        .find(|fs| fs.group == 0)?
        .frames
        .first()?;
    let img = building::to_rgba(anlagen, entry, 0, frame).ok()?;
    let (mut sum, mut n) = ([0u64; 3], 0u64);
    for px in img.pixels.chunks(4).filter(|p| p[3] != 0) {
        for (s, &c) in sum.iter_mut().zip(px) {
            *s += u64::from(c);
        }
        n += 1;
    }
    let avg = |s: u64| u8::try_from(s / n.max(1)).expect("average of u8");
    (n > 0).then(|| [avg(sum[0]), avg(sum[1]), avg(sum[2]), 255])
}

/// How the sea is drawn.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WaterStyle {
    /// As the original (default).
    #[default]
    Original,
    /// **Not in the original** (opt-in enhancement, docs/render/iso.md
    /// "Enhanced water"): a slow swell on open water and breaking waves that
    /// travel towards the shore along the depth gradient and foam out in
    /// shallow water.
    Enhanced,
}

/// Distance in tiles from each corner of the height grid (`w × h`) to the
/// nearest corner at or above sea level (two-pass chamfer distance,
/// 1 / √2 steps). Used by the enhanced water only.
pub(crate) fn shore_distance(heights: &[u8], w: usize, h: usize) -> Vec<f32> {
    const DIAG: f32 = std::f32::consts::SQRT_2;
    let mut d: Vec<f32> = heights
        .iter()
        .map(|&z| {
            if i64::from(z) >= SEA_LEVEL {
                0.0
            } else {
                f32::INFINITY
            }
        })
        .collect();
    let at = |x: usize, y: usize| y * w + x;
    for y in 0..h {
        for x in 0..w {
            let mut v = d[at(x, y)];
            if x > 0 {
                v = v.min(d[at(x - 1, y)] + 1.0);
            }
            if y > 0 {
                v = v.min(d[at(x, y - 1)] + 1.0);
                if x > 0 {
                    v = v.min(d[at(x - 1, y - 1)] + DIAG);
                }
                if x + 1 < w {
                    v = v.min(d[at(x + 1, y - 1)] + DIAG);
                }
            }
            d[at(x, y)] = v;
        }
    }
    for y in (0..h).rev() {
        for x in (0..w).rev() {
            let mut v = d[at(x, y)];
            if x + 1 < w {
                v = v.min(d[at(x + 1, y)] + 1.0);
            }
            if y + 1 < h {
                v = v.min(d[at(x, y + 1)] + 1.0);
                if x + 1 < w {
                    v = v.min(d[at(x + 1, y + 1)] + DIAG);
                }
                if x > 0 {
                    v = v.min(d[at(x - 1, y + 1)] + DIAG);
                }
            }
            d[at(x, y)] = v;
        }
    }
    d
}

/// Tuning of the enhanced water. Not derived from the original; tuned by
/// eye against the colleague's recordings (2026-10-17). Kept together so it
/// can become configuration (PLAN §0.1, §0.2). The WGSL port in `gpu.wgsl`
/// mirrors these values.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Waves {
    /// Tiles from the shore where coastal waves start.
    pub surf_zone: f32,
    /// Tiles from the shore where crests break into foam.
    pub break_zone: f32,
    /// Distance between incoming crests (tiles).
    pub wavelength: f32,
    /// Time for one crest to follow the next (ms).
    pub period_ms: f32,
    /// Brightness amplitude of the open-water swell.
    pub swell: f32,
    /// Strength of the turquoise tint of shallow water at the shoreline.
    pub shallow_tint: f32,
    /// Colour of shallow water (0–1 RGB).
    pub shallow_colour: [f32; 3],
    /// Depth (`HEIGHT` units below sea level) where the sea floor starts to
    /// fade out, and where it is hidden completely.
    pub deep_from: f32,
    pub deep_to: f32,
}

impl Waves {
    pub const DEFAULT: Waves = Waves {
        surf_zone: 3.0,
        break_zone: 1.6,
        wavelength: 1.4,
        period_ms: 3000.0,
        swell: 0.07,
        shallow_tint: 0.3,
        shallow_colour: [0.36, 0.74, 0.76],
        deep_from: 10.0,
        deep_to: 28.0,
    };
}

/// How much of the sea floor deep water hides (0–1) at `depth` `HEIGHT`
/// units below sea level ([`Waves::deep_from`] → [`Waves::deep_to`]). The
/// terrain height is smooth across tiles, so this removes the tile-shaped
/// edges between the original's floor textures in deep water.
pub(crate) fn deep_water(depth: f32) -> f32 {
    let w = Waves::DEFAULT;
    smoothstep(w.deep_from, w.deep_to, depth)
}

/// Detail level of the enhanced water for tile width `tw` (pixels): 1 at
/// zoom group 0, fading to 0 at group 2, where fine foam texture would be
/// smaller than a few pixels and alias into patterns.
pub(crate) fn wave_detail(tw: f32) -> f32 {
    smoothstep(24.0, 64.0, tw)
}

/// Hash of an integer lattice point to `[0, 1)`.
fn hash3(x: i32, y: i32, z: i32) -> f32 {
    #[allow(clippy::cast_sign_loss)]
    let mut h = (x as u32).wrapping_mul(0x8DA6_B343)
        ^ (y as u32).wrapping_mul(0xD816_3841)
        ^ (z as u32).wrapping_mul(0xCB1A_B31F);
    h ^= h >> 13;
    h = h.wrapping_mul(0x5BD1_E995);
    h ^= h >> 15;
    #[allow(clippy::cast_precision_loss)]
    let v = (h & 0x00FF_FFFF) as f32 / 16_777_216.0;
    v
}

/// Smooth value noise in `[0, 1)` (trilinear, smoothstep fade).
fn noise3(x: f32, y: f32, z: f32) -> f32 {
    let (fx, fy, fz) = (x.floor(), y.floor(), z.floor());
    #[allow(clippy::cast_possible_truncation)]
    let (ix, iy, iz) = (fx as i32, fy as i32, fz as i32);
    let fade = |t: f32| t * t * (3.0 - 2.0 * t);
    let (u, v, w) = (fade(x - fx), fade(y - fy), fade(z - fz));
    let lerp = |a: f32, b: f32, t: f32| a + (b - a) * t;
    let corner = |dx: i32, dy: i32, dz: i32| hash3(ix + dx, iy + dy, iz + dz);
    let plane = |dz: i32| {
        lerp(
            lerp(corner(0, 0, dz), corner(1, 0, dz), u),
            lerp(corner(0, 1, dz), corner(1, 1, dz), u),
            v,
        )
    };
    lerp(plane(0), plane(1), w)
}

/// `0` below `a`, `1` above `b`, smooth in between (also for `a > b`).
fn smoothstep(a: f32, b: f32, x: f32) -> f32 {
    let t = ((x - a) / (b - a)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

/// Enhanced-water shading of one sea pixel (not original, opt-in): returns
/// `(light, foam, tint)` — relative brightness change, foam amount (0–1)
/// and shallow-water tint (0–1). `shore` = distance to the shore in tiles,
/// `pos` = position on the water plane in tiles, `detail` = [`wave_detail`].
///
/// Model:
/// - Open water: a slow swell from two drifting layers of smooth value
///   noise (no regular stripes, no whitecaps).
/// - Coast (inside `surf_zone`): crests of equal phase `shore / λ + t / T`
///   travel towards the beach; slow noise bends them so they do not trace
///   the shore exactly, and breaks them into segments. Crests brighten the
///   water; inside `break_zone` they turn into mottled foam; at the beach a
///   thin swash sheet runs up after each crest and fades.
/// - Shallow water near the shore is tinted turquoise.
pub(crate) fn wave_shade(
    shore: f32,
    (wx, wy): (f32, f32),
    time_ms: u64,
    detail: f32,
) -> (f32, f32, f32) {
    let w = Waves::DEFAULT;
    #[allow(clippy::cast_precision_loss)]
    let t = (time_ms % 3_600_000) as f32;
    let s1 = noise3(
        wx * 0.22 + t * 0.000_11,
        wy * 0.22 - t * 0.000_04,
        t * 0.000_06,
    );
    let s2 = noise3(
        wx * 0.55 - t * 0.000_19,
        wy * 0.55 + t * 0.000_13,
        t * 0.000_11,
    );
    let mut light = w.swell * 2.0 * (0.65 * s1 + 0.35 * s2 - 0.5);
    let near = smoothstep(w.surf_zone, w.break_zone * 0.5, shore);
    let mut foam: f32 = 0.0;
    if near > 0.0 {
        let bend = noise3(wx * 0.18, wy * 0.18, t * 0.000_05);
        // A shorter-scale offset staggers neighbouring crest segments, so
        // they reach the beach at different times.
        let stagger = noise3(wx * 0.6 + 7.3, wy * 0.6, t * 0.000_04);
        let phase = shore / w.wavelength + t / w.period_ms + 0.8 * bend + 0.5 * stagger;
        let u = phase - phase.floor();
        // Gentle back, steep front just before the next phase.
        let crest = smoothstep(0.55, 0.95, u) * (1.0 - smoothstep(0.95, 1.0, u));
        // Crests come in separate pieces with calm water between them.
        let segment = smoothstep(0.4, 0.62, noise3(wx * 0.32 + 3.1, wy * 0.32, t * 0.000_06));
        let amp = near * segment;
        light += 0.12 * amp * crest;
        // Curl: a white line on the steep front just before it breaks.
        let curl = smoothstep(0.88, 0.95, u)
            * (1.0 - smoothstep(0.95, 0.985, u))
            * smoothstep(w.break_zone * 1.6, w.break_zone, shore);
        let fine = noise3(wx * 2.2, wy * 2.2, t * 0.0011);
        let mottle = 0.55 + detail * (fine - 0.55);
        let breaking = smoothstep(w.break_zone, w.break_zone * 0.3, shore);
        foam = (breaking * amp * crest.powf(0.5) * smoothstep(0.15, 0.6, mottle) * 1.3)
            .max(0.75 * amp * curl);
        if shore < 0.7 {
            let at_shore = phase - shore / w.wavelength;
            let since = at_shore - at_shore.floor();
            let burst = smoothstep(0.0, 0.05, since) * (-since * 3.0).exp();
            let edge = (1.0 - shore / 0.7).powf(0.8);
            foam = foam.max(edge * burst * (0.5 + 0.5 * mottle));
        }
    }
    let tint = w.shallow_tint * smoothstep(2.2, 0.0, shore);
    (light, foam.clamp(0.0, 1.0), tint)
}

/// `rgba` tinted by `tint`, lit by `light` and whitened by `foam`, as
/// computed by [`wave_shade`].
pub(crate) fn shade_apply(rgba: [u8; 4], (light, foam, tint): (f32, f32, f32)) -> [u8; 4] {
    let w = Waves::DEFAULT;
    let brightness = 1.0 + light;
    let channel = |c: u8, shallow: f32, white: f32| -> u8 {
        let v = f32::from(c);
        let v = (v + (shallow * 255.0 - v) * tint) * brightness;
        let v = v + (white - v) * foam;
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let out = v.clamp(0.0, 255.0) as u8;
        out
    };
    [
        channel(rgba[0], w.shallow_colour[0], 236.0),
        channel(rgba[1], w.shallow_colour[1], 244.0),
        channel(rgba[2], w.shallow_colour[2], 250.0),
        rgba[3],
    ]
}

/// [`wave_shade`] applied to one pixel at full detail; returns the colour
/// and the foam.
#[cfg(test)]
fn enhance(rgba: [u8; 4], shore: f32, pos: (f32, f32), time_ms: u64) -> ([u8; 4], f32) {
    let s = wave_shade(shore, pos, time_ms, 1.0);
    (shade_apply(rgba, s), s.1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn corner_heights_and_interpolation() {
        // 2 × 2 heights: corners of tile (0, 0) are the four values; tile (1, 1)
        // clamps to the island edge.
        let h = [0u8, 10, 20, 30];
        assert_eq!(corners(&h, 2, 2, 0, 0), [0, 10, 20, 30]);
        assert_eq!(corners(&h, 2, 2, 1, 1), [30, 30, 30, 30]);
        let c = [0, 10, 20, 30];
        // c = [A, C, B, D] = [0, 10, 20, 30]; results scaled by n = 2.
        assert_eq!(interpolate(c, 0, 0, 2), 0); // A
        assert_eq!(interpolate(c, 2, 0, 2), 20); // C = 10
        assert_eq!(interpolate(c, 0, 2, 2), 40); // B = 20
        assert_eq!(interpolate(c, 2, 2, 2), 60); // D = 30
        // The centre lies on the B–C diagonal: (B + C) / 2 = 15.
        assert_eq!(interpolate(c, 1, 1, 2), 30);
        // Off the diagonal the two triangles differ from bilinear blending.
        assert_eq!(interpolate([0, 0, 0, 40], 3, 3, 4), 4 * 20); // C-B-D triangle
        assert_eq!(interpolate([40, 0, 0, 0], 1, 1, 4), 4 * 20); // A-B-C triangle
        // Lift: 1 px per unit at group 0, half at group 1 (tile width 42).
        let lift = Lift { num: 1, den: 1 };
        assert_eq!(lift.scaled(30, 2, 0), 15);
        assert_eq!(lift.scaled(30, 2, 1), 7);
        assert_eq!(Lift::NONE.pixels(200, 0), 0);
    }

    #[test]
    fn wave_noise_is_deterministic_and_bounded() {
        for i in 0..200 {
            let f = |v: i32| v as f32 * 0.37;
            let n = noise3(f(i), f(i * 3), f(i * 7));
            assert!((0.0..1.0).contains(&n));
            assert_eq!(n, noise3(f(i), f(i * 3), f(i * 7)));
        }
        let water = [30, 90, 140, 255];
        let (a, fa) = enhance(water, 0.3, (10.0, 4.0), 1234);
        assert_eq!((a, fa), enhance(water, 0.3, (10.0, 4.0), 1234));
        assert!((0.0..=1.0).contains(&fa));
        // Waves move: the same spot differs over time.
        let frames: Vec<_> = (0..20)
            .map(|k| enhance(water, 1.0, (10.0, 4.0), k * 150).0)
            .collect();
        assert!(frames.windows(2).any(|w| w[0] != w[1]));
    }

    #[test]
    fn projection() {
        assert_eq!(tile_top(0, 0, 0), (0, 0));
        assert_eq!(tile_top(1, 0, 0), (42, 21));
        assert_eq!(tile_top(0, 1, 0), (-42, 21));
        assert_eq!(tile_top(2, 2, 1), (0, 42));
    }
}
