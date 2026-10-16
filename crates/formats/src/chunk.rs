//! Max Design chunk container, shared by `.sze`, `.scp`, `.tex`, `.inc`,
//! `.zei` and `Anlagen.dat`.
//!
//! A chunk is a 12-byte name field, a `u32` little-endian payload size and the
//! payload. The name is NUL-terminated; bytes after the NUL may be garbage and
//! are kept verbatim in [`Chunk::name_field`]. A payload is treated as a
//! container when it parses *exactly* into one or more valid chunks.
//!
//! Evidence: all 50 `.sze`, 1214 `.tex`, 1213 `.inc`, 11 `.zei` and
//! `Anlagen.dat` of GOG 2.0.0.5 parse completely; all 465 `.scp` parse up to
//! the end of the root chunk and carry trailing bytes
//! (`docs/research/game-files-analysis.md` §3).
//!
//! The semantics deliberately mirror the reference parser `tools/chunks.py`,
//! including its two heuristics: the name pattern `[A-Z][A-Z0-9_]{1,11}` and
//! the nesting limit [`MAX_DEPTH`]. Both are parser choices, not proven
//! properties of the original format.
//!
//! Writing ([`write_file`]) re-encodes the tree: chunks with children are
//! rebuilt from their children, leaf payloads and name fields are copied
//! verbatim. Every corpus file round-trips byte-identically
//! (`tests/corpus.rs`). Spec: `docs/formats/chunk-container.md`.

use std::{fmt, io};

/// Size of the name field in bytes.
pub const NAME_LEN: usize = 12;
/// Size of a chunk header (name field + `u32` size).
pub const HEADER_LEN: usize = NAME_LEN + 4;
/// Payloads are only probed for children up to this depth (as in `tools/chunks.py`).
pub const MAX_DEPTH: usize = 6;

/// One chunk, borrowing from the file buffer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Chunk<'a> {
    /// The raw 12-byte name field, including the NUL and any trailing bytes.
    pub name_field: &'a [u8; NAME_LEN],
    /// Absolute offset of the chunk header within the file.
    pub offset: usize,
    /// The payload (`size` bytes following the header).
    pub payload: &'a [u8],
    /// Child chunks, if the payload parses exactly as a chunk sequence.
    pub children: Option<Vec<Chunk<'a>>>,
}

impl<'a> Chunk<'a> {
    /// The name up to the first NUL. Always ASCII (validated during parsing).
    pub fn name(&self) -> &'a str {
        let field: &'a [u8; NAME_LEN] = self.name_field;
        let raw = name_bytes(field);
        // Validated by `valid_name` before a Chunk is constructed.
        std::str::from_utf8(raw).unwrap_or_default()
    }

    /// Payload size in bytes as stored in the header.
    pub fn size(&self) -> usize {
        self.payload.len()
    }

    /// Length of this chunk when written: header plus re-encoded payload.
    pub fn encoded_len(&self) -> usize {
        HEADER_LEN + self.encoded_payload_len()
    }

    fn encoded_payload_len(&self) -> usize {
        match &self.children {
            Some(kids) => kids.iter().map(Chunk::encoded_len).sum(),
            None => self.payload.len(),
        }
    }
}

/// Builds a name field for a new chunk: the name, then NUL padding.
///
/// Returns `None` if `name` does not match `[A-Z][A-Z0-9_]{1,11}`. Original
/// files almost always pad with zeros (17,507 of 17,529 corpus chunks); the
/// rest carry leftover bytes after the NUL, which only a parsed
/// [`Chunk::name_field`] preserves.
pub fn name_field(name: &str) -> Option<[u8; NAME_LEN]> {
    let bytes = name.as_bytes();
    if bytes.len() >= NAME_LEN || !valid_name(bytes) {
        return None;
    }
    let mut field = [0u8; NAME_LEN];
    field[..bytes.len()].copy_from_slice(bytes);
    Some(field)
}

