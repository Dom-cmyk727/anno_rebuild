# MAC-HEAVY-1 methods and limits

The tested source is `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`, tree
`e218b8f5dae9e7e133e8f803618ba6e14d355286`. Source checks surround each
command; the canonical runner also watches the tracked source. Tracked report/evidence
files are written only after native verification workloads finish. Their later
commit identifies the handover, not a new engine candidate.

## Execution and counts

All suites use the unchanged canonical `tools/test/run_selected.py`. Stage 2's
whole-binary commands and Stage 5's existing population CLI commands need execution
bindings beyond the normal per-case CTest inventory. Private, ignored-cache CMake
registries use `CMAKE_PROJECT_INCLUDE`, with the actual repository as
`CMAKE_HOME_DIRECTORY`. They bind existing commands and the real
`picture_matrix_contract`; they compile no replacement engine or tests. Actual
executables come from the normal Release/ASAN builds. Registry commands, native
outputs, binary hashes and the binding/reader source are retained for review.
CTest jobs 16/8 supplies the packet's population-process parallelism in place of
`xargs -P 16/8`, while retaining the mandatory canonical gate.

A Stage 2 CTest entry is one whole-process operation. Its native framework cases,
failed cases, prerequisite returns and filter exclusions are separately recorded.
Framework passes that returned for a prerequisite are subtracted from executed
passes. Doctest's filtered-out cases are not prerequisite skips. Full ordered
case-name lists and the prefix before every failure are preserved. A changed
order and changed outcome are observations; timing or shared-state causes are
not asserted without proof. The render shared-reclaimer assertion is labelled
known, as the packet requires.

Stage 3's three full runs are explicitly prescribed measurements, not retries
of a failed full gate. Original failures remain failures. Memory refusals
receive isolated reruns; the recorded Stage 1 minimap isolation is a targeted
failure investigation, not a full-gate retry. All native test/build workloads run in stage order;
light source reading and preparation of unused cache scripts overlap waiting.
Each stage's recorded execution box starts at its first stage command.

## Environment and native command scope

Every command records its actual explicit `ANNO*`, `ASAN_OPTIONS` and
`UBSAN_OPTIONS` values. Baseline: real `ANNO1503_GAME_DIR`,
`ANNO_TEST_THREADS=4`. ASAN adds `ANNO_TEST_MB_PER_THREAD=2048`,
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`,
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`. A safe host capability probe
established that LeakSanitizer is unsupported; no engine leak claim is made.
Third-party SDL/WGPU code is not claimed fully instrumented.

Stage 4 runs one named opt-in switch per case. Its preflight documents which
assertions actually run, versus numbers merely printed. Actor-free/component
imports in existing opt-ins remain component evidence. `/usr/bin/time -l`
wraps each selected gate; its maximum-resident-set-size field is retained as
reported, not represented as aggregate concurrent memory. CTest's terminal case
duration and gate wall duration are distinguished.

The population CLI uses its existing normal context loader; no projection,
script suppression, repair, injected stock or additional rules are introduced.
Its native load-time `ThreadExecutor` uses up to hardware concurrency, not
`ANNO_TEST_THREADS` or the test-support coordinator. Requested CTest process
parallelism remains explicit. Exit 1 at load is separated from an invariant
failure. Exit 0 proves the CLI's printed invariant checkpoints, not an additional
world-fault assertion or complete state-hash equality. Release/ASAN comparisons
use exact native stdout, stderr and exit, with no normalization. Release exports
add relative scene headers around the unchanged captured streams.

The game files are treated read-only throughout. The hash manifest recorded at
the Stage 5 boundary is explicitly a later observation, not a retroactive Stage
0 snapshot. No original file contents, pixels or script listings are published.

## Logging qualifications

Stage 1's initial in-memory header filter omitted `ANNO1503_GAME_DIR` from its
preamble, while the child environment and canonical provenance had the correct
path. Original bytes remain intact; complete environment-prefixed views and the
correction record accompany them. No tests were restarted for this correction.

A private Stage 2 metadata-only correction moved native duration capture before
reader postprocessing. Both helper versions and their hashes are retained. No
native arguments, predicates, ordering or executable changed. Canonical CTest
terminal durations are used uniformly for the published matrix.

The first serial Release GPU run executed 51/10/26 assertions in the three
formerly failing timing cases on Apple M5 Max (Metal). The river cost-source
MESSAGE prints a pointer address rather than its intended readable label
(`gpu_test.cpp:2409`). That label is not used to infer GPU-versus-fallback timing;
this is a diagnostic limitation, not a failed assertion or sanitizer report.

The atlas byte hash uses the selected decimal data strings, in original row
order, tab-separated with LF after every row; no header, comment, scene path or
floating fill ratio is included. Scene byte columns precede the figure-catalog
byte columns. Refused scene rows have no invented byte values and are listed
separately. The atlas summary names columns and row counts. Windows comparison
belongs to the orchestrator and is NOT_RUN here.

The population CLI prints an `island templates` prelude before a refusal. The
initial wrapper's advisory `load_refused` flag checked only the start of stdout
and therefore missed these refusals. The final reader classifies exact refusal
prefixes line by line and retains `wrapper_load_refused_initial`; all native
stdout/stderr/result files and canonical failures remain unchanged. Both builds
used the same original wrapper, and no command was rerun for this correction.

The leather scale requests did not all become buildings: K=8 built 8 farms/3
tanneries; K=16 and K=32 each built 12 farms/0 tanneries. The measured workloads
and ledger assertions remain valid for those actual sizes. They do not establish
32-farm/32-tannery scale coverage. The play timing unit is a nominal 100 ms turn:
10 substeps of 10 ms, verified through PlayRunner::one_turn and Timing. Its setup
checks do not assert that every requested turn completed without simulation_error.

Whitespace verification preserves generated data byte-for-byte. The atlas TSV's
21st `error` cell is empty for successful rows, so their final delimiter is a
structural tab. Population exports retain the final empty stderr block. Plain
`git diff --check` flags these three data artifacts; it passes on all other
changed files after prose EOF cleanup. A data-only check disables only
blank-at-eol/blank-at-eof, and SHA256 equality with the original cache artifacts
proves that no delimiter or native output was stripped for style.
