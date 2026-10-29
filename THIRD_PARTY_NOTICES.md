# Third-party software

This project is licensed under the GNU General Public License, version 3 or (at your option) any later version (`LICENSE`). It uses the following third-party software, each under its own license. Builds that bundle them (for example the preview packages) must ship these notices.

| Component | Version | Used by | License | Source |
|---|---|---|---|---|
| SDL 3 | 3.4.18 | `anno1503` (window, input) | zlib | https://github.com/libsdl-org/SDL |
| wgpu-native | v29.0.1.1 | `anno1503`, `bench_frame` (GPU path) | MIT OR Apache-2.0 | https://github.com/gfx-rs/wgpu-native |
| doctest | 2.4.12 | tests only, not shipped | MIT | https://github.com/doctest/doctest |
| Rust crates in `Cargo.lock` | as locked | the Rust reference in `crates/` | their own licenses (`cargo metadata`) | https://crates.io |

The versions are pinned in `CMakeLists.txt`, `cmake/WgpuNative.cmake` and `Cargo.lock`. Update this table when one changes.

# The original game

ANNO 1503 is © its rights holders (Max Design, Sunflowers, now Ubisoft). "ANNO" is a trademark of its owner. This project is not affiliated with or endorsed by them. It contains no original game files. You need your own legally obtained copy of the game to run it.
