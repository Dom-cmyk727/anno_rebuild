//! Frame pipeline of the interactive view, independent of any window
//! system: the viewer and the benchmark (`examples/bench_frame.rs`) use the
//! same code, so measured numbers are the viewer's numbers.
//!
//! Static data (ground, objects, the sea's per-pixel [`WaterField`] and
//! whether animated water objects are present) is rendered once per
//! [`CHUNK`]-sized square of scene screen space and cached. A frame then
//! composes, for the visible rectangle and split over row bands on all
//! threads: cached ground → water objects below sea level (rendered at the
//! frame time) → sea surface (per-pixel pattern lookup, [`crate::sea`]) →
//! water objects above sea level → cached objects. [`FrameStats`] reports
//! the time of every stage.
//!
//! Chunks are built either inside the frame (blocking: headless renders) or
//! by background workers ([`spawn_chunk_workers`]): then a chunk that is not
//! ready yet shows open sea for a few frames, and a ring of chunks around
//! the view is built ahead, so panning never waits for chunk rendering.

use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc::{self, Receiver};
use std::sync::{Arc, Condvar, Mutex};
use std::time::Instant;

use crate::iso::{Canvas, TILE, WaterStyle, over};
use crate::scene::{Layers, Rect, Scene};
use crate::sea::{self, SeaTarget};

/// Edge length of a cached chunk in screen pixels.
pub const CHUNK: i32 = 512;

/// Cached static data of one chunk; empty vectors where nothing is drawn.
pub struct Chunk {
    /// Ground, opaque `0RGB`; empty = no island ground (open-water colour).
    pub(crate) ground: Vec<u32>,
    /// Objects except water objects, straight-alpha RGBA; empty = none.
    pub(crate) objects: Vec<[u8; 4]>,
    /// Sea field (see [`sea::WaterField`]); empty = open sea everywhere.
    pub(crate) cover: Vec<u8>,
    pub(crate) shore: Vec<u8>,
    pub(crate) depth: Vec<u8>,
    pub(crate) flags: Vec<u8>,
    /// Some island here has water objects below / above sea level.
    pub(crate) water_below: bool,
    pub(crate) water_above: bool,
}

impl Chunk {
    /// Open sea, nothing else (also the stand-in for a chunk being built).
    const EMPTY: Chunk = Chunk {
        ground: Vec::new(),
        objects: Vec::new(),
        cover: Vec::new(),
        shore: Vec::new(),
        depth: Vec::new(),
        flags: Vec::new(),
        water_below: false,
        water_above: false,
    };
}

/// Key of a cached chunk: `(zoom group, chunk x, chunk y)`.
pub type ChunkKey = (usize, i32, i32);

/// Work shared between the renderer and the chunk builders.
#[derive(Default)]
struct JobQueue {
    /// Chunks to build, most wanted first; rewritten by every frame.
    jobs: VecDeque<ChunkKey>,
    /// Chunks being built right now.
    busy: BTreeSet<ChunkKey>,
    stop: bool,
}

/// Background chunk builders: a priority job list and a result queue.
pub struct ChunkWorkers {
    queue: Arc<(Mutex<JobQueue>, Condvar)>,
    done: Receiver<(ChunkKey, Chunk)>,
}

impl ChunkWorkers {
    /// Replaces the job list with `wanted` (in priority order) minus the
    /// chunks being built; returns whether any work is queued or running.
    fn submit(&self, wanted: impl Iterator<Item = ChunkKey>) -> bool {
        let (lock, ready) = &*self.queue;
        let mut q = lock.lock().expect("no panic");
        let jobs: VecDeque<ChunkKey> = wanted.filter(|k| !q.busy.contains(k)).collect();
        q.jobs = jobs;
        ready.notify_all();
        !q.jobs.is_empty() || !q.busy.is_empty()
    }
}

impl Drop for ChunkWorkers {
    fn drop(&mut self) {
        let (lock, ready) = &*self.queue;
        lock.lock().expect("no panic").stop = true;
        ready.notify_all();
    }
}

