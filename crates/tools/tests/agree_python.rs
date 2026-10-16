//! `anno-dump chunks` must print exactly what the reference parser
//! `tools/chunks.py tree` prints. Needs `ANNO1503_GAME_DIR` and `python3`;
//! skips with a message when either is missing.

use std::path::{Path, PathBuf};
use std::process::Command;

/// Sample covering every chunk-container file type, both scenario roots
/// (`ANNO`, `ANNOADDON1`) and an island with trailing bytes.
const SAMPLES: [&str; 10] = [
    "Anlagen.dat",
    "Textures/debug.tex",
    "Textures/Felder.tex",
    "Textures/debug.inc",
    "ToolGfx/Startup.zei",
    "Textures/ScapeSmall.zei",
    "Scenes/Islands/Polar/Po_064x064_0008_Eskimo.scp",
    "Scenes/0200Single/Single_01.sze",
    "Scenes/0201SingleAddOn/single30.sze",
    "Scenes/Tutorial/01Discover01.sze",
];

fn run(cmd: &mut Command) -> String {
    let out = cmd.output().expect("spawn");
    assert!(
        out.status.success(),
        "{cmd:?} failed: {}",
        String::from_utf8_lossy(&out.stderr)
    );
    String::from_utf8(out.stdout).expect("utf-8 output")
}

#[test]
fn anno_dump_matches_chunks_py() {
    let Some(dir) = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from) else {
        eprintln!("skipping anno_dump_matches_chunks_py: ANNO1503_GAME_DIR not set");
        return;
    };
    if Command::new("python3").arg("--version").output().is_err() {
        eprintln!("skipping anno_dump_matches_chunks_py: python3 not found");
        return;
    }
    let script = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../tools/chunks.py");
    for sample in SAMPLES {
        let file = dir.join(sample);
        let py = run(Command::new("python3")
            .arg("-I")
            .arg(&script)
            .arg("tree")
            .arg(&file));
        let rs = run(Command::new(env!("CARGO_BIN_EXE_anno-dump"))
            .arg("chunks")
            .arg(&file));
        assert_eq!(rs, py, "{sample}");
        assert!(py.lines().count() > 1, "{sample}: empty tree");
    }
}
