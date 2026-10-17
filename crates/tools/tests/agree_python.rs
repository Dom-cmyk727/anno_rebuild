//! `anno-dump` must print exactly what the Python reference tools print:
//! `chunks` = `tools/chunks.py tree`, `tex` = `tools/tex.py dump`,
//! `inc` = `tools/tex.py inc`. The synthetic tests run everywhere (they need
//! only `python3`); the corpus tests also need `ANNO1503_GAME_DIR`. Each
//! skips with a message when a requirement is missing.

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

fn have_python(test: &str) -> bool {
    // `output().is_ok()` alone is not enough: Windows may have a `python3`
    // store alias that runs but is not Python.
    let ok = Command::new("python3")
        .arg("--version")
        .output()
        .is_ok_and(|o| o.status.success() && o.stdout.starts_with(b"Python 3"));
    if !ok {
        eprintln!("skipping {test}: python3 not found");
    }
    ok
}

fn game_dir(test: &str) -> Option<PathBuf> {
    let dir = std::env::var_os("ANNO1503_GAME_DIR").map(PathBuf::from);
    if dir.is_none() {
        eprintln!("skipping {test}: ANNO1503_GAME_DIR not set");
    }
    dir
}

fn script(name: &str) -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../tools")
        .join(name)
}

/// Runs a command; returns (exit code, stdout).
fn run(cmd: &mut Command) -> (Option<i32>, String) {
    let out = cmd.output().expect("spawn");
    let stdout = String::from_utf8(out.stdout).expect("utf-8 output");
    (out.status.code(), stdout)
}

/// Runs `anno-dump <args>` and `python3 -I tools/<py> <py_args>` on the same
/// files and asserts identical stdout and exit code. Returns the stdout.
fn agree(py: &str, py_cmd: &[&str], rs_cmd: &[&str], files: &[PathBuf]) -> String {
    let py = run(Command::new("python3")
        .arg("-I")
        .arg(script(py))
        .args(py_cmd)
        .args(files));
    let rs = run(Command::new(env!("CARGO_BIN_EXE_anno-dump"))
        .args(rs_cmd)
        .args(files));
    assert_eq!(rs.1, py.1, "{rs_cmd:?} output differs");
    assert_eq!(rs.0, py.0, "{rs_cmd:?} exit code differs");
    rs.1
}

#[test]
fn anno_dump_matches_chunks_py() {
    let test = "anno_dump_matches_chunks_py";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    for sample in SAMPLES {
        let out = agree("chunks.py", &["tree"], &["chunks"], &[dir.join(sample)]);
        assert!(out.lines().count() > 1, "{sample}: empty tree");
    }
}

/// Every `.tex` and `.inc` of the corpus, plus `--full` on `Textures/debug.tex`.
#[test]
fn anno_dump_matches_tex_py_corpus() {
    let test = "anno_dump_matches_tex_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let (mut texs, mut incs) = (Vec::new(), Vec::new());
    collect(&dir, &mut texs, &mut incs);
    texs.sort();
    incs.sort();
    assert_eq!((texs.len(), incs.len()), (1214, 1213));
    let out = agree("tex.py", &["dump"], &["tex"], &texs);
    assert_eq!(out.matches("pixels=BAD").count(), 0);
    agree("tex.py", &["inc"], &["inc"], &incs);
    let debug = [dir.join("Textures/debug.tex")];
    agree("tex.py", &["dump", "--full"], &["tex", "--full"], &debug);
}

fn collect(dir: &Path, texs: &mut Vec<PathBuf>, incs: &mut Vec<PathBuf>) {
    for entry in std::fs::read_dir(dir).expect("readable game dir") {
        let path = entry.expect("dir entry").path();
        let ext = path.extension().map(|e| e.to_ascii_lowercase());
        if path.is_dir() {
            collect(&path, texs, incs);
        } else if ext.as_deref() == Some("tex".as_ref()) {
            texs.push(path);
        } else if ext.as_deref() == Some("inc".as_ref()) {
            incs.push(path);
        }
    }
}

// ---- synthetic files (no original data) ----

fn chunk(name: &str, payload: &[u8]) -> Vec<u8> {
    let mut out = vec![0; 12];
    out[..name.len()].copy_from_slice(name.as_bytes());
    let size = u32::try_from(payload.len()).expect("small payload");
    out.extend_from_slice(&size.to_le_bytes());
    out.extend_from_slice(payload);
    out
}

fn entry(name: &str, flag: u32, id: u32, subs: &[Vec<u8>]) -> Vec<u8> {
    let mut payload = [flag.to_le_bytes(), id.to_le_bytes()].concat();
    subs.iter().for_each(|s| payload.extend_from_slice(s));
    chunk(name, &payload)
}

fn palette(flag: u32, id: u32, seed: u8) -> Vec<u8> {
    let body: Vec<u8> = (0..=255u8)
        .flat_map(|i| [i ^ seed, i.wrapping_mul(3), i.wrapping_add(seed), 0])
        .collect();
    entry("COLOR", flag, id, &[chunk("BODY", &body)])
}

fn head(w: u16, h: u16, pal: u16, format: &[u8], key: [u8; 4], anchor: (u16, u16)) -> Vec<u8> {
    let mut raw = Vec::new();
    for v in [w, h, pal, 8, 0] {
        raw.extend_from_slice(&v.to_le_bytes());
    }
    let mut fmt = [0u8; 10];
    fmt[..format.len()].copy_from_slice(format);
    raw.extend_from_slice(&fmt);
    raw.extend_from_slice(&key);
    raw.extend_from_slice(&anchor.0.to_le_bytes());
    raw.extend_from_slice(&anchor.1.to_le_bytes());
    raw.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 1]);
    chunk("HEAD", &raw)
}