/// Starts `threads` chunk builders for `scene` in `scope`. They stop when
/// the returned handle (or the [`FrameRenderer`] holding it) is dropped.
pub fn spawn_chunk_workers<'s, 'e, 'a>(
    scope: &'s std::thread::Scope<'s, 'e>,
    scene: &'e Scene<'a>,
    threads: usize,
) -> ChunkWorkers
where
    'a: 'e,
{
    let queue = Arc::new((Mutex::new(JobQueue::default()), Condvar::new()));
    let (done_tx, done) = mpsc::channel();
    for _ in 0..threads.max(1) {
        let (queue, done_tx) = (Arc::clone(&queue), done_tx.clone());
        scope.spawn(move || {
            let (lock, ready) = &*queue;
            loop {
                let key @ (group, cx, cy) = {
                    let mut q = lock.lock().expect("no panic");
                    loop {
                        if q.stop {
                            return;
                        }
                        if let Some(k) = q.jobs.pop_front() {
                            q.busy.insert(k);
                            break k;
                        }
                        q = ready.wait(q).expect("no panic");
                    }
                };
                let chunk = render_chunk(scene, group, cx, cy);
                lock.lock().expect("no panic").busy.remove(&key);
                if done_tx.send((key, chunk)).is_err() {
                    return;
                }
            }
        });
    }
    ChunkWorkers { queue, done }
}

/// Time spent per stage of one frame, in milliseconds.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct FrameStats {
    /// Chunks rendered for this frame (blocking) or received from the
    /// workers, and the time spent in the frame on it.
    pub chunks_built: usize,
    pub chunks_ms: f64,
    /// Visible chunks still being built (drawn as open sea).
    pub chunks_missing: usize,
    pub ground_ms: f64,
    /// Rendering and compositing the animated water objects.
    pub water_objects_ms: f64,
    pub sea_ms: f64,
    pub objects_ms: f64,
    pub total_ms: f64,
}

/// Renders frames of a [`Scene`] into `0RGB` buffers.
pub struct FrameRenderer {
    cache: BTreeMap<ChunkKey, Chunk>,
    workers: Option<ChunkWorkers>,
    /// Whether the workers had chunks queued or in progress last frame.
    pending: bool,
    /// Worker threads per stage.
    pub threads: usize,
    /// Chunks kept besides the visible ones (others of the current group
    /// first, farthest dropped first).
    pub spare_chunks: usize,
}

impl FrameRenderer {
    pub fn new(threads: usize) -> Self {
        Self {
            cache: BTreeMap::new(),
            workers: None,
            pending: false,
            threads: threads.max(1),
            spare_chunks: 48,
        }
    }

    /// Builds chunks with `workers` from now on (`None`: in the frame).
    pub fn set_workers(&mut self, workers: Option<ChunkWorkers>) {
        self.workers = workers;
        self.pending = false;
    }

    /// Whether chunks are still being built in the background.
    pub fn busy(&self) -> bool {
        self.pending
    }

    /// Number of cached chunks.
    pub fn cached(&self) -> usize {
        self.cache.len()
    }

    /// Brings the chunks of `view` at `group` up to date (built in the frame
    /// or requested from the workers); returns the chunks built or received,
    /// the time spent, and the visible chunks still missing.
    pub fn update_chunks(
        &mut self,
        scene: &Scene<'_>,
        view: Rect,
        group: usize,
    ) -> (usize, f64, usize) {
        let t = Instant::now();
        let chunks = visible_chunks(view);
        let built = if self.workers.is_some() {
            self.request_chunks(group, view, &chunks)
        } else {
            self.fill_cache(scene, group, &chunks)
        };
        let missing = chunks
            .iter()
            .filter(|&&(cx, cy)| !self.cache.contains_key(&(group, cx, cy)))
            .count();
        (built, t.elapsed().as_secs_f64() * 1000.0, missing)
    }

    /// The cached chunk `(group, cx, cy)`, if built.
    #[cfg_attr(not(feature = "gpu"), allow(dead_code))]
    pub(crate) fn chunk(&self, key: ChunkKey) -> Option<&Chunk> {
        self.cache.get(&key)
    }

