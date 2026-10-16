//! Textures (`.tex`): palettes (`COLOR`) and 8-bit indexed images (`TEXTURE`).
//!
//! Spec and evidence: `docs/formats/tex.md`, `docs/formats/palette.md`.
//! Summary (all P = checked over every `.tex` of GOG 2.0.0.5):
//!
//! - Root `ANNO` holds `COLOR` and `TEXTURE` chunks, or is empty (338 files).
//! - Both start with an 8-byte prefix `u32 0, u32 id` followed by sub-chunks.
//!   `COLOR` = `BODY` with 256 × `[B, G, R, 0]`. `TEXTURE` = `HEAD` (36 bytes),
//!   `BODY` (pixels), optional `NAME` (source file name, NUL-terminated).
//! - `HEAD.palette_id` always names the `id` of a `COLOR` in the same file.
//! - Pixel formats: `BSH_RGB`/`BSH_RGBH` = row RLE, `TEX_RGB` = raw `w × h` bytes.

use std::fmt;

use crate::chunk::{self, Chunk};

/// Number of entries in a palette.
pub const PALETTE_LEN: usize = 256;
/// Size of a `TEXTURE/HEAD` payload.
pub const HEAD_LEN: usize = 36;
/// RLE marker: end of the current row.
pub const RLE_END_OF_ROW: u8 = 0xFE;
/// RLE marker: end of the image (also ends the last row).
pub const RLE_END_OF_IMAGE: u8 = 0xFF;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// The root chunk is missing or not a container.
    NoRoot,
    /// A `COLOR`/`TEXTURE` payload is shorter than its 8-byte prefix or its
    /// remainder is not a chunk sequence.
    BadEntry { name: &'static str, offset: usize },
    /// A required sub-chunk is missing or has the wrong size.
    BadSubChunk { what: &'static str, offset: usize },
    /// `HEAD.format` is not one of the known pixel formats.
    UnknownFormat([u8; 10]),
    /// Pixel data does not match the declared size.
    BadPixels(&'static str),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoRoot => write!(f, "no root container chunk"),
            Error::BadEntry { name, offset } => write!(f, "malformed {name} at {offset}"),
            Error::BadSubChunk { what, offset } => write!(f, "bad {what} in entry at {offset}"),
            Error::UnknownFormat(raw) => write!(f, "unknown pixel format {raw:02x?}"),
            Error::BadPixels(why) => write!(f, "bad pixel data: {why}"),
        }
    }
}

impl std::error::Error for Error {}

/// A 256-colour palette from a `COLOR` chunk. Entries are stored as in the
/// file: `[blue, green, red, reserved]` (Windows `RGBQUAD` order).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Palette<'a> {
    /// The entry id from the 8-byte prefix.
    pub id: u32,
    /// Raw prefix (`u32 unknown, u32 id`), kept for writing back. The first
    /// u32 is 0 except for 10 palettes in `fenster(8).tex`, where it is 2 (OQ-021).
    pub prefix: &'a [u8; 8],
    pub entries: &'a [[u8; 4]; PALETTE_LEN],
}

impl Palette<'_> {
    /// Entry `index` as `[red, green, blue]`.
    pub fn rgb(&self, index: u8) -> [u8; 3] {
        let [b, g, r, _] = self.entries[usize::from(index)];
        [r, g, b]
    }
}

/// Pixel encodings named by `HEAD.format`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PixelFormat {
    /// `BSH_RGB`: row RLE with transparent skips (5,028 textures).
    BshRgb,
    /// `BSH_RGBH`: same RLE (1 texture, `Textures/debug.tex`); meaning of `H` unknown.
    BshRgbH,
    /// `TEX_RGB`: raw `width × height` indices, no transparency (147 ground tiles).
    TexRgb,
}

impl PixelFormat {
    fn from_raw(raw: &[u8; 10]) -> Option<Self> {
        let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
        match &raw[..len] {
            b"BSH_RGB" => Some(Self::BshRgb),
            b"BSH_RGBH" => Some(Self::BshRgbH),
            b"TEX_RGB" => Some(Self::TexRgb),
            _ => None,
        }
    }

