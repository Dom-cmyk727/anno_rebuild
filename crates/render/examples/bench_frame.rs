//! Times one viewer frame's work for a 1920 × 1080 view of a scenario:
//! the static layers (ground, objects) once, the per-frame water layer
//! (original and enhanced) averaged over 10 frames.
//!
//! `cargo run --release -p anno-render --example bench_frame -- <game_dir> <file.sze> [group]`

use std::time::Instant;

use anno_assets::island::GroundTextures;
use anno_formats::anlagen::AnlagenFile;
use anno_formats::insel;
use anno_formats::tex::TexFile;
use anno_render::iso::{SpriteCache, WaterStyle};
use anno_render::scene::{self, IslandBox, Layers, Rect};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (dir, file) = (std::path::Path::new(&args[0]), &args[1]);
    let group: usize = args.get(2).map_or(Ok(1), |g| g.parse())?;
    let buf = std::fs::read(file)?;
    let islands = insel::islands(&buf)?;
    let felder = std::fs::read(dir.join("Textures/Felder.tex"))?;
    let textures = GroundTextures::new(&TexFile::parse(&felder)?)?;
    let anl = std::fs::read(dir.join("Anlagen.dat"))?;
    let anlagen = AnlagenFile::parse(&anl)?;
    let boxes: Vec<IslandBox> = islands.iter().map(IslandBox::of).collect();
    let layout = scene::layout(&boxes, group);
    // View centred on the largest island.
    let big = layout
        .iter()
        .max_by_key(|p| p.bounds.w * p.bounds.h)
        .ok_or("no islands")?;
    let view = Rect {
        x: big.bounds.x + big.bounds.w / 2 - 960,
        y: big.bounds.y + big.bounds.h / 2 - 540,
        w: 1920,
        h: 1080,
    };
    let cache = SpriteCache::default();
    let run = |layers: Layers| {
        let mut c = scene::blank(view);
        scene::render_region(
            &mut c, &islands, &layout, &textures, &anlagen, group, layers, &cache,
        )
    };
    let only = |ground, water, objects, water_style| Layers {
        ground,
        water,
        objects,
        water_style,
    };
    for (name, layers) in [
        ("ground", only(true, None, false, WaterStyle::Original)),
        ("objects", only(false, None, true, WaterStyle::Original)),
    ] {
        let t = Instant::now();
        run(layers);
        println!(
            "{name:>16}: {:7.1} ms (once, cache cold)",
            t.elapsed().as_secs_f64() * 1e3
        );
    }
    for (name, style) in [
        ("water original", WaterStyle::Original),
        ("water enhanced", WaterStyle::Enhanced),
    ] {
        run(only(false, Some(0), false, style)); // warm the sprite cache
        let t = Instant::now();
        for k in 0..10 {
            run(only(false, Some(k * 50), false, style));
        }
        println!(
            "{name:>16}: {:7.1} ms per frame",
            t.elapsed().as_secs_f64() * 1e2
        );
    }
    Ok(())
}
