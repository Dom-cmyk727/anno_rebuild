# Rivers (enhanced water): flow, gap fix, zoom spikes

**Status 2026-10-19 (after the investigation run):** findings 2 and 3 are **already fixed** by a separate, earlier investigation (commits "fix(render): enhanced rivers carry water along the whole painted bed" and "fix(render): zoom changes and chunk bursts no longer block the window thread"; their causes and numbers are in `iso.md` "River gaps", `docs/perf/README.md` "2026-10-19 — zoom stutter" and the `river_qa` tool). They supersede stages S0–S3 below where they overlap (the gap fix used an interpolating Catmull-Rom bed plus a painted-bed floor instead of the per-piece `tau + 0.5` mask of S3; the spike fix is `GpuLimits::interactive()`); what remains open is **finding 1, the flow itself: stages S4–S9** (hydraulics, lattice bake, shading, GPU port, extras, options). **S4 (hydraulics, CPU only) is built (2026-10-19): section 14 records what shipped, where it differs from this design and the measured numbers. S5 (lattice bake and CPU shading, the first owner-visible flow) is built (2026-10-19): section 15. S6 (GPU port and quality tiers) is built (2026-10-19): section 16; the viewer's GPU compositor now draws the flow.** The review of S5 found and fixed an aliasing stretch, per-quad shear steps and unprotected rules (section 15.7). Before S4, re-read S3's mask against the shipped gap fix and reuse the shipped one. Original status line of the design: design, nothing implemented yet (2026-10-19, tree `359b1ce`). Written for the owner feedback of the same day (enhanced water, Windows PC): (1) rivers have no flow physics, only a pattern sliding along one fixed axis; (2) enhanced rivers have gaps where the original has none (**fix**); (3) 85-132 ms frame spikes when the zoom group changes (**fix**). Companion to [`iso.md`](iso.md) (Water, Enhanced water); once a stage lands, the matching rows of `iso.md` are updated and this page keeps the physics, the contracts and the QA method.

Evidence tags used below: **M** measured on this machine 2026-10-19 (RTX 4090, Vulkan, `build/msvc` release, `bench_frame`); **P** parsed from the original files (`tools/insel.py`, read-only); **D** disassembly (module + address); **C** code read at `359b1ce`; **E** estimate (to be replaced by a measurement in the stage named); **T** tuning value (a number chosen by look, never evidence); **?** open question. Every constant in section 2 is T or derived; the only evidence-backed facts are the data statistics (1.2) and the original's depth test.

## 0. Summary

| Owner finding | Root cause | Fix | Section |
|---|---|---|---|
| Rivers do not flow | `river_shade` adds constant time offsets to the tile axes, the same drift vector everywhere; the water field has no velocity (C: `iso.cpp:686`, `gpu.wgsl:313`) | steady depth-averaged flow solved once per island in tile space on a worker, baked to a small per-chunk lattice, animated by flow-map advection on the GPU | 2-4 |
| Gaps (brown bed between blue stretches) | `kRiverLevel = 9` (`sea.hpp:46`) against the original's depth test `terrain <= item + kRiverTolerance` with 10 (`scene.hpp:129`, `scene.cpp:375/696`); levels averaged across tiers; mask taken from an approximating B-spline plus shore noise | per-piece exact test in tile space, soft edge, union with the depth-tested sprite alpha, connectivity checked against the piece graph | 5 |
| 85-132 ms at zoom changes | `FluidGpu::update` refills the whole wave-simulation window on the window thread and joins a 16-thread pool there (C: `gpu_fluid.cpp:243-381`, `gpu.cpp:760-786`); 87.3 / 113.4 / 138.2 ms first frame at groups 1 / 2 / 0 (M) | window built by a worker, time-sliced upload into a second grid, analytic fallback with cross-fade; worker-packed chunk buffers; byte budget; thread budget | 7 |

Model choice: a **steady diffusive-wave (zero-inertia) flow with Manning conveyance**, solved once per island, advected visually with **flow maps**. Rejected for now: a runtime Saint-Venant solver on the GPU (very high implementation and stability cost for a field that is nearly steady, Froude 0.1-0.45 everywhere except falls; tolerance-only CPU/GPU parity; history-dependent state) is kept only as a documented optional "Tier 1" that would overwrite the same lattice (option D16). The data contract below does not change if it is ever added.

Lineage of this synthesis (three independent designs were scored): the flow-map design is the base (physics/cheat separation, staged order, parity story); taken from the steady-solve design: linear first solver, falls as links in the mask, water-plane pattern coordinates, turbulence from shear/bends/Froude, the pixel gap metric; taken from the shallow-water design: connectivity invariant against the piece graph, forward projection of fall sprites, kept probe position, bank-roughness closure, oracle in Python plus `anno-dump`, cold group-switch bench rows at 4K, Tier-0-first rule.

## 1. How enhanced rivers work today

### 1.1 Code map (C)

| Step | Where | What |
|---|---|---|
| Pieces | `Scene::Scene` (`scene.cpp:175-197`) | FLUESSE 2701, 2705-2707 become `IslandData::rivers` (`x, y, 3, 3, height`); falls/mouths 2702-2704 become `falls_below/above` |
| Level | `IslandWater::build` (`sea.cpp:246-284`) | per height corner the mean of `height + kRiverLevel (9)` over covering pieces, then 2 smoothing passes |
| Mask | `sea::water_field` (`sea.cpp:351-381`) | water iff `river_level - smooth_height > -0.35`; `smooth_height` is an approximating B-spline plus +-2 units of shore noise; `flags = 2 \| slope << 2` |
| Falls, mouths | `Scene::apply_fall_masks`, `blend_rivers_into_sea` (`scene.cpp:349-554`) | sprite alpha of 2702-2704 (depth-tested), blurred 4 px, becomes river water; river pixels within `kRiverBlend` (16 px) of sea take the sea look through the `shore` byte (`kb`) |
| Chunks | `render_chunk` (`frame.cpp:151`) | 512 x 512 px per zoom group on background workers: `Chunk::{cover, shore, depth, flags, terrain, objects, base}`; the ground rasteriser records the terrain tile position per pixel (`GroundProbe::pos`) |
| GPU / CPU shade | `sea_fs` (`gpu.wgsl:505-595`), `sea::shade_sea` (`sea.hpp:197-338`) | `flags & 2` selects `river_shade`, mixed with the sea look by `kb` |
| Rocks | `object_in_water` (`gpu.wgsl:141`) | ripple `sin(fx*0.35/s + t*0.0021)`, again an axis-fixed drift |

### 1.2 What the river data looks like (P, all 465 templates)