fn texture(id: u32, head: Vec<u8>, body: &[u8], name: Option<&[u8]>) -> Vec<u8> {
    let mut subs = vec![head, chunk("BODY", body)];
    if let Some(n) = name {
        subs.push(chunk("NAME", n));
    }
    entry("TEXTURE", 0, id, &subs)
}

fn file(entries: &[Vec<u8>]) -> Vec<u8> {
    chunk("ANNO", &entries.concat())
}

const MAGENTA: [u8; 4] = [0xff, 0, 0xff, 0];

fn synthetic_dir(name: &str) -> PathBuf {
    let dir = Path::new(env!("CARGO_TARGET_TMPDIR")).join(name);
    std::fs::create_dir_all(&dir).expect("tmp dir");
    dir
}

#[test]
fn anno_dump_matches_tex_py_synthetic() {
    let test = "anno_dump_matches_tex_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    // RLE 4×3: row 0 skip 1 + 2 px, row 1 empty, row 2 4 px with a zero-count run.
    let rle = [1, 2, 7, 8, 0xFE, 0xFE, 0, 0, 0, 4, 1, 2, 3, 4, 0xFF];
    let good = file(&[
        palette(0, 0, 0x5a),
        texture(1, head(4, 3, 0, b"BSH_RGB", MAGENTA, (2, 3)), &rle, None),
        palette(2, 7, 0x11),
        texture(
            2,
            head(3, 2, 7, b"TEX_RGB", [0, 0, 0, 0], (9, 9)),
            &[0, 1, 2, 253, 254, 255],
            Some(b"Gr\xe4s \"a\\b\".bmp\0junk"),
        ),
        texture(
            3,
            head(2, 1, 0, b"BSH_RGBH", [0xff, 0x94, 0, 0], (1, 1)),
            &[0, 2, 9, 9, 0xFF],
            None,
        ),
        // Undecodable: run past the width, unknown format.
        texture(
            4,
            head(1, 1, 0, b"BSH_RGB", MAGENTA, (0, 0)),
            &[0, 2, 1, 1, 0xFF],
            None,
        ),
        texture(5, head(1, 1, 0, b"XYZ", MAGENTA, (0, 0)), &[0], None),
    ]);
    let empty = chunk("ANNO", &[]);
    let paths: Vec<PathBuf> = [("good.tex", good), ("empty.tex", empty)]
        .into_iter()
        .map(|(n, bytes)| {
            let p = dir.join(n);
            std::fs::write(&p, bytes).expect("write");
            p
        })
        .collect();
    let out = agree("tex.py", &["dump"], &["tex"], &paths);
    assert_eq!(out.lines().filter(|l| l.starts_with("TEXTURE")).count(), 5);
    assert_eq!(out.matches("pixels=BAD").count(), 2);
    assert!(
        out.contains(r#"name="Gr\xe4s \x22a\x5cb\x22.bmp""#),
        "{out}"
    );
    let full = agree("tex.py", &["dump", "--full"], &["tex", "--full"], &paths);
    assert!(
        full.contains("\n  R0 ..0708..\n  R1 ........\n  R2 01020304\n"),
        "{full}"
    );
    // Entry 2 is stored as B=0x58 G=0x06 R=0x5c and printed as rrggbb.
    assert!(full.contains("\n  P0 5a005a 5b035b 5c0658 "), "{full}");
}

#[test]
fn anno_dump_matches_tex_py_rejects() {
    let test = "anno_dump_matches_tex_py_rejects";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let short_body = entry("COLOR", 0, 0, &[chunk("BODY", &[0; 1025])]);
    let short_head = texture(0, chunk("HEAD", &[0; 35]), &[0xFF], None);
    let cases: [(&str, Vec<u8>); 5] = [
        ("palette_1025.tex", file(&[short_body])),
        ("head_35.tex", file(&[short_head])),
        ("unknown_entry.tex", file(&[chunk("OTHER", &[0; 8])])),
        ("prefix_short.tex", file(&[chunk("COLOR", &[0; 4])])),
        ("not_chunks.tex", b"hello".to_vec()),
    ];
    for (name, bytes) in cases {
        let p = dir.join(name);
        std::fs::write(&p, bytes).expect("write");
        let out = agree("tex.py", &["dump"], &["tex"], &[p]);
        assert!(out.ends_with("\nNOPARSE\n"), "{name}: {out}");
    }
}

#[test]
fn anno_dump_matches_inc_synthetic() {
    let test = "anno_dump_matches_inc_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let cases: [(&str, Vec<u8>, bool); 6] = [
        (
            "two.inc",
            file(&[chunk("ACCESS", b"image001.pcx,1\0a,b\xfc.pcx,42\0")]),
            true,
        ),
        ("empty.inc", file(&[chunk("ACCESS", b"")]), true),
        ("no_nul.inc", file(&[chunk("ACCESS", b"x.pcx,1")]), false),
        (
            "overflow.inc",
            file(&[chunk("ACCESS", b"x.pcx,4294967296\0")]),
            false,
        ),
        (
            "two_kids.inc",
            file(&[chunk("ACCESS", b""), chunk("ACCESS", b"")]),
            false,
        ),
        (
            "trailing.inc",
            [file(&[chunk("ACCESS", b"x,1\0")]), vec![1, 2]].concat(),
            false,
        ),
    ];
    for (name, bytes, ok) in cases {
        let p = dir.join(name);
        std::fs::write(&p, bytes).expect("write");
        let out = agree("tex.py", &["inc"], &["inc"], &[p]);
        assert_eq!(!out.contains("NOPARSE"), ok, "{name}: {out}");
    }
    let out = agree("tex.py", &["inc"], &["inc"], &[dir.join("two.inc")]);
    assert!(
        out.ends_with(
            "INC records=2\nACCESS source=\"image001.pcx\" number=1\n\
             ACCESS source=\"a,b\\xfc.pcx\" number=42\n"
        ),
        "{out}"
    );
}

