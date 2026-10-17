//! Interactive scene window (`--view` without `--png`): `winit` window, pan
//! and zoom-group switching. Static chunks come from
//! `anno_render::frame::FrameRenderer` (built by background workers on half
//! the cores); frames are composed either on the GPU
//! (`anno_render::gpu::GpuFrame`, wgpu, presented with vsync) or on the CPU
//! (`FrameRenderer::render` into a `softbuffer` window, all cores).
//! `--renderer auto` (default) uses the GPU when one is available. The title
//! shows the renderer, frames per second and the average frame time; stderr
//! gets a per-stage breakdown every 2 seconds.
//! Controls: see the crate docs; Space pauses the animation, `E` toggles
//! the non-original enhanced water.

use std::num::NonZeroU32;
use std::sync::Arc;
use std::time::{Duration, Instant};

use anno_render::frame::FrameRenderer;
use anno_render::gpu::GpuFrame;
use anno_render::iso::{TILE, WaterStyle};
use anno_render::scene::{self, Rect, Scene};
use winit::application::ApplicationHandler;
use winit::dpi::LogicalSize;
use winit::event::{ElementState, MouseButton, MouseScrollDelta, WindowEvent};
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::keyboard::{Key, NamedKey};
use winit::window::{Window, WindowId};

use crate::BoxError;

/// Refresh interval when the display does not report its rate (60 Hz).
const DEFAULT_FRAME: Duration = Duration::from_micros(16_667);
/// Retry interval while the window cannot be presented to (minimised or
/// hidden).
const OCCLUDED_RETRY: Duration = Duration::from_millis(100);
/// Interval of the FPS display and the stderr breakdown.
const REPORT: Duration = Duration::from_secs(2);

/// Which renderer composes the frames.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Renderer {
    /// The GPU if one can be opened, else the CPU.
    Auto,
    Gpu,
    Cpu,
}

/// Frame statistics since the last report: frames, summed and worst frame
/// time, summed chunk time and summed renderer stages (see `STAGES_*`).
#[derive(Default)]
struct Meter {
    frames: u32,
    total_ms: f64,
    worst_ms: f64,
    chunks_ms: f64,
    stages: [f64; 4],
}

const STAGES_CPU: [&str; 4] = ["ground", "water objects", "sea", "objects"];
const STAGES_GPU: [&str; 4] = ["uploads", "sprites", "encode", "present"];

/// Opens the window and runs until it is closed.
pub fn run(scene: &Scene<'_>, group: usize, renderer: Renderer) -> Result<(), BoxError> {
    let bounds = scene::scene_bounds(&scene.layouts[group]).ok_or("no islands")?;
    let threads = std::thread::available_parallelism().map_or(4, std::num::NonZeroUsize::get);
    let mut viewer = Viewer {
        scene,
        group,
        cam: (
            f64::from(bounds.x) + f64::from(bounds.w) / 2.0,
            f64::from(bounds.y) + f64::from(bounds.h) / 2.0,
        ),
        window: None,
        renderer,
        frames: FrameRenderer::new(threads),
        clock: Instant::now(),
        paused_at: None,
        water_style: WaterStyle::Original,
        interval: DEFAULT_FRAME,
        occluded_until: None,
        next_frame: Instant::now(),
        meter: Meter::default(),
        meter_since: Instant::now(),
        fps: None,
        drag: false,
        cursor: None,
        scroll: 0.0,
        error: None,
    };
    let event_loop = EventLoop::new()?;
    event_loop.set_control_flow(ControlFlow::Wait);
    // Chunk builders use half the cores; frames use all of them.
    std::thread::scope(|s| {
        // Sea patterns of all zoom groups, built off the window thread.
        for g in 0..TILE.len() {
            s.spawn(move || {
                scene.sea(g);
            });
        }
        let workers = anno_render::frame::spawn_chunk_workers(s, scene, threads.div_ceil(2));
        viewer.frames.set_workers(Some(workers));
        let result = event_loop.run_app(&mut viewer);
        // Stops the workers, so that the scope can end.
        viewer.frames.set_workers(None);
        result
    })?;
    viewer.error.map_or(Ok(()), Err)
}

