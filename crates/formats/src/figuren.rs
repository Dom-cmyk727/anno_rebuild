//! Figures and sprites: `FigurenHeader.dat` (index) and `Figuren<g><nn>.dat`
//! (pixel volumes).
//!
//! Spec and evidence: `docs/formats/figuren.md`. Summary (all P = checked
//! over GOG 2.0.0.5, `crates/formats/tests/figuren_corpus.rs`):
//!
//! - `FigurenHeader.dat` = root header `ANNO` (its size also counts the
//!   volume files), one `FIGUREN` chunk with 368 `ENTRY` chunks, then three
//!   24-byte volume headers (`TEXTURES`, `u32 group`, `u32 frame count`).
//! - `ENTRY` = 8-byte prefix (`u32 0`, `u32 id`) + records (generic chunks):
//!   `VERSION2`, `HEAD`, `BEWOHNTYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER`, …
//! - `COLORS` = `ENTRY` chunks with the `Anlagen.dat` colour-table layout.
//! - `HAEUSER` = `ENTRY` chunks (sprite sets): 16-byte prefix + `BODY2`,
//!   `ANIM`, `SOUND2`, `GFXDATA`, `TEXTURES` × 3 (zoom groups 0, 1, 2), and
//!   optionally `SHADOW` (8-byte prefix + the same records).
//! - `TEXTURES` = `u32 group`, `u32 n`, `n` × 36-byte [`Frame`].
//! - Frame pixels live at `offset .. offset + length` of the concatenated
//!   volume files of the frame's group, `Figuren<group><part:02>.dat`.

use std::fmt;

use crate::chunk::{self, Chunk, HEADER_LEN, NAME_LEN};
use crate::colors::{self, ColorTable};
use crate::tex::{RLE_END_OF_IMAGE, RLE_END_OF_ROW};

/// Size of one frame header in a `TEXTURES` record.
pub const FRAME_LEN: usize = 36;
/// Size of one volume header at the end of `FigurenHeader.dat`.
pub const VOLUME_HEADER_LEN: usize = 24;
/// Prefix of a top-level `ENTRY` (`u32 flag`, `u32 id`).
pub const ENTRY_PREFIX_LEN: usize = 8;
/// Prefix of a sprite set (`HAEUSER/ENTRY`), meaning unknown (OQ-023).
pub const GFX_PREFIX_LEN: usize = 16;
/// Prefix of a `SHADOW` sprite set, `03 00 00 00 00 00 00 00` in all 1,047.
pub const SHADOW_PREFIX_LEN: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// Root `ANNO` / `FIGUREN` header missing or malformed.
    NoRoot,
    /// A record does not have the layout above; `what` names the check.
    Bad { what: &'static str, offset: usize },
    /// Frame format name is not one of the known ones.
    UnknownFormat([u8; 12]),
    /// Pixel data does not match the frame header.
    BadPixels(&'static str),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoRoot => write!(f, "no ANNO/FIGUREN root"),
            Error::Bad { what, offset } => write!(f, "malformed {what} at {offset}"),
            Error::UnknownFormat(raw) => write!(f, "unknown frame format {raw:02x?}"),
            Error::BadPixels(why) => write!(f, "bad pixel data: {why}"),
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

/// Frame pixel encodings, named by [`Frame::format_raw`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameFormat {
    /// `BSH_RGBV`: 1 byte per pixel (palette index).
    RgbV,
    /// `BSH_RGBH`: 1 byte per pixel; meaning of `H` unknown (24 frames).
    RgbH,
    /// `BSH_RGBZ`: 2 bytes per pixel, `[z, index]`.
    RgbZ,
    /// `BSH_RGBAV`: 1 byte per pixel; count byte carries a 2-bit level.
    RgbAV,
    /// `BSH_RGBAZ`: 2 bytes per pixel `[z, index]`; count byte carries a level.
    RgbAZ,
    /// `BSH_ALPHA`: shadow coverage, runs without pixel bytes.
    Alpha,
    /// `BSH_RGBWZ` (`Anlagen.dat` only): 2 bytes per pixel `[z, index]`; `W` unknown.
    RgbWZ,
    /// `BSH_RGBZM` (`Anlagen.dat` only): 2 bytes per pixel `[z, index]`; `M` unknown.
    RgbZM,
    /// `BSH_RGBAH` (`Anlagen.dat` only): 1 byte per pixel; count byte carries a level.
    RgbAH,
}

impl FrameFormat {
    pub fn from_raw(raw: &[u8; 12]) -> Option<Self> {
        let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
        match &raw[..len] {
            b"BSH_RGBV" => Some(Self::RgbV),
            b"BSH_RGBH" => Some(Self::RgbH),
            b"BSH_RGBZ" => Some(Self::RgbZ),
            b"BSH_RGBAV" => Some(Self::RgbAV),
            b"BSH_RGBAZ" => Some(Self::RgbAZ),
            b"BSH_ALPHA" => Some(Self::Alpha),
            b"BSH_RGBWZ" => Some(Self::RgbWZ),
            b"BSH_RGBZM" => Some(Self::RgbZM),
            b"BSH_RGBAH" => Some(Self::RgbAH),
            _ => None,
        }
    }

    /// Bytes stored per opaque pixel.
    pub fn bytes_per_pixel(self) -> usize {
        match self {
            Self::Alpha => 0,
            Self::RgbV | Self::RgbH | Self::RgbAV | Self::RgbAH => 1,
            Self::RgbZ | Self::RgbAZ | Self::RgbWZ | Self::RgbZM => 2,
        }
    }

    /// True if the run count byte is `level << 6 | count`.
    pub fn has_level(self) -> bool {
        matches!(self, Self::RgbAV | Self::RgbAZ | Self::RgbAH)
    }
}

/// One 36-byte frame header from a `TEXTURES` record. Little-endian.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Frame {
    /// 0x00
    pub width: u16,
    /// 0x02
    pub height: u16,
    /// 0x04: index into the entry's `COLORS` tables (0..=2, always in range).
    pub palette: u16,
    /// 0x06: always 8; plausibly bits per pixel (as `.tex` HEAD 0x06).
    pub unknown_06: u16,
    /// 0x08: format name, NUL-padded, e.g. `BSH_RGBV`.
    pub format_raw: [u8; 12],
    /// 0x14, 0x16: anchor point; outside `width × height` for 783 frames (OQ-023).
    pub anchor_x: u16,
    pub anchor_y: u16,
    /// 0x18: byte offset in the group's concatenated volume files.
    pub offset: u32,
    /// 0x1C: byte length of the pixel data; 0 = empty frame.
    pub length: u32,
    /// 0x20: always 0.
    pub unknown_20: u32,
}

