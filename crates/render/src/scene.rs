//! Scenario composition: every island of a scenario at its world position,
//! each rendered with [`crate::iso`] and composited into one screen region.
//!
//! Rules (spec: `docs/render/iso.md`, "Scene composition"):
//! - Island tile `(0, 0)` is world tile `(Island::x, Island::y)` (`INSEL5`
//!   head, `docs/formats/insel.md`). World tiles use the projection of
//!   [`iso::tile_top`], so an island's local screen coordinates (those of
//!   [`iso::Canvas`]) are shifted by `tile_top(island.x, island.y)`.
//! - An island covers the rectangle of its tile diamonds plus [`MARGIN`]
//!   above the top vertex for sprites that rise above the top row.
//! - Islands are drawn in the order of their far corner `x + w + y + h`,
//!   then `x` (the simplification used for objects; OQ-026).
//! - An island is rendered into a temporary canvas that covers only its
//!   overlap with the target region, then composited source-over.

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel::Island;

use crate::iso::{self, Canvas, DrawStats, HEIGHT_LIFT, Objects, SpriteCache, TILE, over};

/// Room above an island's top vertex per zoom group: the largest sprite
/// anchor height in `Anlagen.dat` (P: entry 1330, `anchor_y` 504 / 252 / 126
/// in groups 0 / 1 / 2). An anchor lies on a footprint centre, at least half
/// a tile below the top vertex, so no flat-drawn sprite reaches higher.
pub const MARGIN: [i32; 3] = [504, 252, 126];

/// Space above an island: [`MARGIN`] plus the largest elevation (`u8::MAX`
/// height at [`HEIGHT_LIFT`]).
pub fn margin(group: usize) -> i32 {
    MARGIN[group] + HEIGHT_LIFT.pixels(u8::MAX, group)
}

/// A screen rectangle in pixels; `x`, `y` = top-left corner.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Rect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl Rect {
    /// Overlap of two rectangles, `None` if they do not overlap.
    pub fn intersect(self, o: Rect) -> Option<Rect> {
        let (x0, y0) = (self.x.max(o.x), self.y.max(o.y));
        let x1 = (self.x + self.w).min(o.x + o.w);
        let y1 = (self.y + self.h).min(o.y + o.h);
        (x1 > x0 && y1 > y0).then_some(Rect {
            x: x0,
            y: y0,
            w: x1 - x0,
            h: y1 - y0,
        })
    }

    /// Smallest rectangle containing both.
    pub fn union(self, o: Rect) -> Rect {
        let (x0, y0) = (self.x.min(o.x), self.y.min(o.y));
        let x1 = (self.x + self.w).max(o.x + o.w);
        let y1 = (self.y + self.h).max(o.y + o.h);
        Rect {
            x: x0,
            y: y0,
            w: x1 - x0,
            h: y1 - y0,
        }
    }
}

/// World position and size of an island in tiles (from its `INSEL5` head).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct IslandBox {
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
}

impl IslandBox {
    pub fn of(island: &Island<'_>) -> Self {
        Self {
            x: island.x,
            y: island.y,
            width: island.width,
            height: island.height,
        }
    }
}

/// Where an island lands on the scene screen at one zoom group.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Placement {
    /// Index into the island list given to [`layout`].
    pub index: usize,
    /// Scene screen position of the island's local screen point `(0, 0)`
    /// (the top vertex of its tile `(0, 0)`).
    pub offset: (i32, i32),
    /// Scene screen rectangle the island can draw into.
    pub bounds: Rect,
}

/// Placements of all islands at `group`, in draw order (back to front).
pub fn layout(boxes: &[IslandBox], group: usize) -> Vec<Placement> {
    let (tw, th) = TILE[group];
    let mut order: Vec<usize> = (0..boxes.len()).collect();
    order.sort_by_key(|&i| {
        let b = boxes[i];
        (
            u32::from(b.x) + u32::from(b.width) + u32::from(b.y) + u32::from(b.height),
            b.x,
            i,
        )
    });
    order
        .into_iter()
        .map(|index| {
            let b = boxes[index];
            let offset = iso::tile_top(i32::from(b.x), i32::from(b.y), group);
            let (w, h) = (i32::from(b.width), i32::from(b.height));
            // Same rectangle as `iso::Canvas::for_island` with `MARGIN`.
            let left = -h * tw / 2;
            let bottom = (w + h) * th / 2;
            let bounds = Rect {
                x: offset.0 + left,
                y: offset.1 - margin(group),
                w: w * tw / 2 - left,
                h: bottom + margin(group),
            };
            Placement {
                index,
                offset,
                bounds,
            }
        })
        .collect()
}

/// Union of all placement bounds; `None` without islands.
pub fn scene_bounds(placements: &[Placement]) -> Option<Rect> {
    placements.iter().map(|p| p.bounds).reduce(Rect::union)
}

