//! Interactive scene window (`--view` without `--png`): `winit` window,
//! `softbuffer` CPU blit, pan and zoom-group switching. Frames come from
//! `anno_render::frame::FrameRenderer` (cached static chunks, per-frame
//! water and sea on all cores). The window redraws at the display's refresh
//! rate while animating; the title shows frames per second and the average
//! frame time, and stderr gets a per-stage breakdown every 2 seconds.
//! Controls: see the crate docs; Space pauses the animation, `E` toggles
//! the non-original enhanced water.

use std::num::NonZeroU32;
use std::rc::Rc;
use std::time::{Duration, Instant};

use anno_render::frame::{FrameRenderer, FrameStats};
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
/// Interval of the FPS display and the stderr breakdown.
const REPORT: Duration = Duration::from_secs(2);

/// Frame statistics since the last report.
#[derive(Default)]
struct Meter {
    frames: u32,
    sum: FrameStats,
    worst_ms: f64,
}

/// Opens the window and runs until it is closed.
pub fn run(scene: &Scene<'_>, group: usize) -> Result<(), BoxError> {
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
        frames: FrameRenderer::new(threads),
        clock: Instant::now(),
        paused_at: None,
        water_style: WaterStyle::Original,
        interval: DEFAULT_FRAME,
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

struct Gfx {
    window: Rc<Window>,
    // Kept alive for the surface.
    _context: softbuffer::Context<Rc<Window>>,
    surface: softbuffer::Surface<Rc<Window>, Rc<Window>>,
}

struct Viewer<'s, 'a> {
    scene: &'s Scene<'a>,
    group: usize,
    /// Scene screen position (at `group`) shown in the window centre.
    cam: (f64, f64),
    window: Option<Gfx>,
    frames: FrameRenderer,
    /// Animation clock; while paused, the time it was paused at.
    clock: Instant,
    paused_at: Option<Duration>,
    /// Sea style; `E` toggles the non-original enhanced water.
    water_style: WaterStyle,
    /// Redraw interval while animating (display refresh).
    interval: Duration,
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
        match create_gfx(el) {
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

fn create_gfx(el: &ActiveEventLoop) -> Result<Gfx, BoxError> {
    let attrs = Window::default_attributes()
        .with_title("anno1503")
        .with_inner_size(LogicalSize::new(1280.0, 800.0));
    let window = Rc::new(el.create_window(attrs)?);
    let context = softbuffer::Context::new(window.clone())?;
    let surface = softbuffer::Surface::new(&context, window.clone())?;
    Ok(Gfx {
        window,
        _context: context,
        surface,
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
            g.window.set_title(&format!(
                "anno1503 — {} — zoom group {}{water}{fps}",
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
        let Some(gfx) = self.window.as_mut() else {
            return Ok(());
        };
        let (Some(nw), Some(nh)) = (NonZeroU32::new(w), NonZeroU32::new(h)) else {
            return Ok(());
        };
        gfx.surface.resize(nw, nh)?;
        let mut buf = gfx.surface.buffer_mut()?;
        let stats = self.frames.render(scene, view, group, t, style, &mut buf);
        buf.present()?;
        if stats.chunks_built > 0 {
            eprintln!(
                "zoom group {group}: {} chunk(s) rendered in {:.0} ms",
                stats.chunks_built, stats.chunks_ms
            );
        }
        self.measure(&stats);
        Ok(())
    }

    /// Accumulates frame statistics; every [`REPORT`] updates the title and
    /// prints the per-stage averages.
    fn measure(&mut self, s: &FrameStats) {
        let m = &mut self.meter;
        m.frames += 1;
        m.worst_ms = m.worst_ms.max(s.total_ms);
        m.sum.chunks_built += s.chunks_built;
        m.sum.chunks_ms += s.chunks_ms;
        m.sum.ground_ms += s.ground_ms;
        m.sum.water_objects_ms += s.water_objects_ms;
        m.sum.sea_ms += s.sea_ms;
        m.sum.objects_ms += s.objects_ms;
        m.sum.total_ms += s.total_ms;
        let elapsed = self.meter_since.elapsed();
        if elapsed < REPORT {
            return;
        }
        let n = f64::from(m.frames);
        let fps = n / elapsed.as_secs_f64();
        let avg = |v: f64| v / n;
        eprintln!(
            "zoom group {}: {fps:.1} fps, frame avg {:.1} ms / worst {:.1} ms \
             (chunks {:.1}, ground {:.1}, water objects {:.1}, sea {:.1}, objects {:.1}; {} chunks cached)",
            self.group,
            avg(m.sum.total_ms),
            m.worst_ms,
            avg(m.sum.chunks_ms),
            avg(m.sum.ground_ms),
            avg(m.sum.water_objects_ms),
            avg(m.sum.sea_ms),
            avg(m.sum.objects_ms),
            self.frames.cached(),
        );
        self.fps = Some((fps, avg(m.sum.total_ms)));
        self.meter = Meter::default();
        self.meter_since = Instant::now();
        self.update_title();
    }
}
