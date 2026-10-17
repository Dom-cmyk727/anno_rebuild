//! Scenarios (`.sze`): the `SCENE` chunk, its players (`SPIELER`) and their
//! ships (`SCHIFF`) and soldiers (`SOLDAT`).
//!
//! Spec and evidence: `docs/formats/sze.md`. Summary (P = checked over all 50
//! `.sze` of GOG 2.0.0.5, `crates/formats/tests/scene_corpus.rs`):
//!
//! - Root `ANNO` (base game) or `ANNOADDON1` (add-on) holds one `SCENE`
//!   container; its children are generic chunks (`HEAD`, `INSEL5`,
//!   `SPIELER`, …).
//! - `SPIELER` payload = `u32 0`, `u32 index`, then tagged records
//!   ([`crate::tagged`]). Each scenario lists its players twice: a first
//!   series whose blocks hold `HEAD` (and `VOLK`, `PROFIL`, …), then a second
//!   series with the same indices whose blocks hold the game state (`SCHIFF`,
//!   `SOLDAT`, `STADT2`, `SEL_0`…`SEL_7`, …).
//! - `SCHIFF` and `SOLDAT` payload = 16-byte [`Unit`] prefix, then tagged
//!   records (`HEAD`, `WAREN`, `PRIVAT`, `TASK`).

use std::fmt;

use crate::chunk::{self, Chunk};
use crate::tagged::{self, Record};

/// Size of the `SPIELER` prefix (`u32 flag`, `u32 index`).
pub const PLAYER_PREFIX_LEN: usize = 8;
/// Size of the `SCHIFF` / `SOLDAT` prefix.
pub const UNIT_PREFIX_LEN: usize = 16;

/// Records whose payload is a fixed prefix of this many bytes followed by a
/// nested record sequence, keyed by (parent record, record); the top level
/// is `SPIELER`. Every instance in the corpus parses exactly with these
/// prefixes; all other records are leaves as far as known. Table and
/// counts: `docs/formats/sze.md`, "Nesting".
pub fn nested_prefix(parent: &str, name: &str) -> Option<usize> {
    match (parent, name) {
        ("SPIELER", "SCHIFF" | "SOLDAT") => Some(UNIT_PREFIX_LEN),
        ("SPIELER", "STADT2") => Some(6),
        ("SPIELER", "DIPL" | "FORSCH") => Some(0),
        ("STADT2", "PROD") => Some(0),
        ("SCHIFF" | "SOLDAT" | "ENTRY", "PRIVAT") => Some(0),
        ("PRIVAT", "CREW" | "ROUTE") => Some(0),
        ("PRIVAT", "ESEL") => Some(8),
        ("CREW", "ENTRY") => Some(6),
        _ => None,
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// No `ANNO`/`ANNOADDON1` root with a `SCENE` container.
    NoScene,
    /// A record sequence does not parse.
    Tagged(tagged::Error),
    /// A payload is shorter than its fixed prefix.
    Short { what: &'static str, offset: usize },
    /// A string field is not NUL-terminated or holds an inner NUL.
    BadString { what: &'static str, offset: usize },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoScene => write!(f, "no ANNO/SCENE or ANNOADDON1/SCENE container"),
            Error::Tagged(e) => write!(f, "{e}"),
            Error::Short { what, offset } => {
                write!(f, "{what} at {offset} shorter than its prefix")
            }
            Error::BadString { what, offset } => write!(f, "{what} at {offset}: bad string"),
        }
    }
}

impl std::error::Error for Error {}

impl From<tagged::Error> for Error {
    fn from(e: tagged::Error) -> Self {
        Error::Tagged(e)
    }
}

fn u16_at(b: &[u8], o: usize) -> u16 {
    u16::from_le_bytes([b[o], b[o + 1]])
}

fn u32_at(b: &[u8], o: usize) -> u32 {
    u32::from_le_bytes([b[o], b[o + 1], b[o + 2], b[o + 3]])
}

/// Which game wrote the scenario, from the root chunk name.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Edition {
    /// Root `ANNO`.
    Base,
    /// Root `ANNOADDON1`.
    Addon,
}

/// A parsed scenario: the children of its `SCENE` chunk.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Scene<'a> {
    pub edition: Edition,
    /// `SCENE` children in file order; offsets are absolute in the file.
    pub children: Vec<Chunk<'a>>,
}

