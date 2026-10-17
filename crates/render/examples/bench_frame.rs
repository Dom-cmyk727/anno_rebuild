//! Frame benchmark of the interactive view: runs the viewer's frame
//! pipeline (`anno_render::frame::FrameRenderer`) headless along scripted
//! camera paths and reports frame-time percentiles and the time per stage.
//!
//! ```text
//! cargo run --release -p anno-render --features gpu --example bench_frame -- <game_dir> <file.sze|.scp>
//!     [--threads n] [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2]
//!     [--styles original,enhanced] [--chunks blocking,async]
//!     [--renderer cpu,gpu] [--check] [--island n] [--at dx,dy] [--t0 ms]
//!     [--warmup ms] [--quick]
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
//! time than at a real 60 Hz and `missing` is an upper bound.
//!
//! `--renderer gpu` composes on the graphics card (`anno_render::gpu`, as the
//! viewer does when a GPU is available) into an offscreen target; frame time
//! then includes waiting for the GPU. `--check` compares the first static
//! frame of each configuration with the CPU path pixel by pixel. Output: one
//! Markdown table (method: `docs/perf/README.md`).

use std::path::Path;

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel;
use anno_formats::tex::TexFile;
use anno_render::frame::{self, FrameRenderer};
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
    /// `false` = CPU, `true` = GPU.
    renderers: Vec<bool>,
    check: bool,
    /// Camera offset from the largest island's centre, in pixels.
    at: (i32, i32),
    /// Island to centre on (default: the largest).
    island: Option<usize>,
    /// Animation clock at the first frame (ms).
    t0: u64,
    /// Untimed frames (16 ms apart) before the paths, e.g. to let the
    /// wave simulation develop.
    warmup: u64,
    quick: bool,
}

