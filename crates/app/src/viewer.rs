//! Interactive scene window (`--view` without `--png`): `winit` window,
//! `softbuffer` CPU blit, pan and zoom-group switching. The static layers
//! (ground; objects) are rendered in [`CHUNK`]-sized squares of scene screen
//! space, on all cores, and cached per zoom group. The water layer (sea
//! surface, rivers, surf) is redrawn every [`FRAME_MS`] for the visible
//! part of each chunk, at the animation time, and composited between them.
//! Controls: see the crate docs; Space pauses the animation.

use std::collections::BTreeMap;
use std::num::NonZeroU32;
use std::rc::Rc;
use std::sync::Mutex;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};

use anno_assets::island::WATER;
use anno_render::iso::{Canvas, TILE, over};
use anno_render::scene::{self, Rect};
use winit::application::ApplicationHandler;
use winit::dpi::LogicalSize;
use winit::event::{ElementState, MouseButton, MouseScrollDelta, WindowEvent};
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::keyboard::{Key, NamedKey};
use winit::window::{Window, WindowId};

use crate::{BoxError, Scene};

/// Edge length of a cached chunk in screen pixels.
const CHUNK: i32 = 1024;
/// Cache limit in chunks (4 MiB each); chunks of other groups and those
/// farthest from the view go first.
const MAX_CHUNKS: usize = 160;
/// Background (open sea): the island-water placeholder colour, 0RGB.
const BACKGROUND: u32 = rgb(WATER[0], WATER[1], WATER[2]);
/// Redraw interval while animating: the sea sprite's frame time (50 ms in
/// its `ANIM` record); other water objects use their own frame times.
const FRAME_MS: u64 = 50;

/// Cached static layers of one chunk; empty vectors = no island overlaps.
struct ChunkLayers {
    /// Ground over [`BACKGROUND`], 0RGB.
    ground: Vec<u32>,
    /// Placed objects (except water objects), straight-alpha RGBA.
    objects: Vec<[u8; 4]>,
}

const fn rgb(r: u8, g: u8, b: u8) -> u32 {
    (r as u32) << 16 | (g as u32) << 8 | b as u32
}

/// Opens the window and runs until it is closed.
pub fn run(scene: &Scene<'_>, group: usize) -> Result<(), BoxError> {
    let bounds = scene::scene_bounds(&scene.layouts[group]).ok_or("no islands")?;
    let mut viewer = Viewer {
        scene,
        group,
        cam: (
            f64::from(bounds.x) + f64::from(bounds.w) / 2.0,
            f64::from(bounds.y) + f64::from(bounds.h) / 2.0,
        ),
        window: None,
        cache: BTreeMap::new(),
        clock: Instant::now(),
        paused_at: None,
        next_frame: Instant::now(),
        frames: (0, Duration::ZERO),
        drag: false,
        cursor: None,
        scroll: 0.0,
        error: None,
    };
    let event_loop = EventLoop::new()?;
    event_loop.set_control_flow(ControlFlow::Wait);
    event_loop.run_app(&mut viewer)?;
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
    /// `(group, chunk x, chunk y)` → static layers.
    cache: BTreeMap<(usize, i32, i32), ChunkLayers>,
    /// Animation clock; while paused, the time it was paused at.
    clock: Instant,
    paused_at: Option<Duration>,
    next_frame: Instant,
    /// Frames drawn and their summed draw time, for the stderr log.
    frames: (u32, Duration),
    drag: bool,
    cursor: Option<(f64, f64)>,
    /// Accumulated wheel movement in lines.
    scroll: f64,
    error: Option<BoxError>,
}

impl ApplicationHandler for Viewer<'_, '_> {
    fn about_to_wait(&mut self, el: &ActiveEventLoop) {
        if self.paused_at.is_some() {
            el.set_control_flow(ControlFlow::Wait);
            return;
        }
        let now = Instant::now();
        if now >= self.next_frame {
            self.next_frame = now + Duration::from_millis(FRAME_MS);
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
            g.window.set_title(&format!(
                "anno1503 — {} — zoom group {}",
                self.scene.name, self.group
            ));
        }
    }

