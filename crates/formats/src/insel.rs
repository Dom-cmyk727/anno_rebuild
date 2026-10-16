//! Islands (`INSEL5`, legacy `INSEL4`) in island templates (`.scp`) and
//! scenarios (`.sze`).
//!
//! Spec and evidence: `docs/formats/insel.md`. Summary (all P = checked over
//! all 465 `.scp` and the 668 islands in the 50 `.sze` of GOG 2.0.0.5,
//! `crates/formats/tests/insel_corpus.rs`):
//!
//! - `INSEL5` payload = 12-byte head (`u16 x, u16 y, u16 width, u16 height,
//!   u32 number`; position and number are 0 in templates) + chunk records.
//! - `INSEL4` (21 `*_DUMMY.scp`): 8-byte payload (`u16 width, u16 height,
//!   u32 0`); the records follow as siblings in the root.
//! - `HEIGHT` and `GFXID` hold the terrain in blocks: 6-byte block header
//!   (`u16 x, u16 y, u8 width, u8 height`) + `width × height` cells of 1
//!   (`HEIGHT`) or 2 (`GFXID`) bytes, row-major. Blocks are 32 × 32 (smaller
//!   at the right/bottom edge), stored row of blocks by row of blocks, and
//!   tile the island exactly.
//! - `FRUCHTBAR`: one byte per tile, row-major, no blocks.
//! - A `GFXID` value selects ground: `texture << 8 | u << 5 | v << 2 | r`,
//!   cell `(u, v)` of a 6 × 6 grid over a 256 × 256 `Textures/Felder.tex`
//!   texture, turned by `r` quarter turns; texture 0 = open water
//!   ([`GroundCell`]).
//! - `GELAENDE`, `PROD2`, `FLUESSE` blocks hold 6-byte items that place
//!   `Anlagen.dat` objects ([`PlacedObject`]).

use std::fmt;

use crate::chunk::{self, Chunk};

/// Edge length of a full terrain block.
pub const BLOCK: usize = 32;
/// Size of a block header.
pub const BLOCK_HEADER_LEN: usize = 6;
/// Size of the `INSEL5` head.
pub const INSEL5_HEAD_LEN: usize = 12;
/// Size of the `INSEL4` payload.
pub const INSEL4_HEAD_LEN: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// No island root (`INSEL5` / `INSEL4`) where one was expected.
    NoIsland,
    /// A record does not have the layout above; `what` names the check.
    Bad { what: &'static str, offset: usize },
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::NoIsland => write!(f, "no INSEL5/INSEL4 island"),
            Error::Bad { what, offset } => write!(f, "malformed {what} at {offset}"),
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Version {
    Insel4,
    Insel5,
}

/// One block record (`HEIGHT`, `GFXID`, `GELAENDE`, `PROD2`, `FLUESSE`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Block<'a> {
    /// Tile position of the block's top-left corner.
    pub x: u16,
    pub y: u16,
    pub width: u8,
    pub height: u8,
    /// Everything after the 6-byte header.
    pub data: &'a [u8],
}

impl<'a> Block<'a> {
    pub fn parse(payload: &'a [u8], offset: usize) -> Result<Self, Error> {
        if payload.len() < BLOCK_HEADER_LEN {
            return Err(Error::Bad {
                what: "block header",
                offset,
            });
        }
        Ok(Self {
            x: u16_at(payload, 0),
            y: u16_at(payload, 2),
            width: payload[4],
            height: payload[5],
            data: &payload[BLOCK_HEADER_LEN..],
        })
    }

    pub fn header_bytes(&self) -> [u8; BLOCK_HEADER_LEN] {
        let [x0, x1] = self.x.to_le_bytes();
        let [y0, y1] = self.y.to_le_bytes();
        [x0, x1, y0, y1, self.width, self.height]
    }
}