    /// Draws `view` (scene screen rectangle at `group`) at `time_ms` into
    /// `out` (`view.w × view.h`, row-major).
    pub fn render(
        &mut self,
        scene: &Scene<'_>,
        view: Rect,
        group: usize,
        time_ms: u64,
        style: WaterStyle,
        out: &mut [u32],
    ) -> FrameStats {
        let start = Instant::now();
        let mut stats = FrameStats::default();
        let ms = |t: Instant| t.elapsed().as_secs_f64() * 1000.0;
        let chunks = visible_chunks(view);
        (stats.chunks_built, stats.chunks_ms, stats.chunks_missing) =
            self.update_chunks(scene, view, group);
        let cache = &self.cache;
        let empty = Chunk::EMPTY;
        let get = |cx: i32, cy: i32| cache.get(&(group, cx, cy)).unwrap_or(&empty);
        let water = rgb(scene.textures.water());

        // Ground.
        let t = Instant::now();
        par_bands(out, view, self.threads, |band, rows| {
            for &(cx, cy) in &chunks {
                let Some(r) = chunk_rect(cx, cy).intersect(rows) else {
                    continue;
                };
                let c = get(cx, cy);
                for y in r.y..r.y + r.h {
                    let d = index(rows, r.x, y);
                    let dst = &mut band[d..d + to_usize(r.w)];
                    if c.ground.is_empty() {
                        dst.fill(water);
                    } else {
                        let s = chunk_index(cx, cy, r.x, y);
                        dst.copy_from_slice(&c.ground[s..s + to_usize(r.w)]);
                    }
                }
            }
        });
        stats.ground_ms = ms(t);

        // Water objects at the frame time, rendered per visible chunk part.
        let t = Instant::now();
        let render_parts = |below: bool| -> Vec<Canvas> {
            let parts: Vec<Rect> = chunks
                .iter()
                .filter(|&&(cx, cy)| {
                    let c = get(cx, cy);
                    if below { c.water_below } else { c.water_above }
                })
                .filter_map(|&(cx, cy)| chunk_rect(cx, cy).intersect(view))
                .collect();
            let layers = Layers {
                water_below: below,
                water_above: !below,
                time_ms,
                water_style: style,
                ..Layers::NONE
            };
            par_map(&parts, self.threads, |&r| scene.render(r, group, layers).0)
        };
        let below = render_parts(true);
        composite_all(out, view, &below, self.threads);
        stats.water_objects_ms = ms(t);

        // Sea surface.
        let t = Instant::now();
        if let Some(pattern) = scene.sea(group) {
            let lift = scene.lift;
            par_bands(out, view, self.threads, |band, rows| {
                for &(cx, cy) in &chunks {
                    let Some(r) = chunk_rect(cx, cy).intersect(rows) else {
                        continue;
                    };
                    let c = get(cx, cy);
                    let d = index(rows, r.x, r.y);
                    let field = |i: usize| {
                        if c.cover.is_empty() {
                            return (255, 255, 255, 1);
                        }
                        let (x, y) = (i % to_usize(r.w), i / to_usize(r.w));
                        let s = chunk_index(cx, cy, r.x + to_i32(x), r.y + to_i32(y));
                        (c.cover[s], c.shore[s], c.depth[s], c.flags[s])
                    };
                    sea::shade_sea(
                        &mut band[d..],
                        to_usize(view.w),
                        r,
                        field,
                        pattern,
                        time_ms,
                        style,
                        group,
                        lift,
                    );
                }
            });
        }
        stats.sea_ms = ms(t);

        let t = Instant::now();
        let above = render_parts(false);
        composite_all(out, view, &above, self.threads);
        stats.water_objects_ms += ms(t);

        // Cached objects.
        let t = Instant::now();
        par_bands(out, view, self.threads, |band, rows| {
            for &(cx, cy) in &chunks {
                let c = get(cx, cy);
                if c.objects.is_empty() {
                    continue;
                }
                let Some(r) = chunk_rect(cx, cy).intersect(rows) else {
                    continue;
                };
                for y in r.y..r.y + r.h {
                    let d = index(rows, r.x, y);
                    let s = chunk_index(cx, cy, r.x, y);
                    let w = to_usize(r.w);
                    for (dst, src) in band[d..d + w].iter_mut().zip(&c.objects[s..s + w]) {
                        over_rgb(dst, *src);
                    }
                }
            }
        });
        stats.objects_ms = ms(t);
        stats.total_ms = ms(start);
        stats
    }

