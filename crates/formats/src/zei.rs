//! Bitmap fonts (`*.zei`): root `CHARSET` with `HEAD` and `BODY`.
//!
//! Spec and evidence: `docs/formats/zei.md`. Summary (P = checked over all
//! 11 `.zei` of GOG 2.0.0.5, `crates/formats/tests/zei_corpus.rs`):
//!
//! ```text
//! HEAD (2,064 bytes)
//! 0x00  u16      size_a     11–31, meaning open (OQ-031)
//! 0x02  u16      size_b     13–31, meaning open (OQ-031)
//! 0x04  12 bytes            0
//! 0x10  256 × { u32 width, u32 length }   per character code 0–255
//! BODY  the glyphs of all characters with length > 0, in code order,
//!       each `length` bytes
//! ```
//!
//! A glyph is a row RLE as in `.tex` `BSH_RGB` ([`crate::tex::decode_rle`]):
//! `skip, count, count × level` runs, `0xFE` ends a row, `0xFF` the glyph.
//! Levels are 1–3 (coverage steps; meaning of the values open, OQ-031).
//! All glyphs of a font have the same number of rows; it is not stored in
//! `HEAD` ([`Glyph::decode`] counts them).

use std::fmt;

use crate::chunk::{self, Chunk};
use crate::tex::{RLE_END_OF_IMAGE, RLE_END_OF_ROW};

/// Size of the `HEAD` payload.
pub const HEAD_LEN: usize = 16 + 256 * 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// Not `CHARSET` with exactly `HEAD` and `BODY`.
    NoCharset,
    /// `HEAD` is not [`HEAD_LEN`] bytes.
    BadHead,
    /// The glyph lengths do not add up to the `BODY` size.
    BadBody,
    /// A glyph's RLE data is malformed.
    BadGlyph { code: u8 },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoCharset => write!(f, "no CHARSET with HEAD and BODY"),
            Error::BadHead => write!(f, "HEAD is not {HEAD_LEN} bytes"),
            Error::BadBody => write!(f, "glyph lengths do not match BODY"),
            Error::BadGlyph { code } => write!(f, "glyph {code} is malformed"),
        }
    }
}

impl std::error::Error for Error {}

fn u16_at(b: &[u8], o: usize) -> u16 {
    u16::from_le_bytes([b[o], b[o + 1]])
}

fn u32_at(b: &[u8], o: usize) -> u32 {
    u32::from_le_bytes([b[o], b[o + 1], b[o + 2], b[o + 3]])
}

/// One character: its advance width and RLE data (empty for codes without
/// a glyph).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Glyph<'a> {
    pub code: u8,
    pub width: u32,
    pub data: &'a [u8],
}

/// A decoded glyph: `width × height` levels, row-major, `None` = not covered.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GlyphImage {
    /// The glyph width, or more if a row's pixels reach further (never in
    /// the corpus; in `MainSmall8.zei` and `MainMedium8.zei` every glyph has
    /// a row skip that ends one column past the width, without pixels).
    pub width: usize,
    pub height: usize,
    pub levels: Vec<Option<u8>>,
}

impl Glyph<'_> {
    /// Decodes the RLE data; the height is the number of rows.
    pub fn decode(&self) -> Result<GlyphImage, Error> {
        let bad = || Error::BadGlyph { code: self.code };
        // Pass 1: rows and extent.
        let mut rows: Vec<Vec<(usize, &[u8])>> = vec![Vec::new()];
        let (mut i, mut x, mut extent) = (0, 0, usize::try_from(self.width).map_err(|_| bad())?);
        loop {
            let b = *self.data.get(i).ok_or_else(bad)?;
            i += 1;
            if b == RLE_END_OF_ROW || b == RLE_END_OF_IMAGE {
                x = 0;
                if b == RLE_END_OF_IMAGE {
                    break;
                }
                rows.push(Vec::new());
                continue;
            }
            let count = usize::from(*self.data.get(i).ok_or_else(bad)?);
            i += 1;
            let px = self.data.get(i..i + count).ok_or_else(bad)?;
            i += count;
            x += usize::from(b);
            rows.last_mut().expect("one row").push((x, px));
            x += count;
            // Only pixels widen the image; a skip may end past the width.
            if count > 0 {
                extent = extent.max(x);
            }
        }
        if i != self.data.len() {
            return Err(bad());
        }
        let (width, height) = (extent, rows.len());
        let mut levels = vec![None; width * height];
        for (y, runs) in rows.iter().enumerate() {
            for &(x0, px) in runs {
                for (k, &v) in px.iter().enumerate() {
                    levels[y * width + x0 + k] = Some(v);
                }
            }
        }
        Ok(GlyphImage {
            width,
            height,
            levels,
        })
    }
}

/// A parsed font.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CharSet<'a> {
    /// The 12-byte name fields of `CHARSET`, `HEAD`, `BODY` as stored. In
    /// the two `Textures/Scape*.zei` the bytes after the NUL are filled
    /// (`CHARSET\0` + `e0 e1 e2 e3`, `HEAD`/`BODY` + 4 × 0 + `u32 16`); the
    /// other nine are zero-padded (OQ-031).
    pub name_fields: [[u8; chunk::NAME_LEN]; 3],
    pub size_a: u16,
    pub size_b: u16,
    /// `HEAD` bytes 0x04–0x0F (zero in the corpus).
    pub reserved: [u8; 12],
    /// All 256 character codes in order.
    pub glyphs: Vec<Glyph<'a>>,
}

