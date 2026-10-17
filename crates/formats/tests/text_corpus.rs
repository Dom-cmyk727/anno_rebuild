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

/// Keys of a record, joined with spaces.
fn pattern(r: &[text::Entry<'_>]) -> String {
    r.iter()
        .map(|e| String::from_utf8_lossy(e.key).into_owned())
        .collect::<Vec<_>>()
        .join(" ")
}

/// Record layouts of the keyed files (`docs/formats/data-text.md`,
/// "Records").
#[test]
fn records_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping records_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let read = |name: &str| std::fs::read(dir.join("Data").join(name)).expect("readable");

    // BGruppen.dat [WAREN]: 56 records from `Id`, 48 with one `Preis`.
    let bg = read("BGruppen.dat");
    let waren = text::section_entries(&bg, Some("WAREN"));
    let (head, goods) = text::split_records(&waren, "Id");
    assert!(head.is_empty());
    assert_eq!(goods.len(), 56);
    let allowed = [
        "Id",
        "IdSec",
        "Forsch",
        "BauFirst",
        "BauLast",
        "Preis",
        "Bonus",
        "PreisLevel",
    ];
    let mut priced = 0;
    for g in &goods {
        assert!(
            g.iter()
                .all(|e| allowed.contains(&&*String::from_utf8_lossy(e.key)))
        );
        let prices: Vec<_> = g.iter().filter(|e| e.key == b"Preis").collect();
        assert!(prices.len() <= 1, "{}", pattern(g));
        if let Some(p) = prices.first() {
            assert!(text::int(p.value).is_some_and(|v| v > 0));
            priced += 1;
        }
    }
    assert_eq!(priced, 48);

    // [BGRUPPE]: 5 groups from `BGruppeNr`; each a header, then needs of
    // exactly `Ware Menge Priority Force` (47), `Menge` with ≤ 2 decimals.
    let groups = text::section_entries(&bg, Some("BGRUPPE"));
    let (head, groups) = text::split_records(&groups, "BGruppeNr");
    assert!(head.is_empty());
    let names: Vec<_> = groups.iter().map(|g| g[0].value).collect();
    assert_eq!(names.len(), 5);
    assert_eq!(names[0], b"PIONIER");
    let mut needs = 0;
    for g in &groups {
        let (header, wares) = text::split_records(g, "Ware");
        assert!(pattern(header).starts_with("BGruppeNr Wohnmax BauFirst BauLast"));
        let vermehr = header
            .iter()
            .find(|e| e.key == b"VermehrTab")
            .expect("VermehrTab");
        let fs = text::fields(vermehr.value);
        assert_eq!(fs.len(), 5);
        assert!(fs.iter().all(|f| text::fixed(f, 2).is_some()));
        for w in wares {
            assert_eq!(pattern(w), "Ware Menge Priority Force");
            assert!(text::fixed(w[1].value, 2).is_some());
            needs += 1;
        }
    }
    assert_eq!(needs, 47);

    // [BAUSTUFEN]: 31 `Stufe: <name>, <int>, <int>`, 5 `Wissen: <name>, <int>`.
    let stufen = text::section_entries(&bg, Some("BAUSTUFEN"));
    let count = |k: &[u8]| stufen.iter().filter(|e| e.key == k).count();
    assert_eq!(
        (count(b"Stufe"), count(b"Wissen"), stufen.len()),
        (31, 5, 36)
    );
    for e in &stufen {
        let fs = text::fields(e.value);
        let n = if e.key == b"Stufe" { 3 } else { 2 };
        assert_eq!(fs.len(), n);
        assert!(fs[1..].iter().all(|f| text::int(f).is_some()));
    }

    // Wachstum.dat: `Size: 6, 6`, then 146 records from `Id`, each six
    // `Proz` rows of six integers, optionally 2 `Klima` or 6 `Bridge`; the
    // ids are `Felder.tex` texture ids (all but 116).
    let wa = read("Wachstum.dat");
    let entries = text::section_entries(&wa, Some("WACHSTUM"));
    let (head, records) = text::split_records(&entries, "Id");
    assert_eq!(pattern(head), "Size");
    assert_eq!(head[0].value, b"6, 6");
    let six = "Id Proz Proz Proz Proz Proz Proz";
    let (mut kinds, mut missing_comma) = ([0; 3], 0);
    for r in &records {
        let p = pattern(r);
        let k = if p == six {
            0
        } else if p == format!("{six} Klima Klima") {
            1
        } else {
            assert_eq!(p, format!("{six}{}", " Bridge".repeat(6)));
            2
        };
        kinds[k] += 1;
        for row in &r[1..7] {
            // Six comma-separated fields, except one row that lacks a comma.
            let commas = text::fields(row.value).len();
            if commas != 6 {
                assert_eq!((row.line, commas), (765, 5));
                missing_comma += 1;
            }
            let vs = text::ints(row.value).expect("integers");
            assert_eq!(vs.len(), 6);
            assert!(vs.iter().all(|v| (0..=100).contains(v)));
        }
    }
    assert_eq!((kinds, missing_comma), ([95, 46, 5], 1));
    let felder = std::fs::read(dir.join("Textures/Felder.tex")).expect("Felder.tex");
    let felder = anno_formats::tex::TexFile::parse(&felder).expect("parses");
    let tex_ids: BTreeSet<u32> = felder.textures.iter().map(|t| t.id).collect();
    let ids: BTreeSet<u32> = records
        .iter()
        .map(|r| u32::try_from(text::int(r[0].value).expect("id")).expect("u32"))
        .collect();
    assert_eq!(ids.len(), 146);
    assert_eq!(tex_ids.difference(&ids).copied().collect::<Vec<_>>(), [116]);
    assert!(ids.is_subset(&tex_ids));

    // wegspeed.dat: 5 records `Ob Un Mask × 8`, masks of eight 0/1 digits.
    let ws = read("wegspeed.dat");
    let entries = text::section_entries(&ws, Some("WALKMASK"));
    let (head, records) = text::split_records(&entries, "Ob");
    assert!(head.is_empty());
    assert_eq!(records.len(), 5);
    for r in &records {
        assert_eq!(pattern(r), format!("Ob Un{}", " Mask".repeat(8)));
        for m in &r[2..] {
            assert!(m.value.len() == 8 && m.value.iter().all(|b| matches!(b, b'0' | b'1')));
        }
    }

    // Felder.dat [TEXTURE]: one `Name` record per `Felder.tex` texture.
    let fd = read("Felder.dat");
    let entries = text::section_entries(&fd, Some("TEXTURE"));
    let (_, records) = text::split_records(&entries, "Name");
    assert_eq!(records.len(), felder.textures.len());
    assert_eq!(records.len(), 147);
}
