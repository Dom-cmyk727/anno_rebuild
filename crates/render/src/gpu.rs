//! GPU frame compositor (feature `gpu`, wgpu): composes the same frame as
//! [`crate::frame::FrameRenderer::render`] on the graphics card.
//!
//! The static data stays on the CPU side and is shared: chunks (ground,
//! objects, sea field) are built by [`FrameRenderer`] (in the frame or by
//! its background workers) and uploaded once as textures; the sea pattern of
//! a zoom group is uploaded as two texture arrays (one layer per animation
//! step). Per frame only the animated parts run, in two render passes:
//!
//! 1. into an offscreen texture: cached ground, water objects below sea
//!    level (sprites from an atlas);
//! 2. into the target: the sea per pixel (`sea_fs` in `gpu.wgsl`, the rules
//!    of `sea::shade_sea`, reading pass 1), water objects above sea level,
//!    cached objects.
//!
//! Positions are integer scene pixels and textures are read with
//! `textureLoad`, so the result matches the CPU path up to blending
//! rounding. Shader and rules: `gpu.wgsl`; spec: `docs/render/iso.md`.

use std::borrow::Cow;
use std::collections::{BTreeMap, BTreeSet};
use std::time::Instant;

use wgpu::{
    BindGroup, BindGroupLayout, BindingResource, BindingType, BlendState, Buffer, BufferUsages,
    Color, ColorTargetState, ColorWrites, Device, Extent3d, LoadOp, Operations, Origin3d, Queue,
    RenderPipeline, ShaderStages, StoreOp, TexelCopyBufferLayout, TexelCopyTextureInfo, Texture,
    TextureAspect, TextureFormat, TextureSampleType, TextureUsages, TextureView,
    TextureViewDimension,
};

/// The wgpu version this module is built against (for callers' types).
pub use wgpu;

use crate::frame::{CHUNK, Chunk, ChunkKey, FrameRenderer, NO_TERRAIN, chunk_rect, visible_chunks};
use crate::iso::{SEA_LEVEL, SpriteDraw, SpriteKey, TILE, WaterStyle};
use crate::scene::{Rect, Scene, SpriteLayer};

/// Edge length of an atlas layer for sprite frames.
const ATLAS_SIZE: u32 = 2048;
/// Chunks uploaded per frame at most; the rest show open sea until a later
/// frame, so zooming never stalls on uploads.
const UPLOADS_PER_FRAME: usize = 6;
/// Animation steps of other zoom groups' sea patterns uploaded per frame
/// (each 2 × up to 1344 × 672 RGBA at group 0, ≈ 7 MB), so that a zoom
/// change never uploads a whole pattern at once.
const SEA_STEPS_PER_FRAME: usize = 2;
/// Format of the terrain-height textures.
const TERRAIN_FORMAT: TextureFormat = TextureFormat::R16Uint;
/// Bytes of one sprite instance (see `sprite_vs`).
const INSTANCE_BYTES: u64 = 32;

/// Time spent per stage of one GPU frame on the CPU, in milliseconds.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct GpuStats {
    pub chunks_built: usize,
    pub chunks_ms: f64,
    pub chunks_missing: usize,
    /// Chunks, sea pattern and atlas uploads.
    pub upload_ms: f64,
    /// Placing and packing the animated sprites.
    pub sprites_ms: f64,
    /// Recording and submitting the commands.
    pub encode_ms: f64,
    pub total_ms: f64,
}

/// A chunk on the GPU; `real` = false for a stand-in (open sea) while the
/// chunk is being built.
struct GpuChunk {
    bind: BindGroup,
    real: bool,
    _textures: Vec<Texture>,
    _uniform: Buffer,
}

/// The sea pattern of one zoom group.
struct GpuSea {
    size: (i32, i32),
    steps: usize,
    /// Animation steps uploaded so far (all = usable).
    uploaded: usize,
    step_ms: u64,
    sprite: TextureView,
    filled: TextureView,
    _textures: [Texture; 2],
}

/// Sprite frames packed on demand into a growing texture array (shelf
/// packing per layer).
struct Atlas {
    texture: Texture,
    bind: BindGroup,
    layers: u32,
    /// Where each frame lives: `(layer, x, y)`; `None` = cannot be drawn.
    place: BTreeMap<SpriteKey, Option<(u32, u32, u32)>>,
    /// Packing cursor: layer, x, y, height of the current shelf.
    cursor: (u32, u32, u32, u32),
}

