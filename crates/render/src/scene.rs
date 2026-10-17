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
//! - Units ([`UnitSprite`]) stand at their world position. Ships float on
//!   the sea surface ([`iso::SEA_LEVEL`]) and are drawn after all islands,
//!   back to front by `x + y`. Soldiers stand on the ground (terrain height
//!   at their position) and are drawn with their island's objects, in the
//!   objects' back-to-front order, as a 1 × 1 footprint on their tile.

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel::Island;

use std::sync::{Arc, OnceLock};

use anno_assets::texture::Rgba;
use anno_formats::figuren::Frame;

use crate::iso::{
    self, Canvas, DrawStats, HEIGHT_LIFT, Lift, Objects, Prepared, SpriteCache, SpriteDraw, TILE,
    WaterStyle, over,
};
use crate::sea::{self, IslandWater, SceneProbe, SeaPattern, WaterField};

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
    /// Units drawn ([`Scene::units`]).
    pub units: usize,
}

/// Layers [`Scene::render`] draws, in this order: ground; water objects
/// below sea level (surf on the sea floor); the sea surface (everywhere,
/// also between islands); water objects at or above sea level (rivers);
/// the other placed objects. Animated layers use `time_ms`. A viewer caches
/// the static layers and redraws only the animated ones per frame.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(clippy::struct_excessive_bools)]
pub struct Layers {
    pub ground: bool,
    pub water_below: bool,
    pub sea: bool,
    pub water_above: bool,
    pub objects: bool,
    pub time_ms: u64,
    /// Sea style; `Original` unless the user opts in.
    pub water_style: WaterStyle,
}

impl Layers {
    /// Everything, at time 0.
    pub const ALL: Layers = Layers {
        ground: true,
        water_below: true,
        sea: true,
        water_above: true,
        objects: true,
        time_ms: 0,
        water_style: WaterStyle::Original,
    };
    /// Nothing; combine with struct update syntax.
    pub const NONE: Layers = Layers {
        ground: false,
        water_below: false,
        sea: false,
        water_above: false,
        objects: false,
        time_ms: 0,
        water_style: WaterStyle::Original,
    };
}

/// A layer of placed-object sprites ([`Scene::sprites`]).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpriteLayer {
    /// Water objects below sea level (surf), drawn before the sea.
    WaterBelow,
    /// Water objects at or above sea level (rivers), drawn after the sea.
    WaterAbove,
    /// All other objects.
    Objects,
}

/// `ANLAGEN` ids of river pieces (`Fluss*`; 2702–2704 are waterfalls and
/// river mouths and keep their sprites). P: `docs/render/iso.md`, "Water".
/// Original values; to become content configuration (PLAN §0.1).
pub const RIVER_ANLAGEN: [u32; 4] = [2701, 2705, 2706, 2707];

/// Tolerance (`HEIGHT` units) of the river depth test: terrain up to this
/// much above the object's height still shows its water. Tuning value; how
/// the original's per-pixel `z` offsets of `BSH_RGBWZ` frames shift the
/// test is not traced (OQ-026 b).
pub const RIVER_TOLERANCE: f32 = 10.0;

/// Object records drawn: scenery, built objects, rivers and surf.
pub const OBJECT_RECORDS: [&str; 3] = ["GELAENDE", "PROD2", "FLUESSE"];

impl IslandData {
    /// Water objects below sea level for `style`.
    fn above_list(&self, style: WaterStyle) -> &[Prepared] {
        match style {
            WaterStyle::Original => &self.above,
            WaterStyle::Enhanced => &self.above_enhanced,
        }
    }

    fn below_list(&self, style: WaterStyle) -> &[Prepared] {
        match style {
            WaterStyle::Original => &self.below,
            WaterStyle::Enhanced => &self.below_enhanced,
        }
    }
}