/// A transparent canvas covering `rect` (`origin` = its top-left corner).
pub fn blank(rect: Rect) -> Canvas {
    let width = usize::try_from(rect.w.max(0)).expect("non-negative");
    let height = usize::try_from(rect.h.max(0)).expect("non-negative");
    Canvas {
        width,
        height,
        origin: (rect.x, rect.y),
        pixels: vec![0; width * height * 4],
    }
}

/// Composites `src` source-over onto `dst`; `src`'s screen point `(sx, sy)`
/// lands on `dst`'s screen point `(sx + offset.0, sy + offset.1)`. Pixels
/// outside `dst` are dropped.
pub fn composite(dst: &mut Canvas, src: &Canvas, offset: (i32, i32)) {
    let to_i32 = |n: usize| i32::try_from(n).expect("canvas fits i32");
    let src_rect = Rect {
        x: src.origin.0 + offset.0,
        y: src.origin.1 + offset.1,
        w: to_i32(src.width),
        h: to_i32(src.height),
    };
    let dst_rect = Rect {
        x: dst.origin.0,
        y: dst.origin.1,
        w: to_i32(dst.width),
        h: to_i32(dst.height),
    };
    let Some(r) = src_rect.intersect(dst_rect) else {
        return;
    };
    let idx = |v: i32| usize::try_from(v).expect("inside");
    let w = idx(r.w);
    for row in 0..r.h {
        let so = (idx(r.y + row - src_rect.y) * src.width + idx(r.x - src_rect.x)) * 4;
        let d_o = (idx(r.y + row - dst_rect.y) * dst.width + idx(r.x - dst_rect.x)) * 4;
        let s = &src.pixels[so..so + w * 4];
        let d = &mut dst.pixels[d_o..d_o + w * 4];
        for (sp, dp) in s.as_chunks::<4>().0.iter().zip(d.as_chunks_mut::<4>().0) {
            over(dp, sp);
        }
    }
}

/// Outcome of [`render_region`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SceneStats {
    /// Islands that overlap the region and were drawn.
    pub islands: usize,
    /// Of those, islands whose `HEIGHT`/`GFXID` grids could not be read (no
    /// ground drawn).
    pub no_ground: usize,
    /// Summed over the drawn islands. An object that overlaps several
    /// regions is counted in each.
    pub objects: DrawStats,
}

/// Layers [`render_region`] draws, in this order per island: ground; the
/// sea surface and water objects (rivers, surf) at time `water` (ms); the
/// other placed objects. A viewer caches the static layers and redraws
/// only the water layer per frame.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Layers {
    pub ground: bool,
    pub water: Option<u64>,
    pub objects: bool,
}

impl Layers {
    /// Everything, water at time 0.
    pub const ALL: Layers = Layers {
        ground: true,
        water: Some(0),
        objects: true,
    };
}

/// Object records drawn: scenery, built objects, rivers and surf.
pub const OBJECT_RECORDS: [&str; 3] = ["GELAENDE", "PROD2", "FLUESSE"];