    /// Collects finished chunks, requests the missing visible chunks and
    /// then a ring of one chunk around the view, trims the cache, and
    /// returns the number of chunks received.
    fn request_chunks(&mut self, group: usize, view: Rect, visible: &[(i32, i32)]) -> usize {
        let Some(w) = &self.workers else {
            return 0;
        };
        let mut received = 0;
        while let Ok((key, chunk)) = w.done.try_recv() {
            self.cache.insert(key, chunk);
            received += 1;
        }
        let ring = visible_chunks(Rect {
            x: view.x - CHUNK,
            y: view.y - CHUNK,
            w: view.w + 2 * CHUNK,
            h: view.h + 2 * CHUNK,
        });
        let mut keep: BTreeSet<ChunkKey> = visible
            .iter()
            .chain(&ring)
            .map(|&(cx, cy)| (group, cx, cy))
            .collect();
        // Once the current zoom group is (nearly) done, prepare the same
        // area in the neighbouring zoom groups, so zooming shows islands at
        // once.
        let missing = |k: &(i32, i32)| !self.cache.contains_key(&(group, k.0, k.1));
        if visible.iter().chain(&ring).filter(|k| missing(k)).count() <= self.threads {
            for g in [group.checked_sub(1), Some(group + 1)]
                .into_iter()
                .flatten()
            {
                if g < TILE.len() {
                    let other = zoomed_view(view, group, g);
                    keep.extend(
                        visible_chunks(other)
                            .into_iter()
                            .map(|(cx, cy)| (g, cx, cy)),
                    );
                }
            }
        }
        // Visible chunks first, then the ring, then other zoom groups.
        let mut order: Vec<ChunkKey> = keep.iter().copied().collect();
        order.sort_by_key(|&(g, cx, cy)| (g != group, !visible.contains(&(cx, cy))));
        self.pending = w.submit(order.into_iter().filter(|k| !self.cache.contains_key(k)));
        self.trim(group, view, &keep);
        received
    }

    /// Drops cached chunks outside `keep` beyond `spare_chunks`: other zoom
    /// groups first, then those farthest from `view`.
    fn trim(&mut self, group: usize, view: Rect, keep: &BTreeSet<ChunkKey>) {
        if self.cache.len() <= keep.len() + self.spare_chunks {
            return;
        }
        let centre = (
            (view.x + view.w / 2).div_euclid(CHUNK),
            (view.y + view.h / 2).div_euclid(CHUNK),
        );
        let mut keys: Vec<(bool, i32, ChunkKey)> = self
            .cache
            .keys()
            .filter(|k| !keep.contains(k))
            .map(|&k @ (g, cx, cy)| {
                let dist = (cx - centre.0).abs() + (cy - centre.1).abs();
                (g != group, dist, k)
            })
            .collect();
        keys.sort_unstable();
        for (_, _, k) in keys.into_iter().skip(self.spare_chunks) {
            self.cache.remove(&k);
        }
    }

    /// Renders the missing chunks of `wanted` in parallel, trims the cache,
    /// and returns the number rendered.
    fn fill_cache(&mut self, scene: &Scene<'_>, group: usize, wanted: &[(i32, i32)]) -> usize {
        let todo: Vec<(i32, i32)> = wanted
            .iter()
            .copied()
            .filter(|&(cx, cy)| !self.cache.contains_key(&(group, cx, cy)))
            .collect();
        let built = par_map(&todo, self.threads, |&(cx, cy)| {
            ((group, cx, cy), render_chunk(scene, group, cx, cy))
        });
        self.cache.extend(built);
        let keep = wanted.iter().map(|&(cx, cy)| (group, cx, cy)).collect();
        let view = wanted
            .first()
            .map_or(chunk_rect(0, 0), |&(cx, cy)| chunk_rect(cx, cy));
        self.trim(group, view, &keep);
        todo.len()
    }
}

