//! `.inc` files: a root `ANNO` with one `ACCESS` chunk whose payload is a list
//! of NUL-terminated strings `"<source file>,<number>"`, e.g. `image001.pcx,1`.
//!
//! Evidence (P): all 1,213 `.inc` of GOG 2.0.0.5 have exactly one `ACCESS`
//! chunk; 340 are empty. What the number refers to (likely a `TEXTURE` id in
//! the matching `.tex`) is checked in `tests/corpus.rs`; spec: `docs/formats/tex.md`.

use crate::chunk;

/// One `ACCESS` record.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AccessEntry<'a> {
    /// Source file name, raw bytes (Windows-1252).
    pub source: &'a [u8],
    /// The number after the comma.
    pub number: u32,
}

/// Parses a whole `.inc` file into its `ACCESS` records.
/// `None` if the file does not have the layout described above.
pub fn parse_access(buf: &[u8]) -> Option<Vec<AccessEntry<'_>>> {
    let file = chunk::parse_file(buf).ok()?;
    let root = file.chunks.first()?;
    let kids = root.children.as_deref()?;
    let [access] = kids else { return None };
    if access.name() != "ACCESS" || !file.trailing.is_empty() {
        return None;
    }
    parse_records(access.payload)
}

/// Parses an `ACCESS` payload: a concatenation of NUL-terminated records.
pub fn parse_records(payload: &[u8]) -> Option<Vec<AccessEntry<'_>>> {
    let mut out = Vec::new();
    let mut rest = payload;
    while !rest.is_empty() {
        let nul = rest.iter().position(|&b| b == 0)?;
        let record = &rest[..nul];
        rest = &rest[nul + 1..];
        let comma = record.iter().rposition(|&b| b == b',')?;
        let digits = std::str::from_utf8(&record[comma + 1..]).ok()?;
        if digits.is_empty() || !digits.bytes().all(|b| b.is_ascii_digit()) {
            return None;
        }
        out.push(AccessEntry {
            source: &record[..comma],
            number: digits.parse().ok()?,
        });
    }
    Some(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records() {
        let p = b"image001.pcx,1\0image002.pcx,2\0";
        let r = parse_records(p).unwrap();
        assert_eq!(r.len(), 2);
        assert_eq!((r[1].source, r[1].number), (&b"image002.pcx"[..], 2));
        assert_eq!(parse_records(b"").unwrap(), []);
        assert!(parse_records(b"image001.pcx,1").is_none()); // no NUL
        assert!(parse_records(b"image001.pcx\0").is_none()); // no number
    }
}
