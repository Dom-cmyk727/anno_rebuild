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