/// Draws every island of `placements` that overlaps `dst` (whose `origin`
/// is a scene screen position): the selected `layers`, raised by
/// `iso::HEIGHT_LIFT`.
#[allow(clippy::too_many_arguments)]
pub fn render_region(
    dst: &mut Canvas,
    islands: &[Island<'_>],
    placements: &[Placement],
    textures: &GroundTextures,
    anlagen: &AnlagenFile<'_>,
    group: usize,
    layers: Layers,
    cache: &SpriteCache,
) -> SceneStats {
    let to_i32 = |n: usize| i32::try_from(n).expect("canvas fits i32");
    let region = Rect {
        x: dst.origin.0,
        y: dst.origin.1,
        w: to_i32(dst.width),
        h: to_i32(dst.height),
    };
    let mut stats = SceneStats::default();
    for p in placements {
        let Some(r) = p.bounds.intersect(region) else {
            continue;
        };
        let island = &islands[p.index];
        // Temporary canvas in the island's local screen coordinates.
        let mut local = blank(Rect {
            x: r.x - p.offset.0,
            y: r.y - p.offset.1,
            ..r
        });
        if island.gfx_ids().is_err() || island.heights().is_err() {
            stats.no_ground += 1;
        }
        if layers.ground {
            iso::draw_ground(&mut local, island, textures, group, HEIGHT_LIFT);
        }
        if let Some(t) = layers.water {
            iso::draw_water(&mut local, island, anlagen, group, HEIGHT_LIFT, t, cache);
            iso::draw_objects(
                &mut local,
                island,
                anlagen,
                &OBJECT_RECORDS,
                Objects::Water,
                group,
                HEIGHT_LIFT,
                t,
                cache,
            );
        }
        let s = if layers.objects {
            iso::draw_objects(
                &mut local,
                island,
                anlagen,
                &OBJECT_RECORDS,
                Objects::NotWater,
                group,
                HEIGHT_LIFT,
                0,
                cache,
            )
        } else {
            DrawStats::default()
        };
        composite(dst, &local, p.offset);
        stats.islands += 1;
        stats.objects.drawn += s.drawn;
        stats.objects.invisible += s.invisible;
        stats.objects.failed += s.failed;
    }
    stats
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bx(x: u16, y: u16, width: u16, height: u16) -> IslandBox {
        IslandBox {
            x,
            y,
            width,
            height,
        }
    }

    #[test]
    fn layout_offsets_bounds_and_order() {
        // Island 0 lies further front (larger x + w + y + h) than island 1.
        let p = layout(&[bx(10, 4, 8, 6), bx(0, 0, 4, 4)], 0);
        assert_eq!(p.iter().map(|p| p.index).collect::<Vec<_>>(), [1, 0]);
        // Top vertex of world tile (10, 4) at group 0 (84 × 42).
        assert_eq!(p[1].offset, ((10 - 4) * 42, (10 + 4) * 21));
        assert_eq!(
            p[1].bounds,
            Rect {
                x: 6 * 42 - 6 * 42,
                y: 14 * 21 - margin(0),
                w: (8 + 6) * 42,
                h: (8 + 6) * 21 + margin(0),
            }
        );
        assert_eq!(p[0].offset, (0, 0));
        assert_eq!(
            scene_bounds(&p),
            Some(Rect {
                x: -4 * 42,
                y: -margin(0),
                w: 4 * 42 + 6 * 42 + 8 * 42,
                h: 14 * 21 + 14 * 21 + margin(0),
            })
        );
        assert_eq!(scene_bounds(&[]), None);
    }

    #[test]
    fn layout_matches_iso_canvas() {
        // The bounds rectangle equals `Canvas::for_island` shifted by offset.
        let p = layout(&[bx(3, 5, 7, 2)], 1)[0];
        let (tw, th) = TILE[1];
        assert_eq!(p.offset, iso::tile_top(3, 5, 1));
        assert_eq!(
            (p.bounds.x - p.offset.0, p.bounds.w),
            (-2 * tw / 2, 9 * tw / 2)
        );
        assert_eq!(p.bounds.h, 9 * th / 2 + margin(1));
    }

    #[test]
    fn rect_ops() {
        let a = Rect {
            x: 0,
            y: 0,
            w: 10,
            h: 10,
        };
        let b = Rect {
            x: 5,
            y: -5,
            w: 10,
            h: 10,
        };
        assert_eq!(
            a.intersect(b),
            Some(Rect {
                x: 5,
                y: 0,
                w: 5,
                h: 5
            })
        );
        assert_eq!(
            a.union(b),
            Rect {
                x: 0,
                y: -5,
                w: 15,
                h: 15
            }
        );
        assert_eq!(a.intersect(Rect { x: 10, ..a }), None);
    }

    fn px(c: &Canvas, x: usize, y: usize) -> [u8; 4] {
        let o = (y * c.width + x) * 4;
        [
            c.pixels[o],
            c.pixels[o + 1],
            c.pixels[o + 2],
            c.pixels[o + 3],
        ]
    }

    #[test]
    fn composite_places_clips_and_blends() {
        let mut dst = blank(Rect {
            x: 100,
            y: 50,
            w: 4,
            h: 3,
        });
        for p in dst.pixels.as_chunks_mut::<4>().0 {
            *p = [0, 0, 200, 255];
        }
        // 2 × 2 source with local origin (-1, -1): opaque, transparent,
        // half-transparent, opaque.
        let mut src = blank(Rect {
            x: -1,
            y: -1,
            w: 2,
            h: 2,
        });
        src.pixels.copy_from_slice(&[
            255, 0, 0, 255, 9, 9, 9, 0, // row 0
            200, 100, 0, 128, 0, 255, 0, 255, // row 1
        ]);
        // Local (-1, -1) → scene (102, 50) = dst pixel (2, 0).
        composite(&mut dst, &src, (103, 51));
        assert_eq!(px(&dst, 2, 0), [255, 0, 0, 255]);
        assert_eq!(px(&dst, 3, 0), [0, 0, 200, 255], "transparent keeps dst");
        assert_eq!(px(&dst, 3, 1), [0, 255, 0, 255]);
        // 128/255 of (200, 100, 0) over (0, 0, 200).
        assert_eq!(px(&dst, 2, 1), [100, 50, 99, 255]);
        assert_eq!(px(&dst, 1, 1), [0, 0, 200, 255], "untouched");
        // Partly outside: only the overlapping pixel (dst (0, 2)) changes.
        composite(&mut dst, &src, (101, 53));
        assert_eq!(px(&dst, 0, 2), [255, 0, 0, 255]);
        // Fully outside: nothing happens.
        let before = dst.pixels.clone();
        composite(&mut dst, &src, (0, 0));
        assert_eq!(dst.pixels, before);
    }

    #[test]
    fn over_onto_transparent_keeps_colour() {
        let mut d = [0, 0, 0, 0];
        over(&mut d, &[10, 20, 30, 77]);
        assert_eq!(d, [10, 20, 30, 77]);
    }
}