impl Frame {
    pub fn parse(raw: &[u8]) -> Option<Self> {
        let raw: &[u8; FRAME_LEN] = raw.try_into().ok()?;
        let mut format_raw = [0; 12];
        format_raw.copy_from_slice(&raw[0x08..0x14]);
        Some(Self {
            width: u16_at(raw, 0x00),
            height: u16_at(raw, 0x02),
            palette: u16_at(raw, 0x04),
            unknown_06: u16_at(raw, 0x06),
            format_raw,
            anchor_x: u16_at(raw, 0x14),
            anchor_y: u16_at(raw, 0x16),
            offset: u32_at(raw, 0x18),
            length: u32_at(raw, 0x1C),
            unknown_20: u32_at(raw, 0x20),
        })
    }

    pub fn to_bytes(&self) -> [u8; FRAME_LEN] {
        let mut out = [0; FRAME_LEN];
        out[0x00..0x02].copy_from_slice(&self.width.to_le_bytes());
        out[0x02..0x04].copy_from_slice(&self.height.to_le_bytes());
        out[0x04..0x06].copy_from_slice(&self.palette.to_le_bytes());
        out[0x06..0x08].copy_from_slice(&self.unknown_06.to_le_bytes());
        out[0x08..0x14].copy_from_slice(&self.format_raw);
        out[0x14..0x16].copy_from_slice(&self.anchor_x.to_le_bytes());
        out[0x16..0x18].copy_from_slice(&self.anchor_y.to_le_bytes());
        out[0x18..0x1C].copy_from_slice(&self.offset.to_le_bytes());
        out[0x1C..0x20].copy_from_slice(&self.length.to_le_bytes());
        out[0x20..0x24].copy_from_slice(&self.unknown_20.to_le_bytes());
        out
    }

    pub fn format(&self) -> Option<FrameFormat> {
        FrameFormat::from_raw(&self.format_raw)
    }

