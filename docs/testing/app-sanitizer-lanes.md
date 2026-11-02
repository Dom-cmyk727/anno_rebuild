# App sanitizer lanes: the viewer-enabled build and the full-corpus cases (W4-WIN-FINAL)

Added on the W4.4 Windows branch; read with `docs/testing/README.md` and `evidence-contract.md` on `main`.

## The two full-corpus cases are heavy, with a sanitizer timeout

`anno_app_tests` has two cases that run every scenario file of the original game with every assertion:

- `play: islands are the ones the sim placed` (50 scenarios, 5 492 assertions)
- `play: tick 0 matches the viewer except objects the sim did not place`

They are in the doctest suite `heavy` (`src/app/tests/play_test.cpp`), so CTest registers them once, with the
label `heavy` and `ANNO_HEAVY_TEST_PROCESSORS` slots (the other app cases are unchanged), and in a **sanitizer**
build (`ANNO_SANITIZE=ON`) they get `TIMEOUT 8000` (`src/app/AnnoAppSanitizerTimeouts.cmake`, read after the cases
are discovered). Reason: alone under ASan+UBSan the first needed 5 100-5 400 s and the second 2 313 s in a loaded
run, against the gate's default of 3 400 s (W4-WIN-CLOSE C3: the case passes alone, it is slow, not stuck). No
scenario, assertion or global timeout is reduced or raised; a non-sanitizer build has no timeout property.

Lanes, which together are the whole required app inventory:

```
# ordinary lane (everything but the heavy cases)
tools/test/run_selected.py --source-root <src> --build-dir <build> --select '^anno_app(_alloc|_input)?_tests\.|^viewer_wiring' \
    --exclude-label heavy --require-test viewer_wiring_contract --require-test window_table_contract ...
# corpus lane (exactly the two cases, serially, once, after the source is frozen)
tools/test/run_selected.py ... --select '^anno_app_tests\.play: (islands are|tick 0 matches)' --label heavy
```

Check the generated metadata before the expensive lane: `ctest --show-only=json-v1` lists each of the two names
once with the label `heavy`, `PROCESSORS` and (sanitizer builds) `TIMEOUT 8000`.

## The viewer-enabled sanitizer build

`viewer.cpp` and the SDL input cases are built only with `ANNO_BUILD_VIEWER=ON`. On WSL, SDL's X11/Wayland check
fails without desktop development packages; the smallest supported headless setup is
`-DSDL_UNIX_CONSOLE_BUILD=ON` (SDL3 builds with its dummy video driver; no package install). The GPU library
stays on, as in every CI configuration (`bench_picture`, which the shell contracts need, does not build with
GCC `-Werror` without it): `viewer.cpp` is then instrumented with its GPU code too, and
`tools/test/viewer_wiring.py` drives the CPU/SDL-dummy window (`SDL_VIDEODRIVER=dummy --renderer cpu`):

```
cmake -S <src> -B ~/anno-build/w4-asan-viewer -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-13 \
      -DANNO_SANITIZE=ON -DANNO_BUILD_TESTS=ON -DANNO_BUILD_VIEWER=ON -DANNO_BUILD_GPU=ON -DSDL_UNIX_CONSOLE_BUILD=ON
```

A headless CPU run says nothing about the GPU path actually rendering or about a display (the sanitizer build cannot create a GPU device here). `tools/check_unix_warnings.sh` also
compiles the app sources without `ANNO_VIEWER_GPU` with GCC (it found an unused function there).
