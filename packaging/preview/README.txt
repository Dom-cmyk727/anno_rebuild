ANNO 1503 rebuild - preview build (@NAME@, git commit @COMMIT@)
=========================================================================

What this is
------------
A clean-room reimplementation of ANNO 1503 in C++20. This build is an
island VIEWER, not the playable game yet: it reads the original game files
and draws scenarios and islands isometrically (ground, terrain height,
animated sea, rivers and surf, trees, rocks, buildings, shadows, ships,
soldiers). No gameplay, no civilians, no sound.

New in this build (since 2026-10-18b)
-------------------------------------
- Smooth zoom: the mouse wheel zooms continuously (about 10 % per notch,
  eased, around the point under the cursor) instead of jumping between
  three zoom steps. + / - zoom in bigger steps, Home or 0 goes back to the
  default zoom. You can zoom out much further than the original, until the
  whole map fits the window. --zoom-mode stepped gives back the original's
  three steps (the CPU renderer always zooms in steps).
- Zooming and panning at the same time no longer show open sea or empty
  patches: until the new zoom level's map areas are ready, the viewer shows
  the nearest ready level scaled and fades the sharp picture in; the areas
  ahead of a moving view are prepared in advance, and preparing one takes
  about 40 % less time.
- No more stutter when zooming (your Linux log of 2026-10-18 showed
  60-100 ms frames after zooming): the wave simulation and the uploads to
  the GPU no longer block the window. With E on, right after a zoom the
  coast shows calm waves for a moment and fades into the simulated ones.
- Frame-hitch log: when more than 30 ms pass between two frames, the
  console prints a "frame hitch:" line with the steps that took the time, and
  every 2-second report line names the steps of the slowest frame.
- Enhanced water (E): rivers FLOW. A flow computed from the island's
  terrain (downhill from source to mouth) moves the light on the water.
  Key R switches between the flow and the old look ("legacy rivers" in the
  window title). --river-gain <x> sets how strongly the flow shows
  (default 4), --river-flow legacy starts with the old look. The quality
  is chosen for your GPU (high on a dedicated graphics card; the console
  line "GPU renderer: ..." names it); set ANNO_RIVER_QUALITY=low, medium
  or high to override it.
- Enhanced water (E): rivers carry water along the whole river bed, no dry
  gaps any more (river pieces up to 8 tiles apart are joined;
  --river-gap-patch <tiles>, 0 = off).
- Choice of GPU on PCs with more than one (e.g. a laptop with integrated
  and dedicated graphics): "bench_frame --list-adapters" lists them, and
  ANNO_GPU_ADAPTER=<number or part of the name> picks one. The window title
  names the GPU in use.
- --water enhanced now really starts in enhanced water (it always started
  in the original water before).

Previous build (2026-10-18b)
----------------------------
- Enhanced water (E): rivers run through their waterfalls and mouths into
  the sea as one water surface, after your feedback; rocks and other
  objects standing in water sink in at their base instead of floating.

Earlier builds
--------------
- 2026-10-18a: the first build of the C++ version (same pictures as the
  Rust version, checked pixel by pixel). Ships sit IN the water with
  enhanced water: the hull sinks below the surface, foam marks the
  waterline, and the ship rises, falls and pitches with the waves.
- GPU renderer (Vulkan, DirectX 12, Metal or OpenGL via wgpu-native) with
  a CPU fallback (force one with --renderer gpu or --renderer cpu); map
  areas are prepared on all CPU cores in the background.
- Enhanced water (E) is a real-time wave simulation on the GPU: waves slow
  down and steepen in shallow water, bend around islands, break into foam
  and run up the beach and back.
- Soldiers in their owner's colour; every ship a scenario places, with its
  own ship type (sails and flags still white).
- Endless games (Scenes/0010SingleEndlos) open. Their scenario files only
  say "an island of this climate and size goes here"; until we know how the
  original picks the island, the viewer puts a real island of that climate
  and size there, always the same one for the same map. These islands are
  stand-ins and will not match what the original shows.

Requirements
------------
- Windows 10/11 64-bit, or Linux x86_64 (glibc 2.35 or newer, e.g.
  Ubuntu 22.04 / Debian 12 / Fedora 36 or later; X11 or Wayland desktop).
- Your own copy of ANNO 1503 (GOG "A.D." edition, 2.0.0.5). The rebuild reads
  the original files at runtime; they are NOT included. Point it at the folder
  that contains Anlagen.dat: the GOG install folder on Windows; on Linux e.g.
  the Wine install folder (.../drive_c/GOG Games/ANNO 1503) or a folder
  extracted with innoextract from the GOG installer.