/// The window and how frames reach it.
struct Gfx {
    window: Arc<Window>,
    backend: Backend,
}

enum Backend {
    Cpu {
        // Kept alive for the surface.
        _context: softbuffer::Context<Arc<Window>>,
        surface: softbuffer::Surface<Arc<Window>, Arc<Window>>,
    },
    Gpu {
        surface: wgpu::Surface<'static>,
        config: wgpu::SurfaceConfiguration,
        frame: Box<GpuFrame>,
        name: String,
    },
}

struct Viewer<'s, 'a> {
    scene: &'s Scene<'a>,
    group: usize,
    /// Scene screen position (at `group`) shown in the window centre.
    cam: (f64, f64),
    window: Option<Gfx>,
    renderer: Renderer,
    frames: FrameRenderer,
    /// Animation clock; while paused, the time it was paused at.
    clock: Instant,
    paused_at: Option<Duration>,
    /// Sea style; `E` toggles the non-original enhanced water.
    water_style: WaterStyle,
    /// Redraw interval while animating (display refresh).
    interval: Duration,
    /// Set while the GPU surface reports the window as occluded: no redraw
    /// before this time.
    occluded_until: Option<Instant>,
    next_frame: Instant,
    meter: Meter,
    meter_since: Instant,
    /// Last measured (frames per second, average frame ms).
    fps: Option<(f64, f64)>,
    drag: bool,
    cursor: Option<(f64, f64)>,
    /// Accumulated wheel movement in lines.
    scroll: f64,
    error: Option<BoxError>,
}

impl ApplicationHandler for Viewer<'_, '_> {
    fn about_to_wait(&mut self, el: &ActiveEventLoop) {
        if self.paused_at.is_some() && !self.frames.busy() {
            el.set_control_flow(ControlFlow::Wait);
            return;
        }
        if self
            .window
            .as_ref()
            .is_some_and(|g| matches!(g.backend, Backend::Gpu { .. }))
        {
            // Vsync paces the GPU path: acquiring the next surface texture
            // waits for the display, so a redraw can always be requested,
            // except while the window is occluded.
            match self.occluded_until {
                Some(t) if Instant::now() < t => el.set_control_flow(ControlFlow::WaitUntil(t)),
                _ => {
                    self.occluded_until = None;
                    self.redraw();
                    el.set_control_flow(ControlFlow::Wait);
                }
            }
            return;
        }
        let now = Instant::now();
        if now >= self.next_frame {
            self.next_frame = (self.next_frame + self.interval).max(now);
            self.redraw();
        }
        el.set_control_flow(ControlFlow::WaitUntil(self.next_frame));
    }

    fn resumed(&mut self, el: &ActiveEventLoop) {
        if self.window.is_some() {
            return;
        }
        match create_gfx(el, self.renderer) {
            Ok(g) => {
                if let Some(mhz) = g
                    .window
                    .current_monitor()
                    .and_then(|m| m.refresh_rate_millihertz())
                    .filter(|&r| r > 0)
                {
                    self.interval = Duration::from_micros(1_000_000_000 / u64::from(mhz));
                }
                self.window = Some(g);
                self.update_title();
            }
            Err(e) => {
                self.error = Some(e);
                el.exit();
            }
        }
    }