/// An island: head fields plus its records in file order.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Island<'a> {
    pub version: Version,
    /// Raw head: the 12-byte `INSEL5` head or the 8-byte `INSEL4` payload.
    pub head: &'a [u8],
    /// World position of the top-left tile (`INSEL5` 0x00/0x02; 0 in templates and `INSEL4`).
    pub x: u16,
    pub y: u16,
    pub width: u16,
    pub height: u16,
    /// `INSEL5` 0x08 / `INSEL4` 0x04: island number in a scenario, 0 in templates.
    pub number: u32,
    /// Records (`INFO`, `HEIGHT`, `GFXID`, …) in file order.
    pub records: Vec<Chunk<'a>>,
}

impl<'a> Island<'a> {
    /// Parses an `INSEL5` payload. `offset` is only used in errors.
    pub fn from_insel5(payload: &'a [u8], offset: usize) -> Result<Self, Error> {
        let bad = |what| Error::Bad { what, offset };
        let (head, rest) = payload
            .split_at_checked(INSEL5_HEAD_LEN)
            .ok_or(bad("INSEL5 head"))?;
        let records = chunk::parse_chunks(rest).ok_or(bad("INSEL5 records"))?;
        Ok(Self {
            version: Version::Insel5,
            head,
            x: u16_at(head, 0),
            y: u16_at(head, 2),
            width: u16_at(head, 4),
            height: u16_at(head, 6),
            number: u32_at(head, 8),
            records,
        })
    }

    /// Builds an `INSEL4` island from a root's children: the `INSEL4`
    /// chunk and all chunks after it.
    pub fn from_insel4(root_children: &[Chunk<'a>]) -> Result<Self, Error> {
        let i = root_children
            .iter()
            .position(|c| c.name() == "INSEL4")
            .ok_or(Error::NoIsland)?;
        let c = &root_children[i];
        if c.payload.len() != INSEL4_HEAD_LEN {
            return Err(Error::Bad {
                what: "INSEL4 size",
                offset: c.offset,
            });
        }
        Ok(Self {
            version: Version::Insel4,
            head: c.payload,
            x: 0,
            y: 0,
            width: u16_at(c.payload, 0),
            height: u16_at(c.payload, 2),
            number: u32_at(c.payload, 4),
            records: root_children[i + 1..].to_vec(),
        })
    }

    /// All records called `name`, parsed as blocks, in file order.
    pub fn blocks(&self, name: &str) -> Result<Vec<Block<'a>>, Error> {
        self.records
            .iter()
            .filter(|r| r.name() == name)
            .map(|r| Block::parse(r.payload, r.offset))
            .collect()
    }

    /// Payload of the first record called `name`.
    pub fn record(&self, name: &str) -> Option<&'a [u8]> {
        self.records
            .iter()
            .find(|r| r.name() == name)
            .map(|r| r.payload)
    }

    /// Assembles the `HEIGHT` blocks: one byte per tile, row-major.
    pub fn heights(&self) -> Result<Vec<u8>, Error> {
        self.grid("HEIGHT", 1)
            .map(|cells| cells.into_iter().map(|c| c[0]).collect())
    }

    /// Assembles the `GFXID` blocks: one `u16` per tile, row-major.
    pub fn gfx_ids(&self) -> Result<Vec<u16>, Error> {
        self.grid("GFXID", 2).map(|cells| {
            cells
                .into_iter()
                .map(|c| u16::from_le_bytes([c[0], c[1]]))
                .collect()
        })
    }

    /// `FRUCHTBAR`: one byte per tile, row-major, if present with that size.
    pub fn fertility(&self) -> Option<&'a [u8]> {
        self.record("FRUCHTBAR")
            .filter(|f| f.len() == usize::from(self.width) * usize::from(self.height))
    }

    /// Assembles fixed-size cells from blocks; every tile must be covered
    /// exactly once.
    fn grid(&self, name: &str, cell: usize) -> Result<Vec<[u8; 2]>, Error> {
        let (w, h) = (usize::from(self.width), usize::from(self.height));
        let mut out = vec![[0u8; 2]; w * h];
        let mut covered = vec![false; w * h];
        let bad = |what| Error::Bad { what, offset: 0 };
        for b in self.blocks(name)? {
            let (bx, by) = (usize::from(b.x), usize::from(b.y));
            let (bw, bh) = (usize::from(b.width), usize::from(b.height));
            if b.data.len() != bw * bh * cell {
                return Err(bad("block data size"));
            }
            if bx + bw > w || by + bh > h {
                return Err(bad("block outside island"));
            }
            for j in 0..bh {
                for i in 0..bw {
                    let t = (by + j) * w + bx + i;
                    if std::mem::replace(&mut covered[t], true) {
                        return Err(bad("tile covered twice"));
                    }
                    let src = &b.data[(j * bw + i) * cell..][..cell];
                    out[t][..cell].copy_from_slice(src);
                }
            }
        }
        if covered.iter().any(|c| !c) {
            return Err(bad("tile not covered"));
        }
        Ok(out)
    }
}