    pub fn is_rle(self) -> bool {
        matches!(self, Self::BshRgb | Self::BshRgbH)
    }
}

/// The 36-byte `TEXTURE/HEAD` payload. All fields little-endian.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Head {
    /// 0x00
    pub width: u16,
    /// 0x02
    pub height: u16,
    /// 0x04: `id` of the `COLOR` chunk used by this texture.
    pub palette_id: u16,
    /// 0x06: always 8 in the corpus; plausibly bits per pixel (unconfirmed, OQ-021).
    pub unknown_06: u16,
    /// 0x08: always 0 in the corpus.
    pub unknown_08: u16,
    /// 0x0A: pixel format name, NUL-padded (`BSH_RGB`, `BSH_RGBH`, `TEX_RGB`).
    pub format_raw: [u8; 10],
    /// 0x14: a colour as `[B, G, R, 0]`; magenta `ff 00 ff 00` in 5,169 of
    /// 5,176 textures, plausibly a transparency key (unconfirmed, OQ-021).
    pub color_key: [u8; 4],
    /// 0x18, 0x1A: two u16 often equal to `(width / 2, height)`, but out of
    /// bounds for 1,207 textures; meaning unknown (OQ-021).
    pub unknown_18: u16,
    pub unknown_1a: u16,
    /// 0x1C: always zero in the corpus.
    pub unknown_1c: [u8; 8],
}

impl Head {
    pub fn parse(raw: &[u8]) -> Option<Self> {
        let raw: &[u8; HEAD_LEN] = raw.try_into().ok()?;
        let u16_at = |o: usize| u16::from_le_bytes([raw[o], raw[o + 1]]);
        let mut format_raw = [0; 10];
        format_raw.copy_from_slice(&raw[0x0A..0x14]);
        let mut color_key = [0; 4];
        color_key.copy_from_slice(&raw[0x14..0x18]);
        let mut unknown_1c = [0; 8];
        unknown_1c.copy_from_slice(&raw[0x1C..0x24]);
        Some(Self {
            width: u16_at(0x00),
            height: u16_at(0x02),
            palette_id: u16_at(0x04),
            unknown_06: u16_at(0x06),
            unknown_08: u16_at(0x08),
            format_raw,
            color_key,
            unknown_18: u16_at(0x18),
            unknown_1a: u16_at(0x1A),
            unknown_1c,
        })
    }

    pub fn to_bytes(&self) -> [u8; HEAD_LEN] {
        let mut out = [0; HEAD_LEN];
        out[0x00..0x02].copy_from_slice(&self.width.to_le_bytes());
        out[0x02..0x04].copy_from_slice(&self.height.to_le_bytes());
        out[0x04..0x06].copy_from_slice(&self.palette_id.to_le_bytes());
        out[0x06..0x08].copy_from_slice(&self.unknown_06.to_le_bytes());
        out[0x08..0x0A].copy_from_slice(&self.unknown_08.to_le_bytes());
        out[0x0A..0x14].copy_from_slice(&self.format_raw);
        out[0x14..0x18].copy_from_slice(&self.color_key);
        out[0x18..0x1A].copy_from_slice(&self.unknown_18.to_le_bytes());
        out[0x1A..0x1C].copy_from_slice(&self.unknown_1a.to_le_bytes());
        out[0x1C..0x24].copy_from_slice(&self.unknown_1c);
        out
    }

    pub fn format(&self) -> Option<PixelFormat> {
        PixelFormat::from_raw(&self.format_raw)
    }
}

/// One `TEXTURE` chunk.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Texture<'a> {
    /// The entry id from the 8-byte prefix.
    pub id: u32,
    pub prefix: &'a [u8; 8],
    pub head: Head,
    /// Encoded pixel data (`BODY` payload).
    pub body: &'a [u8],
    /// Source file name from the optional `NAME` chunk, raw (Windows-1252,
    /// NUL-terminated), e.g. `Wiese011.bmp`.
    pub name: Option<&'a [u8]>,
    /// The sub-chunks as parsed, for byte-exact writing.
    pub chunks: Vec<Chunk<'a>>,
}

