# Atomic road correction and forester E1-FINAL — direct review, 2026-11-03

**Road correction and reference-runner robustness: ACCEPTED. Forestry evidence
and implementation scope: reviewed; contract/reference correction E2.0 is still
required before engine work.** The orchestrator authorizes the finite correction
below, then E2–E6 without another owner handoff after its narrow independent
approval. This is not unconditional acceptance of the current forestry contract.
The current assignment is in [TASKS.md](../TASKS.md).

## Pins and closed findings

| Input | Exact revision and disposition |
|---|---|
| Atomic latest-object code and tests | `d9a056823ad1aedfb7c41bf8d07b4dc6c965e39f` — accepted |
| Clean focused gate, including the hardened reference/contract | `d0e4ae601da7281388d2e26d10fb9ae54ee6751a` — accepted within the selections below |
| Reviewed candidate, final forestry text and tree-table tool | `362f99ab65e5b4bac8a45f64f778bab0ec299365` — road/runner fixes accepted; forestry text/reference subject to E2.0 below |
| Earlier independent road policy/state choices | `eae9f4aeff6030c1886970fe3271160c538f15af`, comment/doc increment `aad1735bc00612695eb49ce356a4e0a42eb17d5d` — retained, stream 125 |
| Gate workflow | clean main `050ceb8223c0f151e90ea6cb1e604d1d5ca0d668` |

The root reviewer directly inspected the full correction diff and the forestry
contract/reference changes, surrounding default/custom placement callers,
scheduler, tile writers and transport delivery. No source review was delegated.
The candidate worktree was not modified. The earlier
[review](2026-11-03-roadclose-forestry.md) remains historical evidence; the
placement-mask interpretation repeated there is superseded below.

Default placement now sees one coherent latest-object view: the atomic overlay
overrides physical tile objects, while building occupancy and refusal order stay
separate. The separate virtual-object precheck is limited to an installed test
placement seam. The original reviewer counterexample was recompiled against the
fixed production libraries: both live records and both atomic records succeed,
and **10/10 assertions pass**. Before the fix, nine controls passed and the
atomic-success assertion failed. The committed directed test covers all four
road-policy combinations and both record orders; the property varies object
flags and mode instead of merely repeating one fixed set of flags.

The public reference now refuses empty/type-only input, terminates at full
output, checks computed answers and uses bounded owned subprocesses. Root ran
the final **61 vectors** and **13 public-command contract tests**, all passing.
These establish runner behavior and agreement with the written reference,
not original-game validity of every modeled predicate.

## Remaining contract findings and binding correction

### P2 — preserve the original claim bit on faithful placement and clearing

`docs/re/forestry.md` U-4, `docs/sim/forestry.md` §3.5 and
`tools/forester_ref.py::claims` say placement clears bit 29. The cited mask
actually preserves it. Root read the original instructions directly:

- AnnoWelt `0x10082442`: `ebp = 0x3fffffff`; `0x100824a7`: `eax &= ebp`,
  then the second tile dword is written at `0x100824ac`.
- The other write to this dword in the inspected placement loop changes its
  low byte (`0x1008248a`), not bit 29. Thus this loop clears **bits 30 and 31**,
  while preserving bit 29.
- The claim setter at `0x100878da` shifts by `0x1d` (29) and at `0x100878dd`
  clears with `0xdfffffff` before installing that one bit.
- `(0x3fffffff & (1 << 29)) == 0x20000000`. The documented clear arm also
  preserves the claim; keep placement and clearing as separate transitions.

Original module SHA-256:
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
This matches the research identity. This is static evidence, not an original
runtime capture. The earlier review also repeated the wrong mask interpretation.

**Binding behavior:** faithful placement/clearing preserves the claim bit.
Worker cut/abort still clears it as traced. Enhanced invalidation may clear its
bit/owner entry as an explicit adaptation under the existing false-default
hygiene choice. Do not implement that enhanced behavior unconditionally or
describe placement as a proven faithful way for a second worker to take a claim.
Keep valid faithful orphan/missing-claim states where otherwise permitted;
do not invent automatic loader repairs or claim that all such states are
reachable through this placement loop.

Correct the RE and simulation notes, related transport mask text, pure reference
and vectors together. `claims faithful A+ P` must leave bit 29 set; the current
longer `A+ P B+ A-` vector ends at zero under both interpretations and does not
distinguish them. Add the short discriminating control, faithful clearing and
normal-unclaim controls, plus enhanced-owner controls. A temporary reference
change restoring the wrong placement clear must fail the new intended assertion
while the legal unclaim control passes. Preserve the bad prior interpretation as
withdrawn evidence; do not require the old wrong vector semantics to stay fixed.

