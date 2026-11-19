# WILDLIFE-RASTER-FP-1 — close the original load-thread numeric profile

**RELEASED after independent dispatch review, 2026-11-19.** EXECUTOR II MAC.
One bounded 2–4 hour research packet, six-hour checkpoint. Resolve the specific
remaining floating-point precondition of the secondary raster. Do not turn this
into a general reverse-engineering or numerical framework project.

## F0 — intake

Create `task/WILDLIFE-RASTER-FP-1` from locally reviewed
`8217e1879aead7fa3b451c155b060f0a05405874`. Read this packet at the supplied
exact `orch/mac-lane` dispatch, without merging the board/main or rebasing.
Preserve previous refs and raw evidence. Keep this exact assignment through
context resets; earlier PREFIX/CLIFF packets are historical.

Read CLAUDE; PLAN §0.1–0.3; TASKS owner rules/§1; evidence-contract/testing README;
`docs/sim/wildlife-secondary-raster.md` §§1–4; its orchestrator review;
`docs/sim/wildlife-postload.md` §4; navigation CAP-2 and the startup/reset helpers
already indexed under `cache/wildlife-raster-contract-1/`. Production routing,
fill and graphics remain unchanged and unwired. Owner navigation compatibility
approval is a separate question and does not authorize this packet to repair it.

## F1 — two explicit numeric checkpoints

Identify separately (a) the control word when the height table is initialized,
and (b) the control word at the first postload raster callback, including the
scales, per-sample stores and normalization. The verified CRT reset sets precision
53 but leaves rounding bits outside its mask. It does not prove nearest rounding
or the later callback environment.

Reuse the resolved chain: Welt process attach/CRT hook and initializer array,
`0x1007c760` table writer, original scene/world load and second callback,
`0x100a3cc0` / arithmetic at `0x100a3cce`. Trace the actual calling thread where
provable; don't assume DLL initialization and scene load run on the same thread.
Record a concise edge table: caller/callee, thread fact, entering condition,
control-word write, restore on each relevant exit, resulting known/unknown bits.

## F2 — bounded writer and restore audit

Inventory concrete reachable control-word writers/reset operations in the
participating original modules, including direct x87 instructions and resolved
runtime wrappers. Distinguish precision, rounding and exceptions. Check the
conversion helper's save/restore and any caller that changes state without
restoring it. Inspect only direct edges needed for the actual supported load
path; enumerate the scanned modules/ranges so a negative search has honest scope.

An external library call or unresolved thread transition is an unresolved edge,
not proof of mutation and not proof of preservation. Do not attempt to rule out
arbitrary injected code, every computed pointer or all possible process history.
Do not require an unbounded whole-program proof to finish the packet. Reuse
aligned listings/module hashes; any newly decisive assertion needs its own
primary address and smallest distinguishing example.

## F3 — quantify whether the open condition matters

Use small independently hand-derived inputs to distinguish plausible remaining
profiles at the two checkpoints. Precision and rounding must be varied separately. Table initialization and
raster execution profiles must also be independently selectable; do not assume
their modes match.
If extending the private exact-dyadic model, reproduce known controls first and
show a meaningful changed result or explicitly state that the sampled cases
agree; agreement of a sample is not a universal equivalence proof.

Keep actual x87 operations, f32 memory-store rounding, retained accumulator and
integer truncation distinct. Preserve the 0.04 bit pattern, per-add stores,
nominal border denominator and converted-byte mask. Do not switch to rational
1/25 or a byte mean. A source-derived model is not original runtime observation.
No large exhaustive census or new emulator installation is required. Stop once
the remaining condition is either proved for the supported path or demonstrated
as a real, precisely located runtime dependency.

## F4 — close or hand off exactly, without choosing a policy

If static closure succeeds, document the supported path and its preconditions,
then propose the smallest isolated integer/dyadic helper contract with independent
expected vectors. Do not implement it yet or silently generalize to all threads,
save/load entry points or arbitrary histories.

If a reachable external/thread edge prevents closure, write a concise Windows
capture card: module hashes, breakpoints at table initialization and callback,
thread identity, non-mutating control-word observation, a few decisive table/output
bits, exact scenario/load steps, and expected discriminating outcomes. Explain
which observations settle which claim and which are only corroboration. Keep
original bytes/listings and runtime dumps private. Do not dispatch work to the
Windows lane yourself; send the card to this orchestrator for coordination.

Both outcomes are valid READY FOR REVIEW results. Do not repeat this same static
search indefinitely, label an unread address a policy decision, or choose nearest
rounding merely to unblock implementation. Provide one bounded next task that
can advance wildlife integration while any genuine Windows capture is pending,
based on known dependencies; no automatic permission to start it.

## F5 — verification, scope and direct reportback

Expected scope: research/spec/evidence docs and private tiny checks. No production
src, CMake/CI, state/native/tag/default, routing, World wiring or other-lane changes.
No full CTest/build/sanitizer campaign for a docs-only packet. Run docs/link/scope
checks, hash the decisive private receipts and report exact direct-probe outcomes.
If adding public reference code is necessary, include its bounded CLI contract;
never manufacture canonical gate counts. Preserve failed explorations and state
unavailable evidence accurately. Leave coordinators, other jobs and the owner's
awake assertion alone.

Ordinary-push your task branch only. Send READY FOR REVIEW directly to
**Orchestrator II Mac**, this assigning chat, with full head/tree, exact CI URL,
the two-checkpoint verdict, addressed acceptance/evidence table, decisions,
remaining conditions and NOT_RUN. Submit before CI finishes; terminal receipts
separately. No executor reviewer agents or self-approval. Stop at review; the
orchestrator reviews and releases the next bounded task directly.
