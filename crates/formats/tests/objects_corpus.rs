//! Placed objects (`GELAENDE`, `PROD2`, `FLUESSE` items) over all islands,
//! checked against `Anlagen.dat`. Figures recorded in `docs/formats/insel.md`.

mod common;

use std::collections::{BTreeMap, BTreeSet};

use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel::{self, PlacedObject};
use common::{files_with_ext, game_dir};

#[test]
fn objects_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping objects_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let anl_buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let anlagen = AnlagenFile::parse(&anl_buf).expect("parses");
    let mut files = Vec::new();
    files_with_ext(&dir.join("Scenes"), "scp", &mut files);
    files_with_ext(&dir.join("Scenes"), "sze", &mut files);
    files.sort();

    let mut items = BTreeMap::<&str, usize>::new();
    let mut dups = BTreeMap::<&str, usize>::new();
    let (mut overlap_swap, mut overlap_plain) = (0, 0);
    for path in &files {
        let buf = std::fs::read(path).expect("readable");
        for island in insel::islands(&buf).expect("islands") {
            let (w, h) = (island.width, island.height);
            for name in ["GELAENDE", "PROD2", "FLUESSE"] {
                let objs = island.objects(name).expect("items");
                *items.entry(name).or_default() += objs.len();
                for o in &objs {
                    assert!(
                        anlagen.entry(u32::from(o.anlage)).is_some(),
                        "{}: id {}",
                        path.display(),
                        o.anlage
                    );
                    assert_eq!(o.reserved, 0);
                    assert!(
                        o.x < w && o.y < h,
                        "{}: object outside island",
                        path.display()
                    );
                }
                let unique: BTreeSet<PlacedObject> = objs.iter().copied().collect();
                *dups.entry(name).or_default() += objs.len() - unique.len();
                if name == "PROD2" {
                    overlap_swap += overlaps(&unique, &anlagen, true);
                    overlap_plain += overlaps(&unique, &anlagen, false);
                }
            }
        }
    }
    let expect = [
        ("FLUESSE", 33_106),
        ("GELAENDE", 4_475_412),
        ("PROD2", 76_863),
    ];
    assert_eq!(items, expect.into());
    assert_eq!((dups["GELAENDE"], dups["PROD2"]), (97_041, 485));
    // Buildings: swapping the footprint for odd rotations removes most overlaps.
    assert_eq!((overlap_swap, overlap_plain), (115, 1_350));
}

/// Tiles covered by more than one footprint.
fn overlaps(objs: &BTreeSet<PlacedObject>, anlagen: &AnlagenFile<'_>, swap: bool) -> usize {
    let mut covered = BTreeMap::<(u16, u16), u32>::new();
    for o in objs {
        let (fw, fh) = anlagen.footprint(u32::from(o.anlage)).expect("HEAD");
        let (w, h) = if swap { o.footprint(fw, fh) } else { (fw, fh) };
        for y in o.y..o.y + u16::from(h) {
            for x in o.x..o.x + u16::from(w) {
                *covered.entry((x, y)).or_default() += 1;
            }
        }
    }
    covered.values().filter(|&&n| n > 1).count()
}
