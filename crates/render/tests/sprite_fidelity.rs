//! Asset fidelity at single-pixel level (PLAN §0.4 (1)): every frame of every
//! `Anlagen.dat` and `FigurenHeader.dat` sprite set and shadow, at all three
//! zoom groups, keeps every covered pixel when decoded, and the renderer's
//! blitter reproduces the decoded image exactly (shadows as their coverage
//! mask in the shadow colour). Needs `ANNO1503_GAME_DIR`; skips otherwise.

use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

use anno_assets::texture::Rgba;
use anno_assets::{building, figure};
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, FigurenHeader, Frame, GfxSet};
use anno_render::iso::{self, Canvas, OVERRUN_SLACK, SHADOW};

fn game_dir() -> Option<PathBuf> {
    std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from)
}

/// Draws `img` with the renderer's blitter onto a transparent canvas that
/// covers exactly the image, placed by the frame anchor, and checks it
/// pixel by pixel. Returns the number of covered pixels.
fn check_blit(img: &Rgba, frame: &Frame, shadow: bool, what: &str) -> usize {
    let anchor = (1000, 2000);
    let (w, h) = (usize::from(img.width), usize::from(img.height));
    let mut canvas = Canvas {
        width: w,
        height: h,
        origin: (
            anchor.0 - i32::from(frame.anchor_x),
            anchor.1 - i32::from(frame.anchor_y),
        ),
        pixels: vec![0; w * h * 4],
    };
    iso::blit_sprite(&mut canvas, img, frame, anchor, shadow, |_, _, px| Some(px));
    let mut covered = 0;
    for (i, (got, want)) in canvas
        .pixels
        .chunks(4)
        .zip(img.pixels.chunks(4))
        .enumerate()
    {
        let expect: [u8; 4] = match (want[3], shadow) {
            (0, _) => [0; 4],
            (_, true) => SHADOW,
            _ => [want[0], want[1], want[2], want[3]],
        };
        assert_eq!(got, expect, "{what}: pixel ({}, {})", i % w, i / w);
        covered += usize::from(want[3] > 0);
    }
    covered
}

/// Per zoom group: frames checked, covered pixels, frames wider than their
/// header, largest overrun.
#[derive(Default, Debug, PartialEq, Eq)]
struct Tally {
    frames: usize,
    pixels: usize,
    widened: usize,
    max_overrun: i32,
    /// Pixels past the frame width, which clipping to the width would lose.
    restored: usize,
}

fn sets<'s, 'a>(gfx: &'s [GfxSet<'a>]) -> Vec<(&'s GfxSet<'a>, bool)> {
    gfx.iter()
        .flat_map(|s| std::iter::once((s, false)).chain(s.shadow.as_deref().map(|sh| (sh, true))))
        .collect()
}