    /// The format name up to the first NUL.
    pub fn format_name(&self) -> String {
        let len = self.format_raw.iter().position(|&b| b == 0);
        let raw = &self.format_raw[..len.unwrap_or(12)];
        raw.iter().map(|&b| char::from(b)).collect()
    }
}

/// A `TEXTURES` record: all frames of one zoom group.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FrameSet {
    /// 0 = largest, 1 = half, 2 = quarter size; names the volume files.
    pub group: u32,
    pub frames: Vec<Frame>,
}

impl FrameSet {
    pub fn parse(payload: &[u8], offset: usize) -> Result<Self, Error> {
        let bad = |what| Error::Bad { what, offset };
        if payload.len() < 8 {
            return Err(bad("TEXTURES shorter than 8"));
        }
        let n = usize::try_from(u32_at(payload, 4)).map_err(|_| bad("TEXTURES count"))?;
        if payload.len() != 8 + n * FRAME_LEN {
            return Err(bad("TEXTURES size is not 8 + 36 n"));
        }
        let frames = payload[8..]
            .as_chunks::<FRAME_LEN>()
            .0
            .iter()
            .map(|f| Frame::parse(f).ok_or(bad("frame")))
            .collect::<Result<_, _>>()?;
        Ok(Self {
            group: u32_at(payload, 0),
            frames,
        })
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let n = u32::try_from(self.frames.len()).expect("frame count fits u32");
        let mut out = Vec::with_capacity(8 + self.frames.len() * FRAME_LEN);
        out.extend_from_slice(&self.group.to_le_bytes());
        out.extend_from_slice(&n.to_le_bytes());
        for f in &self.frames {
            out.extend_from_slice(&f.to_bytes());
        }
        out
    }
}

/// A sprite set (`HAEUSER/ENTRY`) or its `SHADOW`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GfxSet<'a> {
    /// Raw prefix: 16 bytes for a sprite set, 8 for a shadow (OQ-023).
    pub prefix: &'a [u8],
    /// All records in file order (`BODY2`, `ANIM`, `SOUND2`, `GFXDATA`,
    /// `TEXTURES`, `SHADOW`); payloads of `TEXTURES`/`SHADOW` are also
    /// available parsed below.
    pub records: Vec<Chunk<'a>>,
    /// `TEXTURES` records in file order (groups 0, 1, 2).
    pub frame_sets: Vec<FrameSet>,
    pub shadow: Option<Box<GfxSet<'a>>>,
}

impl<'a> GfxSet<'a> {
    fn parse(payload: &'a [u8], prefix_len: usize, offset: usize) -> Result<Self, Error> {
        let bad = |what| Error::Bad { what, offset };
        let (prefix, rest) = payload
            .split_at_checked(prefix_len)
            .ok_or(bad("sprite set prefix"))?;
        let records = chunk::parse_chunks(rest).ok_or(bad("sprite set records"))?;
        let mut frame_sets = Vec::new();
        let mut shadow = None;
        for r in &records {
            match r.name() {
                "TEXTURES" => frame_sets.push(FrameSet::parse(r.payload, offset)?),
                "SHADOW" => {
                    let s = GfxSet::parse(r.payload, SHADOW_PREFIX_LEN, offset)?;
                    if shadow.replace(Box::new(s)).is_some() {
                        return Err(bad("two SHADOW records"));
                    }
                }
                _ => {}
            }
        }
        Ok(Self {
            prefix,
            records,
            frame_sets,
            shadow,
        })
    }

    /// Re-encodes the payload from the parsed fields.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = self.prefix.to_vec();
        let mut sets = self.frame_sets.iter();
        for r in &self.records {
            let payload = match r.name() {
                "TEXTURES" => sets.next().map(FrameSet::to_bytes),
                "SHADOW" => self.shadow.as_ref().map(|s| s.to_bytes()),
                _ => None,
            };
            write_record(
                r.name_field,
                payload.as_deref().unwrap_or(r.payload),
                &mut out,
            );
        }
        out
    }
}

fn write_record(name_field: &[u8; NAME_LEN], payload: &[u8], out: &mut Vec<u8>) {
    let size = u32::try_from(payload.len()).expect("record fits u32");
    out.extend_from_slice(name_field);
    out.extend_from_slice(&size.to_le_bytes());
    out.extend_from_slice(payload);
}

