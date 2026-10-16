//! `Anlagen.dat/COLORS` corpus test. Every figure asserted here is recorded
//! in `docs/formats/palette.md`.

mod common;

use anno_formats::colors;
use common::game_dir;

#[test]
fn colors_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping colors_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let tables = colors::parse_anlagen(&buf).unwrap_or_else(|e| panic!("{e}"));
    assert_eq!(tables.len(), 440);
    let mut magenta_at_1 = 0;
    for (i, t) in tables.iter().enumerate() {
        let id = t.palette.id;
        assert_eq!(
            usize::try_from(id).unwrap(),
            i + 1,
            "ids are 1..=440 in order"
        );
        // 0 everywhere except 4 on id 109, `Farbtabellen\Flagge01_01.PCX` (OQ-022).
        let flag = &t.palette.prefix[..4];
        if id == 109 {
            assert_eq!(flag, &[4, 0, 0, 0], "entry {id}: flag");
            assert_eq!(t.name_lossy(), "Farbtabellen\\Flagge01_01.PCX");
        } else {
            assert_eq!(flag, &[0; 4], "entry {id}: flag");
        }
        assert!(
            t.palette.entries.iter().all(|e| e[3] == 0),
            "entry {id}: reserved byte"
        );
        let name = t.name_lossy().to_ascii_lowercase();
        assert!(
            name.starts_with("farbtabellen\\") && name.ends_with(".pcx"),
            "entry {id}: {name}"
        );
        magenta_at_1 += usize::from(t.palette.entries[1] == [0xff, 0, 0xff, 0]);
    }
    assert_eq!(magenta_at_1, 287);
    // Byte-identical re-encoding of every ENTRY payload.
    let file = anno_formats::chunk::parse_file(&buf).unwrap();
    let entries = file.chunks[0].children.as_deref().unwrap()[0]
        .children
        .as_deref()
        .unwrap();
    assert_eq!(entries.len(), tables.len());
    for (e, t) in entries.iter().zip(&tables) {
        assert_eq!(t.to_bytes(), e.payload, "entry {}", t.palette.id);
    }
}
