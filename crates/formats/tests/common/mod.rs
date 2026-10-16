//! Helpers shared by the corpus tests. They need `ANNO1503_GAME_DIR`
//! pointing at the extracted GOG 2.0.0.5 install and skip otherwise.
#![allow(dead_code)]

use std::path::{Path, PathBuf};

pub fn game_dir() -> Option<PathBuf> {
    let dir = PathBuf::from(std::env::var_os("ANNO1503_GAME_DIR")?);
    assert!(
        dir.is_dir(),
        "ANNO1503_GAME_DIR={} is not a directory",
        dir.display()
    );
    Some(dir)
}

pub fn files_with_ext(dir: &Path, ext: &str, out: &mut Vec<PathBuf>) {
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
