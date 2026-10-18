//! Reference digests for the C++ port (ADR 0001): FNV-1a 64 of scene renders
//! and of frames of the chunked pipeline, in the order and format of
//! `src/render/tests/render_corpus_test.cpp`, which asserts the same lines.
//!
//! ```text
//! cargo run --release -p anno-render --example digest_scene -- <game_dir> scenes
//! cargo run --release -p anno-render --example digest_scene -- <game_dir> frames
//! ```
use std::path::{Path, PathBuf};
use std::sync::Arc;

use anno_assets::island::GroundTextures;
use anno_assets::{figure, unit};
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, FigurenHeader};
use anno_formats::{insel, sze, tex::TexFile};
use anno_render::frame::FrameRenderer;
use anno_render::iso::{self, WaterStyle};
use anno_render::scene::{Layers, Rect, Scene, UnitSprite};

fn fnv(b: impl IntoIterator<Item = u8>) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for x in b {
        h ^= u64::from(x);
        h = h.wrapping_mul(0x0100_0000_01b3);
    }
    h
}

/// The scene of `buf` without units (`islands` from the file only).
fn scene<'a>(buf: &'a [u8], felder_buf: &[u8], anl_buf: &'a [u8]) -> Scene<'a> {
    let anlagen = AnlagenFile::parse(anl_buf).expect("Anlagen.dat");
    let mut textures =
        GroundTextures::new(&TexFile::parse(felder_buf).expect("Felder.tex")).expect("textures");
    if let Some(c) = iso::water_colour(&anlagen) {
        textures.set_water(c);
    }
    let islands = insel::islands(buf).expect("islands");
    Scene::new(String::new(), islands, textures, anlagen, iso::HEIGHT_LIFT)
}

fn scenes(dir: &Path, felder_buf: &[u8], anl_buf: &[u8]) {
    let hbuf = std::fs::read(dir.join("FigurenHeader.dat")).expect("FigurenHeader.dat");
    let header = FigurenHeader::parse(&hbuf).expect("parses");
    let vols: Vec<Vec<u8>> = (0..3)
        .map(|g| {
            let mut v = Vec::new();
            for part in 0.. {
                match std::fs::read(dir.join(figuren::volume_file_name(g, part))) {
                    Ok(d) => v.extend(d),
                    Err(_) => break,
                }
            }
            v
        })
        .collect();
    let mut files: Vec<PathBuf> = std::fs::read_dir(dir.join("Scenes/0200Single"))
        .expect("dir")
        .map(|e| e.expect("entry").path())
        .filter(|p| p.extension().is_some_and(|x| x == "sze"))
        .collect();
    files.sort();
    files.push(dir.join("Scenes/Islands/Nord/N_064x064_0001_0B.scp"));
    for p in &files {
        let buf = std::fs::read(p).expect("read");
        let mut scene = scene(&buf, felder_buf, anl_buf);
        if let Ok(s) = sze::Scene::parse(&buf) {
            let read = |g: u32, o: u64, l: u64| {
                let (o, l) = (usize::try_from(o).ok()?, usize::try_from(l).ok()?);
                vols.get(usize::try_from(g).ok()?)?
                    .get(o..o + l)
                    .map(<[u8]>::to_vec)
            };
            let kinds = [unit::UnitKind::Ship, unit::UnitKind::Soldier];
            let (units, _) =
                unit::units(&s, &header, &kinds, &figure::ORIGINAL_PLAYER_COLOURS, read)
                    .expect("units");
            scene.units = units
                .into_iter()
                .filter_map(|u| {
                    let z = match u.kind {
                        unit::UnitKind::Ship => None,
                        unit::UnitKind::Soldier => Some(scene.ground_height(u.x, u.y)?),
                    };
                    Some(UnitSprite {
                        x: u.x,
                        y: u.y,
                        z,
                        frames: u
                            .frames
                            .into_iter()
                            .map(|f| f.map(|(fr, img)| (fr, Arc::clone(&img))))
                            .collect(),
                        shadows: u.shadows,
                    })
                })
                .collect();
        }
        let name = p.file_name().expect("name").to_string_lossy();
        for (time, style) in [(0, WaterStyle::Original), (2500, WaterStyle::Enhanced)] {
            let rect = scene.bounds(2).expect("bounds");
            let layers = Layers {
                time_ms: time,
                water_style: style,
                ..Layers::ALL
            };
            let (c, st) = scene.render(rect, 2, layers);
            println!(
                "{name} g2 t{time} {style:?} {}x{} {:016x} drawn={} inv={} failed={} units={}",
                rect.w,
                rect.h,
                fnv(c.pixels.iter().copied()),
                st.objects.drawn,
                st.objects.invisible,
                st.objects.failed,
                st.units
            );
        }
        let b = scene.bounds(1).expect("bounds");
        let rect = Rect {
            x: b.x + b.w / 2 - 800,
            y: b.y + b.h / 2 - 450,
            w: 1600,
            h: 900,
        };
        for style in [WaterStyle::Original, WaterStyle::Enhanced] {
            let layers = Layers {
                time_ms: 700,
                water_style: style,
                ..Layers::ALL
            };
            let (c, st) = scene.render(rect, 1, layers);
            println!(
                "{name} g1 region {style:?} {:016x} drawn={} units={}",
                fnv(c.pixels.iter().copied()),
                st.objects.drawn,
                st.units
            );
        }
    }
}

fn frames(dir: &Path, felder_buf: &[u8], anl_buf: &[u8]) {
    for name in ["Single_01.sze", "Single_24.sze"] {
        let buf = std::fs::read(dir.join("Scenes/0200Single").join(name)).expect("read");
        let scene = scene(&buf, felder_buf, anl_buf);
        let mut fr = FrameRenderer::new(4);
        for group in [2usize, 1, 0] {
            let b = scene.bounds(group).expect("bounds");
            // Single_24's centre at group 0 is all land: move to a coast.
            let dx = if name == "Single_24.sze" && group == 0 {
                2000
            } else {
                0
            };
            let view = Rect {
                x: b.x + b.w / 2 - 640 + dx,
                y: b.y + b.h / 2 - 360,
                w: 1280,
                h: 720,
            };
            for (time, style) in [
                (0u64, WaterStyle::Original),
                (900, WaterStyle::Original),
                (900, WaterStyle::Enhanced),
            ] {
                let mut out = vec![0u32; 1280 * 720];
                fr.render(&scene, view, group, time, style, &mut out);
                let h = fnv(out.iter().flat_map(|p| p.to_le_bytes()));
                println!("{name} g{group} t{time} {style:?} {h:016x}");
            }
        }
    }
}

fn main() {
    let mut args = std::env::args().skip(1);
    let dir = PathBuf::from(args.next().expect("game dir"));
    let felder_buf = std::fs::read(dir.join("Textures/Felder.tex")).expect("Felder.tex");
    let anl_buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    match args.next().as_deref() {
        Some("scenes") => scenes(&dir, &felder_buf, &anl_buf),
        Some("frames") => frames(&dir, &felder_buf, &anl_buf),
        _ => eprintln!("usage: digest_scene <game_dir> scenes|frames"),
    }
}
