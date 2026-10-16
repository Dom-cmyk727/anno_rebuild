//! `anno-dump`: inspect original ANNO 1503 files.
//!
//! ```text
//! anno-dump chunks <file>...   print the chunk tree (same output as `tools/chunks.py tree`)
//! ```

use std::fmt::Write as _;
use std::io::Write as _;
use std::process::ExitCode;

use anno_formats::chunk;

const USAGE: &str = "usage: anno-dump chunks <file>...";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.split_first() {
        Some((cmd, files)) if cmd == "chunks" && !files.is_empty() => dump_chunks(files),
        _ => {
            eprintln!("{USAGE}");
            ExitCode::from(2)
        }
    }
}

fn dump_chunks(files: &[String]) -> ExitCode {
    let mut status = ExitCode::SUCCESS;
    let mut stdout = std::io::stdout().lock();
    for path in files {
        let buf = match std::fs::read(path) {
            Ok(b) => b,
            Err(e) => {
                eprintln!("{path}: {e}");
                status = ExitCode::FAILURE;
                continue;
            }
        };
        let mut out = format!("# {path}\n");
        match chunk::parse_file(&buf) {
            Ok(file) => chunk::write_tree(&file, &mut out).expect("writing to a String"),
            Err(e) => {
                writeln!(out, "NOPARSE {e}").expect("writing to a String");
                status = ExitCode::FAILURE;
            }
        }
        if stdout.write_all(out.as_bytes()).is_err() {
            return ExitCode::FAILURE;
        }
    }
    status
}