/// The rectangle of `view`'s size at zoom group `to` centred on the scene
/// point that `view` centres on at group `from` (screen coordinates are
/// linear in tile coordinates per group).
pub fn zoomed_view(view: Rect, from: usize, to: usize) -> Rect {
    let (cx, cy) = (
        i64::from(view.x) + i64::from(view.w) / 2,
        i64::from(view.y) + i64::from(view.h) / 2,
    );
    let scale =
        |v: i64, a: i32, b: i32| i32::try_from(v * i64::from(b) / i64::from(a)).expect("fits");
    let (x, y) = (
        scale(cx, TILE[from].0, TILE[to].0),
        scale(cy, TILE[from].1, TILE[to].1),
    );
    Rect {
        x: x - view.w / 2,
        y: y - view.h / 2,
        ..view
    }
}

fn to_usize(v: i32) -> usize {
    usize::try_from(v).expect("non-negative")
}

fn to_i32(v: usize) -> i32 {
    i32::try_from(v).expect("fits")
}

/// Index of scene pixel `(x, y)` in a buffer covering `rect`.
fn index(rect: Rect, x: i32, y: i32) -> usize {
    to_usize(y - rect.y) * to_usize(rect.w) + to_usize(x - rect.x)
}

/// Index of scene pixel `(x, y)` in chunk `(cx, cy)`.
fn chunk_index(cx: i32, cy: i32, x: i32, y: i32) -> usize {
    index(chunk_rect(cx, cy), x, y)
}

const fn rgb(c: [u8; 4]) -> u32 {
    (c[0] as u32) << 16 | (c[1] as u32) << 8 | c[2] as u32
}

/// `src` blended over an opaque `0RGB` pixel.
fn over_rgb(dst: &mut u32, src: [u8; 4]) {
    match src[3] {
        0 => {}
        255 => *dst = rgb(src),
        _ => {
            let mut d = dst.rgba();
            over(&mut d, &src);
            *dst = rgb(d);
        }
    }
}

