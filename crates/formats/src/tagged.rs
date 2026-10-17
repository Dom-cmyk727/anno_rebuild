//! Tagged records: the small record container inside scenario players
//! (`SCENE/SPIELER`), `SCENE/COMPPROFIL2` and `Anlagen.dat` `COLORS/ENTRY`.
//!
//! Spec and evidence: `docs/formats/sze.md` ("Tagged records"). Summary (P =
//! checked over all 724 `SPIELER` and 50 `COMPPROFIL2` chunks of the 50 `.sze`
//! of GOG 2.0.0.5, `crates/formats/tests/scene_corpus.rs`):
//!
//! ```text
//! 0x00  6 bytes  name     [A-Z][A-Z0-9_]{0,5}, NUL-padded with zeros
//! 0x06  u16      length   payload length
//! 0x08  length   payload
//! ```
//!
//! Long form: if `length` is `0xFF00`, the real length is the `u16` at 0x08
//! and the payload starts at 0x0A. It occurs only in `ANNOADDON1` scenarios,
//! where every `STADT2` record and every `STADT2/PROD` record uses it
//! (including a 22-byte `PROD`), and in no `ANNO` scenario; why the writer
//! chose it is open (OQ-027).
//!
//! A record sequence is a payload that parses *exactly* into records. Some
//! payloads hold a fixed prefix followed by a nested sequence (e.g. `SCHIFF`:
//! 16 bytes, then records); which ones is a property of the record, not of
//! this container, and is decoded in [`crate::scene`].

use std::fmt;

/// Size of the name field.
pub const NAME_LEN: usize = 6;
/// Size of a record header in the short form.
pub const HEADER_LEN: usize = NAME_LEN + 2;
/// Size of a record header in the long form.
pub const LONG_HEADER_LEN: usize = HEADER_LEN + 2;
/// `length` value that announces the long form.
pub const LONG_MARKER: u16 = 0xFF00;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// Fewer bytes than a header where a record was expected.
    Truncated { offset: usize },
    /// The name field does not match the pattern above.
    InvalidName { offset: usize },
    /// The payload runs past the end of the sequence.
    LengthOutOfBounds { offset: usize, length: u16 },
    /// A record cannot be written: payload longer than `u16::MAX`, or a
    /// short-form payload of exactly [`LONG_MARKER`] bytes.
    Unencodable { name: String, length: usize },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Truncated { offset } => write!(f, "truncated record header at {offset}"),
            Error::InvalidName { offset } => write!(f, "invalid record name at {offset}"),
            Error::LengthOutOfBounds { offset, length } => {
                write!(
                    f,
                    "record at {offset} declares length {length} past end of data"
                )
            }
            Error::Unencodable { name, length } => {
                write!(f, "record {name} with {length} bytes cannot be encoded")
            }
        }
    }
}

impl std::error::Error for Error {}

/// One record, borrowing from the buffer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Record<'a> {
    /// The name without padding. Always ASCII (validated during parsing).
    pub name: &'a str,
    /// Offset of the record header: relative to the start of the parsed
    /// sequence plus the `base` passed to [`parse_records`].
    pub offset: usize,
    /// Whether the header uses the long form ([`LONG_MARKER`]).
    pub long: bool,
    pub payload: &'a [u8],
}

impl Record<'_> {
    /// Length of this record when written.
    pub fn encoded_len(&self) -> usize {
        let header = if self.long {
            LONG_HEADER_LEN
        } else {
            HEADER_LEN
        };
        header + self.payload.len()
    }
}

/// Parses `data` as an exact record sequence. `base` is added to the
/// offsets (in records and errors) so they can point into the whole file.
pub fn parse_records(data: &[u8], base: usize) -> Result<Vec<Record<'_>>, Error> {
    let mut out = Vec::new();
    let mut off = 0;
    while off < data.len() {
        let at = base + off;
        let header = data
            .get(off..off + HEADER_LEN)
            .ok_or(Error::Truncated { offset: at })?;
        let name = name_of(&header[..NAME_LEN]).ok_or(Error::InvalidName { offset: at })?;
        let mut length = u16::from_le_bytes([header[6], header[7]]);
        let mut start = off + HEADER_LEN;
        let long = length == LONG_MARKER;
        if long {
            let ext = data
                .get(start..start + 2)
                .ok_or(Error::Truncated { offset: at })?;
            length = u16::from_le_bytes([ext[0], ext[1]]);
            start += 2;
        }
        let end = start + usize::from(length);
        let payload = data
            .get(start..end)
            .ok_or(Error::LengthOutOfBounds { offset: at, length })?;
        out.push(Record {
            name,
            offset: at,
            long,
            payload,
        });
        off = end;
    }
    Ok(out)
}