/// A `COLORS/ENTRY` payload per `docs/formats/palette.md`.
fn color_entry(id: u32, seed: u8, name: &[u8]) -> Vec<u8> {
    let mut p = [0u32.to_le_bytes(), id.to_le_bytes()].concat();
    p.extend_from_slice(b"BODY\0\0\x00\x04");
    p.extend((0..=255u8).flat_map(|i| [i ^ seed, i.wrapping_mul(5), i.wrapping_add(seed), 0]));
    p.extend_from_slice(b"NAME\0\0");
    p.extend_from_slice(&u16::try_from(name.len()).expect("short").to_le_bytes());
    p.extend_from_slice(name);
    chunk("ENTRY", &p)
}

fn anlagen(entries: &[Vec<u8>]) -> Vec<u8> {
    file(&[
        chunk("COLORS", &entries.concat()),
        chunk("ANLAGEN", &[0; 4]),
    ])
}

#[test]
fn anno_dump_matches_colors_synthetic() {
    let test = "anno_dump_matches_colors_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let good = anlagen(&[
        color_entry(1, 0x33, b"Farbtabellen\\Gr\xe4ser01.PCX\0"),
        color_entry(2, 0x00, b"Farbtabellen\\x.pcx\0"),
    ]);
    let mut bad_len = color_entry(1, 0, b"a.pcx\0");
    bad_len[16 + 0x0E] = 1;
    let mut inner_nul = color_entry(1, 0, b"a.pcx\0");
    inner_nul[16 + 0x418] = 0;
    let cases: [(&str, Vec<u8>, bool); 5] = [
        ("good.dat", good, true),
        ("no_colors.dat", file(&[chunk("ANLAGEN", &[0; 4])]), false),
        ("body_len.dat", anlagen(&[bad_len]), false),
        ("inner_nul.dat", anlagen(&[inner_nul]), false),
        ("not_entry.dat", anlagen(&[chunk("OTHER", &[0; 4])]), false),
    ];
    for (name, bytes, ok) in cases {
        let p = dir.join(name);
        std::fs::write(&p, bytes).expect("write");
        let out = agree("tex.py", &["colors"], &["colors"], std::slice::from_ref(&p));
        assert_eq!(!out.contains("NOPARSE"), ok, "{name}: {out}");
        if ok {
            assert!(out.contains("COLORS entries=2\n"), "{out}");
            assert!(
                out.contains(r#"name="Farbtabellen\x5cGr\xe4ser01.PCX""#),
                "{out}"
            );
            let full = agree("tex.py", &["colors", "--full"], &["colors", "--full"], &[p]);
            // Entry 1 of table 1 is stored B=0x32 G=0x05 R=0x34, printed rrggbb.
            assert!(full.contains("\n  P0 330033 340532 350a31 "), "{full}");
        }
    }
}

#[test]
fn anno_dump_matches_colors_corpus() {
    let test = "anno_dump_matches_colors_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let anlagen = [dir.join("Anlagen.dat")];
    let out = agree("tex.py", &["colors"], &["colors"], &anlagen);
    assert!(out.contains("COLORS entries=440\n"), "{out}");
    agree(
        "tex.py",
        &["colors", "--full"],
        &["colors", "--full"],
        &anlagen,
    );
}

#[test]
fn encode_rle_matches_tex_py_synthetic() {
    let test = "encode_rle_matches_tex_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    // 900×3: long gap + long span, fully transparent row, span ending at the
    // right edge. Bodies from the Rust encoder must re-encode identically in
    // tools/tex.py; the hand-made non-canonical body (empty row as a bare
    // 0xFE) must not.
    let (w, h) = (900, 3);
    let index = |i: usize| u8::try_from(i % 200).expect("< 200");
    let mut px = vec![None; 300];
    px.extend((0..600).map(|i| Some(index(i))));
    px.extend(vec![None; w]);
    px.extend((0..w).map(|i| (i >= 400).then_some(index(i % 7))));
    let body = anno_formats::tex::encode_rle(&px, w, h);
    let canonical = file(&[
        palette(0, 0, 0x5a),
        texture(1, head(900, 3, 0, b"BSH_RGB", MAGENTA, (0, 0)), &body, None),
    ]);
    let other = file(&[
        palette(0, 0, 0x5a),
        texture(
            1,
            head(2, 2, 0, b"BSH_RGB", MAGENTA, (0, 0)),
            &[0xFE, 0, 2, 1, 1, 0xFF],
            None,
        ),
    ]);
    let run_py = |name: &str, bytes: Vec<u8>| {
        let p = dir.join(name);
        std::fs::write(&p, bytes).expect("write");
        let out = run(Command::new("python3")
            .arg("-I")
            .arg(script("tex.py"))
            .arg("reencode")
            .arg(&p));
        (
            out.0,
            out.1
                .trim_end()
                .rsplit(' ')
                .take(2)
                .collect::<Vec<_>>()
                .join(" "),
        )
    };
    assert_eq!(
        run_py("canonical.tex", canonical),
        (Some(0), "identical=1 rle=1".to_string())
    );
    assert_eq!(
        run_py("other.tex", other),
        (Some(1), "identical=0 rle=1".to_string())
    );
}