/// Splits a row-major grid of `cell`-byte values into block payloads in
/// file order (rows of 32 × 32 blocks, smaller at the edges), as written
/// for `HEIGHT` (`cell` = 1) and `GFXID` (`cell` = 2, little-endian).
pub fn encode_blocks(grid: &[u8], width: usize, height: usize, cell: usize) -> Vec<Vec<u8>> {
    assert_eq!(grid.len(), width * height * cell, "grid size");
    let mut out = Vec::new();
    for by in (0..height).step_by(BLOCK) {
        for bx in (0..width).step_by(BLOCK) {
            let (bw, bh) = (BLOCK.min(width - bx), BLOCK.min(height - by));
            let header = Block {
                x: u16::try_from(bx).expect("island fits u16"),
                y: u16::try_from(by).expect("island fits u16"),
                width: u8::try_from(bw).expect("block fits u8"),
                height: u8::try_from(bh).expect("block fits u8"),
                data: &[],
            };
            let mut p = header.header_bytes().to_vec();
            for j in 0..bh {
                let start = ((by + j) * width + bx) * cell;
                p.extend_from_slice(&grid[start..start + bw * cell]);
            }
            out.push(p);
        }
    }
    out
}

/// Ground cells per texture edge: a cell spans 1/6 of the texture (D:
/// `AnnoWelt.dll` UV table `0x100DCF44` = [0, 1/6, …, 1]; P: `u`, `v` ≤ 5 on
/// all but 230 of 13.4 M ground tiles; seam error between neighbouring tiles
/// is less than half that of 1/8 cells).
pub const CELLS_PER_TEXTURE: usize = 6;
/// Edge length of a ground texture in pixels.
pub const GROUND_TEXTURE_PX: usize = 256;

/// The ground a `GFXID` value selects (spec: `docs/formats/insel.md`,
/// "Ground"; evidence: corpus statistics and rendering, OQ-024).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GroundCell {
    /// `Textures/Felder.tex` texture id; 0 = open water (no texture).
    pub texture: u8,
    /// Cell column (0–5; 3 bits, 6 and 7 occur on 230 tiles) in the texture.
    pub u: u8,
    /// Cell row (0–5).
    pub v: u8,
    /// Quarter turns (0–3), see [`GroundCell::texel`].
    pub rotation: u8,
}

impl GroundCell {
    pub fn from_gfx_id(gfx: u16) -> Self {
        let [low, texture] = gfx.to_le_bytes();
        Self {
            texture,
            u: low >> 5,
            v: (low >> 2) & 7,
            rotation: low & 3,
        }
    }

    pub fn to_gfx_id(self) -> u16 {
        u16::from_le_bytes([self.u << 5 | self.v << 2 | self.rotation, self.texture])
    }

