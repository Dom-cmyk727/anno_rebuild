//! `anno-dump`: inspect original ANNO 1503 files.
//!
//! ```text
//! anno-dump chunks <file>...        chunk tree (same output as `tools/chunks.py tree`)
//! anno-dump tex [--full] <file>...  palettes and textures (same as `tools/tex.py dump`)
//! anno-dump inc <file>...           ACCESS records (same as `tools/tex.py inc`)
//! anno-dump colors [--full] <Anlagen.dat>...  COLORS tables (same as `tools/tex.py colors`)
//! anno-dump figuren [--full] <FigurenHeader.dat>...  figure index (same as `tools/figuren.py dump`)
//! anno-dump frames <game_dir> <entry id>...  decoded frames (same as `tools/figuren.py frames`)
//! anno-dump insel [--full] <file.scp|file.sze>...  islands (same as `tools/insel.py dump`)
//! anno-dump probe <path> <file>...   statistics over all chunks at a name path
//! ```
//!
//! `probe` is an investigation aid for undecoded chunks (e.g.
//! `ANNO/COLORS/ENTRY` in `Anlagen.dat`); its output is not a reference
//! format and has no Python counterpart.
//!
//! The `tex`/`inc` text format is specified in `docs/formats/tex.md`
//! ("Reference dump"), `colors` in `docs/formats/palette.md`, `figuren` and
//! `frames` in `docs/formats/figuren.md`, `insel` in `docs/formats/insel.md`.

use std::fmt::Write as _;
use std::io::Write as _;
use std::process::ExitCode;

use anno_formats::{chunk, colors, figuren, inc, insel, tex};

const USAGE: &str = "usage: anno-dump chunks <file>...\n       \
                     anno-dump tex [--full] <file>...\n       \
                     anno-dump inc <file>...\n       \
                     anno-dump colors [--full] <Anlagen.dat>...\n       \
                     anno-dump figuren [--full] <FigurenHeader.dat>...\n       \
                     anno-dump frames <game_dir> <entry id>...\n       \
                     anno-dump insel [--full] <file.scp|file.sze>...\n       \
                     anno-dump probe <path> <file>...";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (cmd, rest) = match args.split_first() {
        Some((cmd, rest)) => (cmd.as_str(), rest),
        None => return usage(),
    };
    let (full, files) = match rest.split_first() {
        Some((flag, files))
            if matches!(cmd, "tex" | "colors" | "figuren" | "insel") && flag == "--full" =>
        {
            (true, files)
        }
        _ => (false, rest),
    };
    if cmd == "probe" {
        return match rest.split_first() {
            Some((path, files)) if !files.is_empty() => {
                dump_files(files, |buf, out| probe(buf, path, out))
            }
            _ => usage(),
        };
    }
    if cmd == "frames" {
        return match rest.split_first() {
            Some((dir, ids)) if !ids.is_empty() => dump_frames(dir, ids),
            _ => usage(),
        };
    }
    if files.is_empty() {
        return usage();
    }
    match cmd {
        "chunks" => dump_files(files, dump_chunks),
        "tex" => dump_files(files, |buf, out| dump_tex(buf, full, out)),
        "inc" => dump_files(files, dump_inc),
        "colors" => dump_files(files, |buf, out| dump_colors(buf, full, out)),
        "figuren" => dump_files(files, |buf, out| dump_figuren(buf, full, out)),
        "insel" => dump_files(files, |buf, out| dump_insel(buf, full, out)),
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

/// FNV-1a 64 over the 256 entries as `R, G, B`, as 16 hex digits.
fn palette_hash(p: &tex::Palette<'_>) -> String {
    format!("{:016x}", fnv64((0..=255).flat_map(|i| p.rgb(i))))
}

/// The `--full` palette lines: `  P<row> <16 × rrggbb>`.
fn palette_rows(p: &tex::Palette<'_>, out: &mut String) {
    for row in 0..16u8 {
        let cells: Vec<String> = (0..16u8).map(|i| hex(&p.rgb(row * 16 + i))).collect();
        writeln!(out, "  P{row} {}", cells.join(" ")).expect("writing to a String");
    }
}

fn dump_colors(buf: &[u8], full: bool, out: &mut String) -> bool {
    let tables = match colors::parse_anlagen(buf) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("NOPARSE: {e}");
            out.push_str("NOPARSE\n");
            return false;
        }
    };
    writeln!(out, "COLORS entries={}", tables.len()).expect("writing to a String");
    for t in &tables {
        let p = &t.palette;
        writeln!(
            out,
            "COLORTABLE id={} flag={} rgb={} name={}",
            p.id,
            prefix_flag(p.prefix),
            palette_hash(p),
            quote(t.name)
        )
        .expect("writing to a String");
        if full {
            palette_rows(p, out);
        }
    }
    true
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
            w(
                out,
                format_args!(
                    "PALETTE id={} flag={} rgb={}",
                    p.id,
                    prefix_flag(p.prefix),
                    palette_hash(p)
                ),
            );
            if full {
                palette_rows(p, out);
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

/// All chunks reached by following the `/`-separated name path from the top level.
fn find<'a>(chunks: &[chunk::Chunk<'a>], names: &[&str], acc: &mut Vec<chunk::Chunk<'a>>) {
    let Some((first, rest)) = names.split_first() else {
        return;
    };
    for c in chunks.iter().filter(|c| c.name() == *first) {
        if rest.is_empty() {
            acc.push(c.clone());
        } else if let Some(kids) = &c.children {
            find(kids, rest, acc);
        }
    }
}

