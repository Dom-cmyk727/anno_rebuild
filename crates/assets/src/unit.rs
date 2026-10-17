//! Ships and soldiers of a scenario as decoded figure frames, ready to draw.
//!
//! Rules (spec: `docs/formats/sze.md`, "Units"; `docs/render/iso.md`,
//! "Scene composition"): a `SCHIFF` or `SOLDAT` unit names its figure type
//! (`figure`, a `FigurenHeader.dat` entry) and its world position. Each unit
//! is drawn with sprite set 0 of its entry (ships: sails furled; soldiers:
//! standing, 8 frames), frame `attr mod n` of each zoom group, and the same
//! frame of the set's `SHADOW` if it has one; that `attr` is the facing is a
//! hypothesis (OQ-027 d). Figures of the player-coloured types (soldiers,
//! scouts, pack animals, flags: [`figure::PLAYER_COLOURED_TYPES`]) are
//! decoded in their owner's colour ([`figure::recolour`]); the owner is
//! the `SPIELER` index `p`, drawn in colour `colours[p]` — a stand-in, since
//! the original assigns colours on its colour-selection screen and a
//! scenario does not store them (OQ-027 f). Owners beyond the table keep
//! the palette colours.

use std::collections::BTreeMap;
use std::sync::Arc;

use anno_formats::figuren::{FigurenHeader, Frame, FrameSet};
use anno_formats::sze::{self, Scene};

use crate::figure;
use crate::texture::Rgba;

/// A decoded frame and its header.
pub type DecodedFrame = (Frame, Arc<Rgba>);

/// Which `SPIELER` record a unit comes from.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum UnitKind {
    /// `SCHIFF`, drawn on the sea surface.
    Ship,
    /// `SOLDAT`, drawn on the ground.
    Soldier,
}

impl UnitKind {
    pub fn record(self) -> &'static str {
        match self {
            UnitKind::Ship => "SCHIFF",
            UnitKind::Soldier => "SOLDAT",
        }
    }
}

/// One unit: world position in tiles and, per zoom group (index = group),
/// its frame and shadow frame (`None` where the entry has none or decoding
/// failed).
#[derive(Debug, Clone)]
pub struct UnitFrames {
    pub kind: UnitKind,
    pub figure: u16,
    pub x: f32,
    pub y: f32,
    pub frames: Vec<Option<DecodedFrame>>,
    pub shadows: Vec<Option<DecodedFrame>>,
}

/// Counts of [`units`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct UnitStats {
    pub units: usize,
    /// Units whose figure entry or sprite set is missing.
    pub missing: usize,
    /// Frames that could not be read or decoded.
    pub failed_frames: usize,
}

/// Decoded frame per `(figure, shadow, group, frame index, colour)`, shared
/// by units of the same type, facing and colour.
type FrameCache = BTreeMap<(u16, bool, u32, usize, Option<[u8; 3]>), Option<DecodedFrame>>;

/// The units of `kinds` of all players of `scene`, in file order, with
/// player colours from `colours` (e.g. [`figure::ORIGINAL_PLAYER_COLOURS`]).
/// `read(group, offset, length)` returns `length` bytes from `offset` of a
/// zoom group's concatenated figure volume (`Figuren<group><nn>.dat`), or
/// `None`.
pub fn units(
    scene: &Scene<'_>,
    header: &FigurenHeader<'_>,
    kinds: &[UnitKind],
    colours: &[[u8; 3]],
    mut read: impl FnMut(u32, u64, u64) -> Option<Vec<u8>>,
) -> Result<(Vec<UnitFrames>, UnitStats), sze::Error> {
    let mut stats = UnitStats::default();
    let mut cache = FrameCache::new();
    let mut out = Vec::new();
    for player in scene.players()? {
        for &kind in kinds {
            for unit in player.units(kind.record())? {
                stats.units += 1;
                let set = header
                    .entries
                    .iter()
                    .find(|e| e.id == u32::from(unit.figure))
                    .and_then(|e| e.gfx.first().map(|s| (e, s)));
                let Some((entry, set)) = set else {
                    stats.missing += 1;
                    continue;
                };
                let colour = figure::player_coloured(entry)
                    .then(|| colours.get(usize::from(unit.owner)).copied())
                    .flatten();
                let mut frames_of = |sets: &[FrameSet],
                                     shadow: bool|
                 -> Vec<Option<DecodedFrame>> {
                    let groups = sets.iter().map(|fs| fs.group).max().map_or(0, |g| g + 1);
                    (0..groups)
                        .map(|g| {
                            let fs = sets.iter().find(|fs| fs.group == g)?;
                            if fs.frames.is_empty() {
                                return None;
                            }
                            let k = usize::from(unit.attr) % fs.frames.len();
                            // Shadows are coverage masks: no colours.
                            let colour = if shadow { None } else { colour };
                            cache
                                .entry((unit.figure, shadow, g, k, colour))
                                .or_insert_with(|| {
                                    let f = &fs.frames[k];
                                    let img = read(g, u64::from(f.offset), u64::from(f.length))
                                        .and_then(|data| {
                                            figure::decode_coloured(entry, f, &data, colour).ok()
                                        });
                                    if img.is_none() {
                                        stats.failed_frames += 1;
                                    }
                                    img.map(|img| (f.clone(), Arc::new(img)))
                                })
                                .clone()
                        })
                        .collect()
                };
                let frames = frames_of(&set.frame_sets, false);
                let shadows = set
                    .shadow
                    .as_ref()
                    .map(|s| frames_of(&s.frame_sets, true))
                    .unwrap_or_default();
                out.push(UnitFrames {
                    kind,
                    figure: unit.figure,
                    x: unit.x(),
                    y: unit.y(),
                    frames,
                    shadows,
                });
            }
        }
    }
    Ok((out, stats))
}
