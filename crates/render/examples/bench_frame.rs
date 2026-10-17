//! Frame benchmark of the interactive view: runs the viewer's frame
//! pipeline (`anno_render::frame::FrameRenderer`) headless along scripted
//! camera paths and reports frame-time percentiles and the time per stage.
//!
//! ```text
//! cargo run --release -p anno-render --example bench_frame -- <game_dir> <file.sze|.scp>
//!     [--threads n] [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2]
//!     [--styles original,enhanced] [--chunks blocking,async] [--quick]
//! ```
//!
//! Per resolution, zoom group and water style, from a cold cache centred on
//! the largest island:
//! - `cold`: the first frame (every visible chunk rendered),
//! - `static`: 120 frames without camera movement,
//! - `pan slow`: 240 frames moving 6 × 3 px per frame,
//! - `pan fast`: 120 frames moving 48 × 24 px per frame (new chunks keep
//!   appearing, as when dragging across the map).
//!
//! Chunks are built in the frame (`blocking`) or by background workers on
//! half the threads (`async`, as the viewer does; missing chunks show open
//! sea). The animation clock advances 16 ms per frame (60 Hz); in `async`
//! mode the benchmark does not wait between frames, so workers get less
//! time than at a real 60 Hz and `missing` is an upper bound. Output: one
//! Markdown table (method: `docs/perf/README.md`).

use std::path::Path;

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel;
use anno_formats::tex::TexFile;
use anno_render::frame::{self, FrameRenderer, FrameStats};
use anno_render::iso::{self, WaterStyle};
use anno_render::scene::{Rect, Scene};

type BoxError = Box<dyn std::error::Error>;

struct Options {
    threads: usize,
    resolutions: Vec<(i32, i32)>,
    groups: Vec<usize>,
    styles: Vec<WaterStyle>,
    /// `false` = blocking, `true` = async.
    modes: Vec<bool>,
    quick: bool,
}