impl<'a> Scene<'a> {
    pub fn parse(buf: &'a [u8]) -> Result<Self, Error> {
        let file = chunk::parse_file(buf).map_err(|_| Error::NoScene)?;
        let root = file.chunks.first().ok_or(Error::NoScene)?;
        let edition = match root.name() {
            "ANNO" => Edition::Base,
            "ANNOADDON1" => Edition::Addon,
            _ => return Err(Error::NoScene),
        };
        let scene = root
            .children
            .as_ref()
            .and_then(|k| k.iter().find(|c| c.name() == "SCENE"))
            .ok_or(Error::NoScene)?;
        let children = scene.children.clone().ok_or(Error::NoScene)?;
        Ok(Self { edition, children })
    }

    /// All `SCENE` children called `name`, in file order.
    pub fn chunks<'s>(&'s self, name: &'s str) -> impl Iterator<Item = &'s Chunk<'a>> + 's {
        self.children.iter().filter(move |c| c.name() == name)
    }

    /// All `INSELLNK` records in file order.
    pub fn island_links(&self) -> Result<Vec<IslandLink<'a>>, Error> {
        self.chunks("INSELLNK")
            .map(|c| IslandLink::parse(c.payload, c.offset))
            .collect()
    }

    /// All `SPIELER` blocks in file order (both series).
    pub fn players(&self) -> Result<Vec<Player<'a>>, Error> {
        self.chunks("SPIELER")
            .map(|c| Player::parse(c.payload, c.offset + chunk::HEADER_LEN))
            .collect()
    }
}

/// An island placed by reference (`INSELLNK`, add-on only): the 12-byte
/// `INSEL5` head (position, size, island number), then the NUL-terminated
/// path of an `INSEL4` `*_dummy.scp` template relative to `Scenes`, whose
/// size equals `width × height` in all 229 links. Which island the game
/// puts there is open (OQ-028).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IslandLink<'a> {
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
    pub number: u32,
    /// The path without its NUL, raw (Windows-1252, `\` separators).
    pub path: &'a [u8],
}

impl<'a> IslandLink<'a> {
    pub fn parse(payload: &'a [u8], offset: usize) -> Result<Self, Error> {
        let what = "INSELLNK";
        let (head, path) = payload
            .split_at_checked(crate::insel::INSEL5_HEAD_LEN)
            .ok_or(Error::Short { what, offset })?;
        // Exactly one NUL, at the end.
        let path = match path.split_last() {
            Some((0, p)) if !p.contains(&0) => p,
            _ => return Err(Error::BadString { what, offset }),
        };
        Ok(Self {
            x: u16_at(head, 0),
            y: u16_at(head, 2),
            width: u16_at(head, 4),
            height: u16_at(head, 6),
            number: u32_at(head, 8),
            path,
        })
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let mut out = Vec::new();
        for v in [self.x, self.y, self.width, self.height] {
            out.extend_from_slice(&v.to_le_bytes());
        }
        out.extend_from_slice(&self.number.to_le_bytes());
        out.extend_from_slice(self.path);
        out.push(0);
        out
    }
}

/// One `SPIELER` block.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Player<'a> {
    /// 0x00: 0 in all 724 blocks.
    pub flag: u32,
    /// 0x04: player index (0–30 for players, 31 in every scenario; OQ-027).
    pub index: u32,
    pub records: Vec<Record<'a>>,
}

impl<'a> Player<'a> {
    /// Parses a `SPIELER` payload; `offset` (of the payload in the file)
    /// is used for record offsets and errors.
    pub fn parse(payload: &'a [u8], offset: usize) -> Result<Self, Error> {
        if payload.len() < PLAYER_PREFIX_LEN {
            return Err(Error::Short {
                what: "SPIELER",
                offset,
            });
        }
        Ok(Self {
            flag: u32_at(payload, 0),
            index: u32_at(payload, 4),
            records: tagged::parse_records(
                &payload[PLAYER_PREFIX_LEN..],
                offset + PLAYER_PREFIX_LEN,
            )?,
        })
    }

    pub fn to_bytes(&self) -> Result<Vec<u8>, Error> {
        let mut out = Vec::new();
        out.extend_from_slice(&self.flag.to_le_bytes());
        out.extend_from_slice(&self.index.to_le_bytes());
        tagged::write_records(&self.records, &mut out)?;
        Ok(out)
    }

    /// The records called `name` (`SCHIFF`, `SOLDAT`) parsed as units.
    pub fn units(&self, name: &str) -> Result<Vec<Unit<'a>>, Error> {
        self.records
            .iter()
            .filter(|r| r.name == name)
            .map(|r| Unit::parse(r.payload, r.offset + r.encoded_len() - r.payload.len()))
            .collect()
    }
}

