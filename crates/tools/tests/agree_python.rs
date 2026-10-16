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
