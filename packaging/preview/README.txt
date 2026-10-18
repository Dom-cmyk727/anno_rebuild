ANNO 1503 rebuild - preview build (@NAME@, git commit @COMMIT@)
=========================================================================

What this is
------------
A clean-room reimplementation of ANNO 1503 in C++20 (the earlier previews
were built from the Rust version). This build is an island VIEWER, not the
playable game yet: it reads the original game files and draws scenarios and
islands isometrically (ground, terrain height, animated sea, rivers and
surf, trees, rocks, buildings, shadows, ships, soldiers). No gameplay, no
civilians, no sound.

New in this build
-----------------
- Enhanced water (E): rivers now run through their waterfalls and mouths
  into the sea as one water surface (no dark patches with straight edges,
  no hard line at the coast, no foam on rivers), after your feedback.
- Enhanced water (E): rocks and other objects standing in water sink in at
  their base instead of floating on it; foam only at the sea.

Previous build (2026-10-18a)
----------------------------
- The whole viewer is now the C++ version: same pictures (checked pixel by
  pixel against the Rust version), same controls and options.
- Ships sit IN the water with enhanced water (E): the lowest part of the
  hull sinks below the surface (tinted and fading into the sea), broken
  foam marks the waterline, and the ship rises, falls and pitches with the
  simulated waves. E off shows ships as the original draws them.
- GPU: Vulkan, DirectX 12, Metal or OpenGL (via wgpu-native); on the
  developer's machine 120 fps (the display rate) at 2560 x 1600 with
  0.2-0.4 ms of CPU time per frame.
- Faster frame waiting in the benchmark; fixes for the OpenGL backend.

Earlier builds
--------------
- Soldiers on the ground in their owner's colour; a few building pixels
  that were cut off.
- Ships: every ship a scenario places is drawn on the sea with its own
  ship type. Sails and flags are still white; player colours come later.
- Endless games (Scenes/0010SingleEndlos) open now. Their scenario files
  only say "an island of this climate and size goes here"; the original
  presumably picks the island when the game starts. Until we know how,
  the viewer puts a real island of that climate and size there, always
  the same one for the same map. So these islands are stand-ins and will
  not match what the original shows.

- Enhanced water (E) is now a real-time wave simulation on the GPU:
  waves slow down and steepen in shallow water, bend around islands,
  break into foam that drifts with the water, and run up the beach and
  back. Give it a few seconds after starting or pressing E. Rivers are
  smooth with soft banks, no waves.
- The GPU draws the frames now (Vulkan, DirectX 12 or Metal),
  presented in sync with your display. Measured on a 4K screen: 1-2 ms per
  frame on the developer's machine, before 7-25 ms on the CPU. Without a
  usable GPU the viewer falls back to the CPU renderer by itself
  (force one with --renderer gpu or --renderer cpu).
- Map areas are prepared on all CPU cores in the background, about 8x
  faster than before; the area around the view and the neighbouring zoom
  levels are prepared ahead, so zooming in shows islands at once.
- Enhanced water (E) reworked after your video: no white blobs or grid
  stripes on the open sea, no lines along the coast; waves now roll in,
  curl, break into foam and wash up the beach; shallow water is turquoise.
- Enhanced water (E): deep water now hides the sea floor smoothly, so the
  tile-shaped edges between near-island water and deep sea are gone (E off
  keeps the original look on purpose).
- Zooming no longer stutters: sea images of all zoom levels are uploaded
  to the GPU in the background.
- The window title shows the renderer and the frames per second; the
  console prints a timing line every 2 seconds.

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
Another scenario or island: pass it as the second argument, e.g.
  ./start-viewer.sh "<game folder>" "<game folder>/Scenes/Islands/Nord/N_128x128_0049_1B_Indianer.scp"

Controls
--------
  drag (left mouse), arrow keys or WASD  pan
  mouse wheel, + / -                     zoom (3 levels)
  Space                                  pause / resume the water animation
  E                                      enhanced water on/off (see below)
  Esc or Q                               quit

Enhanced water (E)
------------------
Off by default. A real-time wave simulation: waves grow towards the coast,
break into foam, run up the beach and wash back; ships sit in the water and
move with the waves. This is NOT in the original game; it is an optional
improvement. The default shows the water as the original draws it.

Benchmark
---------
Windows: double-click run-benchmark.bat.  Linux: ./run-benchmark.sh "<game folder>"
It renders scripted camera paths without a window (GPU and CPU, 1080p to
4K) and writes benchmark-results.md next to the script (1-3 minutes).

Please report
-------------
- benchmark-results.md.
- The fps in the window title and a few of the console lines, for each
  zoom level, with E off and on.
- Whether panning and zooming feel smooth, and a short recording of the
  enhanced water (E) at a coast and around a ship (zoom in fully on a
  ship: do the hull, waterline and foam look right?).

Command-line tools (windows\ or linux/)
-------------------------------------
  anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group 0|1|2]
           [--png out.png [--time <ms>]] [--water original|enhanced]
           [--renderer auto|gpu|cpu]
  bench_frame <game dir> <file> [options]    frame benchmark (see above)
      --png renders the whole scene to an image without opening a window.
  anno-dump tex|figuren|anlagen|insel|colors|chunks ...   inspect file formats
  Run any tool without arguments for its usage.

Known limitations of this preview
---------------------------------
- Built by cross-compiling on macOS (Windows: MinGW-w64; Linux: Ubuntu
  22.04 in Docker); not run on a Windows or Linux PC by us. Please report
  any start-up problem with the exact error text.
- anno-extract (graphics export) is not ported to C++ yet and is missing
  from this build; earlier packages have it.
- Rivers still show as hard tile-shaped patches with seams (next fix).
- The enhanced water is an improvement on the original, not part of it,
  and still being tuned; E off shows the water as the original draws it.
- Only water (and ships bobbing in enhanced water) is animated so far;
  other animations, moving ships, people and the original's terrain
  lighting are missing.
- Details and evidence: docs/PLAN.md, docs/render/iso.md, docs/perf/README.md
  and docs/open-questions.md in the source.

Source
------
anno1503-source.zip is a snapshot of the repository at the commit above
(code, specs, tests; no game files). Build (CMake 3.25+, a C++20 compiler):
  cmake --preset release && cmake --build --preset release
