//! GPU host of the coastal wave simulation (`fluid.wgsl`): textures,
//! compute pipelines, the camera-local ring-buffer window and time
//! stepping. Model, parameters and the CPU reference: [`crate::fluid`].

use std::borrow::Cow;

use wgpu::{
    BindGroup, BindingResource, BindingType, Buffer, BufferUsages, CommandEncoder, ComputePipeline,
    Device, Extent3d, Origin3d, Queue, ShaderStages, TexelCopyBufferLayout, TexelCopyTextureInfo,
    Texture, TextureAspect, TextureFormat, TextureUsages, TextureView, TextureViewDimension,
};

use crate::fluid::{
    CellRect, FluidConfig, FluidWindow, SWELL_COMPONENTS, SwellComponent, reset_cell,
    swell_components,
};

/// Bytes of the per-step uniform (`Step` in `fluid.wgsl`).
const STEP_BYTES: u64 = 32;

/// The simulation on the GPU.
pub struct FluidGpu {
    pub cfg: FluidConfig,
    comps: [SwellComponent; SWELL_COMPONENTS],
    window: Option<FluidWindow>,
    group: usize,
    grid: Grid,
    layout: wgpu::BindGroupLayout,
    params: Buffer,
    steps: Buffer,
    align: u64,
    vel_pipe: ComputePipeline,
    eta_pipe: ComputePipeline,
    /// Simulation time in seconds.
    sim_t: f64,
}

/// Textures of a grid of `n × n` cells and their bind groups; recreated
/// when the window size changes (viewport resize).
struct Grid {
    n: u32,
    /// State `(η, u, v, foam)`: texture 0 holds the current state after
    /// every step; texture 1 is the intermediate of pass A.
    states: [Texture; 2],
    views: [TextureView; 2],
    bathy: Texture,
    bathy_view: TextureView,
    /// Pass A reads 0 writes 1; pass B reads 1 writes 0.
    bind_a: BindGroup,
    bind_b: BindGroup,
}

impl Grid {
    fn new(
        device: &Device,
        layout: &wgpu::BindGroupLayout,
        params: &Buffer,
        steps: &Buffer,
        n: u32,
    ) -> Self {
        let size = Extent3d {
            width: n,
            height: n,
            depth_or_array_layers: 1,
        };
        let state = || {
            device.create_texture(&wgpu::TextureDescriptor {
                label: Some("fluid state"),
                size,
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: TextureFormat::Rgba32Float,
                usage: TextureUsages::TEXTURE_BINDING
                    | TextureUsages::STORAGE_BINDING
                    | TextureUsages::COPY_DST
                    | TextureUsages::COPY_SRC,
                view_formats: &[],
            })
        };
        let states = [state(), state()];
        let views = [
            states[0].create_view(&wgpu::TextureViewDescriptor::default()),
            states[1].create_view(&wgpu::TextureViewDescriptor::default()),
        ];
        let bathy = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("fluid depth"),
            size,
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: TextureFormat::R32Float,
            usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST,
            view_formats: &[],
        });
        let bathy_view = bathy.create_view(&wgpu::TextureViewDescriptor::default());
        let bind = |src: &TextureView, dst: &TextureView| {
            device.create_bind_group(&wgpu::BindGroupDescriptor {
                label: Some("fluid"),
                layout,
                entries: &[
                    wgpu::BindGroupEntry {
                        binding: 0,
                        resource: params.as_entire_binding(),
                    },
                    wgpu::BindGroupEntry {
                        binding: 1,
                        resource: BindingResource::Buffer(wgpu::BufferBinding {
                            buffer: steps,
                            offset: 0,
                            size: wgpu::BufferSize::new(STEP_BYTES),
                        }),
                    },
                    wgpu::BindGroupEntry {
                        binding: 2,
                        resource: BindingResource::TextureView(src),
                    },
                    wgpu::BindGroupEntry {
                        binding: 3,
                        resource: BindingResource::TextureView(dst),
                    },
                    wgpu::BindGroupEntry {
                        binding: 4,
                        resource: BindingResource::TextureView(&bathy_view),
                    },
                ],
            })
        };
        let bind_a = bind(&views[0], &views[1]);
        let bind_b = bind(&views[1], &views[0]);
        Self {
            n,
            states,
            views,
            bathy,
            bathy_view,
            bind_a,
            bind_b,
        }
    }
}

