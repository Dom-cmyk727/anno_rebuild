//! Island ground as a top-down RGBA image.
//!
//! Mapping (spec: `docs/formats/insel.md`, "Ground"): tile `(tx, ty)` covers
//! `scale × scale` output pixels; output pixel `(i, j)` of the tile shows
//! `GroundCell::texel(i, j, scale)` of the tile's `Textures/Felder.tex`
//! texture through its palette. Open water (texture 0) becomes [`WATER`]; a texture id
//! missing from `Felder.tex` becomes [`MISSING`]. This is a top-down
//! inspection view, not the game's isometric projection, and `HEIGHT` is
//! not applied.

use std::collections::BTreeMap;

use anno_formats::insel::{GroundCell, Island};
use anno_formats::tex::{self, TexFile};

use crate::texture::Rgba;

/// Colour for open water (texture 0), which has no ground texture.
pub const WATER: [u8; 4] = [0, 0, 80, 255];
/// Colour for a texture id that `Felder.tex` does not contain.
pub const MISSING: [u8; 4] = [255, 0, 255, 255];
/// Ground textures are 256 × 256.
const TEXTURE_PX: usize = 256;

/// Decoded ground textures by id: RGB per pixel, row-major 256 × 256.
pub struct GroundTextures {
    textures: BTreeMap<u32, Vec<[u8; 3]>>,
    /// Colour for open water (texture 0); [`WATER`] unless set.
    water: [u8; 4],
}

impl GroundTextures {
    /// Decodes every 256 × 256 texture of `Felder.tex` with its palette.
    pub fn new(felder: &TexFile<'_>) -> Result<Self, tex::Error> {
        let mut textures = BTreeMap::new();
        for t in &felder.textures {
            if (usize::from(t.head.width), usize::from(t.head.height)) != (TEXTURE_PX, TEXTURE_PX) {
                continue;
            }
            let Some(palette) = felder.palette(t.head.palette_id) else {
                continue;
            };
            let rgb = t
                .decode()?
                .iter()
                .map(|p| p.map_or([0, 0, 0], |i| palette.rgb(i)))
                .collect();
            textures.insert(t.id, rgb);
        }
        Ok(Self {
            textures,
            water: WATER,
        })
    }

    /// Sets the colour drawn for open water (texture 0), e.g. the average
    /// colour of the sea sprite (`anno_render::iso::water_colour`).
    pub fn set_water(&mut self, rgba: [u8; 4]) {
        self.water = rgba;
    }

    /// Colour shown at tile position `(s/n, t/n)` of a tile with ground `cell`.
    pub fn cell_pixel(&self, cell: GroundCell, s: usize, t: usize, n: usize) -> [u8; 4] {
        if cell.texture == 0 {
            return self.water;
        }
        match self.textures.get(&u32::from(cell.texture)) {
            Some(rgb) => {
                let (sx, sy) = cell.texel(s, t, n);
                let [r, g, b] = rgb[sy * TEXTURE_PX + sx];
                [r, g, b, 255]
            }
            None => MISSING,
        }
    }
}

/// Renders the island's `GFXID` grid with `scale` pixels per tile (1–32).
pub fn ground_top_down(
    island: &Island<'_>,
    textures: &GroundTextures,
    scale: u16,
) -> Result<Rgba, anno_formats::insel::Error> {
    let gfx = island.gfx_ids()?;
    let (w, s) = (usize::from(island.width), usize::from(scale.clamp(1, 32)));
    let out_w = w * s;
    let out_h = usize::from(island.height) * s;
    let mut pixels = vec![0; out_w * out_h * 4];
    for (t, &g) in gfx.iter().enumerate() {
        let cell = GroundCell::from_gfx_id(g);
        let (tx, ty) = (t % w, t / w);
        for j in 0..s {
            for i in 0..s {
                let px = textures.cell_pixel(cell, i, j, s);
                let o = ((ty * s + j) * out_w + tx * s + i) * 4;
                pixels[o..o + 4].copy_from_slice(&px);
            }
        }
    }
    let dim = |n: usize| u16::try_from(n).expect("image fits u16");
    Ok(Rgba {
        width: dim(out_w),
        height: dim(out_h),
        pixels,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use anno_formats::insel::{self, Island};

    fn rec(name: &str, payload: &[u8]) -> Vec<u8> {
        let mut out = vec![0; 12];
        out[..name.len()].copy_from_slice(name.as_bytes());
        out.extend_from_slice(&u32::try_from(payload.len()).unwrap().to_le_bytes());
        out.extend_from_slice(payload);
        out
    }

    #[test]
    fn renders_cells_water_and_missing() {
        // Felder-like file: palette 0 (index i = [B=i, G=0, R=0]) and a
        // 256 × 256 TEX_RGB texture 11 whose pixel (x, y) has index x ^ y.
        let pal: Vec<u8> = (0..=255u8).flat_map(|i| [i, 0, 0, 0]).collect();
        let color = rec("COLOR", &[&[0u8; 8][..], &rec("BODY", &pal)].concat());
        let mut head = vec![0, 1, 0, 1, 0, 0, 8, 0, 0, 0];
        head.extend_from_slice(b"TEX_RGB\0\0\0");
        head.extend_from_slice(&[0; 16]);
        let body: Vec<u8> = (0..256usize)
            .flat_map(|y| (0..256usize).map(move |x| u8::try_from(x ^ y).unwrap()))
            .collect();
        let texture = rec(
            "TEXTURE",
            &[
                &[0, 0, 0, 0, 11, 0, 0, 0][..],
                &rec("HEAD", &head),
                &rec("BODY", &body),
            ]
            .concat(),
        );
        let felder_buf = rec("ANNO", &[color, texture].concat());
        let felder = TexFile::parse(&felder_buf).unwrap();
        let textures = GroundTextures::new(&felder).unwrap();
        // 3 × 1 island: texture 11 cell (1, 2) rotated once, water, texture 99.
        let cell = GroundCell {
            texture: 11,
            u: 1,
            v: 2,
            rotation: 1,
        };
        let gfx: Vec<u8> = [cell.to_gfx_id(), 0, 99 << 8]
            .iter()
            .flat_map(|g| g.to_le_bytes())
            .collect();
        let heights = [0u8; 3];
        let mut payload = [0u16, 0, 3, 1]
            .iter()
            .flat_map(|v| v.to_le_bytes())
            .collect::<Vec<_>>();
        payload.extend_from_slice(&0u32.to_le_bytes());
        payload.extend(rec("HEIGHT", &insel::encode_blocks(&heights, 3, 1, 1)[0]));
        payload.extend(rec("GFXID", &insel::encode_blocks(&gfx, 3, 1, 2)[0]));
        let island = Island::from_insel5(&payload, 0).unwrap();
        let img = ground_top_down(&island, &textures, 32).unwrap();
        assert_eq!((img.width, img.height), (96, 32));
        // Output (0, 0) of tile 0 is tile position (0, 0) of 32 → rotated
        // (31, 0) → texture x (32 + 31)·256/192 = 84, y 64·256/192 = 85.
        let want = 84 ^ 85;
        assert_eq!(&img.pixels[0..4], &[0, 0, want, 255]);
        assert_eq!(&img.pixels[32 * 4..32 * 4 + 4], &WATER);
        assert_eq!(&img.pixels[64 * 4..64 * 4 + 4], &MISSING);
    }
}
