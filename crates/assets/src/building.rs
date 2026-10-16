//! `Anlagen.dat` frames (buildings, objects) as 8-bit RGBA images.
//!
//! Mapping (spec: `docs/formats/anlagen.md`): as `crate::figure`, except
//! that the colour table is the global `Anlagen.dat` table with id
//! `entry.palette_ids[frame.palette]`, and pixels a row writes past the
//! frame's width are dropped (`decode_frame_clipped`, OQ-025).

use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, Entry, Frame, FrameFormat};

use crate::figure::Error;
use crate::texture::Rgba;

/// Decodes `frame` of `entry` from the volume of zoom group `group`.
pub fn to_rgba(
    file: &AnlagenFile<'_>,
    entry: &Entry<'_>,
    group: u32,
    frame: &Frame,
) -> Result<Rgba, Error> {
    let format = frame.format().ok_or(Error::UnknownFormat)?;
    let volume = file.volume(group).ok_or(Error::OutsideVolume)?;
    let start = usize::try_from(frame.offset).map_err(|_| Error::OutsideVolume)?;
    let len = usize::try_from(frame.length).map_err(|_| Error::OutsideVolume)?;
    let data = start
        .checked_add(len)
        .and_then(|end| volume.data.get(start..end))
        .ok_or(Error::OutsideVolume)?;
    let (px, _) = figuren::decode_frame_clipped(
        format,
        data,
        usize::from(frame.width),
        usize::from(frame.height),
    )
    .map_err(Error::Decode)?;
    let palette = if format == FrameFormat::Alpha {
        None
    } else {
        let id = entry
            .palette_ids
            .get(usize::from(frame.palette))
            .copied()
            .ok_or(Error::MissingPalette(frame.palette))?;
        let table = file
            .colors
            .iter()
            .find(|t| t.palette.id == id && id != 0)
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