impl Texture<'_> {
    /// `NAME` decoded as Windows-1252 up to the first NUL. Bytes 0x80–0x9F
    /// are mapped as Latin-1, which matches Windows-1252 for the umlauts seen.
    pub fn name_lossy(&self) -> Option<String> {
        let raw = self.name?;
        let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
        Some(raw[..len].iter().map(|&b| char::from(b)).collect())
    }

    /// Decodes to `width × height` palette indices; `None` marks transparency.
    pub fn decode(&self) -> Result<Vec<Option<u8>>, Error> {
        let (w, h) = (usize::from(self.head.width), usize::from(self.head.height));
        match self.head.format() {
            Some(f) if f.is_rle() => decode_rle(self.body, w, h),
            Some(_) => {
                if self.body.len() != w * h {
                    return Err(Error::BadPixels("raw size is not width * height"));
                }
                Ok(self.body.iter().map(|&b| Some(b)).collect())
            }
            None => Err(Error::UnknownFormat(self.head.format_raw)),
        }
    }
}

/// A parsed `.tex` file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TexFile<'a> {
    pub palettes: Vec<Palette<'a>>,
    pub textures: Vec<Texture<'a>>,
}

impl<'a> TexFile<'a> {
    pub fn parse(buf: &'a [u8]) -> Result<Self, Error> {
        let file = chunk::parse_file(buf).map_err(|_| Error::NoRoot)?;
        let root = file.chunks.first().ok_or(Error::NoRoot)?;
        let mut out = TexFile {
            palettes: Vec::new(),
            textures: Vec::new(),
        };
        if root.payload.is_empty() {
            return Ok(out);
        }
        for entry in root.children.as_deref().ok_or(Error::NoRoot)? {
            match entry.name() {
                "COLOR" => out.palettes.push(parse_color(entry)?),
                "TEXTURE" => out.textures.push(parse_texture(entry)?),
                _ => {
                    return Err(Error::BadEntry {
                        name: "unknown",
                        offset: entry.offset,
                    });
                }
            }
        }
        Ok(out)
    }

    pub fn palette(&self, id: u16) -> Option<&Palette<'a>> {
        self.palettes.iter().find(|p| p.id == u32::from(id))
    }
}

fn split_entry<'a>(
    entry: &Chunk<'a>,
    name: &'static str,
) -> Result<(u32, &'a [u8; 8], Vec<Chunk<'a>>), Error> {
    let bad = Error::BadEntry {
        name,
        offset: entry.offset,
    };
    let (prefix, rest) = entry.payload.split_first_chunk::<8>().ok_or(bad.clone())?;
    let id = u32::from_le_bytes([prefix[4], prefix[5], prefix[6], prefix[7]]);
    let chunks = chunk::parse_chunks(rest).ok_or(bad)?;
    Ok((id, prefix, chunks))
}

fn sub<'a, 'c>(chunks: &'c [Chunk<'a>], name: &str) -> Option<&'c Chunk<'a>> {
    chunks.iter().find(|c| c.name() == name)
}

fn parse_color<'a>(entry: &Chunk<'a>) -> Result<Palette<'a>, Error> {
    let (id, prefix, chunks) = split_entry(entry, "COLOR")?;
    let body = sub(&chunks, "BODY")
        .filter(|_| chunks.len() == 1)
        .ok_or(Error::BadSubChunk {
            what: "COLOR/BODY",
            offset: entry.offset,
        })?;
    let bad_size = Error::BadSubChunk {
        what: "COLOR/BODY size",
        offset: entry.offset,
    };
    let (entries, rest) = body.payload.as_chunks::<4>();
    if !rest.is_empty() {
        return Err(bad_size);
    }
    let entries = entries.try_into().map_err(|_| bad_size)?;
    Ok(Palette {
        id,
        prefix,
        entries,
    })
}