/// Chunks `(cx, cy)` that overlap `view`, row by row.
pub fn visible_chunks(view: Rect) -> Vec<(i32, i32)> {
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

/// Scene rectangle of chunk `(cx, cy)`.
pub fn chunk_rect(cx: i32, cy: i32) -> Rect {
    Rect {
        x: cx * CHUNK,
        y: cy * CHUNK,
        w: CHUNK,
        h: CHUNK,
    }
}

/// Splits `out` (covering `view`) into row bands and runs `f(band, rect)`
/// on `threads` threads; `rect` is the scene rectangle of the band.
fn par_bands(out: &mut [u32], view: Rect, threads: usize, f: impl Fn(&mut [u32], Rect) + Sync) {
    if view.w <= 0 || view.h <= 0 {
        return;
    }
    // A few bands per thread balance uneven rows (sea vs. land).
    let bands = (threads * 4).min(to_usize(view.h));
    let rows = to_usize(view.h).div_ceil(bands);
    let width = to_usize(view.w);
    let next = AtomicUsize::new(0);
    let slices: Vec<Mutex<Option<&mut [u32]>>> = out
        .chunks_mut(rows * width)
        .map(|s| Mutex::new(Some(s)))
        .collect();
    let work = || {
        loop {
            let i = next.fetch_add(1, Ordering::Relaxed);
            let Some(slot) = slices.get(i) else {
                break;
            };
            let band = slot.lock().expect("no panic").take().expect("taken once");
            let y = view.y + to_i32(i * rows);
            let h = to_i32(band.len() / width);
            f(band, Rect { y, h, ..view });
        }
    };
    if threads == 1 {
        work();
        return;
    }
    std::thread::scope(|s| {
        for _ in 0..threads {
            s.spawn(work);
        }
    });
}

/// `f` over `items` on up to `threads` threads, results in item order.
fn par_map<T: Sync, R: Send>(items: &[T], threads: usize, f: impl Fn(&T) -> R + Sync) -> Vec<R> {
    if items.is_empty() {
        return Vec::new();
    }
    let next = AtomicUsize::new(0);
    let done = Mutex::new(Vec::with_capacity(items.len()));
    let work = || {
        loop {
            let i = next.fetch_add(1, Ordering::Relaxed);
            let Some(item) = items.get(i) else {
                break;
            };
            let r = f(item);
            done.lock().expect("no panic").push((i, r));
        }
    };
    let n = threads.min(items.len());
    if n == 1 {
        work();
    } else {
        std::thread::scope(|s| {
            for _ in 0..n {
                s.spawn(work);
            }
        });
    }
    let mut done = done.into_inner().expect("no panic");
    done.sort_by_key(|(i, _)| *i);
    done.into_iter().map(|(_, r)| r).collect()
}

/// Composites transparent canvases (scene coordinates) over `out`.
fn composite_all(out: &mut [u32], view: Rect, canvases: &[Canvas], threads: usize) {
    if canvases.is_empty() {
        return;
    }
    par_bands(out, view, threads, |band, rows| {
        for c in canvases {
            let rect = Rect {
                x: c.origin.0,
                y: c.origin.1,
                w: to_i32(c.width),
                h: to_i32(c.height),
            };
            let Some(r) = rect.intersect(rows) else {
                continue;
            };
            let px = c.pixels.as_chunks::<4>().0;
            for y in r.y..r.y + r.h {
                let d = index(rows, r.x, y);
                let s = index(rect, r.x, y);
                let w = to_usize(r.w);
                for (dst, src) in band[d..d + w].iter_mut().zip(&px[s..s + w]) {
                    over_rgb(dst, *src);
                }
            }
        }
    });
}

/// The static data of one chunk.
fn render_chunk(scene: &Scene<'_>, group: usize, cx: i32, cy: i32) -> Chunk {
    let rect = chunk_rect(cx, cy);
    let islands: Vec<usize> = scene.layouts[group]
        .iter()
        .filter(|p| p.bounds.intersect(rect).is_some())
        .map(|p| p.index)
        .collect();
    if islands.is_empty() {
        return Chunk::EMPTY;
    }
    let only = |l: Layers| scene.render(rect, group, l).0;
    let water = scene.textures.water();
    let (ground, probe) = scene.ground_with_probe(rect, group);
    let ground = ground
        .pixels
        .as_chunks::<4>()
        .0
        .iter()
        .map(|p| {
            let mut px = water;
            over(&mut px, p);
            rgb(px)
        })
        .collect();
    let objects = only(Layers {
        objects: true,
        ..Layers::NONE
    });
    let objects = if objects.pixels.as_chunks::<4>().0.iter().all(|p| p[3] == 0) {
        Vec::new()
    } else {
        objects.pixels.as_chunks::<4>().0.to_vec()
    };
    let field = scene.water_field_from(rect, &probe);
    let (water_below, water_above) = islands
        .iter()
        .map(|&i| scene.has_water_objects(i))
        .fold((false, false), |(a, b), (c, d)| (a || c, b || d));
    Chunk {
        ground,
        objects,
        cover: field.cover,
        shore: field.shore,
        depth: field.depth,
        flags: field.flags,
        water_below,
        water_above,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chunks_and_bands() {
        let view = Rect {
            x: -2,
            y: -1,
            w: 4,
            h: 3,
        };
        assert_eq!(visible_chunks(view), [(-1, -1), (0, -1), (-1, 0), (0, 0)]);
        // Every band gets its own rows; each pixel is written once.
        let mut out = vec![0u32; 12];
        par_bands(&mut out, view, 3, |band, rows| {
            for y in rows.y..rows.y + rows.h {
                for x in rows.x..rows.x + rows.w {
                    let i = index(rows, x, y);
                    band[i] += u32::try_from((y + 1) * 10 + (x + 2)).unwrap();
                }
            }
        });
        assert_eq!(out, [0, 1, 2, 3, 10, 11, 12, 13, 20, 21, 22, 23]);
        let squares = par_map(&[1, 2, 3, 4, 5], 3, |v| v * v);
        assert_eq!(squares, [1, 4, 9, 16, 25]);
    }
}
