//! CPU isometric renderer for one island: ground and placed objects into an
//! RGBA canvas. A reference implementation for the projection rules (and for
//! tests); the interactive renderer will use the same rules.
//!
//! Projection rules (spec: `docs/render/iso.md`; all derived from sprite
//! measurements and visual checks, OQ-026):
//! - Tile `(x, y)` is a diamond `TILE[group]` wide/high whose top vertex is
//!   at screen `((x − y) · w/2, (x + y) · h/2)`.
//! - Ground: the diamond shows its `GroundCell`; diamond point with tile
//!   coordinates `(s, t)` ∈ [0, 1)² shows cell pixel `(32 s, 32 t)`.
//! - Objects: the sprite's anchor is put on the centre of the footprint
//!   diamond. Frame = `rotation · animation frames` (first animation frame);
//!   sets with a single rotation use frame 0.
//! - Draw order: ground, then objects by the depth of their footprint's far
//!   corner (`x + w + y + h`), then by `x`.
//! - Elevation: `HEIGHT` values are heights at tile corners (D: `AnnoWelt.dll`
//!   per-corner grid; that `HEIGHT[x, y]` is corner `(x, y)` is likely) —
//!   tile `(x, y)` spans the heights of `(x, y)`, `(x+1, y)`, `(x, y+1)`,
//!   `(x+1, y+1)`, interpolated bilinearly (the original splits tiles into
//!   two triangles, not traced) — and raise the ground by `height · lift`
//!   pixels at zoom group 0; [`HEIGHT_LIFT`] is the original's scale.
//! - Alpha levels blend (`anno_assets::figure::LEVEL_ALPHA`); shadow frames
//!   darken to 5/8 before their sprite is drawn.

use std::collections::BTreeSet;

use anno_assets::building;
use anno_assets::island::GroundTextures;
use anno_assets::texture::Rgba;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel::{CELL_PX, GroundCell, Island, PlacedObject};

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

    /// Draws `rgba`, blending by its alpha (`src · a + dst · (255 − a)`).
    fn put(&mut self, sx: i32, sy: i32, rgba: [u8; 4]) {
        let Some(o) = self.index(sx, sy) else { return };
        let a = u32::from(rgba[3]);
        if a == 255 {
            self.pixels[o..o + 4].copy_from_slice(&rgba);
            return;
        }
        for (dst, &src) in self.pixels[o..o + 3].iter_mut().zip(&rgba[..3]) {
            let v = (u32::from(src) * a + u32::from(*dst) * (255 - a)) / 255;
            *dst = u8::try_from(v).expect("blend fits u8");
        }
        self.pixels[o + 3] = self.pixels[o + 3].max(rgba[3]);
    }

    /// Shadow: `dst / 2 + dst / 8` per colour channel (D: `AnnoDisplay.dll`
    /// `0x10019f80`); overlapping shadows compound.
    fn shade(&mut self, sx: i32, sy: i32) {
        let Some(o) = self.index(sx, sy) else { return };
        for d in &mut self.pixels[o..o + 3] {
            *d = *d / 2 + *d / 8;
        }
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
    fn scaled(self, h_num: i64, h_den: i64, group: usize) -> i32 {
        let num = h_num * i64::from(self.num) * i64::from(TILE[group].0);
        let den = h_den * i64::from(self.den) * i64::from(TILE[0].0);
        i32::try_from(num / den).expect("offset fits i32")
    }
}

/// Corner heights of tile `(x, y)`: `[(x, y), (x+1, y), (x, y+1), (x+1, y+1)]`,
/// clamped at the island edge.
fn corners(heights: &[u8], w: usize, h: usize, x: usize, y: usize) -> [i64; 4] {
    let at = |cx: usize, cy: usize| i64::from(heights[cy.min(h - 1) * w + cx.min(w - 1)]);
    [at(x, y), at(x + 1, y), at(x, y + 1), at(x + 1, y + 1)]
}

