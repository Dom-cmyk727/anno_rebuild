//! ANNO 1503 rebuild entry point. Wires the crates together; for now it is
//! the M1.6 island viewer.
//!
//! ```text
//! anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>]
//!          [--png <out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]]
//!          [--water original|enhanced] [--renderer auto|gpu|cpu]
//! ```
//!
//! `--view` shows every island of a scenario (`.sze`) at its `INSEL5` world
//! position (islands placed by `INSELLNK` as stand-ins: a real template of
//! the same climate and size, until OQ-028 is resolved), or the single
//! island of a template
//! (`.scp`), rendered with
//! `anno_render::iso` (ground with elevation, animated sea, `GELAENDE` +
//! `PROD2` + `FLUESSE` objects, shadows, and a scenario's ships from
//! `SPIELER/SCHIFF`) and composed by `anno_render::scene` (rules:
//! `docs/render/iso.md`).
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
//!   (default 2) as an RGBA PNG, or with `--at <x>,<y>` only a region of
//!   `--size` pixels (default 1600x900) centred on world tile `(x, y)`, and
//!   prints
//!   one line to stderr:
//!   `<file>: group <g> <w>x<h> px, <n> islands drawn (<k> without ground), objects <d> drawn, <i> invisible, <f> failed, <s> ships drawn`.
//!   Exit code 1 if any object failed.
//!
//! The game directory (for `Textures/Felder.tex`, `Anlagen.dat` and, for
//! ships, `FigurenHeader.dat` + `Figuren*.dat`) is
//! `--game-dir`, else `ANNO1503_GAME_DIR`, else the nearest ancestor of the
//! file that contains `Anlagen.dat`.

mod viewer;

use std::collections::BTreeMap;
use std::fs::File;
use std::io::{BufWriter, Read as _, Seek as _, SeekFrom};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use anno_assets::island::GroundTextures;
use anno_assets::unit;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::figuren::{self, FigurenHeader};
use anno_formats::tex::TexFile;
use anno_formats::{insel, sze};
use anno_render::scene::{self, Scene, UnitSprite};

// Link every crate so the workspace wiring is exercised from the start.
use anno_net as _;
use anno_sim as _;
use anno_ui as _;

const USAGE: &str = "usage: anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png <out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]] [--water original|enhanced] [--renderer auto|gpu|cpu]";

type BoxError = Box<dyn std::error::Error>;

/// Parsed command line of `--view`.
struct Args {
    file: PathBuf,
    game_dir: Option<PathBuf>,
    group: Option<usize>,
    png: Option<PathBuf>,
    /// Animation time for `--png`, in milliseconds.
    time: u64,
    /// `--png` region: centre world tile and size in pixels.
    at: Option<(i32, i32)>,
    size: (i32, i32),
    /// Sea style for `--png` (the window starts with `Original`, `E` toggles).
    water: anno_render::iso::WaterStyle,
    renderer: viewer::Renderer,
}

