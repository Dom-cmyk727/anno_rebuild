# Road correction and forester E1 — direct review, 2026-11-03

CHANGES REQUIRED. The two original road-policy findings are closed, but a
reproduced atomic-validation defect remains in the unaccepted road unit. The
orchestrator has now read the entire forester packet; it needs the corrections
below, not merely another reviewer approval line. Harvest implementation stays
conditional on both accepted input pins.

## Findings

**[P2] Validate later records against the latest object only —
`src/sim/src/apply.cpp:337–360`.** The atomic dry run first checks
`Budget::objects` and then calls the placement rule against the unchanged live
tile layer. A prior road therefore does not actually replace the underlying
object for validation. An eligible data-defined road with flags `0x41a6`
(`0x4186 | 0x20`, no coverage bit) is replaceable under the existing compatibility
rule. With an existing ramp, both road options false, and records [that road,
house] on the same tile, sequential execution accepts both, while atomic says
`AtomicAbort / Occupied`. The registry and the road predicate accept this type.
The isolated reviewer probe linked against the current production libraries
passes nine control assertions and fails the intended atomic-success assertion.
Use one coherent latest-object view, not an added precheck followed by stale
physical occupancy. Preserve row-major refusal order and the separate building
occupancy rule. The supplied random tests select fixed type flags and cannot
discover this missing input dimension by running more rounds.

**[P2] Define read-your-own harvest effects before repeated collection —
`docs/sim/forestry.md:79–94`.** The proposed figure body queues claim/unclaim/cut
edits until the whole figure call ends, yet loops back into search and credits
cargo within that same call. A larger data-defined capacity and sufficient
elapsed ticks can select a tile whose queued claim/cut is not yet visible, then
credit another harvest before the first edit commits. Immediate merge between
different figures does not resolve this same-figure stale read. Specify the
transaction/overlay semantics and add a two-tree, multi-cut-in-one-visit example.
Credit must depend on the successful planned consumption, and later transitions
must observe earlier edits. Do not silently discard elapsed time to avoid it.

**[P2] Make claim policies and validation internally consistent —
`docs/sim/forestry.md:55,99–105`.** The draft requires a claim/active-target
bijection and unconditional cleanup, then proposes faithful leakage, a one-way
invariant, and automatic repair at validation. These cannot all describe the
same saved state. Placement can drop a claim while the old worker still targets
the tile; a later worker can then claim it. Blind cleanup by the old worker could
erase the new claim. Define mode-specific valid states, target invalidation and
ownership of cleanup. Validators/loaders must not silently change authoritative
claims; deterministic runtime transitions perform repairs when the selected
policy requires them. The optional hygiene/uncuttable policies need explicit
status. Also do not impose the baseline forester's `raw <= 0xffff` bound globally
on wider Farm/mod configurations based on its two-worker arithmetic.

**[P2] Reconcile dispatch behavior and pin timing that changes future state —
`docs/re/forestry.md:174,201–204`.** Section 4.3 says full output stops sends,
but section 2.1, the pure reference and sim examples say a two-worker forester
can dispatch at full output when the worker/rate conditions pass. Correct the
contradiction. First job execution and home-counter release also change future
dispatch eligibility and therefore the PRNG timeline; they are not merely
display/first-wood timing details. Finish the named bounded common lifecycle
trace or explicitly identify an approved existing-engine adaptation. Do not
claim the absence of other kill cleanup as established while its callers remain
unread. Count eligible dispatch attempts, including refused sends, separately
from successful worker creation.

**[P2] Reject a forester reference run that executes no vectors —
`tools/forester_ref.py:186–187`.** An empty or type-directive-only file returns
zero and reports zero failures. The reviewer reproduced `0 vectors, 0 failures`,
exit 0. This violates the completion contract and can turn a missing fixture
body into a pass. Reject zero executed cases and add a public-command negative
beside a non-empty positive control.

**[P2] Stop the drain reference when production cannot progress —
`tools/forester_ref.py:117–122`.** `drain` exits only on starved status 1. For
valid u16 raw input 4096, 92 steps produce 1288 units and leave raw 1152; the
next step reports full status 2 without changing either store, so the loop never
ends. The reviewer reproduced this using a one-second bounded child process.
Return the full-output terminal result (or otherwise define the bounded helper
domain explicitly) and add the hand-derived vector `drain 4096 -> 1152 1288`.
Do not change the independently established production arithmetic to stop the loop.