impl<'a> CharSet<'a> {
    pub fn parse(buf: &'a [u8]) -> Result<Self, Error> {
        let file = chunk::parse_file(buf).map_err(|_| Error::NoCharset)?;
        let root = file.chunks.first().ok_or(Error::NoCharset)?;
        let kids: &[Chunk<'a>] = root.children.as_deref().ok_or(Error::NoCharset)?;
        match (root.name(), kids) {
            ("CHARSET", [head, body]) if head.name() == "HEAD" && body.name() == "BODY" => {
                let mut cs = Self::from_parts(head.payload, body.payload)?;
                cs.name_fields = [*root.name_field, *head.name_field, *body.name_field];
                Ok(cs)
            }
            _ => Err(Error::NoCharset),
        }
    }

    fn from_parts(head: &'a [u8], body: &'a [u8]) -> Result<Self, Error> {
        if head.len() != HEAD_LEN {
            return Err(Error::BadHead);
        }
        let mut glyphs = Vec::with_capacity(256);
        let mut pos = 0usize;
        for code in 0..=255u8 {
            let o = 16 + 8 * usize::from(code);
            let width = u32_at(head, o);
            let len = usize::try_from(u32_at(head, o + 4)).map_err(|_| Error::BadBody)?;
            let data = body.get(pos..pos + len).ok_or(Error::BadBody)?;
            pos += len;
            glyphs.push(Glyph { code, width, data });
        }
        if pos != body.len() {
            return Err(Error::BadBody);
        }
        let name = |n: &str| chunk::name_field(n).expect("valid name");
        Ok(Self {
            name_fields: [name("CHARSET"), name("HEAD"), name("BODY")],
            size_a: u16_at(head, 0),
            size_b: u16_at(head, 2),
            reserved: head[4..16].try_into().expect("12 bytes"),
            glyphs,
        })
    }

    /// The `HEAD` payload rebuilt from the fields.
    pub fn head_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(HEAD_LEN);
        out.extend_from_slice(&self.size_a.to_le_bytes());
        out.extend_from_slice(&self.size_b.to_le_bytes());
        out.extend_from_slice(&self.reserved);
        for g in &self.glyphs {
            out.extend_from_slice(&g.width.to_le_bytes());
            let len = u32::try_from(g.data.len()).expect("glyph fits u32");
            out.extend_from_slice(&len.to_le_bytes());
        }
        out
    }

    /// The `BODY` payload rebuilt from the glyphs.
    pub fn body_bytes(&self) -> Vec<u8> {
        self.glyphs
            .iter()
            .flat_map(|g| g.data.iter().copied())
            .collect()
    }

    /// The whole file: `CHARSET { HEAD, BODY }` with the stored name fields.
    pub fn to_bytes(&self) -> Vec<u8> {
        let part = |field: &[u8; chunk::NAME_LEN], payload: &[u8]| {
            let mut out = field.to_vec();
            out.extend_from_slice(&u32::try_from(payload.len()).expect("fits").to_le_bytes());
            out.extend_from_slice(payload);
            out
        };
        let [root, head, body] = &self.name_fields;
        part(
            root,
            &[
                part(head, &self.head_bytes()),
                part(body, &self.body_bytes()),
            ]
            .concat(),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn font(glyph: &[u8]) -> Vec<u8> {
        let mut head = vec![0u8; HEAD_LEN];
        head[0] = 11;
        head[2] = 3;
        let o = 16 + 8 * usize::from(b'A');
        head[o] = 3;
        head[o + 4] = u8::try_from(glyph.len()).unwrap();
        CharSet::from_parts(&head, glyph).unwrap().to_bytes()
    }

    #[test]
    fn parse_decode_round_trip() {
        // Row 0: skip 1, two pixels; row 1: empty; row 2: one pixel at 0.
        let glyph = [1, 2, 1, 2, 0xFE, 0xFE, 0, 1, 3, 0xFF];
        let buf = font(&glyph);
        let cs = CharSet::parse(&buf).unwrap();
        assert_eq!((cs.size_a, cs.size_b), (11, 3));
        assert_eq!(cs.to_bytes(), buf);
        let a = &cs.glyphs[usize::from(b'A')];
        assert_eq!((a.width, a.data), (3, &glyph[..]));
        let img = a.decode().unwrap();
        assert_eq!((img.width, img.height), (3, 3));
        assert_eq!(
            img.levels,
            [
                None,
                Some(1),
                Some(2),
                None,
                None,
                None,
                Some(3),
                None,
                None
            ]
        );
        // A skip past the width does not widen the image; pixels do.
        let skip = Glyph {
            code: 32,
            width: 2,
            data: &[3, 0, 0xFF],
        };
        assert_eq!(skip.decode().unwrap().width, 2);
        let wide = Glyph {
            code: 33,
            width: 2,
            data: &[2, 1, 7, 0xFF],
        };
        assert_eq!(wide.decode().unwrap().width, 3);
        let broken = Glyph {
            code: 1,
            width: 2,
            data: &[0, 5, 1],
        };
        assert_eq!(broken.decode(), Err(Error::BadGlyph { code: 1 }));
        assert_eq!(CharSet::parse(&buf[..40]), Err(Error::NoCharset));
    }
}
