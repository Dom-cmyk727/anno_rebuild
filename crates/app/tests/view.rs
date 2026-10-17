//! `anno1503 --view … --png …` (headless scene render). The corpus test needs
//! `ANNO1503_GAME_DIR`; the usage test runs everywhere.

use std::path::{Path, PathBuf};
use std::process::Command;

fn app() -> Command {
    Command::new(env!("CARGO_BIN_EXE_anno1503"))
}

#[test]
fn bad_arguments_print_usage() {
    for args in [
        &[][..],
        &["--view"][..],
        &["--view", "x.sze", "--group", "3"][..],
        &["--view", "x.sze", "--bogus", "1"][..],
    ] {
        let res = app().args(args).output().expect("spawn");
        assert_eq!(res.status.code(), Some(2), "{args:?}");
        assert!(String::from_utf8_lossy(&res.stderr).contains("usage:"));
    }
}

/// `Single_01.sze` at zoom group 2: all 15 `INSEL5` islands are drawn with
/// ground, no object fails, and the PNG has the scene size. (Measured
/// 2026-10-16: 16,960 × 8,886 px, 69,256 objects drawn, 11,717 invisible.)
#[test]
fn view_png_scenario() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping view_png_scenario: ANNO1503_GAME_DIR not set");
        return;
    };
    let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join("view_single_01_g2.png");
    let _ = std::fs::remove_file(&out);
    let res = app()
        .arg("--view")
        .arg(dir.join("Scenes/0200Single/Single_01.sze"))
        .arg("--game-dir")
        .arg(&dir)
        .args(["--group", "2", "--png"])
        .arg(&out)
        .output()
        .expect("spawn");
    let stderr = String::from_utf8_lossy(&res.stderr);
    assert!(res.status.success(), "{stderr}");
    assert!(
        stderr.contains("15 islands drawn (0 without ground)"),
        "{stderr}"
    );
    assert!(stderr.contains(", 0 failed"), "{stderr}");

    let dec = png::Decoder::new(std::io::BufReader::new(
        std::fs::File::open(&out).expect("png written"),
    ));
    let mut reader = dec.read_info().expect("png header");
    let (w, h) = (reader.info().width, reader.info().height);
    assert!(stderr.contains(&format!("group 2 {w}x{h} px")), "{stderr}");
    let mut buf = vec![0; reader.output_buffer_size().expect("size")];
    reader.next_frame(&mut buf).expect("png data");
    let opaque = buf
        .as_chunks::<4>()
        .0
        .iter()
        .filter(|p| p[3] == 255)
        .count();
    let total = buf.len() / 4;
    // The sea covers the whole scene, also between the islands
    // (`docs/render/iso.md`, "Scene composition").
    assert_eq!(opaque, total, "{opaque} of {total}");
}

/// The water layer animates: two times give different images of the same
/// size; the same time gives identical images (no hidden state).
#[test]
fn view_png_water_animates() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping view_png_water_animates: ANNO1503_GAME_DIR not set");
        return;
    };
    let render = |time: &str, name: &str| -> Vec<u8> {
        let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join(name);
        let res = app()
            .arg("--view")
            .arg(dir.join("Scenes/Islands/Polar/Po_064x064_0008_Eskimo.scp"))
            .arg("--game-dir")
            .arg(&dir)
            .args(["--group", "2", "--png"])
            .arg(&out)
            .args(["--time", time])
            .output()
            .expect("spawn");
        assert!(
            res.status.success(),
            "{}",
            String::from_utf8_lossy(&res.stderr)
        );
        std::fs::read(&out).expect("png")
    };
    let a = render("0", "water_t0.png");
    let b = render("400", "water_t400.png");
    let a2 = render("0", "water_t0_again.png");
    assert_ne!(a, b, "water frames differ over time");
    assert_eq!(a, a2, "rendering is deterministic for a given time");
}

/// Enhanced water is opt-in: the default equals `--water original`, and
/// `--water enhanced` changes the picture.
#[test]
fn view_png_enhanced_water_is_opt_in() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping view_png_enhanced_water_is_opt_in: ANNO1503_GAME_DIR not set");
        return;
    };
    let render = |water: Option<&str>, name: &str| -> Vec<u8> {
        let out = Path::new(env!("CARGO_TARGET_TMPDIR")).join(name);
        let mut cmd = app();
        cmd.arg("--view")
            .arg(dir.join("Scenes/Islands/Polar/Po_064x064_0008_Eskimo.scp"))
            .arg("--game-dir")
            .arg(&dir)
            .args(["--group", "2", "--time", "300", "--png"])
            .arg(&out);
        if let Some(w) = water {
            cmd.args(["--water", w]);
        }
        let res = cmd.output().expect("spawn");
        assert!(
            res.status.success(),
            "{}",
            String::from_utf8_lossy(&res.stderr)
        );
        std::fs::read(&out).expect("png")
    };
    let default = render(None, "water_default.png");
    let original = render(Some("original"), "water_original.png");
    let enhanced = render(Some("enhanced"), "water_enhanced.png");
    assert_eq!(default, original);
    assert_ne!(original, enhanced);
}
