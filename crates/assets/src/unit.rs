//! Ships and soldiers of a scenario as decoded figure frames, ready to draw.
//!
//! Rules (spec: `docs/formats/sze.md`, "Units"; `docs/render/iso.md`,
//! "Scene composition"): a `SCHIFF` or `SOLDAT` unit names its figure type
//! (`figure`, a `FigurenHeader.dat` entry) and its world position. Each unit
//! is drawn with sprite set 0 of its entry (ships: sails furled; soldiers:
//! standing, 8 frames), frame `attr mod n` of each zoom group, and the same
//! frame of the set's `SHADOW` if it has one. `attr` is the starting facing
//! (ships 16 steps, soldiers 8; D: `docs/re/fire-and-ships.md` §3.3); that
//! frame `k` of a set shows facing `k` at the default view rotation is an
//! assumption (the facing → frame mapping is not traced). Figures of the player-coloured types (soldiers,
//! scouts, pack animals, flags: [`figure::PLAYER_COLOURED_TYPES`]) are
//! decoded in their owner's colour ([`figure::recolour`]). A unit belongs
//! to the `SPIELER` block that holds it (the unit's own owner byte is never
//! read, D: `docs/re/annoscene.md` §7); the players' colours are assigned
//! as at the start of a new single-player game ([`start_colours`]).

use std::collections::{BTreeMap, BTreeSet};
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

/// Player colours at the start of a new single-player game: colour slot
/// (1–8) per player index, and the human slots the game removes.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct StartColours {
    pub slots: BTreeMap<u32, u8>,
    /// Human slots without a colour other than the first: deleted with
    /// their objects at game start.
    pub removed: BTreeSet<u32>,
}

/// The colour assignment of a new single-player game (D: `AnnoFrame.dll`
/// game start `0x10029100` and colour screen `0x1001d180`,
/// `docs/re/annoscene.md` §9): a `HEAD` colour 1–8 is kept; the first human
/// slot (types 8, 11; never player 31) gets the colour the player picks on
/// the colour screen — here, as a stand-in, the lowest colour of the pool —
/// which leaves the pool; every other player still at `0xFF` is a human
/// slot (removed), a computer (type 12: the lowest colour left in the pool)
/// or another type (no colour). Preset computer colours stay in the pool,
/// as in the original, so two players can share a colour.
pub fn start_colours(scene: &Scene<'_>) -> Result<StartColours, sze::Error> {
    let heads: Vec<(u32, sze::PlayerHead<'_>)> = scene
        .players()?
        .iter()
        .filter(|p| p.index != 31)
        .filter_map(|p| p.head().map(|h| (p.index, h)))
        .collect();
    let mut out = StartColours::default();
    let mut pool: BTreeSet<u8> = (1..=8).collect();
    let local = heads.iter().find(|(_, h)| h.is_human()).map(|(i, _)| *i);
    for (index, h) in &heads {
        if (1..=8).contains(&h.colour) {
            out.slots.insert(*index, h.colour);
            if Some(*index) == local {
                pool.remove(&h.colour);
            }
        }
    }
    if let Some(i) = local
        && !out.slots.contains_key(&i)
        && let Some(c) = pool.pop_first()
    {
        out.slots.insert(i, c);
    }
    for (index, h) in &heads {
        if h.colour != 0xFF || Some(*index) == local {
            continue;
        }
        if h.is_human() {
            out.removed.insert(*index);
        } else if h.is_computer()
            && let Some(c) = pool.pop_first()
        {
            out.slots.insert(*index, c);
        }
    }
    Ok(out)
}

/// The units of `kinds` of all players of `scene`, in file order, with
/// player colours from `palette` (e.g. [`figure::ORIGINAL_PLAYER_COLOURS`],
/// slot `n` = `palette[n - 1]`) assigned by [`start_colours`]; units of
/// removed human slots are left out.
/// `read(group, offset, length)` returns `length` bytes from `offset` of a
/// zoom group's concatenated figure volume (`Figuren<group><nn>.dat`), or
/// `None`.
pub fn units(
    scene: &Scene<'_>,
    header: &FigurenHeader<'_>,
    kinds: &[UnitKind],
    palette: &[[u8; 3]],
    mut read: impl FnMut(u32, u64, u64) -> Option<Vec<u8>>,
) -> Result<(Vec<UnitFrames>, UnitStats), sze::Error> {
    let mut stats = UnitStats::default();
    let mut cache = FrameCache::new();
    let mut out = Vec::new();
    let start = start_colours(scene)?;
    for player in scene.players()? {
        if start.removed.contains(&player.index) {
            continue;
        }
        let player_colour = start
            .slots
            .get(&player.index)
            .and_then(|&slot| palette.get(usize::from(slot) - 1))
            .copied();
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
                    .then_some(player_colour)
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