/// A parsed file: the top-level chunk sequence plus any bytes after it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ChunkFile<'a> {
    pub chunks: Vec<Chunk<'a>>,
    /// Bytes after the root chunk when the file is not an exact chunk
    /// sequence (seen on `.scp`; meaning unknown, see `docs/open-questions.md`).
    pub trailing: &'a [u8],
    /// Absolute offset of `trailing`.
    pub trailing_offset: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// Fewer than [`HEADER_LEN`] bytes where a header was expected.
    Truncated { offset: usize },
    /// The name field does not match the accepted name pattern.
    InvalidName { offset: usize },
    /// The declared payload size runs past the end of the buffer.
    SizeOutOfBounds { offset: usize, size: u32 },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Truncated { offset } => write!(f, "truncated chunk header at {offset}"),
            Error::InvalidName { offset } => write!(f, "invalid chunk name at {offset}"),
            Error::SizeOutOfBounds { offset, size } => {
                write!(f, "chunk at {offset} declares size {size} past end of data")
            }
        }
    }
}

impl std::error::Error for Error {}

/// Parses a whole file.
///
/// If the buffer is an exact chunk sequence, `trailing` is empty. Otherwise
/// the first chunk is parsed as the root and everything after it is returned
/// as `trailing`.
pub fn parse_file(buf: &[u8]) -> Result<ChunkFile<'_>, Error> {
    if let Some(chunks) = parse_seq(buf, 0, buf.len(), 0) {
        return Ok(ChunkFile {
            chunks,
            trailing: &buf[buf.len()..],
            trailing_offset: buf.len(),
        });
    }
    let (name_field, size) = read_header(buf, 0)?;
    let end = HEADER_LEN + size;
    let root = Chunk {
        name_field,
        offset: 0,
        payload: &buf[HEADER_LEN..end],
        children: probe_children(buf, HEADER_LEN, end, 0),
    };
    Ok(ChunkFile {
        chunks: vec![root],
        trailing: &buf[end..],
        trailing_offset: end,
    })
}

/// Parses `buf[start..end]` as an exact chunk sequence; `None` if it is not one.
/// Offsets in the result are absolute within `buf`.
fn parse_seq(buf: &[u8], start: usize, end: usize, depth: usize) -> Option<Vec<Chunk<'_>>> {
    let mut out = Vec::new();
    let mut off = start;
    while off < end {
        let (name_field, size) = read_header(&buf[..end], off).ok()?;
        let payload_start = off + HEADER_LEN;
        let payload_end = payload_start + size;
        out.push(Chunk {
            name_field,
            offset: off,
            payload: &buf[payload_start..payload_end],
            children: probe_children(buf, payload_start, payload_end, depth),
        });
        off = payload_end;
    }
    // `off == end` holds here; an empty range yields an empty sequence, which
    // callers never request because probing needs at least one header.
    Some(out)
}

fn probe_children(buf: &[u8], start: usize, end: usize, depth: usize) -> Option<Vec<Chunk<'_>>> {
    if depth < MAX_DEPTH && end - start >= HEADER_LEN {
        parse_seq(buf, start, end, depth + 1)
    } else {
        None
    }
}

/// Reads and validates the header at `off`; returns the name field and payload size.
fn read_header(buf: &[u8], off: usize) -> Result<(&[u8; NAME_LEN], usize), Error> {
    let header = buf
        .get(off..off + HEADER_LEN)
        .ok_or(Error::Truncated { offset: off })?;
    let (name, size) = header.split_at(NAME_LEN);
    let name: &[u8; NAME_LEN] = name
        .try_into()
        .map_err(|_| Error::Truncated { offset: off })?;
    if !valid_name(name_bytes(name)) {
        return Err(Error::InvalidName { offset: off });
    }
    let size = u32::from_le_bytes([size[0], size[1], size[2], size[3]]);
    let end = usize::try_from(size)
        .ok()
        .and_then(|s| (off + HEADER_LEN).checked_add(s))
        .filter(|&e| e <= buf.len())
        .ok_or(Error::SizeOutOfBounds { offset: off, size })?;
    Ok((name, end - off - HEADER_LEN))
}

fn name_bytes(field: &[u8; NAME_LEN]) -> &[u8] {
    let len = field.iter().position(|&b| b == 0).unwrap_or(NAME_LEN);
    &field[..len]
}

