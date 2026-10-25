#!/usr/bin/env bash
# The CI's GCC/Clang warning gate, run locally (TASKS.md §1 point 9; on the
# Windows PC in WSL). Compiles every .cpp under src/ with the CI's warning
# flags at -O3 and -Werror, compile only (no link), once with GCC 13 and once
# with Clang 18, with the viewer and GPU code switched on (ANNO_WITH_VIEWER,
# ANNO_VIEWER_GPU, ANNO_BENCH_GPU: every CI configuration builds them); then
# the app sources once more with Clang without ANNO_VIEWER_GPU (a viewer built
# without wgpu, -Wunused-private-field). MSVC reports most of these warnings
# not at all, so a clean MSVC build says nothing about them.
#
# usage: tools/check_unix_warnings.sh [deps-dir]
#   deps-dir: the FetchContent _deps folder of a configured build (doctest,
#   SDL3 and wgpu-native headers). Default: build/msvc/_deps, else the
#   FETCHCONTENT_BASE_DIR in build/msvc/CMakeCache.txt (a worktree whose build
#   shares the main checkout's deps).
# env: GXX     GCC to use (default g++-13, else g++)
#      CLANGXX Clang to use (default clang++-18 on PATH, else the unpacked
#              Ubuntu packages in ~/clang18, see docs/orchestration/REPORTS.md
#              T12); without one the Clang passes are skipped with a message
#      JOBS    parallel compiles (default: nproc)
# From Windows: wsl -d Ubuntu -- bash tools/check_unix_warnings.sh
# Exit status: 0 clean, 1 a file failed, 2 setup error.
set -u

repo=$(cd "$(dirname "$0")/.." && pwd)
jobs=${JOBS:-$(nproc)}

deps=${1:-}
if [ -z "$deps" ]; then
    if [ -d "$repo/build/msvc/_deps" ]; then
        deps="$repo/build/msvc/_deps"
    elif [ -f "$repo/build/msvc/CMakeCache.txt" ]; then
        win=$(sed -n 's/^FETCHCONTENT_BASE_DIR:PATH=//p' "$repo/build/msvc/CMakeCache.txt" | tr -d '\r')
        # C:/Users/... -> /mnt/c/Users/...
        deps=$(printf '%s' "$win" | sed -E 's#^([A-Za-z]):#/mnt/\L\1#')
    fi
fi
if [ -z "$deps" ] || [ ! -d "$deps/doctest-src" ]; then
    echo "check_unix_warnings: no deps folder with doctest-src (configure the msvc preset" \
         "first or pass the _deps folder)" >&2
    exit 2
fi

gxx=${GXX:-}
if [ -z "$gxx" ]; then
    if command -v g++-13 >/dev/null; then gxx=g++-13; else gxx=g++; fi
fi
clangxx=${CLANGXX:-}
if [ -z "$clangxx" ]; then
    if command -v clang++-18 >/dev/null; then
        clangxx=clang++-18
    elif [ -x "$HOME/clang18/root/usr/lib/llvm-18/bin/clang++" ]; then
        clangxx="$HOME/clang18/root/usr/lib/llvm-18/bin/clang++"
        export LD_LIBRARY_PATH="$HOME/clang18/root/usr/lib/x86_64-linux-gnu:$HOME/clang18/root/usr/lib/llvm-18/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
fi

# The flags of cmake/AnnoWarnings.cmake plus the release preset's -O3 -DNDEBUG.
flags="-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
-Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -ffp-contract=off -Werror"
inc=""
for d in "$repo"/src/*/include; do inc="$inc -I$d"; done
inc="$inc -isystem $deps/wgpu_native-src/include -isystem $deps/sdl3-src/include"
inc="$inc -isystem $deps/doctest-src"
common_defs="-DDOCTEST_CONFIG_USE_STD_HEADERS -DANNO_FIXTURES_DIR=\"$repo/fixtures/synthetic\""
gpu_defs="-DANNO_WITH_VIEWER -DANNO_VIEWER_GPU -DANNO_BENCH_GPU"

mapfile -t all_files < <(find "$repo/src" -name '*.cpp' | sort)
mapfile -t app_files < <(find "$repo/src/app" -name '*.cpp' | sort)

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

# check <label> <compiler> <defines> <files...>: prints one FAIL block per file.
check() {
    local label=$1 cxx=$2 defs=$3
    shift 3
    echo "== $label: $("$cxx" --version | head -n 1), $# files"
    export cxx flags inc defs out
    printf '%s\0' "$@" | xargs -0 -P "$jobs" -I{} bash -c '
        log="$out/$(printf "%s" "{}" | md5sum | cut -c1-12).log"
        if ! $cxx $flags $inc $defs -c "{}" -o /dev/null >"$log" 2>&1; then
            { echo "FAIL {}"; grep -E "error|warning" "$log" | head -n 20; } >"$log.fail"
        fi'
    local fails
    fails=$(cat "$out"/*.fail 2>/dev/null)
    rm -f "$out"/*.log "$out"/*.fail
    if [ -n "$fails" ]; then
        printf '%s\n' "$fails"
        echo "== $label: FAILED"
        status=1
    else
        echo "== $label: clean"
    fi
}

status=0
check "GCC, viewer and GPU on" "$gxx" "$common_defs $gpu_defs" "${all_files[@]}"
if [ -n "$clangxx" ]; then
    check "Clang, viewer and GPU on" "$clangxx" "$common_defs $gpu_defs" "${all_files[@]}"
    check "Clang, viewer without GPU" "$clangxx" "$common_defs -DANNO_WITH_VIEWER" "${app_files[@]}"
else
    echo "== Clang: skipped (no clang++-18 on PATH, none in ~/clang18, CLANGXX unset)"
fi
exit $status
