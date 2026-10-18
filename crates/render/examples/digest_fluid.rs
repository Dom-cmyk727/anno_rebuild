//! Reference digests for the C++ port of the wave simulation (ADR 0001):
//! FNV-1a 64 of the swell, of forcing samples and of a `FluidGrid` run over
//! a synthetic coast (no game files), in the order and format asserted by
//! `src/render/tests/fluid_test.cpp`. Floats are hashed as their
//! little-endian bit patterns.
//!
//! ```text
//! cargo run --release -p anno-render --example digest_fluid
//! ```
use anno_render::fluid::{FluidConfig, FluidGrid, FluidWindow, forcing, swell_components};

fn fnv(b: impl IntoIterator<Item = u8>) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for x in b {
        h ^= u64::from(x);
        h = h.wrapping_mul(0x0100_0000_01b3);
    }
    h
}

fn fnv_f32<'a>(v: impl IntoIterator<Item = &'a f32>) -> u64 {
    fnv(v.into_iter().flat_map(|f| f.to_bits().to_le_bytes()))
}

/// Deep water in the west, a beach rising to land in the east, a small
/// island around tile (5, 4).
fn depth(x: f32, y: f32) -> f32 {
    let beach = 16.0 - 2.0 * (x - 2.0).max(0.0) + 0.5 * (y - 8.0).abs();
    let (dx, dy) = (x - 5.0, y - 4.0);
    let island = (dx * dx + dy * dy) * 2.0 - 3.0;
    beach.min(island)
}

/// Depth, then (η, u, v, foam) of every storage cell.
fn grid_digest(g: &FluidGrid) -> u64 {
    fnv_f32(g.depth.iter().chain(g.state.iter().flatten()))
}

/// World cells whose state is printed for tolerance checks.
const CELLS: [(i64, i64); 5] = [(10, 20), (30, 20), (38, 30), (42, 40), (20, 16)];

fn main() {
    let cfg = FluidConfig {
        size: 64,
        cells_per_tile: [4, 4, 4],
        ..FluidConfig::DEFAULT
    };
    let comps = swell_components(&cfg);
    let swell: Vec<f32> = comps
        .iter()
        .flat_map(|c| [c.k[0], c.k[1], c.omega, c.phase, c.amplitude])
        .collect();
    println!("swell {:016x}", fnv_f32(&swell));
    for c in &comps {
        println!(
            "comp {:?} {:?} {:?} {:?} {:?}",
            c.k[0], c.k[1], c.omega, c.phase, c.amplitude
        );
    }
    let mut samples = Vec::new();
    for j in 0..16_u8 {
        for i in 0..16_u8 {
            let (fi, fj) = (f32::from(i), f32::from(j));
            let pos = (fi * 0.75 - 3.0, fj * 1.25 - 5.0);
            let (e, u, v) = forcing(&cfg, &comps, pos, (fi + fj) * 0.37);
            samples.extend([e, u, v]);
        }
    }
    println!("forcing {:016x}", fnv_f32(&samples));
    let window = FluidWindow::new(&cfg, 0, (8.0, 8.0));
    let mut g = FluidGrid::new(cfg, window, depth, 0.0);
    println!("init {:016x}", grid_digest(&g));
    let mut t = 0.0;
    for _ in 0..300 {
        t += cfg.dt;
        g.step(t);
    }
    println!("step300 {:016x}", grid_digest(&g));
    // Scroll the window (ring buffer): 1.5 tiles = 6 cells right, 2 down.
    let new = g.window.recentre((9.5, 8.5), 4);
    for r in &new {
        g.fill(*r, &depth, t);
    }
    println!(
        "recentred {} {} {}",
        g.window.origin.0,
        g.window.origin.1,
        new.len()
    );
    println!("filled {:016x}", grid_digest(&g));
    for _ in 0..300 {
        t += cfg.dt;
        g.step(t);
    }
    println!("step600 {:016x}", grid_digest(&g));
    let n = usize::try_from(g.window.size).expect("positive");
    for (x, y) in CELLS {
        let (sx, sy) = g.window.storage(x, y);
        let [eta, u, v, foam] = g.state[sy * n + sx];
        println!("cell {x} {y} {eta:?} {u:?} {v:?} {foam:?}");
    }
}
