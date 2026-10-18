//! Reference digests for the C++ port (ADR 0001): FNV-1a 64 of every
//! decoded image, in the order and encoding of
//! `src/assets/tests/assets_corpus_test.cpp` (recipe: `docs/render/assets.md`,
//! "Corpus check"). Prints one line per set; the C++ test asserts the same
//! values.
//!
//! ```text
//! cargo run --release -p anno-assets --example digest -- <game_dir>
//! ```
use std::path::{Path, PathBuf};

use anno_assets::{building, figure, island, texture, unit};
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, FigurenHeader, GfxSet};
use anno_formats::{insel, sze, tex};

struct Fnv(u64, usize, usize);
impl Fnv {
    fn new() -> Self {
        Fnv(0xcbf2_9ce4_8422_2325, 0, 0)
    }
    fn bytes(&mut self, b: &[u8]) {
        for &x in b {
            self.0 ^= u64::from(x);
            self.0 = self.0.wrapping_mul(0x0100_0000_01b3);
        }
    }
    fn img(&mut self, r: Option<&texture::Rgba>) {
        match r {
            Some(r) => {
                self.1 += 1;
                self.bytes(&r.width.to_le_bytes());
                self.bytes(&r.height.to_le_bytes());
                self.bytes(&r.pixels);
            }
            None => {
                self.2 += 1;
                self.bytes(&[0xFF; 4]);
            }
        }
    }
    fn line(&self, name: &str) {
        println!("{name} ok={} failed={} digest={:016x}", self.1, self.2, self.0);
    }
}

fn files(dir: &Path, ext: &str, out: &mut Vec<PathBuf>) {
    for e in std::fs::read_dir(dir).expect("dir") {
        let p = e.expect("entry").path();
        if p.is_dir() {
            files(&p, ext, out);
        } else if p.extension().is_some_and(|x| x.eq_ignore_ascii_case(ext)) {
            out.push(p);
        }
    }
}

fn sets<'a, 'b>(g: &'b GfxSet<'a>) -> Vec<&'b GfxSet<'a>> {
    let mut v = vec![g];
    if let Some(s) = &g.shadow {
        v.push(s);
    }
    v
}

fn main() {
    let dir = PathBuf::from(std::env::args().nth(1).expect("game dir"));
    // Textures.
    let mut t = Fnv::new();
    let mut texs = Vec::new();
    files(&dir, "tex", &mut texs);
    texs.sort();
    for p in &texs {
        let buf = std::fs::read(p).expect("read");
        let Ok(f) = tex::TexFile::parse(&buf) else {
            t.bytes(b"NOPARSE");
            continue;
        };
        for x in &f.textures {
            t.img(texture::to_rgba(&f, x).ok().as_ref());
        }
    }
    t.line("tex");
    // Buildings.
    let abuf = std::fs::read(dir.join("Anlagen.dat")).expect("Anlagen.dat");
    let a = AnlagenFile::parse(&abuf).expect("parses");
    let mut b = Fnv::new();
    for e in &a.entries {
        for g in &e.gfx {
            for s in sets(g) {
                for fs in &s.frame_sets {
                    for f in &fs.frames {
                        b.img(building::to_rgba(&a, e, fs.group, f).ok().as_ref());
                    }
                }
            }
        }
    }
    b.line("anlagen");
    // Figures, plain and (player-coloured types) in colour 2 right after.
    let hbuf = std::fs::read(dir.join("FigurenHeader.dat")).expect("FigurenHeader.dat");
    let h = FigurenHeader::parse(&hbuf).expect("parses");
    let vols: Vec<Vec<u8>> = (0..3)
        .map(|g| {
            let mut v = Vec::new();
            for part in 0.. {
                match std::fs::read(dir.join(figuren::volume_file_name(g, part))) {
                    Ok(d) => v.extend(d),
                    Err(_) => break,
                }
            }
            v
        })
        .collect();
    let mut fg = Fnv::new();
    let red = figure::ORIGINAL_PLAYER_COLOURS[1];
    for e in &h.entries {
        for g in &e.gfx {
            for s in sets(g) {
                for fs in &s.frame_sets {
                    let vol = &vols[usize::try_from(fs.group).expect("group")];
                    for f in &fs.frames {
                        fg.img(figure::to_rgba(e, f, vol).ok().as_ref());
                        if figure::player_coloured(e) {
                            let start = usize::try_from(f.offset).expect("fits");
                            let end = start + usize::try_from(f.length).expect("fits");
                            let img = vol
                                .get(start..end)
                                .and_then(|d| figure::decode_coloured(e, f, d, Some(red)).ok());
                            fg.img(img.as_ref());
                        }
                    }
                }
            }
        }
    }
    fg.line("figuren");
    // Units of every scenario.
    let mut scenes = Vec::new();
    files(&dir.join("Scenes"), "sze", &mut scenes);
    scenes.sort();
    let mut u = Fnv::new();
    let (mut n, mut missing, mut failed) = (0, 0, 0);
    for p in &scenes {
        let buf = std::fs::read(p).expect("read");
        let s = sze::Scene::parse(&buf).expect("scene");
        let read = |g: u32, o: u64, l: u64| {
            let (o, l) = (usize::try_from(o).ok()?, usize::try_from(l).ok()?);
            vols.get(usize::try_from(g).ok()?)?
                .get(o..o + l)
                .map(<[u8]>::to_vec)
        };
        let kinds = [unit::UnitKind::Ship, unit::UnitKind::Soldier];
        let (units, st) =
            unit::units(&s, &h, &kinds, &figure::ORIGINAL_PLAYER_COLOURS, read).expect("units");
        n += st.units;
        missing += st.missing;
        failed += st.failed_frames;
        for x in &units {
            u.bytes(&[u8::from(x.kind == unit::UnitKind::Soldier)]);
            u.bytes(&x.figure.to_le_bytes());
            u.bytes(&x.x.to_bits().to_le_bytes());
            u.bytes(&x.y.to_bits().to_le_bytes());
            for fr in x.frames.iter().chain(&x.shadows) {
                match fr {
                    Some((f, img)) => {
                        u.bytes(&f.offset.to_le_bytes());
                        u.img(Some(img));
                    }
                    None => u.bytes(b"none"),
                }
            }
            u.bytes(b"|");
        }
    }
    u.line("units");
    println!("unit stats units={n} missing={missing} failed_frames={failed}");
    // Ground of every island of the 0200Single scenarios, 4 px per tile.
    let fbuf = std::fs::read(dir.join("Textures").join("Felder.tex")).expect("Felder.tex");
    let felder = tex::TexFile::parse(&fbuf).expect("parses");
    let gt = island::GroundTextures::new(&felder).expect("textures");
    let mut gd = Fnv::new();
    let mut singles = Vec::new();
    files(&dir.join("Scenes").join("0200Single"), "sze", &mut singles);
    singles.sort();
    for p in &singles {
        let buf = std::fs::read(p).expect("read");
        for i in insel::islands(&buf).expect("islands") {
            gd.img(island::ground_top_down(&i, &gt, 4).ok().as_ref());
        }
    }
    gd.line("ground");
}