- River piece heights (`state >> 8`) take only {16, 32, 48, 64, 80, 96, 112}; **6,780 of 7,145 pieces (95 %) have height 32 = sea level**. Islands have 1 (47), 2 (21), 3 (2) or 4 (2) distinct river heights. Rivers are **flat tiers**: the surface-level gradient is zero except at the few falls between tiers, so the data carries no slope and no discharge. Every speed in this design is a model output (honest label: "plausible, not original").
- Painted beds are quantised (4-unit steps, textures in 3 x 3 blocks). `D_256x256_0045_5B`, tier 32: bed corners 32:70, 36:109, 38:5, 42:174, 48:354; the original test (`<= 42`) passes 174 corners that the enhanced rule (`<= 41`) drops. Over the corpus 5,347 of 14,849 corners passing `<= z+10` sit **exactly** at `z+10` (M, 26.5 % of the corners counted wet by the original; 46 of 72 river islands exceed 5 %).
- Tile-space network (original criterion inside each footprint) of `D_256x256_0045_5B`: **28 connected components until the falls/mouth footprints are added, 5 after**. Falls and mouths are the links of a river; a mask or a component test that forgets them breaks every cascade.
- Measured gap, `D_256x256_0045_5B` (the owner's canyon): 16.9 % of the area the original criterion covers is missing in tile space; about 28 % of the original's river pixels have no enhanced water at group 1; connected components of the water mask with the original predicate 5, tolerance 11 gives 5, tolerance 9 gives 43, current enhanced rule gives 22 (M).
- Sprites are 3 x 3 decals, 64 frames = 16 animation steps x 4 rotations; the 16 steps of one rotation differ by 0-3.5 % in alpha. Whether the rotation (0-3) encodes a flow direction in the artwork is unknown (? new open question).
- River widths (mean 2A/P): 1.4 (Dschungel) to 2.6 tiles (Nord); median depth 2-5 HEIGHT units (0.09-0.2 tile); `Single_01.sze` has 1,622 tiles of river water on four islands (M).

## 2. Physical model

Units: the model lives in **tile space** (island-local tile axes; `w.x = px/tw + py/th`, `w.y = py/th - px/tw` as in the shaders), lengths in tiles, heights in HEIGHT units (1 unit = 0.04 tile, `iso.md` Elevation), time in seconds, velocities in tiles/s.

### 2.1 Length scale (derived, no new constant)

The wave simulation fixes `g = deep_speed^2 / (depth_cap * 0.04) = 3^2 / (16 * 0.04) = 14.06 tiles/s^2` (`FluidConfig::gravity()` 0.5625 tiles^2/(s^2 unit)). Equating with 9.81 m/s^2:

    ell = 9.81 / 14.06 = 0.698 m per tile                                          (C1, derived)

so rivers and sea share one metre scale (a 2.5-tile river is 1.7 m wide, a 10-unit bed 0.28 m deep). It is used only to convert Manning's roughness.

### 2.2 Geometry (per island raster, tile space)

    Z_i      = h_i + tau + 0.5            water level of piece i, flat; tau = kRiverTolerance = 10         (G1)
    wet_i(p) = T_raw(p) <= Z_i            T_raw = terrain_height, the original's two triangles per tile     (G2)
    Z(p)     = max over wet covering pieces; linear ramp between the tiers across a fall/mouth piece          (G3)
    h(p)     = 0.04 * clamp( G_sigma_h * (Z - b) , h_min, h_max )        masked blur of the bed over wet cells (G4)

`tau + 0.5` makes a bed at exactly `item + 10` wet (0.5 units of water), as in the original's sprites. (G4) uses a masked Gaussian (`sigma_h = 0.75` tile, T) of the carved bed so the 4-unit steps and 3 x 3 painted blocks of the data do not become blocky speed variation through the conveyance `K ~ h^(5/3)` (`h_min = 1.5`, `h_max = 12` units, T; the display depth used for tint keeps its own lighter smoothing and never the shore noise). **Physics:** depth = level minus bed. **Cheat:** rigid lid (the surface does not respond to the flow; the data is flat), depth clamps and bed smoothing.

### 2.3 Hydraulics: steady diffusive wave with Manning conveyance

Manning in tile units (`v_SI = (1/n) R^(2/3) S^(1/2)`, `R_SI = ell R`, `v = v_SI / ell`):

    v   = k_M R^(2/3) S^(1/2),   k_M = 1 / (n ell^(1/3)) = 32.2 tile^(1/3)/s      n = 0.035 s/m^(1/3) (Chow 1959: 0.030-0.050)   (E1)
    q   = h v = k_M h^(5/3) S^(1/2)         wide-channel limit R ~ h, discharge per unit width, tiles^2/s                        (E2)

Depth-averaged continuity with rigid lid and uniform lateral inflow `r`, and the **linear conveyance form** (default, one SPD solve, no iteration):

    div q = r                         on wet cells                                    (E3)
    q = -K grad(phi),   K(p) = k_M h(p)^(5/3) / nu(d_bank(p))                         (E4)
    div( K grad phi ) = -r            phi = 0 at outlets, no flux through banks and holes   (E5)
    nu(d) = 1 + a_b exp(-d / delta_b)      bank roughness multiplier on n; a_b = 4, delta_b = 0.30 tile (T)   (E6)

`phi` is a head-like potential in arbitrary units: the slope in (E2) is frozen at one reference value, which drops out because the absolute scale is set by the discharge rescaling (2.4); only the direction and the ratios of `q` (continuity, conveyance split at braids, widening and narrowing, deflection around holes) are physics. (E6) is the **bank closure**: higher roughness near the banks slows the margins and, because it sits in `K`, redistributes the flux to the core conservatively (one law instead of a post-multiplied wall factor); the speed ratio bank / core is `1 / (1 + a_b) = 0.2`.

Optional refinement (config `picard_iterations`, default 0, decision D1b): the true Manning form `K = k_M h^(5/3) / (nu sqrt(abs(grad phi) + eps))` by Picard iteration, each step one PCG solve warm-started from the last. It is ill-conditioned exactly where the data is flat (heads, ponds, tier-32 pieces), so it is guarded: an iteration cap, a convergence test, and an automatic per-component fall back to the linear solution (`FlowStats` records which).

Boundary conditions:

| Where | Condition |
|---|---|
| banks, dry cells, obstacle holes | no flux (faces to dry cells have zero conductance; automatic in the finite-volume form) |
| outlets | Dirichlet `phi = 0` on wet cells within 1 tile of terrain below `kWaterline`, and on the cells of mouth pieces (2703, 2704) |
| component without any outlet | Dirichlet on the cluster with the lowest level, then the lowest bed (heuristic, nothing to be faithful to; listed per island by the corpus test, D14) |
| heads and springs | none; uniform lateral inflow `r` replaces source detection |

**Solver.** Finite volumes on the hydraulic grid (`flow_cells_per_tile = 8`, the mask grid; 4 if the S4 budget requires it), face conductance = harmonic mean of the two cells, face fluxes conservative by construction. Nested PCG over levels R = 1, 2, 4, 8 (a cell is active at a coarse level if any child is; coarse conductances by series/parallel sums): the slow modes of a long thin network are its long wavelengths, solved cheaply on the coarse levels, and the fine level only repairs the cross-section. Jacobi (optionally IC(0)) preconditioner, `double` accumulations in fixed order, single thread per island (islands run in parallel), so the result is independent of the thread count. **Failure is visible, not silent:** if the residual stays above 1e-3 after the iteration cap, the island falls back to the Eikonal field (fast sweeping distance `D` to the outlets, `u = -grad D / abs(grad D)`, magnitude `V_out`), `FlowStats::fell_back` is set, a log line is written and `--river-debug` paints the island. Loops are wrong in the fallback (the longer arm of a braid flows back to the nearer junction); the corpus test lists every island that falls back.

Why a Poisson solve and not a skeleton plus distance field: a distance-to-outlet field is wrong on loops, cuts meander corners and needs hand-made junction rules; (E5) satisfies continuity at confluences and braids by construction, splits flow in proportion to conveyance, and the cross-channel profile emerges from the bed shape in the data.

### 2.4 Discharge, velocity, falls, ponds

    q <- q * s_c        per connected component c; s_c chosen so that the mean cross-section speed at the outlets of c equals V_out        (E7)
    u = q / h                                                                                              (E8)
    abs(u) <- ( abs(u)^4 + v_floor^4 )^(1/4)         soft floor, heads and springs are not still           (E9)
    abs(u) <- max( abs(u), rho_f * v_fall ),   rho_f = smoothstep(kRapidsFrom, kRapidsFull, S),  S = abs(grad Z)   (E10)
    abs(u) <- min( abs(u), v_cap )                                                                        (E11)

Defaults (T): `V_out = 0.9` tiles/s (0.63 m/s; range 0.4-1.6), `v_floor = 0.12` (0.05-0.3), `v_fall = 2.5`, `v_cap = 3.0` (the deep-water wave speed of the sea simulation), `kRapidsFrom/Full` the existing legacy constants (5 and 14 units/tile; a 48-unit drop over a 3-tile fall piece is 16 units/tile). Bernoulli would give `sqrt(2 g dz) = 7.3` tiles/s for the 80 to 32 drop, far too fast to read, hence the cap (cheat). With uniform lateral inflow the speed grows downstream with the upstream area, tributaries are slower than the main stem, widenings and shallows slower than narrows (`v = Q/A`).

**Discharge model (D2).** The data has no discharge, so this is an explicit option. `Normalised` (default): (E7) per component, every river has `V_out` at its mouth; predictable look, a creek and a large river look alike at their mouths but differ along their course by continuity. `Absolute`: a fixed `r` per wet tile (tuned so the reference island reaches `V_out`), so long rivers are faster at the mouth than short ones. `Sourced`: Neumann inflow `Q_s = v_src * A_s` at detected dead ends instead of uniform inflow. Q6 contact sheets show all three on the same islands before the default is confirmed.

**Ponds.** Components smaller than 12 cells are not solved; they get a static baked eddy `u = 0.03 * (cos th, sin th)`, `th = 2 pi noise(0.2 cell)` (D7) so lakes are not frozen.

### 2.5 Froude number, standing waves, curvature (opt-in)

    Fr = abs(u) / sqrt(g h)                                                       (E12)
    lambda = 2 pi abs(u)^2 / g       clamp to [0.45, 2.5] tiles; crests perpendicular to u    (E13, physics)
    A_sw = 0.05 * smoothstep(0.5, 1.0, Fr)           brightness units, cheat

With the game's numbers (h <= 0.48 tile, c = sqrt(g h) = 2.6 tiles/s) every ordinary reach is subcritical (Fr 0.1-0.45); only falls (h about 3 units, c = 1.3, u = 2.5) are supercritical, so standing waves, when enabled (D3, default off), appear where a real river shows them, at the foot of falls and rapids. They are the only feature that is not advected.

**Bends.** Potential flow is fastest at the inner bank of a bend, the opposite of nature (secondary flow pushes the thalweg to the outer bank). Documented limitation, tested on the synthetic bend (Q11), and an opt-in correction (D12, default off) applied after the solve:

    u <- u * ( 1 + beta_t * tanh( kappa_s W ) * n_hat ),   renormalised to keep the cross-section mean   (E14)

`kappa_s` is the signed streamline curvature (1/tile, positive when the centre of curvature is on the left), `W` the local channel width from the distance transform, `n_hat` in [-1, 1] the lateral position across the channel, positive toward the outer bank; `beta_t = 0.35` (T). Continuity is kept by the renormalisation.

### 2.6 Obstacles and wakes

Objects whose footprint lies mostly in water become holes in the hydraulic domain (zero conductance, footprint shrunk 15 %) before the solve, so the field deflects around them with stagnation upstream and speed-up at the sides. Selection rule (D4, conservative default): footprint area <= 16 tiles, >= 50 % of its cells wet, **and** the Anlagen id is in `obstacle_ids`, a short list reviewed visually in S7 (candidates from the corpus, unverified: 1502-1508, 1549-1551, 1572-1577, 1580, 1581; object categories and the meaning of `WALKMASK` are not in the data, ?). The far-field of the solved flow is checked against the potential flow around a cylinder of radius `a` in a stream `U`:

    u = U (1 - a^2 (x^2 - y^2) / r^4),   v = -2 U a^2 x y / r^4          (x along U, origin at the centre)   (E15)

Wake (lee) channel: for each wet cell trace back up to `L_w = 8 a_eff` (range 4-16) tiles along `-u` and set `wake = exp(-t / L_w)` if the trace enters a hole. Used to scale speed (`u *= 1 - 0.6 wake`), darken (-0.06) and raise the streak amplitude (x1.6). Physics: lee wakes exist. Cheat: the intensity law.

### 2.7 Turbulence channel (cheat driven by physical quantities)

    turb = clamp( 0.35 smoothstep(0, 0.4, 1 - sdf/0.4)      bank shear layer (sdf = distance to the bank, tiles)
                + 0.5  abs(delta_e)                          change of direction over +-0.5 tile: bends, confluences
                + 0.5  smoothstep(0.45, 0.95, Fr)            rapids
                + wake , 0, 1 )                              (E16)

It raises the amplitude of the small-scale layers and the weight of the high-frequency streaks. The weights are T.

### 2.8 Visual layer: two-phase flow-map advection on the river's own water plane

For layer `l` with spatial scale `s_l` (1/tile), period `T_l` (ms), flow `u` (tiles/s) and **water-plane position `x`** (tiles):

    phi_k = fract( t / T_l + psi(x) + k/2 ),   k = 0, 1        psi(x) = 0.5 noise(0.13 x)                       (E17)
    wt_k  = 1 - abs(2 phi_k - 1)                                wt_0 + wt_1 = 1
    c_k   = ( x - u * (T_l / 1000) * phi_k ) * s_l + o_k       (the design's stretch S(u, kappa_l) along u is not built: it aliased, 15.7)
    N_l   = ( wt_0 n(c_0, z) + wt_1 n(c_1, z) ) / sqrt( wt_0^2 + wt_1^2 )     variance-preserving, no contrast dip   (E18)
    light = sum_l A_l f_l(turb) ( N_l(x) - N_l(x + e L) ) / e                  L = light direction (-x tile axis, as fluid_shade), e = 0.15 tile

`d c_k / dt = -u s_l`, so a feature moves downstream with exactly `u`; each phase resets while its weight is 0. `n` is the existing `noise3`; `z` is a slow third coordinate (boil). Artefacts and their mitigations: pulsing (`psi`, variance-preserving blend), shear stretch where `u` varies (`T abs(grad u)` fades the layer amplitude by `smoothstep(0.5, 1.5, .)`, measured on 1-tile rivers at all groups, Q10), hourly wrap (`frame.time_ms` is taken modulo 3,600,000, so every `T_l` must divide it; 800, 1600, 2400 ms do).

**The coordinate fix (F9).** Today the river noise uses the sea plane (`py + raise`, raise = 32 units), which on a high tier is offset from the river surface by `(Z - 32) * 2.059 * tw/84` px, about 117-120 px at group 0 for tier 80 (C, steady-solve measurement), so any pattern slides against the geometry and jumps across a fall. The pattern coordinate is now

    x = w( px, py + lambda * Z(px) ),     lambda = 2.059 * tw / 84 px per HEIGHT unit,   Z from the baked level channel   (E19)

with `Z` interpolated bilinearly (it ramps across falls, so `x` is continuous across tier boundaries). The flow `u` itself is looked up at the exact terrain **probe position** of the pixel (`GroundProbe::pos`), the same position the mask already uses, so every lookup is independent of zoom group and chunk alignment; the difference to the water-plane position is at most `0.04 h lambda / th` (< 0.6 tile at the deepest bed) and the flow is depth-averaged anyway.

Default layer table (T; passed as a uniform array, see 3.3; the count is data):

| layer | role | s_l (1/tile) | T_l (ms) | A_l | kappa_l (s/tile) | LOD |
|---|---|---|---|---|---|---|
| L0 | slow undulation | 0.40 | 2400 | 0.045 | 0.35 | all groups |
| L1 | ripples | 1.15 | 1600 | 0.060 | 0.55 | all groups |
| L2 | glints | 3.10 | 800 | 0.040 | 0.80 | `wave_detail` (0 at group 2) |
| H | whitewater streaks (D5) | 7.3 | 640 | 0.05 + 0.16 n_hi, times `smoothstep(0.55, 0.95, Fr)` | 0.35 | `wave_detail` |

The kappa column (stretch along the flow) is **not built**: applied to the island's tile coordinates it moved the noise coordinate by up to ten times the geometry wherever the flow varies (section 15.7); a stretch needs a coordinate that follows the streamlines.

Light variance is calibrated in S5 to the legacy shimmer's standard deviation (+-20 %) so the average river brightness does not change. Tint stays `0.15 * smoothstep(4, 0, depth)` plus an aeration tint toward (0.62, 0.82, 0.90) scaled by the rapids factor, never white; foam on rivers stays zero (owner 2026-10-18).

### 2.9 Physics versus visual cheats

| Physics (follows an equation) | Cheats (tuned, not derived) |
|---|---|
| depth = level minus bed; Manning's friction law (E1, E2); continuity and flow split at confluences, braids, widenings (E3-E5); cross-channel profile from the bed; deflection around holes; Froude number and standing-wave wavelength (E12, E13); features move with `u` (E17); pattern on the river's own water plane (E19) | rigid lid; lateral inflow as discharge model and `V_out`; `v_floor`, `v_cap`, `v_fall`; bank roughness `a_b, delta_b`; depth clamps and bed smoothing; pond eddies; wake law; turbulence weights; curvature correction (E14); noise as "surface normal"; layer table, amplitudes, `psi` |

## 3. Data: what exists, where it is computed

### 3.1 Pipeline

| When | Where | What | Stored |
|---|---|---|---|
| Scene load | background pool, low priority, islands ordered by distance to the view; never the window thread | phase A: `RiverMask` (R1-R4 of 3.2, cheap); phase B: `RiverFlow` (R5-R9, the solve) | immutable `shared_ptr`s per island: ~1-5 MB (sparse 8 x 8-tile blocks, same as FLUESSE) |
| Chunk build (worker, per zoom group) | `render_chunk` | water field from `RiverMask::sample(probe.pos)`; sprite union; falls/mouth masks (unchanged); `Chunk::flow` lattice if the chunk has river pixels; packed GPU buffers | `Chunk` (+133 KB per river chunk at stride 4, none for others) |
| Upload (window thread) | `GpuFrame::sync_chunks` | `wgpuQueueWriteTexture` of the lattice textures, byte-budgeted | two GPU textures per river chunk, pooled |
| Frame (GPU) | `sea_fs` | river pixels only: lattice fetch, `river_flow_shade`, blend with the sea look by `kb` | nothing |
| Frame (CPU) | none new | | |

A chunk worker never blocks on phase B. Phase A is bounded (budget 100 ms per island, measured in S3) and built under `std::call_once` by whichever worker first needs it. If phase B is not ready (`try_get()` returns null) the chunk is built with the **legacy river shimmer on the correct mask** and tagged `flow_pending`; it is re-queued when phase B finishes (generation counter), and the lattice replaces the legacy look. Prefetch at scene load keeps this rare; a test forces it (S5).

### 3.1a Flow builder lifecycle (round dw, DW2; `src/render/src/scene.cpp`)

Phase B runs on a pool of builders shared by the generations of one scene. The protocol that keeps a solve safe
when its requester is gone, when the scene is destroyed, and when several builders solve at once:

- **States of a slot.** Idle, Queued (a request exists or a builder holds it), Ready (`flow` is published; the
  release store of `state` orders it), Failed (a solve threw; the picture keeps its last good state).
- **Requests.** A request holds its slot (shared) and its asker: a shared asker by weak owner, a non-shared
  asker by address. A non-shared scene removes its own queued requests and its waiter entries under the builders'
  mutex before it is destroyed.
- **Solver choice and registration are one step** under `FlowBuild::mutex`: the builder dequeues a request,
  picks the solver (the asker if it is alive, else a surviving waiter of the slot) and registers the solve in
  `FlowBuild::active` (one entry per solve in progress; several builders may solve one scene).
- **Destruction.** `~Scene` removes its queued requests and waiter entries, then waits until `active` holds no
  entry of the scene. A dequeued solve is in `active` before the lock is released, so no destructor misses it.
- **Strong owners.** A builder that solves for a shared scene holds a strong owner for the solve. The owner is
  released after the lock is gone and after the solve is unregistered: the last owner's destructor takes the same
  mutex.
- **Publication.** `FlowBuild` is created with the scene's caches (never published later); the flow threads belong
  to the caches and are joined by their destructor, except that a builder which releases the last owner detaches
  its own thread (it cannot join itself).
- **Lock order.** `FlowBuild::mutex` is the only mutex of the protocol; no strong scene reference is released while
  it is held, and no solve runs while it is held.
- **Tests.** `river_lifecycle_test.cpp`: two workers with a gated solve (destruction stays blocked until the gate
  opens, checked for 5 s); one, two and four workers with one gated and one throwing solve; queued requests of a
  retired generation; a non-shared scene destroyed with a request queued; the last owner released by the builder.
  Pre-fix evidence: the one-marker version returned early and read freed memory (ASan, `cache/dw-runs`).

### 3.2 Island raster algorithm (R1-R9; pure function of the inputs, deterministic, thread-count independent)

| Step | What |
|---|---|
| R1 raw mask | cells 1/8 tile. Regular pieces: wet iff `T_raw(cell) <= Z_i` (G1, G2) inside the footprint dilated by 0.75 tile (the original's pixel test sees terrain displaced by up to ~0.5 tile from the decal tile), per piece, union, **no averaging across tiers**. Falls and mouths (2702-2704): wet where the **group-0 sprite alpha** (2 x 2 max-filtered, the sprite is a 50 % checkerboard) is >= 128 at the cell's **forward projection** `sx = (x - y) 42`, `sy = (x + y) 21 - lambda_0 T_raw`; needs no inverse of the iso mapping, is exact, and gives all zoom groups the same shape |
| R2 close | morphological closing, radius 2 cells (0.25 tile), fills pinholes and one-cell necks; cells on the ridge of the distance transform stay wet (centreline guarantee) |
| R3 soft edge | `C = box_box(M, sigma)` with `sigma = 0.30` tile, coverage `smoothstep(tau_e - 0.08, tau_e + 0.08, C)`; `tau_e = 0.5` (original width, default) or 0.40 (soft-wide, D6); the centreline cells force `cover = 1` so smoothing can never cut a neck |
| R4 level, depth | `Z` per G1-G3 (falls and mouths take the level of the tier on each side and ramp linearly); `h` per G4 |
| R5 domain | hydraulic grid = wet cells with `C >= tau_e`; obstacle holes (2.6); components < 12 cells are ponds |
| R6 outlets | 2.3 table; sea contact uses the land classifier family of `IslandWater::sea_chunk` |
| R7 solve | nested PCG (2.3), fallback Eikonal; `FlowStats` per island |
| R8 post | discharge rescale per component (E7-E9), falls speed (E10), cap (E11), pond eddies, optional curvature (E14), `turb` (E16), `wake` (2.6), one wetness-weighted 3 x 3 smoothing |
| R9 extend, export | propagate `u`, `turb`, `Z` (nearest valid) 2 tiles beyond the wet domain with `valid` fading 1 to 0, so the soft edge, sprite-only pixels and the falls/mouth masks always find a flow; quantise (`u` 1/4096 tiles/s, `Z` 1/16 unit, `h` 1/16 unit) |

### 3.3 Structures (C++ and WGSL in principle; the layout in files is explicit and fixed-width)

```cpp
namespace anno::render::river {

struct LayerConfig { float scale, period_ms, amplitude; bool detail_fades, rapids; };   // no stretch (15.7)

/// All tunables (data, PLAN 0.1). Defaults are section 2; none is evidence.
struct RiverConfig {
    bool enabled = true;                                 // Enhanced + river flow; --river-flow legacy turns it off
    std::uint32_t mask_cells_per_tile = 8, flow_cells_per_tile = 8;
    float mask_sigma_tiles = 0.30F, edge_threshold = 0.5F, surface_over_tolerance = 0.5F;   // edge_threshold 0.40 = soft-wide
    float h_min = 1.5F, h_max = 12.0F, bed_sigma_tiles = 0.75F;                              // HEIGHT units, tiles
    float manning_n = 0.035F, metres_per_tile = 0.698F, conveyance_exp = 5.0F / 3.0F;        // 1.5 = Chezy
    float bank_roughness = 4.0F, bank_width_tiles = 0.30F;
    float out_speed = 0.9F, speed_floor = 0.12F, speed_cap = 3.0F, fall_speed = 2.5F;
    enum class Discharge { Normalised, Absolute, Sourced } discharge = Discharge::Normalised;
    std::uint32_t picard_iterations = 0, cg_max_iterations = 400; float cg_tolerance = 1e-5F;
    bool thalweg_shift = false; float thalweg_beta = 0.35F;
    float pond_swirl = 0.03F; bool standing_waves = false; bool wake = true;
    std::vector<std::uint32_t> obstacle_ids; std::uint32_t obstacle_max_area = 16; float obstacle_min_wet = 0.5F;
    bool bridge_gaps = false;                                                                // D13: log only
    std::vector<std::uint32_t> lattice_stride = {8, 4, 4};                                   // scene px per texel, per zoom group
    float velocity_max = 8.0F;                                                               // int16 lattice range, tiles/s
    std::vector<LayerConfig> layers;                                                         // section 2.8 table
};

struct FlowStats {                     // logs, tests, --river-debug
    std::size_t wet_cells = 0, components = 0, outlets = 0, holes = 0, ponds = 0, bridges_needed = 0;
    std::array<std::uint32_t, 4> iterations{}; float residual = 0; bool fell_back = false; double build_ms = 0;
};

class RiverMask {                      // phase A, immutable
  public:
    struct Sample { float cover, level, depth, sdf; };
    static std::shared_ptr<const RiverMask> build(const IslandWater&, std::span<const sea::RiverPiece>,
                                                  std::span<const Prepared> falls, const RiverConfig&);
    [[nodiscard]] Sample sample(float tx, float ty) const;       // bilinear in tile space
    [[nodiscard]] bool any_in(Rect tile_rect) const;             // chunk culling
};

class RiverFlow {                      // phase B, immutable, built by a future; try_get() never blocks
  public:
    struct Sample { std::array<float, 2> u; float turb, wake, valid; };
    [[nodiscard]] Sample sample(float tx, float ty) const;
    [[nodiscard]] const FlowStats& stats() const;
};

/// Per chunk and zoom group, global lattice in scene pixels (texel (i,j) at scene pixel (cx*512 + i*stride, cy*512 + j*stride),
/// pixel-corner coordinates, n = 512/stride + 1 texels per side, one apron texel on the right/bottom).
struct FlowPlane {
    std::uint32_t stride_px = 4, n = 0;
    std::vector<std::int16_t> uv;      // 2 per texel, 1/4096 tiles/s
    std::vector<std::uint8_t> aux;     // 4 per texel: valid (weight), turb, wake, level / 0.5 HEIGHT unit
    std::vector<std::uint8_t> shear;   // 1 per texel: |grad u| in 1/32 1/s, baked from the island flow (S5 review)
};
struct FlowSample { float vx = 0, vy = 0, turb = 0, wake = 0, level = 0, valid = 0; };
FlowSample flow_at(const FlowPlane&, int local_x, int local_y);  // valid-weighted bilinear at the pixel centre (+0.5)
}
```

`Chunk::flow` is empty when the chunk has no river pixels. GPU: chunk flags bit 5 = flow plane present, bits 8-15 = lattice stride (the 16-byte chunk struct is unchanged); `@group(1) @binding(6) flow_uv: texture_2d<i32>` (`RG16Sint`) and `@binding(7) flow_aux: texture_2d<u32>` (`RGBA8Uint`), both read with `textureLoad` and bilinear weights computed in the shader (identical arithmetic on CPU); dummy 1 x 1 textures when absent. The `sea_fs` layout then holds 13 sampled textures against the default limit of 16. Frame uniform stays 96 bytes (a padding float becomes `river_flow`). The **layer table is a separate uniform buffer** `river_params { count: u32, layers: array<RiverLayer, kMaxRiverLayers> }` with `kMaxRiverLayers = 8` (reason: a uniform array needs a compile-time size; raising it is one constant plus a shader override), filled from `RiverConfig::layers` and validated at load (every period divides 3,600,000 ms, count <= 8). The lattice strides are derived from the zoom-group tile width (`stride = largest divisor of the chunk size close to 0.1 tile`, defaults 8/4/4 px = 0.10/0.10/0.20 tile at groups 0/1/2), not hard-coded in the shader (carried per chunk in the flags).

```wgsl
struct RiverLayer { scale: f32, period_ms: f32, amp: f32, fades: f32, rapids: f32 }   // no stretch (15.7)
struct RiverParams { count: u32, layers: array<RiverLayer, 8> }
@group(0) @binding(N) var<uniform> river: RiverParams;

fn flow_at(local_px: vec2<i32>) -> FlowSample;       // valid-weighted bilinear of 4 x 2 textureLoad, decode (c / 4096)
fn river_flow_shade(px: vec2<i32>, f: FlowSample, depth: f32, slope: f32, t: f32, detail: f32) -> vec3<f32>;
// x = w(px, py + lambda * f.level) (E19); loops over river.layers[0..river.count); returns (light, 0.0, tint)
```

`sea_fs` change (the only hot-path hunk): in the `flags & 2` branch replace `river_shade(...)` by `river_flow_shade(...)` when `chunk.flags & 32u != 0u && frame.river_flow != 0.0`, else the legacy call. The `kb` blend into the sea look and `shade_apply` are untouched.

## 4. Per-frame work on the GPU

| Item | Cost |
|---|---|
| CPU (window thread) | zero new work per frame; lattice upload only when a river chunk arrives, inside the byte budget (7) |
| Non-river pixels | none (branch is on `flags & 2`) |
| River pixels | 8 lattice loads, one `river_flow_shade`: per layer 2 phases x (1 value-noise lookup + 1 light tap) plus streaks; ~900 ALU at 3 layers (E) |
| LOD | group 2 drops L2 and H (`wave_detail`); `RiverQuality::Low` drops the light taps, `Medium` drops H; a noise-texture variant (4 loads instead of 8 hashes) is the S9 optimisation if S6 measures a problem |

Everything is a function of (baked bytes, pixel position, time): deterministic, reproducible in `--png --time`, identical at every chunk border and zoom group because every pixel samples the same global lattice and the tile-space field gives the same tiles/s at all groups. Speeds on screen scale with the tile width, as they should.

## 5. Gap fix (owner finding 2, tag fix)

### 5.1 Causes (C, M)

1. Level off by one: `kRiverLevel 9` against tolerance 10 (1.2: 26.5 % of the corners the original counts as wet, 174 of the 720 corners under the tier-32 pieces of `D_256x256_0045_5B`).
2. Levels averaged across tiers: `(41 + 89)/2 = 65` next to falls; the upper side of a fall dries, the lower side floods banks.
3. Mask from an approximating surface: `smooth_height` is a cubic B-spline *approximation* (a 1-tile bed of 42 between banks of 48 is raised to 44) plus +-2 units of shore noise designed for beaches.
4. No connectivity guarantee, falls and mouths only exist as sprite alpha at the resolution of the current group (the 28 to 5 component result of 1.2 shows they are the links).
5. Suspected, decided by Q4: `blurred_alpha` in `apply_fall_masks` clamps at the chunk rectangle, so the falls mask may change within ~4 px of a chunk border.

### 5.2 Design

- Mask, level, depth from the island raster, steps R1-R4. The chunk reads `RiverMask::sample(probe.pos)` instead of `river_level`/`smooth_height` (one bilinear fetch of four fields instead of a 16-tap B-spline, two noises and four level taps: chunk building gets cheaper).
- **Safety net in pixel space (coverage is a guaranteed superset of the original):** the river pieces' own sprites are drawn depth-tested into the chunk exactly as `apply_fall_masks` does for falls (`draw_prepared_tested`), blurred 2-4 px, and `cover = max(cover_raster, blur(alpha_sprite) * 0.9)`. The `max` with a sub-unity gain and the blur keep the union a *minimum-coverage clamp with a soft falloff*, not a hard cut; sprite-only pixels get `rd = a * kFallDepth - 0.5` (the falls convention). Edge softness is tested (Q1b) so the union cannot silently bring back the hard triangle edges the 2026-10-17 rewrite removed.
- The slope bits for rapids come from the raster level gradient `S = abs(grad Z)` (zero on flat tiers, ramped across falls and mouths), not from the old averaged level.
- Falls and mouth masks and `blend_rivers_into_sea` stay as they are in pixel space (owner likes the transition); a 4-px apron fixes the suspected seam (F4) if Q4 confirms it.
- Connectivity: components of the tile-space mask (falls and mouths included) are counted per **piece-graph component** (an edge when footprints touch or overlap). Invariant: components(enhanced) <= components(original) and one component per piece-graph component. Where the *data* has a break (a piece pair whose footprints touch but whose beds never reach the level), the island is listed with its position as a data oddity and `bridges_needed` is counted. Default (D13): **log, never patch silently**; `bridge_gaps = true` carves a 3-cell A* bridge over the union of both footprints with cost `1 + 0.2 max(0, T_raw - Z)` so the owner can look and decide.
- Width (D6): `edge_threshold 0.5` keeps the original's width (area-preserving smoothing plus the superset union); 0.40 fills shallow lobes (rivers a little wider than the sprites). The mouth fade to the sea is checked by pixel diff outside river pixels (Q8).

### 5.3 Acceptance (automated, Q1)

Per river island x zoom group, with `M_o` = alpha of the original river sprites (render `Layers{water_above}` with `WaterStyle::Original` into a transparent canvas, depth-tested, exact, no colour classification) and `M_e` = enhanced river pixels (`flags & 2`, coverage > 0.5):

| Metric | Definition | Pass |
|---|---|---|
| coverage | `abs(erode_1px(M_o) and M_e) / abs(erode_1px(M_o))` | >= 99.5 % |
| components | 8-connected after a 1 px closing, size >= 8 px | `components(M_e) <= components(M_o)` |
| largest gap | largest connected piece of `M_o and not M_e` inside the corridor | < 2 px |
| excess | `abs(M_e and not dilate(M_o, 0.15 tile)) / abs(M_e)` | <= 5 % (`edge_threshold` 0.5); area ratio `abs(M_e)/abs(M_o)` <= 1.25 for soft-wide |
| edge softness (Q1b) | share of boundary pixels with coverage gradient above a hard-edge threshold, enhanced vs the raster-only edge | not more than +10 % relative |
| piece graph (Q1c) | components per piece-graph component, bridges needed per island | 1 each; every other case listed (report, not silent) |
| tier statistic | wet corners at exactly `z + 10` | all wet |

The S2 stage records today's baseline (16.9 % tile-space, ~28 % at group 1 on the canyon) with the same utility before the fix, expecting it to fail.

## 6. Interactions

| Feature | Behaviour |
|---|---|
| **Mouths and the sea blend** | `blend_rivers_into_sea` and the `kb` mix over `kRiverBlend` (16 px) are untouched. The raster treats mouth pieces and sea contact as Dirichlet outlets, so the flow is continuous up to the sea at `V_out`; flow shading is multiplied away by `kb` because the sea look has no advected layers, so the river visibly enters the sea at ~0.9 tiles/s and slows into the sea look. Checked by Q8 (pixel diff identical outside river + blend zone; inside the blend zone the sea component is bit-identical to the old) |
| **Falls (2702)** | tile-space mask by forward projection of the sprite (R1); level ramp between the two tiers (R4); `rho_f` raises the speed to `v_fall`; supercritical there, so standing waves (if on) and the Fr streaks appear only at falls; the water-plane pattern coordinate is continuous across the ramp (Q10). Pixel-space falls mask unchanged |
| **Rocks and objects in water** | rocks selected by `obstacle_ids` become holes and wake sources (2.6); `object_in_water` gets the flow at the object's foot so its ripple is advected along `u` (S7, optional); the "enhanced water meets objects" test in `gpu_test.cpp` must keep passing |
| **Wave simulation** | independent: river pixels use `fluid_shade` only for `kb < 1`. No momentum injection at mouths (D15, off) |
| **Original water** | `flags & 2` pixels are ignored by the original path; all digests of original-water views stay bit-identical in every stage |
| **Legacy shimmer** | `--river-flow legacy` (viewer key `R`) keeps `river_shade` bit-identical |
| **Rust reference** | frozen, gets nothing |

## 7. Frame spikes (owner finding 3, tag fix)

### 7.1 Measured attribution (M)

`bench_frame game/app game/app/Scenes/0200Single/Single_01.sze --renderer gpu --chunks async --styles enhanced --res 2560x1369 --groups 0,1,2 --quick --threads 16`; "cold" = first frame of a zoom group in a shared `GpuFrame`.

| Group | cold frame | uploads | encode (contains `update_fluid`) | warm p50 | static p99 with chunks arriving |
|---|---|---|---|---|---|
| 0 | 138.2 ms | 15.7 | 116.6 | 0.6 | 11.8 |
| 1 | 87.3 | 0.8 | 85.4 | 0.6 | 11.1 |
| 2 | 113.4 | 0.7 | 110.8 | 0.6 | 10.3 |
| 4K (3840 x 2160), groups 0 / 1 / 2 | 216 / 189 / 227 | | 194 / 185 / 221 | 1.0-1.5 | |

With original water the same cold frames cost 2.1-2.4 ms. The cost is linear in window cells (60 ns/cell wall time at 1.48 M cells and at 3.2 M cells), which pins it on the fill, not on texture creation. Mechanism (C): on a group change `FluidGpu::update` builds a new window and `fill` evaluates `Scene::fluid_depth` (16-tap B-spline + 2 noises) and `reset_cell` (8 plane waves) for 1.0-1.5 M cells, spawning `hardware_concurrency` `std::jthread`s and **joining them on the window thread**, builds the upload bytes with per-byte `push_back` (~30 M calls) and writes ~26 MB; the `(hw+1)/2` chunk workers started by the same group change oversubscribe the 16 hardware threads. Secondary: `upload_chunk` packs up to 6 chunks per frame on the window thread (5-12 ms bursts, 5 texture creations per chunk); `ensure_sea` uploads all 16 steps of the current group's sea pattern blocking (15.7 ms at group 0; the pattern build itself is 327 / 77 / 18 ms for groups 0 / 1 / 2 on first use).

### 7.2 Design (each item independent, in this order)

- **F1a window off the window thread.** A `WindowBuilder` job on the worker pool computes `(depth, state)` of the new window into a CPU array (generation counter, stale results dropped, work done into preallocated buffers by row `memcpy`, no per-byte `push_back`). The window thread uploads it **time-sliced** (64 rows per frame, `wgpuQueueWriteTexture` of row bands) into a **second grid** (double buffer) and swaps when complete. Until then `fluid_on = false` and the analytic `wave_shade` path in `sea_fs` (already the fallback where the window does not cover a pixel) is drawn; a uniform `fluid_mix` cross-fades analytic to simulated over 250 ms after the swap (option D8 `crossfade` | `snap` | `hold`; `hold` keeps drawing the previous group's window by nearest sampling). The cross-fade uniform never touches the original-water path. The window of the previous group is kept alive until the new one is ready (VRAM +47 MB at 1.1 K^2 cells; on an integrated GPU it is released at once and about 100 ms of analytic waves is accepted). Recentre strips (74 K cells, 3-4 ms) use the same job path. Neighbour-group windows are prefetched when the neighbour chunk prefetch starts (`request_chunks` already has that trigger).
- **F1b (optional, S9):** fill on the GPU. `reset_cell` is a pure function; the HEIGHT grid of each island is uploaded once as `R8Uint`, one compute dispatch fills the new rectangle (~0.05-0.2 ms, no per-cell upload). The CPU `FluidGrid::fill` stays the oracle (agreement within 1e-4).
- **F2 uploads.** Chunk buffers (BGRA ground, RGBA objects, RGBA8 field, u16 terrain, flow lattice) are packed **in the chunk worker** (`Chunk::gpu`), so the window thread only calls `wgpuQueueWriteTexture`; budget in **bytes per frame** (default 6 MB, config) instead of 6 chunks, so river chunks do not cost more than others; textures come from a free-list pool instead of 5 creations plus a bind group per chunk; sea pattern steps of the current group upload 2 per frame and the frame draws with the steps present (`steps` uniform) until complete.
- **F3 scheduling.** Chunk workers at below-normal priority (`SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL)` / `nice`), total worker threads <= `hw - 2`; the 16-thread `fill` on the window thread is removed.
- **F4 instrumentation (S0, first).** `GpuStats` gets `fluid_ms` (split `window_build_wait`, `upload`, `encode`), `sea_ms`, `sync_ms`, `chunk_bytes`; the viewer prints one attribution line for every frame above 8 ms (`spike 102.1 ms = fluid.fill 96.0 (1.2M cells, 16 thr) + sea.upload 0.1 + chunks 4.3 (6, 28 MB)`) so the owner can send logs from any machine; `bench_frame --zoom-script 2,0,1,0,2 --dwell 150`.
- **Never block on content.** A missing chunk draws open sea (current) or, with D9, the nearest built zoom group as a stand-in; a chunk whose phase B is not ready draws the legacy river look (3.1); a window that is not ready draws analytic waves.

### 7.3 Acceptance (numeric, measured on the owner's real sequence)

Zoom script 2 -> 0 -> 1 -> 0 -> 2 with chunk bursts of 8-28 chunks, at 2560 x 1369 **and** 3840 x 2160: window-thread time of the zoom frame <= 3 ms; p99.9 window-thread frame time over the run <= 4 ms; zero blocking waits (`fluid_ms` on the window thread <= 1 ms in every log line); a test with a deliberately slow depth function and slow chunk packing shows stale generations are discarded and the converged window equals the old synchronous result within 1e-6. Baseline above (85-117 ms; 185-221 ms at 4K) is the permanent regression row.

## 8. CPU reference path and parity

- `iso::river_flow_shade(pos, f, depth, slope, time_ms, detail, params)` in `iso.cpp` is the line-by-line twin of the WGSL function: same operation order, shared `noise3`/`smoothstep`, `-ffp-contract=off` (project rule), periods dividing 3,600,000 ms. `sea::shade_sea` takes the lattice through an accessor `flow(i)` beside `field(i)`; `FrameRenderer::render` passes `Chunk::flow`.
- The lattice is **data**: the CPU and the GPU read the same bytes, and the solve has no GPU twin, so parity risk is limited to the bilinear arithmetic, the noise/hash and the time handling. Convention (both paths): texel `(i, j)` at scene pixel corner `(cx*512 + i*stride, cy*512 + j*stride)`; a pixel samples at its centre `(px + 0.5)`; weights `(1-a)(1-b)` etc. multiplied by `valid`, normalised by their sum; zero valid weight falls back to the legacy shimmer for that pixel.
- Apron decision (before S5, no longer open): the chunk's probe render is **grown by one stride on the right and bottom** (+3 % of the ground pass) so apron texels come from the same probe; neighbouring chunks therefore hold bit-identical texels. S5 measures the cost (acceptance <= +5 % per river chunk).
- `RiverConfig::enabled = false` / `--river-flow legacy` keeps `river_shade` and its digests bit-identical.
- **Digest policy** (per stage, documented up front): original-water digests never change in any stage; enhanced digests of scenes with rivers change in S3 (mask) and S5 (shading) only, re-recorded once each with the justification in the commit, as the 2026-10-18 exception in `render_corpus_test.cpp`.
- **Tolerances (Q2):** river pixels `abs(GPU - CPU) <= 2/255` on >= 99.9 % of pixels, max <= 4, groups 0/1/2 at three times; river pixels have no foam and no 4/2/2 shading grid, so the tight bound is realistic; all other pixels keep today's `--check` bounds.
- **Oracle (language-neutral, CLAUDE.md):** `anno-dump river` prints per island the piece counts, wet-corner statistics, mask component and piece-graph component counts and a hash of the 1/8-tile mask; `tools/river.py` (starting from the prototype emulations of this study) is the Python reference; `tools/agree.py` compares them. The flow solve is checked by analytic references in the tests (straight channel Manning, taper, confluence flux) and by invariants, not by a Python solver.

## 9. Performance budget

| Item | Budget / estimate | Basis |
|---|---|---|
| Window-thread work at a group change | 85-117 ms to <= 3 ms (4K: 185-221 ms) | 7.3 |
| Chunk burst frames | 5-12 ms to <= 1.5 ms per frame | byte budget + worker packing |
| `RiverMask::build` (phase A) | <= 100 ms per island (E, measured in S3) | 2.5 M fine cells, forward projection 0.7 M samples |
| `RiverFlow::build` (phase B) | budget 1 s hard, target 0.3 s per island, **measured on all 72 river islands in S4 before S5 starts**; islands in parallel, off the window thread | E: ~25-100 K hydraulic cells, nested PCG |
| Memory | raster 1-5 MB per island; lattice 133 KB per river chunk (stride 4), none for others | |
| Chunk worker | +0.3 ms per river chunk for the lattice bake (E) | 129^2 texels x ~60 flop |
| GPU per river pixel | ~900 ALU (E): ~0.03 ms per 1 M river pixels on the RTX 4090, **about 2 ms per 1 M on the Ryzen iGPU** (E) | |
| Default `RiverQuality` | High on a discrete adapter; on an integrated one High only if the measured river pass is <= 1.0 ms per 1 M river pixels (timestamp queries), else Medium | the owner's 500 Hz monitor (2 ms frame) is driven by the iGPU; measure both devices in S6 |
| CPU reference | ~1 us per river pixel, split over row bands; a fallback and oracle, not a target | |

Stutter is avoided by construction: no runtime simulation, no per-frame CPU work, all heavy work in workers behind generation counters, uploads budgeted in bytes, nothing waited for on the window thread, and a defined cheap look for every "not ready" state.

## 10. QA method

All new tests are doctest cases (each its own CTest test: `ctest -j 16` with `ANNO_TEST_THREADS=16`); synthetic tests build `sea::IslandWater` / `river::RiverMask` / `RiverFlow` directly and run in CI without game files; corpus tests read `ANNO1503_GAME_DIR` and skip without it. Reports and images go to `build/river_qa/` or `cache/qa/river/` (git-ignored), never into git; `tools/river_qa.py` produces contact sheets and metrics tables.

### 10.1 Unit tests (CI, synthetic)

| Case | Check |
|---|---|
| straight channel | `u` along the axis within 2 degrees; mid-channel speed uniform (3 %); bank profile follows E6; flux through 5 cross-sections agrees within 1 %; `u` matches Manning `k_M R^(2/3) S^(1/2)` with the solved `S` within 2 % |
| taper / widening | `u * width * depth` constant within 5 %; `u` rises in the narrowing, ratio within 5 % of the area ratio |
| Y confluence, braid | `Q_main = Q_a + Q_b` within 1 %; in a braid `u` points downstream in both arms (an Eikonal field fails this) |
| 90-degree bend, meander | streamlines follow the banks (`u . n` ~ 0 at bank cells), **no corner cutting**; limitation documented: with `thalweg_shift` off the maximum speed is at the inner bank, with it on at the outer bank; cross-section mean unchanged |
| obstacle | `u = 0` in the hole, speed at the shoulders > free stream, stagnation upstream, far field within 15 % of E15 at 3 radii |
| falls (tier step) | no intermediate level; `u` downhill on >= 99 % of cells with `S > kRapidsFrom`; `Fr` supercritical only there |
| flat bed blockiness | a 1-tile bed between banks of 48 and a tier-32 neighbour, with the data's 4-unit steps and 3 x 3 blocks: no cell-to-cell speed jump above 10 %, never dry, never a gap (G4 regression) |
| thin river | 1-tile river at groups 0, 1, 2: no pulsing artefact, shear fade effective (Q10) |
| pond | no NaN, swirl bounded, `u` finite |
| mask | beds at exactly `z + 10` are wet; 1-tile bed between banks of 48 stays wet (B-spline regression); tier boundary has no intermediate level |
| determinism | two builds bit-identical, 1 vs N threads identical |
| fallback | forced non-convergence returns the Eikonal field, `fell_back` set, logged, no crash; forced `try_get() == null` builds the chunk with the legacy look and re-queues it |
| lattice | apron texels of two neighbouring chunks bit-identical; `flow_at` equals direct `sample` at lattice points within quantisation; a `valid = 0` texel does not drag the vector toward zero |
| shading | determinism; boundedness (`abs(light) <= 0.2`); **Galilean**: uniform flow, one layer, frozen phase weights, `I(x + u dt, t + dt) = I(x, t)` within 1e-4; **pulsing**: mean brightness over one period varies < 2 %; **hour wrap**: `t = 3,599,999` and `t = 0` give the same phases |
| config | every `period_ms` divides 3,600,000; `count <= kMaxRiverLayers`; lattice strides divide the chunk size |

### 10.2 Corpus and pixel-versus-pixel (game files)

| # | Test | Method | Pass |
|---|---|---|---|
| Q1 | original vs enhanced river mask | 5.3, all 72 river islands x groups 0/1/2, chunk by chunk through `Scene::water_field`; overlays red = original-only (gap), green = enhanced-only, grey = both | 5.3 table |
| Q2 | CPU vs GPU | `bench_frame --check` extended with river-only statistics (`flags & 2`), 3 groups x 3 times, lattice seams included | 8 |
| Q3 | zoom consistency | same tile centre at groups 0/1/2 (`--at x,y` is in world tiles): water masks rescaled to a common size, IoU >= 0.9; lattice velocity at 200 sampled tile positions equals `RiverFlow::sample` within quantisation; temporal std of light at the same tile position within 15 % between groups | |
| Q4 | chunk seams | `Scene::water_field` over A, B and A union B (offset by an odd number of pixels): cover/depth/flags byte-equal on the overlap (exception: the 4-px falls blur border until the apron lands); lattice texels **bitwise** equal; shaded pixels identical on CPU, within 1/255 on GPU; a frame at view V and V + (3, 5) shifted back agree outside the 1 px boundary | |
| Q5 | flow direction vs motion | block-matching optical flow (16 x 16 blocks) between frames `t` and `t + 50 ms` on river pixels away from the edges, plus phase-correlation along and across the flow with negative controls (across and a still pond give ~0); overlay arrows on the analytic field in `cache/qa/river/*_quiver.png` | median angle error <= 15 degrees, median speed ratio in [0.7, 1.3] at the main stems of `D_256x256_0045_5B`, `N_256x256_0006_2B`, `S_256x256_0004_3B_Orient` and the `Single_01` mouth with rocks; flow coverage >= 99.9 % of river pixels |
| Q6 | contact sheets (human review) | columns: original / legacy enhanced / new enhanced / difference over 800 ms / quiver. Rows: islands x groups 0/1/2 x times {0, 400, 800, 1600 ms}; the three discharge models, the two edge widths, thalweg on/off, standing waves on/off | owner sign-off per taste option |
| Q7 | performance | `bench_frame` rows with rivers (`--island`, `--at` on the river region) for original / legacy / new; chunk-burst frames (28 chunks); iGPU and dGPU | 9 |
| Q8 | faithful water unchanged | original digests identical; enhanced sea pixels outside river + `kRiverBlend` byte-identical; inside the blend zone the sea component bit-identical | |
| Q9 | spike regression | zoom-script rows at 2560 x 1369 and 3840 x 2160 | 7.3 |
| Q10 | water-plane continuity and shear | a pattern feature advected across a tier boundary at a fall stays continuous (displacement matches `u` on both sides within 8 %); thin-river and bend shear test at group 2 (lattice stride 4 px of a 20 px wide river) | |
| Q11 | meander and braid fixtures on real data | the synthetic cases of 10.1 on the 6 QA islands, plus the named cases: braids with equal arms and 1-cell necks of the corpus | |
| Q12 | solver corpus | `RiverFlow::build` of all 72 islands: residual, NaN, iterations, time and memory per island, fallbacks and sea-less components listed | no NaN, residual <= 1e-3, p95 time <= 0.3 s, none above 1 s |

**Islands (rivers inland, different climates):** `Dschungel/D_256x256_0045_5B` (canyon with falls, the owner's screenshot; 184 pieces, 22 falls), `Nord/N_256x256_0006_2B` (250 pieces, mouths), `Nord/N_256x256_0014_2B_Piraten`, `Steppe/S_256x256_0004_3B_Orient`, `Tundra/T_256x256_0008_2B_Po` (6 falls, snow) and `T_256x256_0005_1B_Po`, `Praerie/Pr_128x128_0003_1B`, `Dschungel/D_064x064_0001_0B` (small), and `0200Single/Single_01.sze` (4 river islands, mouths into the sea, ships and rocks). Groups 0, 1, 2; three crops each (source, fall, mouth).

**Commands (read-only; reproduce the numbers of this study):**

    anno1503 --view game/app/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp --game-dir game/app --group G --water original|enhanced --at 144,76 --size 2000x1200 --time MS --png out.png
    anno1503 ... --renderer cpu            (CPU path; the GPU pair comes from bench_frame --check dumps with BENCH_DUMP)
    bench_frame game/app game/app/Scenes/0200Single/Single_01.sze --renderer gpu --chunks async --styles enhanced --res 2560x1369 --groups 0,1,2 --quick --threads 16
    anno1503 ... --river-debug gap|flow|turb   (S2/S5: gap overlay, hue = direction and value = speed, turbulence)

## 11. Staged implementation plan

Every stage builds on all CI targets with warnings as errors, keeps original-water digests identical, passes `ctest`, is clang-format 23.1.3 clean, ASan/UBSan clean on the new modules, and updates `iso.md`, `docs/PLAN.md` and `docs/open-questions.md` (new questions: rotation as flow direction, obstacle categories and `WALKMASK`, sea-less outlets, discharge). Rust code gets nothing. Each stage ships on its own; the first two owner-visible fixes (spikes, gaps) land before any flow work.

| Stage | Goal | Acceptance (measured by the QA harness) |
|---|---|---|
| **S0** instrumentation, no behaviour change | `GpuStats` fields, spike attribution line, `bench_frame --zoom-script`, cold group-switch rows at 2560 x 1369 and 3840 x 2160, baseline recorded | stats monotone and sum to `total_ms` within 5 %; baseline table of 7.1 reproduced within 10 % |
| **S1** frame-spike fix (F1a, F2, F3) | window builder + time-sliced upload + double buffer + analytic fallback and cross-fade; worker-packed chunk buffers, byte budget, texture pool, sea-step slicing; thread budget and priority | 7.3: zoom frame <= 3 ms, p99.9 <= 4 ms at both resolutions with 28-chunk bursts, zero blocking waits; slow-depth-function test; stale generation dropped; converged window within 1e-6 of the old result; existing solver/resize cases still pass |
| **S2** gap metric and oracle, no render change | `tools/river.py`, `anno-dump river`, `agree.py`, `tools/river_qa.py`, the Q1 metric utility and `--river-debug gap`; baseline recorded | `agree.py` agrees for the river command over all 465 templates; Q1 utility reports the recorded baseline (expected to fail: ~28 % at group 1 on the canyon, 16.9 % in tile space) |
| **S3** gap fix | `RiverMask` (R1-R4), per-piece levels, forward-projected falls, sprite union, falls-mask apron if Q4 confirms the seam; `water_field` river branch reads the mask | Q1 (incl. Q1b, Q1c) passes on all 72 islands x 3 groups; Q4 passes; unit mask cases; phase A <= 100 ms per island; original digests unchanged; enhanced digests re-recorded once with the justification |
| **S4** (done 2026-10-19, §14) hydraulics, CPU only, no render change | `RiverFlow`: linear conveyance solve with bank closure, nested PCG, Eikonal fallback, discharge options, falls speed, ponds, bed smoothing; corpus build | all 10.1 flow cases; Q12 on all 72 islands with the 1 s hard budget **before S5 starts**; fallbacks and sea-less components listed; determinism (1 vs N threads) |
| **S5** (done 2026-10-19, §15) lattice + CPU shading (first owner-visible flow) | `Chunk::flow`, `flow_at`, grown-probe apron, `iso::river_flow_shade`, `shade_sea` accessor, layer-table uniform contract on the CPU side, water-plane coordinates, `--river-flow`, `flow_pending` path; flow default **on** for Enhanced | owner sees flow along the course on `--png --time`; Galilean, pulsing, hour-wrap, thin-river cases; Q3 (CPU), Q4 lattice bitwise, Q5 on the CPU path (angle <= 15 degrees, speed ratio [0.7, 1.3], coverage >= 99.9 %), Q10; apron cost <= +5 % per river chunk; forced-pending test; enhanced digests re-recorded |
| **S6** (done 2026-10-19, §16) GPU port and quality | bindings 6 to 8, flags, uniform, WGSL, shader validation, dummy textures, `RiverQuality`, adapter-based default, timestamp queries | Q2 (99.9 % within 2/255, max 4); `errors() == 0`; river-pass cost per 1 M river pixels measured on the RTX 4090 and the Ryzen iGPU with the default chosen by the 1.0 ms gate; no regression of the Q9 rows |
| **S7** hydraulic extras | turbulence channel, obstacles and wakes (`obstacle_ids` reviewed on contact sheets), Froude streaks and aeration tint, standing waves (off), thalweg shift (off), Picard refinement (off, guarded), `object_in_water` along the flow, mouth fade check | obstacle/wake/Fr/bend unit cases; Q6 sheets for rocks and falls reviewed; Q8 mouth blend zone bit-identical; "enhanced water meets objects" still passes |
| **S8** options, tuning, docs | config plumbing, viewer key `R`, `--river-flow`, perf table in `docs/perf/README.md`, tuning pass with the owner against D1-D6, calibration of light variance to the legacy shimmer | Q6 sign-off recorded per taste option; Q7 table complete; PLAN status updated |
| **S9** optional | F1b GPU window fill, noise-texture shading variant, coarse-group stand-in chunks (D9), Tier 1 dynamic window (D16) | measured gain on the Q7/Q9 rows before any of them lands |

## 12. Taste options for the owner

Rule (owner): **fix** = an obvious error, just remove it; **decision-needed** = the default is the current or faithful behaviour behind an option and the owner picks. "Default" is what the implementation does until the owner decides.

| # | Tag | Item | Default | Where |
|---|---|---|---|---|
| F-a | fix | River level 9 against tolerance 10: dry bed corners at exactly `z + 10` (26.5 % of the original-wet corners) = the gaps | `tau + 0.5`, per-piece exact test | `sea.hpp kRiverLevel`, `sea.cpp:246-381` |
| F-b | fix | Level averaged across tiers (levels between tiers at falls) | per-piece level, linear ramp across falls | `sea.cpp:246-284` |
| F-c | fix | Mask from the approximating B-spline plus shore noise | exact linear terrain, tile-space smoothing | `sea.cpp:349-381` |
| F-d | fix | Falls/mouth blur clamps at the chunk rectangle (suspected, Q4 decides) | 4-px apron | `scene.cpp:275-303, 406-441` |
| F-e | fix | Wave-simulation window rebuilt synchronously on every group change (85-117 ms; per-byte `push_back`) | worker build, time-sliced upload, double buffer | `gpu_fluid.cpp:243-381`, `gpu.cpp:760-786` |
| F-f | fix | Chunk packing and 5 texture creations per chunk on the window thread (5-12 ms bursts) | worker packing, byte budget, texture pool | `gpu.cpp:383-510` |
| F-g | fix | Sea pattern: current group uploaded blocking (15.7 ms at group 0) | 2 steps per frame, draw with what is there | `gpu.cpp:329-381` |
| F-h | fix | 8 chunk workers plus a 16-thread fill compete for 16 hardware threads | priority and `hw - 2` budget | `viewer.cpp:295`, `gpu.cpp:780` |
| F-i | fix | River noise on the sea plane: pattern offset by up to ~117 px on high tiers and jumping across falls | river's own water plane (E19) | `iso.cpp:686`, `gpu.wgsl:313` |
| F-j | fix | Blocky speed from the quantised painted bed through `h^(5/3)` | masked bed smoothing (G4) | `river.cpp` |
| F-k | fix (minor, shared) | `time_ms % 3,600,000` makes every animation jump once per hour; new periods divide it, old ones do not | document; 64-bit-safe phase later | `gpu.cpp:740` |
| D1 | decision-needed | Speed scale and bank profile: `V_out 0.9`, `v_floor 0.12`, `v_cap 3.0` tiles/s; `a_b = 4`, `delta_b = 0.3` | as given | `RiverConfig` |
| D1b | decision-needed | Picard refinement of the Manning form | off (linear conveyance), guarded when on | `RiverConfig::picard_iterations` |
| D2 | decision-needed | Discharge model: Normalised (every river `V_out` at its mouth) / Absolute (long rivers faster) / Sourced (inflow at dead ends) | Normalised | `river_solve.cpp` |
| D3 | decision-needed | Standing waves (Froude light bands at falls and fast reaches) | off | `RiverConfig::standing_waves` |
| D4 | decision-needed | Which objects are flow obstacles (no category data) | footprint rule AND short rock/cliff id list reviewed visually | `RiverConfig::obstacle_*` |
| D5 | decision-needed | Whitewater at falls and rapids (owner 2026-10-18: no foam on rivers) | no foam; high-frequency streaks and aeration tint, never white | shader |
| D6 | decision-needed | River width and edge: original width (`edge_threshold 0.5`) or soft-wide (0.40, fills shallow lobes) | original width | `RiverConfig::edge_threshold` |
| D7 | decision-needed | Ponds and lakes: tiny static swirl so they are not frozen | on, 0.03 tiles/s | `RiverConfig::pond_swirl` |
| D8 | decision-needed | After a zoom change: analytic waves, then 250 ms cross-fade / snap / hold the previous window | cross-fade | F1a |
| D9 | decision-needed | Missing chunks: open sea (current) or the nearest built zoom group as a stand-in | open sea | `frame.cpp`, `gpu.cpp` |
| D10 | decision-needed | Legacy shimmer available as `--river-flow legacy`; flow on in Enhanced (the owner asked for it) | flow on | `cli.cpp`, viewer |
| D11 | decision-needed | `RiverQuality` Low / Medium / High | High on discrete, gated by the measured iGPU cost otherwise | `GpuLimits` |
| D12 | decision-needed | Thalweg shift toward the outer bank of bends (E14) | off (inner-bank maximum documented and tested) | `RiverConfig::thalweg_shift` |
| D13 | decision-needed | Gaps in the original data (piece pairs whose beds never reach the level) | log and list, never patch | `RiverConfig::bridge_gaps` |
| D14 | decision-needed | Outlet of a river component with no sea contact | lowest level, then lowest bed cluster, listed per island | `river_solve.cpp` |
| D15 | decision-needed | River plume: momentum from mouths into the wave simulation | off (render-only) | `fluid` |
| D16 | decision-needed | Tier 1: dynamic shallow-water window over the same lattice contract | not built | later |

## 13. Risks and open questions

| Risk | Mitigation |
|---|---|
| Flat data: every speed is a model output (discharge, `V_out`, floor) | labelled "plausible, not original"; D1/D2 with contact sheets; the only evidence-backed parts are the data statistics and the original's depth test |
| Solver on long thin networks, 1-cell necks, equal braids; build time is an estimate | nested PCG, Eikonal fallback made visible (log, `FlowStats`, `--river-debug`), named corpus cases (Q11), budget gate in S4 before S5 |
| Flow-map pulsing and shear at narrow bends and group 2 (20-60 px wide rivers, 4 px stride) | `psi`, variance-preserving blend, shear fade, thin-river test at all groups (Q10); stride tightens (to 2 px at group 2) if the test fails |
| Probe discontinuities at cliff silhouettes | valid-weighted bilinear, extension band; checked on the canyon scene (Q6) |
| Sprite union brings back hard edges in narrow spots | minimum-coverage clamp with soft falloff, edge-softness test Q1b |
| Shader cost on the iGPU that drives the 500 Hz monitor | `RiverQuality`, measured gate, noise-texture variant (S9) |
| Layer count and strides as fixed limits (CLAUDE.md "no hardcoded limits") | uniform layer array with `kMaxRiverLayers = 8` (reason and path to raise documented in 3.3), strides derived per zoom group and carried per chunk, int16 velocity range `velocity_max` in config; the 13-texture layout leaves headroom of 3 |
| Visible transient (analytic waves, legacy river look) while data is late | D8, prefetch, rare by design; tested |
| Open: rotation (0-3) or animation steps of 2701 encode a flow direction? obstacle categories and `WALKMASK` meaning; original behaviour of river sprites vs the wave window | `docs/open-questions.md` entries added in S2/S4/S7 |

### 13.1 OQ-068 gap patch survey over all river islands (T6, 2026-10-20)

The gap patch (`RiverConfig::gap_patch_tiles`, viewer default 8, library default 0, `sea::patch_river_gaps`) was judged on the canyon island only. This is the same measurement on every island template that has river, waterfall or river-mouth pieces: `river_qa --per-island --gap-patch 0` against `--gap-patch 8`, groups 0, 1 and 2, through the viewer's own chunk path (`Scene::ground_with_probe`, `Scene::water_field_from`); the definitions are in the header of `src/tools/river_qa/main.cpp`. No default was changed; this is a survey.

**Reproduce** (about 20 minutes on the 9800X3D at 16 threads; `--cache` keeps the measured rows so that table and sheets can be redone at once):

```
river_qa game/app --groups 0,1,2 --per-island --gap-patch 8 <scene.scp>...     # one measurement
python tools/river_qa.py gap-patch --exe build/msvc/src/tools/river_qa/Release/river_qa.exe \
    --game-dir game/app --patch 8 --cache rows.json --table survey.md --sheets <dir>
python tools/test_river_qa_gap.py --exe build/msvc/src/tools/river_qa/Release/river_qa.exe
```

**Columns.** `E comps`: 8-connected components of the enhanced river water that touches the original's river sprites (the river networks as drawn; fewer means pieces were joined). `gap px`: original river-sprite pixels with no enhanced water within a few pixels (the owner's "brown bed, no water"). `extra px`: enhanced river water where the original draws no river sprite (water outside the painted bed); `extra growth` is the gain of `extra px` as a share of the island's original river pixels (`O px`). `edge px` (summary only): gap pixels with water within a few pixels, which the `gap px` count ignores. `E comps` counts per island (all river networks of the island together), not per network. Each cell is `patch 0 -> patch 8`. A **regression** is: more components, more split or lost sprite components, more gap pixels, or `extra px` grown by more than 25 % of the island's original river pixels (`EXTRA_GROWTH_FRACTION` in `tools/river_qa_gap.py`: a survey flag with no evidence behind the 25 %, not a game rule; the largest growth measured is 19.5 %, so it cannot fire. Read the `extra growth` columns instead; the rules are unit-tested in `tools/test_river_qa_gap.py`).

**Result over the islands that have river pieces** (the cost of the patch, water outside the painted bed, comes first):

```
scenes: tried 444, loaded 444, failed 0, with river pieces 72
islands with river pieces: 72
g0 water outside the painted bed, share of original river px: 6.5 % -> 17.5 % (largest growth 19.5 %, N_128x128_0005_1B_Piraten i0)
g1 water outside the painted bed, share of original river px: 5.8 % -> 15.3 % (largest growth 17.1 %, N_128x128_0005_1B_Piraten i0)
g2 water outside the painted bed, share of original river px: 4.7 % -> 11.4 % (largest growth 12.4 %, N_128x128_0005_1B_Piraten i0)
islands with a regression (patch 8 against 0): 0
g0 O px total: 34915444 -> 34915444
g1 O px total: 9026661 -> 9026661
g2 O px total: 2203585 -> 2203585
g0 E comps total: 378 -> 354
g1 E comps total: 363 -> 339
g2 E comps total: 338 -> 316
g0 gap px total: 94 -> 0
g1 gap px total: 0 -> 0
g2 gap px total: 0 -> 0
g0 edge px total: 87702 -> 56359
g1 edge px total: 26083 -> 19738
g2 edge px total: 10297 -> 9420
g0 extra px total: 2254378 -> 6098976
g1 extra px total: 521198 -> 1384508
g2 extra px total: 102880 -> 250415
g0 split total: 1 -> 0
g1 split total: 0 -> 0
g2 split total: 0 -> 0
g0 lost total: 0 -> 0
g1 lost total: 0 -> 0
g2 lost total: 0 -> 0
```

**Findings.**

- **The cost is water outside the painted bed, on every island.** `extra px` grows on all 72 islands: from 5.8 % to 15.3 % of the original river pixels at group 1 (group 0: 6.5 -> 17.5 %, group 2: 4.7 -> 11.4 %), by 19.5 % of an island's original pixels at most (`N_128x128_0005_1B_Piraten`, group 0). In absolute pixels it grows 2.7 times over all islands at group 1 and up to 11.6 times on one (`N_256x256_0007_2B`: 3,125 -> 36,127 at group 1), because the original river pixels are the denominator and the painted bed is already large.
- **No regression by the other rules on any island.** No island gets more components, more split or lost sprite components, or more gap pixels.
- **The patch removes the gaps that are left.** Gap pixels fall from 94 to 0 at group 0 (`D_256x256_0003_4B` 23 px, `D_256x256_0013_4B_Azteken` 23, `D_256x256_0015_5B_Afrika` 23, `N_128x128_0022_1B` 3, `N_256x256_0003_2B` 9, `N_256x256_0006_2B` 7, plus scenario variants); groups 1 and 2 have none before or after (the count ignores pixels with water within `th/10 + 1` px; those "edge" pixels fall from 87,702 to 56,359 at group 0, 26,083 to 19,738 at group 1 and 10,297 to 9,420 at group 2). The one split sprite component (`D_256x256_0026_4B_Azteken`, group 0) is joined.
- **Components: 8 of 72 islands lose components, 64 are unchanged, none gains** (group 1: 363 -> 339; counted per island, all networks together). Most rivers were already one water body; the patch matters on a minority.
- **Where the extra water is** (sheets below): a thin fringe along the banks at every bend (the deepened trench corners reach about a pixel past the painted bed) and, on the largest growers, short extensions at the open ends of a river where the painted pieces stop: `N_256x256_0007_2B` and `N_256x256_0016_3B` show a blue tail at the source end (top) and at the lower-left end, a few tiles long at every group, that the unpatched water does not have (a few pixels at the top only). These are new water outside the original sprites, not new branches or pools. Whether this looks right is the owner's call, because it changes the picture (OQ-068).
- **Coverage.** 444 scenes tried (every `.scp` under `Scenes/Islands` except the 21 dummy islands), 444 loaded, 0 failed, 72 with river pieces (one island each, so 72 islands); the 21 dummy islands were measured once separately: 0 with river pieces. The task text said 106 islands; the 50 scenarios (.sze under `Scenes`) place 112 islands with river pieces in 36 scenarios (group 0, measured once with `river_qa --groups 0`), which is not 106 either; neither number was reproduced as 106, and the gap-patch survey covers the island templates only, not the placed instances (whether the placed islands match their templates was not checked).
- Variants of one template (Mongolen, Indianer, Piraten, Azteken, Afrika) repeat the same river and show the same numbers.

**Contact sheets** (seven, in `C:/Users/altin/Pictures/anno-review/T6/`, file names `<island>_i0_gap-patch-8.png`). Ranking rule of the first five: regression count, then remaining gap pixels (none are left after the patch), then the largest growth of `extra px` as a share of the original river pixels (the cost of the patch), then components beyond one per original component; islands with the same pixel and component counts in every group count once: `N_128x128_0005_1B` (19.5 %), `N_128x128_0049_1B_Indianer` (19.1 %), `D_256x256_0003_4B` (18.5 %), `N_128x128_0010_1B` (18.3 %), `N_128x128_0007_1B` (18.2 %). Two more show the largest absolute multiplication of `extra px` (11.6 and 8.9 times at group 1): `N_256x256_0007_2B`, `N_256x256_0016_3B`; they have the end tails described above. Left column patch 0, right column patch 8, one row per zoom group, cropped to the river (red = gap, green = original sprite with water, blue = enhanced river water only, orange = sprite without water).

**Open for the owner (OQ-068):** keep 8 as the viewer default (no gap pixels left, fewer split rivers, a slightly wider river at bends), choose a smaller `gap_patch_tiles`, or limit the trench deepening to the cells that are real gaps so that the fringe stays out (a render change, not part of this survey).

**Full table** (one row per island template, `patch 0 -> patch 8`):

| island | E comps g0 | E comps g1 | E comps g2 | gap px g0 | gap px g1 | gap px g2 | extra px g0 | extra px g1 | extra px g2 | extra growth g0 | extra growth g1 | extra growth g2 | regression |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| D_128x128_0006_3B i0 | 4 -> 4 | 4 -> 4 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 8240 -> 31672 | 1805 -> 7059 | 306 -> 1235 | 14.0 % | 12.1 % | 8.6 % |  |
| D_128x128_0022_1B i0 | 5 -> 5 | 4 -> 4 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 34666 -> 90078 | 7726 -> 20169 | 1390 -> 3575 | 13.8 % | 11.9 % | 8.4 % |  |
| D_128x128_0023_3B i0 | 17 -> 11 | 12 -> 7 | 10 -> 5 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 11792 -> 57716 | 2419 -> 12781 | 340 -> 2204 | 15.7 % | 13.5 % | 9.7 % |  |
| D_128x128_0024_4B i0 | 4 -> 3 | 5 -> 4 | 2 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 35566 -> 86171 | 8143 -> 19412 | 1551 -> 3419 | 11.8 % | 10.1 % | 6.8 % |  |
| D_128x128_0025_3B i0 | 4 -> 4 | 4 -> 4 | 4 -> 4 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 24401 -> 29431 | 5636 -> 6699 | 1176 -> 1288 | 3.8 % | 3.1 % | 1.3 % |  |
| D_128x128_0037_2B i0 | 7 -> 7 | 7 -> 7 | 6 -> 6 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 38293 -> 52395 | 9205 -> 12181 | 1902 -> 2317 | 3.3 % | 2.7 % | 1.5 % |  |
| D_128x128_0046_4B i0 | 4 -> 4 | 4 -> 4 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 3588 -> 11990 | 598 -> 2392 | 68 -> 288 | 3.5 % | 2.9 % | 1.4 % |  |
| D_128x128_0049_4B i0 | 6 -> 6 | 5 -> 5 | 4 -> 4 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 7196 -> 9715 | 1579 -> 2021 | 263 -> 287 | 1.0 % | 0.7 % | 0.2 % |  |
| D_256x256_0003_4B i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 23 -> 0 | 0 -> 0 | 0 -> 0 | 56054 -> 141193 | 11868 -> 31101 | 2283 -> 5750 | 18.5 % | 15.8 % | 11.5 % |  |
| D_256x256_0013_4B_Azteken i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 23 -> 0 | 0 -> 0 | 0 -> 0 | 56054 -> 141193 | 11868 -> 31101 | 2283 -> 5750 | 18.5 % | 15.8 % | 11.5 % |  |
| D_256x256_0015_5B_Afrika i0 | 7 -> 6 | 7 -> 6 | 7 -> 6 | 23 -> 0 | 0 -> 0 | 0 -> 0 | 67911 -> 180610 | 14981 -> 41168 | 2978 -> 7965 | 14.8 % | 13.3 % | 10.6 % |  |
| D_256x256_0026_4B_Azteken i0 | 2 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 12828 -> 88542 | 2645 -> 19616 | 393 -> 3322 | 11.0 % | 9.6 % | 6.9 % |  |
| D_256x256_0045_5B i0 | 16 -> 7 | 16 -> 7 | 15 -> 6 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 142657 -> 357469 | 33396 -> 82938 | 6539 -> 15666 | 16.8 % | 14.9 % | 11.0 % |  |
| N_128x128_0002_3B i0 | 9 -> 9 | 6 -> 6 | 3 -> 3 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 39240 -> 73060 | 8913 -> 16349 | 1642 -> 2604 | 7.7 % | 6.5 % | 3.4 % |  |
| N_128x128_0002_3B_Indianer i0 | 10 -> 10 | 9 -> 9 | 6 -> 6 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 51435 -> 92078 | 11858 -> 20780 | 2233 -> 3343 | 6.8 % | 5.8 % | 2.9 % |  |
| N_128x128_0005_1B i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 42549 -> 136272 | 10329 -> 31647 | 2103 -> 5880 | 19.5 % | 17.1 % | 12.4 % |  |
| N_128x128_0005_1B_Piraten i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 42549 -> 136272 | 10329 -> 31647 | 2103 -> 5880 | 19.5 % | 17.1 % | 12.4 % |  |
| N_128x128_0006_1B i0 | 5 -> 5 | 5 -> 5 | 6 -> 6 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 30612 -> 53970 | 7421 -> 12703 | 1583 -> 2461 | 10.3 % | 9.0 % | 6.1 % |  |
| N_128x128_0006_1B_Mongolen i0 | 7 -> 7 | 7 -> 7 | 7 -> 7 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 46691 -> 119099 | 11121 -> 27528 | 2271 -> 5120 | 13.4 % | 11.7 % | 8.3 % |  |
| N_128x128_0007_1B i0 | 9 -> 9 | 9 -> 9 | 8 -> 8 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 41801 -> 112627 | 9998 -> 26021 | 2089 -> 4913 | 18.2 % | 15.8 % | 11.3 % |  |
| N_128x128_0007_1B_Mongolen i0 | 9 -> 9 | 9 -> 9 | 8 -> 8 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 41801 -> 112627 | 9998 -> 26021 | 2089 -> 4913 | 18.2 % | 15.8 % | 11.3 % |  |
| N_128x128_0010_1B i0 | 8 -> 8 | 7 -> 7 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 27556 -> 97301 | 6315 -> 22204 | 1183 -> 4089 | 18.3 % | 16.0 % | 12.0 % |  |
| N_128x128_0010_1BMongolen i0 | 8 -> 8 | 7 -> 7 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 27556 -> 97301 | 6315 -> 22204 | 1183 -> 4089 | 18.3 % | 16.0 % | 12.0 % |  |
| N_128x128_0014_1B i0 | 13 -> 13 | 16 -> 16 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 26007 -> 48389 | 6222 -> 11301 | 1313 -> 2177 | 12.6 % | 10.9 % | 7.4 % |  |
| N_128x128_0014_1B_Indianer i0 | 13 -> 13 | 16 -> 16 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 26007 -> 48389 | 6222 -> 11301 | 1313 -> 2177 | 12.6 % | 10.9 % | 7.4 % |  |
| N_128x128_0015_1B i0 | 3 -> 3 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 35914 -> 81612 | 8658 -> 19174 | 1768 -> 3695 | 15.1 % | 13.5 % | 10.2 % |  |
| N_128x128_0015_1B_Piraten i0 | 3 -> 3 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 35914 -> 81612 | 8658 -> 19174 | 1768 -> 3695 | 15.1 % | 13.5 % | 10.2 % |  |
| N_128x128_0016_1B i0 | 8 -> 8 | 9 -> 9 | 23 -> 23 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 10847 -> 27327 | 2469 -> 5887 | 499 -> 922 | 3.7 % | 2.9 % | 1.5 % |  |
| N_128x128_0016_1BMongolen i0 | 8 -> 8 | 9 -> 9 | 23 -> 23 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 10847 -> 27324 | 2468 -> 5885 | 501 -> 924 | 3.7 % | 2.9 % | 1.5 % |  |
| N_128x128_0017_3B i0 | 5 -> 5 | 3 -> 3 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 11571 -> 24403 | 2473 -> 5059 | 422 -> 634 | 2.1 % | 1.7 % | 0.6 % |  |
| N_128x128_0017_3B_Indianer i0 | 5 -> 5 | 3 -> 3 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 11571 -> 24403 | 2473 -> 5059 | 422 -> 634 | 2.1 % | 1.7 % | 0.6 % |  |
| N_128x128_0019_2B i0 | 13 -> 13 | 9 -> 9 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 7825 -> 27042 | 1695 -> 5653 | 354 -> 776 | 5.0 % | 4.0 % | 1.8 % |  |
| N_128x128_0019_2B_Indianer i0 | 13 -> 13 | 9 -> 9 | 9 -> 9 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 9025 -> 28248 | 1929 -> 5887 | 375 -> 798 | 5.0 % | 4.0 % | 1.8 % |  |
| N_128x128_0021_1B i0 | 8 -> 8 | 10 -> 10 | 7 -> 7 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 5939 -> 25697 | 1292 -> 5429 | 239 -> 705 | 5.1 % | 4.2 % | 2.0 % |  |
| N_128x128_0021_1B_Mongolen i0 | 8 -> 8 | 10 -> 10 | 7 -> 7 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 5939 -> 25697 | 1292 -> 5429 | 239 -> 705 | 5.1 % | 4.2 % | 2.0 % |  |
| N_128x128_0022_1B i0 | 2 -> 2 | 2 -> 2 | 1 -> 1 | 3 -> 0 | 0 -> 0 | 0 -> 0 | 12177 -> 29441 | 2512 -> 5933 | 458 -> 810 | 3.7 % | 2.8 % | 1.2 % |  |
| N_128x128_0022_1BPiraten i0 | 2 -> 2 | 2 -> 2 | 1 -> 1 | 3 -> 0 | 0 -> 0 | 0 -> 0 | 12177 -> 29441 | 2512 -> 5937 | 458 -> 810 | 3.7 % | 2.8 % | 1.2 % |  |
| N_128x128_0025_2B i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 17025 -> 22873 | 4055 -> 5258 | 797 -> 869 | 2.6 % | 2.1 % | 0.5 % |  |
| N_128x128_0029_2B i0 | 4 -> 4 | 5 -> 5 | 13 -> 13 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 16579 -> 24147 | 3848 -> 5387 | 762 -> 914 | 3.2 % | 2.6 % | 1.0 % |  |
| N_128x128_0029_2B_Indianer i0 | 4 -> 4 | 5 -> 5 | 13 -> 13 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 16579 -> 24147 | 3848 -> 5387 | 762 -> 914 | 3.2 % | 2.6 % | 1.0 % |  |
| N_128x128_0030_1B i0 | 3 -> 3 | 2 -> 2 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 3098 -> 9529 | 534 -> 1864 | 64 -> 206 | 2.5 % | 2.0 % | 0.9 % |  |
| N_128x128_0030_1B_Mongolen i0 | 3 -> 3 | 2 -> 2 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 3338 -> 9768 | 598 -> 1906 | 81 -> 217 | 2.5 % | 2.0 % | 0.8 % |  |
| N_128x128_0034_0B i0 | 5 -> 5 | 3 -> 3 | 3 -> 3 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 30665 -> 43123 | 7270 -> 9873 | 1535 -> 1764 | 3.0 % | 2.5 % | 0.9 % |  |
| N_128x128_0034_0BIndianer i0 | 5 -> 5 | 3 -> 3 | 3 -> 3 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 30665 -> 43123 | 7270 -> 9873 | 1535 -> 1764 | 3.0 % | 2.5 % | 0.9 % |  |
| N_128x128_0049_1B_Indianer i0 | 11 -> 11 | 10 -> 10 | 5 -> 5 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 63067 -> 133250 | 15378 -> 31215 | 3364 -> 6118 | 19.1 % | 16.5 % | 11.6 % |  |
| N_128x128_0050_3B_Indianer i0 | 5 -> 5 | 3 -> 3 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 11571 -> 24403 | 2473 -> 5059 | 422 -> 634 | 2.1 % | 1.7 % | 0.6 % |  |
| N_128x128_0068_2B_Piraten i0 | 2 -> 2 | 2 -> 2 | 1 -> 1 | 3 -> 0 | 0 -> 0 | 0 -> 0 | 12438 -> 28668 | 2554 -> 5791 | 459 -> 791 | 3.4 % | 2.6 % | 1.1 % |  |
| N_256x256_0002_2B i0 | 1 -> 1 | 2 -> 2 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 9584 -> 46824 | 1883 -> 10414 | 253 -> 1819 | 17.6 % | 15.4 % | 11.3 % |  |
| N_256x256_0003_2B i0 | 2 -> 2 | 3 -> 3 | 4 -> 4 | 9 -> 0 | 0 -> 0 | 0 -> 0 | 44622 -> 174750 | 9089 -> 38773 | 1597 -> 7092 | 15.1 % | 13.3 % | 10.2 % |  |
| N_256x256_0006_2B i0 | 7 -> 7 | 8 -> 8 | 10 -> 10 | 7 -> 0 | 0 -> 0 | 0 -> 0 | 29536 -> 58042 | 6390 -> 12168 | 1248 -> 1769 | 2.2 % | 1.8 % | 0.7 % |  |
| N_256x256_0007_2B i0 | 4 -> 2 | 6 -> 3 | 3 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 13078 -> 158508 | 3125 -> 36127 | 460 -> 6568 | 16.4 % | 14.5 % | 11.2 % |  |
| N_256x256_0011_2B_Mongolen i0 | 4 -> 4 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 38681 -> 141475 | 9086 -> 32160 | 1860 -> 5893 | 15.1 % | 13.1 % | 9.4 % |  |
| N_256x256_0012_3B_Mongolen i0 | 2 -> 2 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 19509 -> 147422 | 4286 -> 33443 | 741 -> 6098 | 14.3 % | 12.6 % | 9.7 % |  |
| N_256x256_0013_4B_Mongolen i0 | 1 -> 1 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 6145 -> 42876 | 1334 -> 9749 | 181 -> 1703 | 17.0 % | 14.9 % | 10.9 % |  |
| N_256x256_0014_2B_Piraten i0 | 6 -> 6 | 4 -> 4 | 6 -> 6 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 32262 -> 60985 | 7476 -> 13314 | 1519 -> 2052 | 2.3 % | 1.8 % | 0.7 % |  |
| N_256x256_0015_5B i0 | 2 -> 2 | 2 -> 2 | 3 -> 3 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 54662 -> 125491 | 13076 -> 28929 | 2650 -> 5283 | 9.7 % | 8.4 % | 5.7 % |  |
| N_256x256_0016_3B i0 | 5 -> 3 | 5 -> 2 | 4 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 17466 -> 163082 | 4172 -> 37215 | 693 -> 6805 | 16.7 % | 14.8 % | 11.4 % |  |
| N_256x256_0019_2B i0 | 4 -> 4 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 38850 -> 141668 | 9116 -> 32202 | 1860 -> 5894 | 15.1 % | 13.1 % | 9.4 % |  |
| Pr_128x128_0003_1B i0 | 3 -> 3 | 5 -> 5 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 34014 -> 83108 | 8158 -> 19303 | 1699 -> 3629 | 15.6 % | 13.7 % | 9.6 % |  |
| Pr_128x128_0008_2B_Indianer i0 | 2 -> 2 | 2 -> 2 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 36743 -> 85837 | 8816 -> 19961 | 1885 -> 3815 | 16.6 % | 14.5 % | 10.2 % |  |
| S_128x128_0003_2B i0 | 4 -> 4 | 5 -> 5 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 12745 -> 86596 | 2540 -> 19225 | 316 -> 3235 | 14.5 % | 12.6 % | 9.0 % |  |
| S_128x128_0003_2B_Beduinen i0 | 4 -> 4 | 5 -> 5 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 12745 -> 86596 | 2540 -> 19225 | 316 -> 3235 | 14.5 % | 12.6 % | 9.0 % |  |
| S_128x128_0009_0B i0 | 6 -> 5 | 6 -> 5 | 5 -> 4 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 47676 -> 73729 | 11166 -> 17168 | 2220 -> 3313 | 12.3 % | 10.8 % | 7.9 % |  |
| S_128x128_0009_0B_Orient i0 | 6 -> 5 | 6 -> 5 | 5 -> 4 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 47676 -> 73729 | 11166 -> 17168 | 2220 -> 3313 | 12.3 % | 10.8 % | 7.9 % |  |
| S_256x256_0003_2B i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 42549 -> 129923 | 9948 -> 29666 | 1884 -> 5315 | 15.7 % | 13.6 % | 9.6 % |  |
| S_256x256_0003_2B_Beduinen i0 | 1 -> 1 | 1 -> 1 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 42549 -> 129923 | 9948 -> 29666 | 1884 -> 5315 | 15.7 % | 13.6 % | 9.6 % |  |
| S_256x256_0004_3B_Orient i0 | 2 -> 2 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 120846 -> 298278 | 29055 -> 69198 | 6121 -> 13199 | 17.5 % | 15.3 % | 11.1 % |  |
| T_128x128_0040_3B_Po i0 | 2 -> 2 | 3 -> 3 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 15696 -> 20581 | 3605 -> 4553 | 708 -> 802 | 2.4 % | 1.8 % | 0.7 % |  |
| T_128x128_0053_1B_Po_Mongolen i0 | 2 -> 2 | 3 -> 3 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 15696 -> 20581 | 3605 -> 4553 | 708 -> 802 | 2.4 % | 1.8 % | 0.7 % |  |
| T_256x256_0002_2B i0 | 4 -> 4 | 2 -> 2 | 2 -> 2 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 38681 -> 141475 | 9086 -> 32160 | 1860 -> 5893 | 15.1 % | 13.1 % | 9.4 % |  |
| T_256x256_0005_1B_Po i0 | 3 -> 3 | 2 -> 2 | 1 -> 1 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 37561 -> 178751 | 8603 -> 40373 | 1658 -> 7190 | 15.8 % | 13.7 % | 9.8 % |  |
| T_256x256_0008_2B_Po i0 | 8 -> 8 | 12 -> 12 | 15 -> 15 | 0 -> 0 | 0 -> 0 | 0 -> 0 | 126985 -> 227908 | 30760 -> 53332 | 6478 -> 10372 | 12.4 % | 10.7 % | 7.5 % |  |

## 14. Stage S4 as built (2026-10-19): hydraulics, CPU only

Code: `src/render/include/anno/render/river.hpp` (`RiverConfig`, `RiverMask`, `FlowStats`, `RiverFlow`), `src/render/src/river.cpp` (the raster, `Scene::river_mask`), `src/render/src/river_solve.cpp` (the solver); tests `src/render/tests/river_flow_test.cpp` (synthetic cases, no game files) and the corpus case in `render_corpus_test.cpp`; QA `river_qa --flow` / `tools/river_qa.py flow`. Nothing is drawn: the digests of all enhanced and original views are unchanged (21 PNG views of 5 islands at all zoom groups are byte-identical before and after; `render_corpus_test` unchanged), the zoom benchmark rows are the same (14.4).

### 14.1 What it does

1. **Raster (`RiverMask`, phase A).** Cells of 1/8 tile in island tile space, built from what the shipped gap fix already computes, not from a second model: the physical test of `sea::water_field` (`sea::tile_ground` and `sea::river_point`, now shared by `water_field` and the raster, so both classify identically; a test compares every cell with `water_field`), the painted bed and the waterfall and mouth shapes (the alpha of the river sprites at zoom group 0, closed over the checkerboard, projected forward per cell: `sx = (x - y) 42`, `sy = (x + y) 21 - 2.059 T`, depth-tested per cell as the original, wet when at least half of a 5 x 3 pixel window is covered). Sea is where the original draws sea or the enhanced waterline is above the terrain; sprites never make river water over the original's sea.
2. **Flow (`RiverFlow`, phase B).** Per connected piece of river ("river"): masked bed smoothing (G4), conveyance `K = k_M h^(5/3) / nu(d_bank)` (E1, E4, E6), harmonic face conductances, uniform lateral inflow, outlets, one SPD solve (E5), cell velocity from the face fluxes, discharge rescale (E7, 14.2), soft floor (E9), falls speed (E10), cap (E11), one 3 x 3 smoothing; ponds (D7); Eikonal field if a solve fails. `RiverFlow::sample` is the valid-weighted bilinear field over the wet cells.
3. **Options** (`RiverConfig`, defaults = section 2): `Discharge::{Normalised, Absolute, Sourced}`, `conveyance_exp` (5/3 Manning, 3/2 Chezy), speeds, bank profile, `bridge_tiles`, solver limits. Listed for the owner: `open-questions.md`, "Decisions for the owner (rivers)", OQ-065 to OQ-069.

### 14.2 Where it differs from the design above, and why

| Item | Design | Built | Why |
|---|---|---|---|
| Mask | per-piece `T_raw <= h_i + tau + 0.5` (S3) | the shipped criteria (above) | the gap fix shipped first (iso.md "River gaps"); S4 reuses it, S3's per-piece mask is not built |
| Level `Z` | per-piece level, linear ramp across falls (G1-G3) | the shipped smoothed river level of `IslandWater::river_level` (average of the pieces' `h + 9`, smoothed), bed = Catmull-Rom + half the shore noise | consistent with the drawn depth; the ramp over a fall comes from the smoothing (slope up to 16 units / tile at the 48-unit falls); depth is clamped 1.5-12 units anyway |
| Waterfall links | footprints join the network | the sprite alpha covers only the painted cascade (about 110 of the 1,600 cells around a fall), and where the cliff hides the terrain the river above and below do not touch; a fall or mouth piece joins the pieces of river inside its footprint (dilated by a tile) with a straight channel 3 cells wide, at most `bridge_tiles` 2 tiles, flagged `kBridge` (161 links over the 106 islands). The drawn water is unchanged | **fix**: "falls and mouths are the links of a river" (1.2); without it `D_256x256_0045_5B` had 16 rivers and 12 without outlet, now 7 and 3 |
| Solver | nested PCG over R = 1, 2, 4, 8, Jacobi | PCG with a symmetric V(1,1) aggregation multigrid preconditioner (2 x 2 aggregates, damped Jacobi, coarse conductance = half the sum of the faces across; dense Cholesky below 96 cells; up to 24 levels) | more robust on long thin networks (17-98 iterations for 4-56 K cells, 5-7 levels) at the same cost; one solve per river, relative residual 1e-5 |
| Outlets | Dirichlet `phi = 0` within 1 tile of the sea | a ghost conductance `K` on every face that borders sea (Robin), so the flow continues to the sea at full speed instead of a dead band of zero velocity; a river without sea contact (D14) gets a pseudo outlet: the cell at its lowest level (then lowest bed, mouth cells first) and its neighbours within two steps (5 cells in a channel one cell wide, up to 13 in an open reach) | continuity to the last cell |
| Bank distance | distance transform | chamfer over the wet cells; sea is no bank | the sea is an opening |
| Bed smoothing | masked Gaussian | diffusion on the wet graph (step 1/4, `ceil(2 sigma^2)` passes = 72 for 0.75 tile), no-flux at dry cells | same width, no dense raster |
| Discharge reference (E7) | mean speed over the outlet faces = `V_out` | the fastest cross-section (a band one cell thick of the Eikonal distance, at least 4 cells wide) that still passes half of the river's discharge gets `out_speed` | the original's mouths are broad blobs (up to 119 sea faces for a river 1.4 tiles wide): measured at the faces, half of all cells of `Pr_128x128_0003_1B` sat on the cap (median 2.8 tiles / s). A straight channel still has `out_speed` at its outlet (tested). Any pinch that passes half of the discharge and is at least 4 cells wide does set the scale (the price of "fastest section"); `Reference::MedianCell` (median cell = `median_speed` 0.4) is the option without it. OQ-065 |
| Sourced inflow | at dead ends | local maxima of the Eikonal distance >= 2 tiles from the outlet (the divide of a braid counts too), inflow `source_speed` x width x depth, spread over the disc of the channel's half width around the widest point within 2 tiles of the dead end | the corner cell of a dead end has no width; OQ-066 |
| Fallback | island | per river (a failing solve does not spoil the others); `FlowStats::fallback_components`, `kFallback` cells, a line on stderr | none fell back on the corpus |
| Not in S4 | R9 extension, quantisation, `turb`, `wake`, obstacle holes, Picard, thalweg shift, standing waves, `RiverQuality` | not built | S5-S7 |
| Test form of "Manning" | `u = k_M R^(2/3) S^(1/2)` with the solved `S` | the conveyance exponent (the speeds of two depths side by side split 2^(2/3) = 1.587, Chezy 1.414), and that `k_M` and `n` cancel (the absolute scale is the discharge's) | `S` is not observable: with a linear conveyance `phi` has arbitrary units |

### 14.3 Measured

Windows 11, Ryzen 9 9800X3D, MSVC Release, one island at a time, `river_qa --flow`; the 72 island templates with rivers and the islands of the 13 scenarios = 106 islands, 257 rivers, 147 ponds:

| | p50 | p95 | max |
|---|---|---|---|
| `RiverFlow::build` (72 templates) | 11.5 ms | 33 ms | 58 ms |
| `RiverFlow::build` (all 106) | 11 ms | 43.5 ms | 60 ms |
| `Scene::river_mask` (sprite covers, phase A; all 106) | 30 ms | 83.5 ms | 96 ms |

Gate: no NaN, no fallback, worst relative residual 1.0e-5 (limit 1e-3), worst imbalance between inflow and outflow 3.9e-7, p95 0.044 s (limit 0.3), max 0.060 s (limit 1): **PASS**. Largest island `single31#1`: 55,923 wet cells; the most iterations 98 (`D_256x256_0045_5B`, 7 levels), most islands 20-40. Memory: the raster is 15 bytes per cell while it is built (flags, bed, level, the `near` and sea bits, the labels of the bridge pass; the 48-byte `TileGround` of the first version is gone), 17 MB for the largest raster (`single31`, 1.12 M cells), freed down to 9 bytes per cell (flags, bed, level) when `from_island` returns; the flow keeps about 100 bytes per wet cell (55 K cells = 6 MB). Speeds (tiles / s, per island): median 0.12-0.48 (typically 0.21), 95th percentile 0.19-2.5 (0.94), mean 0.14-0.69; falls 2.5; 86.5-100 % (average 99 %) of the cells flow down the Eikonal distance to the outlets, the others are the divides of loops and the heads of sea-less rivers. What this figure can and cannot show: the Eikonal distance is the very field the solver uses as its fallback direction, and a sea-less river is measured against its own pseudo outlet, so it mostly confirms that the solve points to the nearest outlet. It says nothing about a wrong-way stretch (a river that runs towards its head), and it is not the S5 acceptance for flow direction (Q5); braids, loops and bends are what the unit tests check. `river_qa --flow` now also gives it for the rivers with a sea face only ("downstream sea %": 86.5-100 %, median island 99.6 %). 136 of the 257 rivers have no sea contact (pseudo outlet, OQ-067; `river_qa --flow` and the corpus test list them). Speed scale (OQ-065), over the 106 islands: the soft floor sets the speed of 17-90 % of the cells of an island (median island 38 %, 42 % of all wet cells; 23 islands above 50 %); the cap sets at most 0.4 % (none above 5 %). With `Reference::MedianCell` at 0.4 tiles/s: floor 15-41 % (median 20 %), cap up to 10.5 % (median 0.4 %, 7 islands above 5 %). Contact sheets of the flow field (hue = direction in tile axes, brightness = speed, arrows every 1.5 tiles) for the five QA islands: `C:\Users\altin\Pictures\anno-rivers\S4\flow` (not in git).

### 14.4 Not changed (checked)

- 21 `anno1503 --png` views (the owner's scene `D_256x256_0045_5B` at group 1 with enhanced and original water, one island per climate at groups 0, 1, 2) are byte-identical to the build before S4.
- `bench_frame --zoom 6 --renderer gpu --limits interactive`, `Single_01.sze`, 2560 x 1369, enhanced: p50 / p95 / p99 of the window thread 0.3 / 1.5 / 1.9 ms at group 0, 0.3 / 1.5 / 1.8 at group 1, 0.3 / 1.4 / 1.7 at group 2; before 0.3 / 1.5 / 2.0, 0.3 / 1.5 / 1.7, 0.4 / 1.4 / 2.0 (one 14 ms outlier at group 2 while other jobs ran).
- `render_corpus_test` digests (original and enhanced), all `ctest` cases with and without game files.

### 14.5 Reproduce

    river_qa game/app --flow [--flow-dump DIR] [--discharge normalised|absolute|sourced] <files>     (tools/river_qa.py flow)
    ANNO1503_GAME_DIR=game/app ctest --preset dev -R "river flow"

### 14.6 Open for S5 and later

Prerequisites the review of S4 left for the shading stage (each with the test that closes it):

- R9 and the `valid` fade at the rim, and a wider speed- and direction-smoothing across falls, mouths and bridges before the lattice is baked (S4 has `transition_passes`, 4 speed-weighted passes within 8 cells; the first-order shear test of Q10 on `D_256x256_0045_5B` and the `Single_01` mouths decides whether that is enough). The mouth fans (the two lobes of a mouth that point at each other) are the case to look at.
- Quantise the lattice (1/4096 tiles per second, R9) before any digest is recorded: the solver uses libm `pow`, `exp`, `cbrt` and `hypot` in double, so the field is bit-identical per toolchain, not across MSVC and glibc.
- Build `Scene::river_mask` once per island on a worker (`std::call_once`) and hand out a `shared_ptr`; it costs 30 ms p50, 84 ms p95, 96 ms max and must never run on the window thread or per chunk (a test shows that concurrent callers get identical rasters).
- Rivers without sea contact (136 of 257, OQ-067): before the flow is shown by default, review the list (`river_qa --flow --flow-rivers FILE`, with bounding boxes) on the contact sheets (the pseudo outlets are magenta there) with the owner; `RiverConfig::sea_less_speed_scale` (0 = still water) and the cell flag `RiverFlow::kSeaLess` are the switch.
- The mouth blobs (OQ-070).

General:

The mask is a hydraulic raster, not yet the drawn mask (S3); the lattice bake needs R9 (extension beyond the wet cells) and the `valid` fading; `sample` of a pixel at the rim is already normalised. Speeds are a model output (`out_speed`, floor, cap are decisions OQ-065), the three discharge models need contact sheets, and the rivers without sea contact (OQ-067) are where the data has breaks (OQ-068).

### 14.7 Review of S4 (2026-10-19): what changed

Two reviewers accepted S4 as green and found the following; tags as in `open-questions.md` (fix = removed and written down here, decision-needed = default kept, behind an option, listed for the owner).

| Finding | Tag | Result |
|---|---|---|
| The sprite-derived raster (projection, depth test, closing, coverage window) had no fidelity test | fix | split into pure steps (`river::detail`: `project_cells`, `plane_for`, `cover_from_alpha`, `half_window`) with unit tests for the depth test, the coverage rule and the window; corpus case "river raster agrees with the river the original draws": at zoom group 0, 4 islands (7 rasters), the pixels 6 px inside the original's sprites that do not lie over sea are wet cells in 98.9-100 % (checked >= 98.5 %), the cells that exist only through a sprite lie within a pixel of a sprite pixel in 97.2-99.7 % (>= 96 %), and at most 0.32 % of all wet cells have neither sprite nor drawn river water within 6 px (<= 1 %). Hand mutations now failing: no depth test, the falls and mouths below the sea drawn depth-tested, a coverage rule of one pixel (unit test). Pixels over sea are excluded by design (the sea stays sea) |
| The Normalised reference (OQ-065) was untested against a broad mouth | fix | unit test: a river 2 tiles wide into a blob 12 tiles wide, with and without a bottleneck; the river proper is at 0.70-0.95 of `out_speed`, the blob slower, nothing on the cap; `out_flux_share` 0 and the sea-face reference fail it; corpus: at most 5 % of an island on the cap (measured 0.4 %) |
| Pseudo-outlet priority (level before bed), the enhanced sea as an outlet, `mask_margin_tiles`, the bridge over sea cells, the bridge depth floor were untested | fix | one unit case each; mutations of each now fail. Corpus ratchet: sea-less rivers <= 136 |
| Multigrid efficiency and the Jacobi coarse fallback were untested | fix | iterations <= 28 / 30 / 34 on the 320 x 64 channel, the braid and the Jacobi path (measured 18 / 20 / 22), levels >= 4; `dense_limit` and `coarse_sweeps` moved into `RiverConfig` (cost of the dense factor documented there); a halved-conductance regression doubles the iterations and fails |
| Continuity of sea-less rivers was checked only on the corpus | fix | `flux_imbalance` and the residual are asserted for all sub-cases of the pseudo-outlet test |
| `TileGround` per raster cell (48 bytes) | fix | one bit per cell; the raster is freed to 9 bytes per cell on return (14.3) |
| Constants without a reason or a way to configure them | fix | the coverage window follows `cells_per_tile` and the tile size (`half_window`), the bed-smoothing pass cap, the bridge brush (`bridge_half_width`) and depth (`bridge_min_depth`), the dense limit and the sweeps are in `RiverConfig`; the Jacobi damping and the pond noise are commented; `river_qa` uses `cells_per_tile()`, checks its numbers, and the debug probe is gone |
| The determinism test compared padding bytes and was named for what it does not show | fix | compares fields, renamed "builds share no state"; golden digests wait for the quantised lattice (14.6) |
| Docs: "5 cells wide" pseudo outlet; the downstream metric reads stronger than it is | fix | 14.2 and 14.3 corrected |
| Seams at falls and mouths; direction noise where the flux is nearly zero | fix (partly S5) | `transition_passes` (speed-weighted, near kFall / kMouth / kBridge) and a test; a divide is a slow line with both sides running away from it (test); the rest is S5 (14.6) |
| The speed reference sets most of a network at the floor | decision-needed | default kept; `Reference::MedianCell` added, numbers in 14.3 and OQ-065 |
| 53 % of the rivers have no sea contact | decision-needed | default kept; `sea_less_speed_scale`, `kSeaLess`, `river_qa --flow-rivers`, pseudo outlets magenta in the contact sheets; OQ-067 |
| Mouth-sprite basins in the raster | decision-needed | default kept, OQ-070 |

## 15. Stage S5 as built (2026-10-19): lattice bake and CPU shading, the first owner-visible flow

Code: `src/render/include/anno/render/river.hpp` (`LayerConfig`, the S5 fields of `RiverConfig`, `FlowPlane`, `flow_at`, `RiverFlow::sample_extended`), `src/render/src/river_lattice.cpp` (configuration, texel and `flow_at`), `src/render/src/iso.cpp` (`flow_phases`, `river_flow_shade`), `src/render/include/anno/render/sea.hpp` (`shade_sea_flow`), `src/render/src/scene.cpp` (per-island flow slots, builder threads, `Scene::flow_plane`), `src/render/src/frame.cpp` (`Chunk::flow`, `flow_pending`, the rebuild), `anno1503 --river-flow legacy|flow --river-gain x` and key `R`, `bench_frame --river-flow`, `river_qa --rect g,x,y,w,h --shade DIR`. Tests: `src/render/tests/river_shade_test.cpp` (synthetic, no game files: 14 cases), `river_flow_corpus_test.cpp` (game files: 8 cases), `render_corpus_test.cpp` (digests, legacy reproduction). **(Written before S6: the GPU compositor drew the legacy river look until S6, the viewer showed the flow with `--renderer cpu` and `--png`. S6 is done: section 16, the GPU draws the flow and the tests compare it with the CPU path in the same look.)**

### 15.1 What it does

1. **Lattice (`Chunk::flow`, 3.3).** A global lattice of `FlowPlane` texels: texel (i, j) at scene pixel (x0 + i s, y0 + j s), strides 8 / 4 / 4 px at groups 0 / 1 / 2 (`RiverConfig::lattice_stride`, validated: they divide the chunk size). A texel is the island's flow at the tile position the ground probe shows at its pixel: `RiverFlow::sample_extended` (R9: the nearest wet cell's flow and level past the raster, `valid` fading from 1 to 0 over `extend_tiles` = 2 tiles, nearest cell by an 8SSEDT pass) quantised to 1/4096 tiles/s (int16, +-`velocity_max` 8), `valid`, turbulence and wake (0 until S7), the level in 1/2 unit and the shear |grad u| in 1/32 1/s (the forward differences of the island's flow over `shear_probe_tiles` = 0.25 tile along each tile axis, a pure function of the probe position like the rest). `flow_at` reads the four texels around the pixel centre, `(1-a)(1-b)` weights times `valid`, normalised by their sum (a texel without flow does not drag the vector or the shear to zero); the shear is interpolated like the velocity (the first version derived it from the differences inside the texel quad, constant per lattice cell: the shear fade then stepped at every texel border, 15.7). A chunk that touches an island with rivers carries the plane; one without river pixels carries none (the plane is dropped when no texel has a flow).
2. **Shading (`iso::river_flow_shade`, 2.8).** Per layer two phases `phi_k = fract(t / T + psi(x) + k/2)`, weights `1 - |2 phi - 1|`, coordinates `c_k = (x - u T phi_k) s + o_k` on the **river's own water plane** (`x = w(px, py + lambda * level)`, E19, `lambda` the lift at the group), variance-preserving blend of the centred noise, light = amplitude * (N(x) - N(x + e L)) / e (L = -x tile axis, e = 0.15 tile). The amplitude fades with the shear (`T |grad u|` from 0.5 to 1.5), with the zoom group's wave detail (layers marked `detail_fades`: 0 at group 2) and with the turbulence channel (S7). The rapids layer (whitewater streaks, no foam) keeps the legacy slope rule: `smoothstep(kRapidsFrom, kRapidsFull, slope) * (0.05 + amplitude * noise)`, advected like the others. Tint and the mix with the sea look at the mouths (`kb`) are the legacy ones, untouched. No time-dependent term except the phases: the hourly wrap is seamless.
3. **Where flow is used.** `FrameRenderer::river_flow` (default Flow; key `R`, `--river-flow`), `Layers::river_flow` (`--png`, `Scene::render` builds the lattice of its region itself, blocking), `sea::shade_sea_flow(..., flow, RiverShading)`. A pixel without flow (`valid < min_valid` 0.02) keeps the legacy shimmer. Original water ignores the river mode.
4. **`flow_pending` (3.1).** Flows are built by `Scene::river_flow(index, wait)`: the raster (phase A) and the solve (phase B) under a `call_once` per island. `wait = false` queues the island on the flow builders (1-4 threads, below-normal priority, started on the first request; the viewer queues every island with rivers at start) and returns *pending*: the chunk worker builds the chunk with the legacy look on the correct mask and tags it (`Chunk::flow_waiting` lists the islands). `FrameRenderer::request_chunks` rebuilds such a chunk (it stays in the cache meanwhile) as soon as `Scene::river_flow_done` holds for all of them. The window thread never calls a build.

### 15.2 Where it differs from the design above, and why

| Item | Design | Built | Why |
|---|---|---|---|
| Apron | probe render grown by one stride | two thin probes (1 px wide) for the apron column and row (`Scene::flow_plane`) | the texel is a pure function of its pixel, so neighbours hold bit-identical texels either way (tested: apron columns of all neighbouring chunks, a region with an odd offset, single-pixel probes, groups 0-2); the thin probes cost 0.7-1.2 % of the chunk's ground pass instead of ~3 % |
| Plane | `n = 512 / stride + 1` square, chunk origin | `x0, y0, nx, ny`, any rectangle, `tile_w/h` for the shear | `Scene::render` (`--png`) builds a lattice for an arbitrary region; a chunk plane is the special case |
| Layer struct | four floats | `scale, period, amp, fades, rapids` + `rapids_base, shear_from, shear_to` in `RiverParams` (no `stretch`) | the LOD flag and the rapids layer need a place; the layer count is still data (`kMaxRiverLayers` 8) |
| Rapids | Froude streaks (S7) | the legacy slope rule, advected | Fr needs the turbulence work of S7; the falls keep their look |
| Stretch `kappa` | 0.35 / 0.55 / 0.80 / 0.35 | **none** (**fix**) | S5 as first built used 0.2 times the table and blamed the aperture effect for the Q5 speed ratio of 0.69-0.82 at the full value. The review found the real cause: the stretch is applied to the absolute island coordinate 100-250 tiles from the origin, so wherever speed or direction varies the noise coordinate moves by (distance) x d gain/d speed x d speed/d pixel, about ten times the geometry (7-93 % of the river pixels of the QA islands above three times the geometric step; static per-pixel hash light, fingerprint swirls). Removed (15.7); Q5 flat stems now give 0.90-1.07 |
| Shear | from the texel quad | a baked lattice channel | the quad estimate was constant per lattice cell: hard-edged flat blocks of zero light where the fade stepped (**fix**, 15.7) |
| Amplitude | 0.045 / 0.060 / 0.040 | times 0.52 | calibrated to the legacy light variance (2.8) **for a uniform flow at full wave detail**: sd ratio 1.00-1.06 at speeds 0.12 / 0.5 / 0.9 tiles/s, averaged over four directions. On real rivers the detail fade and the shear fade leave 0.78-0.98 at group 0, 0.51-0.59 at group 1 and 0.42-0.50 at group 2 (15.3, OQ-083) |
| `z` coordinate (boil) | slow third noise coordinate | none; the second phase samples another part of the field | a time-dependent `z` jumps at the hourly wrap |
| Level for E19 | baked level channel | the raster's smoothed river level, 1/2 unit | same data the solver used |
| Q5 on rendered frames | block matching of frames t, t + 50 ms | of the *light* of two moments, from the lattice and the same shading function | the static sea sprite texture dominates the rendered frame (the light is +-2 % of the brightness) and the sea pattern animates in place, so a frame match locks onto it; the shading is a function, so shifted evaluation replaces image resampling |
| Q3 temporal std per group | within 15 % between groups | not asserted | the glints fade with the zoom group by design (`detail_fades`); the lattice values and the mask are compared instead |

### 15.3 Measured (Windows 11, Ryzen 9 9800X3D, MSVC Release, CPU path)

| Check | Result |
|---|---|
| Q5 (group 1, 3,200 x 1,800 px window, flat stems off the rapids, 64 px windows, 50 ms; after the review) | median angle error / speed ratio: `D_256x256_0045_5B` 7.9 deg / 0.91 (36 blocks), `N_256x256_0006_2B` 3.8 deg / 1.03 (2 blocks), `S_256x256_0004_3B_Orient` 9.0 deg / 1.00 (49 blocks); reported: Tundra 5.7 deg / 1.05, Praerie 5.3 deg / 1.07; limits 15 deg and [0.7, 1.3] |
| Q5 in all windows (review; 64 px windows without rapids, predicted shift >= 3 px in 0.3 s, shift of the mean flow including the climb of the surface under it) | the prediction beats standing still in 63 % (`D_256x256_0045_5B`, the owner's canyon; 41 windows, the shear fade has taken most of the light there), 100 % (Nord, 5), 94 % (Steppe, 52), 89 % (Tundra, 70), 74 % (Praerie, 31); 83 % of all 199 windows; median best / predicted shift 0.81-1.12. The shipped stretch measured 10-20 % |
| Noise coordinate steps (review; 2,000 x 1,200 px windows, 5 islands x groups 0/1/2, all layers) | the step between neighbouring river pixels against the geometry (scale x tile distance of one pixel): 94.8-99.8 % within twice, 98.2-99.9 % within three times (the shipped stretch: 7-93 % above three times), the audible layers (shear fade >= 0.5) 99.2-99.9 % within twice; p90 1.06-1.62 |
| Rendered pixels (review) | `Scene::render` river pixels at groups 0, 1, 2 equal the tint-only render times 1 + `river_flow_shade(...).light` of the production lattice with the water plane written out from E19: mean error 0.39-0.42 levels (rounding) against 1.0-1.9 with the lift set to zero |
| Flow coverage (river pixels with a flow, 5 islands x groups 0/1/2) | >= 99.9 % (the owner's window: 86,396 of 86,441 = 99.948 %) |
| Water mask over the zoom groups (same 24 x 12 tile area, common size) | IoU 0.94-0.98 (limit 0.9) |
| Q4 | all apron texels of all neighbouring chunk pairs and a region at an odd offset bit-identical (groups 0/1/2: 2,990 / 3,612 / 1,548 apron texels compared; 5,162 / 20,178 / 20,178 region texels); 150 sampled texels equal the island's flow at a one-pixel probe |
| Q10 (the canyon's tiers >= 64) | own water plane: median distance to the terrain position 0.21-0.23 tiles, p90 0.61-0.77, p99 1.3-1.7 (cliff silhouettes); the sea plane of the legacy shimmer: median 3.7 tiles |
| Apron cost | 0.7 / 0.9 / 1.2 % of a chunk's ground pass at groups 0 / 1 / 2; the whole lattice (apron + 16,641 texels) +0.4-1.1 % of the chunk (56 / 80 / 83 ms), flow built before |
| Pulsing | spatial sd of the light over a 1,600 tile patch varies 0.8 % (all layers) and 1.2 % (one layer) over a cycle; a linear blend of the phases would pulse > 8 % (control in the test) |
| Frame cost (CPU, `D_256x256_0045_5B`, 1280 x 720, group 1, enhanced) | sea stage 17.3 -> 20.4 ms, frame p50 22.5 -> 25.4 ms (flow against legacy); original water unchanged |
| GPU zoom benchmark (`bench_frame --zoom 6`, `Single_01.sze`, 2560 x 1369, interactive limits) | see 15.5: no change (the GPU path does not read the lattice yet) |

Light and calibration: the legacy shimmer's standard deviation is 0.018 (brightness +-1.8 %). The layer table is calibrated to it **for a uniform flow at full wave detail** (unit test). On real rivers (corpus test "the light of the flow shading against the legacy shimmer on real rivers", river pixels off the rapids, four moments) the standard deviation of the flow light over the legacy's is 0.78-0.98 at group 0, 0.51-0.59 at group 1 and 0.42-0.50 at group 2 on the five QA islands (mean 0.89 / 0.56 / 0.45): the glints fade with the zoom group's wave detail by design and the shear fade removes layers near the banks, so **the default is quieter than the legacy shimmer at groups 1 and 2, and not louder at group 0**. The mean change of a river pixel over 250 ms is the same for legacy and flow (1.9 levels in the owner's window). `--river-gain 4` makes the structure easy to see; both are in the contact sheets. Decision OQ-083 (with these numbers); the speed floor behind the quiet look is OQ-065.

### 15.4 Digests

Only the Enhanced digests that contain river pixels changed, once, in `render_corpus_test.cpp`: Single_01 g2, Single_06 g2, Single_10 g2, Single_14 g2, Single_24 g2 and g1 region, and the Single_24 g2 / g1 frames. Reason for every line: river pixels are shaded by the flow shading (no other pixel changes: `Scene::render` flow against legacy differs in river pixels with a flow only, on all five QA islands). The test "legacy river shading reproduces the previous enhanced digests" renders Single_24 in legacy mode and finds the previous digests (`de09ab1e97a17170`, `febd87d511c3ec25`). Every Original line is unchanged. The lines were recorded once with the final shading (after the review of 15.7); the test "legacy river shading reproduces the previous enhanced digests" now covers all five scenarios with river pixels (Single_01, 06, 10, 14, 24: the former expected lines) and the two Single_24 river frames of the frames test, in legacy mode.

### 15.5 Reproduce

    anno1503 --view game/app/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp --game-dir game/app --group 1 --water enhanced --river-flow flow|legacy [--river-gain 4] --time 250 --at 152,97 --size 2494x1355 --renderer cpu --png out.png
    river_qa game/app --rect 1,555,2264,1200,700 --shade DIR --times 0,250,500,750 [--shade-gain 4] game/app/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp
    bench_frame game/app <scene> --renderer cpu --styles enhanced --river-flow legacy|flow ...

(`--at` is in island tiles; the owner's scene is at tile 152,97 at group 1: the screenshot window of 2494 x 1355 px is centred at scene pixel (1151, 2611).) Frame sequences (8 frames 250 ms apart, legacy / flow / flow with gain 4), light-only images and quiver images of the owner's scene and one island per climate at groups 0-2: `C:\Users\altin\Pictures\anno-rivers\S5` (not in git); the same views as first built (before the review) are in `S5\before_review`, and `S5\compare_before_after.png` puts light images and frames of both side by side.

### 15.6 Open for S6 and later

- GPU port (S6): done, section 16 (bindings 6 to 8, flags, uniform, the WGSL twin of `river_flow_shade` and `flow_at`, `RiverQuality`; `GpuFrame::sync_chunks` uploads a rebuilt chunk again, keyed by `Chunk::serial`).
- Turbulence, wake, obstacles, Froude streaks, thalweg, standing waves (S7); the `kSeaLess` rivers (OQ-067) are shown as flowing (default scale 1).
- The lattice stride of group 2 (4 px = 0.2 tile in x, 0.4 tile in y) is as wide as the banks of a river one tile wide: with the baked shear the slow layers fade across the whole river there (calm, sd < 0.006, both bands), where the first estimate left the banks calmer than the core; 2 px stays the fallback (OQ-083 lists it with the quiet group 2).
- GPU path (S6): done: the shear byte is a third texture (R8Uint, binding 8) beside the two of 3.3.

### 15.7 Review of S5 (2026-10-19): what changed

Two reviewers (one on the picture, one on the engineering) accepted the flow direction, the mask, the gaps, the seams and the zoom groups, and found the following. Tags as in `open-questions.md` (fix = removed and written down here, decision-needed = default kept, behind an option, listed for the owner).

| Finding | Tag | Result |
|---|---|---|
| **Stretch along the flow aliases.** `c = x + g(|u|) (x . u_hat) u_hat` with `x` the absolute island coordinate (100-250 tiles from the origin): where speed or direction varies the noise coordinate changes by (distance) x dg/dspeed x dspeed/dpixel, about ten times the geometry (L2 step 0.75-1.05 noise cells per pixel against 0.074; 7-44 % of the river pixels above three times the geometric step on Dschungel / Nord, 88-93 % on Steppe): the light was a static per-pixel hash there (correlation 0.993 between t = 0 and 250 ms), did not move with the flow (the predicted shift beat standing still in 10-20 % of the windows) and showed fingerprint rings and swirls at bends. S5 had scaled the stretch to 0.2 and blamed the aperture effect, which hid it on flat stems only | fix (blocker) | the stretch is removed (`LayerConfig::stretch` and `RiverLayer::stretch` are gone; a stretch needs a coordinate that follows the streamlines). New tests: the noise coordinate follows the geometry where the flow varies, at 275 tiles from the origin (synthetic; 13 % of the steps within twice under the old formula), and on all QA islands and groups (94.8-99.8 % within twice, the audible layers 99.2-99.9 %); Q5 in all windows, not only the flat stems (15.3). Mutation: the old formula back in fails the synthetic coordinate test, the uniform shift test and four corpus tests |
| **Shear in steps.** The shear fade came from the differences inside the 2 x 2 texel quad: piecewise constant per lattice cell, so the amplitude stepped at texel borders (hard-edged rectangles of exactly zero light, a checkerboard-dotted block at gain 4; the shear is large on 10-38 % of the owner's pixels) | fix (major) | the shear is a baked lattice channel (`FlowPlane::shear`, forward differences of the island's flow over 0.25 tile), interpolated bilinearly like the velocity; tests: round trip and clamping, linear change between texels with no step, valid weighting, the fade along a row has no step larger than the calm row's, thin rivers at all groups with the analytic shear (group 2 is calm everywhere, 15.6) |
| **Calibration claim.** "sd ratio 1.00-1.06" holds for a uniform flow at full detail only; on real rivers 0.78-0.98 / 0.51-0.59 / 0.42-0.50 at groups 0 / 1 / 2 (detail fade, shear fade); the mean change over 250 ms equals the legacy's (1.88 against 1.91 levels) | fix (wording, test, doc); decision-needed (the default) | doc, test title and comments say what the calibration covers (15.2, 15.3); a corpus test measures the real ratios and pins their range; the default (calibrated table, quiet at groups 1 and 2) is kept and listed in OQ-083 with the numbers, `light_gain` is the option |
| 55 % of the river samples in the owner's window sit at the soft floor (< 0.15 tiles/s, median 0.138, p95 0.82, p99 1.34, none above 2), about 5 px/s at group 1: the contrast of narrows against wide shallows is only in the tails | decision-needed | defaults kept (`Reference::FastestSection`, `speed_floor` 0.12, `light_gain` 1); the numbers are in OQ-065 and OQ-083 (`Reference::MedianCell` and gain 4 are the options to look at with the sheets) |
| Rivers without sea contact show converging arrows in the owner's scene (the lower-left stretch, a pond fed from the bottom that turns into its closed top) | decision-needed | default kept (flow shown); listed in OQ-067 with the place; `sea_less_speed_scale` 0 keeps these reaches still |
| The GPU path shows no change until S6 (flow frames differ from legacy by a median of 2 levels, 12 at most, at gain 1 on the CPU path); the viewer's GPU path bakes lattices that nothing reads (+0.4-1.1 % of the chunk time, 100-180 ms of low-priority CPU at start, 13-19 MB; a chunk built before its flow is built twice) | fix (statement); decision-needed (the waste) | the owner-visible effect at the defaults is below perception and absent on the GPU path until S6 (stated in OQ-083 and the checklist); skipping the bake while the GPU compositor runs is OQ-084 (default: leave as is until S6). The 4-47 changed non-river pixels per frame the reviewer saw in frame diffs are a window-offset artefact of that comparison: `Scene::render` flow against legacy changes river pixels with a flow only (test, five islands) |
| `nice(5)` lowered the priority of the whole process on macOS | fix | per thread: QoS class `UTILITY` on Apple, `setpriority` on the thread id on Linux, `SetThreadPriority` on Windows |
| `RiverConfig::validate` was only called by tests; `extend_tiles` was unbounded (the extension grid is (nx + 2 reach) x (ny + 2 reach) cells) | fix | `Scene::river_active()` validates once at first use (a refused configuration is reported on stderr and the rivers keep the legacy look), `extend_tiles` <= `kMaxExtendTiles` 16, `shear_probe_tiles` > 0, `flow_threads` <= 64; test with a refused configuration |
| Rules without a failing test (mutation check): `light_gain`, the production water-plane lift at group 0, the `min_valid` fallback, the detail wiring at group 2, the extension at corners and ends and inside the fade band, the level blend | fix | tests: `light_gain` scales the ripple layers and not the rapids layer (and is validated); `sea::river_view` (the lift of each group, written out from E19) and `sea::river_pixel_shade` (below `min_valid` exactly the legacy shimmer, above exactly `river_flow_shade`) are factored out of `shade_sea_flow` and tested; rendered river pixels at groups 0-2 equal the shading of the lattice (15.3); the extension is compared with a brute-force nearest cell on a channel (ends, corners), a diagonal strip, two blobs and scattered cells (the diagonal passes of the transform are redundant on all of these: removing them is not seen, only the level blend and the cell choice are) |
| The failed-build path had no test; a pending result would rebuild a chunk forever | fix | `Scene::flow_build_fault` (test hook) injects a failure: the failure is final (no flow, never pending, the build ran once, no chunk is rebuilt, the picture is the legacy look) |
| "Never waited for" was a 250 ms bound (one island build is 100-180 ms); a rebuilt chunk was not compared with a blocking build | fix | the bound is half the measured build time, the structural check (flow not done) stays, and a chunk rebuilt after its flow arrived equals `render_chunk(..., wait_for_flow = true)` (lattice and field byte for byte) |
| Limits without a reason: builder threads, the level byte, a dead `river_flow_epoch()`, the public pause hook | fix | `RiverConfig::flow_threads` (0 = a quarter of the hardware threads, 1-4, with the reason), the 127.5-unit range of the level byte documented with the path (a 16-bit channel with the GPU texture, 121 is the original's top), `river_flow_epoch` removed, the pause hook marked tests only |
| The digest tests compared C++-recorded enhanced river lines under a name that says Rust reference; the legacy reproduction covered Single_24 only | fix | tests renamed (recorded digests; the Rust reference's except the enhanced river lines); legacy reproduction over all five scenarios with river pixels and the two river frames |
| The apron cost test asserted a wall-clock ratio in a parallel suite | fix | the apron is asserted structurally (two thin probes, <= 1.5 % of the chunk's area; 0.6 % here); the timing is reported |

Not done: ASan/UBSan of the new code on this host (the Windows build has no sanitizer run; the reviewer's WSL kept crashing under load), checked in CI.


## 16. Stage S6 as built (2026-10-19): GPU port and quality tiers

Code: `src/render/shaders/gpu.wgsl` (`flow_at`, `flow_psi`, `flow_phase_noise`, `flow_layer_field`, `river_flow_shade`, the river branch of `sea_fs`; bindings below), `src/render/src/gpu.cpp` and `gpu.hpp` (layouts, flow planes, `river_params_bytes`, `RiverCost`, `choose_river_quality`, `GpuFrame::measure_river_cost`, `river_quality`, `sea_pass_ms`, timestamp queries, `GpuLimits::{river_quality, river_gate_ms_per_mpx, gpu_timing}`), `src/render/src/wgpu.cpp` (adapter enumeration, `ANNO_GPU_ADAPTER`, timestamp feature), `iso.hpp`/`iso.cpp` (`RiverQuality`, `RiverParams::quality`, `kLowTapGain`: the CPU twin has the tiers too), `frame.hpp`/`frame.cpp` (`Chunk::serial`, `FrameRenderer::insert_chunk`), `bench_frame` (`--list-adapters`, `--river-cost`, `--river-pass`, `--river-quality`, `--river-gain`, river statistics in `--check`), the viewer (`ANNO_RIVER_QUALITY=auto|low|medium|high`). Tests: `gpu_test.cpp` (4 cases that need no device: the binding contract, the uniform bytes, the gate logic including unmeasured costs, adapter picking; 6 synthetic ones that run on any adapter and skip without one: shader parity per tier / group / time, chunks at non-zero keys and over their borders, the hour wrap, the legacy look kept, a rebuilt chunk uploaded again, the cost measurement; 2 with game files: Q2 on five islands and Q2 at the Low and Medium tiers; the frame-uniform case extended), `river_shade_test.cpp` (2 CPU cases: the tiers; the lattice stride limit in the config case). The CPU path, the original water and every digest are untouched (section 16.4).

### 16.1 What it does

1. **Bindings (3.3, built as designed except the shear plane).** Chunk flag bit 5 (value 32) = the chunk has a lattice, bits 8-15 its stride in pixels (carried per chunk: 8 / 4 / 4 at groups 0 / 1 / 2 are data, `RiverConfig::lattice_stride`); `@group(1) @binding(6) flow_uv: texture_2d<i32>` (RG16Sint, 1/4096 tiles/s), `@binding(7) flow_aux: texture_2d<u32>` (RGBA8Uint: valid, turbulence, wake, level in 1/2 HEIGHT unit), `@binding(8) flow_shear: texture_2d<u32>` (R8Uint, 1/32 1/s: the baked shear of 15.7). The stride field is 8 bits wide, so strides above `river::kMaxLatticeStride` (255) are refused by `RiverConfig::validate` (a 256 or 512 px stride used to pass validation and made the GPU drop the lattice silently while the CPU drew it); the free bits 16-31 of the flag word are the path to widen it. Dummy 1 x 1 textures when a chunk has no lattice. `@group(0) @binding(7) river: RiverParams` (288 bytes: count, tier, `min_valid`, rapids base, shear fade limits, `kLowTapGain`, then `kMaxRiverLayers` = 8 layers of 32 bytes; unused layers are zero). The frame uniform keeps its 96 bytes: two former padding floats are `river_flow` (1 = flow look) and `river_lambda` (the lift, pixels per HEIGHT unit, `sea::river_view`). `sea_fs` has 14 sampled textures against the default limit of 16. A test reads the shader text and compares every binding number, the flag bits, the layer count and the tier numbers with the C++ constants (`kFlowVelocityBinding`, `kChunkFlagFlow`, `kMaxRiverLayers`, ...), so a one-sided change fails without a GPU.
2. **`river_flow_shade` in WGSL** is the line-by-line twin of `iso::river_flow_shade` and `flow_at` of `river_lattice.cpp` (same operations in the same order, quantised texels decoded as `w * f32(texel) / 4096`, `psi` from the same `noise3`, the hour wrap in the frame's `time_ms`). A pixel without a flow (`valid < min_valid`, no lattice, or `river_flow` off) keeps the legacy shimmer, bit-identical. Original water never reaches the branch.
3. **Upload.** The three planes are uploaded as they are (the vectors' bytes are the texel bytes) with the rest of the chunk, inside the same byte and time budget; planes are pooled per (format, size) (the lattice is 129 x 129 at stride 4, 65 x 65 at stride 8, 257 x 257 at 2). A lattice that does not belong to the chunk (corner not at the chunk origin, or planes of the wrong size) is left out instead of misread (tested). **A chunk rebuilt under the same key is uploaded again** (`Chunk::serial`: every `render_chunk` result and inserted chunk gets a new number, the GPU copy remembers it; 15.6 asked for this), and the planes of the replaced copy go back to the pool.
4. **Tiers (`RiverQuality`).** High: every layer, the ripple light as the slope of the noise (two taps per phase) = the reference look. Medium: the layers that fade with the zoom group's wave detail (the glints and the whitewater streaks) are left out. Low: Medium with one tap per phase (light = amplitude x `kLowTapGain` 1.8 x scale x noise; the standard deviation is 0.97 of Medium's, tested within 12 %). The CPU twin takes the same `quality`, so every tier is testable against the shader. Definitions: OQ-086. At group 2 Medium equals High (the fine layers have no amplitude there).
5. **Default tier.** `GpuLimits::river_quality` (nullopt = automatic): High on a discrete adapter without measuring; Low on a software adapter without measuring; on any other adapter `GpuFrame::create` measures (`measure_river_cost` with a gate: legacy shimmer, then High, Medium; Low is the floor) and takes the highest tier whose cost **added to the legacy shimmer's** is <= `river_gate_ms_per_mpx` (1.0), else Low (`choose_river_quality`, unit-tested including a heavy baseline, the exact gate and the order of the list). The measurement has a wall-clock budget (`GpuLimits::river_measure_budget_ms`, 500 ms, checked before every pass): when it runs out the tier in progress is dropped, the rest are not measured and the choice is Low, so a slow adapter never waits long at startup. A pass whose timestamp reading gives nothing takes the wall clock of that pass (never 0 ms); a cost that is not a positive finite number is unmeasured (`RiverCost::measured`) and never qualifies, and without a measured legacy reference the answer is Low (a driver that reports no time must not get the dearest tier). The library prints nothing: `GpuFrame::river_tier_note()` holds the line (adapter, gate, added costs, or that it was not fully measured) and the viewer and `bench_frame` print it to stderr. Overrides: the limit, `bench_frame --river-quality`, the viewer's `ANNO_RIVER_QUALITY`.
6. **Measured cost.** `measure_river_cost` renders 4 chunks (1024 x 1024 = 1.048576 Mpx) of pure river pixels with a full-amplitude, shear-free lattice (the shear fade removes layers: this is the worst case) at group 1 through the real `sea_fs` pipeline, 3 warm-up and N timed passes per tier, median; GPU timestamps (`WGPUFeatureName_TimestampQuery`, requested when the adapter has it, `Context::timestamps`, period from `wgpuQueueGetTimestampPeriod`) else the wall clock around submit and wait. `GpuLimits::gpu_timing` times the sea pass of every `render()` the same way (`GpuFrame::sea_pass_ms()`, read after the GPU is idle; a driver that reports no time for its first frame gives nullopt, not 0). `bench_frame --river-pass` renders the real view with the legacy shimmer and with the flow and divides the difference by the visible river pixels.
7. **Adapters.** `list_adapters()` / `bench_frame --list-adapters` enumerate every adapter of every backend; `ANNO_GPU_ADAPTER` (a number or a part of the name) picks one for headless contexts (tools, tests, not the window: OQ-088). The Ryzen iGPU can be selected here.

### 16.2 Where it differs from the design above, and why

| Item | Design | Built | Why |
|---|---|---|---|
| Textures | two (`flow_uv`, `flow_aux`) | three: plus `flow_shear` (R8Uint, binding 8) | the shear byte of 15.7 (15.6 offered "a fifth aux byte"; `aux` has four) |
| Layer table | `river_params { count, layers }` | plus `quality`, `min_valid`, rapids base, shear limits, `kLowTapGain` | the scalars of `iso::RiverParams`; layers are padded to 32 bytes (a uniform array stride is a multiple of 16); the size 8 is a WGSL constant compared by a test, not an override |
| Frame uniform | `river_flow` replaces a padding float | and `river_lambda` | the lift of E19 per zoom group is not `raise / 32` (rounded) |
| Quality | Low drops the light taps, Medium drops H | Medium drops the glints and H, Low = Medium with one tap | H alone costs nothing off the falls (OQ-086) |
| Gate | "River pass <= 1.0 ms per 1 M river pixels" | on the **added** cost over the legacy shimmer | the sea pass's own cost is there either way; on the iGPU the legacy look alone costs 8.1 ms per Mpx (OQ-085) |
| Measurement | timestamp queries on the real pass | a synthetic worst-case view (all river, no shear), plus `--river-pass` on real views | the real pass mixes in sea and ground pixels; the synthetic one isolates the river and needs no scene |
| Q2 | whole pixel within 2/255 on 99.9 % | the flow shading alone 99.9 % / max 4 (measured 100 % / 2); the whole pixel 99 % / max 4 (measured 99.4-99.97 %, max 3; pixels under objects excluded, see below) | the CPU path truncates after every step of the sea, the GPU rounds once (OQ-087): the offset is the legacy look's too |
| `--check` reference | the CPU path in legacy mode until S6 | the CPU path in the same river look; river statistics (pure river pixels, blend zone, shading alone) | the GPU draws the flow now |
| Pixels under objects | all river pixels | not counted in the whole-pixel statistic | `object_in_water` sinks objects standing in water on the GPU only (not an S6 change) |
| Naga validation test | a test with naga | the existing "the shared WGSL shaders compile on the device" (wgpu-native validates with naga) plus a text-level binding contract test that needs no device | the C++ build has no naga crate |

### 16.3 Measured (Windows 11, Ryzen 9 9800X3D, MSVC Release; baselines recorded before the first change)

**Parity (Q2).** Synthetic chunk (a lattice that varies in direction, speed, level, shear, turbulence, with a hole without flow and a half-weight rim; a band of rapids; light gain 10 so that a wrong direction shows): GPU against the production CPU functions on every third pixel of 512 x 512, 3 tiers x groups 0 / 1 / 2 x times 0, 700, 1599 and 3,599,950 ms (the hour wrap): **100 % within 2/255, largest difference 1**, on five adapters of this machine (RTX 4090 through Vulkan and DirectX 12, the Radeon iGPU through both, the software rasteriser); the reversed flow as a negative control agrees on less than 60 %. Real scenes (corpus test and `bench_frame --check`, River pixels with no sea blend and no object over them): the owner's canyon at group 1 173,227 px, 99.31 % within 2/255 (about 1,200 px at 3), max 3, and the flow shading alone (GPU flow minus GPU legacy against CPU flow minus CPU legacy) **100 %, max 2**, at times 0 / 700 / 1600 (107,545 river pixels lie under objects and are not counted); Nord, Steppe, Tundra, Praerie islands at groups 0 / 1 / 2, three times each: whole pixel 99.44-99.97 % in the corpus test's windows (the review's own windows reached down to 99.146 %, Steppe at group 1; 14 of 15 views are below the 99.9 % of the spec), shading alone 100 %, max 3 and 2. The legacy look gives the same range (99.42-99.98 %) and its shimmer has the same size on both paths (standard deviation of the 800 ms change of a river pixel 1.19-1.22 levels on the GPU against 1.18-1.21 on the CPU path, asserted within 3 %). **What the whole-pixel statistic leaves out:** the river pixels with an object over them, 38 % of the owner's canyon at group 1 (107,988 of 281,215): the GPU sinks objects standing in water (`object_in_water`) and the CPU path does not, so the whole pixel differs there (62k pixels, largest difference 193, the same before and after S6). The flow shading alone is compared on them too: 97.6-100 % within 2/255, largest difference 11 (asserted at 97 % and 12; `bench_frame --check` prints it); the water under an object is shaded alike, the object's edge is what differs. **Low and Medium against the CPU:** the CPU reference follows the GPU's tier (`FrameRenderer::river_quality`, `bench_frame --check` passes it; before, the check always rendered the High look and Low / Medium showed 23-28 % agreement); on the owner's canyon at group 1 all three tiers agree on 99.29-99.31 % of the pure river pixels (max 3) and the shading alone on 100 % (max 2); the corpus test checks Low and Medium at groups 0 and 1 (and that Low differs from High). The same views in legacy mode give the same whole-pixel numbers (99.28 % on the canyon): the offset is not the shading (OQ-087). `errors() == 0` everywhere.

**Cost per million river pixels** (`bench_frame <game> x --river-cost`, synthetic worst case, group 1, GPU timestamps; ms for 1.048576 Mpx / per Mpx; "added" = tier minus legacy shimmer):

| adapter | legacy | Low | Medium | High | added Low / Medium / High | automatic tier |
|---|---|---|---|---|---|---|
| RTX 4090 (Vulkan, discrete) | 0.043 / 0.041 | 0.059 / 0.057 | 0.071 / 0.067 | 0.085 / 0.081 | 0.016 / 0.026 / 0.040 | High (discrete) |
| Radeon iGPU (Vulkan, integrated) | 8.508 / 8.113 | 10.913 / 10.407 | 12.736 / 12.146 | 14.858 / 14.169 | 2.30 / 4.03 / 6.06 | Low (floor, over the gate) |
| Radeon iGPU (DirectX 12) | 9.982 / 9.520 | 12.129 / 11.567 | 13.941 / 13.296 | 16.087 / 15.342 | 2.05 / 3.78 / 5.82 | Low (floor, over the gate) |

The 1.0 ms gate is met by the RTX 4090 (0.04 added at High) and missed by the iGPU at every tier (OQ-085). The software rasteriser gets Low unmeasured. Tiers cost 38 % (Low), 66 % (Medium) and 100 % of High's added cost on the iGPU. The iGPU's legacy look alone is 8.1 ms per Mpx: its whole sea pass is already far over a 2 ms frame, with or without the flow.

**On real frames** (`bench_frame --river-pass`, the owner's canyon 2494 x 1355, the sea pass with GPU timestamps, legacy against flow, median of 22 frames; river pixels 470,675 / 281,215 / 77,524 at groups 0 / 1 / 2): RTX 4090 (Vulkan) High adds 0.025 / 0.015 / 0.004 ms (0.05 ms per Mpx), Medium 0.017 / 0.011 / 0.004, Low 0.012 / 0.007 / 0.003 on a sea pass of 0.09 / 0.09 / 0.15 ms; the iGPU (Vulkan) adds 3.25 / 2.11 / 0.52 ms at High (6.9 / 7.5 / 6.8 per Mpx), 2.17 / 1.45 / 0.50 at Medium, 1.28 / 0.90 / 0.33 at Low on a sea pass of 7.4 / 7.3 / 25.6 ms (DirectX 12: 2.98 / 1.95 / 0.43 at High on 7.3 / 6.8 / 28.3). Startup: an integrated adapter measures the legacy look, High and Medium once in `create` with 3 warm-up and 3 timed passes each (about 0.2 s on this iGPU, estimated from the pass times); a discrete one measures nothing.

**Zoom stutter rows (Q9)**, `bench_frame --zoom 6 --renderer gpu --limits interactive --chunks async`, enhanced, window-thread time (p50 / p95 / p99 / max ms), before this stage against after: `Single_01.sze` 2560 x 1369 group 0 0.3 / 1.2 / 1.8 / 19.0 against 0.4 / 1.4 / 2.6 / 3.2, group 1 0.3 / 1.2 / 1.7 / 1.9 against 0.3 / 1.3 / 1.8 / 1.8, group 2 0.3 / 1.4 / 1.8 / 3.8 against 0.4 / 1.5 / 1.9 / 2.4; the owner's canyon group 1 0.3 / 1.0 / 1.6 / 2.8 against 0.3 / 1.0 / 1.8 / 2.1; 3840 x 2160 (two repetitions each, groups 0 / 1 / 2): p50 0.4 against 0.5, p95 1.3-1.7 against 1.5-1.8, p99 1.8-2.5 against 2.1-2.7, worst frame 1.9-4.1 against 2.6-4.0 at groups 0 and 1 and, at group 2, an occasional 6-27 ms outlier both before (12.7, 14.1, 24.5, 6.3, 9.7) and after (4.7, 26.8, 21.5, 19.1, 6.3) while other jobs ran. No regression beyond 0.1 ms at the median (the chunk bind groups have three more entries: more validation per draw). The worst busy frame stays in the low milliseconds.

### 16.4 Not changed (checked)

- The CPU path, `iso::river_flow_shade` at High, the layer table and every digest: the original-water digests and the enhanced digests of `render_corpus_test` are identical (the `river_flow_shade` change only adds the tier branch, High is the old code path).
- The GPU's original-water frames (compared pixel for pixel with the CPU within 1 level in the existing tests), the wave simulation, the ships and objects in the water.
- The frame uniform's 96 bytes and every existing binding.

### 16.5 Reproduce

    bench_frame --list-adapters
    bench_frame <game> x --river-cost                                         (ANNO_GPU_ADAPTER=1 for the iGPU)
    bench_frame <game> game/app/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp --styles enhanced --renderer gpu --chunks blocking --groups 1 --res 2494x1355 --at 1155,-74 --t0 700 --check --river-quality high [--river-gain 4]
    bench_frame <game> <scene> --styles enhanced --renderer gpu --chunks blocking --groups 1 --res 2494x1355 --at 1155,-74 --river-pass --river-quality high|medium|low
    ANNO1503_GAME_DIR=game/app ctest --preset dev -R "river"                     (the GPU cases skip without an adapter)

Pictures (not in git): `C:\Users\altin\Pictures\anno-rivers\S6`: `frames\` (the owner's canyon at group 1 on the GPU, 8 frames 250 ms apart at gain 1 and 4, a sequence sheet and the amplified difference of consecutive frames: only the water changes, no seam at chunk borders), `gpu_before` / `gpu_after` (the GPU views of the owner's scene at group 1 and of one island per climate at groups 0 / 1 / 2, before and after), `owner_crop_before_after_cpu.png` (GPU before, GPU after, CPU after).

### 16.6 Issues found in S6 and their tags

| Finding | Tag | Result |
|---|---|---|
| The GPU copy of a chunk was uploaded once per key: a chunk rebuilt after its flow arrived kept the legacy look on the GPU forever (15.6) | fix | `Chunk::serial`, re-upload with budget, pooled planes; tested by inserting a rebuilt chunk (mutation: a serial that is ignored fails) |
| The GPU tests and `bench_frame --check` compared enhanced frames against the CPU's legacy look "until S6" | fix | the reference is the CPU path in the same river look; river statistics added |
| The first `GpuFrame::sea_pass_ms()` of a device reported 0 on the AMD Vulkan driver | fix | a non-positive duration is "no time" (nullopt) |
| Unused uniform layers held the layer default (scale 1) | fix | written as zeros |
| Q2 as written (whole pixel, 99.9 %) is limited by the CPU path's truncation, which the legacy look shares | decision-needed | OQ-087; the flow shading alone meets it (100 %), the whole pixel is asserted at 99 % / max 4 |
| The gate cannot be met by the iGPU at any tier | decision-needed | OQ-085 (Low is the floor); the tiers' meaning OQ-086; the viewer's adapter OQ-088; OQ-079 updated; OQ-084 resolved |
| The GPU window thread's frame cost rose by about 0.1 ms at the median at 4K (bind groups with three more entries) | not a defect | listed in 16.3 |

### 16.8 Review of S6 (2026-10-19): what changed

Two reviewers checked S6 (one on the pictures and the physics, one on the code with 41 hand mutations, 29 killed, 7 survivors that were real test gaps, 5 equivalent). Verdict: sound to commit; the owner's headline goal (visibly flowing rivers) is not met at the defaults, which is a decision (OQ-083).

| Finding | Tag | Result |
|---|---|---|
| The viewer draws the flow, but at the default strength it is not visible as flow: flow light sd 1.09 levels, the in-place sea animation changes 3.7 levels rms per 250 ms against 0.66 for the flow light, median 3.2 px/s, 86 % of river pixels under 6 px/s; block matching 81 % of windows within 45 degrees (250 ms), speed ratio 0.54 | decision-needed | OQ-083 and OQ-065 rewritten with the numbers and the candidates (gain 3-4, `MedianCell`, damping the sea sprite in river pixels, longer periods); the default stays (gain 1) until the owner chooses; "the owner sees the flow" is no longer claimed |
| Rivers without sea contact: the flow runs back against the course at the pseudo outlets, and these are the fastest water of the scene | decision-needed | OQ-067 lists the three sea-less rivers of the owner's island with tile coordinates; `river_qa --sea-less-scale` shows `sea_less_speed_scale` 0 / 0.3 / 1; the default stays 1; do not raise the gain before it is decided |
| `bench_frame --check` rendered the CPU reference at High whatever the GPU's tier (Low and Medium showed 23-28 % agreement and a misleading failure) | fix | `FrameRenderer::river_quality`, the reference follows the GPU; a real-scene Low and Medium test |
| Stale text: OQ-078 "legacy until S6", OQ-083 "nothing changes until S6", OQ-086 "within 3 %" against 12 %, the Q2 range 99.19-99.97 % | fix | updated (this section, 16.1, 16.3, OQ-078, OQ-083, OQ-086, OQ-087) |
| The Q2 statistic excludes the river pixels under objects (38 %) without saying so | fix | stated in 16.3, in OQ-087 and the checklist; the flow shading alone is measured and asserted there too |
| `measure_river_cost` turned a missing timestamp reading or a failed submit into 0.0 ms, so a driver without timestamps got the dearest tier | fix | wall-clock fallback per pass, a failed submit ends the measurement, unmeasured costs never qualify (`RiverCost::measured`), tests for the zero cases |
| The chunk-origin rule (every pixel samples the same global lattice) was only tested with game files (mutation: `flow_at(px)` survived all 17 synthetic cases) | fix | a synthetic test with chunks (0,0), (1,0), (0,1), (1,1) cut from one global lattice, checked over the whole view and 3 px around every border (0 of 129,887 pixels beyond 2/255, max 1); the mutation now fails the multi-chunk case |
| The GPU hour wrap (`time_ms % 3600000`) was untested | fix | the parity test adds 3,600,700 ms; a test renders t, t + 1 h, 2 h and 100 h byte-identical (a time of 360,000,777 ms would be rounded to 32 ms in f32 without the wrap); without the wrap the wrap case and the parity at 3,600,700 ms fail |
| A lattice stride of 256 or 512 passed `validate` and made the GPU drop the lattice silently | fix | `kMaxLatticeStride` 255 refused by `validate`, the 8-bit reason and the widening path documented, tested |
| The automatic tier measured with no total time cap (3 warm-up + 3 timed passes of 1 Mpx per tier, on the window thread) and printed from library code | fix | `GpuLimits::river_measure_budget_ms` 500 ms; `river_tier_note()` for the caller to print |
| The GPU legacy shimmer had no real-scene parity (the tolerance on the synthetic chunk is wider than its signal of a fraction of a level) | fix | Q2 asserts the legacy look's whole pixel 99 % / max 4 on every view, and the size of the shimmer (sd of the 800 ms change, 1.19-1.22 levels) within 3 % at the tiers test |
| The `min_valid` threshold of the legacy fallback was not pinned (mutations `> 0.0` and `/254` survived) | fix | the synthetic lattice has texels of weight 0.01 and 0.03 around the threshold 0.02; `> 0.0` now fails the parity test (`/254` moves the boundary by 0.4 % of a texel: not pinned at pixel resolution) |
| `ANNO_GPU_ADAPTER` with a number that overflows threw `std::out_of_range` | fix | `pick_adapter` (pure function, `std::from_chars`), a device-free test of index, name part, overflow, no match |
| The synthetic GPU parity tests skip silently on the Linux CI job (no Vulkan driver) | decision-needed | OQ-089; default: a CI warning annotation says so |
| A river chunk creates 8 textures instead of 5 and the pool grew by 72 MB | decision-needed | OQ-090; default as built |

Not changed: the CPU path, the original water, every digest (the original-water and enhanced digests of `render_corpus_test` are identical), the shader, the tier arithmetic, the cost numbers of 16.3.

### 16.7 Open for S7 and later

- Turbulence, wake, obstacles, Froude streaks, standing waves, thalweg shift, `object_in_water` along the flow (S7): the lattice already carries `turb` and `wake` bytes (0 until then) and the shader reads them (`turb` scales the fading layers).
- If the iGPU matters: the noise-texture variant of the shading (4 texture loads instead of 8 integer hashes per noise tap) is the S9 optimisation; this stage measured the cost (16.3) and did not build it (a visible change of the noise, decision OQ-085/086).
- Q4 on the GPU (lattice texels at chunk seams) follows from the lattice being bit-identical per pixel (S5) and the GPU reading the same bytes; the difference sheets show no seam (16.5).
