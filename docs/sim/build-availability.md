# MAP-BOOTSTRAP-1: construction eligibility candidate

Source: `a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0`; branch
`task/MAP-BOOTSTRAP-1`; isolated checkout
`/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-BOOTSTRAP-1`.
Local candidate only: no publication, merge, rebase or default House flip.
Current dispatch read from fetched `origin/main` (`20bd97fd`). The explicit
owner packet releases this work independently of its historical queued wording.

## Acceptance table (before implementation)

| Requirement | Legal control | Distinguishing negative | Primary source | Test/evidence | Review |
|---|---|---|---|---|---|
| B0 present behavior | Original Inn 1335 with player mask 5 builds | Same build with mask 1 currently ALSO builds | Anlagen INFRAMIN 1B; apply.cpp check_record | bootstrap baseline test/raw B0 gate | pending |
| B0 imports | Later-stage original Inn loads with mask 1 | Do not use command gate for scenario import | scenario.cpp create_building caller | same baseline | pending |
| B1 bit semantics | mask 8 meets stage 3 | mask 16 does not; ordinal >= is wrong | AnnoWelt 1001bb49..1001bb70 | independent byte review, fixtures | pending |
| B1 exceptions | Original EVER reads bit 0 | mask 0 must not mean default 1 | same original routine | pure-query tests | pending |
| B1 research/overrides | Trace original branches before wiring | Stage-only model must not claim full fidelity | AnnoWelt 1001bae0..1001bc70 | B1 note and reviewer | pending |
| B2 validated content | New id using existing requirements | Unknown/repeated/malformed must not become EVER | AnnoGame type parser + reviewed adaptation | parser/query/digest tests | pending |
| B3 authoritative apply | Eligible effective type builds | stale query, wrong owner, locked effective type | shared check_record atomic/live | nonmutation and mutation controls | pending |
| B4 real budget | Unchanged cargo can pay a supported prefix | Cannot treat inert/unsupported chains as supply | original HEAD/loadout and public commands | ledger and prefix | pending |
| B5 compatibility | Reload/replay joins unlock/build/debit | changed registry rejects identity mismatch | header/registry/callers | focused + broader canonical gate | pending |

## Boundary and caller inventory

Normal builds use `Commands::apply -> build -> check_record -> place` in
`src/sim/src/apply.cpp`. Atomic validation also calls `check_record` using a
virtual Budget. Rewrites loop through placement again before the existing
optional research check. Both paths must use the authoritative owner and facts.

Scenario import uses `scenario.cpp::build_island -> create_building` with no
payment. Founding calls `create_building` in `founding.cpp` under its ship/city
contract. Automatic house transitions replace the existing record/payload and
tiles in `house_transition.cpp`; they are not player construction. The common
creation primitive must not acquire a player availability check.

Original UI availability, original apply-time behavior, and stronger authoritative
rebuild enforcement are separate claims. Production wiring waits for B1 review;
materially unresolved policy blocks only its dependent work.

## Host/workflow

2026-11-05 Mac arm64: 18 logical CPUs, 51,539,607,552 bytes physical RAM,
1.5 TiB disk available. Snapshot: ~976135 free 16-KiB pages; other review
compilation was observed and left alone. Existing recovery calibration:
main `cache/mac-recovery-2026-10-30/logs/calib-compile.log` cold j6=27.0s,
j12=17.0s, j16=14.0s. Candidate uses j4 while shared host is active; test
admission remains enabled. Tools from main `cache/mac-tools/venv/bin` for
CMake/Ninja/clang-format; system Python has pefile 2024.8.26, capstone 5.0.7.
Canonical gate is main `tools/test/run_selected.py` with explicit source/build.

The app create_worktree workflow rejected both nested repository sourceRoot and
non-repository workspace root; Git worktree add created this isolated checkout
at the exact accepted pin. No other worktree was changed.

## B0 checkpoint

Commit15b4ef02; independent reviewer /root/review_original: APPROVED B0.
Canonical baseline-03: selected1/executed1/failed0/empty0/skipped0/incomplete0.
Baseline-01: build failed (test author used wrong stock shape and unparenthesized
doctest message). Baseline-02: test failed at scenario stock size143 vs required56.
Both preserved under cache/bootstrap/B0; corrected03 supersedes neither historical
failure. No enforcement changed. Tests include original Inn1335 with masks1 and5
and scenario import while locked. The baseline test remains a regression of the
unchanged command until an authorized production scope is established.
