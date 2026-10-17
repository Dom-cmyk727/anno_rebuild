//! Font corpus test over all 11 `.zei`. Every figure asserted here is
//! recorded in `docs/formats/zei.md`.

mod common;

use anno_formats::tex;
use anno_formats::zei::CharSet;
use common::{files_with_ext, game_dir};

#[test]
fn zei_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping zei_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut files = Vec::new();
    files_with_ext(&dir, "zei", &mut files);
    files.sort();
    assert_eq!(files.len(), 11);
    // (file, size_a, size_b, rows of every glyph)
    let expect = [
        ("HelpNormal.zei", 21, 21, 25),
        ("HelpNormal8.zei", 21, 21, 25),
        ("MainLarge.zei", 31, 31, 37),
        ("MainLarge8.zei", 28, 28, 33),
        ("MainMedium.zei", 21, 21, 25),
        ("MainMedium8.zei", 13, 17, 17),
        ("MainSmall.zei", 21, 21, 25),
        ("MainSmall8.zei", 10, 13, 13),
        ("ScapeMedium.zei", 16, 21, 21),
        ("ScapeSmall.zei", 11, 17, 17),
        ("Startup.zei", 22, 24, 29),
    ];
    let mut reencoded = 0;
    for path in &files {
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        let buf = std::fs::read(path).expect("readable");
        let cs = CharSet::parse(&buf).unwrap_or_else(|e| panic!("{name}: {e}"));
        assert_eq!(cs.to_bytes(), buf, "{name}: round trip");
        assert_eq!(cs.reserved, [0; 12], "{name}");
        // Bytes after the NUL of the name fields: set only in the two
        // `Textures/Scape*.zei`, zero in the nine `ToolGfx` fonts.
        let tails: Vec<u8> = cs
            .name_fields
            .iter()
            .flat_map(|f| {
                let len = f.iter().position(|&b| b == 0).expect("NUL");
                f[len..].to_vec()
            })
            .collect();
        if name.starts_with("Scape") {
            let mut expect = vec![0, 0xe0, 0xe1, 0xe2, 0xe3];
            expect.extend([0, 0, 0, 0, 16, 0, 0, 0].repeat(2));
            assert_eq!(tails, expect, "{name}");
        } else {
            assert!(tails.iter().all(|&b| b == 0), "{name}");
        }
        let (_, a, b, rows) = *expect.iter().find(|e| e.0 == name).expect("listed");
        assert_eq!((cs.size_a, cs.size_b), (a, b), "{name}");
        // Codes 32–255 have glyphs, 0–31 none.
        for g in &cs.glyphs {
            assert_eq!(g.data.is_empty(), g.code < 32, "{name} code {}", g.code);
            assert_eq!(g.width == 0, g.code < 32, "{name} code {}", g.code);
            if g.data.is_empty() {
                continue;
            }
            let img = g.decode().unwrap_or_else(|e| panic!("{name}: {e}"));
            assert_eq!(img.height, rows, "{name} code {}", g.code);
            assert_eq!(
                img.width, g.width as usize,
                "{name} code {}: no pixel past the width",
                g.code
            );
            assert!(img.levels.iter().flatten().all(|&v| (1..=3).contains(&v)));
            // Same RLE as `.tex` BSH_RGB: does the encoder reproduce the bytes?
            if tex::encode_rle(&img.levels, img.width, img.height) == g.data {
                reencoded += 1;
            }
        }
    }
    // The `.tex` RLE encoder reproduces every glyph except those of the two
    // fonts whose rows end with a skip past the width (2 × 224): same RLE.
    assert_eq!(reencoded, 11 * 224 - 2 * 224);
}