/// `name:size` of each chunk in `seq`, comma-separated.
fn seq_names(seq: &[chunk::Chunk<'_>]) -> String {
    let names: Vec<String> = seq
        .iter()
        .map(|c| format!("{}:{}", c.name(), c.size()))
        .collect();
    names.join(",")
}

/// Prints `<key>×<count>` lines for the `limit` most frequent keys.
fn histogram(out: &mut String, label: &str, counts: &std::collections::BTreeMap<String, usize>) {
    let mut v: Vec<(&String, &usize)> = counts.iter().collect();
    v.sort_by(|a, b| b.1.cmp(a.1).then(a.0.cmp(b.0)));
    for (k, n) in v.iter().take(20) {
        writeln!(out, "{label} {k} x{n}").expect("writing to a String");
    }
    if v.len() > 20 {
        writeln!(out, "{label} ... {} more distinct", v.len() - 20).expect("writing to a String");
    }
}

/// Statistics over every chunk at `path`: payload sizes, child structure,
/// the first offset at which the payload is an exact chunk sequence (a fixed
/// prefix, as in `.tex` `COLOR`), sample bytes and per-`u32` value ranges
/// over the first 64 bytes of the most common size.
fn probe(buf: &[u8], path: &str, out: &mut String) -> bool {
    use std::collections::{BTreeMap, BTreeSet};
    let file = match chunk::parse_file(buf) {
        Ok(f) => f,
        Err(e) => {
            writeln!(out, "NOPARSE {e}").expect("writing to a String");
            return false;
        }
    };
    let names: Vec<&str> = path.split('/').collect();
    let mut hits = Vec::new();
    find(&file.chunks, &names, &mut hits);
    writeln!(out, "PROBE path={path} count={}", hits.len()).expect("writing to a String");
    let Some(last) = hits.last() else {
        return true;
    };
    let count = |f: &dyn Fn(&chunk::Chunk<'_>) -> String| {
        let mut m = BTreeMap::new();
        for c in &hits {
            *m.entry(f(c)).or_insert(0usize) += 1;
        }
        m
    };
    histogram(out, "SIZE", &count(&|c| c.size().to_string()));
    histogram(
        out,
        "CHILDREN",
        &count(&|c| {
            c.children
                .as_deref()
                .map_or_else(|| "leaf".to_string(), seq_names)
        }),
    );
    histogram(
        out,
        "PREFIX",
        &count(&|c| {
            (0..c.size().min(64))
                .find_map(|o| {
                    chunk::parse_chunks(&c.payload[o..])
                        .filter(|s| !s.is_empty())
                        .map(|s| format!("{o} {}", seq_names(&s)))
                })
                .unwrap_or_else(|| "none".to_string())
        }),
    );
    for (i, c) in [(0, &hits[0]), (hits.len() - 1, last)] {
        let n = c.size().min(64);
        writeln!(
            out,
            "SAMPLE i={i} off={} size={} head={}",
            c.offset,
            c.size(),
            hex(&c.payload[..n])
        )
        .expect("writing to a String");
    }
    let mut sizes = BTreeMap::new();
    for c in &hits {
        *sizes.entry(c.size()).or_insert(0usize) += 1;
    }
    let (&size, &n) = sizes
        .iter()
        .max_by_key(|(s, n)| (**n, std::cmp::Reverse(**s)))
        .expect("non-empty");
    writeln!(out, "FIELDS size={size} entries={n}").expect("writing to a String");
    for o in (0..size.min(64).saturating_sub(3)).step_by(4) {
        let vals: BTreeSet<u32> = hits
            .iter()
            .filter(|c| c.size() == size)
            .map(|c| {
                u32::from_le_bytes([
                    c.payload[o],
                    c.payload[o + 1],
                    c.payload[o + 2],
                    c.payload[o + 3],
                ])
            })
            .collect();
        let (min, max) = (vals.first().expect("n > 0"), vals.last().expect("n > 0"));
        let mut line = format!("U32 +0x{o:02x} distinct={} min={min} max={max}", vals.len());
        if vals.len() <= 6 {
            let list: Vec<String> = vals.iter().map(|v| format!("{v:#x}")).collect();
            write!(line, " values={}", list.join(",")).expect("writing to a String");
        }
        writeln!(out, "{line}").expect("writing to a String");
    }
    true
}

fn names(chunks: &[chunk::Chunk<'_>]) -> String {
    chunks
        .iter()
        .map(chunk::Chunk::name)
        .collect::<Vec<_>>()
        .join(",")
}

fn dump_figuren(buf: &[u8], full: bool, out: &mut String) -> bool {
    let h = match figuren::FigurenHeader::parse(buf) {
        Ok(h) => h,
        Err(e) => {
            eprintln!("NOPARSE: {e}");
            out.push_str("NOPARSE\n");
            return false;
        }
    };
    let w = |out: &mut String, line: std::fmt::Arguments<'_>| {
        out.write_fmt(line).expect("writing to a String");
        out.push('\n');
    };
    w(
        out,
        format_args!(
            "FIGUREN entries={} volumes={}",
            h.entries.len(),
            h.volumes.len()
        ),
    );
    for v in &h.volumes {
        w(
            out,
            format_args!(
                "VOLUME group={} size={} frames={}",
                v.group, v.size, v.frames
            ),
        );
    }
    for e in &h.entries {
        w(
            out,
            format_args!(
                "ENTRY id={} flag={} records={}",
                e.id,
                prefix_flag(e.prefix),
                names(&e.records)
            ),
        );
        for r in e
            .records
            .iter()
            .filter(|r| !matches!(r.name(), "COLORS" | "HAEUSER"))
        {
            w(
                out,
                format_args!(
                    "  RECORD {} size={} hash={:016x}",
                    r.name(),
                    r.size(),
                    fnv64(r.payload.iter().copied())
                ),
            );
        }
        for (i, t) in e.palettes.iter().enumerate() {
            let p = &t.palette;
            w(
                out,
                format_args!(
                    "  COLORTABLE i={i} id={} flag={} rgb={} name={}",
                    p.id,
                    prefix_flag(p.prefix),
                    palette_hash(p),
                    quote(t.name)
                ),
            );
        }
        for (i, g) in e.gfx.iter().enumerate() {
            dump_gfx_set(out, &format!("SET i={i}"), "  ", g, full);
        }
    }
    true
}

fn dump_gfx_set(out: &mut String, label: &str, indent: &str, g: &figuren::GfxSet<'_>, full: bool) {
    let other = g
        .records
        .iter()
        .filter(|r| !matches!(r.name(), "TEXTURES" | "SHADOW"));
    let hash = fnv64(other.flat_map(|r| r.payload.iter().copied()));
    writeln!(
        out,
        "{indent}{label} prefix={} records={} other={hash:016x}",
        hex(g.prefix),
        names(&g.records)
    )
    .expect("writing to a String");
    for fs in &g.frame_sets {
        let bytes = fs.to_bytes();
        writeln!(
            out,
            "{indent}  TEXTURES group={} frames={} hash={:016x}",
            fs.group,
            fs.frames.len(),
            fnv64(bytes)
        )
        .expect("writing to a String");
        if full {
            for (k, f) in fs.frames.iter().enumerate() {
                writeln!(
                    out,
                    "{indent}    FRAME k={k} width={} height={} palette={} u06={} format={} anchor={},{} offset={} length={} u20={}",
                    f.width, f.height, f.palette, f.unknown_06, quote(&f.format_raw), f.anchor_x, f.anchor_y, f.offset, f.length, f.unknown_20
                )
                .expect("writing to a String");
            }
        }
    }
    if let Some(s) = &g.shadow {
        dump_gfx_set(out, "SHADOW", &format!("{indent}  "), s, full);
    }
}

/// Group volumes: concatenated `Figuren<g><nn>.dat` contents.
fn read_volume(dir: &std::path::Path, group: u32) -> std::io::Result<Vec<u8>> {
    let mut data = Vec::new();
    for part in 0.. {
        match std::fs::read(dir.join(figuren::volume_file_name(group, part))) {
            Ok(buf) => data.extend_from_slice(&buf),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound && part > 0 => break,
            Err(e) => return Err(e),
        }
    }
    Ok(data)
}

/// For each requested entry id: one `FRAME` line per frame of every sprite
/// set and shadow, with a hash over the decoded pixels.
fn dump_frames(dir: &str, ids: &[String]) -> ExitCode {
    let dir = std::path::Path::new(dir);
    let run = || -> Result<(String, bool), Box<dyn std::error::Error>> {
        let buf = std::fs::read(dir.join("FigurenHeader.dat"))?;
        let h = figuren::FigurenHeader::parse(&buf)?;
        let vols = (0..3)
            .map(|g| read_volume(dir, g))
            .collect::<Result<Vec<_>, _>>()?;
        let (mut out, mut ok) = (String::new(), true);
        for id in ids {
            let Some(e) = h.entries.iter().find(|e| e.id.to_string() == *id) else {
                writeln!(out, "ENTRY id={id} MISSING")?;
                ok = false;
                continue;
            };
            writeln!(out, "ENTRY id={id} sets={}", e.gfx.len())?;
            for (i, g) in e.gfx.iter().enumerate() {
                let sets = [(0, Some(g)), (1, g.shadow.as_deref())];
                for (shadow, set) in sets {
                    for fs in set.map(|s| s.frame_sets.as_slice()).unwrap_or_default() {
                        for (k, f) in fs.frames.iter().enumerate() {
                            write!(
                                out,
                                "FRAME set={i} shadow={shadow} group={} k={k} ",
                                fs.group
                            )?;
                            ok &= frame_line(&vols, fs.group, f, &mut out)?;
                        }
                    }
                }
            }
        }
        Ok((out, ok))
    };
    match run() {
        Ok((out, ok)) => {
            let written = std::io::stdout().lock().write_all(out.as_bytes()).is_ok();
            if ok && written {
                ExitCode::SUCCESS
            } else {
                ExitCode::FAILURE
            }
        }
        Err(e) => {
            eprintln!("{}: {e}", dir.display());
            ExitCode::FAILURE
        }
    }
}

/// `width= height= format= opaque= pixels=` where `pixels` is FNV-1a 64 over
/// `0` per transparent pixel and `1, index, z, level` per covered one, or `BAD`.
fn frame_line(
    vols: &[Vec<u8>],
    group: u32,
    f: &figuren::Frame,
    out: &mut String,
) -> Result<bool, std::fmt::Error> {
    let range = usize::try_from(f.offset)
        .ok()
        .and_then(|s| Some(s..s.checked_add(usize::try_from(f.length).ok()?)?));
    let data = usize::try_from(group)
        .ok()
        .and_then(|g| vols.get(g))
        .zip(range)
        .and_then(|(v, r)| v.get(r));
    let px = match (f.format(), data) {
        (Some(fmt), Some(d)) => {
            figuren::decode_frame(fmt, d, usize::from(f.width), usize::from(f.height)).ok()
        }
        _ => None,
    };
    let (opaque, pixels) = match &px {
        Some(px) => {
            let bytes = px.iter().flat_map(|p| match p {
                Some(p) => vec![1, p.index, p.z, p.level],
                None => vec![0],
            });
            (
                px.iter().flatten().count(),
                format!("{:016x}", fnv64(bytes)),
            )
        }
        None => (0, "BAD".to_string()),
    };
    writeln!(
        out,
        "width={} height={} format={} opaque={opaque} pixels={pixels}",
        f.width,
        f.height,
        quote(&f.format_raw)
    )?;
    Ok(px.is_some())
}

const BLOCK_RECORDS: [&str; 5] = ["HEIGHT", "GFXID", "GELAENDE", "PROD2", "FLUESSE"];

/// The reason of an island error, without offsets (part of the reference format).
fn reason(e: insel::Error) -> &'static str {
    match e {
        insel::Error::NoIsland => "no island",
        insel::Error::Bad { what, .. } => what,
    }
}

