//! Evidence for `iso::WATERLINE`: at which terrain height do the original's
//! painted ground textures change from sand to water?
//!
//! ```text
//! cargo run --release -p anno-render --example waterline_evidence -- <game_dir> <island files...>
//! ```
//!
//! Renders the ground of every island flat (no elevation, zoom group 2),
//! maps each pixel back to its tile position, interpolates the `HEIGHT`
//! there (bilinear) and classifies the painted colour by blue − red: sand
//! (< −40), transition, water (> 40). Prints, per height, the counts and
//! the share of water + transition pixels. Result 2026-10-17 over 12
//! `*_0B.scp` templates: ≥ 0.53 at every height ≤ 16, ≤ 0.08 from 17 on.
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_precision_loss,
    clippy::cast_sign_loss
)]
use anno_assets::island::GroundTextures;
use anno_formats::{anlagen::AnlagenFile, insel, tex::TexFile};
use anno_render::{
    iso,
    scene::{Layers, Scene},
};
fn main() {
    let a: Vec<String> = std::env::args().collect();
    let dir = std::path::Path::new(&a[1]);
    let felder = std::fs::read(dir.join("Textures/Felder.tex")).unwrap();
    let anl = std::fs::read(dir.join("Anlagen.dat")).unwrap();
    let mut hist = vec![[0u64; 3]; 64]; // per height: [sand, transition, water]
    for f in &a[2..] {
        let fb = std::fs::read(f).unwrap();
        let anlagen = AnlagenFile::parse(&anl).unwrap();
        let t = GroundTextures::new(&TexFile::parse(&felder).unwrap()).unwrap();
        let islands = insel::islands(&fb).unwrap();
        let scene = Scene::new(String::new(), islands, t, anlagen, iso::Lift::NONE);
        // Flat projection (no lift): screen pixel <-> tile position is exact.
        for g in [2usize] {
            for p in &scene.layouts[g] {
                let r = p.bounds;
                let ground = scene
                    .render(
                        r,
                        g,
                        Layers {
                            ground: true,
                            ..Layers::NONE
                        },
                    )
                    .0;
                let isl = &scene.islands[p.index];
                let (w, h) = (usize::from(isl.width), usize::from(isl.height));
                let heights = isl.heights().unwrap();
                let (tw, th) = iso::TILE[g];
                for (k, px) in ground.pixels.chunks(4).enumerate() {
                    if px[3] == 0 {
                        continue;
                    }
                    let sx = r.x + (k % ground.width) as i32 - p.offset.0;
                    let sy = r.y + (k / ground.width) as i32 - p.offset.1;
                    // tile coords: x = sx/tw + sy/th, y = sy/th - sx/tw
                    let fx = sx as f32 / tw as f32 + sy as f32 / th as f32;
                    let fy = sy as f32 / th as f32 - sx as f32 / tw as f32;
                    if fx < 0.0 || fy < 0.0 || fx >= w as f32 - 1.0 || fy >= h as f32 - 1.0 {
                        continue;
                    }
                    let (x0, y0) = (fx as usize, fy as usize);
                    let (ux, uy) = (fx - x0 as f32, fy - y0 as f32);
                    let hv = |x: usize, y: usize| f32::from(heights[y * w + x]);
                    let z = hv(x0, y0) * (1.0 - ux) * (1.0 - uy)
                        + hv(x0 + 1, y0) * ux * (1.0 - uy)
                        + hv(x0, y0 + 1) * (1.0 - ux) * uy
                        + hv(x0 + 1, y0 + 1) * ux * uy;
                    let zb = (z as usize).min(63);
                    let br = i32::from(px[2]) - i32::from(px[0]);
                    let c = if br < -40 {
                        0
                    } else if br < 40 {
                        1
                    } else {
                        2
                    };
                    hist[zb][c] += 1;
                }
            }
        }
    }
    println!("height  sand  transition  water   (share water+transition)");
    for (z, c) in hist.iter().enumerate() {
        let n = c.iter().sum::<u64>();
        if n == 0 {
            continue;
        }
        println!(
            "{z:6} {:7} {:7} {:7}   {:.2}",
            c[0],
            c[1],
            c[2],
            (c[1] + c[2]) as f64 / n as f64
        );
    }
}
