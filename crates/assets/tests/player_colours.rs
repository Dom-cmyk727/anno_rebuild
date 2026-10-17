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

/// Colours at game start over all scenarios (`unit::start_colours`).
#[test]
fn start_colours_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping start_colours_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut files = Vec::new();
    collect(&dir.join("Scenes"), &mut files);
    files.sort();
    let (mut removed, mut coloured, mut without_colour) = (0, 0, 0);
    for f in &files {
        let buf = std::fs::read(f).expect("readable");
        let scene = anno_formats::sze::Scene::parse(&buf).expect("scene");
        let start = anno_assets::unit::start_colours(&scene).expect("colours");
        if f.ends_with("Single_24.sze") {
            // All eight preset in the scenario (player 0 = colour 2).
            let expect = [
                (0, 2),
                (1, 1),
                (2, 6),
                (3, 3),
                (4, 4),
                (5, 5),
                (6, 8),
                (7, 7),
            ];
            assert_eq!(start.slots, expect.into());
        }
        removed += start.removed.len();
        coloured += start.slots.len();
        // Every human or computer player that is not removed has a colour,
        // unless the pool ran out.
        for p in scene.players().expect("players") {
            let Some(h) = p.head() else { continue };
            if p.index != 31
                && (h.is_human() || h.is_computer())
                && !start.removed.contains(&p.index)
                && !start.slots.contains_key(&p.index)
                && h.colour != 0
            {
                without_colour += 1;
            }
        }
    }
    assert_eq!((coloured, removed, without_colour), (130, 0, 0));
}

fn collect(dir: &std::path::Path, out: &mut Vec<std::path::PathBuf>) {
    for e in std::fs::read_dir(dir).expect("dir") {
        let p = e.expect("entry").path();
        if p.is_dir() {
            collect(&p, out);
        } else if p.extension().is_some_and(|x| x.eq_ignore_ascii_case("sze")) {
            out.push(p);
        }
    }
}