fn tex_entry(binding: u32) -> wgpu::BindGroupLayoutEntry {
    wgpu::BindGroupLayoutEntry {
        binding,
        visibility: ShaderStages::COMPUTE,
        ty: BindingType::Texture {
            sample_type: wgpu::TextureSampleType::Float { filterable: false },
            view_dimension: TextureViewDimension::D2,
            multisampled: false,
        },
        count: None,
    }
}

fn uniform_entry(binding: u32, dynamic: bool) -> wgpu::BindGroupLayoutEntry {
    wgpu::BindGroupLayoutEntry {
        binding,
        visibility: ShaderStages::COMPUTE,
        ty: BindingType::Buffer {
            ty: wgpu::BufferBindingType::Uniform,
            has_dynamic_offset: dynamic,
            min_binding_size: None,
        },
        count: None,
    }
}

impl FluidGpu {
    pub fn new(device: &Device, queue: &Queue, cfg: FluidConfig) -> Self {
        let module = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("fluid.wgsl"),
            source: wgpu::ShaderSource::Wgsl(Cow::Borrowed(include_str!("fluid.wgsl"))),
        });
        let layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("fluid"),
            entries: &[
                uniform_entry(0, false),
                uniform_entry(1, true),
                tex_entry(2),
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: ShaderStages::COMPUTE,
                    ty: BindingType::StorageTexture {
                        access: wgpu::StorageTextureAccess::WriteOnly,
                        format: TextureFormat::Rgba32Float,
                        view_dimension: TextureViewDimension::D2,
                    },
                    count: None,
                },
                tex_entry(4),
            ],
        });
        let pl = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("fluid"),
            bind_group_layouts: &[Some(&layout)],
            immediate_size: 0,
        });
        let pipe = |entry: &str| {
            device.create_compute_pipeline(&wgpu::ComputePipelineDescriptor {
                label: Some(entry),
                layout: Some(&pl),
                module: &module,
                entry_point: Some(entry),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                cache: None,
            })
        };
        let (vel_pipe, eta_pipe) = (pipe("vel_step"), pipe("eta_step"));
        let comps = swell_components(&cfg);
        let params = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("fluid params"),
            size: 80 + 16 * 16,
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        queue.write_buffer(&params, 0, &params_bytes(&cfg, &comps));
        let align = u64::from(device.limits().min_uniform_buffer_offset_alignment).max(STEP_BYTES);
        let steps = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("fluid steps"),
            size: align * u64::from(cfg.max_steps.max(1)),
            usage: BufferUsages::UNIFORM | BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        // Small until the first update sizes it to the view.
        let grid = Grid::new(device, &layout, &params, &steps, 64);
        Self {
            cfg,
            comps,
            window: None,
            group: usize::MAX,
            grid,
            layout,
            params,
            steps,
            align,
            vel_pipe,
            eta_pipe,
            sim_t: 0.0,
        }
    }

    /// The current state `(η, u, v, foam)` (ring-buffer storage).
    pub fn state_view(&self) -> &TextureView {
        &self.grid.views[0]
    }

    /// Still-water depth per cell (ring-buffer storage, `R32Float`).
    pub fn depth_view(&self) -> &TextureView {
        &self.grid.bathy_view
    }

    /// The current state texture (for read-back in tests).
    pub fn state_texture(&self) -> &Texture {
        &self.grid.states[0]
    }

    /// The window, once [`FluidGpu::update`] has run.
    pub fn window(&self) -> Option<FluidWindow> {
        self.window
    }

    /// Brings the simulation to `time_ms` for a view at zoom `group`
    /// centred on tile position `centre` and spanning `span_tiles` tiles
    /// (sizes the window, [`FluidConfig::window_size`]): recentres the window (new cells
    /// get depth `depth(x, y)` and the incoming swell, computed on
    /// `threads` CPU threads) and records the time steps into `encoder`.
    #[allow(clippy::too_many_arguments)]
    pub fn update(
        &mut self,
        device: &Device,
        queue: &Queue,
        encoder: &mut CommandEncoder,
        group: usize,
        centre: (f32, f32),
        span_tiles: f32,
        time_ms: u64,
        depth: &(dyn Fn(f32, f32) -> f32 + Sync),
        threads: usize,
    ) {
        #[allow(clippy::cast_precision_loss)]
        let target = time_ms as f64 / 1000.0;
        let size = self.cfg.window_size(group, span_tiles);
        if size != self.grid.n {
            self.grid = Grid::new(device, &self.layout, &self.params, &self.steps, size);
            self.window = None;
        }
        let new_rects = match &mut self.window {
            Some(w) if self.group == group => w.recentre(centre, self.cfg.recentre_cells),
            _ => {
                let w = FluidWindow::with_size(&self.cfg, group, centre, size);
                self.window = Some(w);
                self.group = group;
                self.sim_t = target;
                vec![w.rect()]
            }
        };
        let window = self.window.expect("set");
        #[allow(clippy::cast_possible_truncation)]
        let now = self.sim_t as f32;
        for r in new_rects {
            self.fill(queue, &window, r, depth, now, threads);
        }
        let dt = f64::from(self.cfg.dt);
        if target < self.sim_t {
            // Time went back (pause, clock wrap): restart from here.
            self.sim_t = target;
        }
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let due = ((target - self.sim_t) / dt).floor() as u64;
        let n = due.min(u64::from(self.cfg.max_steps));
        if due > n {
            // Too far behind: skip ahead rather than stall the frame.
            #[allow(clippy::cast_precision_loss)]
            let skip = (due - n) as f64 * dt;
            self.sim_t += skip;
        }
        if n == 0 {
            return;
        }
        let mut bytes = vec![0u8; usize::try_from(self.align * n).expect("fits")];
        for k in 0..n {
            #[allow(clippy::cast_precision_loss)]
            let t = self.sim_t + dt * (k + 1) as f64;
            let o = usize::try_from(self.align * k).expect("fits");
            #[allow(clippy::cast_possible_truncation)]
            let step = step_bytes(&window, t as f32);
            bytes[o..o + step.len()].copy_from_slice(&step);
        }
        queue.write_buffer(&self.steps, 0, &bytes);
        #[allow(clippy::cast_precision_loss)]
        {
            self.sim_t += dt * n as f64;
        }
        let groups = self.grid.n.div_ceil(8);
        let mut pass = encoder.begin_compute_pass(&wgpu::ComputePassDescriptor {
            label: Some("fluid"),
            timestamp_writes: None,
        });
        for k in 0..n {
            let off = u32::try_from(self.align * k).expect("fits");
            pass.set_pipeline(&self.vel_pipe);
            pass.set_bind_group(0, &self.grid.bind_a, &[off]);
            pass.dispatch_workgroups(groups, groups, 1);
            pass.set_pipeline(&self.eta_pipe);
            pass.set_bind_group(0, &self.grid.bind_b, &[off]);
            pass.dispatch_workgroups(groups, groups, 1);
        }
    }

    /// Computes depth and initial state of the cells of `r` and uploads
    /// them (split where the ring buffer wraps).
    fn fill(
        &self,
        queue: &Queue,
        window: &FluidWindow,
        r: CellRect,
        depth: &(dyn Fn(f32, f32) -> f32 + Sync),
        t: f32,
        threads: usize,
    ) {
        let w = usize::try_from(r.x1 - r.x0).expect("positive");
        let h = usize::try_from(r.y1 - r.y0).expect("positive");
        let mut d = vec![0.0f32; w * h];
        let mut s = vec![[0.0f32; 4]; w * h];
        let rows = h.div_ceil(threads.max(1)).max(1);
        std::thread::scope(|scope| {
            for (k, (dc, sc)) in d
                .chunks_mut(rows * w)
                .zip(s.chunks_mut(rows * w))
                .enumerate()
            {
                let (cfg, comps) = (&self.cfg, &self.comps);
                scope.spawn(move || {
                    for (i, (dv, sv)) in dc.iter_mut().zip(sc.iter_mut()).enumerate() {
                        let x = r.x0 + i64::try_from(i % w).expect("fits");
                        let y = r.y0 + i64::try_from(k * rows + i / w).expect("fits");
                        let (px, py) = window.cell_centre(x, y);
                        *dv = depth(px, py);
                        *sv = reset_cell(cfg, comps, window, (x, y), t, *dv);
                    }
                });
            }
        });
        let n = window.size;
        // Split the rectangle where it wraps around the storage.
        let split = |a0: i64, a1: i64| -> Vec<(i64, i64)> {
            let s0 = a0.rem_euclid(n);
            let len = a1 - a0;
            if s0 + len <= n {
                vec![(a0, a1)]
            } else {
                vec![(a0, a0 + (n - s0)), (a0 + (n - s0), a1)]
            }
        };
        for (x0, x1) in split(r.x0, r.x1) {
            for (y0, y1) in split(r.y0, r.y1) {
                let (pw, ph) = (
                    usize::try_from(x1 - x0).expect("positive"),
                    usize::try_from(y1 - y0).expect("positive"),
                );
                let mut db = Vec::with_capacity(pw * ph * 4);
                let mut sb = Vec::with_capacity(pw * ph * 16);
                for y in y0..y1 {
                    let row = usize::try_from(y - r.y0).expect("inside") * w;
                    let col = usize::try_from(x0 - r.x0).expect("inside");
                    for v in &d[row + col..row + col + pw] {
                        db.extend(v.to_le_bytes());
                    }
                    for v in &s[row + col..row + col + pw] {
                        for c in v {
                            sb.extend(c.to_le_bytes());
                        }
                    }
                }
                let (sx, sy) = window.storage(x0, y0);
                let origin = Origin3d {
                    x: u32::try_from(sx).expect("fits"),
                    y: u32::try_from(sy).expect("fits"),
                    z: 0,
                };
                let extent = Extent3d {
                    width: u32::try_from(pw).expect("fits"),
                    height: u32::try_from(ph).expect("fits"),
                    depth_or_array_layers: 1,
                };
                let write = |tex: &Texture, data: &[u8], bpp: usize| {
                    queue.write_texture(
                        TexelCopyTextureInfo {
                            texture: tex,
                            mip_level: 0,
                            origin,
                            aspect: TextureAspect::All,
                        },
                        data,
                        TexelCopyBufferLayout {
                            offset: 0,
                            bytes_per_row: Some(u32::try_from(pw * bpp).expect("fits")),
                            rows_per_image: Some(u32::try_from(ph).expect("fits")),
                        },
                        extent,
                    );
                };
                write(&self.grid.bathy, &db, 4);
                write(&self.grid.states[0], &sb, 16);
            }
        }
    }
}