    /// Animation time in milliseconds.
    fn time_ms(&self) -> u64 {
        let t = self.paused_at.unwrap_or_else(|| self.clock.elapsed());
        u64::try_from(t.as_millis()).unwrap_or(u64::MAX)
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
        let started = Instant::now();
        self.draw_frame()?;
        self.frames.0 += 1;
        self.frames.1 += started.elapsed();
        if self.frames.0 == 100 {
            eprintln!(
                "zoom group {}: average frame {:.1} ms",
                self.group,
                self.frames.1.as_secs_f64() * 10.0
            );
            self.frames = (0, Duration::ZERO);
        }
        Ok(())
    }

    fn draw_frame(&mut self) -> Result<(), BoxError> {
        let (w, h) = self.view_size();
        let (wi, hi) = (i32::try_from(w)?, i32::try_from(h)?);
        // Scene screen position of the window's top-left pixel.
        #[allow(clippy::cast_possible_truncation)]
        let (vx, vy) = (
            (self.cam.0 - f64::from(w) / 2.0).round() as i32,
            (self.cam.1 - f64::from(h) / 2.0).round() as i32,
        );
        let view = Rect {
            x: vx,
            y: vy,
            w: wi,
            h: hi,
        };
        let chunks = visible_chunks(view);
        self.fill_cache(&chunks);
        let t = self.time_ms();

        let Some(gfx) = self.window.as_mut() else {
            return Ok(());
        };
        let (Some(nw), Some(nh)) = (NonZeroU32::new(w), NonZeroU32::new(h)) else {
            return Ok(());
        };
        gfx.surface.resize(nw, nh)?;
        let mut buf = gfx.surface.buffer_mut()?;
        let group = self.group;
        blit(&mut buf, view, &chunks, |cx, cy| {
            &self.cache[&(group, cx, cy)].ground
        });
        // Water at the animation time, then the cached objects, per visible
        // chunk part (water rendered in parallel).
        let parts: Vec<Rect> = chunks
            .iter()
            .filter(|&&(cx, cy)| !self.cache[&(group, cx, cy)].ground.is_empty())
            .filter_map(|&(cx, cy)| chunk_rect(cx, cy).intersect(view))
            .collect();
        let scene = self.scene;
        let water: Vec<Canvas> = std::thread::scope(|s| {
            let jobs: Vec<_> = parts
                .iter()
                .map(|&r| {
                    s.spawn(move || {
                        let layers = scene::Layers {
                            ground: false,
                            water: Some(t),
                            objects: false,
                        };
                        scene.render(r, group, layers).0
                    })
                })
                .collect();
            jobs.into_iter()
                .map(|j| j.join().expect("no panic"))
                .collect()
        });
        for c in &water {
            composite_rgba(&mut buf, view, c);
        }
        for &(cx, cy) in &chunks {
            let layer = &self.cache[&(group, cx, cy)].objects;
            if !layer.is_empty() {
                composite_layer(&mut buf, view, chunk_rect(cx, cy), layer);
            }
        }
        buf.present()?;
        Ok(())
    }