/// `FigurenHeader.dat` (`--full`) and decoded frames of one entry per format.
#[test]
fn anno_dump_matches_figuren_py_corpus() {
    let test = "anno_dump_matches_figuren_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let header = [dir.join("FigurenHeader.dat")];
    let out = agree(
        "figuren.py",
        &["dump", "--full"],
        &["figuren", "--full"],
        &header,
    );
    assert!(out.contains("FIGUREN entries=368 volumes=3\n"));
    // Formats V, Z, AZ, AV, ALPHA (shadows of 101/103), H.
    let ids = ["101", "103", "1510", "1539", "1546", "1574"];
    let args: Vec<PathBuf> = std::iter::once(dir.clone())
        .chain(ids.iter().map(PathBuf::from))
        .collect();
    let out = agree("figuren.py", &["frames"], &["frames"], &args);
    assert_eq!(out.matches("pixels=BAD").count(), 0);
}

fn frame_header(w: u16, h: u16, format: &[u8], offset: u32, length: u32) -> Vec<u8> {
    let mut raw = Vec::new();
    for v in [w, h, 1, 8] {
        raw.extend_from_slice(&v.to_le_bytes());
    }
    let mut fmt = [0u8; 12];
    fmt[..format.len()].copy_from_slice(format);
    raw.extend_from_slice(&fmt);
    for v in [w / 2, h] {
        raw.extend_from_slice(&v.to_le_bytes());
    }
    for v in [offset, length, 0] {
        raw.extend_from_slice(&v.to_le_bytes());
    }
    raw
}

fn frame_set(group: u32, frames: &[Vec<u8>]) -> Vec<u8> {
    let n = u32::try_from(frames.len()).expect("few frames");
    let mut payload = [group.to_le_bytes(), n.to_le_bytes()].concat();
    frames.iter().for_each(|f| payload.extend_from_slice(f));
    chunk("TEXTURES", &payload)
}