/// `Params` of `fluid.wgsl`.
fn params_bytes(cfg: &FluidConfig, comps: &[SwellComponent; SWELL_COMPONENTS]) -> Vec<u8> {
    #[allow(clippy::cast_precision_loss)]
    let v = [
        cfg.gravity(),
        cfg.depth_cap,
        cfg.min_depth,
        cfg.dt,
        cfg.damping,
        cfg.friction,
        cfg.break_ratio,
        cfg.break_rate,
        cfg.foam_gain,
        (-cfg.dt / cfg.foam_decay).exp(),
        cfg.force_depth.0,
        cfg.force_depth.1,
        cfg.force_rate,
        cfg.sponge_cells as f32,
        cfg.infiltration,
        cfg.deep_speed / cfg.depth_cap,
        cfg.max_speed,
        cfg.swash_foam,
        cfg.break_min_depth,
        0.0,
    ];
    let mut out: Vec<u8> = v.iter().flat_map(|f| f.to_le_bytes()).collect();
    for c in comps {
        for f in [c.k[0], c.k[1], c.omega, c.phase, c.amplitude, 0.0, 0.0, 0.0] {
            out.extend(f.to_le_bytes());
        }
    }
    out
}

/// `Step` of `fluid.wgsl`.
fn step_bytes(w: &FluidWindow, t: f32) -> Vec<u8> {
    let i = |v: i64| i32::try_from(v).expect("world cell fits i32").to_le_bytes();
    let mut out = Vec::with_capacity(32);
    out.extend(i(w.origin.0));
    out.extend(i(w.origin.1));
    out.extend(i(w.size));
    out.extend(0i32.to_le_bytes());
    #[allow(clippy::cast_precision_loss)]
    out.extend((w.cells_per_tile as f32).to_le_bytes());
    out.extend(t.to_le_bytes());
    out.extend([0u8; 8]);
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fluid::FluidGrid;

    #[test]
    fn fluid_shader_is_valid_wgsl() {
        let src = include_str!("fluid.wgsl");
        let module = naga::front::wgsl::parse_str(src)
            .unwrap_or_else(|e| panic!("{}", e.emit_to_string(src)));
        naga::valid::Validator::new(
            naga::valid::ValidationFlags::all(),
            naga::valid::Capabilities::all(),
        )
        .validate(&module)
        .unwrap_or_else(|e| panic!("{e:?}"));
    }

    /// The GPU solver matches the CPU reference (`FluidGrid`) after many
    /// steps on a beach with a recentre in between. Skipped without a GPU.
    #[test]
    fn gpu_matches_cpu_reference() {
        let Some((device, queue, _)) = crate::gpu::headless_device() else {
            eprintln!("no GPU adapter; skipped");
            return;
        };
        let cfg = FluidConfig {
            size: 64,
            cells_per_tile: [4, 4, 4],
            recentre_cells: 4,
            ..FluidConfig::DEFAULT
        };
        let depth = |x: f32, y: f32| 16.0 - 2.0 * (x - 2.0).max(0.0) + 0.3 * (y * 0.7).sin();
        let mut gpu = FluidGpu::new(&device, &queue, cfg);
        let mut time_ms = 1000u64;
        // The first update creates the window at the current time.
        let mut enc =
            device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: None });
        gpu.update(
            &device,
            &queue,
            &mut enc,
            0,
            (8.0, 8.0),
            0.0,
            time_ms,
            &depth,
            2,
        );
        queue.submit([enc.finish()]);
        #[allow(clippy::cast_possible_truncation)]
        let mut cpu = Some(FluidGrid::new(
            cfg,
            gpu.window().expect("window"),
            depth,
            gpu.sim_t as f32,
        ));
        for frame in 0..40 {
            // Move the view after 20 frames so new strips are filled.
            let centre = if frame < 20 { (8.0, 8.0) } else { (10.0, 9.5) };
            let mut enc =
                device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: None });
            let before = gpu.sim_t;
            gpu.update(
                &device, &queue, &mut enc, 0, centre, 0.0, time_ms, &depth, 2,
            );
            queue.submit([enc.finish()]);
            let w = gpu.window().expect("window");
            let g = cpu.get_or_insert_with(|| {
                #[allow(clippy::cast_possible_truncation)]
                FluidGrid::new(cfg, w, depth, before as f32)
            });
            if g.window != w {
                // Same strips as the GPU fill.
                let mut old = g.window;
                let rects = old.recentre(centre, cfg.recentre_cells);
                g.window = old;
                #[allow(clippy::cast_possible_truncation)]
                for r in rects {
                    g.fill(r, &depth, before as f32);
                }
            }
            let mut t = before;
            while t + 1e-9 < gpu.sim_t {
                t += f64::from(cfg.dt);
                #[allow(clippy::cast_possible_truncation)]
                g.step(t as f32);
            }
            time_ms += 33;
        }
        let gf = crate::gpu::GpuFrame::new(device, queue, TextureFormat::Rgba8Unorm);
        let px = gf.read_back_rgba32f(gpu.state_texture(), (64, 64));
        let g = cpu.expect("ran");
        let mut max = 0.0f32;
        for (a, b) in px.iter().zip(&g.state) {
            for c in 0..4 {
                max = max.max((a[c] - b[c]).abs());
            }
        }
        assert!(max < 1e-2, "max difference {max}");
    }
}