fn main() -> Result<(), BoxError> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (Some(dir), Some(file)) = (args.first(), args.get(1)) else {
        return Err("usage: bench_frame <game_dir> <file> [--threads n] [--res WxH,..] [--groups 0,1,2] [--styles original,enhanced] [--quick]".into());
    };
    let mut o = Options {
        threads: std::thread::available_parallelism().map_or(4, std::num::NonZeroUsize::get),
        resolutions: vec![(1920, 1080), (2560, 1440), (3840, 2160)],
        groups: vec![0, 1, 2],
        styles: vec![WaterStyle::Original, WaterStyle::Enhanced],
        modes: vec![false, true],
        quick: false,
    };
    let mut it = args[2..].iter();
    while let Some(a) = it.next() {
        let mut value = || it.next().ok_or(format!("{a} needs a value"));
        match a.as_str() {
            "--threads" => o.threads = value()?.parse()?,
            "--res" => {
                o.resolutions = value()?
                    .split(',')
                    .map(|r| {
                        let (w, h) = r.split_once('x').ok_or("WxH")?;
                        Ok((w.parse()?, h.parse()?))
                    })
                    .collect::<Result<_, BoxError>>()?;
            }
            "--groups" => {
                o.groups = value()?
                    .split(',')
                    .map(str::parse)
                    .collect::<Result<_, _>>()?;
            }
            "--styles" => {
                o.styles = value()?
                    .split(',')
                    .map(|s| match s {
                        "original" => Ok(WaterStyle::Original),
                        "enhanced" => Ok(WaterStyle::Enhanced),
                        _ => Err(format!("unknown style {s}")),
                    })
                    .collect::<Result<_, _>>()?;
            }
            "--chunks" => {
                o.modes = value()?
                    .split(',')
                    .map(|s| match s {
                        "blocking" => Ok(false),
                        "async" => Ok(true),
                        _ => Err(format!("unknown chunk mode {s}")),
                    })
                    .collect::<Result<_, _>>()?;
            }
            "--quick" => o.quick = true,
            _ => return Err(format!("unknown option {a}").into()),
        }
    }
    let dir = Path::new(dir);
    let file_buf = std::fs::read(file)?;
    let felder_buf = std::fs::read(dir.join("Textures/Felder.tex"))?;
    let anl_buf = std::fs::read(dir.join("Anlagen.dat"))?;
    let islands = insel::islands(&file_buf)?;
    let anlagen = AnlagenFile::parse(&anl_buf)?;
    let mut textures = GroundTextures::new(&TexFile::parse(&felder_buf)?)?;
    if let Some(c) = iso::water_colour(&anlagen) {
        textures.set_water(c);
    }
    let largest = islands
        .iter()
        .enumerate()
        .max_by_key(|(_, i)| u32::from(i.width) * u32::from(i.height))
        .map(|(k, _)| k)
        .ok_or("no islands")?;
    let started = std::time::Instant::now();
    let scene = Scene::new(file.clone(), islands, textures, anlagen, iso::HEIGHT_LIFT);
    eprintln!(
        "scene loaded in {:.0} ms",
        started.elapsed().as_secs_f64() * 1000.0
    );
    for g in &o.groups {
        let t = std::time::Instant::now();
        scene.sea(*g);
        eprintln!(
            "sea pattern group {g}: {:.0} ms",
            t.elapsed().as_secs_f64() * 1000.0
        );
    }
    println!(
        "{} — {} {} — {} threads{}\n",
        file,
        std::env::consts::OS,
        std::env::consts::ARCH,
        o.threads,
        if o.quick { " — quick" } else { "" }
    );
    println!(
        "| resolution | group | water | chunks | path | frames | p50 ms | p95 ms | p99 ms | max ms | ≤16.7 ms | built | chunk ms | missing | ground | water obj | sea | objects |"
    );
    println!("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    let scale = if o.quick { 4 } else { 1 };
    let o = &o;
    for &(w, h) in &o.resolutions {
        for &group in &o.groups {
            for &style in &o.styles {
                for &async_chunks in &o.modes {
                    let p = scene.layouts[group]
                        .iter()
                        .find(|p| p.index == largest)
                        .ok_or("no placement")?;
                    let centre = (p.bounds.x + p.bounds.w / 2, p.bounds.y + p.bounds.h / 2);
                    std::thread::scope(|s| -> Result<(), BoxError> {
                        let mut fr = FrameRenderer::new(o.threads);
                        if async_chunks {
                            let workers =
                                frame::spawn_chunk_workers(s, &scene, o.threads.div_ceil(2));
                            fr.set_workers(Some(workers));
                        }
                        let mut out = vec![0u32; usize::try_from(w * h)?];
                        let mut cam = (centre.0 - w / 2, centre.1 - h / 2);
                        let mut time = 0u64;
                        let paths: [(&str, usize, (i32, i32)); 4] = [
                            ("cold", 1, (0, 0)),
                            ("static", 120 / scale, (0, 0)),
                            ("pan slow", 240 / scale, (6, 3)),
                            ("pan fast", 120 / scale, (48, 24)),
                        ];
                        for (name, n, (dx, dy)) in paths {
                            let mut stats = Vec::with_capacity(n);
                            for _ in 0..n {
                                let view = Rect {
                                    x: cam.0,
                                    y: cam.1,
                                    w,
                                    h,
                                };
                                stats.push(fr.render(&scene, view, group, time, style, &mut out));
                                time += 16;
                                cam = (cam.0 + dx, cam.1 + dy);
                            }
                            let mode = if async_chunks { "async" } else { "blocking" };
                            report(w, h, group, style, mode, name, &stats);
                        }
                        fr.set_workers(None);
                        Ok(())
                    })?;
                }
            }
        }
    }
    Ok(())
}

fn report(
    w: i32,
    h: i32,
    group: usize,
    style: WaterStyle,
    mode: &str,
    name: &str,
    s: &[FrameStats],
) {
    let mut total: Vec<f64> = s.iter().map(|f| f.total_ms).collect();
    total.sort_by(f64::total_cmp);
    let pct = |p: f64| {
        #[allow(
            clippy::cast_possible_truncation,
            clippy::cast_sign_loss,
            clippy::cast_precision_loss
        )]
        let i = ((total.len() as f64 - 1.0) * p).round() as usize;
        total[i]
    };
    #[allow(clippy::cast_precision_loss)]
    let n = s.len() as f64;
    let mean = |f: fn(&FrameStats) -> f64| s.iter().map(f).sum::<f64>() / n;
    let within = s.iter().filter(|f| f.total_ms <= 16.7).count();
    let style = match style {
        WaterStyle::Original => "original",
        WaterStyle::Enhanced => "enhanced",
    };
    println!(
        "| {w}×{h} | {group} | {style} | {mode} | {name} | {} | {:.1} | {:.1} | {:.1} | {:.1} | {:.0} % | {} | {:.1} | {:.1} | {:.1} | {:.1} | {:.1} | {:.1} |",
        s.len(),
        pct(0.5),
        pct(0.95),
        pct(0.99),
        total.last().copied().unwrap_or(0.0),
        100.0 * f64::from(u32::try_from(within).unwrap_or(u32::MAX)) / n,
        s.iter().map(|f| f.chunks_built).sum::<usize>(),
        mean(|f| f.chunks_ms),
        mean(|f| f64::from(u32::try_from(f.chunks_missing).unwrap_or(u32::MAX))),
        mean(|f| f.ground_ms),
        mean(|f| f.water_objects_ms),
        mean(|f| f.sea_ms),
        mean(|f| f.objects_ms),
    );
}
