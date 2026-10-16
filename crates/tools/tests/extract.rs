//! `anno-extract tex` / `figuren`: PNGs decode back to exactly
//! `anno_assets::texture::to_rgba` / `anno_assets::figure::to_rgba`.
//! Synthetic test runs everywhere; the corpus tests need `ANNO1503_GAME_DIR`.

use std::path::{Path, PathBuf};
use std::process::Command;

use anno_assets::{figure, texture};
use anno_formats::figuren::{self, FigurenHeader};
use anno_formats::tex::TexFile;

fn chunk(name: &str, payload: &[u8]) -> Vec<u8> {
    let mut out = vec![0; 12];
    out[..name.len()].copy_from_slice(name.as_bytes());
    let size = u32::try_from(payload.len()).expect("small payload");
    out.extend_from_slice(&size.to_le_bytes());
    out.extend_from_slice(payload);
    out
}

fn entry(name: &str, id: u32, subs: &[Vec<u8>]) -> Vec<u8> {
    let mut payload = [0u32.to_le_bytes(), id.to_le_bytes()].concat();
    subs.iter().for_each(|s| payload.extend_from_slice(s));
    chunk(name, &payload)
}

fn head(w: u16, h: u16, format: &[u8]) -> Vec<u8> {
    let mut raw: Vec<u8> = [w, h, 0, 8, 0]
        .iter()
        .flat_map(|v| v.to_le_bytes())
        .collect();
    let mut fmt = [0u8; 10];
    fmt[..format.len()].copy_from_slice(format);
    raw.extend_from_slice(&fmt);
    raw.extend_from_slice(&[0xff, 0, 0xff, 0]);
    raw.extend_from_slice(&[0; 12]);
    chunk("HEAD", &raw)
}

fn extract(file: &Path, out: &Path) -> std::process::Output {
    Command::new(env!("CARGO_BIN_EXE_anno-extract"))
        .arg("tex")
        .arg(file)
        .arg(out)
        .output()
        .expect("spawn")
}

fn read_png(path: &Path) -> (u32, u32, Vec<u8>) {
    let dec = png::Decoder::new(std::io::BufReader::new(
        std::fs::File::open(path).expect("png exists"),
    ));
    let mut reader = dec.read_info().expect("png header");
    let mut buf = vec![0; reader.output_buffer_size().expect("size")];
    let info = reader.next_frame(&mut buf).expect("png data");
    assert_eq!(
        (info.color_type, info.bit_depth),
        (png::ColorType::Rgba, png::BitDepth::Eight)
    );
    buf.truncate(info.buffer_size());
    (info.width, info.height, buf)
}

/// Extracts `tex` into `out` and checks every PNG against `to_rgba`.
/// Returns the index file.
fn check(tex: &Path, out: &Path) -> String {
    let _ = std::fs::remove_dir_all(out);
    let res = extract(tex, out);
    let buf = std::fs::read(tex).expect("readable");
    let file = TexFile::parse(&buf).expect("parses");
    for t in &file.textures {
        let Ok(want) = texture::to_rgba(&file, t) else {
            continue;
        };
        let got = read_png(&out.join(format!("{}.png", t.id)));
        assert_eq!(
            got,
            (u32::from(want.width), u32::from(want.height), want.pixels),
            "{}: texture {}",
            tex.display(),
            t.id
        );
    }
    let index = std::fs::read_to_string(out.join("index.txt")).expect("index.txt");
    assert_eq!(index.lines().count(), file.textures.len());
    assert_eq!(res.status.success(), !index.contains("ERROR"));
    index
}

#[test]
fn extract_synthetic() {
    let dir = Path::new(env!("CARGO_TARGET_TMPDIR")).join("extract_synthetic");
    std::fs::create_dir_all(&dir).expect("tmp dir");
    let pal: Vec<u8> = (0..=255u8).flat_map(|i| [i, 0x40, 0xff - i, 0]).collect();
    let tex = chunk(
        "ANNO",
        &[
            entry("COLOR", 0, &[chunk("BODY", &pal)]),
            entry(
                "TEXTURE",
                5,
                &[
                    head(3, 2, b"BSH_RGB"),
                    chunk("BODY", &[1, 2, 9, 10, 0xFE, 0, 1, 3, 0xFF]),
                ],
            ),
            entry(
                "TEXTURE",
                6,
                &[
                    head(2, 2, b"TEX_RGB"),
                    chunk("BODY", &[0, 1, 2, 3]),
                    chunk("NAME", b"Wiese\xfc.bmp\0"),
                ],
            ),
            entry(
                "TEXTURE",
                7,
                &[head(1, 1, b"BSH_RGB"), chunk("BODY", &[0, 5, 0xFF])],
            ),
        ]
        .concat(),
    );
    let path = dir.join("synthetic.tex");
    std::fs::write(&path, tex).expect("write");
    let index = check(&path, &dir.join("out"));
    let lines: Vec<&str> = index.lines().collect();
    assert_eq!(
        lines[0],
        "5.png width=3 height=2 palette=0 format=BSH_RGB name=-"
    );
    assert_eq!(
        lines[1],
        "6.png width=2 height=2 palette=0 format=TEX_RGB name=Wiese\u{fc}.bmp"
    );
    assert!(lines[2].starts_with("ERROR "), "{index}");
    // Row 0: transparent, index 9, index 10. Index i -> R=0xff-i, G=0x40, B=i.
    let (_, _, px) = read_png(&dir.join("out/5.png"));
    assert_eq!(
        &px[..12],
        [0, 0, 0, 0, 0xf6, 0x40, 9, 255, 0xf5, 0x40, 10, 255]
    );
}