/// The wgpu state of the compositor.
pub struct GpuFrame {
    pub device: Device,
    pub queue: Queue,
    /// Format of the target and of the offscreen texture (not sRGB).
    format: TextureFormat,
    frame_layout: BindGroupLayout,
    chunk_layout: BindGroupLayout,
    atlas_layout: BindGroupLayout,
    ground_pipe: RenderPipeline,
    /// Writes the terrain height seen at each pixel (river depth test).
    terrain_pipe: RenderPipeline,
    objects_pipe: RenderPipeline,
    sea_pipe: RenderPipeline,
    sprite_pipe: RenderPipeline,
    frame_uniform: Buffer,
    /// Offscreen texture of pass 1 and its size.
    under: Option<(Texture, TextureView, (u32, u32))>,
    /// Terrain height per view pixel (`frame::TERRAIN_SCALE`, `NO_TERRAIN`).
    terrain: Option<(Texture, TextureView)>,
    dummy_terrain: TextureView,
    /// Sea patterns uploaded so far, by zoom group (kept, so that zooming
    /// back does not upload them again), and the one in use.
    seas: BTreeMap<usize, GpuSea>,
    sea_group: Option<usize>,
    chunks: BTreeMap<ChunkKey, GpuChunk>,
    /// 1 × 1 stand-ins for unused bindings.
    dummy_color: TextureView,
    dummy_field: TextureView,
    dummy_array: TextureView,
    atlas: Option<Atlas>,
    instances: Option<Buffer>,
}

fn texture_entry(
    binding: u32,
    sample: TextureSampleType,
    dim: TextureViewDimension,
) -> wgpu::BindGroupLayoutEntry {
    wgpu::BindGroupLayoutEntry {
        binding,
        visibility: ShaderStages::FRAGMENT,
        ty: BindingType::Texture {
            sample_type: sample,
            view_dimension: dim,
            multisampled: false,
        },
        count: None,
    }
}

fn uniform_entry(binding: u32) -> wgpu::BindGroupLayoutEntry {
    wgpu::BindGroupLayoutEntry {
        binding,
        visibility: ShaderStages::VERTEX | ShaderStages::FRAGMENT,
        ty: BindingType::Buffer {
            ty: wgpu::BufferBindingType::Uniform,
            has_dynamic_offset: false,
            min_binding_size: None,
        },
        count: None,
    }
}

const FLOAT: TextureSampleType = TextureSampleType::Float { filterable: false };

/// A 2D texture (`layers` > 1 or `array`: an array) filled from `data`.
#[allow(clippy::too_many_arguments)]
fn upload_texture(
    device: &Device,
    queue: &Queue,
    format: TextureFormat,
    (w, h): (u32, u32),
    bytes_per_texel: u32,
    layers: &[&[u8]],
    array: bool,
) -> (Texture, TextureView) {
    let layer_count = u32::try_from(layers.len()).expect("fits").max(1);
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: None,
        size: Extent3d {
            width: w,
            height: h,
            depth_or_array_layers: layer_count,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST | TextureUsages::COPY_SRC,
        view_formats: &[],
    });
    for (z, data) in layers.iter().enumerate() {
        queue.write_texture(
            TexelCopyTextureInfo {
                texture: &texture,
                mip_level: 0,
                origin: Origin3d {
                    x: 0,
                    y: 0,
                    z: u32::try_from(z).expect("fits"),
                },
                aspect: TextureAspect::All,
            },
            data,
            TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(w * bytes_per_texel),
                rows_per_image: Some(h),
            },
            Extent3d {
                width: w,
                height: h,
                depth_or_array_layers: 1,
            },
        );
    }
    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        dimension: Some(if array {
            TextureViewDimension::D2Array
        } else {
            TextureViewDimension::D2
        }),
        ..Default::default()
    });
    (texture, view)
}

fn rgba_bytes(px: &[[u8; 4]]) -> Vec<u8> {
    px.iter().flatten().copied().collect()
}

