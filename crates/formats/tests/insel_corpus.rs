//! Island corpus test over all `.scp` templates and `.sze` scenarios. Every
//! figure asserted here is recorded in `docs/formats/insel.md`.

mod common;

use std::collections::BTreeMap;

use anno_formats::insel::{self, Island, Version};
use common::{files_with_ext, game_dir};

/// Checks one island; returns its number of tiles.
fn check(island: &Island<'_>, ctx: &str) -> usize {
    let (w, h) = (usize::from(island.width), usize::from(island.height));
    let heights = island
        .heights()
        .unwrap_or_else(|e| panic!("{ctx}: HEIGHT {e}"));
    let gfx = island
        .gfx_ids()
        .unwrap_or_else(|e| panic!("{ctx}: GFXID {e}"));
    // Byte-identical re-encoding of both grids, block by block, in file order.
    let raw = |name| -> Vec<&[u8]> {
        island
            .records
            .iter()
            .filter(|r| r.name() == name)
            .map(|r| r.payload)
            .collect()
    };
    let enc = insel::encode_blocks(&heights, w, h, 1);
    assert_eq!(
        raw("HEIGHT"),
        enc.iter().map(Vec::as_slice).collect::<Vec<_>>(),
        "{ctx}: HEIGHT"
    );
    let gfx_bytes: Vec<u8> = gfx.iter().flat_map(|g| g.to_le_bytes()).collect();
    let enc = insel::encode_blocks(&gfx_bytes, w, h, 2);
    assert_eq!(
        raw("GFXID"),
        enc.iter().map(Vec::as_slice).collect::<Vec<_>>(),
        "{ctx}: GFXID"
    );
    // Sparse block records: same header, then 6-byte items.
    for name in ["GELAENDE", "PROD2", "FLUESSE"] {
        for b in island.blocks(name).unwrap() {
            assert_eq!(b.data.len() % 6, 0, "{ctx}: {name} items");
            let step = if name == "FLUESSE" { 8 } else { 32 };
            assert_eq!((b.x % step, b.y % step), (0, 0), "{ctx}: {name} origin");
        }
    }
    assert!(island.fertility().is_some(), "{ctx}: FRUCHTBAR is w × h");
    assert_eq!(
        island.record("INFO").map(<[u8]>::len),
        Some(12),
        "{ctx}: INFO"
    );
    w * h
}

#[test]
fn insel_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping insel_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut scp = Vec::new();
    files_with_ext(&dir.join("Scenes"), "scp", &mut scp);
    let mut sze = Vec::new();
    files_with_ext(&dir.join("Scenes"), "sze", &mut sze);
    assert_eq!((scp.len(), sze.len()), (465, 50));

    let (mut v4, mut v5, mut misnamed, mut tiles) = (0, 0, Vec::new(), 0);
    for path in &scp {
        let buf = std::fs::read(path).expect("readable");
        let ctx = path.display().to_string();
        let isl = insel::islands(&buf).unwrap_or_else(|e| panic!("{ctx}: {e}"));
        assert_eq!(isl.len(), 1, "{ctx}: one island per template");
        let i = &isl[0];
        match i.version {
            Version::Insel4 => v4 += 1,
            Version::Insel5 => v5 += 1,
        }
        assert_eq!((i.x, i.y, i.number), (0, 0, 0), "{ctx}: template head");
        // File names carry the size as `_<w>x<h>_`.
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        let size = name.split('_').nth(1).expect("size in name");
        if size != format!("{:03}x{:03}", i.width, i.height) {
            misnamed.push(name);
        }
        tiles += check(i, &ctx);
    }
    assert_eq!((v4, v5), (21, 444));
    misnamed.sort();
    assert_eq!(misnamed, ["D_080x080_0022_2B.scp", "D_080x080_0023_1B.scp"]);
    assert_eq!(tiles, 7_952_640, "template tiles");

    let (mut count, mut numbers) = (0, BTreeMap::<String, Vec<u32>>::new());
    for path in &sze {
        let buf = std::fs::read(path).expect("readable");
        let ctx = path.display().to_string();
        for i in insel::islands(&buf).unwrap_or_else(|e| panic!("{ctx}: {e}")) {
            assert_eq!(i.version, Version::Insel5);
            count += 1;
            check(&i, &ctx);
            numbers.entry(ctx.clone()).or_default().push(i.number);
        }
    }
    assert_eq!(count, 668);
    // Island numbers are unique within a scenario.
    for (ctx, mut n) in numbers {
        let len = n.len();
        n.sort_unstable();
        n.dedup();
        assert_eq!(n.len(), len, "{ctx}: duplicate island number");
    }
}

/// Every `GFXID` names water (texture 0) or a `Textures/Felder.tex` texture,
/// except one 3 × 3 patch of texture 205 (absent from `Felder.tex`) in
/// `N_256x256_0015_5B.scp` and the three scenarios that contain that island.
#[test]
fn ground_textures() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping ground_textures: ANNO1503_GAME_DIR not set");
        return;
    };
    let felder = std::fs::read(dir.join("Textures/Felder.tex")).expect("Felder.tex");
    let felder = anno_formats::tex::TexFile::parse(&felder).expect("parses");
    let ids: std::collections::BTreeSet<u32> = felder.textures.iter().map(|t| t.id).collect();
    let mut files = Vec::new();
    files_with_ext(&dir.join("Scenes"), "scp", &mut files);
    files_with_ext(&dir.join("Scenes"), "sze", &mut files);
    let (mut water, mut ground, mut missing) = (0usize, 0usize, BTreeMap::<String, usize>::new());
    let mut used = std::collections::BTreeSet::new();
    for path in &files {
        let buf = std::fs::read(path).expect("readable");
        for i in insel::islands(&buf).expect("islands") {
            for g in i.gfx_ids().expect("grid") {
                let cell = insel::GroundCell::from_gfx_id(g);
                if cell.texture == 0 {
                    water += 1;
                } else if ids.contains(&u32::from(cell.texture)) {
                    ground += 1;
                    used.insert(cell.texture);
                } else {
                    assert_eq!(cell.texture, 205, "{}", path.display());
                    let name = path.file_name().unwrap().to_string_lossy().to_string();
                    *missing.entry(name).or_default() += 1;
                }
            }
        }
    }
    assert_eq!((water, ground), (3_804_534, 13_402_214));
    assert_eq!((used.len(), ids.len()), (141, 147));
    let expect = [
        "00_1024_easyentry_01.sze",
        "N_256x256_0015_5B.scp",
        "S01_12_01.sze",
        "endless01.sze",
    ];
    assert_eq!(missing, expect.map(|n| (n.to_string(), 9)).into());
}