Start
-----
Windows: double-click start-viewer.bat, enter your ANNO 1503 folder.
Linux:   ./start-viewer.sh "/path/to/ANNO 1503"
Both open the first single-player scenario (Scenes/0200Single/Single_01.sze).
Another scenario or island: pass it as the second argument, e.g. the jungle
island with the canyon river:
  start-viewer.bat "<game folder>" "<game folder>\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp"
  ./start-viewer.sh "<game folder>" "<game folder>/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp"

Controls
--------
  drag (left mouse), arrow keys or WASD  pan
  mouse wheel                            zoom smoothly (at the cursor)
  + / -                                  zoom in bigger steps
  Home or 0                              back to the default zoom
  Space                                  pause / resume the water animation
  E                                      enhanced water on/off (see below)
  R                                      river flow on/off (enhanced water)
  Esc or Q                               quit

Enhanced water (E)
------------------
Off by default. A real-time wave simulation: waves grow towards the coast,
break into foam, run up the beach and wash back; ships sit in the water and
move with the waves; rivers flow. This is NOT in the original game; it is
an optional improvement. The default shows the water as the original draws
it.

Choosing the GPU
----------------
Windows: windows\bench_frame.exe --list-adapters   (in a command prompt)
Linux:   linux/bench_frame --list-adapters
Then start with ANNO_GPU_ADAPTER set to a number from that list or a part
of the name, e.g. Windows: set ANNO_GPU_ADAPTER=1 before start-viewer.bat
in the same command prompt; Linux: ANNO_GPU_ADAPTER=1 ./start-viewer.sh ...

Benchmark
---------
Windows: double-click run-benchmark.bat.  Linux: ./run-benchmark.sh "<game folder>"
It renders scripted camera paths without a window (GPU and CPU, 1080p to
4K) and writes benchmark-results.md next to the script (1-3 minutes).

Please report
-------------
- Whether it starts (the exact error text if not), the GPU named in the
  window title, and the first console lines ("GPU renderer: ...").
- benchmark-results.md.
- Zoom: turn the wheel in and out quickly, also while dragging the map,
  over islands and open sea. Does it feel smooth? Any empty or flickering
  patches, areas that stay blurry, or jumps? Please copy any
  "frame hitch:" lines and a few of the 2-second report lines (fps, worst
  frame) from the console.
- Rivers with E on (e.g. the canyon island above, zoomed in): do they
  visibly flow, and in the right direction? Compare with R (old look). Is
  the flow too strong or too weak (try --river-gain 2 and --river-gain 8)?
  Any river stretch without water? And again the river mouths into the
  sea, which you found choppy and unnatural on 2026-10-18: do they look
  natural now, with the flow? A short screen recording helps.
- With E on, zoomed in fully on a ship: do the hull, the waterline foam and
  the bobbing look right?
- If your PC has two GPUs: the list from bench_frame --list-adapters and
  whether ANNO_GPU_ADAPTER switches to the other one.

Command-line tools (windows\ or linux/)
-------------------------------------
  anno1503 --view <file.sze|file.scp> [--game-dir <dir>]
           [--water original|enhanced] [--renderer auto|gpu|cpu]
           [--river-flow legacy|flow] [--river-gain <x>]
           [--river-gap-patch <tiles>] [--zoom-mode continuous|stepped]
           [--zoom-step <factor>] [--zoom-ease <ms>] [--min-zoom <tile width>]
           [--png out.png [--group <n>] [--time <ms>]]
      --png renders the whole scene to an image without opening a window
      (--group 0 near ... 2 far; 3 and up: the further zoomed-out levels).
  bench_frame <game dir> <file> [options]    frame benchmark (see above)
  bench_frame --list-adapters                list the GPUs
  anno-dump tex|figuren|anlagen|insel|colors|chunks ...   inspect file formats
  Run any tool without arguments for its usage.

Known limitations of this preview
---------------------------------
- Built on a Windows 11 PC in Docker containers (Windows: MinGW-w64,
  linked statically; Linux: Ubuntu 22.04). We ran the Windows build on that
  PC (RTX 4090) and the Linux build only without a window (no GPU) in a
  container. Please report any start-up problem with the exact error text.
- The river flow's strength and speed are still being tuned (your opinion
  helps), and the river mouths are still being worked on.
- anno-extract (graphics export) is not ported to C++ yet and is missing
  from this build; earlier packages have it.
- The enhanced water is an improvement on the original, not part of it,
  and still being tuned; E off shows the water as the original draws it.
- Only water (and ships bobbing in enhanced water) is animated so far;
  other animations, moving ships, people and the original's terrain
  lighting are missing.
- Details and evidence: docs/PLAN.md, docs/render/iso.md,
  docs/render/rivers.md, docs/perf/README.md and docs/open-questions.md in
  the source.

Source
------
anno1503-source.zip is a snapshot of the repository at the commit above
(code, specs, tests; no game files). Build (CMake 3.25+, a C++20 compiler):
  cmake --preset release && cmake --build --preset release