/// `[A-Z][A-Z0-9_]{1,11}`, the pattern used by `tools/chunks.py`.
fn valid_name(name: &[u8]) -> bool {
    match name.split_first() {
        Some((first, rest)) => {
            first.is_ascii_uppercase()
                && !rest.is_empty()
                && rest
                    .iter()
                    .all(|&b| b.is_ascii_uppercase() || b.is_ascii_digit() || b == b'_')
        }
        None => false,
    }
}

/// Writes a parsed (or constructed) file: its chunks, then the trailing bytes.
pub fn write_file(file: &ChunkFile<'_>, out: &mut impl io::Write) -> io::Result<()> {
    for chunk in &file.chunks {
        write_chunk(chunk, out)?;
    }
    out.write_all(file.trailing)
}

/// Writes one chunk. If it has children, the payload is re-encoded from them
/// and the size field recomputed; otherwise the payload is copied.
pub fn write_chunk(chunk: &Chunk<'_>, out: &mut impl io::Write) -> io::Result<()> {
    let size = u32::try_from(chunk.encoded_payload_len()).map_err(|_| {
        io::Error::new(
            io::ErrorKind::InvalidInput,
            format!("chunk {} payload exceeds u32::MAX bytes", chunk.name()),
        )
    })?;
    out.write_all(chunk.name_field)?;
    out.write_all(&size.to_le_bytes())?;
    match &chunk.children {
        Some(kids) => kids.iter().try_for_each(|kid| write_chunk(kid, out)),
        None => out.write_all(chunk.payload),
    }
}

