//! `.tex` textures as 8-bit RGBA images.
//!
//! Mapping (spec: `docs/formats/tex.md`, `docs/formats/palette.md`): an opaque
//! pixel with palette index `i` becomes `[R, G, B, 255]` from the texture's
//! palette; a transparent pixel (gap in `BSH_RGB` RLE) becomes `[0, 0, 0, 0]`.
//! `HEAD.color_key` is **not** applied: whether the engine treats that colour
//! as transparent is open (OQ-021).

use std::fmt;

use anno_formats::tex::{self, TexFile, Texture};

/// A decoded image, rows top to bottom, 4 bytes per pixel (`R, G, B, A`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rgba {
    pub width: u16,
    pub height: u16,
    pub pixels: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// `HEAD.palette_id` names no `COLOR` in the file.
    MissingPalette(u16),
    Decode(tex::Error),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::MissingPalette(id) => write!(f, "palette {id} not in file"),
            Error::Decode(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for Error {}

/// Decodes `texture` with its palette from `file`.
pub fn to_rgba(file: &TexFile<'_>, texture: &Texture<'_>) -> Result<Rgba, Error> {
    let palette = file
        .palette(texture.head.palette_id)
        .ok_or(Error::MissingPalette(texture.head.palette_id))?;
    let indices = texture.decode().map_err(Error::Decode)?;
    let pixels = indices
        .iter()
        .flat_map(|p| match p {
            Some(i) => {
                let [r, g, b] = palette.rgb(*i);
                [r, g, b, 255]
            }
            None => [0, 0, 0, 0],
        })
        .collect();
    Ok(Rgba {
        width: texture.head.width,
        height: texture.head.height,
        pixels,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn chunk(name: &str, payload: &[u8]) -> Vec<u8> {
        let mut out = vec![0; 12];
        out[..name.len()].copy_from_slice(name.as_bytes());
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    #[test]
    fn rle_texture_to_rgba() {
        // Palette 3: index i = [B=i, G=0x10, R=0xff-i, 0].
        let pal: Vec<u8> = (0..=255u8).flat_map(|i| [i, 0x10, 0xff - i, 0]).collect();
        let color = chunk(
            "COLOR",
            &[&[0, 0, 0, 0, 3, 0, 0, 0][..], &chunk("BODY", &pal)].concat(),
        );
        let mut head = vec![2, 0, 1, 0, 3, 0, 8, 0, 0, 0];
        head.extend_from_slice(b"BSH_RGB\0\0\0");
        head.extend_from_slice(&[0xff, 0, 0xff, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
        let body = [1, 1, 0x20, 0xFF]; // skip 1, one pixel of index 0x20
        let texture = chunk(
            "TEXTURE",
            &[
                &[0, 0, 0, 0, 9, 0, 0, 0][..],
                &chunk("HEAD", &head),
                &chunk("BODY", &body),
            ]
            .concat(),
        );
        let buf = chunk("ANNO", &[color, texture].concat());
        let file = TexFile::parse(&buf).unwrap();
        let img = to_rgba(&file, &file.textures[0]).unwrap();
        assert_eq!((img.width, img.height), (2, 1));
        assert_eq!(img.pixels, [0, 0, 0, 0, 0xdf, 0x10, 0x20, 255]);
    }
}