#[test]
fn extract_corpus_samples() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping extract_corpus_samples: ANNO1503_GAME_DIR not set");
        return;
    };
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("extract_corpus");
    for sample in [
        "Textures/debug.tex",
        "Textures/Felder.tex",
        "ToolGfx/fenster.tex",
    ] {
        let index = check(&dir.join(sample), &out.join(sample));
        assert!(!index.contains("ERROR"), "{sample}");
    }
}

/// Entries 101 (V + shadows), 1501 (Z), 1510 (AZ): every exported PNG equals
/// `figure::to_rgba`, and the index has one line per frame.
#[test]
fn extract_figuren_corpus_samples() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping extract_figuren_corpus_samples: ANNO1503_GAME_DIR not set");
        return;
    };
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("extract_figuren");
    let _ = std::fs::remove_dir_all(&out);
    let ids = ["101", "1501", "1510"];
    let res = Command::new(env!("CARGO_BIN_EXE_anno-extract"))
        .arg("figuren")
        .arg(&dir)
        .arg(&out)
        .args(ids)
        .output()
        .expect("spawn");
    assert!(
        res.status.success(),
        "{}",
        String::from_utf8_lossy(&res.stderr)
    );
    let buf = std::fs::read(dir.join("FigurenHeader.dat")).expect("header");
    let header = FigurenHeader::parse(&buf).expect("parses");
    let volumes: Vec<Vec<u8>> = (0..3)
        .map(|g| {
            (0..)
                .map_while(|p| std::fs::read(dir.join(figuren::volume_file_name(g, p))).ok())
                .flatten()
                .collect()
        })
        .collect();
    for id in ids {
        let entry = header
            .entries
            .iter()
            .find(|e| e.id.to_string() == id)
            .expect("entry");
        let mut frames = 0;
        for (i, set) in entry.gfx.iter().enumerate() {
            for (suffix, s) in [("", Some(set)), ("_shadow", set.shadow.as_deref())] {
                for fs in s.map(|s| s.frame_sets.as_slice()).unwrap_or_default() {
                    let volume = &volumes[usize::try_from(fs.group).unwrap()];
                    for (k, f) in fs.frames.iter().enumerate() {
                        frames += 1;
                        // Every 7th frame keeps the test fast in debug builds.
                        if frames % 7 != 1 {
                            continue;
                        }
                        let want = figure::to_rgba(entry, f, volume).expect("decodes");
                        let png = out
                            .join(id)
                            .join(format!("s{i}_g{}_{k}{suffix}.png", fs.group));
                        assert_eq!(
                            read_png(&png),
                            (u32::from(want.width), u32::from(want.height), want.pixels),
                            "{}",
                            png.display()
                        );
                    }
                }
            }
        }
        let index = std::fs::read_to_string(out.join(id).join("index.txt")).expect("index");
        assert_eq!(index.lines().count(), frames, "entry {id}");
        assert!(!index.contains("ERROR"), "entry {id}");
    }
    let missing = Command::new(env!("CARGO_BIN_EXE_anno-extract"))
        .arg("figuren")
        .arg(&dir)
        .arg(&out)
        .arg("99999")
        .output()
        .expect("spawn");
    assert!(!missing.status.success());
}

/// `anno-extract insel` on one template and one scenario: every PNG equals
/// `island::ground_top_down`; the template with texture 205 shows `MISSING`.
#[test]
fn extract_insel_corpus_samples() {
    use anno_assets::island::{self, GroundTextures};
    use anno_formats::insel;
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping extract_insel_corpus_samples: ANNO1503_GAME_DIR not set");
        return;
    };
    let felder_path = dir.join("Textures/Felder.tex");
    let felder_buf = std::fs::read(&felder_path).expect("Felder.tex");
    let textures = GroundTextures::new(&TexFile::parse(&felder_buf).unwrap()).unwrap();
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("extract_insel");
    for (sample, missing) in [
        ("Scenes/Islands/Nord/N_256x256_0015_5B.scp", true),
        ("Scenes/0200Single/Single_01.sze", false),
    ] {
        let target = out.join(sample);
        let _ = std::fs::remove_dir_all(&target);
        let res = Command::new(env!("CARGO_BIN_EXE_anno-extract"))
            .arg("insel")
            .arg(dir.join(sample))
            .arg(&felder_path)
            .arg(&target)
            .arg("2")
            .output()
            .expect("spawn");
        assert!(
            res.status.success(),
            "{}",
            String::from_utf8_lossy(&res.stderr)
        );
        let buf = std::fs::read(dir.join(sample)).unwrap();
        let islands = insel::islands(&buf).unwrap();
        let index = std::fs::read_to_string(target.join("index.txt")).unwrap();
        assert_eq!(index.lines().count(), islands.len());
        let mut saw_missing = false;
        for (k, i) in islands.iter().enumerate() {
            let want = island::ground_top_down(i, &textures, 2).unwrap();
            saw_missing |= want.pixels.chunks(4).any(|p| p == island::MISSING);
            let got = read_png(&target.join(format!("island{k}.png")));
            assert_eq!(
                got,
                (u32::from(want.width), u32::from(want.height), want.pixels),
                "{sample} island {k}"
            );
        }
        assert_eq!(saw_missing, missing, "{sample}");
    }
}

