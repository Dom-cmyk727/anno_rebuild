//! `anno-dump probe` on a synthetic file shaped like a `.tex` `COLOR` list.

use std::path::Path;
use std::process::Command;

fn chunk(name: &str, payload: &[u8]) -> Vec<u8> {
    let mut out = vec![0; 12];
    out[..name.len()].copy_from_slice(name.as_bytes());
    let size = u32::try_from(payload.len()).expect("small payload");
    out.extend_from_slice(&size.to_le_bytes());
    out.extend_from_slice(payload);
    out
}

#[test]
fn probe_finds_prefix_and_fields() {
    let pal: Vec<u8> = (0..4).flat_map(|_| 0..=255u8).collect();
    let entries: Vec<u8> = (0..3u32)
        .flat_map(|i| {
            let payload = [
                &0u32.to_le_bytes()[..],
                &i.to_le_bytes(),
                &chunk("BODY", &pal),
            ]
            .concat();
            chunk("ENTRY", &payload)
        })
        .collect();
    let file = chunk(
        "ANNO",
        &[chunk("COLORS", &entries), chunk("OTHER", &[1; 20])].concat(),
    );
    let dir = Path::new(env!("CARGO_TARGET_TMPDIR")).join("probe");
    std::fs::create_dir_all(&dir).expect("tmp dir");
    let path = dir.join("fake.dat");
    std::fs::write(&path, file).expect("write");
    let out = Command::new(env!("CARGO_BIN_EXE_anno-dump"))
        .args(["probe", "ANNO/COLORS/ENTRY"])
        .arg(&path)
        .output()
        .expect("spawn");
    assert!(out.status.success());
    let text = String::from_utf8(out.stdout).expect("utf-8");
    for line in [
        "PROBE path=ANNO/COLORS/ENTRY count=3",
        "SIZE 1048 x3",
        "CHILDREN leaf x3",
        "PREFIX 8 BODY:1024 x3",
        "FIELDS size=1048 entries=3",
        "U32 +0x04 distinct=3 min=0 max=2 values=0x0,0x1,0x2",
    ] {
        assert!(
            text.lines().any(|l| l == line),
            "missing {line:?} in\n{text}"
        );
    }
}
