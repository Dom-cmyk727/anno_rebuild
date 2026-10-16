//! Figure frames (`Figuren*.dat`) as 8-bit RGBA images.
//!
//! Mapping (spec: `docs/formats/figuren.md`): a covered pixel of a sprite
//! frame with palette index `i` becomes `[R, G, B, 255]` from the entry's
//! colour table `Frame::palette`; a covered pixel of a `BSH_ALPHA` shadow
//! frame becomes `[0, 0, 0, 255]` (a coverage mask); uncovered pixels become
//! `[0, 0, 0, 0]`. The `z` byte and the 2-bit `level` of `A` formats are
//! **not** applied: their meaning is open (OQ-023).

use std::fmt;

use anno_formats::figuren::{self, Entry, Frame, FrameFormat};

use crate::texture::Rgba;

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
    let format = frame.format().ok_or(Error::UnknownFormat)?;
    let start = usize::try_from(frame.offset).map_err(|_| Error::OutsideVolume)?;
    let len = usize::try_from(frame.length).map_err(|_| Error::OutsideVolume)?;
    let data = start
        .checked_add(len)
        .and_then(|end| volume.get(start..end))
        .ok_or(Error::OutsideVolume)?;
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
                let [r, g, b] = pal.rgb(p.index);
                [r, g, b, 255]
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
