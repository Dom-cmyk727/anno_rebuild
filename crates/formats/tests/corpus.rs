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

/// Corpus facts accumulated over all parsed files.
#[derive(Debug, Default, PartialEq, Eq)]
struct Stats {
    files: usize,
    /// Files with bytes after the root chunk.
    trailing: usize,
    chunks: usize,
    /// Name fields with non-zero bytes after the terminating NUL.
    dirty_names: usize,
}

fn visit(chunks: &[chunk::Chunk<'_>], stats: &mut Stats) {
    for c in chunks {
        stats.chunks += 1;
        let nul = c.name().len();
        assert!(nul < chunk::NAME_LEN, "{}: name without NUL", c.name());
        if c.name_field[nul + 1..].iter().any(|&b| b != 0) {
            stats.dirty_names += 1;
        }
        if let Some(kids) = &c.children {
            visit(kids, stats);
        }
    }
}

/// Parses `path`, checks the byte-identical round trip and records stats.
fn check_file(path: &Path, stats: &mut Stats) {
    let buf = std::fs::read(path).expect("readable file");
    let file = chunk::parse_file(&buf).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
    assert!(!file.chunks.is_empty(), "{}: no chunks", path.display());

    let mut written = Vec::with_capacity(buf.len());
    chunk::write_file(&file, &mut written).expect("write to Vec");
    assert!(written == buf, "{}: round trip differs", path.display());

    stats.files += 1;
    if !file.trailing.is_empty() {
        stats.trailing += 1;
    }
    visit(&file.chunks, stats);
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
    let mut total = Stats::default();
    for (ext, count, trailing) in expected {
        let mut files = Vec::new();
        files_with_ext(&dir, ext, &mut files);
        let mut stats = Stats::default();
        for path in &files {
            check_file(path, &mut stats);
        }
        assert_eq!((stats.files, stats.trailing), (count, trailing), ".{ext}");
        total.files += stats.files;
        total.trailing += stats.trailing;
        total.chunks += stats.chunks;
        total.dirty_names += stats.dirty_names;
    }

    let mut anlagen = Stats::default();
    check_file(&dir.join("Anlagen.dat"), &mut anlagen);
    assert_eq!(anlagen.trailing, 0, "Anlagen.dat");
    total.chunks += anlagen.chunks;
    total.dirty_names += anlagen.dirty_names;

    // Recorded in docs/formats/chunk-container.md.
    assert_eq!((total.chunks, total.dirty_names), (17_529, 22));
}