/// A ship (`SCHIFF`) or soldier (`SOLDAT`): 16-byte prefix + records.
///
/// ```text
/// 0x00  u16  id       unique within the scenario
/// 0x02  u16  figure   FigurenHeader.dat entry id (ships 1501–1509)
/// 0x04  u8   owner    = the SPIELER index of the block holding the unit
/// 0x05  u8   attr     ships 0–14, soldiers 0–7; meaning open (OQ-027)
/// 0x06  u16  0
/// 0x08  f32  x        world tile position; tile centres (n + 0.5) for
/// 0x0C  f32  y        310 of 327 ships and 2,253 of 2,317 soldiers
/// ```
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Unit<'a> {
    pub id: u16,
    pub figure: u16,
    pub owner: u8,
    pub attr: u8,
    pub reserved: u16,
    /// IEEE 754 bits of `x` and `y`, kept raw for a byte-identical round trip.
    pub x_bits: u32,
    pub y_bits: u32,
    pub records: Vec<Record<'a>>,
}

impl<'a> Unit<'a> {
    pub fn parse(payload: &'a [u8], offset: usize) -> Result<Self, Error> {
        if payload.len() < UNIT_PREFIX_LEN {
            return Err(Error::Short {
                what: "unit",
                offset,
            });
        }
        Ok(Self {
            id: u16_at(payload, 0),
            figure: u16_at(payload, 2),
            owner: payload[4],
            attr: payload[5],
            reserved: u16_at(payload, 6),
            x_bits: u32_at(payload, 8),
            y_bits: u32_at(payload, 12),
            records: tagged::parse_records(&payload[UNIT_PREFIX_LEN..], offset + UNIT_PREFIX_LEN)?,
        })
    }

    pub fn x(&self) -> f32 {
        f32::from_bits(self.x_bits)
    }

    pub fn y(&self) -> f32 {
        f32::from_bits(self.y_bits)
    }

    pub fn to_bytes(&self) -> Result<Vec<u8>, Error> {
        let mut out = Vec::new();
        out.extend_from_slice(&self.id.to_le_bytes());
        out.extend_from_slice(&self.figure.to_le_bytes());
        out.push(self.owner);
        out.push(self.attr);
        out.extend_from_slice(&self.reserved.to_le_bytes());
        out.extend_from_slice(&self.x_bits.to_le_bytes());
        out.extend_from_slice(&self.y_bits.to_le_bytes());
        tagged::write_records(&self.records, &mut out)?;
        Ok(out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn chunk(name: &str, payload: &[u8]) -> Vec<u8> {
        let mut out = chunk::name_field(name).unwrap().to_vec();
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    #[test]
    fn players_and_ships() {
        let mut ship = vec![7, 0, 0xDE, 0x05, 2, 3, 0, 0];
        ship.extend_from_slice(&10.5f32.to_le_bytes());
        ship.extend_from_slice(&20.5f32.to_le_bytes());
        tagged::write_record("HEAD", false, &[1, 2], &mut ship).unwrap();
        let mut spieler = vec![0, 0, 0, 0, 2, 0, 0, 0];
        tagged::write_record("SCHIFF", false, &ship, &mut spieler).unwrap();
        tagged::write_record("STADT2", true, &[0; 6], &mut spieler).unwrap();
        let scene = chunk(
            "SCENE",
            &[chunk("HEAD", &[0; 8]), chunk("SPIELER", &spieler)].concat(),
        );
        let file = chunk("ANNOADDON1", &scene);

        let s = Scene::parse(&file).unwrap();
        assert_eq!(s.edition, Edition::Addon);
        let players = s.players().unwrap();
        assert_eq!(players.len(), 1);
        let p = &players[0];
        assert_eq!((p.flag, p.index), (0, 2));
        assert_eq!(p.to_bytes().unwrap(), spieler);
        let ships = p.units("SCHIFF").unwrap();
        assert_eq!(ships.len(), 1);
        let u = &ships[0];
        assert_eq!((u.id, u.figure, u.owner, u.attr), (7, 1502, 2, 3));
        assert_eq!((u.x(), u.y()), (10.5, 20.5));
        assert_eq!(u.records[0].name, "HEAD");
        assert_eq!(u.to_bytes().unwrap(), ship);
        // Record offsets point into the file: SPIELER payload + prefix +
        // SCHIFF header + unit prefix.
        let spieler_payload = 16 + 16 + 16 + 8 + 16;
        assert_eq!(u.records[0].offset, spieler_payload + 8 + 8 + 16);
        assert_eq!(&file[u.records[0].offset..][..4], b"HEAD");
    }

    #[test]
    fn rejects_non_scenarios() {
        assert_eq!(
            Scene::parse(&chunk("ANNO", &chunk("HEAD", &[0; 8]))),
            Err(Error::NoScene)
        );
        assert_eq!(
            Scene::parse(&chunk("TEXTURES", &chunk("SCENE", &chunk("HEAD", &[0; 8])))),
            Err(Error::NoScene)
        );
    }
}
