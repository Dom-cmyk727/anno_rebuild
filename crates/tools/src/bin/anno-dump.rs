//! `anno-dump`: inspect original ANNO 1503 files.
//!
//! ```text
//! anno-dump chunks <file>...        chunk tree (same output as `tools/chunks.py tree`)
//! anno-dump tex [--full] <file>...  palettes and textures (same as `tools/tex.py dump`)
//! anno-dump inc <file>...           ACCESS records (same as `tools/tex.py inc`)
//! ```
//!
//! The `tex`/`inc` text format is specified in `docs/formats/tex.md`
//! ("Reference dump").

use std::fmt::Write as _;
use std::io::Write as _;
use std::process::ExitCode;

use anno_formats::{chunk, inc, tex};

const USAGE: &str = "usage: anno-dump chunks <file>...\n       \
                     anno-dump tex [--full] <file>...\n       \
                     anno-dump inc <file>...";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (cmd, rest) = match args.split_first() {
        Some((cmd, rest)) => (cmd.as_str(), rest),
        None => return usage(),
    };
    let (full, files) = match rest.split_first() {
        Some((flag, files)) if cmd == "tex" && flag == "--full" => (true, files),
        _ => (false, rest),
    };
    if files.is_empty() {
        return usage();
    }
    match cmd {
        "chunks" => dump_files(files, dump_chunks),
        "tex" => dump_files(files, |buf, out| dump_tex(buf, full, out)),
        "inc" => dump_files(files, dump_inc),
        _ => usage(),
    }
}

fn usage() -> ExitCode {
    eprintln!("{USAGE}");
    ExitCode::from(2)
}

/// Reads each file, prints `# <path>` and the dump. `dump` returns `false`
/// when the file does not parse (it has then written `NOPARSE…`).
fn dump_files(files: &[String], dump: impl Fn(&[u8], &mut String) -> bool) -> ExitCode {
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
        if !dump(&buf, &mut out) {
            status = ExitCode::FAILURE;
        }
        if stdout.write_all(out.as_bytes()).is_err() {
            return ExitCode::FAILURE;
        }
    }
    status
}

fn dump_chunks(buf: &[u8], out: &mut String) -> bool {
    match chunk::parse_file(buf) {
        Ok(file) => {
            chunk::write_tree(&file, out).expect("writing to a String");
            true
        }
        Err(e) => {
            writeln!(out, "NOPARSE {e}").expect("writing to a String");
            false
        }
    }
}

/// FNV-1a 64-bit.
fn fnv64(bytes: impl IntoIterator<Item = u8>) -> u64 {
    bytes.into_iter().fold(0xcbf2_9ce4_8422_2325, |h, b| {
        (h ^ u64::from(b)).wrapping_mul(0x0000_0100_0000_01b3)
    })
}

/// Bytes up to the first NUL, quoted: printable ASCII except `"` and `\`
/// as is, everything else as `\xHH`.
fn quote(raw: &[u8]) -> String {
    let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
    let mut s = String::from("\"");
    for &b in &raw[..len] {
        if (0x20..=0x7e).contains(&b) && b != b'"' && b != b'\\' {
            s.push(char::from(b));
        } else {
            write!(s, "\\x{b:02x}").expect("writing to a String");
        }
    }
    s.push('"');
    s
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().fold(String::new(), |mut s, b| {
        write!(s, "{b:02x}").expect("writing to a String");
        s
    })
}

fn prefix_flag(prefix: &[u8; 8]) -> u32 {
    u32::from_le_bytes([prefix[0], prefix[1], prefix[2], prefix[3]])
}

fn dump_tex(buf: &[u8], full: bool, out: &mut String) -> bool {
    let file = match tex::TexFile::parse(buf) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("NOPARSE: {e}");
            out.push_str("NOPARSE\n");
            return false;
        }
    };
    // `TexFile` keeps palettes and textures apart; the dump follows file order.
    let order: Vec<&str> = chunk::parse_file(buf)
        .ok()
        .and_then(|f| f.chunks.into_iter().next())
        .and_then(|root| root.children)
        .map(|kids| kids.iter().map(|c| c.name()).collect())
        .unwrap_or_default();
    let w = |out: &mut String, line: std::fmt::Arguments<'_>| {
        out.write_fmt(line).expect("writing to a String");
        out.push('\n');
    };
    w(
        out,
        format_args!(
            "TEX palettes={} textures={}",
            file.palettes.len(),
            file.textures.len()
        ),
    );
    let (mut palettes, mut textures) = (file.palettes.iter(), file.textures.iter());
    for name in order {
        if name == "COLOR" {
            let Some(p) = palettes.next() else { break };
            let rgb = (0..=255).flat_map(|i| p.rgb(i));
            w(
                out,
                format_args!(
                    "PALETTE id={} flag={} rgb={:016x}",
                    p.id,
                    prefix_flag(p.prefix),
                    fnv64(rgb)
                ),
            );
            if full {
                for row in 0..16u8 {
                    let cells: Vec<String> = (0..16u8).map(|i| hex(&p.rgb(row * 16 + i))).collect();
                    w(out, format_args!("  P{row} {}", cells.join(" ")));
                }
            }
        } else {
            let Some(t) = textures.next() else { break };
            dump_texture(t, full, out);
        }
    }
    true
}

fn dump_texture(t: &tex::Texture<'_>, full: bool, out: &mut String) {
    let h = &t.head;
    let px = t.decode();
    if let Err(e) = &px {
        eprintln!("texture {}: {e}", t.id);
    }
    let (opaque, pixels) = match &px {
        Ok(px) => {
            let bytes = px.iter().flat_map(|p| match p {
                Some(i) => [1, *i],
                None => [0, 0],
            });
            (
                px.iter().filter(|p| p.is_some()).count(),
                format!("{:016x}", fnv64(bytes)),
            )
        }
        Err(_) => (0, "BAD".to_string()),
    };
    writeln!(
        out,
        "TEXTURE id={} flag={} width={} height={} palette={} u06={} u08={} format={} key={} \
         u18={} u1a={} u1c={} body={} name={} opaque={opaque} pixels={pixels}",
        t.id,
        prefix_flag(t.prefix),
        h.width,
        h.height,
        h.palette_id,
        h.unknown_06,
        h.unknown_08,
        quote(&h.format_raw),
        hex(&h.color_key),
        h.unknown_18,
        h.unknown_1a,
        hex(&h.unknown_1c),
        t.body.len(),
        t.name.map_or_else(|| "-".to_string(), quote),
    )
    .expect("writing to a String");
    if let (true, Ok(px)) = (full, &px) {
        let width = usize::from(h.width);
        for y in 0..usize::from(h.height) {
            let cells: String = px[y * width..][..width]
                .iter()
                .map(|p| p.map_or_else(|| "..".to_string(), |i| format!("{i:02x}")))
                .collect();
            writeln!(out, "  R{y} {cells}").expect("writing to a String");
        }
    }
}

fn dump_inc(buf: &[u8], out: &mut String) -> bool {
    let Some(records) = inc::parse_access(buf) else {
        out.push_str("NOPARSE\n");
        return false;
    };
    writeln!(out, "INC records={}", records.len()).expect("writing to a String");
    for r in records {
        writeln!(out, "ACCESS source={} number={}", quote(r.source), r.number)
            .expect("writing to a String");
    }
    true
}
