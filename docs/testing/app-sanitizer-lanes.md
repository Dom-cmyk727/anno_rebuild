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

## Child diagnostics and the viewer verdict (W4-WIN-VERIFY V1)

The gate counts sanitizer lines in the test output it reads. A viewer case runs the window as a child of
`tools/test/viewer_wiring.py`, so what the child writes is invisible to the gate unless the wrapper passes it on.
The wrapper keeps both streams as `<out>.stdout.txt` and `<out>.stderr.txt`, scans them with
`ctest_verdict.SANITIZER` (the one definition: AddressSanitizer, LeakSanitizer, ThreadSanitizer,
UndefinedBehaviorSanitizer, `runtime error:`), prints the lines with their file and line number and fails the
case, whatever the exit status and the session record say. The expected SDL delivery messages of the
`cpu_delivery_failures` case are not sanitizer lines. App targets do not set `-fno-sanitize-recover`, so UBSan
only prints; the wrapper therefore also sets `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` for the child
unless the caller has set it (defence in depth, not the contract). The earlier scan of `/tmp/viewer_wiring_*`
stderr files found zero files and so provides no evidence; the three reruns with kept stderr
(`docs/orchestration/evidence/w4-win-final/extracts/wsl-viewer-runs-summary.txt`) are the earlier evidence, and
the W4-WIN-VERIFY gate runs are the first to scan child streams as part of the verdict.
