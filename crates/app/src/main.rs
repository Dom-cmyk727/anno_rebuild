//! ANNO 1503 rebuild entry point. Wires the crates together; for now it is
//! the M1.6 island viewer.
//!
//! ```text
//! anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png <out.png> [--time <ms>]]
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
//!   (near); `Esc` or `Q` quits; `Space` pauses the water animation. The scene is rendered lazily in 1024 × 1024
//!   pixel chunks on all cores and cached per zoom group; progress goes to
//!   stderr. Open sea is drawn in the island-water placeholder colour.
//! - `--png <out.png>`: headless; writes the whole scene at `--group`
//!   (default 2) as an RGBA PNG (transparent outside the islands) and prints
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
use anno_formats::insel::{self, Island};
use anno_formats::tex::TexFile;
use anno_render::scene::{self, IslandBox, Placement, SceneStats};

// Link every crate so the workspace wiring is exercised from the start.
use anno_net as _;
use anno_sim as _;
use anno_ui as _;

const USAGE: &str = "usage: anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png <out.png> [--time <ms>]]";

type BoxError = Box<dyn std::error::Error>;

/// Parsed command line of `--view`.
struct Args {
    file: PathBuf,
    game_dir: Option<PathBuf>,
    group: Option<usize>,
    png: Option<PathBuf>,
    /// Animation time for `--png`, in milliseconds.
    time: u64,
}

fn parse_args(args: &[String]) -> Option<Args> {
    let mut out = Args {
        file: PathBuf::new(),
        game_dir: None,
        group: None,
        png: None,
        time: 0,
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

/// Everything the viewer draws, borrowed from the loaded file buffers.
pub struct Scene<'a> {
    pub name: String,
    pub islands: Vec<Island<'a>>,
    pub textures: GroundTextures,
    pub anlagen: AnlagenFile<'a>,
    /// Placements per zoom group, in draw order.
    pub layouts: [Vec<Placement>; 3],
    /// Decoded sprite frames, shared by all renders and threads.
    pub sprites: anno_render::iso::SpriteCache,
}

impl Scene<'_> {
    /// Renders `layers` of the scene region `rect` at `group` into a new
    /// (transparent) canvas.
    pub fn render(
        &self,
        rect: scene::Rect,
        group: usize,
        layers: scene::Layers,
    ) -> (anno_render::iso::Canvas, SceneStats) {
        let mut canvas = scene::blank(rect);
        let stats = scene::render_region(
            &mut canvas,
            &self.islands,
            &self.layouts[group],
            &self.textures,
            &self.anlagen,
            group,
            layers,
            &self.sprites,
        );
        (canvas, stats)
    }
}

fn run(args: &Args) -> Result<bool, BoxError> {
    let dir = game_dir(args)?;
    let file_buf = std::fs::read(&args.file)?;
    let felder_buf = std::fs::read(dir.join("Textures/Felder.tex"))?;
    let anl_buf = std::fs::read(dir.join("Anlagen.dat"))?;
    let islands = insel::islands(&file_buf)?;
    let boxes: Vec<IslandBox> = islands.iter().map(IslandBox::of).collect();
    let anlagen = AnlagenFile::parse(&anl_buf)?;
    let mut textures = GroundTextures::new(&TexFile::parse(&felder_buf)?)?;
    if let Some(c) = anno_render::iso::water_colour(&anlagen) {
        textures.set_water(c);
    }
    let scene = Scene {
        name: args.file.display().to_string(),
        textures,
        anlagen,
        layouts: [0, 1, 2].map(|g| scene::layout(&boxes, g)),
        islands,
        sprites: anno_render::iso::SpriteCache::default(),
    };
    if scene.islands.is_empty() {
        return Err("no islands".into());
    }
    match &args.png {
        Some(out) => write_scene_png(&scene, args.group.unwrap_or(2), out, args.time),
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
        water: Some(time),
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
