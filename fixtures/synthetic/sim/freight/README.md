# Automatic freight contract vectors

Our own synthetic arithmetic, transition and schema inputs; no original files,
decoded assets or disassembly. Evidence and deliberate adaptations are in
`docs/re/ship-trade-routes.md` and `docs/sim/ship-trade-routes.md`.

`contract_vectors.jsonl` has one complete independent input/expected-output pair
per line. Quantities inside holds/stocks/effects are raw 1/256 tons. Action `tons`
means desired total aboard for LoadTarget0, transfer request for UnloadAmount1.
Every action keeps its delay, even after grouping gives a later duplicate zero.
Effect rows are action index, resolved hold, requested whole tons, actual raw
transfer and result. Full and fractional cases deliberately challenge a simple
additive or per-hold direct-request interpretation.

Delay vectors are isolated integer stop clocks; arrival starts at the modern
visit stamp, and a new Move ends old stop credit. They are not physical route or
whole-world partition-invariance claims. Progress vectors test notification
identity, cursor/wrap and control state. Wire/native vectors have hand-written
little-endian hex, including nonempty pending and partially completed route state.
They propose route-only136/ECONBK1/PRNG5, without salt135. Runtime acceptance is
separate and requires public C++ execution, native validation and fresh replay.

Run `python3 -I tools/freight_ref.py check` or `selftest` with the vector path.
`tools/freight_ref_test.py` tests the public CLI, including empty/malformed input
and wrong expectations. Neither program executes a simulation or copies an
expected answer into the computation.

The partial-progress native witness is arithmetically reachable: LoadTarget5t,
matching holds0/2 initially256/0 raw, emptyhold1, capacity1024, source512. Candidate0
requests3t from its768raw room, transfers512 from available stock and leaves512
remaining toward1280target; candidate_next1. The cached initial matching total
is256, not current total768. The separate Wait record has321ticks left and0credit.

Cargo inputs explicitly supply hold_count_a. Extra vector entries are preserved
but excluded from matching totals/candidates/resolver. Lifecycle includes the
separate NeedLeg visit, matching/stale failures, idempotent controls and failed
manual commands. Control clocks distinguish active pre-suspend time from paused
time. Native Wait may legitimately retain credit after Suspend/Resume accrual.