impl GpuFrame {
    /// Creates the pipelines for targets of `format` (an sRGB format is
    /// used through its non-sRGB view, see [`GpuFrame::view_format`]).
    pub fn new(device: Device, queue: Queue, format: TextureFormat) -> Self {
        let format = format.remove_srgb_suffix();
        let module = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gpu.wgsl"),
            source: wgpu::ShaderSource::Wgsl(Cow::Borrowed(include_str!("gpu.wgsl"))),
        });
        let frame_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("frame"),
            entries: &[
                uniform_entry(0),
                texture_entry(1, FLOAT, TextureViewDimension::D2),
                texture_entry(2, FLOAT, TextureViewDimension::D2Array),
                texture_entry(3, FLOAT, TextureViewDimension::D2Array),
                texture_entry(4, TextureSampleType::Uint, TextureViewDimension::D2),
            ],
        });
        let chunk_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("chunk"),
            entries: &[
                uniform_entry(0),
                texture_entry(1, FLOAT, TextureViewDimension::D2),
                texture_entry(2, FLOAT, TextureViewDimension::D2),
                texture_entry(3, TextureSampleType::Uint, TextureViewDimension::D2),
                texture_entry(4, TextureSampleType::Uint, TextureViewDimension::D2),
            ],
        });
        let atlas_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("atlas"),
            entries: &[texture_entry(0, FLOAT, TextureViewDimension::D2Array)],
        });
        let chunk_pl = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("chunk"),
            bind_group_layouts: &[Some(&frame_layout), Some(&chunk_layout)],
            immediate_size: 0,
        });
        let sprite_pl = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("sprite"),
            bind_group_layouts: &[Some(&frame_layout), None, Some(&atlas_layout)],
            immediate_size: 0,
        });
        let target = |blend: Option<BlendState>| {
            [Some(ColorTargetState {
                format,
                blend,
                write_mask: ColorWrites::ALL,
            })]
        };
        let opaque = target(None);
        let alpha = target(Some(BlendState::ALPHA_BLENDING));
        let pipeline = |label: &str,
                        layout: &wgpu::PipelineLayout,
                        vs: &str,
                        fs: &str,
                        buffers: &[Option<wgpu::VertexBufferLayout<'_>>],
                        targets: &[Option<ColorTargetState>]| {
            device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some(label),
                layout: Some(layout),
                vertex: wgpu::VertexState {
                    module: &module,
                    entry_point: Some(vs),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    buffers,
                },
                primitive: wgpu::PrimitiveState::default(),
                depth_stencil: None,
                multisample: wgpu::MultisampleState::default(),
                fragment: Some(wgpu::FragmentState {
                    module: &module,
                    entry_point: Some(fs),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    targets,
                }),
                multiview_mask: None,
                cache: None,
            })
        };
        let instance_attrs = [
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Sint32x2,
                offset: 0,
                shader_location: 0,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32x2,
                offset: 8,
                shader_location: 1,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32x2,
                offset: 16,
                shader_location: 2,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32x2,
                offset: 24,
                shader_location: 3,
            },
        ];
        let instances = [Some(wgpu::VertexBufferLayout {
            array_stride: INSTANCE_BYTES,
            step_mode: wgpu::VertexStepMode::Instance,
            attributes: &instance_attrs,
        })];
        let ground_pipe = pipeline("ground", &chunk_pl, "chunk_vs", "ground_fs", &[], &opaque);
        let terrain_target = [Some(ColorTargetState {
            format: TERRAIN_FORMAT,
            blend: None,
            write_mask: ColorWrites::ALL,
        })];
        let terrain_pipe = pipeline(
            "terrain",
            &chunk_pl,
            "chunk_vs",
            "terrain_fs",
            &[],
            &terrain_target,
        );
        let sea_pipe = pipeline("sea", &chunk_pl, "chunk_vs", "sea_fs", &[], &opaque);
        let objects_pipe = pipeline("objects", &chunk_pl, "chunk_vs", "objects_fs", &[], &alpha);
        let sprite_pipe = pipeline(
            "sprite",
            &sprite_pl,
            "sprite_vs",
            "sprite_fs",
            &instances,
            &alpha,
        );
        let frame_uniform = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("frame"),
            size: 64,
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let one = (1, 1);
        let dummy_color = upload_texture(
            &device,
            &queue,
            TextureFormat::Rgba8Unorm,
            one,
            4,
            &[&[0; 4]],
            false,
        )
        .1;
        let dummy_field = upload_texture(
            &device,
            &queue,
            TextureFormat::Rgba8Uint,
            one,
            4,
            &[&[255, 255, 255, 1]],
            false,
        )
        .1;
        let none = NO_TERRAIN.to_le_bytes();
        let dummy_terrain =
            upload_texture(&device, &queue, TERRAIN_FORMAT, one, 2, &[&none], false).1;
        let dummy_array = upload_texture(
            &device,
            &queue,
            TextureFormat::Rgba8Unorm,
            one,
            4,
            &[&[0; 4]],
            true,
        )
        .1;
        Self {
            device,
            queue,
            format,
            frame_layout,
            chunk_layout,
            atlas_layout,
            ground_pipe,
            terrain_pipe,
            objects_pipe,
            sea_pipe,
            sprite_pipe,
            frame_uniform,
            under: None,
            terrain: None,
            dummy_terrain,
            seas: BTreeMap::new(),
            sea_group: None,
            chunks: BTreeMap::new(),
            dummy_color,
            dummy_field,
            dummy_array,
            atlas: None,
            instances: None,
        }
    }

    /// The (non-sRGB) format targets must be viewed in.
    pub fn view_format(&self) -> TextureFormat {
        self.format
    }

    /// Composes `view` (scene rectangle at `group`) at `time_ms` into
    /// `target` (`view.w × view.h`, viewed in [`GpuFrame::view_format`]) and
    /// submits the commands.
    #[allow(clippy::too_many_arguments)]
    pub fn render(
        &mut self,
        frames: &mut FrameRenderer,
        scene: &Scene<'_>,
        view: Rect,
        group: usize,
        time_ms: u64,
        style: WaterStyle,
        target: &TextureView,
    ) -> GpuStats {
        let start = Instant::now();
        let ms = |t: Instant| t.elapsed().as_secs_f64() * 1000.0;
        let mut stats = GpuStats::default();
        (stats.chunks_built, stats.chunks_ms, stats.chunks_missing) =
            frames.update_chunks(scene, view, group);
        let size = (
            u32::try_from(view.w.max(1)).expect("positive"),
            u32::try_from(view.h.max(1)).expect("positive"),
        );

        let t = Instant::now();
        self.ensure_under(size);
        self.ensure_sea(scene, group);
        let visible = visible_chunks(view);
        self.sync_chunks(frames, group, &visible);
        stats.upload_ms = ms(t);

        let t = Instant::now();
        let below = scene.sprites(view, group, time_ms, SpriteLayer::WaterBelow, style);
        let above = scene.sprites(view, group, time_ms, SpriteLayer::WaterAbove, style);
        let mut bytes = Vec::with_capacity((below.len() + above.len()) * 32);
        let n_below = self.pack_sprites(scene, &below, false, &mut bytes);
        let n_above = self.pack_sprites(scene, &above, true, &mut bytes);
        self.write_instances(&bytes);
        stats.sprites_ms = ms(t);

        let t = Instant::now();
        self.write_frame_uniform(view, group, time_ms, style, scene);
        let frame_pass1 = self.frame_bind(None, None);
        let frame_pass2 = self.frame_bind(
            self.under.as_ref().map(|u| &u.1),
            self.terrain.as_ref().map(|t| &t.1),
        );
        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("frame"),
            });
        let [r, g, b, _] = scene.textures.water();
        let clear = Color {
            r: f64::from(r) / 255.0,
            g: f64::from(g) / 255.0,
            b: f64::from(b) / 255.0,
            a: 1.0,
        };
        let chunk_binds: Vec<&BindGroup> = visible
            .iter()
            .filter_map(|&(cx, cy)| self.chunks.get(&(group, cx, cy)).map(|c| &c.bind))
            .collect();
        let instances = self.instances.as_ref();
        let atlas = self.atlas.as_ref().map(|a| &a.bind);
        {
            let terrain = &self.terrain.as_ref().expect("ensured").1;
            let none = Color {
                r: f64::from(NO_TERRAIN),
                g: 0.0,
                b: 0.0,
                a: 0.0,
            };
            let mut pass = begin(&mut encoder, terrain, LoadOp::Clear(none));
            pass.set_bind_group(0, &frame_pass1, &[]);
            pass.set_pipeline(&self.terrain_pipe);
            for bind in &chunk_binds {
                pass.set_bind_group(1, *bind, &[]);
                pass.draw(0..6, 0..1);
            }
        }
        {
            let under = &self.under.as_ref().expect("ensured").1;
            let mut pass = begin(&mut encoder, under, LoadOp::Clear(clear));
            pass.set_bind_group(0, &frame_pass1, &[]);
            pass.set_pipeline(&self.ground_pipe);
            for bind in &chunk_binds {
                pass.set_bind_group(1, *bind, &[]);
                pass.draw(0..6, 0..1);
            }
            if let (Some(buf), Some(atlas), true) = (instances, atlas, n_below > 0) {
                pass.set_pipeline(&self.sprite_pipe);
                pass.set_bind_group(2, atlas, &[]);
                pass.set_vertex_buffer(0, buf.slice(..));
                pass.draw(0..6, 0..n_below);
            }
        }
        {
            let mut pass = begin(&mut encoder, target, LoadOp::Clear(clear));
            pass.set_bind_group(0, &frame_pass2, &[]);
            pass.set_pipeline(&self.sea_pipe);
            for bind in &chunk_binds {
                pass.set_bind_group(1, *bind, &[]);
                pass.draw(0..6, 0..1);
            }
            if let (Some(buf), Some(atlas), true) = (instances, atlas, n_above > 0) {
                pass.set_pipeline(&self.sprite_pipe);
                pass.set_bind_group(2, atlas, &[]);
                pass.set_vertex_buffer(0, buf.slice(..));
                pass.draw(0..6, n_below..n_below + n_above);
            }
            pass.set_pipeline(&self.objects_pipe);
            for bind in &chunk_binds {
                pass.set_bind_group(1, *bind, &[]);
                pass.draw(0..6, 0..1);
            }
        }
        self.queue.submit([encoder.finish()]);
        stats.encode_ms = ms(t);
        stats.total_ms = ms(start);
        stats
    }

    /// The sea pattern of the current zoom group.
    fn sea(&self) -> Option<&GpuSea> {
        self.sea_group.and_then(|g| self.seas.get(&g))
    }

    fn ensure_under(&mut self, size: (u32, u32)) {
        if self.under.as_ref().is_some_and(|u| u.2 == size) {
            return;
        }
        let texture = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("under"),
            size: Extent3d {
                width: size.0,
                height: size.1,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: self.format,
            usage: TextureUsages::RENDER_ATTACHMENT | TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
        self.under = Some((texture, view, size));
        let terrain = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("terrain"),
            size: Extent3d {
                width: size.0,
                height: size.1,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: TERRAIN_FORMAT,
            usage: TextureUsages::RENDER_ATTACHMENT | TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let tv = terrain.create_view(&wgpu::TextureViewDescriptor::default());
        self.terrain = Some((terrain, tv));
    }

    /// Makes the sea pattern of `group` usable (uploading what is missing,
    /// blocking) and uploads [`SEA_STEPS_PER_FRAME`] animation steps of the
    /// other zoom groups' patterns that are already rendered, so a zoom
    /// change finds them on the GPU.
    fn ensure_sea(&mut self, scene: &Scene<'_>, group: usize) {
        self.sea_group = None;
        if let Some(p) = scene.sea(group) {
            self.upload_sea_steps(group, p, usize::MAX);
            self.sea_group = Some(group);
        }
        let mut budget = SEA_STEPS_PER_FRAME;
        for g in (0..TILE.len()).filter(|&g| g != group) {
            if budget == 0 {
                break;
            }
            if let Some(p) = scene.sea_if_ready(g) {
                budget -= self.upload_sea_steps(g, p, budget);
            }
        }
    }

    /// Uploads up to `max` missing animation steps of `p` as the pattern of
    /// `group`; returns the number uploaded.
    fn upload_sea_steps(&mut self, group: usize, p: &crate::sea::SeaPattern, max: usize) -> usize {
        let size = (
            u32::try_from(p.width).expect("positive"),
            u32::try_from(p.height).expect("positive"),
        );
        if !self.seas.contains_key(&group) {
            let layers = u32::try_from(p.step_count()).expect("fits");
            let make = || {
                let texture = self.device.create_texture(&wgpu::TextureDescriptor {
                    label: Some("sea"),
                    size: Extent3d {
                        width: size.0,
                        height: size.1,
                        depth_or_array_layers: layers,
                    },
                    mip_level_count: 1,
                    sample_count: 1,
                    dimension: wgpu::TextureDimension::D2,
                    format: TextureFormat::Rgba8Unorm,
                    usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST,
                    view_formats: &[],
                });
                let view = texture.create_view(&wgpu::TextureViewDescriptor {
                    dimension: Some(TextureViewDimension::D2Array),
                    ..Default::default()
                });
                (texture, view)
            };
            let ((ts, vs), (tf, vf)) = (make(), make());
            self.seas.insert(
                group,
                GpuSea {
                    size: (p.width, p.height),
                    steps: p.step_count(),
                    step_ms: p.step_ms,
                    uploaded: 0,
                    sprite: vs,
                    filled: vf,
                    _textures: [ts, tf],
                },
            );
        }
        let sea = self.seas.get_mut(&group).expect("inserted");
        let mut n = 0;
        while sea.uploaded < sea.steps && n < max {
            let k = sea.uploaded;
            let (sprite, filled) = p.step_images(k);
            for (texture, px) in sea._textures.iter().zip([sprite, filled]) {
                self.queue.write_texture(
                    TexelCopyTextureInfo {
                        texture,
                        mip_level: 0,
                        origin: Origin3d {
                            x: 0,
                            y: 0,
                            z: u32::try_from(k).expect("fits"),
                        },
                        aspect: TextureAspect::All,
                    },
                    &rgba_bytes(px),
                    TexelCopyBufferLayout {
                        offset: 0,
                        bytes_per_row: Some(4 * size.0),
                        rows_per_image: Some(size.1),
                    },
                    Extent3d {
                        width: size.0,
                        height: size.1,
                        depth_or_array_layers: 1,
                    },
                );
            }
            sea.uploaded += 1;
            n += 1;
        }
        n
    }

    /// Uploads newly built visible chunks (at most [`UPLOADS_PER_FRAME`]),
    /// adds stand-ins for missing ones, and drops GPU chunks the CPU cache
    /// no longer holds.
    fn sync_chunks(&mut self, frames: &FrameRenderer, group: usize, visible: &[(i32, i32)]) {
        let visible_keys: BTreeSet<ChunkKey> =
            visible.iter().map(|&(cx, cy)| (group, cx, cy)).collect();
        self.chunks.retain(|k, c| {
            if c.real {
                frames.chunk(*k).is_some()
            } else {
                visible_keys.contains(k)
            }
        });
        let mut uploads = 0;
        for &key in &visible_keys {
            let have_real = self.chunks.get(&key).is_some_and(|c| c.real);
            if have_real {
                continue;
            }
            match frames.chunk(key) {
                Some(chunk) if uploads < UPLOADS_PER_FRAME => {
                    uploads += 1;
                    let c = self.upload_chunk(key, Some(chunk));
                    self.chunks.insert(key, c);
                }
                _ => {
                    if !self.chunks.contains_key(&key) {
                        let c = self.upload_chunk(key, None);
                        self.chunks.insert(key, c);
                    }
                }
            }
        }
    }

    fn upload_chunk(&self, (_, cx, cy): ChunkKey, chunk: Option<&Chunk>) -> GpuChunk {
        let r = chunk_rect(cx, cy);
        let n = u32::try_from(CHUNK).expect("positive");
        let mut flags = 0u32;
        let mut textures = Vec::new();
        let mut view =
            |format, bpp, data: Option<Vec<u8>>, bit: u32, dummy: &TextureView| match data {
                Some(d) => {
                    let (t, v) = upload_texture(
                        &self.device,
                        &self.queue,
                        format,
                        (n, n),
                        bpp,
                        &[&d],
                        false,
                    );
                    textures.push(t);
                    flags |= bit;
                    v
                }
                None => dummy.clone(),
            };
        let ground = chunk
            .filter(|c| !c.ground.is_empty())
            .map(|c| c.ground.iter().flat_map(|p| p.to_le_bytes()).collect());
        let objects = chunk
            .filter(|c| !c.objects.is_empty())
            .map(|c| rgba_bytes(&c.objects));
        let field = chunk.filter(|c| !c.cover.is_empty()).map(|c| {
            c.cover
                .iter()
                .zip(&c.shore)
                .zip(&c.depth)
                .zip(&c.flags)
                .flat_map(|(((&a, &b), &d), &f)| [a, b, d, f])
                .collect()
        });
        // 0RGB little-endian = B, G, R, 0.
        let ground = view(TextureFormat::Bgra8Unorm, 4, ground, 1, &self.dummy_color);
        let objects = view(TextureFormat::Rgba8Unorm, 4, objects, 2, &self.dummy_color);
        let field = view(TextureFormat::Rgba8Uint, 4, field, 4, &self.dummy_field);
        let terrain = chunk
            .filter(|c| !c.terrain.is_empty())
            .map(|c| c.terrain.iter().flat_map(|v| v.to_le_bytes()).collect());
        let terrain = view(TERRAIN_FORMAT, 2, terrain, 8, &self.dummy_terrain);
        let uniform = self.device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("chunk"),
            size: 16,
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let mut u = Vec::with_capacity(16);
        u.extend(r.x.to_le_bytes());
        u.extend(r.y.to_le_bytes());
        u.extend(CHUNK.to_le_bytes());
        u.extend(flags.to_le_bytes());
        self.queue.write_buffer(&uniform, 0, &u);
        let bind = self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("chunk"),
            layout: &self.chunk_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: uniform.as_entire_binding(),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: BindingResource::TextureView(&ground),
                },
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: BindingResource::TextureView(&objects),
                },
                wgpu::BindGroupEntry {
                    binding: 3,
                    resource: BindingResource::TextureView(&field),
                },
                wgpu::BindGroupEntry {
                    binding: 4,
                    resource: BindingResource::TextureView(&terrain),
                },
            ],
        });
        GpuChunk {
            bind,
            real: chunk.is_some(),
            _textures: textures,
            _uniform: uniform,
        }
    }

    /// Appends one instance per drawable sprite of `draws` to `bytes`
    /// (packing new frames into the atlas) and returns their number.
    fn pack_sprites(
        &mut self,
        scene: &Scene<'_>,
        draws: &[SpriteDraw<'_>],
        tested: bool,
        bytes: &mut Vec<u8>,
    ) -> u32 {
        let mut n = 0;
        for d in draws {
            let Some((layer, ax, ay)) = self.atlas_place(scene, d) else {
                continue;
            };
            let (x, y, w, h) = d.rect();
            for v in [x, y] {
                bytes.extend(v.to_le_bytes());
            }
            let w = u32::try_from(w).expect("positive");
            let h = u32::try_from(h).expect("positive");
            // Bit 0 shadow, bits 8-15 object height, bit 16 depth-tested.
            let flags =
                u32::from(d.is_shadow()) | u32::from(d.height) << 8 | u32::from(tested) << 16;
            for v in [w, h, ax, ay, layer, flags] {
                bytes.extend(v.to_le_bytes());
            }
            n += 1;
        }
        n
    }

    /// The atlas position of a sprite frame, packing it on first use.
    fn atlas_place(&mut self, scene: &Scene<'_>, d: &SpriteDraw<'_>) -> Option<(u32, u32, u32)> {
        if let Some(p) = self.atlas.as_ref().and_then(|a| a.place.get(&d.key)) {
            return *p;
        }
        let img = scene.sprites.get(&scene.anlagen, d.key, d.frame);
        let place = img.and_then(|img| {
            let (w, h) = (u32::from(img.width), u32::from(img.height));
            if w == 0 || h == 0 || w > ATLAS_SIZE || h > ATLAS_SIZE {
                return None;
            }
            let (layer, x, y) = self.atlas_alloc(w, h);
            let atlas = self.atlas.as_ref().expect("allocated");
            self.queue.write_texture(
                TexelCopyTextureInfo {
                    texture: &atlas.texture,
                    mip_level: 0,
                    origin: Origin3d { x, y, z: layer },
                    aspect: TextureAspect::All,
                },
                &img.pixels,
                TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(4 * w),
                    rows_per_image: Some(h),
                },
                Extent3d {
                    width: w,
                    height: h,
                    depth_or_array_layers: 1,
                },
            );
            Some((layer, x, y))
        });
        if let Some(a) = self.atlas.as_mut() {
            a.place.insert(d.key, place);
        } else if place.is_none() {
            // No atlas yet and nothing to place: remember nothing.
        }
        place
    }

    /// Room for `w × h` texels: shelf packing, growing the array by one
    /// layer (copying the old layers) when full.
    fn atlas_alloc(&mut self, w: u32, h: u32) -> (u32, u32, u32) {
        if self.atlas.is_none() {
            self.atlas = Some(self.new_atlas(1, None));
        }
        let a = self.atlas.as_mut().expect("created");
        let (mut layer, mut x, mut y, mut shelf) = a.cursor;
        if x + w > ATLAS_SIZE {
            (x, y, shelf) = (0, y + shelf, 0);
        }
        if y + h > ATLAS_SIZE {
            (layer, x, y, shelf) = (layer + 1, 0, 0, 0);
        }
        a.cursor = (layer, x + w, y, shelf.max(h));
        if layer >= a.layers {
            let old = self.atlas.take().expect("present");
            self.atlas = Some(self.new_atlas(layer + 1, Some(old)));
        }
        (layer, x, y)
    }

    fn new_atlas(&self, layers: u32, old: Option<Atlas>) -> Atlas {
        let texture = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("atlas"),
            size: Extent3d {
                width: ATLAS_SIZE,
                height: ATLAS_SIZE,
                depth_or_array_layers: layers,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: TextureFormat::Rgba8Unorm,
            usage: TextureUsages::TEXTURE_BINDING
                | TextureUsages::COPY_DST
                | TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let (place, cursor) = match old {
            Some(old) => {
                let mut encoder =
                    self.device
                        .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                            label: Some("atlas"),
                        });
                encoder.copy_texture_to_texture(
                    old.texture.as_image_copy(),
                    texture.as_image_copy(),
                    Extent3d {
                        width: ATLAS_SIZE,
                        height: ATLAS_SIZE,
                        depth_or_array_layers: old.layers,
                    },
                );
                self.queue.submit([encoder.finish()]);
                (old.place, old.cursor)
            }
            None => (BTreeMap::new(), (0, 0, 0, 0)),
        };
        let view = texture.create_view(&wgpu::TextureViewDescriptor {
            dimension: Some(TextureViewDimension::D2Array),
            ..Default::default()
        });
        let bind = self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("atlas"),
            layout: &self.atlas_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: BindingResource::TextureView(&view),
            }],
        });
        Atlas {
            texture,
            bind,
            layers,
            place,
            cursor,
        }
    }

    fn write_instances(&mut self, bytes: &[u8]) {
        if bytes.is_empty() {
            return;
        }
        let len = u64::try_from(bytes.len()).expect("fits");
        if self.instances.as_ref().is_none_or(|b| b.size() < len) {
            self.instances = Some(self.device.create_buffer(&wgpu::BufferDescriptor {
                label: Some("sprites"),
                size: len.next_power_of_two().max(4096),
                usage: BufferUsages::VERTEX | BufferUsages::COPY_DST,
                mapped_at_creation: false,
            }));
        }
        self.queue
            .write_buffer(self.instances.as_ref().expect("created"), 0, bytes);
    }

    fn write_frame_uniform(
        &self,
        view: Rect,
        group: usize,
        time_ms: u64,
        style: WaterStyle,
        scene: &Scene<'_>,
    ) {
        let (k0, k1, frac, size) = match self.sea() {
            Some(s) => {
                let (k0, k1, frac) = steps_at(s.steps, s.step_ms, time_ms);
                (k0, k1, frac, s.size)
            }
            None => (0, 0, 0.0, (1, 1)),
        };
        #[allow(clippy::cast_precision_loss)]
        let (tw, th) = (TILE[group].0 as f32, TILE[group].1 as f32);
        #[allow(clippy::cast_precision_loss)]
        let raise = scene.lift.scaled(SEA_LEVEL, 1, group) as f32;
        #[allow(clippy::cast_precision_loss)]
        let t = (time_ms % 3_600_000) as f32;
        let mut u = Vec::with_capacity(64);
        for v in [view.x, view.y] {
            u.extend(v.to_le_bytes());
        }
        for v in [view.w, view.h] {
            u.extend(u32::try_from(v.max(1)).expect("positive").to_le_bytes());
        }
        for v in [size.0, size.1] {
            u.extend(v.to_le_bytes());
        }
        for v in [k0, k1] {
            u.extend(u32::try_from(v).expect("fits").to_le_bytes());
        }
        u.extend(frac.to_le_bytes());
        u.extend(t.to_le_bytes());
        u.extend(u32::from(style == WaterStyle::Enhanced).to_le_bytes());
        for v in [tw, th, raise, 0.0, 0.0] {
            u.extend(v.to_le_bytes());
        }
        self.queue.write_buffer(&self.frame_uniform, 0, &u);
    }

    fn frame_bind(&self, under: Option<&TextureView>, terrain: Option<&TextureView>) -> BindGroup {
        let (sprite, filled) = match self.sea() {
            Some(s) => (&s.sprite, &s.filled),
            None => (&self.dummy_array, &self.dummy_array),
        };
        self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("frame"),
            layout: &self.frame_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: self.frame_uniform.as_entire_binding(),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: BindingResource::TextureView(under.unwrap_or(&self.dummy_color)),
                },
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: BindingResource::TextureView(sprite),
                },
                wgpu::BindGroupEntry {
                    binding: 3,
                    resource: BindingResource::TextureView(filled),
                },
                wgpu::BindGroupEntry {
                    binding: 4,
                    resource: BindingResource::TextureView(terrain.unwrap_or(&self.dummy_terrain)),
                },
            ],
        })
    }
}

