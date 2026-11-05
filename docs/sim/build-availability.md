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
| B0 present behavior | Original Inn 1335 with player mask 5 builds | Same build with mask 1 currently ALSO builds | Anlagen INFRAMIN 1B; apply.cpp check_record | bootstrap baseline test/raw B0 gate | APPROVED / review_original |
| B0 imports | Later-stage original Inn loads with mask 1 | Do not use command gate for scenario import | scenario.cpp create_building caller | same baseline | APPROVED / review_original + final 50-file creation check |
| B1 bit semantics | mask 8 meets stage 3 | mask 16 does not; ordinal >= is wrong | AnnoWelt 1001bb49..1001bb70 | independent byte review, fixtures | APPROVED / review_original + review_model |
| B1 exceptions | Original EVER reads bit 0 | mask 0 must not mean default 1 | same original routine | pure-query tests | APPROVED / review_original + review_model |
| B1 research/overrides | Trace original branches before wiring | Stage-only model must not claim full fidelity | AnnoWelt 1001bae0..1001bc70 | B1 note and reviewer | APPROVED original contract; B3 policy block remains |
| B2 validated content | New id using existing requirements | Unknown/repeated/malformed must not become EVER | AnnoGame type parser + reviewed adaptation | parser/query/digest tests | APPROVED / review_model + focused correction |
| B3 authoritative apply | Eligible effective type builds | stale query, wrong owner, locked effective type | shared check_record atomic/live | nonmutation and mutation controls | BLOCKED by owner; authoritative controls NOT_IMPLEMENTED |
| B4 real budget | Unchanged cargo can pay a supported prefix | Cannot treat inert/unsupported chains as supply | original HEAD/loadout and public commands | ledger and prefix | APPROVED / review_budget |
| B5 compatibility | Reload/replay joins unlock/build/debit | changed registry rejects identity mismatch | header/registry/callers | focused + broader canonical gate | APPROVED package proofs; final integration review recorded in handoff |

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

## B2 runtime facts and pure query

`BuildingType::construction` stores stage u32 and a vector of research OR ids.
INFRAMIN uses existing stage_index_of, defaults0; unknown, empty, nonterminated,
trailing nonzero and repeated records fail with entry/name diagnostics. Original
parser accepted unknown/repeated differently; strict validation is an adaptation.
Stage values must fit the existing32-bit player mask; research ids must fit the
world research table, revalidated against each world's config. Runtime type/goods
ids remain widened; the original parser's32-stage scan is not an id-count cap.

Stage31 research candidates are derived once from a content-adapter profile:
if output0 belongs to declared research_outputs, include its goods Forsch and
(nonzero) output1's Forsch; then type FORSCHTYPE, including id0. Extra declared
research ORs are independent of stage. The original loader declares original
output ids46..55 and type1021 alternatives35/36. The generic sim query contains
no original building-id branch. Alternate content can declare other output ids
and other type ids through the same profile. Generic TypeRegistry::build callers
that need these profile-specific exceptions must explicitly supply that profile;
the production original_registry adapter does so.

`build_eligibility(types, players, owner, type, permission_context)` is pure and
allocation-free. Typed outcomes: Allowed, UnknownPlayer, UnknownType,
InvalidPermissions, ScenarioDenied, StageLocked, ResearchRequired. Context owns
an ascending unique type/byte-value span and names its player; wrong owner,
unknown type, duplicate rows or values above255 are errors. An explicit override
precedes mask and research; its raw truthy byte is retained. A default/empty override
context is accepted as an explicitly empty set; the API does not detect missing
script context or prove that a scenario had no restrictions.
A successful query says nothing about terrain, payment or command authority.

No production BuildCmd integration is present: owner explicitly requested the
scenario-permission contract first. No new menu, state permission table or script
interpreter is introduced. Founding, imports and automatic transitions are
untouched. Atomic/live availability enforcement and stale-preview refusal are
BLOCKED/NOT_IMPLEMENTED, never counted as proved by this query.

## Identity

No persisted field or PRNG draw changed: stream128 and timeline5 stay published
and unchanged. Behavior-relevant immutable facts extend registry identity, after
the existing stage table: only if any nondefault facts exist, LE u32 0x31415642
(bytes BVA1),u64 nondefault type count; ascending type rows u32 id,u32 stage,u64
research count,then u32 research ids. Default stage0/no-alternatives adds no bytes,
so existing synthetic golden/replay identities remain valid. Original registries
change identity; old saves/recordings remain preserved at their old pins and are
intentionally rejected when loaded with a changed registry. No unrelated golden
is regenerated. The independent exact extension-byte test uses literal bytes
for id90001/stage3 and extends the preexisting FNV value by hand.

