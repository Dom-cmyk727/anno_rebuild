# FISHER-RE-3: a fisher hut with every entrance on land

**Status:**
- **Written and RELEASED** 2026-11-15 by the Claude Code orchestrator for **the Mac executor**.
- **The owner's order** (2026-11-15): "fisher hut with every entrance on land. this needs to be matching pixel
  perfect with HE as reference."

**Base:** `origin/main` `67a960c9` or later. Branch `task/FISHER-RE-3`. About 2 hours. A read with a reference
vector: no engine code, no pins. Never rebase, force-push or push `main`. No reviewer subagents.

## Why

FISHER-1d's rig with **both** entrances closed (corner mean 32 or more) reached the cut but did not deliver
([`fisher.md` §7](../../re/fisher.md)). FISHER-1e proved whole trips only for huts with one open entrance. The
closed case has no proved rule, and none may be invented.

## What "pixel perfect with HE" means for this packet

The owner's acceptance for this behaviour is a **picture comparison against the History Edition**: the same hut on
the same map, the boat seen at the same places in the same frames of its trip. That comparison is made on the
Windows PC, where the History Edition is installed, in a later step (FISHER-HE-1, not released). This packet makes
it possible:
- it finds the rule in the binary;
- it writes the exact capture recipe the Windows step will follow;
- it claims no History Edition parity itself. Anything not read in a History Edition binary is marked so.

## Steps

1. **Can the case arise at all?** Read the original's placement rule for the three FISCHER types (810, 3978, 3510):
   what a site must have (coast, water tiles, entrance tiles). If the rule guarantees at least one entrance on
   water as [`fisher.md` §3](../../re/fisher.md) defines it, say so with addresses: then the all-closed case
   cannot be built or loaded normally, and the question closes there. Check it against the shipped maps: the census
   found four standing huts, each with exactly one open entrance.
2. **If it can arise:** read what the worker does, with module and address for every claim:
   - at dispatch: whether a hut with no open entrance sends a worker out at all;
   - on the way out and at the catch;
   - on the way home: where the walk ends, what happens to the cargo, the worker and the hut's raw stock;
   - any RNG draw and any saved state on those paths.
3. **The oracle:** extend `tools/fisher_ref.py` with hand-derived vectors for the case (or for "cannot be placed"),
   and its contract test. The vectors come from the read, not from the engine.
4. **Both editions:**
   - the GOG 2.0.0.5 binaries are the read's base;
   - if the History Edition binaries are available to you, find the same routines there (the RTTI names in
     [`he-vs-gog-comparison.md`](../../research/he-vs-gog-comparison.md) help) and report every difference;
   - if they are not available, say so: History Edition NOT_RUN. Do not assume the two agree.
5. **The capture recipe**, in `fisher.md`: a shipped map, the player, a hut position and rotation where every
   entrance is on land (or the nearest placeable site, with its entrance means), the build steps from a fresh
   start, what to record (the boat's tile positions and the frames where it appears, turns and disappears, the
   hut's stock), and for how long. Use names, coordinates and counts only. The Windows step must be able to follow
   it in the History Edition and in the rebuild without asking.
6. **If** the rule cannot be proved in the time, stop with a QUESTION naming the exact gap. Do not guess.

**Done:** the note in `fisher.md` with addresses and evidence letters, the oracle vectors and contract test, the
capture recipe, the REPORTS entry of about 60 lines, READY FOR REVIEW. No sim code. 0 pins moved. No original
bytes, pixels or script text in git.
