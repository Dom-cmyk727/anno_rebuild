//! ANNO 1503 rebuild entry point. Wires the crates together; for now it is
//! the M1.6 island viewer.
//!
//! ```text
//! anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png <out.png> [--time <ms>]]
//!          [--water original|enhanced]
//! ```
//!
//! `--view` shows every island of a scenario (`.sze`) at its `INSEL5` world
//! position, or the single island of a template (`.scp`), rendered with
//! `anno_render::iso` (ground with elevation, animated sea, `GELAENDE` +
//! `PROD2` + `FLUESSE` objects, shadows) and
//! composed by `anno_render::scene` (rules: `docs/render/iso.md`).
//!
//! - Window (default): drag with the left mouse button, arrow keys or WASD to
//!   pan; mouse wheel or `+` / `-` to switch zoom groups 2 (far) / 1 / 0
//!   (near); `Esc` or `Q` quits; `Space` pauses the water animation; `E` toggles
//!   enhanced water (not in the original; off by default). Static layers are
//!   rendered lazily in chunks on all cores and cached
//!   (`anno_render::frame`); the sea covers the whole view, also between
//!   islands. The title shows frames per second; stderr gets a per-stage
//!   frame-time breakdown.
//! - `--png <out.png>`: headless; writes the whole scene at `--group`
//!   (default 2) as an RGBA PNG and prints
//!   one line to stderr:
//!   `<file>: group <g> <w>x<h> px, <n> islands drawn (<k> without ground), objects <d> drawn, <i> invisible, <f> failed`.
//!   Exit code 1 if any object failed.
//!
//! The game directory (for `Textures/Felder.tex` and `Anlagen.dat`) is
//! `--game-dir`, else `ANNO1503_GAME_DIR`, else the nearest ancestor of the
//! file that contains `Anlagen.dat`.

mod viewer;

use std::fs::File;
use std::io::BufWriter;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel;
use anno_formats::tex::TexFile;
use anno_render::scene::{self, Scene};

// Link every crate so the workspace wiring is exercised from the start.
use anno_net as _;
use anno_sim as _;
use anno_ui as _;

const USAGE: &str = "usage: anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png <out.png> [--time <ms>]] [--water original|enhanced]";

type BoxError = Box<dyn std::error::Error>;

/// Parsed command line of `--view`.
struct Args {
    file: PathBuf,
    game_dir: Option<PathBuf>,
    group: Option<usize>,
    png: Option<PathBuf>,
    /// Animation time for `--png`, in milliseconds.
    time: u64,
    /// Sea style for `--png` (the window starts with `Original`, `E` toggles).
    water: anno_render::iso::WaterStyle,
}

fn parse_args(args: &[String]) -> Option<Args> {
    let mut out = Args {
        file: PathBuf::new(),
        game_dir: None,
        group: None,
        png: None,
        time: 0,
        water: anno_render::iso::WaterStyle::Original,
    };
    let mut have_file = false;
    let mut it = args.iter();
    while let Some(a) = it.next() {
        let value = it.next()?;
        match a.as_str() {
            "--view" => {
                out.file = PathBuf::from(value);
                have_file = true;
            }
            "--game-dir" => out.game_dir = Some(PathBuf::from(value)),
            "--group" => out.group = Some(value.parse().ok().filter(|&g: &usize| g < 3)?),
            "--png" => out.png = Some(PathBuf::from(value)),
            "--time" => out.time = value.parse().ok()?,
            "--water" => {
                out.water = match value.as_str() {
                    "original" => anno_render::iso::WaterStyle::Original,
                    "enhanced" => anno_render::iso::WaterStyle::Enhanced,
                    _ => return None,
                }
            }
            _ => return None,
        }
    }
    have_file.then_some(out)
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let Some(args) = parse_args(&args) else {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    };
    match run(&args) {
        Ok(true) => ExitCode::SUCCESS,
        Ok(false) => ExitCode::FAILURE,
        Err(e) => {
            eprintln!("{}: {e}", args.file.display());
            ExitCode::FAILURE
        }
    }
}

/// Game directory: explicit, `ANNO1503_GAME_DIR`, or an ancestor of `file`
/// holding `Anlagen.dat`.
fn game_dir(args: &Args) -> Result<PathBuf, BoxError> {
    if let Some(d) = &args.game_dir {
        return Ok(d.clone());
    }
    if let Some(d) = std::env::var_os("ANNO1503_GAME_DIR") {
        return Ok(PathBuf::from(d));
    }
    let abs = std::path::absolute(&args.file)?;
    abs.ancestors()
        .skip(1)
        .find(|d| d.join("Anlagen.dat").is_file())
        .map(Path::to_path_buf)
        .ok_or_else(|| "game directory not found; pass --game-dir or set ANNO1503_GAME_DIR".into())
}

fn run(args: &Args) -> Result<bool, BoxError> {
    let dir = game_dir(args)?;
    let file_buf = std::fs::read(&args.file)?;
    let felder_buf = std::fs::read(dir.join("Textures/Felder.tex"))?;
    let anl_buf = std::fs::read(dir.join("Anlagen.dat"))?;
    let islands = insel::islands(&file_buf)?;
    let anlagen = AnlagenFile::parse(&anl_buf)?;
    let mut textures = GroundTextures::new(&TexFile::parse(&felder_buf)?)?;
    if let Some(c) = anno_render::iso::water_colour(&anlagen) {
        textures.set_water(c);
    }
    let scene = Scene::new(
        args.file.display().to_string(),
        islands,
        textures,
        anlagen,
        anno_render::iso::HEIGHT_LIFT,
    );
    if scene.islands.is_empty() {
        return Err("no islands".into());
    }
    match &args.png {
        Some(out) => write_scene_png(&scene, args.group.unwrap_or(2), out, args.time, args.water),
        None => {
            viewer::run(&scene, args.group.unwrap_or(2))?;
            Ok(true)
        }
    }
}

/// Headless `--png`: renders the whole scene at `group`.
fn write_scene_png(
    scene: &Scene<'_>,
    group: usize,
    out: &Path,
    time: u64,
    water_style: anno_render::iso::WaterStyle,
) -> Result<bool, BoxError> {
    let rect = scene::scene_bounds(&scene.layouts[group]).ok_or("no islands")?;
    let bytes = i64::from(rect.w) * i64::from(rect.h) * 4;
    if bytes > 1 << 32 {
        return Err(format!(
            "scene is {} x {} px at group {group} ({} MiB); use a higher group",
            rect.w,
            rect.h,
            bytes >> 20
        )
        .into());
    }
    let layers = scene::Layers {
        time_ms: time,
        water_style,
        ..scene::Layers::ALL
    };
    let (canvas, stats) = scene.render(rect, group, layers);
    eprintln!(
        "{}: group {group} {}x{} px, {} islands drawn ({} without ground), objects {} drawn, {} invisible, {} failed",
        scene.name,
        rect.w,
        rect.h,
        stats.islands,
        stats.no_ground,
        stats.objects.drawn,
        stats.objects.invisible,
        stats.objects.failed
    );
    let mut enc = png::Encoder::new(
        BufWriter::new(File::create(out)?),
        u32::try_from(canvas.width)?,
        u32::try_from(canvas.height)?,
    );
    enc.set_color(png::ColorType::Rgba);
    enc.set_depth(png::BitDepth::Eight);
    let mut writer = enc.write_header()?;
    writer.write_image_data(&canvas.pixels)?;
    writer.finish()?;
    Ok(stats.objects.failed == 0)
}
