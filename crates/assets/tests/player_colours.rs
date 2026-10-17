//! Player colours against the game files: which figures get them and how
//! many palette colours the green test selects (`docs/re/annodisplay.md`,
//! "Player colours"). Needs `ANNO1503_GAME_DIR`; skips otherwise.

use anno_assets::figure::{self, ORIGINAL_PLAYER_COLOURS};
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::FigurenHeader;

fn game_dir() -> Option<std::path::PathBuf> {
    std::env::var_os("ANNO1503_GAME_DIR").map(std::path::PathBuf::from)
}

/// Palette colours that the player variant changes.
fn hits(p: &anno_formats::tex::Palette<'_>) -> usize {
    (0..=255u8)
        .filter(|&i| figure::recolour(p.rgb(i), ORIGINAL_PLAYER_COLOURS[0]) != p.rgb(i))
        .count()
}

#[test]
fn player_colour_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping player_colour_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("FigurenHeader.dat")).expect("FigurenHeader.dat");
    let header = FigurenHeader::parse(&buf).expect("parses");
    let coloured: Vec<u32> = header
        .entries
        .iter()
        .filter(|e| figure::player_coloured(e))
        .map(|e| e.id)
        .collect();
    assert_eq!(coloured.len(), 69);
    assert!(coloured.contains(&701) && coloured.contains(&718) && coloured.contains(&1543));
    // No ship type, and the thief (723) keeps its colours.
    assert!(
        !coloured
            .iter()
            .any(|id| (1501..=1511).contains(id) || *id == 723)
    );
    let archer = header.entries.iter().find(|e| e.id == 701).expect("701");
    assert_eq!(hits(&archer.palettes[0].palette), 48);

    let abuf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let anlagen = AnlagenFile::parse(&abuf).expect("parses");
    let flag = anlagen
        .colors
        .iter()
        .find(|t| t.palette.id == 109)
        .expect("table 109");
    assert_eq!(hits(&flag.palette), 30);
}
