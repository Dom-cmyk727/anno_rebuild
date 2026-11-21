# Review: ISLAND-CROPS-1 stage 1b (the island's crop list in the World)

Reviewer: orchestrator (Claude Code), 2026-11-17. Head `1eef2cfb` on `task/ISLAND-CROPS-1`, on 1a
`4bebe08e`. CI run 44108255131 attempt 3: green on that head (attempts 1 and 2 had no macOS runner).

**Verdict: ACCEPTED.** It changes the state stream, so it lands only after the Mac's full game-file
run on its candidate.

## What was checked

- **Versioning as decided:** section bases header 2 and islands 2, `kStateHashVersion` stays 139;
  the outgoing stream is kept as `legacy139s1/` and refused by a test with a clear message.
- **The writers match the cited reads:** constructor value = every id of the mask (`0x1008cbdf`), the
  ANBAU base set unless climate 0, names inside `0x38..0x44` only, the 13-name cap counting unknown
  names, and INSELLNK = base set plus `0x38 + k` per raw mark, unconditionally. Every constant is in
  `CropTables` with its address and validated; `links.raw_kinds <= crops.mask_bits` is enforced.
- **Adaptations are labelled as such:** a second ANBAU record refuses the load (the original keeps the
  last); the unknown-name warning says the original is silent.
- **No reader:** production, AI and UI do not read the field, so every moved golden moves by the
  section bytes alone. The list of 50 pins and the `current/` fixtures is complete and each has its
  reason; the oracle-owned pins come from `tools/sim_stream.py`, which now transcribes the field.
- **Failure proof:** without the INSELLNK assignment 4 of 6 selected tests fail.
- **Gate:** pins check 1,690 executed, 0 failed (3 known symlink skips on this account);
  run_selected 95/95; WSL clean; CI green.

## NOT_RUN (accepted as stated)

The Mac's full game-file run: it runs on the landing candidate.
