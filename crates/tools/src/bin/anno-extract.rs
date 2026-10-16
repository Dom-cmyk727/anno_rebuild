//! `anno-extract`: export original ANNO 1503 assets to standard formats.
//!
//! ```text
//! anno-extract tex <file.tex> <outdir>
//! anno-extract figuren <game_dir> <outdir> <entry id>...
//! ```
//!
//! `tex` writes every texture as `<outdir>/<id>.png` (8-bit RGBA, transparent
//! RLE gaps as alpha 0, `HEAD.color_key` not applied; see
//! `anno_assets::texture`) and `<outdir>/index.txt` with one line per
//! texture, in file order:
//! `<id>.png width=<w> height=<h> palette=<id> format=<str> name=<str|->`.
//!
//! `figuren` writes, per requested entry of `FigurenHeader.dat`, every frame
//! of every sprite set and shadow as `<outdir>/<entry>/s<set>_g<group>_<k>.png`
//! (shadows `…_<k>_shadow.png`; mapping in `anno_assets::figure`) and
//! `<outdir>/<entry>/index.txt` with one line per frame:
//! `<file> width=<w> height=<h> format=<str> anchor=<x>,<y> palette=<i> table=<str|->`.
//!
//! Items that fail to decode are listed with `ERROR <message>` instead of a
//! file name and make the exit code 1.

use std::fmt::Write as _;
use std::fs::File;
use std::io::BufWriter;
use std::path::Path;
use std::process::ExitCode;

use anno_assets::figure;
use anno_assets::texture::{self, Rgba};
use anno_formats::figuren::{self, FigurenHeader};
use anno_formats::tex::TexFile;

const USAGE: &str = "usage: anno-extract tex <file.tex> <outdir>\n       \
                     anno-extract figuren <game_dir> <outdir> <entry id>...";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let result = match args.as_slice() {
        [cmd, file, outdir] if cmd == "tex" => (file, extract_tex(file, Path::new(outdir))),
        [cmd, dir, outdir, ids @ ..] if cmd == "figuren" && !ids.is_empty() => {
            (dir, extract_figuren(Path::new(dir), Path::new(outdir), ids))
        }
        _ => {
            eprintln!("{USAGE}");
            return ExitCode::from(2);
        }
    };
    match result {
        (_, Ok(true)) => ExitCode::SUCCESS,
        (_, Ok(false)) => ExitCode::FAILURE,
        (input, Err(e)) => {
            eprintln!("{input}: {e}");
            ExitCode::FAILURE
        }
    }
}

type BoxError = Box<dyn std::error::Error>;

/// Returns `Ok(false)` if some textures could not be decoded.
fn extract_tex(path: &str, outdir: &Path) -> Result<bool, BoxError> {
    let buf = std::fs::read(path)?;
    let file = TexFile::parse(&buf)?;
    std::fs::create_dir_all(outdir)?;
    let mut index = String::new();
    let mut all_ok = true;
    for t in &file.textures {
        let h = &t.head;
        let name = format!("{}.png", t.id);
        let written = texture::to_rgba(&file, t)
            .map_err(BoxError::from)
            .and_then(|img| write_png(&outdir.join(&name), &img));
        match written {
            Ok(()) => write!(index, "{name}")?,
            Err(e) => {
                eprintln!("{path}: texture {}: {e}", t.id);
                write!(index, "ERROR {e}")?;
                all_ok = false;
            }
        }
        let format = text(&h.format_raw);
        let name = t.name.map_or_else(|| "-".to_string(), text);
        writeln!(
            index,
            " width={} height={} palette={} format={format} name={name}",
            h.width, h.height, h.palette_id
        )?;
    }
    std::fs::write(outdir.join("index.txt"), index)?;
    Ok(all_ok)
}

/// Concatenated `Figuren<group><nn>.dat` files of one zoom group.
fn read_volume(dir: &Path, group: u32) -> Result<Vec<u8>, BoxError> {
    let mut data = Vec::new();
    for part in 0.. {
        match std::fs::read(dir.join(figuren::volume_file_name(group, part))) {
            Ok(buf) => data.extend_from_slice(&buf),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound && part > 0 => break,
            Err(e) => return Err(e.into()),
        }
    }
    Ok(data)
}

/// Returns `Ok(false)` if an entry is missing or some frames could not be decoded.
fn extract_figuren(dir: &Path, outdir: &Path, ids: &[String]) -> Result<bool, BoxError> {
    let buf = std::fs::read(dir.join("FigurenHeader.dat"))?;
    let header = FigurenHeader::parse(&buf)?;
    let volumes = (0..3)
        .map(|g| read_volume(dir, g))
        .collect::<Result<Vec<_>, _>>()?;
    let mut all_ok = true;
    for id in ids {
        let Some(entry) = header.entries.iter().find(|e| e.id.to_string() == *id) else {
            eprintln!("entry {id}: not in FigurenHeader.dat");
            all_ok = false;
            continue;
        };
        let out = outdir.join(id);
        std::fs::create_dir_all(&out)?;
        let mut index = String::new();
        for (i, set) in entry.gfx.iter().enumerate() {
            for (shadow, s) in [("", Some(set)), ("_shadow", set.shadow.as_deref())] {
                for fs in s.map(|s| s.frame_sets.as_slice()).unwrap_or_default() {
                    let volume = usize::try_from(fs.group)
                        .ok()
                        .and_then(|g| volumes.get(g))
                        .map_or(&[][..], Vec::as_slice);
                    for (k, f) in fs.frames.iter().enumerate() {
                        let name = format!("s{i}_g{}_{k}{shadow}.png", fs.group);
                        let written = figure::to_rgba(entry, f, volume)
                            .map_err(BoxError::from)
                            .and_then(|img| write_png(&out.join(&name), &img));
                        match written {
                            Ok(()) => write!(index, "{name}")?,
                            Err(e) => {
                                eprintln!("entry {id}: {name}: {e}");
                                write!(index, "ERROR {e}")?;
                                all_ok = false;
                            }
                        }
                        let table = entry
                            .palettes
                            .get(usize::from(f.palette))
                            .map_or_else(|| "-".to_string(), |t| text(t.name));
                        writeln!(
                            index,
                            " width={} height={} format={} anchor={},{} palette={} table={table}",
                            f.width,
                            f.height,
                            f.format_name(),
                            f.anchor_x,
                            f.anchor_y,
                            f.palette
                        )?;
                    }
                }
            }
        }
        std::fs::write(out.join("index.txt"), index)?;
    }
    Ok(all_ok)
}

/// Bytes up to the first NUL as Latin-1 (matches Windows-1252 for the names seen).
fn text(raw: &[u8]) -> String {
    let len = raw.iter().position(|&b| b == 0).unwrap_or(raw.len());
    raw[..len].iter().map(|&b| char::from(b)).collect()
}

fn write_png(path: &Path, img: &Rgba) -> Result<(), BoxError> {
    let mut enc = png::Encoder::new(
        BufWriter::new(File::create(path)?),
        u32::from(img.width),
        u32::from(img.height),
    );
    enc.set_color(png::ColorType::Rgba);
    enc.set_depth(png::BitDepth::Eight);
    let mut writer = enc.write_header()?;
    writer.write_image_data(&img.pixels)?;
    writer.finish()?;
    Ok(())
}