/// Bilinear height at `(i/n, j/n)` inside a tile, times `n²`.
fn interpolate(c: [i64; 4], i: i64, j: i64, n: i64) -> i64 {
    c[0] * (n - i) * (n - j) + c[1] * i * (n - j) + c[2] * (n - i) * j + c[3] * i * j
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
    let (tw, th) = TILE[group];
    let w = usize::from(island.width);
    let hgt = usize::from(island.height);
    let mut order: Vec<usize> = (0..gfx.len()).collect();
    order.sort_by_key(|&t| (t % w + t / w, t % w));
    if lift != Lift::NONE {
        // Forward mapping: walk the cell in tile space, oversampled so that
        // steep slopes leave no holes.
        let n = i64::from(2 * (tw + th));
        for t in order {
            let cell = GroundCell::from_gfx_id(gfx[t]);
            let c = corners(&heights, w, hgt, t % w, t / w);
            let (x, y) = (
                i32::try_from(t % w).expect("fits"),
                i32::try_from(t / w).expect("fits"),
            );
            let (sx, sy) = tile_top(x, y, group);
            for j in 0..n {
                for i in 0..n {
                    let px = i32::try_from((i - j) * i64::from(tw) / (2 * n)).expect("fits");
                    let py = i32::try_from((i + j) * i64::from(th) / (2 * n)).expect("fits");
                    let up = lift.scaled(interpolate(c, i, j, n), n * n, group);
                    let (cx, cy) = (
                        usize::try_from(i * 32 / n).expect("fits"),
                        usize::try_from(j * 32 / n).expect("fits"),
                    );
                    // Cover the pixel below too: sample rows can skip a screen row on
                    // slopes; back-to-front order lets nearer tiles overdraw it.
                    let rgba = textures.cell_pixel(cell, cx, cy);
                    canvas.put(sx + px, sy + py - up, rgba);
                    canvas.put(sx + px, sy + py - up + 1, rgba);
                }
            }
        }
        return;
    }
    for t in order {
        let cell = GroundCell::from_gfx_id(gfx[t]);
        let (x, y) = (
            i32::try_from(t % w).expect("fits"),
            i32::try_from(t / w).expect("fits"),
        );
        let (sx, top) = tile_top(x, y, group);
        let sy = top - lift.pixels(heights[t], group);
        for py in 0..th {
            for px in -tw / 2..tw / 2 {
                // Tile coordinates of the pixel centre, scaled by 2·tw·th.
                let (u, v) = (2 * px + 1, 2 * py + 1);
                let s2 = u * th + v * tw; // = s · tw · th · 2
                let t2 = v * tw - u * th;
                let full = tw * th * 2;
                if s2 < 0 || t2 < 0 || s2 >= full || t2 >= full {
                    continue;
                }
                let cx = usize::try_from(s2 * 32 / full).expect("in range");
                let cy = usize::try_from(t2 * 32 / full).expect("in range");
                let rgba = textures.cell_pixel(cell, cx.min(CELL_PX - 1), cy.min(CELL_PX - 1));
                canvas.put(sx + px, sy + py, rgba);
            }
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

/// Draws the placed objects of `records` (e.g. `["GELAENDE", "PROD2"]`),
/// duplicates removed.
pub fn draw_objects(
    canvas: &mut Canvas,
    island: &Island<'_>,
    anlagen: &AnlagenFile<'_>,
    records: &[&str],
    group: usize,
    lift: Lift,
) -> DrawStats {
    let heights = island.heights().unwrap_or_default();
    let iw = usize::from(island.width);
    let ih = usize::from(island.height);
    let mut objs = BTreeSet::<PlacedObject>::new();
    for r in records {
        objs.extend(island.objects(r).unwrap_or_default());
    }
    let mut list: Vec<(PlacedObject, (u8, u8))> = objs
        .into_iter()
        .map(|o| {
            let (fw, fh) = anlagen.footprint(u32::from(o.anlage)).unwrap_or((1, 1));
            (o, o.footprint(fw, fh))
        })
        .collect();
    list.sort_by_key(|(o, (w, h))| (o.x + u16::from(*w) + o.y + u16::from(*h), o.x));
    let mut stats = DrawStats::default();
    let g = u32::try_from(group).expect("group");
    for (o, (w, h)) in list {
        let Some(entry) = anlagen.entry(u32::from(o.anlage)) else {
            stats.failed += 1;
            continue;
        };
        let Some(set) = entry.gfx.first() else {
            stats.invisible += 1;
            continue;
        };
        let Some(frames) = set.frame_sets.iter().find(|fs| fs.group == g) else {
            stats.failed += 1;
            continue;
        };
        let anim = set
            .records
            .iter()
            .find(|r| r.name() == "ANIM")
            .and_then(|r| r.payload.get(2..4))
            .map_or(1, |b| usize::from(u16::from_le_bytes([b[0], b[1]])).max(1));
        let rotations = (frames.frames.len() / anim).max(1);
        let k = (usize::from(o.rotation) % rotations) * anim;
        let Some(frame) = frames.frames.get(k) else {
            stats.failed += 1;
            continue;
        };
        let Ok(img) = building::to_rgba(anlagen, entry, g, frame) else {
            stats.failed += 1;
            continue;
        };
        // Footprint centre: tile (x + w/2, y + h/2), computed in doubled units.
        let (tw, th) = TILE[group];
        let (cx2, cy2) = (
            2 * i32::from(o.x) + i32::from(w),
            2 * i32::from(o.y) + i32::from(h),
        );
        // Height at the footprint centre, interpolated like the ground.
        let raise = if heights.is_empty() {
            0
        } else {
            let (cx, cy) = (
                usize::try_from(cx2 / 2).expect("fits"),
                usize::try_from(cy2 / 2).expect("fits"),
            );
            let c = corners(&heights, iw, ih, cx, cy);
            lift.scaled(
                interpolate(c, i64::from(cx2 % 2), i64::from(cy2 % 2), 2),
                4,
                group,
            )
        };
        let (sx, sy) = ((cx2 - cy2) * tw / 4, (cx2 + cy2) * th / 4 - raise);
        let (ox, oy) = (
            sx - i32::from(frame.anchor_x),
            sy - i32::from(frame.anchor_y),
        );
        // Shadow first: same frame index of the set's SHADOW, own anchor.
        let shadow = set.shadow.as_ref().and_then(|s| {
            let sf = s
                .frame_sets
                .iter()
                .find(|fs| fs.group == g)?
                .frames
                .get(k)?;
            Some((sf, building::to_rgba(anlagen, entry, g, sf).ok()?))
        });
        if let Some((sf, simg)) = shadow {
            let (sox, soy) = (sx - i32::from(sf.anchor_x), sy - i32::from(sf.anchor_y));
            let sw = usize::from(simg.width);
            for (i, px) in simg.pixels.chunks(4).enumerate() {
                if px[3] != 0 {
                    let (x, y) = (
                        i32::try_from(i % sw).expect("fits"),
                        i32::try_from(i / sw).expect("fits"),
                    );
                    canvas.shade(sox + x, soy + y);
                }
            }
        }
        let fw = usize::from(img.width);
        for (i, px) in img.pixels.chunks(4).enumerate() {
            if px[3] == 0 {
                continue;
            }
            let (x, y) = (
                i32::try_from(i % fw).expect("fits"),
                i32::try_from(i / fw).expect("fits"),
            );
            canvas.put(ox + x, oy + y, [px[0], px[1], px[2], px[3]]);
        }
        stats.drawn += 1;
    }
    stats
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
        // Corners and centre, scaled by n² = 4.
        assert_eq!(interpolate(c, 0, 0, 2), 0);
        assert_eq!(interpolate(c, 2, 0, 2), 40);
        assert_eq!(interpolate(c, 0, 2, 2), 80);
        assert_eq!(interpolate(c, 1, 1, 2), 60); // (0 + 10 + 20 + 30) / 4 · 4
        // Lift: 1 px per unit at group 0, half at group 1 (tile width 42).
        let lift = Lift { num: 1, den: 1 };
        assert_eq!(lift.scaled(60, 4, 0), 15);
        assert_eq!(lift.scaled(60, 4, 1), 7);
        assert_eq!(Lift::NONE.pixels(200, 0), 0);
    }

    #[test]
    fn projection() {
        assert_eq!(tile_top(0, 0, 0), (0, 0));
        assert_eq!(tile_top(1, 0, 0), (42, 21));
        assert_eq!(tile_top(0, 1, 0), (-42, 21));
        assert_eq!(tile_top(2, 2, 1), (0, 42));
    }
}