    fn window_event(&mut self, el: &ActiveEventLoop, _id: WindowId, event: WindowEvent) {
        match event {
            WindowEvent::CloseRequested => el.exit(),
            WindowEvent::Resized(_) => self.redraw(),
            WindowEvent::RedrawRequested => {
                if let Err(e) = self.draw() {
                    self.error = Some(e);
                    el.exit();
                }
            }
            WindowEvent::KeyboardInput { event, .. } if event.state == ElementState::Pressed => {
                let (w, h) = self.view_size();
                let (dx, dy) = (f64::from(w) / 4.0, f64::from(h) / 4.0);
                match event.logical_key.as_ref() {
                    Key::Named(NamedKey::Escape) => el.exit(),
                    Key::Named(NamedKey::ArrowLeft) => self.pan(-dx, 0.0),
                    Key::Named(NamedKey::ArrowRight) => self.pan(dx, 0.0),
                    Key::Named(NamedKey::ArrowUp) => self.pan(0.0, -dy),
                    Key::Named(NamedKey::ArrowDown) => self.pan(0.0, dy),
                    Key::Character(c) => match c.to_ascii_lowercase().as_str() {
                        "q" => el.exit(),
                        " " => self.toggle_pause(),
                        "e" => self.toggle_water(),
                        "a" => self.pan(-dx, 0.0),
                        "d" => self.pan(dx, 0.0),
                        "w" => self.pan(0.0, -dy),
                        "s" => self.pan(0.0, dy),
                        "+" | "=" => self.zoom(-1, None),
                        "-" | "_" => self.zoom(1, None),
                        _ => {}
                    },
                    _ => {}
                }
            }
            WindowEvent::MouseInput {
                state,
                button: MouseButton::Left,
                ..
            } => self.drag = state == ElementState::Pressed,
            WindowEvent::CursorMoved { position, .. } => {
                let now = (position.x, position.y);
                if let (true, Some(last)) = (self.drag, self.cursor) {
                    self.pan(last.0 - now.0, last.1 - now.1);
                }
                self.cursor = Some(now);
            }
            WindowEvent::CursorLeft { .. } => self.cursor = None,
            WindowEvent::MouseWheel { delta, .. } => {
                self.scroll += match delta {
                    MouseScrollDelta::LineDelta(_, y) => f64::from(y),
                    MouseScrollDelta::PixelDelta(p) => p.y / 50.0,
                };
                if self.scroll.abs() >= 1.0 {
                    // Wheel up = closer = lower group.
                    let step = if self.scroll > 0.0 { -1 } else { 1 };
                    self.scroll = 0.0;
                    self.zoom(step, self.cursor);
                }
            }
            _ => {}
        }
    }
}

fn create_gfx(el: &ActiveEventLoop, renderer: Renderer) -> Result<Gfx, BoxError> {
    let attrs = Window::default_attributes()
        .with_title("anno1503")
        .with_inner_size(LogicalSize::new(1280.0, 800.0));
    let window = Arc::new(el.create_window(attrs)?);
    if renderer != Renderer::Cpu {
        match create_gpu(&window) {
            Ok(backend) => return Ok(Gfx { window, backend }),
            Err(e) if renderer == Renderer::Auto => {
                eprintln!("GPU renderer unavailable ({e}); using the CPU renderer");
            }
            Err(e) => return Err(e),
        }
    }
    let context = softbuffer::Context::new(window.clone())?;
    let surface = softbuffer::Surface::new(&context, window.clone())?;
    Ok(Gfx {
        window,
        backend: Backend::Cpu {
            _context: context,
            surface,
        },
    })
}

/// Opens the high-performance GPU for `window`, presenting with vsync.
fn create_gpu(window: &Arc<Window>) -> Result<Backend, BoxError> {
    let instance = wgpu::Instance::new(wgpu::InstanceDescriptor::new_with_display_handle_from_env(
        Box::new(window.clone()),
    ));
    let surface = instance.create_surface(window.clone())?;
    let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::HighPerformance,
        compatible_surface: Some(&surface),
        ..Default::default()
    }))?;
    let info = adapter.get_info();
    let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
        label: Some("anno1503"),
        required_limits: adapter.limits(),
        ..Default::default()
    }))?;
    let size = window.inner_size();
    let mut config = surface
        .get_default_config(&adapter, size.width.max(1), size.height.max(1))
        .ok_or("the GPU cannot present to this window")?;
    // Colours are written as stored (no sRGB conversion).
    config.view_formats.push(config.format.remove_srgb_suffix());
    config.present_mode = wgpu::PresentMode::AutoVsync;
    surface.configure(&device, &config);
    let frame = GpuFrame::new(device, queue, config.format);
    let name = format!("{} ({:?})", info.name, info.backend);
    eprintln!("GPU renderer: {name}");
    Ok(Backend::Gpu {
        surface,
        config,
        frame: Box::new(frame),
        name,
    })
}

impl Viewer<'_, '_> {
    fn view_size(&self) -> (u32, u32) {
        self.window.as_ref().map_or((1, 1), |g| {
            let s = g.window.inner_size();
            (s.width.max(1), s.height.max(1))
        })
    }

