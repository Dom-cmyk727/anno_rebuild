# WILDLIFE-RE-1b review (2026-11-12)

**Verdict: ACCEPTED.** The animals' own life is read with addresses throughout: the complete override table of
both animal classes, their tick, motion and death, construction and attach, the spawn record's release, and the
route and walk draws. Every address the orchestrator re-checked matches. The draw table says plainly what it is not
yet (a certified total over transitive callbacks, OQ-578) instead of assuming zero. No fix round.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WILDLIFE-RE-1b` `34d3fdce`, one commit on `main` `eeec2cd9`: `docs/re/wildlife.md` sections
4–7 (+379 lines), dated bullets on OQ-573 and OQ-575, new OQ-578 to OQ-582, the REPORTS entry. Documents only, in a
new worktree. Hosted CI green (run 43217006006). Committed at 13:20, 28 minutes after the packet reached `main`.

## What the orchestrator re-checked

`tools/disasm.py` and direct table reads on GOG `AnnoWelt.dll` (SHA-256 `f220a878…e6e2`):

| Claim | Where | Result |
|---|---|---|
| Override table: slots `+0x28`, `+0x74`, `+0x110`, `+0x174`, `+0x17c` in Boden `0x100c35a8`, WildTier `0x100c653c`, WildTierNew `0x100c66fc`, Zivilist `0x100c6a98`; slots `+0x194`–`+0x19c` exist only in WildTierNew | the four tables | matches, all 20 entries |
| The generic route factory returns null for routes 62, 63, 64: route − 13 indexes the byte map, all three map to index 11, the default `0x1003b43a`, with `eax` cleared at `0x1003b0ff` | `0x1003b0fb`–`0x1003b113` | matches |
| New completion draw `0x10051b77`: `(~draw & 1) | 26` | `0x10051b77`–`0x10051b81` | matches |
| Walk completion draw `0x10051b9c`: `% 5` | `0x10051b9c`–`0x10051ba9` | matches |
| Behaviour 7's draw `0x10051515`: `% 6` | `0x1005150c`–`0x1005151d` | matches |
| Radius draw `0x10058382`: `div` by the radius modulus | `0x10058382`–`0x1005838a` | matches |
| Angle draw `0x100583b2`: `div` by `2 × half_span + 1` | `0x100583b2`–`0x100583be` | matches |
| Idle draw `0x10051441` | `0x10051441` | matches |
| Old kind-3 draw `0x1005e35d`, then `test al, 1` | `0x1005e35d`–`0x1005e363` | matches |
| The release: category-0 lookup by the home chunk (signed remainder by 8), then `and byte [eax+8], 0xbf`; no write to `+0xc` in the callback | `0x1008bdb0`–`0x1008bde5` | matches |
| WildTierNew's constructor budget 25000 at `+0xb0` | `0x10052cea` | matches |
| Kinds 49/51 branch allocates `0xbc` | `0x1009a4a1` | matches |

**Not re-checked:** the action machine's transitions beyond its two draw sites; the sine tables and their copied
tail; the walk clamp to low + 3 / high − 4; the cargo quantities `0x200`/`0x600`; the habitat target search
`0x10092320`; Old route control apart from its draw. WILDLIFE-RE-1d's reference vectors exercise the walk arithmetic.

## Notes (no fix round)

- **Two corrections to earlier notes, both well evidenced:** the generic route factory does not build the animal
  route (route 62 returns null; the old animal class builds its own, the new one runs an internal state machine), and
  the two flag bits live in a word at figure `+6`, read through the query slot `+0x8c`.
- **A good catch:** the new animal's display code draws sound variants from the C runtime's `rand`
  (`0x100b68a9`), not from the world generator. Those draws must stay out of the simulation and its reference
  vectors.
- **OQ-578 is the main risk for a faithful port.** The direct draw inventory is complete for the bodies read, but
  not certified over transitive callbacks (shared search, resource and storage callbacks, runtime type interfaces).
  Before WILDLIFE-1 changes the PRNG timeline, OQ-578 needs closing, by more reads or by a draw-count comparison
  against the running original. The orchestrator will size that when 1c reports.
- **`wildlife.md`'s title and 1a's handover paragraph were rewritten,** because the paragraph had become stale (it
  called OQ-578–579 unused). OQ-573 and OQ-575 got dated bullets only, as the packet asked.

## Disposition

Lands as is, with this review, with the owner's OK. Next on the Mac lane: WILDLIFE-RE-1c (the hunter's approach, the
fur trapper, the wolf and predators), released only with the owner's go.
