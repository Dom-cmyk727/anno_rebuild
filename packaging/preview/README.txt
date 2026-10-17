ANNO 1503 rebuild - preview build (@NAME@, git commit @COMMIT@)
=========================================================================

What this is
------------
A clean-room reimplementation of ANNO 1503 in Rust (project milestone M1.6).
This build is an island VIEWER, not the playable game yet: it reads the
original game files and draws scenarios and islands isometrically (ground,
terrain height, animated sea, rivers and surf, trees, rocks, buildings,
shadows). No gameplay, no ships or people, no sound.

New in this build
-----------------
- Much faster: frames are composed from cached chunks; new map areas are
  prepared in the background (they show open sea for a moment instead of
  freezing the window). Redraws at your display's refresh rate.
- Frames per second in the window title. Every 2 seconds the console
  (Windows: the black window; Linux: the terminal) prints a line like
  "zoom group 1: 60.0 fps, frame avg 4.1 ms / worst 6.0 ms (...)".
- The sea now covers the whole map, also between islands, and continues
  seamlessly into the island water (no more dark void, no rectangles).
- Smooth transition from shallow to deep water (no staircase at the coast);
  the open-sea colour is now close to the original.
- Enhanced water (E) blends smoothly between animation frames.

Requirements
------------
- Windows 10/11 64-bit, or Linux x86_64 (glibc 2.28 or newer, X11 or
  Wayland desktop).
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
Off by default. Adds crashing waves: they grow towards the coast, break
into whitewater, burst onto the beach as foam and wash back; whitecaps on
the open sea. This is NOT in the original game; it is an optional
improvement. The default shows the water as the original draws it.

Please report
-------------
- The fps in the title and a few of the console lines, for each zoom level,
  with E off and on, plus your CPU, number of cores and screen resolution.
- Whether panning is smooth, and a short recording if anything looks wrong.

Command-line tools (windows\ or linux/)
-------------------------------------
  anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group 0|1|2]
           [--png out.png [--time <ms>]] [--water original|enhanced]
      --png renders the whole scene to an image without opening a window.
  anno-extract view|figuren|anlagen|tex|insel ...   export graphics to PNG
  anno-dump tex|figuren|anlagen|insel|colors|chunks ...   inspect file formats
  Run any tool without arguments for its usage.

Known limitations of this preview
---------------------------------
- Built by cross-compiling on macOS; not run on Windows or Linux by us.
  Please report any start-up problem with the exact error text.
- Rivers still show as hard tile-shaped patches with seams (next fix).
- Only water is animated so far; other animations, ships, people and the
  original's terrain lighting are missing.
- Details and evidence: docs/PLAN.md, docs/render/iso.md, docs/perf/README.md
  and docs/open-questions.md in the source.

Source
------
anno1503-source.zip is a snapshot of the repository at the commit above
(code, specs, tests; no game files). Build: rustup (stable), cargo build --release