fn dump_insel(buf: &[u8], full: bool, out: &mut String) -> bool {
    let islands = match insel::islands(buf) {
        Ok(i) => i,
        Err(e) => {
            eprintln!("NOPARSE: {e}");
            out.push_str("NOPARSE\n");
            return false;
        }
    };
    let mut ok = true;
    let mut w = |line: String| {
        out.push_str(&line);
        out.push('\n');
    };
    w(format!("ISLANDS count={}", islands.len()));
    for (k, i) in islands.iter().enumerate() {
        let version = match i.version {
            insel::Version::Insel4 => 4,
            insel::Version::Insel5 => 5,
        };
        w(format!(
            "ISLAND i={k} version={version} x={} y={} width={} height={} number={} head={} records={}",
            i.x,
            i.y,
            i.width,
            i.height,
            i.number,
            hex(i.head),
            names(&i.records)
        ));
        let mut seen = Vec::new();
        for r in &i.records {
            if !BLOCK_RECORDS.contains(&r.name()) {
                w(format!(
                    "  RECORD {} size={} hash={:016x}",
                    r.name(),
                    r.size(),
                    fnv64(r.payload.iter().copied())
                ));
            } else if !seen.contains(&r.name()) {
                seen.push(r.name());
            }
        }
        match (i.heights(), i.gfx_ids()) {
            (Ok(hs), Ok(gs)) => {
                let (lo, hi) = (
                    hs.iter().min().copied().unwrap_or(0),
                    hs.iter().max().copied().unwrap_or(0),
                );
                w(format!(
                    "  GRID HEIGHT min={lo} max={hi} hash={:016x}",
                    fnv64(hs.iter().copied())
                ));
                let mut distinct = gs.clone();
                distinct.sort_unstable();
                distinct.dedup();
                let bytes = gs.iter().flat_map(|g| g.to_le_bytes());
                w(format!(
                    "  GRID GFXID distinct={} hash={:016x}",
                    distinct.len(),
                    fnv64(bytes)
                ));
                let mut textures: Vec<u8> = gs
                    .iter()
                    .map(|&g| insel::GroundCell::from_gfx_id(g).texture)
                    .filter(|&t| t != 0)
                    .collect();
                textures.sort_unstable();
                textures.dedup();
                let water = gs
                    .iter()
                    .filter(|&&g| insel::GroundCell::from_gfx_id(g).texture == 0)
                    .count();
                let list: Vec<String> = textures.iter().map(u8::to_string).collect();
                w(format!(
                    "  GROUND water={water} textures={}",
                    list.join(",")
                ));
                if full {
                    let width = usize::from(i.width).max(1);
                    for (y, row) in hs.chunks(width).enumerate() {
                        w(format!("    H{y} {}", hex(row)));
                    }
                    for (y, row) in gs.chunks(width).enumerate() {
                        let cells: Vec<String> = row.iter().map(|g| format!("{g:04x}")).collect();
                        w(format!("    G{y} {}", cells.join("")));
                    }
                }
            }
            (h, g) => {
                let e = h.err().or(g.err()).map(reason).unwrap_or_default();
                w(format!("  GRID BAD {e}"));
                ok = false;
            }
        }
        for name in ["GELAENDE", "PROD2", "FLUESSE"]
            .iter()
            .filter(|n| seen.contains(n))
        {
            match i.blocks(name) {
                Ok(bs) => {
                    let items: usize = bs.iter().map(|b| b.data.len() / 6).sum();
                    let bytes = bs
                        .iter()
                        .flat_map(|b| b.header_bytes().into_iter().chain(b.data.iter().copied()));
                    w(format!(
                        "  BLOCKS {name} count={} items={items} hash={:016x}",
                        bs.len(),
                        fnv64(bytes)
                    ));
                }
                Err(e) => {
                    w(format!("  BLOCKS {name} BAD {}", reason(e)));
                    ok = false;
                }
            }
        }
    }
    ok
}