/// One `FIGUREN/ENTRY`: a figure type.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry<'a> {
    /// Absolute offset of the `ENTRY` chunk header.
    pub offset: usize,
    /// Raw prefix: `u32 0`, `u32 id`.
    pub prefix: &'a [u8; ENTRY_PREFIX_LEN],
    pub id: u32,
    /// All records in file order.
    pub records: Vec<Chunk<'a>>,
    /// Parsed `COLORS` record when it holds colour tables (one per `ENTRY`
    /// chunk inside it): `FigurenHeader.dat`.
    pub palettes: Vec<ColorTable<'a>>,
    /// Parsed `COLORS` record when it is a list of `u32` colour-table ids
    /// (`Anlagen.dat` `ANLAGEN`: 1–3 ids of the global `COLORS` tables, 0 =
    /// unused slot). Frames select `palette_ids[frame.palette]`.
    pub palette_ids: Vec<u32>,
    /// Parsed `HAEUSER` record.
    pub gfx: Vec<GfxSet<'a>>,
}

impl<'a> Entry<'a> {
    /// Parses one `ENTRY` chunk (also used for `Anlagen.dat` `ANLAGEN`,
    /// which has the same layout).
    pub fn parse(c: &Chunk<'a>) -> Result<Self, Error> {
        let bad = |what| Error::Bad {
            what,
            offset: c.offset,
        };
        if c.name() != "ENTRY" {
            return Err(bad("FIGUREN child is not ENTRY"));
        }
        let (prefix, rest) = c
            .payload
            .split_first_chunk::<ENTRY_PREFIX_LEN>()
            .ok_or(bad("ENTRY prefix"))?;
        let records = chunk::parse_chunks(rest).ok_or(bad("ENTRY records"))?;
        let mut palettes = Vec::new();
        let mut palette_ids = Vec::new();
        let mut gfx = Vec::new();
        for r in &records {
            match r.name() {
                // Shorter than one chunk header: an id list (`ANLAGEN`).
                "COLORS" if !r.payload.is_empty() && r.payload.len() < HEADER_LEN => {
                    let (ids, rest) = r.payload.as_chunks::<4>();
                    if !rest.is_empty() {
                        return Err(bad("COLORS id list"));
                    }
                    palette_ids = ids.iter().map(|b| u32::from_le_bytes(*b)).collect();
                }
                "COLORS" => {
                    for t in sub_entries(r.payload).ok_or(bad("COLORS"))? {
                        let table = colors::parse_entry(t.payload, t.offset)
                            .map_err(|_| bad("COLORS/ENTRY"))?;
                        palettes.push(table);
                    }
                }
                "HAEUSER" => {
                    for s in sub_entries(r.payload).ok_or(bad("HAEUSER"))? {
                        gfx.push(GfxSet::parse(s.payload, GFX_PREFIX_LEN, c.offset)?);
                    }
                }
                _ => {}
            }
        }
        Ok(Self {
            offset: c.offset,
            prefix,
            id: u32_at(prefix, 4),
            records,
            palettes,
            palette_ids,
            gfx,
        })
    }

    /// Payload of the first record called `name`.
    pub fn record(&self, name: &str) -> Option<&'a [u8]> {
        self.records
            .iter()
            .find(|r| r.name() == name)
            .map(|r| r.payload)
    }

    /// A string record (`BEWOHNTYPE`, `ROUTETYPE`, `PATH`, …) up to its first
    /// NUL, Latin-1 decoded.
    pub fn record_str(&self, name: &str) -> Option<String> {
        let raw = self.record(name)?;
        let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
        Some(raw[..len].iter().map(|&b| char::from(b)).collect())
    }

    /// Re-encodes the `ENTRY` payload from the parsed fields.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = self.prefix.to_vec();
        for r in &self.records {
            let payload = match r.name() {
                "COLORS" if !self.palette_ids.is_empty() => Some(
                    self.palette_ids
                        .iter()
                        .flat_map(|i| i.to_le_bytes())
                        .collect(),
                ),
                "COLORS" => Some(entries_bytes(
                    self.palettes.iter().map(ColorTable::to_bytes),
                )),
                "HAEUSER" => Some(entries_bytes(self.gfx.iter().map(GfxSet::to_bytes))),
                _ => None,
            };
            write_record(
                r.name_field,
                payload.as_deref().unwrap_or(r.payload),
                &mut out,
            );
        }
        out
    }
}

