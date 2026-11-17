#!/usr/bin/env bash
# The CI's GCC/Clang warning gate, run locally (TASKS.md §1 point 9; on the
# Windows PC in WSL). Compiles every .cpp under src/ with the CI's warning
# flags at -O3 and -Werror, compile only (no link), once with GCC 13 and once
# with Clang 18, with the viewer and GPU code switched on (ANNO_WITH_VIEWER,
# ANNO_VIEWER_GPU, ANNO_BENCH_GPU: every CI configuration builds them); then
# the app sources once more with GCC and Clang without ANNO_VIEWER_GPU (a viewer built
# without wgpu, -Wunused-private-field). MSVC reports most of these warnings
# not at all, so a clean MSVC build says nothing about them.
#
# UNIX-WARN-1: src/sim/tests/extensions_test.cpp needs a CMake-generated-content define
# (src/sim/CMakeLists.txt's ANNO_EXTENSION_ORACLE_FILE, pointing at a file
# tools/integration_stream_ref.py --extension-output writes). This script reads the define's
# name and the generator's own path out of that CMakeLists.txt at run time (extract_oracle_defs
# below) instead of a second, hand-written copy of either - a rename or removal there is caught
# (exit 2, a clear message) instead of silently going stale the way the define's absence here
# did for eight reports running. No other file in src/ needs a generated-content define
# (`grep -rln 'target_compile_definitions.*_tests PRIVATE [A-Z_]*="' src/*/CMakeLists.txt` finds
# only this one), so this stays a single extracted pair, not a general per-file define table;
# whoever adds a second one extends extract_oracle_defs and the per-file override mechanism
# below, not a parallel one.
#
# A negative control (negative_control below) compiles a scratch file with a shadowed variable
# under the same flags and expects it to fail: this is what makes "clean" trustworthy - a flags
# or toolchain regression that silently stopped catching warnings would make every real check
# pass for the wrong reason, and only the control would say so.
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
# Exit status: 0 clean, 1 a file failed, 2 setup error (deps missing, the oracle define/script
#              not found or failed to generate, or the negative control did not fail).
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
# test_support's private headers (host_test includes coord_wire.hpp from src/)
inc="$inc -I$repo/src/test_support/src"
inc="$inc -isystem $deps/wgpu_native-src/include -isystem $deps/sdl3-src/include"
inc="$inc -isystem $deps/doctest-src"
common_defs="-DDOCTEST_CONFIG_USE_STD_HEADERS -DANNO_FIXTURES_DIR=\"$repo/fixtures/synthetic\" -DANNO_REPO_DIR=\"$repo\""
gpu_defs="-DANNO_WITH_VIEWER -DANNO_VIEWER_GPU -DANNO_BENCH_GPU"

mapfile -t all_files < <(find "$repo/src" -name '*.cpp' | sort)
mapfile -t app_files < <(find "$repo/src/app" -name '*.cpp' | sort)

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

# Reads src/sim/CMakeLists.txt for the one CMake-generated-content define it adds to
# anno_sim_tests (its own `target_compile_definitions(anno_sim_tests PRIVATE NAME=...)`) and the
# python script its matching `add_custom_command` runs to produce the file NAME points at.
# Fails loudly (exit 2) rather than silently compiling extensions_test.cpp without it, the way
# this script did before UNIX-WARN-1.
extract_oracle_defs() {
    local cmake_file="$repo/src/sim/CMakeLists.txt"
    oracle_define=$(sed -n \
        's/^[[:space:]]*target_compile_definitions(anno_sim_tests PRIVATE \([A-Za-z_][A-Za-z0-9_]*\)=.*/\1/p' \
        "$cmake_file" | head -n1)
    oracle_script=$(sed -n \
        's#.*COMMAND \${Python3_EXECUTABLE} -I "\${PROJECT_SOURCE_DIR}/\([^"]*\)".*#\1#p' \
        "$cmake_file" | head -n1)
    if [ -z "$oracle_define" ] || [ -z "$oracle_script" ]; then
        echo "check_unix_warnings: could not find the extension-oracle define or its generator" \
             "script in $cmake_file (CMake may have renamed or removed one) - see UNIX-WARN-1" >&2
        exit 2
    fi
}

