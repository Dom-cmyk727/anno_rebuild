# MAP.2 founding: second packet review, 2026-11-01

**Disposition: changes required, limited to the two findings below.** Continue
gameplay work under the next packet in `../TASKS.md`; this is not a restart of
founding or a coordinator assignment. MAP stays on the Mac. Rendering's Windows
dispatch is unchanged.

Reviewed `d3bfbd7..f0812cde4103cc8884cf749abe6f72294dc60c83` on
`task/MAP-2-original-like`; code head
`4f22cf6495266647e8f2ac4b702eaf3d7243bd88`. The worktree was clean. The review
covered the complete diff, relevant callers, tests, review ledger and saved
evidence. No files or builds in the MAP worktree were changed.

## Findings

### P2 — Bound signed hull facts at every supported entry point

`src/sim/include/anno/sim/types.hpp:173` changes the hull to an optional signed
64-bit length. The original-file decoder bounds it, but
`TypeRegistry::Builder::figure_type` / `build` accepts an arbitrary `int64_t`.
The founding adapter passes this fact to `veto::nearby_figures`; its hull-box
arithmetic adds/subtracts it from fixed-point coordinates without overflow checks.

A scratch probe built a registry with `hull_half_88 = INT64_MAX` successfully,
then passed that registry fact to the public veto with an ordinary nearby ship.
UBSan reported signed overflow at `figure_veto.cpp:93`:
`14208 + 9223372036854775807`. This is an invalid-content/API-boundary defect,
not a claim that the shipped figure files trigger it. Enforce a documented safe
domain through the builder as well as the file adapter, and make the public veto's
input contract safe. Preserve absent, present-zero and the shipped negative type
391 as distinct supported facts; do not solve this by rejecting all negatives.

### P2 — Assert replay process status independently of its text

`src/tools/sim_replay/CMakeLists.txt:39–46` uses
`PASS_REGULAR_EXPRESSION` for both new replay cases. CTest treats a matching
expression as success even when the normal exit status is wrong. In particular,
the negative fixture still passes if the CLI prints `DIVERGED at entry 3` but
incorrectly returns 0. `WILL_FAIL FALSE` does not enforce the expected exit 1.

An isolated CTest project using the exact negative properties and a fake child
that printed the expected line and exited 0 passed 1/1. The real CLI currently
behaves correctly: this is a regression-test defect. Use a small public-command
contract that checks exact status and output together; test right-message/wrong-
status and right-status/wrong-message independently. Correct the analogous replay
success cases when reusing that helper, without redesigning the shared gate.

## Evidence checked

The canonical verdict reader independently reconciled all five saved `final3`
logs against their inventories and recorded gate endings. Their source identity
is `4f22cf6`, and every recorded test-binary hash matches the current binary.

| Saved gate | Selected | Executed passes | Failed / empty / incomplete | Skipped |
|---|---:|---:|---:|---:|
| Focused | 136 | 136 | 0 / 0 / 0 | 0 |
| Full sim | 819 | 818 | 0 / 0 / 0 | 1 opt-in |
| Formats | 82 | 82 | 0 / 0 / 0 | 0 |
| ASan/UBSan focused | 136 | 136 | 0 / 0 / 0 | 0 |
| ASan/UBSan full sim | 819 | 818 | 0 / 0 / 0 | 1 opt-in |

Review checks, not another acceptance campaign: 23 existing focused cases passed
(2,167 assertions); the real fresh-process founding replay reproduced 4/4
checkpoints with exit 0; the altered replay diverged at entry 3 with exit 1.
The two independent probes above demonstrated the remaining defects. Scratch
evidence is retained in the orchestrator checkout under
`cache/orch-map2-founding-2026-11-01/`; it is not a portable GitHub prerequisite.

## Assessment and limits

The earlier crew-slot, hull-presence and in-advance draw-accounting findings have
targeted fixes. The packet adds useful gameplay proof: real-island founding with
ore discovery, a different warehouse definition/player/city, and separate replay
and executor-usage claims. The local gate counts are supported.

This does not establish full original-game parity. Kind-11 city-number reuse is
still conservatively refused, loaded COMP records remain uncertain, and discovery
event consumers are not implemented. File-loaded ore objects are not mineable
deposits. Windows/Linux and original-runtime checks remain not run. The figure-
count microbenchmark is not a late-game economy or multiplayer benchmark.

The intentional unreleased stream-120 layout changes are documented, but the next
persisted-state change must receive a distinct version and cause-specific old-
version rejection coverage. Figure-type digest changes still lack an independent
byte oracle; the next packet closes this small gap rather than relabelling
self-generated hashes as independent proof.

**Owner decision C5, received during this review:** the five-player ship start
begins at **peace, with shared visibility, and without alliances**. Sharing is
independent of alliance/trade/military rights. It does not grant land, select a
candidate, force co-settlement, or pre-reveal unexplored terrain. Original loaded
scenarios retain their own rules; this new start policy is an explicit adaptation.

Independent dispatch review: `/root/review_windows_dispatch` inspected this report,
the MAP-FOUND-3 task packet and relevant interfaces, and approved the documentation
package with no findings. It did not repeat or perform the MAP source-code review.