fn parse_texture<'a>(entry: &Chunk<'a>) -> Result<Texture<'a>, Error> {
    let (id, prefix, chunks) = split_entry(entry, "TEXTURE")?;
    let bad = |what| Error::BadSubChunk {
        what,
        offset: entry.offset,
    };
    let head = sub(&chunks, "HEAD")
        .and_then(|c| Head::parse(c.payload))
        .ok_or(bad("TEXTURE/HEAD"))?;
    let body = sub(&chunks, "BODY").ok_or(bad("TEXTURE/BODY"))?.payload;
    let name = sub(&chunks, "NAME").map(|c| c.payload);
    Ok(Texture {
        id,
        prefix,
        head,
        body,
        name,
        chunks,
    })
}

/// Decodes `BSH_RGB` row RLE.
///
/// Per row, a sequence of `(skip: u8, count: u8, count × index)` runs;
/// `skip` transparent pixels precede `count` opaque ones. `0xFE` ends a row,
/// `0xFF` ends the last row and the image. Pixels not covered are transparent.
pub fn decode_rle(data: &[u8], width: usize, height: usize) -> Result<Vec<Option<u8>>, Error> {
    let mut out = vec![None; width * height];
    let (mut i, mut x, mut y) = (0, 0, 0);
    loop {
        let b = *data.get(i).ok_or(Error::BadPixels("missing end marker"))?;
        i += 1;
        match b {
            RLE_END_OF_ROW | RLE_END_OF_IMAGE => {
                y += 1;
                x = 0;
                if b == RLE_END_OF_IMAGE {
                    break;
                }
            }
            skip => {
                let count = usize::from(*data.get(i).ok_or(Error::BadPixels("truncated run"))?);
                i += 1;
                let px = data
                    .get(i..i + count)
                    .ok_or(Error::BadPixels("truncated run"))?;
                i += count;
                x += usize::from(skip);
                if y >= height || x + count > width {
                    return Err(Error::BadPixels("run outside image"));
                }
                for (dst, &p) in out[y * width + x..][..count].iter_mut().zip(px) {
                    *dst = Some(p);
                }
                x += count;
            }
        }
    }
    if y != height {
        return Err(Error::BadPixels("row count differs from height"));
    }
    if i != data.len() {
        return Err(Error::BadPixels("bytes after end marker"));
    }
    Ok(out)
}

/// Largest `skip + count` of one RLE run (P: maximum over the corpus; the
/// markers `0xFE`/`0xFF` are never valid `skip` values).
pub const RLE_MAX_RUN: usize = 253;