/// Average colour of the most common sea floor texture at the lowest
/// `HEIGHT` of the islands' textured tiles. Islands carry deep sea
/// floor textures (e.g. `Meer_Meer02.bmp`) around their land; using their
/// colour for open water (texture 0, not drawn as terrain by the original;
/// OQ-026 b) makes the open sea continue the island water without a seam.
/// `None` if no island has textured ground.
pub fn sea_floor_colour(islands: &[Island<'_>], textures: &GroundTextures) -> Option<[u8; 4]> {
    let mut count = std::collections::BTreeMap::<(u8, u8), u64>::new();
    for island in islands {
        let (Ok(h), Ok(g)) = (island.heights(), island.gfx_ids()) else {
            continue;
        };
        for (&z, &id) in h.iter().zip(&g) {
            let tex = anno_formats::insel::GroundCell::from_gfx_id(id).texture;
            if tex != 0 {
                *count.entry((z, tex)).or_default() += 1;
            }
        }
    }
    let lowest = count.keys().next()?.0;
    // The most common texture at that height (ties: lowest id).
    let (&(_, tex), _) = count
        .iter()
        .filter(|((z, _), _)| *z == lowest)
        .max_by_key(|&(&(_, t), &k)| (k, std::cmp::Reverse(t)))?;
    textures.average(u16::from(tex))
}

/// Per-island data decoded once: placed objects split by layer in draw
/// order, the `GFXID` and `HEIGHT` grids, and the sea inputs.
struct IslandData {
    below: Vec<Prepared>,
    /// `below` without the original's surf sprites, which the enhanced
    /// water replaces with its own surf.
    below_enhanced: Vec<Prepared>,
    /// `above` without the river pieces, which the enhanced water draws
    /// itself (`sea::IslandWater` river level).
    above_enhanced: Vec<Prepared>,
    /// River pieces for the enhanced river surface.
    rivers: Vec<sea::RiverPiece>,
    above: Vec<Prepared>,
    other: Vec<Prepared>,
    /// `None` if a grid cannot be read (no ground drawn).
    grids: Option<(Vec<u16>, Vec<u8>)>,
    water: OnceLock<Option<IslandWater>>,
}

/// A unit to draw (a ship or soldier of a scenario): world position in
/// tiles, the ground height it stands on (`None` = on the sea surface), and
/// its decoded frame and shadow frame per zoom group (`None` where it has
/// none).
#[derive(Debug, Clone)]
pub struct UnitSprite {
    pub x: f32,
    pub y: f32,
    /// `HEIGHT` units; `None` for ships.
    pub z: Option<f32>,
    pub frames: Vec<Option<(Frame, Arc<Rgba>)>>,
    pub shadows: Vec<Option<(Frame, Arc<Rgba>)>>,
}

impl UnitSprite {
    /// Scene screen point of the frame anchor at `group`: the world point
    /// projected as [`iso::tile_top`], raised to the ground height or the
    /// sea surface.
    pub fn anchor(&self, group: usize, lift: Lift) -> (i32, i32) {
        let (tw, th) = TILE[group];
        #[allow(clippy::cast_possible_truncation)]
        let (sx, sy) = (
            ((self.x - self.y) * tw as f32 / 2.0).round() as i32,
            ((self.x + self.y) * th as f32 / 2.0).round() as i32,
        );
        let raise = match self.z {
            // Height in 1/1024 units, so sloped ground lifts smoothly.
            #[allow(clippy::cast_possible_truncation)]
            Some(z) => lift.scaled((z * 1024.0).round() as i64, 1024, group),
            None => lift.scaled(iso::SEA_LEVEL, 1, group),
        };
        (sx, sy - raise)
    }

    /// Draws shadow and frame at `group` with the anchor shifted by
    /// `-offset` (into a canvas with other coordinates); returns whether the
    /// frame touched the canvas.
    fn draw(&self, canvas: &mut Canvas, group: usize, lift: Lift, offset: (i32, i32)) -> bool {
        let Some(Some((frame, img))) = self.frames.get(group) else {
            return false;
        };
        let a = self.anchor(group, lift);
        let anchor = (a.0 - offset.0, a.1 - offset.1);
        if let Some(Some((sf, simg))) = self.shadows.get(group) {
            iso::blit_sprite(canvas, simg, sf, anchor, true, |_, _, px| Some(px));
        }
        let (x, y) = (
            anchor.0 - i32::from(frame.anchor_x),
            anchor.1 - i32::from(frame.anchor_y),
        );
        let hit = canvas.overlaps(x, y, i32::from(frame.width), i32::from(frame.height));
        iso::blit_sprite(canvas, img, frame, anchor, false, |_, _, px| Some(px));
        hit
    }
}

/// Everything needed to draw a scenario: its islands, ground textures,
/// `Anlagen.dat`, island placements per zoom group, decoded sprites, the
/// objects of each island sorted for drawing, and the sea pattern per zoom
/// group (built on first use).
pub struct Scene<'a> {
    pub name: String,
    pub islands: Vec<Island<'a>>,
    pub textures: GroundTextures,
    pub anlagen: AnlagenFile<'a>,
    /// Placements per zoom group, in draw order.
    pub layouts: [Vec<Placement>; 3],
    /// Decoded sprite frames, shared by all renders and threads.
    pub sprites: SpriteCache,
    /// Elevation scale ([`HEIGHT_LIFT`] unless changed for inspection).
    pub lift: Lift,
    /// Ships, drawn with the objects layer (empty unless set by the caller).
    pub units: Vec<UnitSprite>,
    objects: Vec<IslandData>,
    seas: [OnceLock<Option<SeaPattern>>; 3],
}