/// `payload` as a sequence of `ENTRY` chunks; empty payload = no entries.
fn sub_entries(payload: &[u8]) -> Option<Vec<Chunk<'_>>> {
    if payload.is_empty() {
        return Some(Vec::new());
    }
    let kids = chunk::parse_chunks(payload)?;
    kids.iter().all(|k| k.name() == "ENTRY").then_some(kids)
}

fn entries_bytes(payloads: impl Iterator<Item = Vec<u8>>) -> Vec<u8> {
    let field = chunk::name_field("ENTRY").expect("valid name");
    let mut out = Vec::new();
    for p in payloads {
        write_record(&field, &p, &mut out);
    }
    out
}

/// One 24-byte volume header at the end of `FigurenHeader.dat`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VolumeHeader<'a> {
    /// Raw `TEXTURES` name field.
    pub name_field: &'a [u8; NAME_LEN],
    /// Chunk size field: 8 + total size of the group's volume files, i.e.
    /// the payload continues in those files after `group` and `frames`.
    pub size: u32,
    pub group: u32,
    /// Frames per group: 243,707 for each group (equals the frame headers).
    pub frames: u32,
}

/// A parsed `FigurenHeader.dat`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FigurenHeader<'a> {
    /// Raw 16-byte `ANNO` header; its size field counts the header file
    /// minus 16 plus all volume files, so it is kept, not recomputed.
    pub root_header: &'a [u8; HEADER_LEN],
    /// Raw 12-byte `FIGUREN` name field.
    pub figuren_name: &'a [u8; NAME_LEN],
    pub entries: Vec<Entry<'a>>,
    pub volumes: Vec<VolumeHeader<'a>>,
}

impl<'a> FigurenHeader<'a> {
    pub fn parse(buf: &'a [u8]) -> Result<Self, Error> {
        let root_header: &[u8; HEADER_LEN] = buf
            .first_chunk()
            .filter(|h| h.starts_with(b"ANNO\0"))
            .ok_or(Error::NoRoot)?;
        let figuren = buf
            .get(HEADER_LEN..2 * HEADER_LEN)
            .filter(|h| h.starts_with(b"FIGUREN\0"))
            .ok_or(Error::NoRoot)?;
        let figuren_name: &[u8; NAME_LEN] =
            figuren[..NAME_LEN].try_into().map_err(|_| Error::NoRoot)?;
        let size = usize::try_from(u32_at(figuren, NAME_LEN)).map_err(|_| Error::NoRoot)?;
        let body_end = (2 * HEADER_LEN).checked_add(size).ok_or(Error::NoRoot)?;
        let body = buf.get(2 * HEADER_LEN..body_end).ok_or(Error::NoRoot)?;
        let entries = chunk::parse_chunks(body)
            .ok_or(Error::NoRoot)?
            .into_iter()
            .map(|mut c| {
                c.offset += 2 * HEADER_LEN;
                Entry::parse(&c)
            })
            .collect::<Result<Vec<_>, _>>()?;
        let tail = &buf[body_end..];
        if !tail.len().is_multiple_of(VOLUME_HEADER_LEN) {
            return Err(Error::Bad {
                what: "volume headers",
                offset: body_end,
            });
        }
        let volumes = tail
            .as_chunks::<VOLUME_HEADER_LEN>()
            .0
            .iter()
            .enumerate()
            .map(|(i, v)| {
                let name_field: &[u8; NAME_LEN] = v[..NAME_LEN].try_into().expect("12 bytes");
                if !name_field.starts_with(b"TEXTURES\0") {
                    return Err(Error::Bad {
                        what: "volume header name",
                        offset: body_end + i * VOLUME_HEADER_LEN,
                    });
                }
                Ok(VolumeHeader {
                    name_field,
                    size: u32_at(v, 12),
                    group: u32_at(v, 16),
                    frames: u32_at(v, 20),
                })
            })
            .collect::<Result<_, _>>()?;
        Ok(Self {
            root_header,
            figuren_name,
            entries,
            volumes,
        })
    }

