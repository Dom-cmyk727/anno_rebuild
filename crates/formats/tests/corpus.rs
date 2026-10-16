//! Chunk-container corpus test over the original game files.
//!
//! Needs `ANNO1503_GAME_DIR` pointing at the extracted GOG 2.0.0.5 install
//! (e.g. `game/app`); skips with a message otherwise. Expected counts are
//! those of GOG 2.0.0.5 (`docs/research/game-files-analysis.md` §2–3).

use std::path::{Path, PathBuf};

use anno_formats::chunk;

fn game_dir() -> Option<PathBuf> {
    let dir = PathBuf::from(std::env::var_os("ANNO1503_GAME_DIR")?);
    assert!(
        dir.is_dir(),
        "ANNO1503_GAME_DIR={} is not a directory",
        dir.display()
    );
    Some(dir)
}

fn files_with_ext(dir: &Path, ext: &str, out: &mut Vec<PathBuf>) {
    for entry in std::fs::read_dir(dir).expect("readable game dir") {
        let path = entry.expect("dir entry").path();
        if path.is_dir() {
            files_with_ext(&path, ext, out);
        } else if path
            .extension()
            .is_some_and(|e| e.eq_ignore_ascii_case(ext))
        {
            out.push(path);
        }
    }
}

/// Parses every file with `ext`; returns (file count, files with trailing bytes).
fn check_ext(dir: &Path, ext: &str) -> (usize, usize) {
    let mut files = Vec::new();
    files_with_ext(dir, ext, &mut files);
    let mut trailing = 0;
    for path in &files {
        let buf = std::fs::read(path).expect("readable file");
        let file = chunk::parse_file(&buf).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
        assert!(!file.chunks.is_empty(), "{}: no chunks", path.display());
        if !file.trailing.is_empty() {
            trailing += 1;
        }
    }
    (files.len(), trailing)
}

#[test]
fn chunk_corpus() {
    let Some(dir) = game_dir() else {
        eprintln!("skipping chunk_corpus: ANNO1503_GAME_DIR not set");
        return;
    };
    // (extension, file count, files with trailing bytes after the root chunk)
    let expected = [
        ("sze", 50, 0),
        ("tex", 1214, 0),
        ("inc", 1213, 0),
        ("zei", 11, 0),
        ("scp", 465, 444),
    ];
    for (ext, count, trailing) in expected {
        assert_eq!(check_ext(&dir, ext), (count, trailing), ".{ext}");
    }

    let buf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let file = chunk::parse_file(&buf).expect("Anlagen.dat parses");
    assert!(file.trailing.is_empty());
}
