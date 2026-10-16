//! `anno-extract tex`: PNGs decode back to exactly `anno_assets::texture::to_rgba`.
//! Synthetic test runs everywhere; the corpus test needs `ANNO1503_GAME_DIR`.

use std::path::{Path, PathBuf};
use std::process::Command;

use anno_assets::texture;
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