    /// Re-encodes the whole file from the parsed fields.
    pub fn to_bytes(&self) -> Vec<u8> {
        let field = chunk::name_field("ENTRY").expect("valid name");
        let mut body = Vec::new();
        for e in &self.entries {
            write_record(&field, &e.to_bytes(), &mut body);
        }
        let mut out = self.root_header.to_vec();
        write_record(self.figuren_name, &body, &mut out);
        for v in &self.volumes {
            out.extend_from_slice(v.name_field);
            for n in [v.size, v.group, v.frames] {
                out.extend_from_slice(&n.to_le_bytes());
            }
        }
        out
    }
}

/// File name of volume part `part` of zoom group `group`, e.g. `Figuren003.dat`.
pub fn volume_file_name(group: u32, part: u32) -> String {
    format!("Figuren{group}{part:02}.dat")
}

/// Maps `offset .. offset + length` of a group's concatenated volume to
/// `(part, start, end)` pieces, given the sizes of parts 0, 1, … in order.
/// `None` if the range runs past the last part.
pub fn locate(part_sizes: &[u64], offset: u64, length: u64) -> Option<Vec<(usize, u64, u64)>> {
    let (mut pos, end) = (offset, offset.checked_add(length)?);
    let mut base = 0;
    let mut out = Vec::new();
    for (part, &size) in part_sizes.iter().enumerate() {
        if pos >= end {
            break;
        }
        if pos < base + size {
            let stop = end.min(base + size);
            out.push((part, pos - base, stop - base));
            pos = stop;
        }
        base += size;
    }
    (pos >= end).then_some(out)
}

/// One decoded frame pixel. Fields a format does not store are 0; for
/// `BSH_ALPHA` every covered pixel is all zeros.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Pixel {
    pub index: u8,
    /// First byte of a `Z` pixel pair; meaning unconfirmed (OQ-023).
    pub z: u8,
    /// Top two bits of the run count in `A` formats; meaning unconfirmed (OQ-023).
    pub level: u8,
}

/// Decodes a frame to `width × height` pixels; `None` marks transparency.
///
/// Row RLE as `.tex` `BSH_RGB`: per row `(skip, count, data)` runs, `0xFE`
/// ends a row, `0xFF` the image. `data` is `count × bytes_per_pixel` bytes;
/// in `A` formats the count byte is `level << 6 | count`. Empty data is an
/// empty (fully transparent) frame.
pub fn decode_frame(
    format: FrameFormat,
    data: &[u8],
    width: usize,
    height: usize,
) -> Result<Vec<Option<Pixel>>, Error> {
    decode(format, data, width, height, false).map(|(px, _)| px)
}

/// As [`decode_frame`], but a run that extends past `width` is accepted and
/// its pixels beyond the right edge are dropped; returns the pixels and the
/// number dropped. Needed for 51 `Anlagen.dat` frames whose rows overrun
/// `width` by 1–16 pixels while row count and end marker are exact (P);
/// whether the original clips them is unknown (OQ-025).
pub fn decode_frame_clipped(
    format: FrameFormat,
    data: &[u8],
    width: usize,
    height: usize,
) -> Result<(Vec<Option<Pixel>>, usize), Error> {
    decode(format, data, width, height, true)
}

