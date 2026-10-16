//! Colour tables in `Anlagen.dat`: `ANNO/COLORS/ENTRY` (440 entries).
//!
//! Spec and evidence: `docs/formats/palette.md` ("`Anlagen.dat/COLORS`").
//! Each `ENTRY` payload is, all integers little-endian (P = checked over all
//! 440 entries of GOG 2.0.0.5):
//!
//! ```text
//! 0x000  u32      flag        0 (OQ-022)
//! 0x004  u32      id          1..=440, unique, ascending
//! 0x008  4 bytes  "BODY"      tag
//! 0x00C  u16      0           (OQ-022)
//! 0x00E  u16      1024        length of the palette
//! 0x010  1024     palette     256 x [B, G, R, 0], as `.tex` COLOR/BODY
//! 0x410  4 bytes  "NAME"      tag
//! 0x414  u16      0           (OQ-022)
//! 0x416  u16      n           length of the name, including its NUL
//! 0x418  n        name        `Farbtabellen\<file>.pcx`, NUL-terminated
//! ```
//!
//! The two tagged records are not generic chunks (4-byte tag, no 12-byte
//! name field), so the chunk reader sees `ENTRY` as a leaf.

use std::fmt;

use crate::chunk;
use crate::tex::{PALETTE_LEN, Palette};

const TAG_BODY: &[u8; 4] = b"BODY";
const TAG_NAME: &[u8; 4] = b"NAME";
/// Offset of the palette in an `ENTRY` payload.
pub const PALETTE_OFFSET: usize = 0x10;
/// Offset of the name string in an `ENTRY` payload.
pub const NAME_OFFSET: usize = 0x418;
const PALETTE_BYTES: usize = PALETTE_LEN * 4;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// `ANNO/COLORS` not found.
    NoColors,
    /// An `ENTRY` does not have the layout above; `what` names the check.
    BadEntry { offset: usize, what: &'static str },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoColors => write!(f, "no ANNO/COLORS chunk"),
            Error::BadEntry { offset, what } => write!(f, "COLORS/ENTRY at {offset}: {what}"),
        }
    }
}

impl std::error::Error for Error {}

/// One colour table.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ColorTable<'a> {
    /// `id` and the 8-byte prefix as for `.tex` palettes; `prefix[..4]` is the flag.
    pub palette: Palette<'a>,
    /// Name including the terminating NUL, raw (Windows-1252).
    pub name: &'a [u8],
}

impl ColorTable<'_> {
    /// The name up to the NUL, Latin-1 decoded (matches Windows-1252 for
    /// the umlauts seen, e.g. `Gräser01.PCX`).
    pub fn name_lossy(&self) -> String {
        let len = self.name.len().saturating_sub(1);
        self.name[..len].iter().map(|&b| char::from(b)).collect()
    }

    /// Re-encodes the `ENTRY` payload. Byte-identical to the parsed payload,
    /// since every byte is either a field or a value the parser checked.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(NAME_OFFSET + self.name.len());
        out.extend_from_slice(self.palette.prefix);
        out.extend_from_slice(TAG_BODY);
        out.extend_from_slice(&[0, 0]);
        out.extend_from_slice(&u16_le(PALETTE_BYTES));
        out.extend(self.palette.entries.iter().flatten());
        out.extend_from_slice(TAG_NAME);
        out.extend_from_slice(&[0, 0]);
        out.extend_from_slice(&u16_le(self.name.len()));
        out.extend_from_slice(self.name);
        out
    }
}

fn u16_le(n: usize) -> [u8; 2] {
    u16::try_from(n).expect("length fits u16").to_le_bytes()
}

