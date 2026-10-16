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
//! - Elevation: a tile is raised by `HEIGHT · lift` pixels at zoom group 0
//!   (scaled with the tile width for other groups); the value of `lift` is
//!   being determined (OQ-026). Tiles are flat diamonds, no slopes yet.

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

    fn put(&mut self, sx: i32, sy: i32, rgba: [u8; 4]) {
        let (x, y) = (sx - self.origin.0, sy - self.origin.1);
        if let (Ok(x), Ok(y)) = (usize::try_from(x), usize::try_from(y))
            && x < self.width
            && y < self.height
        {
            let o = (y * self.width + x) * 4;
            self.pixels[o..o + 4].copy_from_slice(&rgba);
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
        i32::from(h) * self.num * TILE[group].0 / (self.den * TILE[0].0)
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
    let (tw, th) = TILE[group];
    let w = usize::from(island.width);
    let mut order: Vec<usize> = (0..gfx.len()).collect();
    order.sort_by_key(|&t| (t % w + t / w, t % w));
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
        let under = usize::from(o.y + u16::from(h) / 2) * iw + usize::from(o.x + u16::from(w) / 2);
        let raise = heights.get(under).map_or(0, |&z| lift.pixels(z, group));
        let (sx, sy) = ((cx2 - cy2) * tw / 4, (cx2 + cy2) * th / 4 - raise);
        let (ox, oy) = (
            sx - i32::from(frame.anchor_x),
            sy - i32::from(frame.anchor_y),
        );
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
    fn projection() {
        assert_eq!(tile_top(0, 0, 0), (0, 0));
        assert_eq!(tile_top(1, 0, 0), (42, 21));
        assert_eq!(tile_top(0, 1, 0), (-42, 21));
        assert_eq!(tile_top(2, 2, 1), (0, 42));
    }
}
