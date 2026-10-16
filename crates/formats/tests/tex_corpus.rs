//! `.tex` / `.inc` corpus test. Every figure asserted here is recorded in
//! `docs/formats/tex.md` and `docs/formats/palette.md`.

mod common;

use std::collections::BTreeMap;

use anno_formats::inc;
use anno_formats::tex::{PixelFormat, TexFile, encode_rle};
use common::{files_with_ext, game_dir};

#[derive(Debug, Default, PartialEq, Eq)]
struct Stats {
    files: usize,
    empty_files: usize,
    palettes: usize,
    textures: usize,
    formats: BTreeMap<String, usize>,
    named: usize,
    /// Palettes whose prefix starts with a non-zero u32.
    flagged_palettes: usize,
    /// RLE textures whose body re-encodes byte-identically.
    rle_reencoded: usize,
}

#[test]
fn tex_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping tex_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut paths = Vec::new();
    files_with_ext(&dir, "tex", &mut paths);
    let mut s = Stats::default();
    for path in &paths {
        let buf = std::fs::read(path).expect("readable");
        let file = TexFile::parse(&buf).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
        let ctx = path.display();
        s.files += 1;
        if file.palettes.is_empty() && file.textures.is_empty() {
            s.empty_files += 1;
        }
        s.palettes += file.palettes.len();
        for pal in &file.palettes {
            if pal.prefix[..4] != [0; 4] {
                // Only value 2, only in ToolGfx/fenster.tex and fenster8.tex (OQ-021).
                assert_eq!(&pal.prefix[..4], &[2, 0, 0, 0], "{ctx}: palette prefix");
                s.flagged_palettes += 1;
            }
            assert!(
                pal.entries.iter().all(|e| e[3] == 0),
                "{ctx}: reserved byte"
            );
        }
        for tex in &file.textures {
            s.textures += 1;
            assert_eq!(&tex.prefix[..4], &[0; 4], "{ctx}: texture prefix");
            let format = tex.head.format().expect("known format");
            *s.formats.entry(format!("{format:?}")).or_default() += 1;
            s.named += usize::from(tex.name.is_some());
            assert!(
                file.palette(tex.head.palette_id).is_some(),
                "{ctx}: texture {} palette {} missing",
                tex.id,
                tex.head.palette_id
            );
            let head_raw = tex
                .chunks
                .iter()
                .find(|c| c.name() == "HEAD")
                .unwrap()
                .payload;
            assert_eq!(tex.head.to_bytes(), head_raw, "{ctx}: HEAD round trip");
            let px = tex
                .decode()
                .unwrap_or_else(|e| panic!("{ctx}: texture {}: {e}", tex.id));
            assert_eq!(
                px.len(),
                usize::from(tex.head.width) * usize::from(tex.head.height)
            );
            if format.is_rle() {
                let (w, h) = (usize::from(tex.head.width), usize::from(tex.head.height));
                assert!(
                    encode_rle(&px, w, h) == tex.body,
                    "{ctx}: texture {} RLE re-encode differs",
                    tex.id
                );
                s.rle_reencoded += 1;
            }
            if format == PixelFormat::TexRgb {
                assert!(tex.name.is_some(), "{ctx}: TEX_RGB without NAME");
            }
        }
    }
    let formats = |v: &[(&str, usize)]| v.iter().map(|&(k, n)| (k.to_string(), n)).collect();
    assert_eq!(
        s,
        Stats {
            files: 1214,
            empty_files: 338,
            palettes: 1427,
            textures: 5176,
            formats: formats(&[("BshRgb", 5028), ("BshRgbH", 1), ("TexRgb", 147)]),
            named: 147,
            flagged_palettes: 10,
            rle_reencoded: 5029,
        }
    );
}

#[test]
fn inc_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping inc_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut paths = Vec::new();
    files_with_ext(&dir, "inc", &mut paths);
    let (mut empty, mut records, mut with_tex, mut numbers_are_texture_ids) = (0, 0, 0, 0);
    for path in &paths {
        let buf = std::fs::read(path).expect("readable");
        let entries =
            inc::parse_access(&buf).unwrap_or_else(|| panic!("{}: bad ACCESS", path.display()));
        if entries.is_empty() {
            empty += 1;
        }
        records += entries.len();
        let Ok(tex_buf) = std::fs::read(path.with_extension("tex")) else {
            continue;
        };
        with_tex += 1;
        let tex = TexFile::parse(&tex_buf).expect("tex parses");
        if entries
            .iter()
            .all(|e| tex.textures.iter().any(|t| t.id == e.number))
        {
            numbers_are_texture_ids += 1;
        }
    }
    assert_eq!(paths.len(), 1213);
    assert_eq!((empty, records), (340, 1559), "records");
    assert_eq!((with_tex, numbers_are_texture_ids), (1213, 1213));
}
