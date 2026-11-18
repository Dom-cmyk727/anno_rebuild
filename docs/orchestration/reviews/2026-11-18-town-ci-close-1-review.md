# TOWN-CI-CLOSE-1 — source and local evidence approved; CI pending

Mac orchestrator, 2026-11-18. Independent read-only source/evidence reviewer:
`/root/review_town_ci_close`. **Not accepted for landing until the exact-head hosted gate passes.**

Branch `task/TOWN-CI-CLOSE-1` starts at town report
`156badab35516609e92796e85a4fb0b9314ddac6`. Current source is
`d1e5c13a2a81568f0df5ebeb4ae86697c24b6568`; CI
[44262416790](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44262416790).
The local report checkpoint `3514802f` preserves earlier receipts and conditional review.
It is documentation, not a substitute for a green source build.

## Reviewed changes

- `8e502b32`: explicit owning freight-vector copy; exact doctest header-selection fix immediately
  after 2.4.12. The reviewer compared all 270 dependency files: only the two header-selection
  files differ. No assertion, discovery or reporting change. Actual build headers were hashed.
- `739bb9e5`: return before indexing an empty publication-statistics vector. Paired writes,
  join-before-read and nonempty output were checked. Real threaded-runner tests still emit two
  matching positive-count rows with ordered quantiles in Debug and ASan.
- `48f88a19`: existing zoom test reads the immutable static original table by const reference.
  Lifetime and all assertions are unchanged; no production zoom change.
- `283e8f9b`: genuine Mine native-reader UB corrected. Decoding the incoming presence byte
  previously caused an empty destination optional to be dereferenced. Read archives now decode
  into a separate numeric temporary before engaging the optional. Write/hash bytes and failure
  reset behavior are unchanged. Existing independently authored byte vectors remain authoritative.
- `d0278ef6`: existing minimap test uses a checked lookup for its third fixture colour slot.
  The three-slot setup and all owner-colour assertions are unchanged; production minimap untouched.
- `d1e5c13a`: only GCC Release adds Ninja `-k 0`, retaining failures while collecting diagnostics
  from independent targets. `pipefail`, warning settings, test eligibility and coverage remain.
  Full build.log joins the existing always-uploaded artifact. Failed prerequisites can still hide
  dependent diagnostics; this is not a promise that every possible error appears in one run.

## Evidence and failures retained

GCC 15.2 x86 controls reproduce the original statistics, zoom-test and minimap-test failures;
corrected compilations pass. GCC 15.2/15.3 freight controls and Clang 21/libstdc++ 15 doctest
controls likewise fail before and pass after. No warning suppression was introduced.

The full original town GCC Debug artifact shows 27 aborted tests and one failed G3 replay,
all arising from the empty optional. Its ZIP matches GitHub's API SHA256
`958ac60b85d12fd5c42f853c0670132ebe25d2be879de9c8a35101a1b59bbf36`.
Exit 9 was an incomplete-crash-evidence verdict, not source mutation. On the Mac, the existing
layout test reproduces the same assertion with libc++ DEBUG hardening (485 assertions pass,
then exit 134). The same hardening remains enabled for the corrected 28/28 recovery gate.

At clean `283e8f9b`, canonical Debug and ASan/UBSan each execute 56/56: all 28 prior failures,
26 freight cases, the threaded runner and the affected zoom case. Each also passes the required
picture contract 1/1 and mandatory preflights. No failures, empty, skipped or incomplete cases,
or sanitizer lines. The independent reviewer re-parsed the logs and reconciled all inventories.
Python stream goldens, lint and formatting pass; no fixtures or identity moved.

The subsequent minimap-only gate is separate from those receipts. Its reviewed change cannot
affect the simulation gates. A tiny real CMake/Ninja control executes two deliberately failing
independent targets and a successful target, then exits 1 with `-k 0`.

Earlier missing-cmake, missing bench_picture and invalid selector attempts remain failed/setup
invalid. CI runs 44258255346, 44259414215 and 44261293763 exposed successive compiler failures;
they are not waived or overwritten by a later documentation run. Raw logs remain private under
cache/town-ci-close-1/; REPORTS records hashes and commands without host paths or original bytes.

## Town reuse and landing boundary

The reviewer traced the actual town measurement: original-format parsing builds a fresh
ScenarioSpec and World, followed by public commands, advancement and state hashing. It performs
no native save/load or replay and reaches neither freight assignment nor PlayRunner statistics.
The Mine reader fix is therefore unreachable there; its writer/hash branch is byte-identical.
The reviewed town traces and food control remain valid without repeating 900,000 ticks.
They establish no native-reload guarantee, sustainable economy or Settler upgrade.

Wildlife continued in its separate branch. No wildlife source, running job, owner keep-awake
assertion or coordinator was changed. No main push, merge, force or automatic other-lane intake.
The Windows orchestrator retains stacking and landing authority. Once CI is green, accept town
on this repaired stack; do not describe the preserved original red town ref as green.
