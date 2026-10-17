//! Plain-text data corpus test: `Texte.dat` and the keyed `Data/*.dat`
//! files. Every figure asserted here is recorded in
//! `docs/formats/data-text.md`.

mod common;

use std::collections::BTreeSet;

use anno_formats::anlagen::AnlagenFile;
use anno_formats::text::{self, Ending, KeyedKind, Texte};
use common::game_dir;

#[test]
fn keyed_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping keyed_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    // (file, lines, blank, sections, entries, other, ends with CR LF)
    let expect = [
        ("BGruppen.dat", 615, 178, 4, 433, 0, true),
        ("Felder.dat", 349, 96, 4, 248, 1, true),
        ("Highmap.dat", 30, 7, 5, 18, 0, true),
        ("ScapeIcon.dat", 62, 27, 4, 31, 0, true),
        ("Wachstum.dat", 1567, 420, 2, 1145, 0, true),
        ("font.dat", 27, 14, 0, 10, 3, false),
        ("htmlpic.dat", 25, 8, 4, 13, 0, true),
        ("music.dat", 115, 69, 3, 43, 0, true),
        ("statusbar.dat", 119, 26, 4, 89, 0, true),
        ("wegspeed.dat", 63, 12, 1, 50, 0, true),
    ];
    for (name, n_lines, blank, sections, entries, other, crlf_end) in expect {
        let buf = std::fs::read(dir.join("Data").join(name)).expect("readable");
        let lines = text::lines(&buf);
        assert_eq!(text::write_lines(&lines), buf, "{name}: round trip");
        assert!(
            lines.iter().all(|l| l.ending != Ending::Lf),
            "{name}: CR LF only"
        );
        let kinds = text::keyed(&buf);
        let count = |f: fn(&KeyedKind<'_>) -> bool| kinds.iter().filter(|(_, k)| f(k)).count();
        assert_eq!(
            (
                lines.len(),
                count(|k| matches!(k, KeyedKind::Blank)),
                count(|k| matches!(k, KeyedKind::Section(_))),
                count(|k| matches!(k, KeyedKind::Entry { .. })),
                count(|k| matches!(k, KeyedKind::Other(_))),
            ),
            (n_lines, blank, sections, entries, other),
            "{name}"
        );
        assert_eq!(
            lines.last().map(|l| l.text.is_empty()),
            Some(crlf_end),
            "{name}: final CR LF"
        );
    }
    // The one non-tool line that is not an entry: a key without colon.
    let buf = std::fs::read(dir.join("Data/Felder.dat")).expect("readable");
    let other: Vec<_> = text::keyed(&buf)
        .into_iter()
        .filter_map(|(l, k)| matches!(k, KeyedKind::Other(_)).then_some((l.number, l.text)))
        .collect();
    assert_eq!(other, [(48, &b"Pos     0, 0"[..])]);
}

#[test]
fn texte_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping texte_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("Texte.dat")).expect("readable");
    let lines = text::lines(&buf);
    assert_eq!(text::write_lines(&lines), buf, "round trip");
    assert_eq!(lines.len(), 2251);
    assert!(lines.iter().all(|l| l.ending != Ending::Lf), "CR LF only");
    let t = Texte::parse(&buf);
    assert!(t.preamble.is_empty());
    assert_eq!(t.sections.len(), 118);
    let names: BTreeSet<&[u8]> = t.sections.iter().map(|s| s.name).collect();
    assert_eq!(names.len(), 118, "section names unique");
    assert_eq!(
        t.sections.last().map(|s| (s.name, s.lines.len())),
        Some((&b"END"[..], 0))
    );
    // Every text line is in a section; no comments in this file.
    let total: usize = t.sections.iter().map(|s| 1 + s.lines.len()).sum();
    assert_eq!(total, 2250);

    // [ANLAGEN]: `<id>:<name>` for 326 distinct `Anlagen.dat` entries.
    let anl_buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let anlagen = AnlagenFile::parse(&anl_buf).expect("parses");
    let anl = t.section("ANLAGEN").expect("[ANLAGEN]");
    assert_eq!(anl.line, 1);
    let ids: Vec<u32> = anl
        .lines
        .iter()
        .map(|l| text::id_text(l).expect("id:text").0)
        .collect();
    assert_eq!(ids.len(), 326);
    assert_eq!(ids.iter().collect::<BTreeSet<_>>().len(), 326);
    assert!(ids.iter().all(|&id| anlagen.entry(id).is_some()));
    assert_eq!(text::id_text(anl.lines[0]), Some((104, &b"Forest"[..])));

    // Name lists.
    let ships = t.section("SHIPS").expect("[SHIPS]");
    assert_eq!(ships.lines.len(), 107);
    assert_eq!(ships.lines[0], b"Altair");
    assert_eq!(t.section("STAEDTE").map(|s| s.lines.len()), Some(160));
}
