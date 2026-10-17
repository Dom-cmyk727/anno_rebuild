//! Ships of a scenario as decoded figure frames, ready to draw.
//!
//! Rules (spec: `docs/formats/sze.md`, "Units"; `docs/render/iso.md`,
//! "Ships"): a `SCHIFF` unit names its figure type (`figure`, a
//! `FigurenHeader.dat` entry) and its world position. Each ship is drawn
//! with sprite set 0 of its entry (sails furled, as for a ship at anchor),
//! frame `attr mod n` of each zoom group; that `attr` is the facing is a
//! hypothesis (OQ-027 d).

use std::collections::BTreeMap;
use std::sync::Arc;

use anno_formats::figuren::{FigurenHeader, Frame};
use anno_formats::sze::{self, Scene};

use crate::figure;
use crate::texture::Rgba;

/// One ship: world position in tiles and its frame per zoom group (index =
/// group; `None` where the entry has no frame for that group or decoding
/// failed).
#[derive(Debug, Clone)]
pub struct UnitFrames {
    pub figure: u16,
    pub x: f32,
    pub y: f32,
    pub frames: Vec<Option<(Frame, Arc<Rgba>)>>,
}

/// Counts of [`ships`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct ShipStats {
    pub ships: usize,
    /// Ships whose figure entry or sprite set is missing.
    pub missing: usize,
    /// Frames that could not be read or decoded.
    pub failed_frames: usize,
}

/// Decoded frame per `(figure, group, frame index)`, shared by ships of the
/// same type and facing.
type FrameCache = BTreeMap<(u16, u32, usize), Option<(Frame, Arc<Rgba>)>>;

/// The ships of all players of `scene`. `read(group, offset, length)`
/// returns `length` bytes from `offset` of a zoom group's concatenated
/// figure volume (`Figuren<group><nn>.dat`), or `None`.
pub fn ships(
    scene: &Scene<'_>,
    header: &FigurenHeader<'_>,
    mut read: impl FnMut(u32, u64, u64) -> Option<Vec<u8>>,
) -> Result<(Vec<UnitFrames>, ShipStats), sze::Error> {
    let mut stats = ShipStats::default();
    let mut cache = FrameCache::new();
    let mut out = Vec::new();
    for player in scene.players()? {
        for unit in player.units("SCHIFF")? {
            stats.ships += 1;
            let set = header
                .entries
                .iter()
                .find(|e| e.id == u32::from(unit.figure))
                .and_then(|e| e.gfx.first().map(|s| (e, s)));
            let Some((entry, set)) = set else {
                stats.missing += 1;
                continue;
            };
            let groups = set
                .frame_sets
                .iter()
                .map(|fs| fs.group)
                .max()
                .map_or(0, |g| g + 1);
            let frames = (0..groups)
                .map(|g| {
                    let fs = set.frame_sets.iter().find(|fs| fs.group == g)?;
                    if fs.frames.is_empty() {
                        return None;
                    }
                    let k = usize::from(unit.attr) % fs.frames.len();
                    cache
                        .entry((unit.figure, g, k))
                        .or_insert_with(|| {
                            let f = &fs.frames[k];
                            let img = read(g, u64::from(f.offset), u64::from(f.length))
                                .and_then(|data| figure::decode(entry, f, &data).ok());
                            if img.is_none() {
                                stats.failed_frames += 1;
                            }
                            img.map(|img| (f.clone(), Arc::new(img)))
                        })
                        .clone()
                })
                .collect();
            out.push(UnitFrames {
                figure: unit.figure,
                x: unit.x(),
                y: unit.y(),
                frames,
            });
        }
    }
    Ok((out, stats))
}
