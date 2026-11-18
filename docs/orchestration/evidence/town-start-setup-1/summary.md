# TOWN-START-SETUP-1 — READY FOR REVIEW

Base: b5c0ada209d94a0082bc88d793c917c7222e6a21. Native-test code head: e71f2dc0294d1c578e044bc0310972d83144af9b. The final report commit changes docs only; the tested code, tests and fixtures remain identical.

Both public ship-start entry points accept a final optional const sim::ExtensionContext& = {}. The installer forwards to the loader and the loader forwards to World::create, which retains sole validation and creation responsibility. No existing world is patched. Existing argument positions, defaults, production caller choices, schema/tag/version, PRNG timeline and manifests stay unchanged.

Four real candidate-A tests cover omitted/empty load/install save bytes and hash equality, explicit enabled=1 through both entry points, native reload for each, enabled=2 refusal on both paths, and unchanged failed-install target bytes/hash after advancing the pre-existing target. Initial direct result: 4 passed, 48 assertions. Missing original files are required-asset skips.

## Distinguishing controls

Each replacement applied exactly once. Dropping load-to-create forwarding: selected 2, default control passed, enabled/reload case failed, exit 1, 25 passed and 2 failed assertions. Dropping install-to-load forwarding: same outcome, 27 passed and 2 failed assertions. The 1,657 filter exclusions per run are not prerequisite skips. A whitespace-only private reader correction preserved the first load-mutant execution without rerunning it. Source restored byte-for-byte and rebuilt before canonical gates; ASan is a fresh build.

## Receipts

`cache/town-start-setup-1/dev-focused-01.log`; required contract separately, then main:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=30 executed=30 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/town-start-setup-1/asan-focused-01.log`; required contract separately, then main:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=30 executed=30 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/town-start-setup-1/pins-check-01.log`; required contract separately, then main:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1709 executed=1691 failed=0 empty=0 skipped=18 (required-asset=0 platform=0 opt-in=18 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

```text
PINS check: PASS; 0 pins moved; 0 fixture byte changes; scope=full; selection=^(anno_sim_tests\.|anno_sim_replay_|population_oracles_agree$|integration_stream_reference_|highmap_digest_reference_agrees$); evidence=cache/town-start-setup-1/pins-check-01.log
```

Normal check only, never regen. Full pins wall 5,184.520 s; existing Venice canary 2,152.87 s, human canary 236.88 s, closed-site canary 633.48 s, all passed. Eighteen skips are opt-in, not corpus passes. No memory refusal, bad_alloc retry, empty/incomplete outcome or sanitizer report. No gate repeated.

ASan configure 12.027 s; fresh target build 131.144 s; focused gate 39.010 s. Jobs 4/internal threads 4, memory admission enabled. ASan reservation 2,048 MiB/thread; ASAN_OPTIONS=detect_leaks=0:halt_on_error=1, UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1. LeakSanitizer unsupported on this host, not claimed.

Host: Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1, Apple Clang 21.0.0, CMake 3.31.10, Ninja 1.13.2. Game files read-only via $ANNO1503_GAME_DIR.

Warning script once, exit 0, 213.277 s: 418 source files twice and 85 app files twice, 1,006 project compiles plus two caught shadow controls. Actual Apple Clang 21 for both script labels; native GCC 13/Clang 18 NOT_RUN. Host-only adaptations: Bash 3 mapfile helper, BSD xargs -S 4096, CMake-detected -Wno-c2y-extensions. No checker/source edit for these launchers. Source compiles clean; duplicate-library linker warnings disclosed separately. clang-format 23.1.3 passes on three changed C++ files.

Focused canonical selector: ^anno_sim_tests\.(ship start|extensions |fisher extension:|section (versions|extensions):), build/dev and build/asan, asset-policy required, jobs 4, timeout 1800. Pins: python3 -I tools/pins.py check --build-dir build/dev --jobs 4 --log cache/town-start-setup-1/pins-check-01.log. Warnings: bash tools/check_unix_warnings.sh $WARNING_DEPS.

Raw argv, environment, inventories, source/executable identities and logs remain in ignored cache/town-start-setup-1/. sha256.json indexes unchanged local raw artifacts, not publicly downloadable files. No original content or absolute host paths are committed.

## Decisions and reasoning

- Keep the prerequisite separate from the town measurement per option B; preserve the unstarted TOWN branch and its private plan.
- Forward the generic existing context rather than add a fisher setter or duplicate validity policy. Empty defaults retain all existing caller choices.
- Test both edges independently, and advance the old target before invalid installation so accidental baseline replacement is detectable.
- Reuse unrelated native lanes: no scheduler, field, feature implementation or default changes. The prescribed normal full pins selection nevertheless exercises sim callers and independent oracles once.

NOT_RUN: native Windows/Linux with original files, running original/HE, unsupported LeakSanitizer, town measurement or UI activation. Hosted exact-head CI is supplied with the final handover; CI has no game files. Orchestrator review pending. QUESTIONs: none remaining for this prerequisite; TOWN resumes only after acceptance.