/// A device without a window (benchmarks, tests): the high-performance
/// adapter and its name, or `None` if there is no usable GPU.
pub fn headless_device() -> Option<(Device, Queue, String)> {
    let instance =
        wgpu::Instance::new(wgpu::InstanceDescriptor::new_without_display_handle_from_env());
    let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
        power_preference: wgpu::PowerPreference::HighPerformance,
        ..Default::default()
    }))
    .ok()?;
    let info = adapter.get_info();
    let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
        label: Some("anno1503"),
        required_limits: adapter.limits(),
        ..Default::default()
    }))
    .ok()?;
    Some((device, queue, format!("{} ({:?})", info.name, info.backend)))
}

impl GpuFrame {
    /// Waits until the GPU has finished all submitted work.
    pub fn wait(&self) {
        let _ = self.device.poll(wgpu::PollType::wait_indefinitely());
    }

    /// An offscreen target of `size` in [`GpuFrame::view_format`] that can
    /// be read back.
    pub fn offscreen_target(&self, (w, h): (u32, u32)) -> (Texture, TextureView) {
        let texture = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("offscreen"),
            size: Extent3d {
                width: w,
                height: h,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: self.format,
            usage: TextureUsages::RENDER_ATTACHMENT | TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
        (texture, view)
    }

    /// The pixels of `texture` (`w × h`, 4 bytes each, in the texture's
    /// channel order), waiting for the GPU.
    pub fn read_back(&self, texture: &Texture, (w, h): (u32, u32)) -> Vec<u8> {
        let row = (4 * w).div_ceil(256) * 256;
        let buffer = self.device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("read back"),
            size: u64::from(row) * u64::from(h),
            usage: BufferUsages::COPY_DST | BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });
        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor { label: None });
        encoder.copy_texture_to_buffer(
            texture.as_image_copy(),
            wgpu::TexelCopyBufferInfo {
                buffer: &buffer,
                layout: TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(row),
                    rows_per_image: Some(h),
                },
            },
            Extent3d {
                width: w,
                height: h,
                depth_or_array_layers: 1,
            },
        );
        self.queue.submit([encoder.finish()]);
        buffer.slice(..).map_async(wgpu::MapMode::Read, |_| {});
        self.wait();
        let data = buffer.slice(..).get_mapped_range().expect("mapped");
        let to_usize = |v: u32| usize::try_from(v).expect("fits");
        (0..to_usize(h))
            .flat_map(|y| {
                let s = y * to_usize(row);
                data[s..s + to_usize(4 * w)].to_vec()
            })
            .collect()
    }
}