impl<'a> Scene<'a> {
    /// Open water (texture 0) gets the colour of the deepest textured sea
    /// floor of `islands` ([`sea_floor_colour`]) if there is one, else the
    /// colour already set in `textures`.
    pub fn new(
        name: String,
        islands: Vec<Island<'a>>,
        mut textures: GroundTextures,
        anlagen: AnlagenFile<'a>,
        lift: Lift,
    ) -> Self {
        if let Some(c) = sea_floor_colour(&islands, &textures) {
            textures.set_water(c);
        }
        let boxes: Vec<IslandBox> = islands.iter().map(IslandBox::of).collect();
        let objects = islands
            .iter()
            .map(|i| {
                let list = |which| iso::prepare_objects(i, &anlagen, &OBJECT_RECORDS, which);
                let below = list(Objects::WaterBelowSea);
                let below_enhanced = below
                    .iter()
                    .copied()
                    .filter(|(o, _)| u32::from(o.anlage) != iso::SURF_ANLAGE)
                    .collect();
                let above = list(Objects::WaterAboveSea);
                let is_river = |o: &anno_formats::insel::PlacedObject| {
                    RIVER_ANLAGEN.contains(&u32::from(o.anlage))
                };
                let rivers = above
                    .iter()
                    .filter(|(o, _)| is_river(o))
                    .map(|&(o, (w, h))| (o.x, o.y, w, h, o.height()))
                    .collect();
                let above_enhanced = above
                    .iter()
                    .copied()
                    .filter(|(o, _)| !is_river(o))
                    .collect();
                IslandData {
                    below,
                    below_enhanced,
                    above_enhanced,
                    rivers,
                    above,
                    other: list(Objects::NotWater),
                    grids: i.gfx_ids().ok().zip(i.heights().ok()),
                    water: OnceLock::new(),
                }
            })
            .collect();
        Self {
            name,
            layouts: [0, 1, 2].map(|g| layout(&boxes, g)),
            islands,
            textures,
            anlagen,
            sprites: SpriteCache::default(),
            lift,
            units: Vec::new(),
            objects,
            seas: Default::default(),
        }
    }

    /// The sea pattern of `group`, rendered on first use.
    pub fn sea(&self, group: usize) -> Option<&SeaPattern> {
        self.seas[group]
            .get_or_init(|| {
                SeaPattern::new(
                    &self.anlagen,
                    group,
                    self.lift,
                    self.textures.water(),
                    &self.sprites,
                )
            })
            .as_ref()
    }

    /// The sea pattern of `group` if it has been rendered already (never
    /// blocks).
    pub fn sea_if_ready(&self, group: usize) -> Option<&SeaPattern> {
        self.seas[group].get().and_then(Option::as_ref)
    }

    /// Whether island `index` has water objects below / above sea level.
    pub fn has_water_objects(&self, index: usize) -> (bool, bool) {
        let o = &self.objects[index];
        (!o.below.is_empty(), !o.above.is_empty())
    }

