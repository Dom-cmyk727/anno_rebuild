# Founding accounts — state 134 prerequisite acceptance (2026-11-06)

This is a narrow prerequisite for both Mac lanes, not completed MAP-EXPAND-1 or
salt acceptance. Portable executor commit `7ba077506e7ab9c11bef3894739b4c1d0d4f66fb`
is based on `b408f311`, with no unfinished expansion helper or documentation
requirement. Runtime/identity/oracle/fixtures match reviewed `d97ad449`; its
focused original-data test is self-contained against the portable parent.

The orchestrator integrated that commit and the concurrent Windows status/push/
mirror instructions (`426bf964`), preserving both histories. Integration reviewer
`/root/review_consolidation_plan` found 11 replay CLI registrations still using
old133 logs. Commit `f406374d` fixes only their paths, preserving expected exit
codes, checkpoint counts and divergence assertions. This closes the P2; it is
part of the prerequisite, not optional documentation.

## Behavior and compatibility

Founding now retains `local.accounts` beside the already-adopted building index.
The original warehouse contributes 15; later forester construction produces 27,
and its inactive transition 19. Payment, random draws, field order, flags and
rule defaults are unchanged. This corrects an engine defect; no compensating gold
or historical account reconstruction is introduced.

Current identity: state134 / ECONBK1 / PRNG5 / replay4 / command1 / native wrapper1.
All old133 states and replays are refused explicitly, including booking-off states.
No migration is provided. Old fixtures and actual buggy enabled/off snapshots are
preserved, not relabelled. Default-off gameplay remains equivalent; byte/hash
identity necessarily changes. All 29 current/archive manifest hashes were checked.

## Evidence and scope

Executor evidence retained by dependency:
- `d97ad449`: canonical Debug36/36, six separately required contracts pass.
- `7ba07750`: portable Debug4/4, six required contracts pass, both actual old133
  off/on controls exercised; current native/replay artifacts match d97 exactly.
- Upstream reviewer `/root/expand_e2_review` approved the portable correction.

Fresh orchestrator integration gates at clean `f406374d`: Debug15/15 (42.07s); ASan/UBSan15/15 (136.11s), both with zero failures, skips, empty/incomplete cases or sanitizer reports.
Both use target `economy_checks`, required original assets, explicit source/build
paths, jobs2, timeout900, and the actual old133 archive directory. The main selected
set comprises the two focused C++ cases, two reference checks and 11 fresh-process
replay CLI cases. Six required contracts execute separately; counts are not added.
No broad game-corpus, Windows/Linux target, GPU/window or full voyage acceptance
is implied. Builds used normal CMake with unchanged warnings-as-errors policy.

Controls distinguish enabled/off founding, post-founding native continuation
before first booking, no duplicate same-tick charge, independent cities, actual
old133 rejection, and three independent identity mismatch layers (outer replay,
embedded native, inner state header). Every refusal has a valid current control
and its exact reason. Changed replay commands reach semantic divergence rather
than being rejected merely for version. Historical-control messages occur twice
in each new gate, proving both optional asset arms ran.

Raw evidence remains Mac-local in `cache/founding134-orchestrator/`; compact
receipt/hashes are committed under `docs/orchestration/evidence/founding134/`.
Old source/runtime receipts remain attributed to their original pins. Integration
ASan covers this prerequisite and CLI consumers; it cannot substitute for the
still-required combined two-island expansion sanitizer voyage.

## Consumer instructions

First Mac: at a clean owned checkpoint, consume the ordered commits **7ba07750,
f406374d** (or a named accepted main checkpoint containing both), retaining all
owned NEEDS corrections. No wholesale expansion-branch merge is needed. State135
remains reserved for the reviewed narrow salt slice after this dependency; X1
contract approval and the remaining original-evidence requirements still apply.
Do not silently amend134 or infer salt implementation approval from this fix.

Second Mac: keep its existing equivalent runtime/identity work, adopt f406374d at
a clean checkpoint, and continue E2–E7. Do not cherry-pick 7ba over an already
identical implementation merely to acquire its ancestry. Source/test differences
must remain explicit. Full expansion acceptance stays open.

Windows remains on its assigned frozen package until its normal integration
checkpoint. Own task branches now push normally at checkpoints per426bf964;
no force/main pushes by executors. GitLab mirror sync remains assigned to the
Windows orchestrator. This Mac's read-only mirror probe failed host-key
verification; no host-key setting was changed and no GitLab push is claimed.

The four exported original-data native/replay artifacts are byte-identical in the
new Debug and ASan lanes and to the upstream portable artifacts. They remain
ignored local evidence; only their hashes are committed.

Final independent integration verdict: `/root/review_consolidation_plan` approved
bounded acceptance at clean `f406374d` after checking both terminal gates and
actual historical controls. No blocking finding remains in this prerequisite.
