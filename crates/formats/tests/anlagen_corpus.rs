//! `Anlagen.dat` corpus test. Every figure asserted here is recorded in
//! `docs/formats/anlagen.md`.

mod common;

use std::collections::BTreeMap;

use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, Frame};
use common::game_dir;

#[test]
fn anlagen_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping anlagen_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let file = AnlagenFile::parse(&buf).unwrap_or_else(|e| panic!("{e}"));
    assert_eq!(file.colors.len(), 440);
    assert_eq!(file.entries.len(), 921);
    assert!(file.entries.windows(2).all(|w| w[0].id < w[1].id));
    assert_eq!((file.entries[0].id, file.entries[920].id), (1, 3998));

    // Every entry re-encodes byte-identically from the parsed fields.
    for (e, c) in file.entries.iter().zip(&file.entry_chunks) {
        assert_eq!(e.to_bytes(), c.payload, "entry {}", e.id);
        assert!(e.palettes.is_empty(), "entry {}: no inline tables", e.id);
    }

    // COLORS: 1–3 ids of global tables, 0 = unused; frames index the list.
    let lists: BTreeMap<usize, usize> = file.entries.iter().fold(BTreeMap::new(), |mut m, e| {
        *m.entry(e.palette_ids.len()).or_default() += 1;
        m
    });
    assert_eq!(
        lists,
        BTreeMap::from([(0, 30), (1, 640), (2, 195), (3, 56)])
    );
    let (mut frames, mut sets, mut shadows) = (Vec::<(u32, &Frame)>::new(), 0, 0);
    for e in &file.entries {
        assert!(e.palette_ids.iter().all(|&i| i <= 440), "entry {}", e.id);
        for g in &e.gfx {
            sets += 1;
            for fs in &g.frame_sets {
                for f in &fs.frames {
                    let id = e.palette_ids.get(usize::from(f.palette)).copied();
                    assert!(matches!(id, Some(1..=440)), "entry {} palette", e.id);
                    frames.push((fs.group, f));
                }
            }
            if let Some(s) = &g.shadow {
                shadows += 1;
                for fs in &s.frame_sets {
                    frames.extend(fs.frames.iter().map(|f| (fs.group, f)));
                }
            }
        }
    }
    assert_eq!((sets, shadows), (1031, 110), "sprite sets");
    assert_eq!(frames.len(), 3 * 9633);

    let mut formats = BTreeMap::<String, usize>::new();
    for (_, f) in &frames {
        assert!(f.format().is_some(), "{}", f.format_name());
        *formats.entry(f.format_name()).or_default() += 1;
    }
    let expect = [
        ("BSH_ALPHA", 981),
        ("BSH_RGBAH", 120),
        ("BSH_RGBAV", 7_470),
        ("BSH_RGBAZ", 6_669),
        ("BSH_RGBH", 108),
        ("BSH_RGBV", 1_899),
        ("BSH_RGBWZ", 3_528),
        ("BSH_RGBZ", 7_212),
        ("BSH_RGBZM", 912),
    ];
    assert_eq!(formats, expect.map(|(k, n)| (k.to_string(), n)).into());

    // Volumes: frame ranges tile each volume exactly; every range decodes,
    // 51 only with clipping.
    let (mut decoded, mut clipped) = (0, BTreeMap::<String, usize>::new());
    for v in &file.volumes {
        let n = frames.iter().filter(|(g, _)| *g == v.group).count();
        assert_eq!(u32::try_from(n).unwrap(), v.frames, "group {}", v.group);
        let mut ranges: Vec<&Frame> = frames
            .iter()
            .filter(|(g, f)| *g == v.group && f.length > 0)
            .map(|(_, f)| *f)
            .collect();
        ranges.sort_by_key(|f| (f.offset, f.length));
        ranges.dedup_by_key(|f| (f.offset, f.length));
        let mut pos = 0usize;
        for f in ranges {
            let (off, len) = (f.offset as usize, f.length as usize);
            assert_eq!(off, pos, "group {} gap or overlap", v.group);
            pos += len;
            let fmt = f.format().unwrap();
            let (w, h) = (usize::from(f.width), usize::from(f.height));
            let data = &v.data[off..off + len];
            if figuren::decode_frame(fmt, data, w, h).is_err() {
                let (_, dropped) = figuren::decode_frame_clipped(fmt, data, w, h)
                    .unwrap_or_else(|e| panic!("group {} offset {off}: {e}", v.group));
                assert!(dropped > 0);
                *clipped.entry(f.format_name()).or_default() += 1;
            }
            decoded += 1;
        }
        assert_eq!(
            pos,
            v.data.len(),
            "group {} ends at the volume end",
            v.group
        );
    }
    assert_eq!(decoded, 6484 + 6452 + 6473);
    let expect = [("BSH_RGBAZ", 26), ("BSH_RGBZ", 19), ("BSH_RGBZM", 6)];
    assert_eq!(clipped, expect.map(|(k, n)| (k.to_string(), n)).into());
}
