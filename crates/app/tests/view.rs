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
    // Islands cover part of the scene, the sea between them stays transparent.
    assert!(opaque > total / 10 && opaque < total, "{opaque} of {total}");
}