/// `SeaPattern::steps_at` for an uploaded pattern.
fn steps_at(steps: usize, step_ms: u64, time_ms: u64) -> (usize, usize, f32) {
    if step_ms == 0 || steps < 2 {
        return (0, 0, 0.0);
    }
    let n = u64::try_from(steps).expect("fits");
    let k = usize::try_from((time_ms / step_ms) % n).expect("< steps");
    #[allow(clippy::cast_precision_loss)]
    let frac = (time_ms % step_ms) as f32 / step_ms as f32;
    (k, (k + 1) % steps, frac)
}

fn begin<'e>(
    encoder: &'e mut wgpu::CommandEncoder,
    view: &'e TextureView,
    load: LoadOp<Color>,
) -> wgpu::RenderPass<'e> {
    encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
        label: None,
        color_attachments: &[Some(wgpu::RenderPassColorAttachment {
            view,
            depth_slice: None,
            resolve_target: None,
            ops: Operations {
                load,
                store: StoreOp::Store,
            },
        })],
        depth_stencil_attachment: None,
        timestamp_writes: None,
        occlusion_query_set: None,
        multiview_mask: None,
    })
}

#[cfg(test)]
mod tests {
    /// The shader parses and validates (catches WGSL errors without a GPU,
    /// e.g. on CI).
    #[test]
    fn shader_is_valid_wgsl() {
        let module = naga::front::wgsl::parse_str(include_str!("gpu.wgsl"))
            .unwrap_or_else(|e| panic!("{}", e.emit_to_string(include_str!("gpu.wgsl"))));
        naga::valid::Validator::new(
            naga::valid::ValidationFlags::all(),
            naga::valid::Capabilities::all(),
        )
        .validate(&module)
        .unwrap_or_else(|e| panic!("{e:?}"));
    }
}