    /// The sprites of `layer` that touch `rect` (scene screen coordinates at
    /// `group`) at `time_ms`, in draw order (islands in layout order, each
    /// island's objects in its order), anchors in scene coordinates.
    pub fn sprites(
        &self,
        rect: Rect,
        group: usize,
        time_ms: u64,
        layer: SpriteLayer,
        style: WaterStyle,
    ) -> Vec<SpriteDraw<'_>> {
        let mut out = Vec::new();
        for p in &self.layouts[group] {
            if p.bounds.intersect(rect).is_none() {
                continue;
            }
            let o = &self.objects[p.index];
            let list = match layer {
                SpriteLayer::WaterBelow => o.below_list(style),
                SpriteLayer::WaterAbove => o.above_list(style),
                SpriteLayer::Objects => &o.other,
            };
            let start = out.len();
            iso::place_prepared(
                list,
                &self.anlagen,
                group,
                self.lift,
                time_ms,
                |x, y, w, h| {
                    Rect {
                        x: x + p.offset.0,
                        y: y + p.offset.1,
                        w,
                        h,
                    }
                    .intersect(rect)
                    .is_some()
                },
                &mut out,
            );
            for d in &mut out[start..] {
                d.anchor = (d.anchor.0 + p.offset.0, d.anchor.1 + p.offset.1);
                if layer != SpriteLayer::Objects && style == WaterStyle::Enhanced && !d.is_shadow()
                {
                    d.key.1 = iso::SMOOTHED;
                }
            }
        }
        out
    }

    /// Static sea data of `rect` at `group`.
    pub fn water_field(&self, rect: Rect, group: usize) -> WaterField {
        let (_, probe) = self.ground_with_probe(rect, group);
        self.water_field_from(rect, &probe)
    }

    /// Static sea data of `rect` from the ground seen there (`probe`, from
    /// [`Scene::ground_with_probe`]).
    pub fn water_field_from(&self, rect: Rect, probe: &SceneProbe) -> WaterField {
        let water = |i: usize| {
            self.objects[i]
                .water
                .get_or_init(|| IslandWater::new(&self.islands[i], &self.objects[i].rivers))
                .as_ref()
        };
        sea::water_field(rect, probe, water)
    }

    /// Water depth for the wave simulation at world tile position
    /// `(x, y)`: waterline minus the smooth terrain of the island there
    /// (as the enhanced waterline), open sea (floor height 0) elsewhere.
    /// Linear in the number of islands; a spatial index can replace the
    /// scan for maps with many islands.
    pub fn fluid_depth(&self, x: f32, y: f32) -> f32 {
        #[allow(clippy::cast_precision_loss)]
        let waterline = iso::WATERLINE as f32;
        for (k, isl) in self.islands.iter().enumerate() {
            let (lx, ly) = (x - f32::from(isl.x), y - f32::from(isl.y));
            if lx < 0.0 || ly < 0.0 || lx >= f32::from(isl.width) || ly >= f32::from(isl.height) {
                continue;
            }
            if let Some((_, heights)) = &self.objects[k].grids {
                let (w, h) = (usize::from(isl.width), usize::from(isl.height));
                return waterline - sea::smooth_height(heights, w, h, lx, ly);
            }
        }
        waterline
    }

    /// Terrain height (`HEIGHT` units) seen at scene pixel `(sx, sy)` of
    /// `rect` according to `probe`; `None` where no island ground is drawn.
    pub fn terrain_at(&self, probe: &SceneProbe, rect: Rect, sx: i32, sy: i32) -> Option<f32> {
        if sx < rect.x || sy < rect.y || sx >= rect.x + rect.w || sy >= rect.y + rect.h {
            return None;
        }
        let i = usize::try_from((sy - rect.y) * rect.w + (sx - rect.x)).ok()?;
        let island = probe.island[i];
        if island == sea::NO_ISLAND {
            return None;
        }
        let k = usize::try_from(island).ok()?;
        let (_, heights) = self.objects[k].grids.as_ref()?;
        let isl = &self.islands[k];
        let [x, y] = probe.pos[i];
        Some(iso::terrain_height(
            heights,
            usize::from(isl.width),
            usize::from(isl.height),
            x,
            y,
        ))
    }

    /// The ground layer of `rect` at `group` and, per pixel, which island's
    /// terrain is seen there and at which tile position.
    pub fn ground_with_probe(&self, rect: Rect, group: usize) -> (Canvas, SceneProbe) {
        let mut canvas = blank(rect);
        let n = canvas.width * canvas.height;
        let mut probe = SceneProbe {
            island: vec![sea::NO_ISLAND; n],
            pos: vec![[0.0; 2]; n],
        };
        for p in &self.layouts[group] {
            let Some(r) = p.bounds.intersect(rect) else {
                continue;
            };
            let island = &self.islands[p.index];
            let Some((gfx, heights)) = &self.objects[p.index].grids else {
                continue;
            };
            let mut local = blank(Rect {
                x: r.x - p.offset.0,
                y: r.y - p.offset.1,
                ..r
            });
            let mut lp = iso::GroundProbe::for_canvas(&local);
            iso::draw_ground_probed(
                &mut local,
                (usize::from(island.width), usize::from(island.height)),
                gfx,
                heights,
                &self.textures,
                group,
                self.lift,
                Some(&mut lp),
            );
            composite(&mut canvas, &local, p.offset);
            let idx = u32::try_from(p.index).expect("fits");
            for (k, pos) in lp.pos.iter().enumerate() {
                if pos[0].is_nan() {
                    continue;
                }
                let (lx, ly) = (
                    i32::try_from(k % local.width).expect("fits"),
                    i32::try_from(k / local.width).expect("fits"),
                );
                let (sx, sy) = (r.x + lx, r.y + ly);
                let i = usize::try_from((sy - rect.y) * rect.w + (sx - rect.x)).expect("inside");
                probe.island[i] = idx;
                probe.pos[i] = *pos;
            }
        }
        (canvas, probe)
    }

    /// Renders `layers` of the scene region `rect` at `group` into a new
    /// canvas (transparent where nothing is drawn; with the sea layer the
    /// whole region is covered).
    pub fn render(&self, rect: Rect, group: usize, layers: Layers) -> (Canvas, SceneStats) {
        self.render_with_terrain(rect, group, layers, None)
    }

    /// [`Scene::render`] with the terrain height seen at each scene pixel
    /// given by `terrain` (e.g. from a chunk cache) instead of rasterising
    /// the ground again for the river depth test.
    pub fn render_with_terrain(
        &self,
        rect: Rect,
        group: usize,
        layers: Layers,
        terrain: Option<&dyn Fn(i32, i32) -> Option<f32>>,
    ) -> (Canvas, SceneStats) {
        let mut canvas = blank(rect);
        let mut stats = SceneStats::default();
        let t = layers.time_ms;
        let mut add = |s: DrawStats| {
            stats.objects.drawn += s.drawn;
            stats.objects.invisible += s.invisible;
            stats.objects.failed += s.failed;
        };
        let mut no_ground = 0;
        let mut drawn = 0;
        let mut probe: Option<SceneProbe> = None;
        // Pass 1: ground and water objects under the sea surface.
        // Pass 2: water objects above it and the other objects.
        for pass in 0..2 {
            for p in &self.layouts[group] {
                let Some(r) = p.bounds.intersect(rect) else {
                    continue;
                };
                let island = &self.islands[p.index];
                let objects = &self.objects[p.index];
                let first = pass == 0;
                let wanted = if first {
                    layers.ground || (layers.water_below && !objects.below.is_empty())
                } else {
                    (layers.water_above && !objects.above.is_empty()) || layers.objects
                };
                if !wanted {
                    continue;
                }
                // Temporary canvas in the island's local screen coordinates.
                let mut local = blank(Rect {
                    x: r.x - p.offset.0,
                    y: r.y - p.offset.1,
                    ..r
                });
                if first {
                    drawn += 1;
                    if layers.ground {
                        match &objects.grids {
                            Some((gfx, heights)) => iso::draw_ground_grids(
                                &mut local,
                                (usize::from(island.width), usize::from(island.height)),
                                gfx,
                                heights,
                                &self.textures,
                                group,
                                self.lift,
                            ),
                            None => no_ground += 1,
                        }
                    }
                    if layers.water_below {
                        // Enhanced water smooths the checkerboard of these
                        // sprites too (river mouths below the waterline).
                        let variant = match layers.water_style {
                            WaterStyle::Original => 0,
                            WaterStyle::Enhanced => iso::SMOOTHED,
                        };
                        add(iso::draw_prepared_tested(
                            &mut local,
                            objects.below_list(layers.water_style),
                            &self.anlagen,
                            group,
                            self.lift,
                            t,
                            &self.sprites,
                            variant,
                            |_, _, _| true,
                        ));
                    }
                } else {
                    if layers.water_above {
                        // Depth test against the terrain seen at each pixel,
                        // as the original's depth buffer: river water shows
                        // only where the bed lies below its surface.
                        let off = p.offset;
                        let probe = match terrain {
                            Some(_) => None,
                            None => Some(
                                &*probe
                                    .get_or_insert_with(|| self.ground_with_probe(rect, group).1),
                            ),
                        };
                        let keep = |x: i32, y: i32, h: u8| {
                            let (sx, sy) = (x + off.0, y + off.1);
                            let z = match (terrain, probe) {
                                (Some(f), _) => f(sx, sy),
                                (None, Some(pr)) => self.terrain_at(pr, rect, sx, sy),
                                (None, None) => None,
                            };
                            z.is_none_or(|z| z <= f32::from(h) + RIVER_TOLERANCE)
                        };
                        add(iso::draw_prepared_tested(
                            &mut local,
                            objects.above_list(layers.water_style),
                            &self.anlagen,
                            group,
                            self.lift,
                            t,
                            &self.sprites,
                            match layers.water_style {
                                WaterStyle::Original => 0,
                                WaterStyle::Enhanced => iso::SMOOTHED,
                            },
                            keep,
                        ));
                    }
                    if layers.objects {
                        let (s, units) =
                            self.draw_island_objects(&mut local, p, &objects.other, group);
                        add(s);
                        stats.units += units;
                    }
                }
                composite(&mut canvas, &local, p.offset);
            }
            if pass == 1 && layers.objects {
                stats.units += self.draw_units(&mut canvas, group);
            }
            if pass == 0
                && layers.sea
                && let Some(pattern) = self.sea(group)
            {
                let field = self.water_field(rect, group);
                let w = canvas.width;
                sea::shade_sea(
                    canvas.pixels.as_chunks_mut::<4>().0,
                    w,
                    rect,
                    |i| {
                        (
                            field.cover[i],
                            field.shore[i],
                            field.depth[i],
                            field.flags[i],
                        )
                    },
                    pattern,
                    t,
                    layers.water_style,
                    group,
                    self.lift,
                );
            }
        }
        stats.islands = drawn;
        stats.no_ground = no_ground;
        (canvas, stats)
    }

    /// Ground height (`HEIGHT` units) at world point `(x, y)`, from the
    /// island whose rectangle contains it; `None` over open sea.
    pub fn ground_height(&self, x: f32, y: f32) -> Option<f32> {
        self.islands.iter().zip(&self.objects).find_map(|(i, o)| {
            let (lx, ly) = (x - f32::from(i.x), y - f32::from(i.y));
            let inside =
                (0.0..f32::from(i.width)).contains(&lx) && (0.0..f32::from(i.height)).contains(&ly);
            let (_, heights) = o.grids.as_ref().filter(|_| inside)?;
            let (w, h) = (usize::from(i.width), usize::from(i.height));
            Some(iso::terrain_height(heights, w, h, lx, ly))
        })
    }

    /// Draws an island's objects (`list`, back to front) with the land units
    /// standing on it interleaved: a unit on local tile `(tx, ty)` counts as
    /// a 1 × 1 footprint, key `(tx + 1 + ty + 1, tx)`, and is drawn after
    /// the objects whose key is not larger. Returns the object statistics
    /// and the number of units drawn.
    fn draw_island_objects(
        &self,
        local: &mut Canvas,
        p: &Placement,
        list: &[Prepared],
        group: usize,
    ) -> (DrawStats, usize) {
        let island = &self.islands[p.index];
        let (ix, iy) = (f32::from(island.x), f32::from(island.y));
        let (w, h) = (f32::from(island.width), f32::from(island.height));
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let mut units: Vec<((u32, u32), &UnitSprite)> = self
            .units
            .iter()
            .filter(|u| u.z.is_some())
            .filter(|u| (ix..ix + w).contains(&u.x) && (iy..iy + h).contains(&u.y))
            .map(|u| {
                let (tx, ty) = ((u.x - ix) as u32, (u.y - iy) as u32);
                ((tx + 1 + ty + 1, tx), u)
            })
            .collect();
        units.sort_by_key(|(k, _)| *k);
        let key = |(o, (fw, fh)): &Prepared| {
            (
                u32::from(o.x) + u32::from(*fw) + u32::from(o.y) + u32::from(*fh),
                u32::from(o.x),
            )
        };
        let mut stats = DrawStats::default();
        let mut add = |s: DrawStats| {
            stats.drawn += s.drawn;
            stats.invisible += s.invisible;
            stats.failed += s.failed;
        };
        let draw = |c: &mut Canvas, part: &[Prepared]| {
            iso::draw_prepared(c, part, &self.anlagen, group, self.lift, 0, &self.sprites)
        };
        let (mut start, mut drawn) = (0, 0);
        for (k, u) in units {
            let end = start + list[start..].partition_point(|o| key(o) <= k);
            add(draw(local, &list[start..end]));
            start = end;
            if u.draw(local, group, self.lift, p.offset) {
                drawn += 1;
            }
        }
        add(draw(local, &list[start..]));
        (stats, drawn)
    }

    /// Screen rectangle of unit `u` at `group`, if it has a frame there.
    fn unit_rect(&self, u: &UnitSprite, group: usize) -> Option<Rect> {
        let (frame, _) = u.frames.get(group)?.as_ref()?;
        let anchor = u.anchor(group, self.lift);
        Some(Rect {
            x: anchor.0 - i32::from(frame.anchor_x),
            y: anchor.1 - i32::from(frame.anchor_y),
            w: i32::from(frame.width),
            h: i32::from(frame.height),
        })
    }

    /// Whether any unit is drawn into `rect` at `group`.
    pub fn units_touch(&self, rect: Rect, group: usize) -> bool {
        self.units.iter().any(|u| {
            self.unit_rect(u, group)
                .is_some_and(|r| r.intersect(rect).is_some())
        })
    }

    /// Scene screen rectangle at `group` covering all islands and units;
    /// `None` if there are neither.
    pub fn bounds(&self, group: usize) -> Option<Rect> {
        let units = self.units.iter().filter_map(|u| self.unit_rect(u, group));
        scene_bounds(&self.layouts[group])
            .into_iter()
            .chain(units)
            .reduce(Rect::union)
    }

    /// Draws the units on the sea surface (ships) that touch `canvas` at
    /// `group`, back to front by `x + y`; returns how many were drawn.
    fn draw_units(&self, canvas: &mut Canvas, group: usize) -> usize {
        let mut order: Vec<&UnitSprite> = self.units.iter().filter(|u| u.z.is_none()).collect();
        order.sort_by(|a, b| (a.x + a.y).total_cmp(&(b.x + b.y)));
        order
            .into_iter()
            .filter(|u| u.draw(canvas, group, self.lift, (0, 0)))
            .count()
    }
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
    fn unit_anchor_projects_world_point_onto_sea_surface() {
        let mut u = UnitSprite {
            x: 10.5,
            y: 4.5,
            z: None,
            frames: Vec::new(),
            shadows: Vec::new(),
        };
        // Group 2 tile 20 × 10: ((x − y)·10, (x + y)·5).
        assert_eq!(u.anchor(2, Lift::NONE), (60, 75));
        // Sea surface 32 units at 2.059 px per unit and group 0, scaled by
        // 20/84: 15 px.
        assert_eq!(u.anchor(2, HEIGHT_LIFT), (60, 60));
        // On the ground at height 16: half the sea-surface lift.
        u.z = Some(16.0);
        assert_eq!(u.anchor(2, HEIGHT_LIFT), (60, 68));
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
