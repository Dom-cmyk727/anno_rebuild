//! `FigurenHeader.dat` + `Figuren*.dat` corpus test. Every figure asserted
//! here is recorded in `docs/formats/figuren.md`.

mod common;

use std::collections::BTreeMap;

use anno_formats::figuren::{self, FigurenHeader, Frame, FrameFormat};
use common::game_dir;

/// Concatenated volume files of one zoom group, and the part sizes.
fn volume(dir: &std::path::Path, group: u32) -> (Vec<u8>, Vec<u64>) {
    let (mut data, mut sizes) = (Vec::new(), Vec::new());
    for part in 0.. {
        let Ok(buf) = std::fs::read(dir.join(figuren::volume_file_name(group, part))) else {
            break;
        };
        sizes.push(u64::try_from(buf.len()).unwrap());
        data.extend_from_slice(&buf);
    }
    (data, sizes)
}

#[test]
fn figuren_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping figuren_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("FigurenHeader.dat")).expect("FigurenHeader.dat");
    let h = FigurenHeader::parse(&buf).unwrap_or_else(|e| panic!("{e}"));
    assert_eq!(h.to_bytes(), buf, "byte-identical round trip");

    // Entries: 368, ids ascending and unique, prefix flag 0.
    assert_eq!(h.entries.len(), 368);
    assert!(h.entries.windows(2).all(|w| w[0].id < w[1].id));
    assert_eq!(
        (h.entries[0].id, h.entries[367].id),
        (1, 1666),
        "first/last id"
    );
    assert!(h.entries.iter().all(|e| e.prefix[..4] == [0; 4]));
    let palettes: BTreeMap<usize, usize> = h.entries.iter().fold(BTreeMap::new(), |mut m, e| {
        *m.entry(e.palettes.len()).or_default() += 1;
        m
    });
    assert_eq!(
        palettes,
        BTreeMap::from([(0, 1), (1, 282), (2, 60), (3, 25)])
    );

    // Sprite sets: groups 0, 1, 2 in order with equal frame counts; shadows
    // mirror the frame counts of their sprite set.
    let (mut sets, mut shadows, mut frames) = (0, 0, Vec::<(u32, bool, &Frame)>::new());
    for e in &h.entries {
        for g in &e.gfx {
            sets += 1;
            let groups: Vec<u32> = g.frame_sets.iter().map(|s| s.group).collect();
            assert!(groups.is_empty() || groups == [0, 1, 2], "entry {}", e.id);
            let counts: Vec<usize> = g.frame_sets.iter().map(|s| s.frames.len()).collect();
            assert!(counts.windows(2).all(|w| w[0] == w[1]), "entry {}", e.id);
            if let Some(s) = &g.shadow {
                shadows += 1;
                assert_eq!(s.prefix, [3, 0, 0, 0, 0, 0, 0, 0]);
                let sc: Vec<usize> = s.frame_sets.iter().map(|s| s.frames.len()).collect();
                assert_eq!(sc, counts, "entry {} shadow", e.id);
                for fs in &s.frame_sets {
                    frames.extend(fs.frames.iter().map(|f| (fs.group, true, f)));
                }
            }
            for fs in &g.frame_sets {
                for f in &fs.frames {
                    assert!(
                        usize::from(f.palette) < e.palettes.len(),
                        "entry {} palette",
                        e.id
                    );
                    frames.push((fs.group, false, f));
                }
            }
        }
    }
    assert_eq!((sets, shadows), (1288, 1047));
    assert_eq!(frames.len(), 731_121);

    // Volume headers: one per group, frame count = frames of that group.
    let vols: Vec<(u32, u32)> = h.volumes.iter().map(|v| (v.group, v.frames)).collect();
    assert_eq!(vols, [(0, 243_707), (1, 243_707), (2, 243_707)]);
    for v in &h.volumes {
        let n = frames.iter().filter(|(g, _, _)| *g == v.group).count();
        assert_eq!(u32::try_from(n).unwrap(), v.frames);
    }

    // Formats: shadows are always BSH_ALPHA, sprites never.
    let mut formats = BTreeMap::<String, usize>::new();
    for (_, shadow, f) in &frames {
        let fmt = f.format().expect("known format");
        assert_eq!(*shadow, fmt == FrameFormat::Alpha);
        assert_eq!((f.unknown_06, f.unknown_20), (8, 0));
        *formats.entry(f.format_name()).or_default() += 1;
    }
    let expect = [
        ("BSH_ALPHA", 347_058),
        ("BSH_RGBAV", 3_837),
        ("BSH_RGBAZ", 504),
        ("BSH_RGBH", 24),
        ("BSH_RGBV", 215_178),
        ("BSH_RGBZ", 164_520),
    ];
    assert_eq!(formats, expect.map(|(k, n)| (k.to_string(), n)).into());

    // Volumes: frame ranges tile each group's files exactly; every frame decodes.
    let mut root_payload = buf.len() as u64 - 16;
    let mut decoded = 0;
    for v in &h.volumes {
        let (data, sizes) = volume(&dir, v.group);
        let total: u64 = sizes.iter().sum();
        root_payload += total;
        let expect_size = total + 8;
        assert_eq!(
            u64::from(v.size),
            expect_size,
            "group {} size field",
            v.group
        );
        let mut ranges: Vec<(u32, u32, &Frame)> = frames
            .iter()
            .filter(|(g, _, f)| *g == v.group && f.length > 0)
            .map(|(_, _, f)| (f.offset, f.length, *f))
            .collect();
        ranges.sort_by_key(|r| (r.0, r.1));
        ranges.dedup_by_key(|r| (r.0, r.1));
        let mut pos = 0u64;
        for (off, len, f) in &ranges {
            assert_eq!(u64::from(*off), pos, "group {} gap or overlap", v.group);
            pos += u64::from(*len);
            assert!(figuren::locate(&sizes, u64::from(*off), u64::from(*len)).is_some());
            let (s, e) = (*off as usize, (*off + *len) as usize);
            let px = figuren::decode_frame(
                f.format().unwrap(),
                &data[s..e],
                usize::from(f.width),
                usize::from(f.height),
            )
            .unwrap_or_else(|err| panic!("group {} offset {off}: {err}", v.group));
            assert_eq!(px.len(), usize::from(f.width) * usize::from(f.height));
            decoded += 1;
        }
        assert_eq!(pos, total, "group {} ranges end at the volume end", v.group);
    }
    assert_eq!(decoded, 655_842, "distinct non-empty frame ranges");
    assert_eq!(
        u64::from(u32::from_le_bytes(
            h.root_header[12..16].try_into().unwrap()
        )),
        root_payload,
        "root size = header file - 16 + all volume files"
    );
    let empty = frames.iter().filter(|(_, _, f)| f.length == 0).count();
    assert_eq!(empty, 5_697);
}
