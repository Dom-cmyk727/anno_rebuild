//! Scenario corpus test over all 50 `.sze`: tagged records, players, ships
//! and soldiers. Every figure asserted here is recorded in
//! `docs/formats/sze.md`.

mod common;

use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

use anno_formats::insel;
use anno_formats::sze::{Edition, Player, Scene, nested_prefix};
use anno_formats::tagged::{self, Record};
use common::{files_with_ext, game_dir};

/// Resolves a `\\`-separated path below `base`, ignoring case per segment
/// (the game ran on case-insensitive Windows file systems).
fn find_ci(base: &Path, rel: &str) -> Option<PathBuf> {
    let mut cur = base.to_path_buf();
    for seg in rel.split('\\') {
        cur = std::fs::read_dir(&cur)
            .ok()?
            .filter_map(Result::ok)
            .map(|e| e.path())
            .find(|p| {
                p.file_name()
                    .is_some_and(|n| n.to_string_lossy().eq_ignore_ascii_case(seg))
            })?;
    }
    Some(cur)
}

fn edition_name(e: Edition) -> &'static str {
    match e {
        Edition::Base => "ANNO",
        Edition::Addon => "ANNOADDON1",
    }
}

#[derive(Default)]
struct Stats {
    records: usize,
    /// Long-form records by name and edition.
    long: BTreeMap<(&'static str, String), usize>,
    /// Record counts by name path.
    paths: BTreeMap<String, usize>,
}

/// Walks a record sequence: every nested sequence must parse exactly and
/// re-encode byte-identically.
fn walk(
    recs: &[Record<'_>],
    parent: &str,
    path: &str,
    edition: Edition,
    st: &mut Stats,
    ctx: &str,
) {
    for r in recs {
        st.records += 1;
        let p = format!("{path}/{}", r.name);
        *st.paths.entry(p.clone()).or_default() += 1;
        if r.long {
            *st.long
                .entry((edition_name(edition), r.name.to_string()))
                .or_default() += 1;
        }
        let Some(k) = nested_prefix(parent, r.name) else {
            continue;
        };
        let body = &r.payload[k..];
        let kids = tagged::parse_records(body, 0).unwrap_or_else(|e| panic!("{ctx} {p}: {e}"));
        let mut again = Vec::new();
        tagged::write_records(&kids, &mut again).unwrap();
        assert_eq!(again, body, "{ctx} {p}: round trip");
        walk(&kids, r.name, &p, edition, st, ctx);
    }
}

#[test]
fn scene_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping scene_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let mut files = Vec::new();
    files_with_ext(&dir.join("Scenes"), "sze", &mut files);
    files.sort();
    assert_eq!(files.len(), 50);

    let mut st = Stats::default();
    let (mut base, mut addon, mut blocks, mut comp) = (0, 0, 0, 0);
    let (mut ships, mut soldiers, mut centred) = (0, 0, [0, 0]);
    let mut figures = BTreeSet::new();
    let mut attrs = [0u8; 2];
    let mut without_31 = BTreeMap::<&str, usize>::new();
    let (mut link_files, mut link_count) = (0, 0);
    for path in &files {
        let buf = std::fs::read(path).expect("readable");
        let ctx = path.display().to_string();
        let scene = Scene::parse(&buf).unwrap_or_else(|e| panic!("{ctx}: {e}"));
        match scene.edition {
            Edition::Base => base += 1,
            Edition::Addon => addon += 1,
        }
        // Island links: same head as INSEL5, then a dummy template whose
        // INSEL4 size matches; island numbers unique across both kinds.
        let links = scene
            .island_links()
            .unwrap_or_else(|e| panic!("{ctx}: {e}"));
        if !links.is_empty() {
            assert_eq!(scene.edition, Edition::Addon, "{ctx}: INSELLNK");
            link_files += 1;
        }
        let mut numbers: Vec<u32> = insel::islands(&buf)
            .expect("islands")
            .iter()
            .map(|i| i.number)
            .collect();
        for (l, c) in links.iter().zip(scene.chunks("INSELLNK")) {
            assert_eq!(l.to_bytes(), c.payload, "{ctx}: INSELLNK round trip");
            numbers.push(l.number);
            let rel = String::from_utf8(l.path.to_vec()).expect("ASCII path");
            assert!(
                rel.to_ascii_lowercase().ends_with("_dummy.scp"),
                "{ctx}: {rel}"
            );
            let template = find_ci(&dir.join("Scenes"), &rel)
                .unwrap_or_else(|| panic!("{ctx}: {rel} missing"));
            let tbuf = std::fs::read(template).expect("readable");
            let t = &insel::islands(&tbuf).expect("template")[0];
            assert_eq!(t.version, insel::Version::Insel4, "{ctx}: {rel}");
            assert_eq!((t.width, t.height), (l.width, l.height), "{ctx}: {rel}");
            link_count += 1;
        }
        let n = numbers.len();
        numbers.sort_unstable();
        numbers.dedup();
        assert_eq!(numbers.len(), n, "{ctx}: island numbers");

        for c in scene.chunks("COMPPROFIL2") {
            let recs = tagged::parse_records(c.payload, 0).unwrap_or_else(|e| panic!("{ctx}: {e}"));
            assert!(recs.iter().all(|r| ["DEF", "ENTRY"].contains(&r.name)));
            comp += 1;
        }

        let players = scene.players().unwrap_or_else(|e| panic!("{ctx}: {e}"));
        blocks += players.len();
        // Two series: a header series (HEAD in every block, player 31 last),
        // then a state series (HEAD in none) with the same indices, where
        // player 31 may be missing.
        let half = players
            .iter()
            .position(|p| tagged::find(&p.records, "HEAD").is_none())
            .unwrap_or(players.len());
        let (first, second) = players.split_at(half);
        let idx = |s: &[Player<'_>]| s.iter().map(|p| p.index).collect::<Vec<_>>();
        let (head, state) = (idx(first), idx(second));
        assert_eq!(head.last(), Some(&31), "{ctx}: player 31 last");
        assert!(
            second
                .iter()
                .all(|p| tagged::find(&p.records, "HEAD").is_none())
        );
        if state.len() + 1 == head.len() {
            assert_eq!(state, head[..state.len()], "{ctx}: state series");
            *without_31.entry(edition_name(scene.edition)).or_default() += 1;
        } else {
            assert_eq!(state, head, "{ctx}: state series");
        }

        let mut ids = BTreeSet::new();
        for (p, c) in players.iter().zip(scene.chunks("SPIELER")) {
            assert_eq!(p.flag, 0, "{ctx}: SPIELER flag");
            assert_eq!(
                p.to_bytes().unwrap(),
                c.payload,
                "{ctx}: SPIELER round trip"
            );
            walk(
                &p.records,
                "SPIELER",
                "SPIELER",
                scene.edition,
                &mut st,
                &ctx,
            );
            for (kind, name) in [(0, "SCHIFF"), (1, "SOLDAT")] {
                let raw: Vec<&[u8]> = p
                    .records
                    .iter()
                    .filter(|r| r.name == name)
                    .map(|r| r.payload)
                    .collect();
                let units = p.units(name).unwrap_or_else(|e| panic!("{ctx}: {e}"));
                for (u, raw) in units.iter().zip(raw) {
                    assert_eq!(u.to_bytes().unwrap(), raw, "{ctx}: {name} round trip");
                    assert_eq!(u32::from(u.owner), p.index, "{ctx}: {name} owner");
                    assert_eq!(u.reserved, 0, "{ctx}: {name} 0x06");
                    assert!(ids.insert(u.id), "{ctx}: duplicate unit id {}", u.id);
                    attrs[kind] = attrs[kind].max(u.attr);
                    if u.x().fract() == 0.5 && u.y().fract() == 0.5 {
                        centred[kind] += 1;
                    }
                    if kind == 0 {
                        figures.insert(u.figure);
                        ships += 1;
                    } else {
                        soldiers += 1;
                    }
                }
            }
        }
    }
    assert_eq!((base, addon), (23, 27));
    assert_eq!((link_files, link_count), (9, 229));
    assert_eq!((blocks, comp), (724, 50));
    assert_eq!(st.records, 29_686);
    assert_eq!(without_31, BTreeMap::from([("ANNO", 22)]));
    // Long form: exactly STADT2 and STADT2/PROD of the add-on, all of them.
    let long: Vec<_> = st
        .long
        .iter()
        .map(|((e, n), c)| (*e, n.as_str(), *c))
        .collect();
    assert_eq!(
        long,
        [("ANNOADDON1", "PROD", 193), ("ANNOADDON1", "STADT2", 193)]
    );
    assert_eq!(st.paths["SPIELER/STADT2"], 272);
    assert_eq!(st.paths["SPIELER/STADT2/PROD"], 272);
    assert_eq!((ships, soldiers), (327, 2317));
    assert_eq!(figures, (1501..=1509).collect());
    assert_eq!(centred, [310, 2253]);
    assert_eq!(attrs, [14, 7]);
    for (path, n) in [
        ("SPIELER/SCHIFF/HEAD", 327),
        ("SPIELER/SCHIFF/WAREN", 327),
        ("SPIELER/SCHIFF/PRIVAT", 327),
        ("SPIELER/SCHIFF/PRIVAT/NAME", 327),
        ("SPIELER/SOLDAT/HEAD", 2317),
        ("SPIELER/STADT2/HEAD2", 272),
    ] {
        assert_eq!(st.paths[path], n, "{path}");
    }
}