    fn redraw(&self) {
        if let Some(g) = &self.window {
            g.window.request_redraw();
        }
    }

    fn update_title(&self) {
        if let Some(g) = &self.window {
            let water = match self.water_style {
                WaterStyle::Original => "",
                WaterStyle::Enhanced => " — enhanced water (not original)",
            };
            let fps = self.fps.map_or(String::new(), |(f, ms)| {
                format!(" — {f:.0} fps ({ms:.1} ms/frame)")
            });
            let renderer = match &g.backend {
                Backend::Cpu { .. } => "CPU".to_string(),
                Backend::Gpu { name, .. } => format!("GPU {name}"),
            };
            g.window.set_title(&format!(
                "anno1503 — {} — zoom group {}{water} — {renderer}{fps}",
                self.scene.name, self.group
            ));
        }
    }

    /// Animation time in milliseconds.
    fn time_ms(&self) -> u64 {
        let t = self.paused_at.unwrap_or_else(|| self.clock.elapsed());
        u64::try_from(t.as_millis()).unwrap_or(u64::MAX)
    }

    fn toggle_water(&mut self) {
        self.water_style = match self.water_style {
            WaterStyle::Original => WaterStyle::Enhanced,
            WaterStyle::Enhanced => WaterStyle::Original,
        };
        self.update_title();
        self.redraw();
    }

    fn toggle_pause(&mut self) {
        match self.paused_at.take() {
            Some(t) => self.clock = Instant::now() - t,
            None => self.paused_at = Some(self.clock.elapsed()),
        }
        self.redraw();
    }

    fn pan(&mut self, dx: f64, dy: f64) {
        self.cam.0 += dx;
        self.cam.1 += dy;
        self.clamp();
        self.redraw();
    }

    /// Keeps the view centre inside the scene bounds.
    fn clamp(&mut self) {
        if let Some(b) = scene::scene_bounds(&self.scene.layouts[self.group]) {
            self.cam.0 = self
                .cam
                .0
                .clamp(f64::from(b.x), f64::from(b.x) + f64::from(b.w));
            self.cam.1 = self
                .cam
                .1
                .clamp(f64::from(b.y), f64::from(b.y) + f64::from(b.h));
        }
    }

    /// Switches to the neighbouring zoom group (`step` −1 = closer), keeping
    /// the scene point under `anchor` (window pixels; default centre) fixed.
    fn zoom(&mut self, step: i32, anchor: Option<(f64, f64)>) {
        let Some(new) = self
            .group
            .checked_add_signed(isize::try_from(step).unwrap_or(0))
            .filter(|&g| g < 3)
        else {
            return;
        };
        let (w, h) = self.view_size();
        let d = anchor.map_or((0.0, 0.0), |(x, y)| {
            (x - f64::from(w) / 2.0, y - f64::from(h) / 2.0)
        });
        let (ot, nt) = (TILE[self.group], TILE[new]);
        // Screen coordinates are linear in tile coordinates per group.
        let p = (self.cam.0 + d.0, self.cam.1 + d.1);
        let p = (
            p.0 * f64::from(nt.0) / f64::from(ot.0),
            p.1 * f64::from(nt.1) / f64::from(ot.1),
        );
        self.cam = (p.0 - d.0, p.1 - d.1);
        self.group = new;
        self.clamp();
        self.update_title();
        self.redraw();
    }