/// Parses one `COLORS/ENTRY` payload. `offset` is only used in errors.
pub fn parse_entry(payload: &[u8], offset: usize) -> Result<ColorTable<'_>, Error> {
    let bad = |what| Error::BadEntry { offset, what };
    let u16_at = |o: usize| u16::from_le_bytes([payload[o], payload[o + 1]]);
    if payload.len() < NAME_OFFSET + 1 {
        return Err(bad("shorter than palette + name header"));
    }
    if &payload[0x08..0x0C] != TAG_BODY || u16_at(0x0C) != 0 {
        return Err(bad("no BODY record"));
    }
    if usize::from(u16_at(0x0E)) != PALETTE_BYTES {
        return Err(bad("BODY length is not 1024"));
    }
    if &payload[0x410..0x414] != TAG_NAME || u16_at(0x414) != 0 {
        return Err(bad("no NAME record"));
    }
    let name = &payload[NAME_OFFSET..];
    if usize::from(u16_at(0x416)) != name.len() {
        return Err(bad("NAME length does not end the entry"));
    }
    if name.iter().position(|&b| b == 0) != Some(name.len() - 1) {
        return Err(bad("NAME is not one NUL-terminated string"));
    }
    let prefix: &[u8; 8] = payload[..8].try_into().map_err(|_| bad("prefix"))?;
    let (entries, _) = payload[PALETTE_OFFSET..PALETTE_OFFSET + PALETTE_BYTES].as_chunks::<4>();
    let entries = entries.try_into().map_err(|_| bad("palette size"))?;
    Ok(ColorTable {
        palette: Palette {
            id: u32::from_le_bytes([prefix[4], prefix[5], prefix[6], prefix[7]]),
            prefix,
            entries,
        },
        name,
    })
}

/// All colour tables of an `Anlagen.dat` buffer, in file order.
pub fn parse_anlagen(buf: &[u8]) -> Result<Vec<ColorTable<'_>>, Error> {
    let file = chunk::parse_file(buf).map_err(|_| Error::NoColors)?;
    let colors = file
        .chunks
        .first()
        .and_then(|root| root.children.as_deref())
        .and_then(|kids| kids.iter().find(|c| c.name() == "COLORS"))
        .ok_or(Error::NoColors)?;
    colors
        .children
        .as_deref()
        .unwrap_or_default()
        .iter()
        .map(|e| {
            if e.name() == "ENTRY" {
                parse_entry(e.payload, e.offset)
            } else {
                Err(Error::BadEntry {
                    offset: e.offset,
                    what: "child is not ENTRY",
                })
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A synthetic entry built from the layout above.
    fn entry(id: u32, name: &[u8]) -> Vec<u8> {
        let mut p = [0u32.to_le_bytes(), id.to_le_bytes()].concat();
        p.extend_from_slice(b"BODY\0\0\x00\x04");
        p.extend((0..=255u8).flat_map(|i| [i, 0x20, 0xff - i, 0]));
        p.extend_from_slice(b"NAME\0\0");
        p.extend_from_slice(&u16_le(name.len()));
        p.extend_from_slice(name);
        p
    }

    #[test]
    fn parses_and_round_trips() {
        let raw = entry(7, b"Farbtabellen\\Gr\xe4ser01.PCX\0");
        let t = parse_entry(&raw, 0).unwrap();
        assert_eq!(t.palette.id, 7);
        assert_eq!(t.palette.rgb(1), [0xfe, 0x20, 1]);
        assert_eq!(t.name_lossy(), "Farbtabellen\\Gr\u{e4}ser01.PCX");
        assert_eq!(t.to_bytes(), raw);
    }

    #[test]
    fn rejects_malformed() {
        let good = entry(1, b"a.pcx\0");
        let mut cases = Vec::new();
        let mut c = good.clone();
        c[0x0E] = 1; // BODY length 1025
        cases.push(c);
        let mut c = good.clone();
        c[0x0C] = 1; // non-zero u16 before the length
        cases.push(c);
        let mut c = good.clone();
        c[0x410] = b'X'; // wrong tag
        cases.push(c);
        let mut c = good.clone();
        c.push(0); // NAME length does not reach the end
        cases.push(c);
        let mut c = good.clone();
        c[NAME_OFFSET] = 0; // NUL inside the name
        cases.push(c);
        cases.push(good[..0x400].to_vec());
        for (i, c) in cases.iter().enumerate() {
            assert!(parse_entry(c, 0).is_err(), "case {i}");
        }
        assert!(parse_entry(&good, 0).is_ok());
    }
}