/// The name in a 6-byte field, if it matches `[A-Z][A-Z0-9_]{0,5}` and is
/// padded with zeros only.
fn name_of(field: &[u8]) -> Option<&str> {
    let len = field.iter().position(|&b| b == 0).unwrap_or(field.len());
    let (name, pad) = field.split_at(len);
    let (first, rest) = name.split_first()?;
    let valid = first.is_ascii_uppercase()
        && rest
            .iter()
            .all(|&b| b.is_ascii_uppercase() || b.is_ascii_digit() || b == b'_')
        && pad.iter().all(|&b| b == 0);
    // ASCII by the check above.
    valid.then(|| std::str::from_utf8(name).ok()).flatten()
}

/// Appends one record. `name` must match the name pattern.
pub fn write_record(
    name: &str,
    long: bool,
    payload: &[u8],
    out: &mut Vec<u8>,
) -> Result<(), Error> {
    let unencodable = || Error::Unencodable {
        name: name.to_string(),
        length: payload.len(),
    };
    let mut field = [0u8; NAME_LEN];
    field
        .get_mut(..name.len())
        .ok_or_else(unencodable)?
        .copy_from_slice(name.as_bytes());
    if name_of(&field) != Some(name) {
        return Err(unencodable());
    }
    let length = u16::try_from(payload.len()).map_err(|_| unencodable())?;
    if !long && length == LONG_MARKER {
        return Err(unencodable());
    }
    out.extend_from_slice(&field);
    if long {
        out.extend_from_slice(&LONG_MARKER.to_le_bytes());
    }
    out.extend_from_slice(&length.to_le_bytes());
    out.extend_from_slice(payload);
    Ok(())
}

/// Re-encodes a parsed sequence (headers rebuilt, payloads copied).
pub fn write_records(records: &[Record<'_>], out: &mut Vec<u8>) -> Result<(), Error> {
    records
        .iter()
        .try_for_each(|r| write_record(r.name, r.long, r.payload, out))
}

/// The first record called `name`.
pub fn find<'r, 'a>(records: &'r [Record<'a>], name: &str) -> Option<&'r Record<'a>> {
    records.iter().find(|r| r.name == name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn short_and_long_forms_round_trip() {
        let mut buf = Vec::new();
        write_record("HEAD", false, &[1, 2, 3], &mut buf).unwrap();
        write_record("SEL_0", true, &[9; 4], &mut buf).unwrap();
        write_record("STADT2", false, &[], &mut buf).unwrap();
        assert_eq!(&buf[..11], b"HEAD\0\0\x03\x00\x01\x02\x03");
        assert_eq!(&buf[11..21], b"SEL_0\0\x00\xff\x04\x00");
        let recs = parse_records(&buf, 100).unwrap();
        let summary: Vec<_> = recs
            .iter()
            .map(|r| (r.name, r.offset, r.long, r.payload.len()))
            .collect();
        assert_eq!(
            summary,
            [
                ("HEAD", 100, false, 3),
                ("SEL_0", 111, true, 4),
                ("STADT2", 125, false, 0)
            ]
        );
        let mut again = Vec::new();
        write_records(&recs, &mut again).unwrap();
        assert_eq!(again, buf);
    }

    #[test]
    fn rejects_malformed_input() {
        assert_eq!(
            parse_records(b"HEAD\0\0\x01", 0),
            Err(Error::Truncated { offset: 0 })
        );
        assert_eq!(
            parse_records(b"HEAD\0x\x00\x00", 0),
            Err(Error::InvalidName { offset: 0 })
        );
        assert_eq!(
            parse_records(b"head\0\0\x00\x00", 0),
            Err(Error::InvalidName { offset: 0 })
        );
        assert_eq!(
            parse_records(b"HEAD\0\0\x05\x00ab", 0),
            Err(Error::LengthOutOfBounds {
                offset: 0,
                length: 5
            })
        );
        assert_eq!(
            parse_records(b"HEAD\0\0\x00\xff\x01", 0),
            Err(Error::Truncated { offset: 0 })
        );
        let mut out = Vec::new();
        assert!(write_record("TOOLONG", false, &[], &mut out).is_err());
        assert!(write_record("x", false, &[], &mut out).is_err());
        assert!(write_record("BIG", false, &vec![0; 0xFF00], &mut out).is_err());
        assert!(write_record("BIG", true, &vec![0; 0xFF00], &mut out).is_ok());
    }
}
