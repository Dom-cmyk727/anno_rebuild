# Rivers (enhanced water): flow, gap fix, zoom spikes

**Status 2026-10-19 (after the investigation run):** findings 2 and 3 are **already fixed** by a separate, earlier investigation (commits "fix(render): enhanced rivers carry water along the whole painted bed" and "fix(render): zoom changes and chunk bursts no longer block the window thread"; their causes and numbers are in `iso.md` "River gaps", `docs/perf/README.md` "2026-10-19 — zoom stutter" and the `river_qa` tool). They supersede stages S0–S3 below where they overlap (the gap fix used an interpolating Catmull-Rom bed plus a painted-bed floor instead of the per-piece `tau + 0.5` mask of S3; the spike fix is `GpuLimits::interactive()`); what remains open is **finding 1, the flow itself: stages S4–S9** (hydraulics, lattice bake, shading, GPU port, extras, options). Before S4, re-read S3's mask against the shipped gap fix and reuse the shipped one. Original status line of the design: design, nothing implemented yet (2026-10-19, tree `359b1ce`). Written for the owner feedback of the same day (enhanced water, Windows PC): (1) rivers have no flow physics, only a pattern sliding along one fixed axis; (2) enhanced rivers have gaps where the original has none (**fix**); (3) 85-132 ms frame spikes when the zoom group changes (**fix**). Companion to [`iso.md`](iso.md) (Water, Enhanced water); once a stage lands, the matching rows of `iso.md` are updated and this page keeps the physics, the contracts and the QA method.

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
    c_k   = S(u, kappa_l) * ( x - u * (T_l / 1000) * phi_k ) * s_l + o_k       S stretches along u by 1/(1 + kappa_l abs(u))
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

struct LayerConfig { float scale, period_ms, amplitude, stretch; bool detail_fades; };

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
};
struct FlowSample { float vx = 0, vy = 0, turb = 0, wake = 0, level = 0, valid = 0; };
FlowSample flow_at(const FlowPlane&, int local_x, int local_y);  // valid-weighted bilinear at the pixel centre (+0.5)
}
```

`Chunk::flow` is empty when the chunk has no river pixels. GPU: chunk flags bit 5 = flow plane present, bits 8-15 = lattice stride (the 16-byte chunk struct is unchanged); `@group(1) @binding(6) flow_uv: texture_2d<i32>` (`RG16Sint`) and `@binding(7) flow_aux: texture_2d<u32>` (`RGBA8Uint`), both read with `textureLoad` and bilinear weights computed in the shader (identical arithmetic on CPU); dummy 1 x 1 textures when absent. The `sea_fs` layout then holds 13 sampled textures against the default limit of 16. Frame uniform stays 96 bytes (a padding float becomes `river_flow`). The **layer table is a separate uniform buffer** `river_params { count: u32, layers: array<RiverLayer, kMaxRiverLayers> }` with `kMaxRiverLayers = 8` (reason: a uniform array needs a compile-time size; raising it is one constant plus a shader override), filled from `RiverConfig::layers` and validated at load (every period divides 3,600,000 ms, count <= 8). The lattice strides are derived from the zoom-group tile width (`stride = largest divisor of the chunk size close to 0.1 tile`, defaults 8/4/4 px = 0.10/0.10/0.20 tile at groups 0/1/2), not hard-coded in the shader (carried per chunk in the flags).

```wgsl
struct RiverLayer { scale: f32, period_ms: f32, amp: f32, stretch: f32 }
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
| **S4** hydraulics, CPU only, no render change | `RiverFlow`: linear conveyance solve with bank closure, nested PCG, Eikonal fallback, discharge options, falls speed, ponds, bed smoothing; corpus build | all 10.1 flow cases; Q12 on all 72 islands with the 1 s hard budget **before S5 starts**; fallbacks and sea-less components listed; determinism (1 vs N threads) |
| **S5** lattice + CPU shading (first owner-visible flow) | `Chunk::flow`, `flow_at`, grown-probe apron, `iso::river_flow_shade`, `shade_sea` accessor, layer-table uniform contract on the CPU side, water-plane coordinates, `--river-flow`, `flow_pending` path; flow default **on** for Enhanced | owner sees flow along the course on `--png --time`; Galilean, pulsing, hour-wrap, thin-river cases; Q3 (CPU), Q4 lattice bitwise, Q5 on the CPU path (angle <= 15 degrees, speed ratio [0.7, 1.3], coverage >= 99.9 %), Q10; apron cost <= +5 % per river chunk; forced-pending test; enhanced digests re-recorded |
| **S6** GPU port and quality | bindings 6 and 7, flags, uniform, WGSL, naga validation, dummy textures, `RiverQuality`, adapter-based default, timestamp queries | Q2 (99.9 % within 2/255, max 4); `errors() == 0`; river-pass cost per 1 M river pixels measured on the RTX 4090 and the Ryzen iGPU with the default chosen by the 1.0 ms gate; no regression of the Q9 rows |
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