    /// Renders the missing chunks of `wanted` (in parallel) and trims the
    /// cache.
    fn fill_cache(&mut self, wanted: &[(i32, i32)]) {
        let group = self.group;
        let todo: Vec<(i32, i32)> = wanted
            .iter()
            .copied()
            .filter(|&(cx, cy)| !self.cache.contains_key(&(group, cx, cy)))
            .collect();
        if !todo.is_empty() {
            let started = Instant::now();
            eprintln!("zoom group {group}: rendering {} chunk(s) …", todo.len());
            let next = AtomicUsize::new(0);
            let done = Mutex::new(Vec::new());
            let workers = std::thread::available_parallelism()
                .map_or(4, std::num::NonZeroUsize::get)
                .min(todo.len());
            let scene = self.scene;
            std::thread::scope(|s| {
                for _ in 0..workers {
                    s.spawn(|| {
                        loop {
                            let i = next.fetch_add(1, Ordering::Relaxed);
                            let Some(&(cx, cy)) = todo.get(i) else {
                                break;
                            };
                            let px = render_chunk(scene, group, cx, cy);
                            done.lock().expect("no panic").push(((group, cx, cy), px));
                        }
                    });
                }
            });
            self.cache.extend(done.into_inner().expect("no panic"));
            eprintln!(
                "zoom group {group}: done in {:.1} s",
                started.elapsed().as_secs_f64()
            );
        }
        if self.cache.len() > MAX_CHUNKS {
            #[allow(clippy::cast_possible_truncation)]
            let (ccx, ccy) = (
                (self.cam.0 / f64::from(CHUNK)).floor() as i64,
                (self.cam.1 / f64::from(CHUNK)).floor() as i64,
            );
            let mut keys: Vec<(bool, i64, (usize, i32, i32))> = self
                .cache
                .keys()
                .map(|&k @ (g, cx, cy)| {
                    let dist = (i64::from(cx) - ccx).abs() + (i64::from(cy) - ccy).abs();
                    (g != group, dist, k)
                })
                .collect();
            keys.sort_unstable();
            for (_, _, k) in keys.into_iter().skip(MAX_CHUNKS.max(wanted.len())) {
                self.cache.remove(&k);
            }
        }
    }
}

/// Chunks `(cx, cy)` that overlap `view`, row by row.
fn visible_chunks(view: Rect) -> Vec<(i32, i32)> {
    let (x0, x1) = (
        view.x.div_euclid(CHUNK),
        (view.x + view.w - 1).div_euclid(CHUNK),
    );
    let (y0, y1) = (
        view.y.div_euclid(CHUNK),
        (view.y + view.h - 1).div_euclid(CHUNK),
    );
    (y0..=y1)
        .flat_map(|cy| (x0..=x1).map(move |cx| (cx, cy)))
        .collect()
}

/// Copies the parts of `chunks` inside `view` into `out` (`view.w × view.h`
/// 0RGB pixels, row-major); `chunk` returns a chunk's pixels (empty =
/// background).
fn blit<'c>(
    out: &mut [u32],
    view: Rect,
    chunks: &[(i32, i32)],
    chunk: impl Fn(i32, i32) -> &'c [u32],
) {
    let idx = |v: i32| usize::try_from(v).expect("inside");
    let width = idx(view.w);
    for &(cx, cy) in chunks {
        let rect = Rect {
            x: cx * CHUNK,
            y: cy * CHUNK,
            w: CHUNK,
            h: CHUNK,
        };
        let Some(r) = rect.intersect(view) else {
            continue;
        };
        let px = chunk(cx, cy);
        for row in 0..r.h {
            let d = idx(r.y + row - view.y) * width + idx(r.x - view.x);
            let dst = &mut out[d..d + idx(r.w)];
            if px.is_empty() {
                dst.fill(BACKGROUND);
            } else {
                let s = idx(r.y + row - rect.y) * idx(CHUNK) + idx(r.x - rect.x);
                dst.copy_from_slice(&px[s..s + idx(r.w)]);
            }
        }
    }
}

/// Scene rectangle of chunk `(cx, cy)`.
fn chunk_rect(cx: i32, cy: i32) -> Rect {
    Rect {
        x: cx * CHUNK,
        y: cy * CHUNK,
        w: CHUNK,
        h: CHUNK,
    }
}

/// `src` (opaque destination assumed) blended over a 0RGB pixel.
fn over_rgb(dst: &mut u32, src: [u8; 4]) {
    if src[3] == 0 {
        return;
    }
    let [_, r, g, b] = dst.to_be_bytes();
    let mut d = [r, g, b, 255];
    over(&mut d, &src);
    *dst = rgb(d[0], d[1], d[2]);
}