    fn draw(&mut self) -> Result<(), BoxError> {
        let (w, h) = self.view_size();
        let (wi, hi) = (i32::try_from(w)?, i32::try_from(h)?);
        // Scene screen position of the window's top-left pixel.
        #[allow(clippy::cast_possible_truncation)]
        let view = Rect {
            x: (self.cam.0 - f64::from(w) / 2.0).round() as i32,
            y: (self.cam.1 - f64::from(h) / 2.0).round() as i32,
            w: wi,
            h: hi,
        };
        let t = self.time_ms();
        let (group, style, scene) = (self.group, self.water_style, self.scene);
        let animating = self.paused_at.is_none();
        let Some(gfx) = self.window.as_mut() else {
            return Ok(());
        };
        let (total, chunks_ms, built, stages) = match &mut gfx.backend {
            Backend::Cpu { surface, .. } => {
                let (Some(nw), Some(nh)) = (NonZeroU32::new(w), NonZeroU32::new(h)) else {
                    return Ok(());
                };
                surface.resize(nw, nh)?;
                let mut buf = surface.buffer_mut()?;
                let s = self.frames.render(scene, view, group, t, style, &mut buf);
                buf.present()?;
                (
                    s.total_ms,
                    s.chunks_ms,
                    s.chunks_built,
                    [s.ground_ms, s.water_objects_ms, s.sea_ms, s.objects_ms],
                )
            }
            Backend::Gpu {
                surface,
                config,
                frame,
                ..
            } => {
                if (config.width, config.height) != (w, h) {
                    (config.width, config.height) = (w, h);
                    surface.configure(&frame.device, config);
                }
                let texture = match surface.get_current_texture() {
                    wgpu::CurrentSurfaceTexture::Success(t)
                    | wgpu::CurrentSurfaceTexture::Suboptimal(t) => t,
                    wgpu::CurrentSurfaceTexture::Outdated | wgpu::CurrentSurfaceTexture::Lost => {
                        surface.configure(&frame.device, config);
                        gfx.window.request_redraw();
                        return Ok(());
                    }
                    wgpu::CurrentSurfaceTexture::Occluded => {
                        self.occluded_until = Some(Instant::now() + OCCLUDED_RETRY);
                        return Ok(());
                    }
                    wgpu::CurrentSurfaceTexture::Timeout
                    | wgpu::CurrentSurfaceTexture::Validation => {
                        gfx.window.request_redraw();
                        return Ok(());
                    }
                };
                let target = texture.texture.create_view(&wgpu::TextureViewDescriptor {
                    format: Some(frame.view_format()),
                    ..Default::default()
                });
                let s = frame.render(&mut self.frames, scene, view, group, t, style, &target);
                let p = Instant::now();
                gfx.window.pre_present_notify();
                frame.queue.present(texture);
                let present = p.elapsed().as_secs_f64() * 1000.0;
                if animating || self.frames.busy() {
                    gfx.window.request_redraw();
                }
                (
                    s.total_ms + present,
                    s.chunks_ms,
                    s.chunks_built,
                    [s.upload_ms, s.sprites_ms, s.encode_ms, present],
                )
            }
        };
        if built > 0 {
            eprintln!("zoom group {group}: {built} chunk(s) received");
        }
        self.measure(total, chunks_ms, stages);
        Ok(())
    }

    /// Accumulates frame statistics; every [`REPORT`] updates the title and
    /// prints the per-stage averages.
    fn measure(&mut self, total_ms: f64, chunks_ms: f64, stages: [f64; 4]) {
        let m = &mut self.meter;
        m.frames += 1;
        m.worst_ms = m.worst_ms.max(total_ms);
        m.total_ms += total_ms;
        m.chunks_ms += chunks_ms;
        for (sum, v) in m.stages.iter_mut().zip(stages) {
            *sum += v;
        }
        let elapsed = self.meter_since.elapsed();
        if elapsed < REPORT {
            return;
        }
        let n = f64::from(m.frames);
        let fps = n / elapsed.as_secs_f64();
        let names = match self.window.as_ref().map(|g| &g.backend) {
            Some(Backend::Gpu { .. }) => STAGES_GPU,
            _ => STAGES_CPU,
        };
        let stages: Vec<String> = names
            .iter()
            .zip(m.stages)
            .map(|(name, v)| format!("{name} {:.1}", v / n))
            .collect();
        eprintln!(
            "zoom group {}: {fps:.1} fps, frame avg {:.1} ms / worst {:.1} ms \
             (chunks {:.1}, {}; {} chunks cached)",
            self.group,
            m.total_ms / n,
            m.worst_ms,
            m.chunks_ms / n,
            stages.join(", "),
            self.frames.cached(),
        );
        self.fps = Some((fps, m.total_ms / n));
        self.meter = Meter::default();
        self.meter_since = Instant::now();
        self.update_title();
    }
}