    /// Texture pixel shown at tile position `(s/n, t/n)` (`0 ≤ s, t < n`;
    /// tile `x` runs along `s`, tile `y` along `t`): apply
    /// `(s, t) → (n − 1 − t, s)` `rotation` times, then map the cell
    /// `[u, u+1) × [v, v+1)` (in sixths of the texture) onto texture pixels.
    /// Cells 6 and 7 (230 tiles) clamp to the texture edge.
    pub fn texel(self, s: usize, t: usize, n: usize) -> (usize, usize) {
        let (mut a, mut b) = (s, t);
        for _ in 0..self.rotation {
            (a, b) = (n - 1 - b, a);
        }
        let px = |cell: u8, f: usize| {
            let p = (usize::from(cell) * n + f) * GROUND_TEXTURE_PX / (CELLS_PER_TEXTURE * n);
            p.min(GROUND_TEXTURE_PX - 1)
        };
        (px(self.u, a), px(self.v, b))
    }
}

/// Size of one item in `GELAENDE`, `PROD2` and `FLUESSE` blocks.
pub const ITEM_LEN: usize = 6;

/// One 6-byte block item: an `Anlagen.dat` object placed on the island
/// (spec: `docs/formats/insel.md`, "Placed objects").
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct PlacedObject {
    /// `ANLAGEN` entry id (bits 0–11 of the first `u16`).
    pub anlage: u16,
    /// Quarter turns (bits 14–15); odd values swap the footprint.
    pub rotation: u8,
    /// Bits 12–13 of the first `u16`; 0 in the whole corpus.
    pub reserved: u8,
    /// Second `u16`: `height << 8 | phase` (bits 6–7 always 0), see
    /// [`PlacedObject::height`], [`PlacedObject::phase`].
    pub state: u16,
    /// Island tile of the footprint's top-left corner (block origin + item offset).
    pub x: u16,
    pub y: u16,
}

impl PlacedObject {
    /// Parses one item of `block`.
    pub fn parse(item: &[u8; ITEM_LEN], block: &Block<'_>) -> Self {
        let id = u16::from_le_bytes([item[0], item[1]]);
        Self {
            anlage: id & 0x0FFF,
            rotation: u8::try_from(id >> 14).expect("2 bits"),
            reserved: u8::try_from((id >> 12) & 3).expect("2 bits"),
            state: u16::from_le_bytes([item[2], item[3]]),
            x: block.x + u16::from(item[4]),
            y: block.y + u16::from(item[5]),
        }
    }

    /// The object's height in `HEIGHT` units (high byte of `state`). D:
    /// `AnnoWelt.dll` loader `0x1007aa40` / saver `0x1007ac40`; drawn at
    /// `height · 0.04` world units (`0x1009f870`). P: equals the terrain
    /// height at the top-left tile for 90 % of `GELAENDE` and 97 % of `PROD2`
    /// items, but only 25 % of `FLUESSE` items (water objects keep their own
    /// level, e.g. surf at 0).
    pub fn height(&self) -> u8 {
        self.state.to_be_bytes()[0]
    }

    /// Animation phase (low 6 bits of `state`); D: as [`PlacedObject::height`].
    pub fn phase(&self) -> u8 {
        self.state.to_le_bytes()[0] & 0x3F
    }

    /// `(width, height)` on the island for an unrotated footprint `(w, h)`.
    pub fn footprint(&self, w: u8, h: u8) -> (u8, u8) {
        if self.rotation % 2 == 1 {
            (h, w)
        } else {
            (w, h)
        }
    }
}

impl<'a> Island<'a> {
    /// All items of the block records called `name` (`GELAENDE`, `PROD2`,
    /// `FLUESSE`) in file order, duplicates included.
    pub fn objects(&self, name: &str) -> Result<Vec<PlacedObject>, Error> {
        let mut out = Vec::new();
        for b in self.blocks(name)? {
            let (items, rest) = b.data.as_chunks::<ITEM_LEN>();
            if !rest.is_empty() {
                return Err(Error::Bad {
                    what: "item list length",
                    offset: 0,
                });
            }
            out.extend(items.iter().map(|i| PlacedObject::parse(i, &b)));
        }
        Ok(out)
    }
}

