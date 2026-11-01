# Window workloads (round ec, D)

These are the command files the round-ec window matrix ran (`docs/perf/README.md`, "The play window,
traced"). They are legal builds for the developer city of `data/maps/lan5.txt`, found by a trial world with
`workload_commands.cpp`. Acceptance in a measured run is read from that run's own `sim.tsv`, never from the
trial.

| file | contents |
|---|---|
| `idle.commands` | none |
| `one.commands` | one build at turn 30 |
| `burst.commands` | 20 builds in turn 30 (one batch, one island version) |
| `sustained.commands` | 60 builds, one every 4 turns from turn 30 |

To regenerate them, link `workload_commands.cpp` against a build's libraries. It is not a CMake target.
On the Mac:

```
c++ -O2 -std=c++20 -Isrc/{app,render,assets,formats,sim_load,sim}/include workload_commands.cpp \
  build/release/src/{app/libanno_app_core,render/libanno_render,sim_load/libanno_sim_load,sim/libanno_sim,assets/libanno_assets,formats/libanno_formats}.a -lpthread
./a.out <game_dir> data/maps/lan5.txt one.commands one 1 30 1
./a.out <game_dir> data/maps/lan5.txt burst.commands burst 20 30 1
./a.out <game_dir> data/maps/lan5.txt sustained.commands sustained 60 30 4
```

`tools/bench/window_matrix.sh` runs the whole matrix.