/// One measured frame: total time, chunk work and four renderer-specific
/// stages (CPU: ground, water objects, sea, objects; GPU: uploads, sprites,
/// encode, GPU wait).
struct Row {
    total: f64,
    built: usize,
    chunk_ms: f64,
    missing: usize,
    stages: [f64; 4],
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
        renderers: vec![false],
        check: false,
        at: (0, 0),
        island: None,
        t0: 0,
        warmup: 0,
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
            "--renderer" => {
                o.renderers = value()?
                    .split(',')
                    .map(|s| match s {
                        "cpu" => Ok(false),
                        "gpu" => Ok(true),
                        _ => Err(format!("unknown renderer {s}")),
                    })
                    .collect::<Result<_, _>>()?;
            }
            "--check" => o.check = true,
            "--t0" => o.t0 = value()?.parse()?,
            "--warmup" => o.warmup = value()?.parse()?,
            "--island" => o.island = Some(value()?.parse()?),
            "--at" => {
                let v = value()?;
                let (x, y) = v.split_once(',').ok_or("--at dx,dy")?;
                o.at = (x.parse()?, y.parse()?);
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
    let largest = o.island.unwrap_or_else(|| {
        islands
            .iter()
            .enumerate()
            .max_by_key(|(_, i)| u32::from(i.width) * u32::from(i.height))
            .map_or(0, |(k, _)| k)
    });
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
    let mut gpu = if o.renderers.contains(&true) {
        let (device, queue, name) = anno_render::gpu::headless_device().ok_or("no GPU adapter")?;
        eprintln!("GPU: {name}");
        Some((
            anno_render::gpu::GpuFrame::new(device, queue, wgpu_format()),
            name,
        ))
    } else {
        None
    };
    println!(
        "{} — {} {} — {} threads{}{}\n",
        file,
        std::env::consts::OS,
        std::env::consts::ARCH,
        o.threads,
        gpu.as_ref()
            .map_or(String::new(), |g| format!(" — GPU {}", g.1)),
        if o.quick { " — quick" } else { "" }
    );
    println!(
        "| resolution | group | water | renderer | chunks | path | frames | p50 ms | p95 ms | p99 ms | max ms | ≤16.7 ms | built | chunk ms | missing | stage 1 | stage 2 | stage 3 | stage 4 |"
    );
    println!("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    let scale = if o.quick { 4 } else { 1 };
    let o = &o;
    for &(w, h) in &o.resolutions {
        for &group in &o.groups {
            for &style in &o.styles {
                for &use_gpu in &o.renderers {
                    for &async_chunks in &o.modes {
                        let p = scene.layouts[group]
                            .iter()
                            .find(|p| p.index == largest)
                            .ok_or("no placement")?;
                        let centre = (
                            p.bounds.x + p.bounds.w / 2 + o.at.0,
                            p.bounds.y + p.bounds.h / 2 + o.at.1,
                        );
                        let gpu = if use_gpu {
                            gpu.as_mut().map(|g| &mut g.0)
                        } else {
                            None
                        };
                        let config = Config {
                            size: (w, h),
                            group,
                            style,
                            async_chunks,
                            centre,
                            frames: 120 / scale,
                            check: o.check,
                            t0: o.t0,
                            warmup: o.warmup,
                            threads: o.threads,
                        };
                        run(&scene, &config, gpu)?;
                    }
                }
            }
        }
    }
    Ok(())
}

/// Target format of the GPU benchmark (readable back as RGBA).
fn wgpu_format() -> wgpu::TextureFormat {
    wgpu::TextureFormat::Rgba8Unorm
}

struct Config {
    size: (i32, i32),
    group: usize,
    style: WaterStyle,
    async_chunks: bool,
    centre: (i32, i32),
    /// Frames of the `static` path (others scale from it).
    frames: usize,
    check: bool,
    t0: u64,
    warmup: u64,
    threads: usize,
}

/// Runs the camera paths of one configuration and prints its rows.
fn run(
    scene: &Scene<'_>,
    c: &Config,
    mut gpu: Option<&mut anno_render::gpu::GpuFrame>,
) -> Result<(), BoxError> {
    let (w, h) = c.size;
    let size = (u32::try_from(w)?, u32::try_from(h)?);
    std::thread::scope(|s| -> Result<(), BoxError> {
        let mut fr = FrameRenderer::new(c.threads);
        if c.async_chunks {
            fr.set_workers(Some(frame::spawn_chunk_workers(
                s,
                scene,
                c.threads.div_ceil(2),
            )));
        }
        let target = gpu.as_ref().map(|g| g.offscreen_target(size));
        let mut out = vec![0u32; usize::try_from(w * h)?];
        let mut cam = (c.centre.0 - w / 2, c.centre.1 - h / 2);
        let mut time = c.t0.saturating_sub(c.warmup);
        let view0 = Rect {
            x: cam.0,
            y: cam.1,
            w,
            h,
        };
        while time < c.t0 {
            match (&mut gpu, &target) {
                (Some(g), Some((_, tv))) => {
                    g.render(&mut fr, scene, view0, c.group, time, c.style, tv);
                }
                _ => {
                    fr.render(scene, view0, c.group, time, c.style, &mut out);
                }
            }
            time += 16;
        }
        let paths: [(&str, usize, (i32, i32)); 4] = [
            ("cold", 1, (0, 0)),
            ("static", c.frames, (0, 0)),
            ("pan slow", 2 * c.frames, (6, 3)),
            ("pan fast", c.frames, (48, 24)),
        ];
        for (name, n, (dx, dy)) in paths {
            let mut rows = Vec::with_capacity(n);
            for i in 0..n {
                let view = Rect {
                    x: cam.0,
                    y: cam.1,
                    w,
                    h,
                };
                let row = match (&mut gpu, &target) {
                    (Some(g), Some((_, tv))) => {
                        let st = g.render(&mut fr, scene, view, c.group, time, c.style, tv);
                        let t = std::time::Instant::now();
                        g.wait();
                        let wait = t.elapsed().as_secs_f64() * 1000.0;
                        Row {
                            total: st.total_ms + wait,
                            built: st.chunks_built,
                            chunk_ms: st.chunks_ms,
                            missing: st.chunks_missing,
                            stages: [st.upload_ms, st.sprites_ms, st.encode_ms, wait],
                        }
                    }
                    _ => {
                        let st = fr.render(scene, view, c.group, time, c.style, &mut out);
                        Row {
                            total: st.total_ms,
                            built: st.chunks_built,
                            chunk_ms: st.chunks_ms,
                            missing: st.chunks_missing,
                            stages: [st.ground_ms, st.water_objects_ms, st.sea_ms, st.objects_ms],
                        }
                    }
                };
                rows.push(row);
                if c.check
                    && name == "static"
                    && i == n - 1
                    && let (Some(g), Some((tex, _))) = (&gpu, &target)
                {
                    compare(scene, c, view, time, g, tex)?;
                }
                time += 16;
                cam = (cam.0 + dx, cam.1 + dy);
            }
            let mode = if c.async_chunks { "async" } else { "blocking" };
            let renderer = if gpu.is_some() { "gpu" } else { "cpu" };
            report(c, renderer, mode, name, &rows);
        }
        fr.set_workers(None);
        Ok(())
    })
}

/// Compares the GPU image of `view` with the CPU path (blocking chunks).
fn compare(
    scene: &Scene<'_>,
    c: &Config,
    view: Rect,
    time: u64,
    g: &anno_render::gpu::GpuFrame,
    tex: &wgpu::Texture,
) -> Result<(), BoxError> {
    let size = (u32::try_from(view.w)?, u32::try_from(view.h)?);
    let gpu_px = g.read_back(tex, size);
    let mut cpu = FrameRenderer::new(c.threads);
    let mut out = vec![0u32; usize::try_from(view.w * view.h)?];
    cpu.render(scene, view, c.group, time, c.style, &mut out);
    let (mut differ, mut max) = (0usize, 0u8);
    for (p, q) in out.iter().zip(gpu_px.as_chunks::<4>().0) {
        let [_, r, g, b] = p.to_be_bytes();
        let d = [r.abs_diff(q[0]), g.abs_diff(q[1]), b.abs_diff(q[2])];
        let m = d.into_iter().max().unwrap_or(0);
        max = max.max(m);
        if m > 2 {
            differ += 1;
        }
    }
    if let Some(dir) = std::env::var_os("BENCH_DUMP") {
        let name = format!("g{}_{:?}", c.group, c.style);
        let cpu_rgba: Vec<u8> = out
            .iter()
            .flat_map(|p| {
                let [_, r, g, b] = p.to_be_bytes();
                [r, g, b, 255]
            })
            .collect();
        std::fs::write(Path::new(&dir).join(format!("{name}_cpu.rgba")), cpu_rgba)?;
        std::fs::write(Path::new(&dir).join(format!("{name}_gpu.rgba")), &gpu_px)?;
    }
    eprintln!(
        "check {}x{} group {} {:?}: {differ} of {} pixels differ by more than 2, max difference {max}",
        view.w,
        view.h,
        c.group,
        c.style,
        out.len()
    );
    Ok(())
}

fn report(c: &Config, renderer: &str, mode: &str, name: &str, s: &[Row]) {
    let (w, h, group) = (c.size.0, c.size.1, c.group);
    let mut total: Vec<f64> = s.iter().map(|f| f.total).collect();
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
    let mean = |f: &dyn Fn(&Row) -> f64| s.iter().map(f).sum::<f64>() / n;
    let within = s.iter().filter(|f| f.total <= 16.7).count();
    let style = match c.style {
        WaterStyle::Original => "original",
        WaterStyle::Enhanced => "enhanced",
    };
    println!(
        "| {w}×{h} | {group} | {style} | {renderer} | {mode} | {name} | {} | {:.1} | {:.1} | {:.1} | {:.1} | {:.0} % | {} | {:.1} | {:.1} | {:.1} | {:.1} | {:.1} | {:.1} |",
        s.len(),
        pct(0.5),
        pct(0.95),
        pct(0.99),
        total.last().copied().unwrap_or(0.0),
        100.0 * f64::from(u32::try_from(within).unwrap_or(u32::MAX)) / n,
        s.iter().map(|f| f.built).sum::<usize>(),
        mean(&|f| f.chunk_ms),
        mean(&|f| f64::from(u32::try_from(f.missing).unwrap_or(u32::MAX))),
        mean(&|f| f.stages[0]),
        mean(&|f| f.stages[1]),
        mean(&|f| f.stages[2]),
        mean(&|f| f.stages[3]),
    );
}