/// Encodes `width × height` pixels (`None` = transparent) as `BSH_RGB` row
/// RLE, byte-identical to the original encoder on all 5,029 RLE textures of
/// the corpus (P, `tex_corpus`; rules in `docs/formats/tex.md`).
///
/// Per row: gaps longer than [`RLE_MAX_RUN`] emit `(253, 0)` runs first; an
/// opaque span is split so that each run's `skip + count` is at most 253,
/// continuation runs having `skip = 0`; a trailing gap is written as an
/// explicit `(gap, 0)` run, so every row reaches `width`.
pub fn encode_rle(px: &[Option<u8>], width: usize, height: usize) -> Vec<u8> {
    assert_eq!(
        px.len(),
        width * height,
        "pixel count is not width * height"
    );
    // Every value written below is at most RLE_MAX_RUN.
    let byte = |n: usize| u8::try_from(n).expect("run length fits in u8");
    let mut out = Vec::new();
    for y in 0..height {
        let row = &px[y * width..][..width];
        let mut x = 0;
        while x < width {
            let mut gap = row[x..].iter().take_while(|p| p.is_none()).count();
            x += gap;
            let mut n = row[x..].iter().take_while(|p| p.is_some()).count();
            while gap > RLE_MAX_RUN {
                out.extend([byte(RLE_MAX_RUN), 0]);
                gap -= RLE_MAX_RUN;
            }
            if n == 0 {
                out.extend([byte(gap), 0]);
                break;
            }
            let mut skip = gap;
            while n > 0 {
                let count = n.min(RLE_MAX_RUN - skip);
                out.extend([byte(skip), byte(count)]);
                out.extend(row[x..x + count].iter().flatten());
                x += count;
                n -= count;
                skip = 0;
            }
        }
        out.push(if y + 1 < height {
            RLE_END_OF_ROW
        } else {
            RLE_END_OF_IMAGE
        });
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rle_encode_two_rows() {
        let px = [
            None,
            Some(7),
            Some(8),
            None,
            Some(1),
            Some(2),
            Some(3),
            Some(4),
        ];
        let data = encode_rle(&px, 4, 2);
        assert_eq!(
            data,
            [
                1,
                2,
                7,
                8,
                1,
                0,
                RLE_END_OF_ROW,
                0,
                4,
                1,
                2,
                3,
                4,
                RLE_END_OF_IMAGE
            ]
        );
        assert_eq!(decode_rle(&data, 4, 2).unwrap(), px);
    }

    #[test]
    fn rle_encode_splits_long_gaps_and_spans() {
        // 300 transparent, then 600 opaque: gap split (253,0)(47,…), span
        // capped at skip + count = 253, continuations with skip 0.
        let mut px = vec![None; 300];
        px.extend((0..600).map(|i| Some((i % 200) as u8)));
        let data = encode_rle(&px, 900, 1);
        let runs: Vec<(u8, u8)> = {
            let (mut i, mut v) = (0, Vec::new());
            while data[i] != RLE_END_OF_IMAGE {
                v.push((data[i], data[i + 1]));
                i += 2 + usize::from(data[i + 1]);
            }
            v
        };
        assert_eq!(runs, [(253, 0), (47, 206), (0, 253), (0, 141)]);
        assert_eq!(decode_rle(&data, 900, 1).unwrap(), px);
        // Fully transparent row: explicit skip runs up to width.
        assert_eq!(
            encode_rle(&[None; 300], 300, 1),
            [253, 0, 47, 0, RLE_END_OF_IMAGE]
        );
    }

    #[test]
    fn rle_two_rows() {
        // Row 0: skip 1, 2 px; trailing skip 1. Row 1: 4 px.
        let data = [
            1,
            2,
            7,
            8,
            1,
            0,
            RLE_END_OF_ROW,
            0,
            4,
            1,
            2,
            3,
            4,
            RLE_END_OF_IMAGE,
        ];
        let px = decode_rle(&data, 4, 2).unwrap();
        assert_eq!(
            px,
            [
                None,
                Some(7),
                Some(8),
                None,
                Some(1),
                Some(2),
                Some(3),
                Some(4)
            ]
        );
    }

    #[test]
    fn rle_rejects_overflow_and_wrong_height() {
        assert!(decode_rle(&[0, 3, 1, 1, 1, RLE_END_OF_IMAGE], 2, 1).is_err());
        assert!(decode_rle(&[0, 1, 1, RLE_END_OF_IMAGE], 1, 2).is_err());
        assert!(decode_rle(&[0, 1, 1], 1, 1).is_err());
    }

    #[test]
    fn head_round_trip() {
        // `TEXTURE/HEAD` of the first texture in Textures/debug.tex (P).
        let raw: [u8; HEAD_LEN] = [
            0x24, 0, 0x23, 0, 0, 0, 8, 0, 0, 0, b'B', b'S', b'H', b'_', b'R', b'G', b'B', 0, 0, 0,
            0xff, 0, 0xff, 0, 0x12, 0, 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        ];
        let head = Head::parse(&raw).unwrap();
        assert_eq!((head.width, head.height, head.palette_id), (36, 35, 0));
        assert_eq!(head.format(), Some(PixelFormat::BshRgb));
        assert_eq!(head.to_bytes(), raw);
    }
}
