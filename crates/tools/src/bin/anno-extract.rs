//! `anno-extract`: export original ANNO 1503 assets to standard formats.
//!
//! ```text
//! anno-extract tex <file.tex> <outdir>
//! ```
//!
//! Writes every texture as `<outdir>/<id>.png` (8-bit RGBA, transparent RLE
//! gaps as alpha 0, `HEAD.color_key` not applied; see `anno_assets::texture`)
//! and `<outdir>/index.txt` with one line per texture, in file order:
//! `<id>.png width=<w> height=<h> palette=<id> format=<str> name=<str|->`.
//! Textures that fail to decode are listed with `ERROR <message>` instead of
//! a file name and make the exit code 1.

use std::fmt::Write as _;
use std::fs::File;
use std::io::BufWriter;
use std::path::Path;
use std::process::ExitCode;

use anno_assets::texture::{self, Rgba};
use anno_formats::tex::TexFile;

const USAGE: &str = "usage: anno-extract tex <file.tex> <outdir>";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.as_slice() {
        [cmd, file, outdir] if cmd == "tex" => match extract_tex(file, Path::new(outdir)) {
            Ok(true) => ExitCode::SUCCESS,
            Ok(false) => ExitCode::FAILURE,
            Err(e) => {
                eprintln!("{file}: {e}");
                ExitCode::FAILURE
            }
        },
        _ => {
            eprintln!("{USAGE}");
            ExitCode::from(2)
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
