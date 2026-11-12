# WILDLIFE-RE-1d review (2026-11-12)

**Verdict: ACCEPTED.** `tools/wildlife_ref.py` turns `docs/re/wildlife.md` into an independent integer oracle: raw
world draws in, decisions and the number of draws consumed out. Its 65 hand-derived vectors cover the ten record
types the packet asked for. Every vector the orchestrator worked by hand agrees, and both tests pass in the MSVC
build too. No fix round.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WILDLIFE-RE-1d` `ce2c8ff1`, two commits on `main` `0ac461d8`: `tools/wildlife_ref.py` (300
lines), `tools/wildlife_ref_contract_test.py`, `fixtures/synthetic/sim/wildlife_vectors.txt` (65 vectors), two CTest
registrations in `CMakeLists.txt`, `docs/re/wildlife.md` §11, the REPORTS entry. Hosted CI green, full matrix (run
43233610007). Committed at 15:05, 34 minutes after the packet reached `main`. Merged onto the GATE-1a candidate
`9745e177` as `5e8a15a0`; the one conflict was an append in `REPORTS.md`, both entries kept.

## What the orchestrator checked

- **Runs here.** `python -I tools/wildlife_ref.py fixtures/synthetic/sim/wildlife_vectors.txt`: 65 vectors, 0
  disagreements. The contract test: 31 checks pass (a changed expected value fails with 1, malformed and empty files
  with 2). In the MSVC build at `5e8a15a0`, through `tools/test/run_selected.py --select wildlife_reference`: selected
  2 / executed 2 / failed 0 / skipped 0, `VERDICT: PASS`.
- **Vectors worked by hand** from `wildlife.md`, all agree:
  - density at n = 0, 13, 14 and 16 (4 → 5 at the 13/14 boundary);
  - replenishment: a collision costs a second draw, and the swap with the last candidate decides the next pick;
  - creation: the 18-step species search from index 31 ends on type 116 when no goods match;
  - the loader's jitter (`63 & 31` on base 65 gives 96) and the mode bit replacing the goods;
  - the walk: radius in sixteenths (40 + `d % 32`), the angle with the byte-wrapped facing (`facing 2` → base 0;
    `facing 255` → base 64), behaviour 7's doubled radii (modulus 64) and the attempt byte wrapping 255 → 0;
  - the new animal's transitions: `(~d & 1) | 26`, the quantity override after the draw, `% 5` on walk completion;
  - the old route's kind-3 parity;
  - the prey rule: the mask-8 kind bounds 49 and 64, predator 503 on the short list only.
- **The wolf's `% 7` mapping against the code:** state 30 by default, 28 for remainder 0, 27 for remainders 4–6
  (`0x100534f3`–`0x1005350c`). The vectors and `wildlife.md` say the same.
- **Open cases are refused, not guessed:** candidate exhaustion (OQ-572), a cave without an entrance (OQ-574), zero-HP
  prey (OQ-585), each with exit 2 and its number.

**Not re-checked:** the executor's ten wrong-interpretation mutants (the contract test's own mutations ran here),
the habitat vectors' neighbour arithmetic, and the wolf's attack and corpse transitions.

## Notes (no fix round)

- **The packet had an error, and the executor caught it.** The packet's example "prey 503 admitted only from the short
  list" named the wrong role: 503 is a predator that gets the short list, and no predator hunts 503. The executor
  wrote the correct rule into the vectors and `wildlife.md` §11 instead of following the packet. That was the
  orchestrator's mistake.
- **Scope is honest:** an integer decision oracle, not a lifecycle emulator. Float positions (OQ-582) and transitive
  callbacks (OQ-578) stay outside, as the packet asked.
- **The wildlife reads are complete** (1a–1d, about 1 h 30 min of executor time in total). The next wildlife step is
  the port, WILDLIFE-1, which waits for ADR 0006 because it changes the PRNG timeline and the state number.

## Disposition

Lands after GATE-1a, as `5e8a15a0` plus this review, with the owner's OK. The Mac lane has no released packet; its
next wildlife packet waits for ADR 0006.