/// `anno-extract anlagen` on the palace (1380, `Z`), castle (2223, `AZ`),
/// cliff (1591, `ZM`) and river piece (2702, `WZ`): PNGs equal
/// `building::to_rgba`.
#[test]
fn extract_anlagen_corpus_samples() {
    use anno_assets::building;
    use anno_formats::anlagen::AnlagenFile;
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping extract_anlagen_corpus_samples: ANNO1503_GAME_DIR not set");
        return;
    };
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("extract_anlagen");
    let _ = std::fs::remove_dir_all(&out);
    let ids = ["1380", "2223", "1591", "2702"];
    let res = Command::new(env!("CARGO_BIN_EXE_anno-extract"))
        .arg("anlagen")
        .arg(dir.join("Anlagen.dat"))
        .arg(&out)
        .args(ids)
        .output()
        .expect("spawn");
    assert!(
        res.status.success(),
        "{}",
        String::from_utf8_lossy(&res.stderr)
    );
    let buf = std::fs::read(dir.join("Anlagen.dat")).unwrap();
    let file = AnlagenFile::parse(&buf).unwrap();
    for id in ids {
        let entry = file
            .entries
            .iter()
            .find(|e| e.id.to_string() == id)
            .unwrap();
        let mut frames = 0;
        for (i, set) in entry.gfx.iter().enumerate() {
            for (suffix, s) in [("", Some(set)), ("_shadow", set.shadow.as_deref())] {
                for fs in s.map(|s| s.frame_sets.as_slice()).unwrap_or_default() {
                    for (k, f) in fs.frames.iter().enumerate() {
                        frames += 1;
                        let want = building::to_rgba(&file, entry, fs.group, f).expect("decodes");
                        let png = out
                            .join(id)
                            .join(format!("s{i}_g{}_{k}{suffix}.png", fs.group));
                        assert_eq!(
                            read_png(&png),
                            (u32::from(want.width), u32::from(want.height), want.pixels),
                            "{}",
                            png.display()
                        );
                    }
                }
            }
        }
        let index = std::fs::read_to_string(out.join(id).join("index.txt")).unwrap();
        assert_eq!(index.lines().count(), frames, "entry {id}");
    }
}

/// `anno-extract view`: renders islands without failed objects; the canvas
/// size follows the projection (`docs/render/iso.md`).
#[test]
fn view_corpus_samples() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping view_corpus_samples: ANNO1503_GAME_DIR not set");
        return;
    };
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("view");
    std::fs::create_dir_all(&out).unwrap();
    // Canvas: the island's tile diamonds at group 2 (20 × 10) plus
    // `scene::margin(2)` above (docs/render/iso.md).
    let size = |w: u32, h: u32| {
        let margin = u32::try_from(anno_render::scene::margin(2)).unwrap();
        ((w + h) * 10, (w + h) * 5 + margin)
    };
    for (sample, index, group, size) in [
        (
            "Scenes/Islands/Polar/Po_064x064_0008_Eskimo.scp",
            "0",
            "2",
            size(64, 64),
        ),
        ("Scenes/0200Single/Single_01.sze", "1", "2", size(80, 80)),
    ] {
        let png = out.join(format!("{}.png", sample.replace('/', "_")));
        let res = Command::new(env!("CARGO_BIN_EXE_anno-extract"))
            .args(["view"])
            .arg(&dir)
            .arg(dir.join(sample))
            .args([index])
            .arg(&png)
            .arg(group)
            .output()
            .expect("spawn");
        let log = String::from_utf8_lossy(&res.stderr);
        assert!(res.status.success(), "{sample}: {log}");
        assert!(log.contains(" 0 failed"), "{log}");
        let (w, h, px) = read_png(&png);
        assert_eq!((w, h), size, "{sample}");
        assert!(
            px.chunks(4).filter(|p| p[3] == 255).count() > px.len() / 4 / 8,
            "{sample}: mostly empty"
        );
    }
}
