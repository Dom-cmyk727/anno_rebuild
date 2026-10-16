//! Buildings and objects: `Anlagen.dat`.
//!
//! Spec and evidence: `docs/formats/anlagen.md`. Summary (all P = checked
//! over GOG 2.0.0.5, `crates/formats/tests/anlagen_corpus.rs`):
//!
//! - Root `ANNO` → `COLORS` (440 colour tables, `crate::colors`), `ANLAGEN`
//!   (921 `ENTRY`), three `TEXTURES` pixel volumes (zoom groups 0, 1, 2).
//! - `ANLAGEN/ENTRY` has the `FigurenHeader.dat` entry layout
//!   ([`figuren::Entry`]): 8-byte prefix (`u32 0`, `u32 id`), records,
//!   colour tables in `COLORS`, sprite sets in `HAEUSER`.
//! - `TEXTURES` payload = `u32 group`, `u32 frames` (9,633 each), then the
//!   frame pixel data; frame `offset`s count from the byte after `frames`.

use crate::chunk::{self, Chunk};
use crate::colors::{self, ColorTable};
use crate::figuren::{self, Entry};

/// Size of the `group`/`frames` prefix of a `TEXTURES` volume.
pub const VOLUME_PREFIX_LEN: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// `ANNO/ANLAGEN` or the volumes are missing or malformed.
    Layout(&'static str),
    Colors(colors::Error),
    Entry(figuren::Error),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Layout(what) => write!(f, "Anlagen.dat: {what}"),
            Error::Colors(e) => write!(f, "{e}"),
            Error::Entry(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for Error {}

/// One inline `TEXTURES` pixel volume.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Volume<'a> {
    /// Zoom group (0, 1, 2), as in `TEXTURES` frame lists.
    pub group: u32,
    /// Number of frame headers of this group in `ANLAGEN`.
    pub frames: u32,
    /// Pixel data; frame `offset`s index into this slice.
    pub data: &'a [u8],
}

/// A parsed `Anlagen.dat`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AnlagenFile<'a> {
    pub colors: Vec<ColorTable<'a>>,
    /// The raw `ANLAGEN/ENTRY` chunks, for byte-exact writing.
    pub entry_chunks: Vec<Chunk<'a>>,
    pub entries: Vec<Entry<'a>>,
    pub volumes: Vec<Volume<'a>>,
}

impl<'a> AnlagenFile<'a> {
    pub fn parse(buf: &'a [u8]) -> Result<Self, Error> {
        let colors = colors::parse_anlagen(buf).map_err(Error::Colors)?;
        let file = chunk::parse_file(buf).map_err(|_| Error::Layout("no root"))?;
        let kids = file
            .chunks
            .first()
            .and_then(|r| r.children.as_deref())
            .ok_or(Error::Layout("root is not a container"))?;
        let anlagen = kids
            .iter()
            .find(|c| c.name() == "ANLAGEN")
            .ok_or(Error::Layout("no ANLAGEN"))?;
        let entry_chunks = chunk::parse_chunks(anlagen.payload)
            .ok_or(Error::Layout("ANLAGEN is not a chunk sequence"))?;
        let entries = entry_chunks
            .iter()
            .map(Entry::parse)
            .collect::<Result<Vec<_>, _>>()
            .map_err(Error::Entry)?;
        let volumes = kids
            .iter()
            .filter(|c| c.name() == "TEXTURES")
            .map(|c| {
                let (prefix, data) = c
                    .payload
                    .split_first_chunk::<VOLUME_PREFIX_LEN>()
                    .ok_or(Error::Layout("TEXTURES shorter than 8"))?;
                let u32_at = |o: usize| {
                    u32::from_le_bytes([prefix[o], prefix[o + 1], prefix[o + 2], prefix[o + 3]])
                };
                Ok(Volume {
                    group: u32_at(0),
                    frames: u32_at(4),
                    data,
                })
            })
            .collect::<Result<Vec<_>, Error>>()?;
        Ok(Self {
            colors,
            entry_chunks,
            entries,
            volumes,
        })
    }

    /// The volume of zoom group `group`.
    pub fn volume(&self, group: u32) -> Option<&Volume<'a>> {
        self.volumes.iter().find(|v| v.group == group)
    }

    /// Entry with `id`.
    pub fn entry(&self, id: u32) -> Option<&Entry<'a>> {
        self.entries.iter().find(|e| e.id == id)
    }
}