/// Composites a transparent canvas (scene coordinates) over `out`.
fn composite_rgba(out: &mut [u32], view: Rect, c: &Canvas) {
    let to_i32 = |n: usize| i32::try_from(n).expect("canvas fits i32");
    let rect = Rect {
        x: c.origin.0,
        y: c.origin.1,
        w: to_i32(c.width),
        h: to_i32(c.height),
    };
    composite_layer(out, view, rect, c.pixels.as_chunks::<4>().0);
}

/// Composites `layer` (RGBA pixels covering `rect`) over `out` within `view`.
fn composite_layer(out: &mut [u32], view: Rect, rect: Rect, layer: &[[u8; 4]]) {
    let Some(r) = rect.intersect(view) else {
        return;
    };
    let idx = |v: i32| usize::try_from(v).expect("inside");
    let (width, lw) = (idx(view.w), idx(rect.w));
    for row in 0..r.h {
        let d = idx(r.y + row - view.y) * width + idx(r.x - view.x);
        let s = idx(r.y + row - rect.y) * lw + idx(r.x - rect.x);
        for (dst, &src) in out[d..d + idx(r.w)].iter_mut().zip(&layer[s..s + idx(r.w)]) {
            over_rgb(dst, src);
        }
    }
}

/// The static layers of one chunk; empty if no island overlaps it.
fn render_chunk(scene: &Scene<'_>, group: usize, cx: i32, cy: i32) -> ChunkLayers {
    let rect = chunk_rect(cx, cy);
    if !scene.layouts[group]
        .iter()
        .any(|p| p.bounds.intersect(rect).is_some())
    {
        return ChunkLayers {
            ground: Vec::new(),
            objects: Vec::new(),
        };
    }
    let only = |ground, objects| scene::Layers {
        ground,
        water: None,
        objects,
    };
    let (ground, _) = scene.render(rect, group, only(true, false));
    let (objects, _) = scene.render(rect, group, only(false, true));
    let ground = ground
        .pixels
        .as_chunks::<4>()
        .0
        .iter()
        .map(|&p| {
            let mut px = BACKGROUND;
            over_rgb(&mut px, p);
            px
        })
        .collect();
    ChunkLayers {
        ground,
        objects: objects.pixels.as_chunks::<4>().0.to_vec(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn view_across_chunk_corners() {
        // A 4 × 3 view around the scene origin touches four chunks.
        let view = Rect {
            x: -2,
            y: -1,
            w: 4,
            h: 3,
        };
        let chunks = visible_chunks(view);
        assert_eq!(chunks, [(-1, -1), (0, -1), (-1, 0), (0, 0)]);
        // Chunk pixel value = 1,000,000 · chunk id + offset inside the chunk;
        // chunk (0, -1) is empty (background).
        let n = u32::try_from(CHUNK).expect("positive");
        let make = |id: u32| -> Vec<u32> { (0..n * n).map(|i| id * 1_000_000 + i).collect() };
        let (a, c, d) = (make(1), make(3), make(4));
        let mut out = vec![0; 12];
        blit(&mut out, view, &chunks, |cx, cy| match (cx, cy) {
            (-1, -1) => &a,
            (-1, 0) => &c,
            (0, 0) => &d,
            _ => &[],
        });
        let last_row = n * (n - 1);
        assert_eq!(
            out,
            [
                // Scene row -1: last row of chunk (-1, -1), then background.
                1_000_000 + last_row + n - 2,
                1_000_000 + last_row + n - 1,
                BACKGROUND,
                BACKGROUND,
                // Scene rows 0 and 1: chunks (-1, 0) and (0, 0).
                3_000_000 + n - 2,
                3_000_000 + n - 1,
                4_000_000,
                4_000_001,
                3_000_000 + 2 * n - 2,
                3_000_000 + 2 * n - 1,
                4_000_000 + n,
                4_000_001 + n,
            ]
        );
    }
}