#[test]
fn anlagen_frames_are_pixel_exact() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping anlagen_frames_are_pixel_exact: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let file = AnlagenFile::parse(&buf).expect("parses");
    let mut tally = BTreeMap::<u32, Tally>::new();
    let mut failed = Vec::new();
    for e in &file.entries {
        for (set, shadow) in sets(&e.gfx) {
            for fs in &set.frame_sets {
                for (k, f) in fs.frames.iter().enumerate() {
                    let what = format!("entry {} group {} frame {k}", e.id, fs.group);
                    if f.length == 0 {
                        continue;
                    }
                    let img = match building::to_rgba(&file, e, fs.group, f) {
                        Ok(img) => img,
                        Err(err) => {
                            failed.push(format!("{what}: {err}"));
                            continue;
                        }
                    };
                    let t = tally.entry(fs.group).or_default();
                    let over = i32::from(img.width) - i32::from(f.width);
                    assert!(over >= 0, "{what}: image narrower than its frame");
                    assert_eq!(img.height, f.height, "{what}");
                    // Every covered pixel survives decoding, overruns included.
                    let fmt = f.format().expect("known format");
                    let vol = file.volume(fs.group).expect("volume");
                    let data = &vol.data[f.offset as usize..(f.offset + f.length) as usize];
                    let (px, _) = figuren::decode_frame_extended(
                        fmt,
                        data,
                        usize::from(f.width),
                        usize::from(f.height),
                    )
                    .expect("decodes");
                    let covered = check_blit(&img, f, shadow, &what);
                    assert_eq!(covered, px.iter().flatten().count(), "{what}: coverage");
                    t.frames += 1;
                    t.pixels += covered;
                    t.widened += usize::from(over > 0);
                    t.max_overrun = t.max_overrun.max(over);
                    let (_, dropped) = figuren::decode_frame_clipped(
                        fmt,
                        data,
                        usize::from(f.width),
                        usize::from(f.height),
                    )
                    .expect("decodes");
                    t.restored += dropped;
                }
            }
        }
    }
    assert!(
        failed.is_empty(),
        "{} frames failed: {:?}",
        failed.len(),
        &failed[..failed.len().min(5)]
    );
    eprintln!("anlagen fidelity: {tally:?}");
    // (frames, covered pixels, widened frames, largest overrun, restored
    // pixels) per zoom group 0, 1, 2 (docs/render/iso.md, "Fidelity").
    let expect = [
        (9506, 78_213_833, 154, 16, 868),
        (9506, 19_955_171, 179, 9, 587),
        (9506, 4_948_688, 129, 4, 240),
    ];
    for (g, (frames, pixels, widened, max_overrun, restored)) in (0..).zip(expect) {
        let t = &tally[&g];
        assert_eq!(
            (t.frames, t.pixels, t.widened, t.max_overrun, t.restored),
            (frames, pixels, widened, max_overrun, restored),
            "group {g}"
        );
    }
    let max = tally.values().map(|t| t.max_overrun).max().unwrap_or(0);
    assert!(max <= OVERRUN_SLACK, "overrun {max} > OVERRUN_SLACK");
    assert_eq!(max, OVERRUN_SLACK, "OVERRUN_SLACK is the corpus maximum");
}

/// Concatenated `Figuren<group><nn>.dat` of one zoom group.
fn volume(dir: &Path, group: u32) -> Vec<u8> {
    let mut data = Vec::new();
    for part in 0.. {
        match std::fs::read(dir.join(figuren::volume_file_name(group, part))) {
            Ok(b) => data.extend_from_slice(&b),
            Err(_) => break,
        }
    }
    data
}

#[test]
fn figure_frames_are_pixel_exact() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping figure_frames_are_pixel_exact: ANNO1503_GAME_DIR not set");
        return;
    };
    let buf = std::fs::read(dir.join("FigurenHeader.dat")).expect("FigurenHeader.dat");
    let header = FigurenHeader::parse(&buf).expect("parses");
    let volumes: Vec<Vec<u8>> = (0..3).map(|g| volume(&dir, g)).collect();
    let mut tally = BTreeMap::<u32, Tally>::new();
    let mut failed = Vec::new();
    for e in &header.entries {
        for (set, shadow) in sets(&e.gfx) {
            for fs in &set.frame_sets {
                let vol = &volumes[usize::try_from(fs.group).expect("group")];
                for (k, f) in fs.frames.iter().enumerate() {
                    if f.length == 0 {
                        continue;
                    }
                    let what = format!("figure {} group {} frame {k}", e.id, fs.group);
                    let img = match figure::to_rgba(e, f, vol) {
                        Ok(img) => img,
                        Err(err) => {
                            failed.push(format!("{what}: {err}"));
                            continue;
                        }
                    };
                    assert_eq!((img.width, img.height), (f.width, f.height), "{what}");
                    let t = tally.entry(fs.group).or_default();
                    t.pixels += check_blit(&img, f, shadow, &what);
                    t.frames += 1;
                }
            }
        }
    }
    assert!(
        failed.is_empty(),
        "{} frames failed: {:?}",
        failed.len(),
        &failed[..failed.len().min(5)]
    );
    eprintln!("figure fidelity: {tally:?}");
    let expect = [
        (241_808, 145_609_961),
        (241_808, 39_471_049),
        (241_808, 9_570_884),
    ];
    for (g, (frames, pixels)) in (0..).zip(expect) {
        assert_eq!(
            (tally[&g].frames, tally[&g].pixels),
            (frames, pixels),
            "group {g}"
        );
    }
}