fn parse_args(args: &[String]) -> Option<Args> {
    let mut out = Args {
        file: PathBuf::new(),
        game_dir: None,
        group: None,
        png: None,
        time: 0,
        at: None,
        size: (1600, 900),
        water: anno_render::iso::WaterStyle::Original,
        renderer: viewer::Renderer::Auto,
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
            "--at" => {
                let (x, y) = value.split_once(',')?;
                out.at = Some((x.parse().ok()?, y.parse().ok()?));
            }
            "--size" => {
                let (w, h) = value.split_once('x')?;
                out.size = (w.parse().ok()?, h.parse().ok()?);
                if out.size.0 <= 0 || out.size.1 <= 0 {
                    return None;
                }
            }
            "--water" => {
                out.water = match value.as_str() {
                    "original" => anno_render::iso::WaterStyle::Original,
                    "enhanced" => anno_render::iso::WaterStyle::Enhanced,
                    _ => return None,
                }
            }
            "--renderer" => {
                out.renderer = match value.as_str() {
                    "auto" => viewer::Renderer::Auto,
                    "gpu" => viewer::Renderer::Gpu,
                    "cpu" => viewer::Renderer::Cpu,
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
    let linked = linked_templates(&dir, &file_buf)?;
    let mut islands = insel::islands(&file_buf)?;
    for (link, buf) in &linked {
        let mut island = insel::islands(buf)?
            .into_iter()
            .next()
            .ok_or("island template without an island")?;
        (island.x, island.y, island.number) = (link.0, link.1, link.2);
        islands.push(island);
    }
    if !linked.is_empty() {
        eprintln!(
            "{} linked islands (INSELLNK) drawn as stand-ins: real templates of the same climate and size (OQ-028)",
            linked.len()
        );
    }
    let anlagen = AnlagenFile::parse(&anl_buf)?;
    let mut textures = GroundTextures::new(&TexFile::parse(&felder_buf)?)?;
    if let Some(c) = anno_render::iso::water_colour(&anlagen) {
        textures.set_water(c);
    }
    let mut scene = Scene::new(
        args.file.display().to_string(),
        islands,
        textures,
        anlagen,
        anno_render::iso::HEIGHT_LIFT,
    );
    scene.units = load_ships(&dir, &file_buf)?;
    if scene.islands.is_empty() {
        return Err("no islands".into());
    }
    match &args.png {
        Some(out) => {
            let group = args.group.unwrap_or(2);
            let region = args.at.map(|(x, y)| {
                let (cx, cy) = anno_render::iso::tile_top(x, y, group);
                scene::Rect {
                    x: cx - args.size.0 / 2,
                    y: cy - args.size.1 / 2,
                    w: args.size.0,
                    h: args.size.1,
                }
            });
            write_scene_png(&scene, group, region, out, args.time, args.water)
        }
        None => {
            viewer::run(&scene, args.group.unwrap_or(2), args.renderer)?;
            Ok(true)
        }
    }
}

/// Position and number of an island link.
type LinkPlace = (u16, u16, u32);

/// Stand-in islands for a scenario's `INSELLNK` records, with the link's
/// world position and island number. A link names a placeholder template
/// (`*_dummy.scp`: flat grass, no coast); the original presumably puts a
/// real island of that climate and size there, which one is open (OQ-028).
/// Until then the viewer takes a real template from the dummy's climate
/// folder with the link's size: candidates sorted by file name, pick =
/// island number mod count (our choice, deterministic). Falls back to the
/// dummy if there is none.
fn linked_templates(dir: &Path, file_buf: &[u8]) -> Result<Vec<(LinkPlace, Vec<u8>)>, BoxError> {
    let Ok(scenario) = sze::Scene::parse(file_buf) else {
        return Ok(Vec::new());
    };
    let mut out = Vec::new();
    for link in scenario.island_links()? {
        let rel = String::from_utf8_lossy(link.path);
        let Some(dummy) = find_ci(&dir.join("Scenes"), &rel) else {
            eprintln!("island link {}: {rel} not found, skipped", link.number);
            continue;
        };
        let size = (link.width, link.height);
        let buf = match stand_in(&dummy, size, link.number)? {
            Some(buf) => buf,
            None => std::fs::read(&dummy)?,
        };
        out.push(((link.x, link.y, link.number), buf));
    }
    Ok(out)
}

/// A real island template next to `dummy` (same folder, not a dummy) of
/// `size`, chosen by `number` (see [`linked_templates`]).
fn stand_in(dummy: &Path, size: (u16, u16), number: u32) -> Result<Option<Vec<u8>>, BoxError> {
    let Some(folder) = dummy.parent() else {
        return Ok(None);
    };
    // Template names carry the size as `_<w>x<h>_`; checked after loading
    // because two templates are misnamed (insel.md).
    let tag = format!("_{:03}x{:03}_", size.0, size.1);
    let mut names: Vec<PathBuf> = std::fs::read_dir(folder)?
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| {
            let n = p
                .file_name()
                .map(|n| n.to_string_lossy().to_ascii_lowercase());
            n.is_some_and(|n| n.ends_with(".scp") && !n.contains("dummy") && n.contains(&tag))
        })
        .collect();
    names.sort();
    if names.is_empty() {
        return Ok(None);
    }
    let start = usize::try_from(number)? % names.len();
    for k in 0..names.len() {
        let buf = std::fs::read(&names[(start + k) % names.len()])?;
        let fits = insel::islands(&buf)
            .ok()
            .and_then(|i| i.first().map(|i| (i.width, i.height) == size))
            .unwrap_or(false);
        if fits {
            return Ok(Some(buf));
        }
    }
    Ok(None)
}

/// Resolves a `\`-separated path below `base`, ignoring case per segment
/// (the original ran on case-insensitive Windows file systems).
fn find_ci(base: &Path, rel: &str) -> Option<PathBuf> {
    let mut cur = base.to_path_buf();
    for seg in rel.split('\\') {
        cur = std::fs::read_dir(&cur)
            .ok()?
            .filter_map(Result::ok)
            .map(|e| e.path())
            .find(|p| {
                p.file_name()
                    .is_some_and(|n| n.to_string_lossy().eq_ignore_ascii_case(seg))
            })?;
    }
    Some(cur)
}

/// The ships of a scenario (none for an island template). Only the frames
/// the ships use are read from the figure volumes.
fn load_ships(dir: &Path, file_buf: &[u8]) -> Result<Vec<UnitSprite>, BoxError> {
    let Ok(scenario) = sze::Scene::parse(file_buf) else {
        return Ok(Vec::new());
    };
    let header_buf = std::fs::read(dir.join("FigurenHeader.dat"))?;
    let header = FigurenHeader::parse(&header_buf)?;
    let mut parts = BTreeMap::<u32, Vec<(PathBuf, u64)>>::new();
    let read = |group: u32, offset: u64, length: u64| {
        let parts = parts
            .entry(group)
            .or_insert_with(|| volume_parts(dir, group));
        let sizes: Vec<u64> = parts.iter().map(|p| p.1).collect();
        let mut data = Vec::new();
        for (part, start, end) in figuren::locate(&sizes, offset, length)? {
            let mut f = File::open(&parts[part].0).ok()?;
            f.seek(SeekFrom::Start(start)).ok()?;
            let mut buf = vec![0; usize::try_from(end - start).ok()?];
            f.read_exact(&mut buf).ok()?;
            data.extend_from_slice(&buf);
        }
        Some(data)
    };
    let (ships, stats) = unit::ships(&scenario, &header, read)?;
    if stats.missing + stats.failed_frames > 0 {
        eprintln!(
            "ships: {} of {} without a figure, {} frames failed",
            stats.missing, stats.ships, stats.failed_frames
        );
    }
    Ok(ships
        .into_iter()
        .map(|u| UnitSprite {
            x: u.x,
            y: u.y,
            frames: u.frames,
        })
        .collect())
}

/// Paths and sizes of the volume parts `Figuren<group><nn>.dat`, in order.
fn volume_parts(dir: &Path, group: u32) -> Vec<(PathBuf, u64)> {
    let mut out = Vec::new();
    for part in 0.. {
        let path = dir.join(figuren::volume_file_name(group, part));
        match std::fs::metadata(&path) {
            Ok(m) => out.push((path, m.len())),
            Err(_) => break,
        }
    }
    out
}

/// Headless `--png`: renders `region`, or the whole scene, at `group`.
fn write_scene_png(
    scene: &Scene<'_>,
    group: usize,
    region: Option<scene::Rect>,
    out: &Path,
    time: u64,
    water_style: anno_render::iso::WaterStyle,
) -> Result<bool, BoxError> {
    let rect = match region {
        Some(r) => r,
        None => scene.bounds(group).ok_or("no islands")?,
    };
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
        "{}: group {group} {}x{} px, {} islands drawn ({} without ground), objects {} drawn, {} invisible, {} failed, {} ships drawn",
        scene.name,
        rect.w,
        rect.h,
        stats.islands,
        stats.no_ground,
        stats.objects.drawn,
        stats.objects.invisible,
        stats.objects.failed,
        stats.units
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