# Generates the oracle file extensions_test.cpp needs (the same script CMake's own custom
# command runs, into this script's own scratch dir rather than a build tree) and writes the one
# per-file override check() below applies only to that file.
generate_oracle_override() {
    local oracle_file="$out/synthetic-extension.txt"
    if ! python3 -I "$repo/$oracle_script" --extension-output "$oracle_file" \
            >"$out/oracle-gen.log" 2>&1; then
        echo "check_unix_warnings: generating the extension oracle failed:" >&2
        cat "$out/oracle-gen.log" >&2
        exit 2
    fi
    printf '%s\t-D%s="%s"\n' "$repo/src/sim/tests/extensions_test.cpp" "$oracle_define" \
        "$oracle_file" >>"$out/overrides.tsv"
}

# check <label> <compiler> <defines> <files...>: prints one FAIL block per file. A file listed in
# $out/overrides.tsv (generate_oracle_override above) compiles with its own extra define
# appended, every other file with just <defines>.
check() {
    local label=$1 cxx=$2 defs=$3
    shift 3
    echo "== $label: $("$cxx" --version | head -n 1), $# files"
    export cxx flags inc defs out
    printf '%s\0' "$@" | xargs -0 -P "$jobs" -I{} bash -c '
        extra=""
        if [ -f "$out/overrides.tsv" ]; then
            extra=$(awk -F"\t" -v f="{}" "\$1==f{print \$2; exit}" "$out/overrides.tsv")
        fi
        log="$out/$(printf "%s" "{}" | md5sum | cut -c1-12).log"
        if ! $cxx $flags $inc $defs $extra -c "{}" -o /dev/null >"$log" 2>&1; then
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

# UNIX-WARN-1 item 2: a scratch file with a deliberate -Wshadow violation must make `check` fail,
# proving the flags/toolchain genuinely catch a warning rather than vacuously passing (a broken
# compiler path, a flags typo that disabled -Werror, ...). Compiled stand-alone (no repo
# includes/defines needed) so this control exercises only the compiler and the shared flags.
negative_control() {
    local cxx=$1
    local src="$out/negative_control.cpp"
    cat >"$src" <<'EOF'
int shadowed(int x) {
    int y = x;
    {
        int y = x + 1; // -Wshadow: must fail this check
        y += 1;
    }
    return y;
}
EOF
    local log="$out/negative_control.log"
    if "$cxx" $flags -c "$src" -o /dev/null >"$log" 2>&1; then
        echo "== negative control ($cxx): FAILED TO FAIL - it compiled clean; the shadowed" \
             "variable above should have been rejected by -Wshadow -Werror. The flags or this" \
             "compiler's path are not catching warnings - every 'clean' result above is" \
             "untrustworthy until this is fixed." >&2
        cat "$log" >&2
        exit 2
    fi
    echo "== negative control ($cxx): failed as expected (shadowed variable caught)"
}

extract_oracle_defs
generate_oracle_override

status=0
negative_control "$gxx"
check "GCC, viewer and GPU on" "$gxx" "$common_defs $gpu_defs" "${all_files[@]}"
# A viewer built without wgpu with GCC too (W4-WIN-FINAL F2: its viewer-enabled WSL sanitizer build has
# no GPU, and GCC found an unused function there that Clang does not report).
check "GCC, viewer without GPU" "$gxx" "$common_defs -DANNO_WITH_VIEWER" "${app_files[@]}"
if [ -n "$clangxx" ]; then
    negative_control "$clangxx"
    check "Clang, viewer and GPU on" "$clangxx" "$common_defs $gpu_defs" "${all_files[@]}"
    check "Clang, viewer without GPU" "$clangxx" "$common_defs -DANNO_WITH_VIEWER" "${app_files[@]}"
else
    echo "== Clang: skipped (no clang++-18 on PATH, none in ~/clang18, CLANGXX unset)"
fi
exit $status