/// Writes the chunk tree in the line format shared with `tools/chunks.py tree`:
/// one `<indent>NAME off=<offset> size=<size>` line per chunk (two spaces per
/// level), then `TRAILING off=<offset> len=<len>` if trailing bytes exist.
pub fn write_tree(file: &ChunkFile<'_>, out: &mut impl fmt::Write) -> fmt::Result {
    fn rec(chunks: &[Chunk<'_>], depth: usize, out: &mut impl fmt::Write) -> fmt::Result {
        for c in chunks {
            writeln!(
                out,
                "{:indent$}{} off={} size={}",
                "",
                c.name(),
                c.offset,
                c.size(),
                indent = depth * 2
            )?;
            if let Some(kids) = &c.children {
                rec(kids, depth + 1, out)?;
            }
        }
        Ok(())
    }
    rec(&file.chunks, 0, out)?;
    if !file.trailing.is_empty() {
        writeln!(
            out,
            "TRAILING off={} len={}",
            file.trailing_offset,
            file.trailing.len()
        )?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn chunk(name: &[u8], payload: &[u8]) -> Vec<u8> {
        let mut v = vec![0u8; NAME_LEN];
        v[..name.len()].copy_from_slice(name);
        v.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        v.extend_from_slice(payload);
        v
    }

    #[test]
    fn nested_exact_fit() {
        let inner = [chunk(b"HEAD", &[1, 2, 3, 4]), chunk(b"BODY", &[])].concat();
        let buf = chunk(b"CHARSET", &inner);
        let f = parse_file(&buf).unwrap();
        assert!(f.trailing.is_empty());
        let root = &f.chunks[0];
        assert_eq!(root.name(), "CHARSET");
        let kids = root.children.as_ref().unwrap();
        assert_eq!(kids.len(), 2);
        assert_eq!(
            (kids[0].name(), kids[0].offset, kids[0].size()),
            ("HEAD", 16, 4)
        );
        assert_eq!(
            (kids[1].name(), kids[1].offset, kids[1].size()),
            ("BODY", 36, 0)
        );
    }

    #[test]
    fn garbage_after_nul_is_kept() {
        let mut buf = chunk(b"ANNO", &[0; 4]);
        buf[5..12].copy_from_slice(b"xyz\xff\x01\x02\x03");
        let f = parse_file(&buf).unwrap();
        assert_eq!(f.chunks[0].name(), "ANNO");
        assert_eq!(&f.chunks[0].name_field[5..8], b"xyz");
    }

    #[test]
    fn opaque_payload_has_no_children() {
        let buf = chunk(b"DATA", &[0xAB; 32]);
        assert_eq!(parse_file(&buf).unwrap().chunks[0].children, None);
    }

    #[test]
    fn trailing_bytes_after_root() {
        let mut buf = chunk(b"INSEL5", &chunk(b"INFO", &[7; 8]));
        buf.extend_from_slice(&[1, 2, 3]);
        let f = parse_file(&buf).unwrap();
        assert_eq!(f.chunks.len(), 1);
        assert_eq!(f.trailing, &[1, 2, 3]);
        assert_eq!(f.trailing_offset, buf.len() - 3);
        let mut s = String::new();
        write_tree(&f, &mut s).unwrap();
        assert_eq!(
            s,
            "INSEL5 off=0 size=24\n  INFO off=16 size=8\nTRAILING off=40 len=3\n"
        );
    }

    #[test]
    fn rejects_bad_names_and_sizes() {
        assert_eq!(
            parse_file(&chunk(b"bad", &[])),
            Err(Error::InvalidName { offset: 0 })
        );
        assert_eq!(
            parse_file(&chunk(b"A", &[])),
            Err(Error::InvalidName { offset: 0 })
        );
        let mut buf = chunk(b"ANNO", &[0; 4]);
        buf[12] = 99;
        assert_eq!(
            parse_file(&buf),
            Err(Error::SizeOutOfBounds {
                offset: 0,
                size: 99
            })
        );
        assert_eq!(parse_file(&[0; 5]), Err(Error::Truncated { offset: 0 }));
    }

    fn round_trip(buf: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        write_file(&parse_file(buf).unwrap(), &mut out).unwrap();
        out
    }

    #[test]
    fn writes_back_identically() {
        let inner = [chunk(b"HEAD", &[1, 2, 3, 4]), chunk(b"BODY", &[])].concat();
        let mut buf = chunk(b"CHARSET", &inner);
        buf[8] = 0xE0; // garbage after the NUL survives
        buf.extend_from_slice(&[9, 9]); // so do trailing bytes
        assert_eq!(round_trip(&buf), buf);
    }

    #[test]
    fn rebuilds_container_from_children() {
        let leaf = [0xAAu8; 4];
        let other = [0xBBu8; 2];
        let kid_name = name_field("HEIGHT").unwrap();
        let root_name = name_field("INSEL5").unwrap();
        let kid = Chunk {
            name_field: &kid_name,
            offset: 0,
            payload: &leaf,
            children: None,
        };
        // A stale payload on the parent is ignored when children are present.
        let root = Chunk {
            name_field: &root_name,
            offset: 0,
            payload: &other,
            children: Some(vec![kid.clone(), kid]),
        };
        assert_eq!(root.encoded_len(), 16 + 2 * 20);
        let mut out = Vec::new();
        write_chunk(&root, &mut out).unwrap();
        let expected = chunk(
            b"INSEL5",
            &[chunk(b"HEIGHT", &leaf), chunk(b"HEIGHT", &leaf)].concat(),
        );
        assert_eq!(out, expected);
    }

    #[test]
    fn name_field_validates() {
        assert_eq!(&name_field("ANNO").unwrap()[..5], b"ANNO\0");
        assert_eq!(name_field("ANNOADDON1").unwrap()[10..], [0, 0]);
        assert_eq!(name_field("anno"), None);
        assert_eq!(name_field("A"), None);
        assert_eq!(name_field("ABCDEFGHIJKL"), None); // no room for the NUL
    }

    #[test]
    fn depth_limit_matches_reference() {
        // 9 levels of nesting; children are only probed for chunks above MAX_DEPTH.
        let mut buf = chunk(b"LEAF", &[0; 16]);
        for _ in 0..8 {
            buf = chunk(b"NEST", &buf);
        }
        let f = parse_file(&buf).unwrap();
        let mut c = &f.chunks[0];
        let mut levels = 0;
        while let Some(kids) = &c.children {
            c = &kids[0];
            levels += 1;
        }
        assert_eq!(levels, MAX_DEPTH);
    }
}
