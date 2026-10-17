//! Figure frames (`Figuren*.dat`) as 8-bit RGBA images.
//!
//! Mapping (spec: `docs/formats/figuren.md`): a covered pixel of a sprite
//! frame with palette index `i` becomes `[R, G, B, 255]` from the entry's
//! colour table `Frame::palette`; a covered pixel of a `BSH_ALPHA` shadow
//! frame becomes `[0, 0, 0, 255]` (a coverage mask); uncovered pixels become
//! `[0, 0, 0, 0]`. The 2-bit `level` of `A` formats becomes the alpha
//! ([`LEVEL_ALPHA`]); the `z` byte (a depth offset) is not applied.

use std::fmt;

use anno_formats::figuren::{self, Entry, Frame, FrameFormat};

use crate::texture::Rgba;

/// Opacity of the 2-bit `level` of `A` formats: 25 %, 50 %, 75 %, opaque
/// (D: `AnnoDisplay.dll` `0x10018890`, `0x100193f0`, `0x100199a0`;
/// `docs/re/annodisplay.md` §3).
pub const LEVEL_ALPHA: [u8; 4] = [64, 128, 191, 255];

/// Alpha of a decoded pixel of `format`.
pub fn pixel_alpha(format: FrameFormat, level: u8) -> u8 {
    if format.has_level() {
        LEVEL_ALPHA[usize::from(level & 3)]
    } else {
        255
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// `Frame::format_raw` is not a known format.
    UnknownFormat,
    /// `Frame::palette` names no colour table of the entry.
    MissingPalette(u16),
    /// `offset .. offset + length` lies outside the group's volume data.
    OutsideVolume,
    Decode(figuren::Error),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::UnknownFormat => write!(f, "unknown frame format"),
            Error::MissingPalette(i) => write!(f, "colour table {i} not in entry"),
            Error::OutsideVolume => write!(f, "frame data outside the volume"),
            Error::Decode(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for Error {}

/// Decodes `frame` of `entry`; `volume` is the concatenated volume data of
/// the frame's zoom group.
pub fn to_rgba(entry: &Entry<'_>, frame: &Frame, volume: &[u8]) -> Result<Rgba, Error> {
    let start = usize::try_from(frame.offset).map_err(|_| Error::OutsideVolume)?;
    let len = usize::try_from(frame.length).map_err(|_| Error::OutsideVolume)?;
    let data = start
        .checked_add(len)
        .and_then(|end| volume.get(start..end))
        .ok_or(Error::OutsideVolume)?;
    decode(entry, frame, data)
}

/// Decodes `frame` of `entry` from its pixel data alone (`frame.length`
/// bytes from `frame.offset` of the group's volume), for callers that read
/// single frames instead of whole volumes.
pub fn decode(entry: &Entry<'_>, frame: &Frame, data: &[u8]) -> Result<Rgba, Error> {
    decode_coloured(entry, frame, data, None)
}

/// The eight player colours of the original, as RGB (D: `AnnoDisplay.dll`
/// `0x1003be70`, eight `0x00RRGGBB` words; colour variant 0 there means "no
/// recolouring"). Defaults: callers pass their own table, so more players
/// or other colours are configuration (`docs/render/iso.md`, "Player
/// colours").
pub const ORIGINAL_PLAYER_COLOURS: [[u8; 3]; 8] = [
    [0, 187, 255],
    [255, 68, 0],
    [255, 244, 10],
    [0, 255, 176],
    [255, 138, 0],
    [47, 254, 41],
    [248, 57, 255],
    [255, 244, 222],
];

/// Figure types (`BEWOHNTYPE`) whose colour tables get player-colour
/// variants: type numbers 97–111, 150 and 53 of the game's type-name table
/// (D: `AnnoGame.dll` figure colour loader `0x10011570`, test at
/// `0x100115b7`; names at `0x10024cb8`; number 112 has no name). 69 of the
/// 368 figure entries; ships (`SHIP`, 113) are not among them.
pub const PLAYER_COLOURED_TYPES: [&str; 17] = [
    "ERKUNDER",
    "TRAGTIER",
    "SCHWERT",
    "BOGEN",
    "ARMBRUST",
    "LANZE",
    "MUSKETIER",
    "KAVALARIE",
    "GEWEHR",
    "MEDICUS",
    "BEDIENUNG",
    "KANONIER",
    "MOERSER",
    "BELAGERTURM",
    "KATAPULT",
    "FLAGGE",
    "ESEL",
];

/// Whether `entry` is drawn in its owner's colour ([`PLAYER_COLOURED_TYPES`]).
pub fn player_coloured(entry: &Entry<'_>) -> bool {
    entry
        .record_str("BEWOHNTYPE")
        .is_some_and(|t| PLAYER_COLOURED_TYPES.contains(&t.as_str()))
}

/// A palette colour in the variant for `player` (D: `AnnoDisplay.dll`
/// `0x10007e60` and `0x10008980`, identical tests). A colour whose green
/// dominates, `G² > trunc(1.5 · (R² + B²))`, becomes the player colour
/// scaled by its brightness: `v = trunc(0.3333 · (R, G, B summed term by
/// term))`, `w = min(trunc(3 · v), 255)`, each channel `player · w / 255`
/// (integer division). Other colours stay. `0.3333` is the original's
/// `f32` constant; the x87 products and sums of 8-bit values with it are
/// exact in `f64`, so this reproduces the original bit for bit.
pub fn recolour(rgb: [u8; 3], player: [u8; 3]) -> [u8; 3] {
    let [r, g, b] = rgb.map(i64::from);
    // Values ≤ 3 · 255², exact in f64; truncation toward zero as `__ftol`.
    #[allow(clippy::cast_possible_truncation, clippy::cast_precision_loss)]
    let t = ((r * r + b * b) as f64 * 1.5) as i64;
    if g * g <= t {
        return rgb;
    }
    let third = f64::from(0.3333_f32);
    #[allow(clippy::cast_possible_truncation, clippy::cast_precision_loss)]
    let v = (third * r as f64 + third * g as f64 + third * b as f64) as i64 & 0xFF;
    #[allow(clippy::cast_possible_truncation, clippy::cast_precision_loss)]
    let w = ((v as f64 * 3.0) as i64).min(255);
    player.map(|c| u8::try_from(i64::from(c) * w / 255).expect("≤ 255"))
}

/// [`decode`] with the palette colours in `player`'s variant ([`recolour`]);
/// `None` draws the palette as it is.
pub fn decode_coloured(
    entry: &Entry<'_>,
    frame: &Frame,
    data: &[u8],
    player: Option<[u8; 3]>,
) -> Result<Rgba, Error> {
    let format = frame.format().ok_or(Error::UnknownFormat)?;
    let px = figuren::decode_frame(
        format,
        data,
        usize::from(frame.width),
        usize::from(frame.height),
    )
    .map_err(Error::Decode)?;
    let palette = if format == FrameFormat::Alpha {
        None
    } else {
        let table = entry
            .palettes
            .get(usize::from(frame.palette))
            .ok_or(Error::MissingPalette(frame.palette))?;
        Some(&table.palette)
    };
    let pixels = px
        .iter()
        .flat_map(|p| match (p, palette) {
            (None, _) => [0, 0, 0, 0],
            (Some(_), None) => [0, 0, 0, 255],
            (Some(p), Some(pal)) => {
                let rgb = pal.rgb(p.index);
                let [r, g, b] = player.map_or(rgb, |c| recolour(rgb, c));
                [r, g, b, pixel_alpha(format, p.level)]
            }
        })
        .collect();
    Ok(Rgba {
        width: frame.width,
        height: frame.height,
        pixels,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recolour_rule() {
        let blue = ORIGINAL_PLAYER_COLOURS[0];
        // Pure green 255: v = trunc(0.3333 · 255) = 84, w = 252.
        assert_eq!(recolour([0, 255, 0], blue), [0, 184, 252]);
        // A darker marker shade scales the player colour down.
        assert_eq!(recolour([0, 156, 0], blue), [0, 112, 153]);
        // Grass-like green whose green does not dominate enough stays.
        assert_eq!(recolour([100, 150, 80], blue), [100, 150, 80]);
        // Boundary: G² must exceed trunc(1.5 · (R² + B²)).
        assert_eq!(recolour([10, 12, 0], blue), [10, 12, 0]); // 144 > 150? no
        assert_ne!(recolour([10, 13, 0], blue), [10, 13, 0]); // 169 > 150
        assert_eq!(recolour([255, 0, 255], blue), [255, 0, 255]);
        assert_eq!(recolour([0, 0, 0], blue), [0, 0, 0]);
    }
    use anno_formats::figuren::FigurenHeader;

    fn rec(name: &str, payload: &[u8]) -> Vec<u8> {
        let mut out = vec![0; 12];
        out[..name.len()].copy_from_slice(name.as_bytes());
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    fn frame(format: &[u8], w: u16, h: u16, offset: u32, length: u32) -> Vec<u8> {
        let mut raw = [w, h, 0, 8]
            .iter()
            .flat_map(|v| v.to_le_bytes())
            .collect::<Vec<_>>();
        let mut fmt = [0u8; 12];
        fmt[..format.len()].copy_from_slice(format);
        raw.extend_from_slice(&fmt);
        raw.extend([0u16, 0].iter().flat_map(|v| v.to_le_bytes()));
        raw.extend([offset, length, 0].iter().flat_map(|v| v.to_le_bytes()));
        raw
    }

    #[test]
    fn sprite_and_shadow_to_rgba() {
        // Colour table 0: index i = [B=i, G=0x10, R=0xff-i, 0].
        let mut table = vec![0; 8];
        table.extend_from_slice(b"BODY\0\0\x00\x04");
        table.extend((0..=255u8).flat_map(|i| [i, 0x10, 0xff - i, 0]));
        table.extend_from_slice(b"NAME\0\0\x02\0a\0");
        // Volume: Z frame (skip 1, one [z, index] pair), ALPHA frame (cover 1).
        let volume = [1, 1, 0x77, 0x20, 0xFF, 0, 1, 0xFF];
        // TEXTURES: group 0, 2 frames.
        let mut tex = [0u32.to_le_bytes(), 2u32.to_le_bytes()].concat();
        tex.extend(frame(b"BSH_RGBZ", 2, 1, 0, 5));
        tex.extend(frame(b"BSH_ALPHA", 2, 1, 5, 3));
        let mut set = vec![0; 16];
        set.extend(rec("TEXTURES", &tex));
        let mut entry = [0u32.to_le_bytes(), 5u32.to_le_bytes()].concat();
        entry.extend(rec("COLORS", &rec("ENTRY", &table)));
        entry.extend(rec("HAEUSER", &rec("ENTRY", &set)));
        let mut buf = b"ANNO\0\0\0\0\0\0\0\0\0\0\0\0".to_vec();
        buf.extend(rec("FIGUREN", &rec("ENTRY", &entry)));
        let h = FigurenHeader::parse(&buf).unwrap();
        let e = &h.entries[0];
        let frames = &e.gfx[0].frame_sets[0].frames;
        let sprite = to_rgba(e, &frames[0], &volume).unwrap();
        assert_eq!(sprite.pixels, [0, 0, 0, 0, 0xdf, 0x10, 0x20, 255]);
        let shadow = to_rgba(e, &frames[1], &volume).unwrap();
        assert_eq!(shadow.pixels, [0, 0, 0, 255, 0, 0, 0, 0]);
        assert_eq!(
            to_rgba(e, &frames[0], &volume[..3]),
            Err(Error::OutsideVolume)
        );
    }
}