Focused B2 model-02 canonical gate: selected5,executed5,failed0,empty0,skipped0,
incomplete0. model-01 build failure is retained (test Chunk payload incorrectly
assumed owning instead of a span). Investigation model-build-01/02 failures
retained; model-build-03 builds. Independent B2 review and its focused corrections are approved; see below.

## Reviewed correction and integration checkpoint

Read-only reviewer `/root/review_model` approved B2 and its focused corrections.
It independently checked original instruction/data bytes and the nonzero research
fallback, original adapter declarations, nested field-count canary and all five
mutation logs. Legal controls passed; mutants reached named assertion failures
without crashes or timeouts. Recipe and logs: `cache/bootstrap/mutations/`.
The exact-byte extension test remains independent of the encoder under test.

`focused-03` executes 9/9 with no failed, empty, skipped or incomplete entries,
including the original 921-entry registry check and a synthetic integration:
existing population pass earns a stage bit, query changes from locked to allowed,
a public build debits exactly 300 gold/3t tools/4t wood, then reload/replay match.
A different registry requirement rejects the saved state. House updates are
explicitly enabled only in this fixture. This is query/command composition,
not proof of authoritative enforcement or a real ship-start population unlock.

The original direct-build registry digest intentionally changes from
`855fce5ed63c1090` to `b62eba128fce9259`: independent original-data decoding gives
241 nondefault rows and a 3948-byte BVA1 extension. Default synthetic identities
are preserved. Only that affected original digest assertion changes; historical
recordings and unrelated goldens remain untouched.

The fresh prefix case passed at `prefix-02`, but its enclosing gate failed on
the first synthetic integration setup (`Unsupported` placement flags). That
first failure is preserved. The corrected fixture gives `focused-03` above;
the successful prefix is retained without repeating sailing for this test-only
correction. Production code, manifest, original files and prefix test are identical.

## Compatibility correction after the broader gate

The first broad Debug attempt at0ceea121 exposed six accepted K3 checkpoint
hashes that include the old registry identity. Its three parallel corpus cases
also could not reach a local admission coordinator because sandbox sockets
returned EPERM. After automatic approval review rejected interruption under the
workspace running-job rule, the owner explicitly authorized stopping only those
three blocked tests and rerunning with host permission. Their terminated outcomes
remain INCOMPLETE, not passes. Raw debug-final-01:1289selected,1283executed passes,
1failed,2opt-in skips,3incomplete; final verdict FAIL/status9.

Accepted checkpoint constants and original scenario fixture rows are preserved.
The test-only construction_identity helper copies every registry fact except
construction, then requires legacy full-registry digest1bd52ba547c3878a, read
independently at byte28 of retained accepted K3 log SHA256
56e389b172d45a7f08df85e82b3f7ac735e7204b9c692d929f7b4f1921dd8765.
Only a World copy's registry reference is changed for historical hash comparison;
it is never advanced, built into or replayed. Current recorder/replay/save paths
keep current identity. A one-gold state change still changes the projected hash;
a non-construction type-cost change gives a different legacy digest. The cheap
50-file creation case checks all original creation pins under that projection
and373imported player masks, without running another long simulation for costs.
Focused compatibility-01 passes2/2 with no failures/skips/empty/incomplete and
visible coordinator admissions. Production code remains unchanged.

## Final frozen-source gates

At clean `4bafd5d00e7f2c0f199f31339350f12d0e7fe191`:
Debug `debug-final-02.log` PASS: 1290 selected, 1288 executed passes, 0 failed,
0 empty, 2 opt-in skips (navigation scale and transport statistics), 0 incomplete.
It includes all 50 scenario five-minute runs, top-ten extended runs, original
player masks, and the new creation-identity comparison. ASan/UBSan correction
`asan-final-02.log` PASS: 12 selected/executed, no failures/skips/empty/incomplete
or sanitizer lines. The earlier ASan `asan-final-01.log` at0ceea121 passes
108/108 and is retained for its unaffected production/model coverage. Both final
gates require budget, verdict and House-reference contracts separately from
selection counts. Workflow contracts pass 21 methods; canonical verdict
preflight passes 24. No tracked source changed during either gate.

The first Debug attempt remains FAIL with 3 incomplete user-authorized terminated
workers. Host-permitted final runs retain admission control and show coordinator
admissions. No shared coordinator, infrastructure or another owner's job was
modified. Final handoff and review dispositions are in
`../orchestration/reviews/2026-11-05-map-bootstrap1.md`.