#[test]
fn anno_dump_matches_figuren_py_synthetic() {
    let test = "anno_dump_matches_figuren_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    // Group 0 volume: one body per format, then a malformed V body.
    let bodies: [(&[u8], &[u8], u16, u16); 7] = [
        (b"BSH_RGBV", &[1, 2, 7, 8, 0xFE, 0, 3, 1, 2, 3, 0xFF], 3, 2),
        (b"BSH_RGBZ", &[0, 1, 0xfb, 0x44, 0xFF], 1, 1),
        (b"BSH_RGBAV", &[1, 0xC2, 5, 6, 0xFF], 3, 1),
        (b"BSH_RGBAZ", &[0, 0x41, 9, 8, 0xFF], 1, 1),
        (b"BSH_RGBH", &[0, 1, 9, 0xFF], 1, 1),
        (b"BSH_ALPHA", &[1, 2, 0xFE, 0, 1, 0xFF], 3, 2),
        (b"BSH_RGBV", &[0, 3, 1, 1, 1, 0xFF], 2, 1),
    ];
    let (mut vol, mut frames) = (Vec::new(), Vec::new());
    for (fmt, body, w, h) in bodies {
        let off = u32::try_from(vol.len()).expect("small");
        let len = u32::try_from(body.len()).expect("small");
        frames.push(frame_header(w, h, fmt, off, len));
        vol.extend_from_slice(body);
    }
    // Empty frame and an unknown format.
    frames.push(frame_header(4, 4, b"BSH_RGBV", 0, 0));
    frames.push(frame_header(1, 1, b"XYZ", 0, 1));
    std::fs::write(dir.join("Figuren000.dat"), &vol).expect("write");
    std::fs::write(dir.join("Figuren100.dat"), [0, 1, 3, 0xFF]).expect("write");
    std::fs::write(dir.join("Figuren200.dat"), []).expect("write");
    let one = |g| {
        frame_set(
            g,
            &[frame_header(
                1,
                1,
                b"BSH_RGBV",
                0,
                if g == 1 { 4 } else { 0 },
            )],
        )
    };
    let mut shadow = vec![3, 0, 0, 0, 0, 0, 0, 0];
    shadow.extend(chunk("BODY2", &[1; 44]));
    shadow.extend(frame_set(0, &frames[5..6]));
    let mut set = vec![4, 0, 4, 0, 0, 0, 0, 0, b'x', b'y', 0, 0, 0, 0, 0, 0];
    set.extend(chunk("BODY2", &[0; 44]));
    set.extend(chunk("GFXDATA", b"Gr\xe4s\\a.tga\0"));
    set.extend(frame_set(0, &frames));
    set.extend(one(1));
    set.extend(one(2));
    set.extend(chunk("SHADOW", &shadow));
    let mut table = [0u32.to_le_bytes(), 0u32.to_le_bytes()].concat();
    table.extend_from_slice(b"BODY\0\0\x00\x04");
    table.extend((0..=255u8).flat_map(|i| [i, i ^ 0x5a, 255 - i, 0]));
    table.extend_from_slice(b"NAME\0\0\x08\0\"b\\.pcx\0");
    let e1 = entry(
        "ENTRY",
        0,
        1,
        &[
            chunk("BEWOHNTYPE", b"EXPLOSION\0"),
            chunk("COLORS", &[]),
            chunk("HAEUSER", &[]),
        ],
    );
    let e7 = entry(
        "ENTRY",
        0,
        7,
        &[
            chunk("PATH", b"Maeher\\\0"),
            chunk("COLORS", &chunk("ENTRY", &table)),
            chunk("HAEUSER", &chunk("ENTRY", &set)),
        ],
    );
    let mut buf = b"ANNO\0\0\0\0\0\0\0\0\x01\x02\x03\x04".to_vec();
    buf.extend(chunk("FIGUREN", &[e1, e7].concat()));
    for g in 0..3u32 {
        buf.extend_from_slice(b"TEXTURES\0\0\0\0");
        for v in [8 + g, g, 9] {
            buf.extend_from_slice(&v.to_le_bytes());
        }
    }
    std::fs::write(dir.join("FigurenHeader.dat"), &buf).expect("write");
    let bad = dir.join("bad.dat");
    std::fs::write(&bad, b"ANNO\0\0\0\0\0\0\0\0\0\0\0\0").expect("write");
    let header = [dir.join("FigurenHeader.dat"), bad];
    let out = agree(
        "figuren.py",
        &["dump", "--full"],
        &["figuren", "--full"],
        &header,
    );
    assert!(out.contains("NOPARSE\n"), "{out}");
    assert!(out.contains(r#"name="\x22b\x5c.pcx""#), "{out}");
    agree("figuren.py", &["dump"], &["figuren"], &header[..1]);
    let args = [
        dir.clone(),
        PathBuf::from("7"),
        PathBuf::from("1"),
        PathBuf::from("99"),
    ];
    let out = agree("figuren.py", &["frames"], &["frames"], &args);
    assert_eq!(out.matches("pixels=BAD").count(), 2, "{out}");
    assert!(out.contains("ENTRY id=99 MISSING"), "{out}");
    // AV level 3 pixel: hash covers (1, index, z, level).
    assert!(out.contains("format=\"BSH_RGBAV\" opaque=2 "), "{out}");
}

/// Every `.scp` and `.sze`, plus `--full` on one scenario.
#[test]
fn anno_dump_matches_insel_py_corpus() {
    let test = "anno_dump_matches_insel_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let mut files = Vec::new();
    collect_ext(&dir.join("Scenes"), &["scp", "sze"], &mut files);
    files.sort();
    assert_eq!(files.len(), 515);
    let out = agree("insel.py", &["dump"], &["insel"], &files);
    assert_eq!(out.matches(" BAD").count(), 0);
    let one = [dir.join("Scenes/0200Single/Single_01.sze")];
    agree("insel.py", &["dump", "--full"], &["insel", "--full"], &one);
}

fn collect_ext(dir: &Path, exts: &[&str], out: &mut Vec<PathBuf>) {
    for entry in std::fs::read_dir(dir).expect("readable dir") {
        let path = entry.expect("dir entry").path();
        let ext = path.extension().map(|e| e.to_ascii_lowercase());
        if path.is_dir() {
            collect_ext(&path, exts, out);
        } else if exts.iter().any(|x| ext.as_deref() == Some(x.as_ref())) {
            out.push(path);
        }
    }
}

/// One block record: header + cells.
fn island_block(name: &str, x: u16, y: u16, w: u8, h: u8, cells: &[u8]) -> Vec<u8> {
    let mut p = [x.to_le_bytes(), y.to_le_bytes()].concat();
    p.extend_from_slice(&[w, h]);
    p.extend_from_slice(cells);
    chunk(name, &p)
}

#[test]
fn anno_dump_matches_insel_py_synthetic() {
    let test = "anno_dump_matches_insel_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    // 34 × 2 island: blocks 32×2 and 2×2; GFXID cells little-endian.
    let heights: Vec<u8> = (0..68u8).collect();
    let row = |y: usize, x0: usize, n: usize| heights[y * 34 + x0..y * 34 + x0 + n].to_vec();
    let gfx = |hs: &[u8]| {
        hs.iter()
            .flat_map(|&v| [v, 0x80 | (v & 3)])
            .collect::<Vec<u8>>()
    };
    let left = [row(0, 0, 32), row(1, 0, 32)].concat();
    let right = [row(0, 32, 2), row(1, 32, 2)].concat();
    let mut recs = vec![
        chunk("INFO", &[1; 12]),
        island_block("HEIGHT", 0, 0, 32, 2, &left),
        island_block("HEIGHT", 32, 0, 2, 2, &right),
        island_block("GFXID", 0, 0, 32, 2, &gfx(&left)),
        island_block("GFXID", 32, 0, 2, 2, &gfx(&right)),
        island_block("GELAENDE", 0, 0, 32, 2, &[1, 2, 3, 4, 5, 6]),
        chunk("FRUCHTBAR", &[7; 68]),
        island_block("FLUESSE", 8, 0, 8, 8, &[9; 12]),
        chunk("NAME", b"Insel \xc4\0"),
    ];
    let head = |x: u16, n: u32| {
        [
            &x.to_le_bytes()[..],
            &3u16.to_le_bytes(),
            &34u16.to_le_bytes(),
            &2u16.to_le_bytes(),
            &n.to_le_bytes(),
        ]
        .concat()
    };
    let insel5 = |x, n, recs: &[Vec<u8>]| chunk("INSEL5", &[head(x, n), recs.concat()].concat());
    let scp = chunk(
        "ANNO",
        &[chunk("WARETYPE", &[0; 4]), insel5(0, 0, &recs)].concat(),
    );
    let sze = chunk(
        "ANNOADDON1",
        &chunk(
            "SCENE",
            &[
                chunk("HEAD", &[0; 8]),
                insel5(100, 1, &recs),
                insel5(200, 2, &recs),
            ]
            .concat(),
        ),
    );
    // Gap: drop the second HEIGHT block.
    recs.remove(2);
    let gap = chunk("ANNO", &insel5(0, 0, &recs));
    let mut insel4 = chunk("INSEL4", &[2, 0, 1, 0, 0, 0, 0, 0]);
    insel4.extend(island_block("HEIGHT", 0, 0, 2, 1, &[5, 6]));
    insel4.extend(island_block("GFXID", 0, 0, 2, 1, &[1, 0, 2, 0]));
    insel4.extend(chunk("LOCKED", &[1]));
    let files: Vec<PathBuf> = [
        ("a.scp", scp),
        ("b.sze", sze),
        ("gap.scp", gap),
        ("old.scp", chunk("ANNO", &insel4)),
        ("bad.scp", b"not a chunk file".to_vec()),
    ]
    .into_iter()
    .map(|(n, bytes)| {
        let p = dir.join(n);
        std::fs::write(&p, bytes).expect("write");
        p
    })
    .collect();
    let out = agree(
        "insel.py",
        &["dump", "--full"],
        &["insel", "--full"],
        &files,
    );
    assert!(out.contains("ISLANDS count=2\n"), "{out}");
    assert!(out.contains("GRID BAD tile not covered"), "{out}");
    assert!(out.contains("version=4 x=0 y=0 width=2 height=1"), "{out}");
    assert!(out.contains("    G0 00010002\n"), "{out}");
    assert!(out.contains("NOPARSE\n"), "{out}");
    agree("insel.py", &["dump"], &["insel"], &files);
}

/// `Anlagen.dat` with `--full`.
#[test]
fn anno_dump_matches_anlagen_py_corpus() {
    let test = "anno_dump_matches_anlagen_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let file = [dir.join("Anlagen.dat")];
    let out = agree(
        "figuren.py",
        &["anlagen", "--full"],
        &["anlagen", "--full"],
        &file,
    );
    assert!(out.contains("ANLAGEN entries=921 volumes=3\n"));
}

#[test]
fn anno_dump_matches_anlagen_py_synthetic() {
    let test = "anno_dump_matches_anlagen_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let mut table = [0u32.to_le_bytes(), 1u32.to_le_bytes()].concat();
    table.extend_from_slice(b"BODY\0\0\x00\x04");
    table.extend((0..=255u8).flat_map(|i| [i, 1, 2, 0]));
    table.extend_from_slice(b"NAME\0\0\x06\0a.pcx\0");
    let mut set = vec![0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    set.extend(chunk("BODY2", &[0; 44]));
    for g in 0..3 {
        set.extend(frame_set(g, &[frame_header(2, 1, b"BSH_RGBWZ", 0, 7)]));
    }
    let ids = [1u32, 0]
        .iter()
        .flat_map(|i| i.to_le_bytes())
        .collect::<Vec<u8>>();
    let e = entry(
        "ENTRY",
        0,
        20,
        &[
            chunk("HEAD", &[9; 52]),
            chunk("COLORS", &ids),
            chunk("HAEUSER", &chunk("ENTRY", &set)),
        ],
    );
    let vol = |g: u32| {
        let mut p = [g.to_le_bytes(), 1u32.to_le_bytes()].concat();
        p.extend_from_slice(&[0, 2, 0xfb, 5, 0xfa, 6, 0xFF]);
        chunk("TEXTURES", &p)
    };
    let good = chunk(
        "ANNO",
        &[
            chunk("COLORS", &chunk("ENTRY", &table)),
            chunk("ANLAGEN", &e),
            vol(0),
            vol(1),
            vol(2),
        ]
        .concat(),
    );
    let files: Vec<PathBuf> = [
        ("Anlagen.dat", good),
        ("bad.dat", chunk("ANNO", &chunk("COLORS", &[]))),
    ]
    .into_iter()
    .map(|(n, bytes)| {
        let p = dir.join(n);
        std::fs::write(&p, bytes).expect("write");
        p
    })
    .collect();
    let out = agree(
        "figuren.py",
        &["anlagen", "--full"],
        &["anlagen", "--full"],
        &files,
    );
    assert!(out.contains("  COLORIDS ids=1,0\n"), "{out}");
    assert!(out.contains("format=\"BSH_RGBWZ\""), "{out}");
    assert!(out.contains("NOPARSE\n"), "{out}");
}

/// Every `.sze`, with and without `--full`.
#[test]
fn anno_dump_matches_sze_py_corpus() {
    let test = "anno_dump_matches_sze_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let mut files = Vec::new();
    collect_ext(&dir.join("Scenes"), &["sze"], &mut files);
    files.sort();
    assert_eq!(files.len(), 50);
    let out = agree("sze.py", &["dump"], &["sze"], &files);
    assert_eq!(out.matches("UNIT SCHIFF id=").count(), 327);
    assert_eq!(out.matches("UNIT SOLDAT id=").count(), 2317);
    let full = agree("sze.py", &["dump", "--full"], &["sze", "--full"], &files);
    assert_eq!(full.matches("BAD").count(), 0);
}

/// One tagged record: 6-byte name, `u16` length (long form: `0xFF00`, then
/// the `u16` length), payload.
fn tagged(name: &str, long: bool, payload: &[u8]) -> Vec<u8> {
    let mut out = vec![0; 6];
    out[..name.len()].copy_from_slice(name.as_bytes());
    if long {
        out.extend_from_slice(&0xFF00u16.to_le_bytes());
    }
    let len = u16::try_from(payload.len()).expect("small payload");
    out.extend_from_slice(&len.to_le_bytes());
    out.extend_from_slice(payload);
    out
}

#[test]
fn anno_dump_matches_sze_py_synthetic() {
    let test = "anno_dump_matches_sze_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let unit = |id: u16, figure: u16, recs: &[u8]| {
        let mut p = [id.to_le_bytes(), figure.to_le_bytes(), [1, 9], [0, 0]].concat();
        p.extend_from_slice(&10.5f32.to_le_bytes());
        p.extend_from_slice(&(-3.25f32).to_le_bytes());
        p.extend_from_slice(recs);
        p
    };
    let crew = tagged(
        "CREW",
        false,
        &tagged(
            "ENTRY",
            false,
            &[
                [5, 0, 0xcd, 2, 1, 0].as_slice(),
                &tagged("HEAD", false, &[1; 16]),
            ]
            .concat(),
        ),
    );
    let ship = unit(
        2,
        1502,
        &[
            tagged("HEAD", false, &[0; 16]),
            tagged(
                "PRIVAT",
                false,
                &[tagged("NAME", false, b"Jana\0"), crew].concat(),
            ),
        ]
        .concat(),
    );
    let stadt = [
        [0u8, 0, 1, 2, 3, 4].as_slice(),
        &tagged("HEAD2", false, &[0; 4]),
        &tagged("PROD", true, &tagged("ENTRY", false, &[7; 3])),
    ]
    .concat();
    let player = |index: u32, recs: &[u8]| {
        [[0; 4], index.to_le_bytes()]
            .concat()
            .into_iter()
            .chain(recs.iter().copied())
            .collect::<Vec<u8>>()
    };
    let good = chunk(
        "ANNOADDON1",
        &chunk(
            "SCENE",
            &[
                chunk("HEAD", &[0; 8]),
                chunk(
                    "INSELLNK",
                    b"\x20\x00\x18\x00\x50\x00\x40\x00\x04\x00\x00\x00islands\\a\"b.scp\0",
                ),
                chunk(
                    "INSELLNK",
                    b"\x20\x00\x18\x00\x50\x00\x40\x00\x04\x00\x00\x00no nul",
                ),
                chunk("SPIELER", &player(0, &tagged("HEAD", false, &[3; 4]))),
                chunk("SPIELER", &player(31, &[])),
                chunk(
                    "SPIELER",
                    &player(
                        0,
                        &[
                            tagged("SCHIFF", false, &ship),
                            tagged("STADT2", true, &stadt),
                            tagged("DIPL", false, b"not records"),
                        ]
                        .concat(),
                    ),
                ),
                // Bad: name with garbage after the NUL, a unit shorter
                // than its prefix, a payload past the end, a short block.
                chunk("SPIELER", &player(1, b"HEAD\0x\x00\x00")),
                chunk("SPIELER", &player(2, &tagged("SOLDAT", false, &[1; 6]))),
                chunk("SPIELER", &player(3, b"HEAD\0\0\x09\x00ab")),
                chunk("SPIELER", &[1, 2, 3]),
                // Long form with a 24-bit length (0x1_0002 bytes).
                chunk(
                    "SPIELER",
                    &player(
                        4,
                        &[&b"QUEST\0\x01\xff\x02\x00"[..], &vec![5u8; 0x1_0002]].concat(),
                    ),
                ),
            ]
            .concat(),
        ),
    );
    let files: Vec<PathBuf> = [
        ("good.sze", good),
        ("noscene.sze", chunk("ANNO", &chunk("HEAD", &[0; 8]))),
        ("bad.sze", b"not a chunk file".to_vec()),
    ]
    .into_iter()
    .map(|(n, bytes)| {
        let p = dir.join(n);
        std::fs::write(&p, bytes).expect("write");
        p
    })
    .collect();
    let out = agree("sze.py", &["dump", "--full"], &["sze", "--full"], &files);
    assert!(
        out.contains("SCENE edition=ANNOADDON1 children=11 players=8\n"),
        "{out}"
    );
    assert!(
        out.contains("  UNIT SCHIFF id=2 figure=1502 owner=1 attr=9 reserved=0 x=41280000 y=c0500000 records=HEAD,PRIVAT\n"),
        "{out}"
    );
    assert!(
        out.contains(
            "LINK x=32 y=24 width=80 height=64 number=4 path=\"islands\\x5ca\\x22b.scp\"\nLINK BAD\n"
        ),
        "{out}"
    );
    assert!(out.contains("  RECORD STADT2 size=39 long=1 "), "{out}");
    assert!(
        out.contains("    PREFIX 000001020304\n    HEAD2 size=4 long=0 "),
        "{out}"
    );
    assert!(out.contains("    PROD size=11 long=1 "), "{out}");
    assert!(out.contains("          PREFIX 0500cd020100\n"), "{out}");
    assert!(
        out.contains("  RECORD DIPL size=11 long=0 hash=") && out.contains("    NESTED BAD\n"),
        "{out}"
    );
    assert!(out.contains("PLAYER i=3 index=1 flag=0 BAD\n"), "{out}");
    assert!(out.contains("  UNIT SOLDAT BAD\n"), "{out}");
    assert!(out.contains("PLAYER i=5 index=3 flag=0 BAD\n"), "{out}");
    assert!(out.contains("PLAYER i=6 BAD\n"), "{out}");
    assert!(
        out.contains("PLAYER i=7 index=4 flag=0 records=1\n  RECORD QUEST size=65538 long=1 "),
        "{out}"
    );
    assert_eq!(out.matches("NOPARSE\n").count(), 2, "{out}");
    agree("sze.py", &["dump"], &["sze"], &files);
}

/// The keyed `Data/` files (and the two tool batch files) and `Texte.dat`,
/// with and without `--full`.
#[test]
fn anno_dump_matches_text_py_corpus() {
    let test = "anno_dump_matches_text_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let mut files = Vec::new();
    collect_ext(&dir.join("Data"), &["dat", "bat"], &mut files);
    files.retain(|f| !f.ends_with("Debug.dat")); // a Bink video
    files.sort();
    assert_eq!(files.len(), 12);
    let out = agree("text.py", &["keyed"], &["keyed"], &files);
    assert_eq!(out.matches("\nOTHER ").count(), 8, "{out}");
    let texte = [dir.join("Texte.dat")];
    let out = agree("text.py", &["texte"], &["texte"], &texte);
    assert_eq!(out.matches("\n  ID id=").count(), 326);
    agree(
        "text.py",
        &["texte", "--full"],
        &["texte", "--full"],
        &texte,
    );
}

#[test]
fn anno_dump_matches_text_py_synthetic() {
    let test = "anno_dump_matches_text_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let keyed =
        b"; head\r\n[WAREN]  ; c\r\n  Id:\t\tEISENERZ\r\nName : \"a;b, c\" ; x\r\nProz: 1, 2,3\n\
                  Pos  0, 0\r\nEndObj;\r\n[]\r\nKey:\r\n\xe4:1\r\nlast";
    let texte = b"pre\r\n[ANLAGEN]\r\n104:Forest\r\nbad line\r\n007:B\xe4r\r\n[S:X]\r\nHi \"you\"\\n\r\n\r\n[END]\r\n";
    let files: Vec<PathBuf> = [("keyed.dat", &keyed[..]), ("texte.dat", &texte[..])]
        .into_iter()
        .map(|(n, bytes)| {
            let p = dir.join(n);
            std::fs::write(&p, bytes).expect("write");
            p
        })
        .collect();
    let out = agree("text.py", &["keyed"], &["keyed"], &files[..1]);
    for line in [
        "LINES count=11 crlf=9 final=0\n",
        "SECTION line=2 name=\"WAREN\"\n",
        "ENTRY line=3 key=\"Id\" fields=1 value=\"EISENERZ\"\n",
        "ENTRY line=4 key=\"Name\" fields=1 value=\"\\x22a;b, c\\x22\"\n",
        "ENTRY line=5 key=\"Proz\" fields=3 value=\"1, 2,3\"\n",
        "OTHER line=6 text=\"Pos  0, 0\"\n",
        "OTHER line=7 text=\"EndObj\"\n",
        "OTHER line=8 text=\"[]\"\n",
        "ENTRY line=9 key=\"Key\" fields=0 value=\"\"\n",
        "OTHER line=10 text=\"\\xe4:1\"\n",
        "OTHER line=11 text=\"last\"\n",
    ] {
        assert!(out.contains(line), "{line}\n{out}");
    }
    let out = agree(
        "text.py",
        &["texte", "--full"],
        &["texte", "--full"],
        &files[1..],
    );
    for line in [
        "TEXTE sections=3 preamble=1\n",
        "  ID id=104 text=\"Forest\"\n  ID BAD text=\"bad line\"\n  ID id=7 text=\"B\\xe4r\"\n",
        "SECTION line=6 name=\"S:X\" lines=2 ",
        "  L \"Hi \\x22you\\x22\\x5cn\"\n  L \"\"\n",
        "SECTION line=9 name=\"END\" lines=0 ",
    ] {
        assert!(out.contains(line), "{line}\n{out}");
    }
    agree("text.py", &["texte"], &["texte"], &files[1..]);
}

/// Every `.zei`, with and without `--full`.
#[test]
fn anno_dump_matches_zei_py_corpus() {
    let test = "anno_dump_matches_zei_py_corpus";
    let Some(dir) = game_dir(test) else { return };
    if !have_python(test) {
        return;
    }
    let mut files = Vec::new();
    collect_ext(&dir, &["zei"], &mut files);
    files.sort();
    assert_eq!(files.len(), 11);
    let out = agree("zei.py", &["dump"], &["zei"], &files);
    assert_eq!(out.matches("\nGLYPH ").count(), 11 * 224);
    assert_eq!(out.matches(" BAD").count(), 0);
    agree("zei.py", &["dump", "--full"], &["zei", "--full"], &files);
}

#[test]
fn anno_dump_matches_zei_py_synthetic() {
    let test = "anno_dump_matches_zei_py_synthetic";
    if !have_python(test) {
        return;
    }
    let dir = synthetic_dir(test);
    let font = |glyphs: &[(u8, u32, &[u8])]| {
        let mut head = vec![0u8; 16 + 256 * 8];
        head[0] = 11;
        head[2] = 3;
        head[4] = 9; // reserved byte, printed
        let mut body = Vec::new();
        for &(code, width, data) in glyphs {
            let o = 16 + 8 * usize::from(code);
            head[o..o + 4].copy_from_slice(&width.to_le_bytes());
            let len = u32::try_from(data.len()).unwrap();
            head[o + 4..o + 8].copy_from_slice(&len.to_le_bytes());
            body.extend_from_slice(data);
        }
        chunk(
            "CHARSET",
            &[chunk("HEAD", &head), chunk("BODY", &body)].concat(),
        )
    };
    let good = font(&[
        (b'A', 3, &[1, 2, 1, 2, 0xFE, 0xFE, 0, 1, 3, 0xFF]),
        (b'B', 2, &[3, 0, 0xFF]),     // skip past the width: not widened
        (b'C', 2, &[2, 1, 12, 0xFF]), // pixel past the width: widened, level ≥ 10
        (b'D', 2, &[0, 5, 1, 0xFF]),  // truncated run: BAD
    ]);
    let files: Vec<PathBuf> = [
        ("good.zei", good),
        ("bad.zei", chunk("CHARSET", &chunk("HEAD", &[0; 4]))),
    ]
    .into_iter()
    .map(|(n, bytes)| {
        let p = dir.join(n);
        std::fs::write(&p, bytes).expect("write");
        p
    })
    .collect();
    let out = agree("zei.py", &["dump", "--full"], &["zei", "--full"], &files);
    for line in [
        "CHARSET size_a=11 size_b=3 reserved=090000000000000000000000 glyphs=4\n",
        "GLYPH code=65 width=3 length=10 image=3x3 ",
        "  .12\n  ...\n  3..\n",
        "GLYPH code=66 width=2 length=3 image=2x1 ",
        "GLYPH code=67 width=2 length=4 image=3x1 ",
        "  ..#\n",
        "GLYPH code=68 width=2 length=4 BAD\n",
        "NOPARSE\n",
    ] {
        assert!(out.contains(line), "{line}\n{out}");
    }
    agree("zei.py", &["dump"], &["zei"], &files);
}