## Accepted findings and retained evidence

Candidate `c82608bd22a8a86ff50dddc039143b46e658467c`; road gate source
`eae9f4aeff6030c1886970fe3271160c538f15af`; comment/doc-only increment
`aad1735bc00612695eb49ce356a4e0a42eb17d5d`. The source review includes the
complete policy/state/reference/test delta and its surrounding production
callers, plus the entire forester RE note, contract and reference tools. The
candidate worktree remained clean and was not modified.

The original road P2/P3 findings are closed: both options are false-default,
independent, actually passed from World Rules, saved and hashed; class `0x1000`
keeps its original threshold/slope checks. The honest version-125 control loads,
and resealed old-version controls assert the version diagnostic. Independent
Python bytes/g0/g1/s5/s6 hashes agree. The other regenerated goldens have the
executor's dependency explanation; this is not independent derivation of all 25.

The orchestrator re-read raw gates with their saved inventories:

| Lane | Selected | Executed passes | Failed / empty / incomplete | Skipped |
|---|---:|---:|---|---:|
| Replay/references | 46 | 46 | 0 / 0 / 0 | 0 |
| Focused ASan/UBSan | 154 | 153 | 0 / 0 / 0 | 1 opt-in |
| Debug sim-all | 1063 | 1061 | 0 / 0 / 0 | 2 opt-in |

All have zero recognized sanitizer lines. The orchestrator reran the 77 placement
vectors and `sim_stream.py --check`, both clean; the 44 forester vectors also
pass. The latter do not test the empty/full-output counterexamples above.
Retained replay evidence is 103/103 honest checkpoints and altered-road
divergence at entry 35594. These valid results are preserved, not converted into
proof for the missing input cases. No broad compiled suite was rerun by the
orchestrator. The small atomic probe is exploratory negative evidence, not a
canonical acceptance gate.

Reviewer repro files and outputs are under ignored
`cache/review-road-close-forestry/` in the main checkout. The probe used only
synthetic types and existing production libraries. Its failing assertion is
`CHECK(dry.ok())`; both sequential records first succeed. The full-output Python
probe stopped only its exact owned child on timeout. No coordinator or other
executor process was touched.

## Scope decisions for finalizing E1

- Reuse the existing deterministic tile-level walker for the first slice and
  describe the missing finer-route veto/retry as an inherited limitation. It
  can affect acceptance/selection and timing, not just a cosmetic delay. No
  pathfinder rewrite or broad finer-route research campaign is required here.
- The cut-to-young transition and its existing growth sweep are part of this
  resource lifecycle and are allowed/required in the approved implementation
  scope. This does not authorize another planting system or prove sustainable
  forestry balance.
- Claim hygiene and skipping uncuttable resources may be explicit false-default,
  hashed enhanced choices, with original and enhanced behavior specified
  separately. Enhanced cleanup must never erase another live worker's claim.
  No automatic state repair at validation/load is authorized.
- Use existing `StoreDelta::Input` for Farm.raw if its reviewed semantics suffice;
  do not mandate a duplicate Raw effect kind. Make same-figure tile-effect order
  explicit. A new dispatch draw needs its proper DrawSite and PRNG-timeline
  identity, not just a state-layout bump.
- The real canary stays on the unchanged candidate, existing trees and original
  loadout, with one expensive opening. Move the tree/no-tree differential into
  small declared fixtures. Its resource ledger covers every eligible type's
  actual N -> N-1 transition, not just 105 -> 104, and derives starting wood from
  the current manifest/founding/unloading ledger rather than an old HUD example.
  Keep carrying/returning save/reload coverage from the task board.

## Next boundary

MAP-ROAD-CLOSE-2 and MAP-ECON-1 E1-FINAL in [TASKS.md](../TASKS.md) cover these
finite corrections and narrow re-review. This direct review supplies the missing
whole-packet read; corrections need focused review, not repeated unrelated
campaigns. E2–E6 remain queued, with both prerequisite acceptances pending.
Windows remains on exact `0c401c8d084cfd9feb8aec846595291f87f22cce`.

Independent documentation reviewer `/root/review_evidence_workflow` approved
this review/dispatch delta with no findings. It checked pins, bounded correction
scope, evidence qualification and the pending implementation latch, and verified
all seven Windows task sections remain byte-identical. The source/contract
review and reviewer probes above were performed directly by the orchestrator.