fn decode(
    format: FrameFormat,
    data: &[u8],
    width: usize,
    height: usize,
    clip: bool,
) -> Result<(Vec<Option<Pixel>>, usize), Error> {
    let mut out = vec![None; width * height];
    let mut dropped = 0;
    if data.is_empty() {
        return Ok((out, dropped));
    }
    let bpp = format.bytes_per_pixel();
    let (mut i, mut x, mut y) = (0, 0, 0);
    loop {
        let b = *data.get(i).ok_or(Error::BadPixels("missing end marker"))?;
        i += 1;
        if b == RLE_END_OF_ROW || b == RLE_END_OF_IMAGE {
            y += 1;
            x = 0;
            if b == RLE_END_OF_IMAGE {
                break;
            }
            continue;
        }
        let raw = *data.get(i).ok_or(Error::BadPixels("truncated run"))?;
        i += 1;
        let (level, count) = if format.has_level() {
            (raw >> 6, usize::from(raw & 0x3F))
        } else {
            (0, usize::from(raw))
        };
        let px = data
            .get(i..i + count * bpp)
            .ok_or(Error::BadPixels("truncated run"))?;
        i += count * bpp;
        x += usize::from(b);
        if y >= height || (!clip && x + count > width) {
            return Err(Error::BadPixels("run outside image"));
        }
        let kept = count.min(width.saturating_sub(x));
        dropped += count - kept;
        for (k, dst) in out[y * width + x.min(width)..][..kept]
            .iter_mut()
            .enumerate()
        {
            *dst = Some(match bpp {
                0 => Pixel::default(),
                1 => Pixel {
                    index: px[k],
                    z: 0,
                    level,
                },
                _ => Pixel {
                    index: px[2 * k + 1],
                    z: px[2 * k],
                    level,
                },
            });
        }
        x += count;
    }
    if y != height {
        return Err(Error::BadPixels("row count differs from height"));
    }
    if i != data.len() {
        return Err(Error::BadPixels("bytes after end marker"));
    }
    Ok((out, dropped))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn frame(format: &[u8], w: u16, h: u16, offset: u32, length: u32) -> Frame {
        let mut format_raw = [0; 12];
        format_raw[..format.len()].copy_from_slice(format);
        Frame {
            width: w,
            height: h,
            palette: 1,
            unknown_06: 8,
            format_raw,
            anchor_x: w / 2,
            anchor_y: h,
            offset,
            length,
            unknown_20: 0,
        }
    }

    #[test]
    fn frame_round_trip() {
        // Zoom-2 frame of entry 1546 (`Nebel01`), bytes from FigurenHeader.dat (P).
        let raw: [u8; FRAME_LEN] = [
            0x5a, 0, 0x2c, 0, 0, 0, 8, 0, b'B', b'S', b'H', b'_', b'R', b'G', b'B', b'A', b'V', 0,
            0, 0, 0x2e, 0, 0x24, 0, 0x4a, 0xcb, 0xca, 0, 0x3e, 0x0c, 0, 0, 0, 0, 0, 0,
        ];
        let f = Frame::parse(&raw).unwrap();
        assert_eq!(
            (f.width, f.height, f.anchor_x, f.anchor_y),
            (90, 44, 46, 36)
        );
        assert_eq!((f.offset, f.length), (0x00ca_cb4a, 0xc3e));
        assert_eq!(f.format(), Some(FrameFormat::RgbAV));
        assert_eq!(f.to_bytes(), raw);
    }

    #[test]
    fn decode_formats() {
        // V: skip 1 + 2 px, row end, 3 px.
        let v = decode_frame(
            FrameFormat::RgbV,
            &[1, 2, 7, 8, 0xFE, 0, 3, 1, 2, 3, 0xFF],
            3,
            2,
        );
        let idx: Vec<_> = v.unwrap().iter().map(|p| p.map(|p| p.index)).collect();
        assert_eq!(idx, [None, Some(7), Some(8), Some(1), Some(2), Some(3)]);
        // Z: pairs are [z, index].
        let z = decode_frame(FrameFormat::RgbZ, &[0, 1, 0xfb, 0x44, 0xFF], 1, 1).unwrap();
        assert_eq!(
            z[0],
            Some(Pixel {
                index: 0x44,
                z: 0xfb,
                level: 0
            })
        );
        // AV: 0xC2 = level 3, 2 px.
        let a = decode_frame(FrameFormat::RgbAV, &[1, 0xC2, 5, 6, 0xFF], 3, 1).unwrap();
        assert_eq!(a[2].map(|p| (p.index, p.level)), Some((6, 3)));
        // ALPHA: coverage only.
        let s = decode_frame(FrameFormat::Alpha, &[1, 2, 0xFE, 0, 1, 0xFF], 3, 2).unwrap();
        let cov: Vec<bool> = s.iter().map(Option::is_some).collect();
        assert_eq!(cov, [false, true, true, true, false, false]);
        // Empty data = empty frame; malformed data is rejected.
        assert!(
            decode_frame(FrameFormat::RgbV, &[], 2, 2)
                .unwrap()
                .iter()
                .all(Option::is_none)
        );
        assert!(decode_frame(FrameFormat::RgbV, &[0, 3, 1, 1, 1, 0xFF], 2, 1).is_err());
        assert!(decode_frame(FrameFormat::RgbZ, &[0, 1, 9, 0xFF], 1, 1).is_err());
    }

    #[test]
    fn clipped_decode_drops_overrun() {
        // Width 2; row 0 writes 3 pixels (one past the edge), row 1 is empty.
        let data = [0, 3, 0xa1, 1, 0xa2, 2, 0xa3, 3, 0xFE, 0xFF];
        assert!(decode_frame(FrameFormat::RgbWZ, &data, 2, 2).is_err());
        let (px, dropped) = decode_frame_clipped(FrameFormat::RgbWZ, &data, 2, 2).unwrap();
        assert_eq!(dropped, 1);
        let idx: Vec<_> = px.iter().map(|p| p.map(|p| (p.z, p.index))).collect();
        assert_eq!(idx, [Some((0xa1, 1)), Some((0xa2, 2)), None, None]);
        // A run starting beyond the edge drops entirely.
        let (_, dropped) =
            decode_frame_clipped(FrameFormat::RgbAH, &[3, 0x42, 7, 8, 0xFF], 2, 1).unwrap();
        assert_eq!(dropped, 2);
    }

    #[test]
    fn locate_spans_parts() {
        let sizes = [10, 5, 20];
        assert_eq!(locate(&sizes, 2, 3), Some(vec![(0, 2, 5)]));
        assert_eq!(
            locate(&sizes, 8, 10),
            Some(vec![(0, 8, 10), (1, 0, 5), (2, 0, 3)])
        );
        assert_eq!(locate(&sizes, 30, 5), Some(vec![(2, 15, 20)]));
        assert_eq!(locate(&sizes, 30, 6), None);
        assert_eq!(locate(&sizes, 4, 0), Some(vec![]));
    }

    #[test]
    fn header_round_trip() {
        // Synthetic file: one entry with a COLORS table and a sprite set with shadow.
        let rec = |name: &str, payload: &[u8]| {
            let mut out = Vec::new();
            write_record(&chunk::name_field(name).unwrap(), payload, &mut out);
            out
        };
        let mut table = [0u32.to_le_bytes(), 0u32.to_le_bytes()].concat();
        table.extend_from_slice(b"BODY\0\0\x00\x04");
        table.extend((0..=255u8).flat_map(|i| [i, i, i, 0]));
        table.extend_from_slice(b"NAME\0\0\x06\0a.pcx\0");
        let frames = |group: u32| FrameSet {
            group,
            frames: vec![frame(b"BSH_RGBV", 4, 4, 0, 0)],
        };
        let mut shadow = vec![3, 0, 0, 0, 0, 0, 0, 0];
        shadow.extend(rec("TEXTURES", &frames(0).to_bytes()));
        let mut set = vec![4, 0, 4, 0, 0, 0, 0, 0, b'x', 0, 0, 0, 0, 0, 0, 0];
        set.extend(rec("BODY2", &[0; 44]));
        for g in 0..3 {
            set.extend(rec("TEXTURES", &frames(g).to_bytes()));
        }
        set.extend(rec("SHADOW", &shadow));
        let mut entry = [0u32.to_le_bytes(), 101u32.to_le_bytes()].concat();
        entry.extend(rec("BEWOHNTYPE", b"MAEHER\0"));
        entry.extend(rec("COLORS", &rec("ENTRY", &table)));
        entry.extend(rec("HAEUSER", &rec("ENTRY", &set)));
        let body = rec("ENTRY", &entry);
        let mut file = b"ANNO\0\0\0\0\0\0\0\0\xff\xff\0\0".to_vec();
        file.extend(rec("FIGUREN", &body));
        file.extend_from_slice(b"TEXTURES\0\0\0\0\x08\0\0\0\x02\0\0\0\x01\0\0\0");
        let h = FigurenHeader::parse(&file).unwrap();
        let e = &h.entries[0];
        assert_eq!((e.id, e.palettes.len(), e.gfx.len()), (101, 1, 1));
        assert_eq!(e.record_str("BEWOHNTYPE").as_deref(), Some("MAEHER"));
        assert_eq!(e.gfx[0].frame_sets.len(), 3);
        assert_eq!(e.gfx[0].shadow.as_ref().unwrap().frame_sets.len(), 1);
        assert_eq!((h.volumes[0].group, h.volumes[0].frames), (2, 1));
        assert_eq!(h.to_bytes(), file);
    }
}