### P2 — retain the first five-tick delay across short visits

`docs/sim/forestry.md` §3.2 discards a sub-five-tick first visit and then charges
five ticks again on the next visit. `World::advance(2)` can advance one tick;
the scheduler's clamped phase-B visit count can visit a lone worker every such
advance. This is a supported path, not a hypothetical future extension. The
proposed rule could keep that worker in its initial delay indefinitely.

**Binding behavior:** persist an explicit first-delay remainder, initially five
ticks, consume it exactly once, and carry unused elapsed ticks into the ordinary
body. Re-entering `Start` after a cut does not reapply the initial delay. Specify
it in E2's state/validation/independent stream transcription. Add small hand
vectors now for five one-tick visits versus one five-tick visit, zero time,
completion with surplus time, and save/reload after two ticks. E3 must exercise
these through the actual scheduler. This is the accepted integer model of the
traced initial micro-step; no claim of original floating-point runtime identity
is added.

### Implementation clarifications to fold into E2.0

- The Farm-home requirement describes spawn origin. An active worker whose
  home was demolished must remain loadable until its defined abort transition;
  a Done/expiring figure has no home. Do not combine those allowed states with
  an invariant requiring a currently existing Farm home for every harvest state.
- Cutting changes object and phase, not the stored walk-code/mask columns.
  Separate immutable type-derived walk facts from the actual tile record.
  Do not silently recompute walkability from the predecessor type. Define the
  overlay against the fields actually changed; retain the finer-route limitation.
- Use disjoint material quantities. A source reservation is part of producer
  output until loading; it is not another physical store. Cart load/sent totals
  are not warehouse receipts. Existing ended figures can retain historical
  cargo fields; distinguish non-spendable history from active cargo instead of
  counting delivery twice or silently changing existing transport semantics.
  In the city equation, ship loading subtracts from stock and unloading adds.
  Assert unexplained terms are zero at each checked boundary.
- Fix the stale-read example's units: it finishes at elapsed tick 22,500;
  **17,500 is unused time** from a 40,000-tick visit. The current reference's
  return value is correct; one prose sentence calls it delivery time.

These are precise contract corrections, not permission for another broad RE,
test-runner, coordinator or pathfinder campaign. E2.0 changes docs/reference/
vectors and their small contract checks only. One independent read-only reviewer
checks these corrections and their discriminating controls. When corrected and
approved, record the exact correction commit in the report and continue E2–E6
autonomously. A different necessary semantic decision or failed original-byte
check stops that transition for the orchestrator; merely completing E2.0 does
not require another owner permission turn.

## Verification and honest limits

Saved C2-1 log inventories and verdicts reconcile:

| Lane | Selected | Executed passes | Failed / empty / incomplete | Skipped |
|---|---:|---:|---|---:|
| References, with mandatory public contracts | 47 | 47 | 0 / 0 / 0 | 0 |
| Focused Debug | 121 | 121 | 0 / 0 / 0 | 0 |
| Same focused ASan/UBSan selection | 121 | 121 | 0 / 0 / 0 | 0 |

Recognized sanitizer lines: zero. Original game directory was set, required
assets were enabled, and source/workflow identities were clean before and after.
The gate ran at **d0e4ae60**, not the later report commit. The latter also adds
a read-only tool, so it is not strictly docs-only. Root inspected and ran that
new tree-table tool separately against the original Anlagen.dat: **46 BAUM
entries, 45 cuttable, 44 regrowing, 259 a stump, 261 uncuttable**. That is a
tool/data check, not a later C++ gate.

The retained 103/103 opening replay is scoped to the earlier stream-125 C1-1
source. Its non-atomic commands have no object overlay; no new format or golden
change was made here. Reusing it is justified; it does not test harvesting.
Root did not repeat a full sailing or compiled gate suite. Exploratory probe,
reference outputs, original-byte excerpts and prior failure evidence are under
ignored `cache/review-road-close-forestry/` in the main checkout.

Windows/MSVC, Linux/WSL warnings and sanitizers, full-ASan opening,
original-runtime observations, GPU/performance and multiplayer scale remain
NOT_RUN for this correction. Inherited finer sub-cell route validation and the
explicit unread lifecycle callers remain limitations. No harvesting engine
exists in the reviewed candidate.

Windows keeps exact `0c401c8d084cfd9feb8aec846595291f87f22cce` and its existing
W4-OPENING-1 assignment. Publishing this Mac candidate does not select it as a
new Windows integration input or merge gameplay into main.