/// All islands of an `.scp` or `.sze` buffer, in file order: `INSEL5`
/// children of the root (`.scp`) or of `SCENE` (`.sze`), or one `INSEL4`
/// island (legacy `.scp`).
pub fn islands(buf: &[u8]) -> Result<Vec<Island<'_>>, Error> {
    let file = chunk::parse_file(buf).map_err(|_| Error::NoIsland)?;
    let root = file.chunks.first().ok_or(Error::NoIsland)?;
    let kids = root.children.as_deref().ok_or(Error::NoIsland)?;
    if kids.iter().any(|c| c.name() == "INSEL4") {
        return Ok(vec![Island::from_insel4(kids)?]);
    }
    let parent = match kids.iter().find(|c| c.name() == "SCENE") {
        Some(scene) => scene.children.as_deref().ok_or(Error::NoIsland)?,
        None => kids,
    };
    parent
        .iter()
        .filter(|c| c.name() == "INSEL5")
        .map(|c| Island::from_insel5(c.payload, c.offset))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rec(name: &str, payload: &[u8]) -> Vec<u8> {
        let mut out = chunk::name_field(name).unwrap().to_vec();
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    /// A 40 × 33 island: blocks 32×32, 8×32, 32×1, 8×1.
    fn island() -> (Vec<u8>, Vec<u8>, Vec<u16>) {
        let (w, h) = (40, 33);
        let heights: Vec<u8> = (0..w * h).map(|i| u8::try_from(i % 251).unwrap()).collect();
        let gfx: Vec<u16> = (0..w * h).map(|i| u16::try_from(i * 7).unwrap()).collect();
        let gfx_bytes: Vec<u8> = gfx.iter().flat_map(|g| g.to_le_bytes()).collect();
        let mut payload = [
            &5u16.to_le_bytes()[..],
            &6u16.to_le_bytes(),
            &40u16.to_le_bytes(),
            &33u16.to_le_bytes(),
            &3u32.to_le_bytes(),
        ]
        .concat();
        payload.extend(rec("INFO", &[0; 12]));
        for b in encode_blocks(&heights, w, h, 1) {
            payload.extend(rec("HEIGHT", &b));
        }
        for b in encode_blocks(&gfx_bytes, w, h, 2) {
            payload.extend(rec("GFXID", &b));
        }
        payload.extend(rec("FRUCHTBAR", &vec![1; w * h]));
        (payload, heights, gfx)
    }

    #[test]
    fn insel5_grids() {
        let (payload, heights, gfx) = island();
        let isl = Island::from_insel5(&payload, 0).unwrap();
        assert_eq!(
            (isl.x, isl.y, isl.width, isl.height, isl.number),
            (5, 6, 40, 33, 3)
        );
        let blocks = isl.blocks("HEIGHT").unwrap();
        let dims: Vec<_> = blocks
            .iter()
            .map(|b| (b.x, b.y, b.width, b.height))
            .collect();
        assert_eq!(
            dims,
            [
                (0, 0, 32, 32),
                (32, 0, 8, 32),
                (0, 32, 32, 1),
                (32, 32, 8, 1)
            ]
        );
        assert_eq!(isl.heights().unwrap(), heights);
        assert_eq!(isl.gfx_ids().unwrap(), gfx);
        assert_eq!(isl.fertility().map(<[u8]>::len), Some(40 * 33));
    }

    #[test]
    fn rejects_gaps_and_overlaps() {
        let (mut payload, _, _) = island();
        // Drop the last HEIGHT block (8×1 at 32,32): one tile row segment uncovered.
        let isl = Island::from_insel5(&payload, 0).unwrap();
        let last = isl.records.iter().rfind(|r| r.name() == "HEIGHT").unwrap();
        let (start, len) = (last.offset, last.encoded_len());
        payload.drain(INSEL5_HEAD_LEN + start..INSEL5_HEAD_LEN + start + len);
        let isl = Island::from_insel5(&payload, 0).unwrap();
        assert!(isl.heights().is_err());
        assert!(isl.gfx_ids().is_ok());
    }

    #[test]
    fn ground_cells() {
        // 0x2c48: texture 44, low 0x48 = u 2, v 2, r 0.
        let c = GroundCell::from_gfx_id(0x2c48);
        assert_eq!((c.texture, c.u, c.v, c.rotation), (44, 2, 2, 0));
        assert_eq!(c.to_gfx_id(), 0x2c48);
        // Cell (2, 2) starts at 2/6 of 256 = 85.33 → pixel 85; n = 6 steps per cell.
        assert_eq!(c.texel(0, 0, 6), (85, 85));
        assert_eq!(c.texel(5, 0, 6), (85 + 35, 85));
        let r1 = GroundCell { rotation: 1, ..c };
        // (0, 0) → (n − 1, 0): the cell's right edge.
        assert_eq!(r1.texel(0, 0, 6), (120, 85));
        // (1, 3) → (6 − 1 − 3, 1) = (2, 1) → ((12 + 2)·256/36, (12 + 1)·256/36).
        assert_eq!(r1.texel(1, 3, 6), (99, 92));
        let edge = GroundCell { u: 7, ..c };
        assert_eq!(edge.texel(5, 0, 6).0, 255, "cells 6, 7 clamp");
        let r4 = (0..4).fold((3, 7), |(x, y), _| (31 - y, x));
        assert_eq!(r4, (3, 7), "four quarter turns are the identity");
        assert_eq!(GroundCell::from_gfx_id(0).texture, 0);
    }

    #[test]
    fn placed_objects() {
        // Fisher's hut 3978 = 0x0f8a, rotation 2 → 0x8f8a; state 0x3000; (0, 13)
        // in the block at (32, 0).
        let block = Block::parse(&[32, 0, 0, 0, 32, 32, 0x8a, 0x8f, 0, 0x30, 0, 13], 0).unwrap();
        let objs: Vec<_> = block
            .data
            .as_chunks::<ITEM_LEN>()
            .0
            .iter()
            .map(|i| PlacedObject::parse(i, &block))
            .collect();
        let o = objs[0];
        assert_eq!(
            (o.anlage, o.rotation, o.reserved, o.state, o.x, o.y),
            (3978, 2, 0, 0x3000, 32, 13)
        );
        assert_eq!((o.height(), o.phase()), (0x30, 0));
        let p = PlacedObject { state: 0x1005, ..o };
        assert_eq!((p.height(), p.phase()), (16, 5));
        assert_eq!(o.footprint(3, 4), (3, 4));
        assert_eq!(PlacedObject { rotation: 1, ..o }.footprint(3, 4), (4, 3));
    }

    #[test]
    fn islands_in_scp_and_insel4() {
        let (payload, heights, _) = island();
        let scp = rec(
            "ANNO",
            &[rec("WARETYPE", &[0; 4]), rec("INSEL5", &payload)].concat(),
        );
        let found = islands(&scp).unwrap();
        assert_eq!(found.len(), 1);
        assert_eq!(found[0].heights().unwrap(), heights);
        // INSEL4: records as root siblings.
        let mut root = rec("INSEL4", &[2, 0, 1, 0, 0, 0, 0, 0]);
        root.extend(rec("HEIGHT", &[0, 0, 0, 0, 2, 1, 7, 9]));
        let file = rec("ANNO", &root);
        let found = islands(&file).unwrap();
        assert_eq!(found[0].version, Version::Insel4);
        assert_eq!(found[0].heights().unwrap(), [7, 9]);
    }
}
