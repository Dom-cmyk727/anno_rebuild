# Mine/quarry/discovery-mask open-question follow-ups (2026-11-07)

Read-only follow-up to the research pass behind OQ-552/OQ-553/OQ-554 and
`docs/sim/salt-extraction.md` section 22 (both on `origin/task/MAP-EXTRACT-1-x2`,
not on `main`; this note lives on its own branch so it does not touch that lane).
Every claim is tagged **verified** (re-disassembled/re-checked in this pass, with
the exact instruction(s)/address(es) cited), **one pass** (read once, by the
earlier research pass this note follows up on, not independently re-derived
here), or **open** (not resolved). GOG 2.0.0.5 binaries are `game/app/*.dll`;
"the earlier pass" is the prior research fork's report. No original text,
strings or code listings beyond short address and instruction-mnemonic
references.

## Q1 — mine attach reached through the loader and Place()

**The call chain from the factory to attach, verified.** The shared
object-construction factory (`AnnoWelt.dll 0x10024010`) ends in an epilogue
that, on success, loads the just-constructed object's own vtable pointer from
`[ebp]` into `eax` (`0x10024659`), sets `ecx = ebp` (thiscall `this`,
`0x1002465d`), and calls `dword ptr [eax + 0x28]` at `0x1002465f`. That is a
polymorphic virtual dispatch through vtable slot `+0x28` of whatever class was
just constructed — not a direct call to `0x1006e270` anywhere in the module.
For a `Mine`, the constructor (`0x1006e460`) writes vtable `0x100c8990` into
`[esi]` at `0x1006e47f`; reading `.rdata` directly confirms slot `+0x28` of
that vtable, absolute address `0x100c89b8` (`0x100c8990 + 0x28`), holds
`0x1006e270` (attach). A second call through the same slot follows at
`0x100246be` (`call dword ptr [edx + 0x28]`, argument `[edi + 0x54]`, the
world/city object) — matching the earlier pass's "kind 0x1400 sets the city"
reading. Both instruction addresses, the vtable base, the slot arithmetic and
the `.rdata` content were re-read directly in this pass.

**Which caller is the loader vs. Place(), verified.** Of the factory's four
callers: `0x100265b7` sits inside the function at real entry `0x100264a0`
(re-disassembled in full, 0x164-byte stack frame, a record loop over a count
at `[esp+0x180]`). `xref.py` (code sweep) finds exactly one caller of
`0x100264a0`, at `0x1002eadb`; the earlier pass's characterization of that
caller as a chunk/tag-string dispatch routine is **one pass** (not
re-disassembled here, out of scope for this follow-up). The other three call
sites — `0x10027c4c`, `0x10027cb8`, `0x10028245` — fall inside
`0x100279a0`-`0x10028fd0` (checked directly against the already-documented
`Place()` range), the live player-build path. So: `0x100264a0` = the
save/island loader, the other three = `Place()`. Verified for the range
check and the single-caller fact; one pass for what `0x1002eadb` itself is.

**Whether the loader overwrites mine `+0x24` after construction: verified —
it does not.** The loader passes the factory an out-parameter (`lea
ecx,[esp+0x14]` pushed at `0x100265a3`); on return it tests `eax` only for the
factory's HRESULT-style failure code (`test eax,eax; jl 0x10026732` at
`0x100265bc`-`0x100265be`) and never references `eax` again — the actual
constructed-object pointer lives in `[esp+0x14]`, written by the factory via
that out-parameter. Tracing every use of `[esp+0x14]` for the rest of the
loop body (down to the next-iteration jump at `0x10026715`/`0x10026730`)
finds exactly two virtual calls made directly on the object: `call dword ptr
[eax+0x7c]` at `0x100265dd` and `call dword ptr [eax+0x78]` at `0x100265f7`.
For the `Mine` vtable these slots resolve (direct `.rdata` read) to `+0x7c =
0x1006e310` (Mine-specific) and `+0x78 = 0x10071c80` (shared/base-class).
Both were re-disassembled in full:
- `0x1006e310` only writes `word` values to `[esi+0x18]` and `[esi+0x1c]`
  (the same two fields the constructor zeroes) — a position/rotation setter,
  consistent with the earlier pass's characterization of the Mine-specific
  override cluster. No write to `[esi+0x24]` or any offset near it.
- `0x10071c80` is a pure getter: it reads fields from `this` (`+6`, `+8`,
  `+0xc`, plus two virtual calls through `this+0x14`'s vtable slots `+0x50`
  and `+0x54`) and writes them into the *caller's* out-parameter buffer; it
  never writes back into `this` at all.

No other code in the loop body touches the object held in `[esp+0x14]`; the
remaining instructions from `0x100265fa` onward operate on the world/city
object (reloaded via `[ebp+0x54]`) and an unrelated list, not on the just-built
mine. **Conclusion, verified within the scope read:** nothing in this loop
restores or overwrites `mine+0x24` after construction. The only writer of
`mine+0x24` is the constructor (zero, `0x1006e478`) and the attach() call
that runs inside the factory itself, for both the live and the loaded path
alike.

**The implication.** Since attach() runs unconditionally inside the factory
for both `Place()`-built and loader-built objects, and nothing after it
touches `mine+0x24`, **the original repeats the radius-7 ore lookup at load
time**, exactly as it does at placement — a loaded mine's binding is
re-derived from whatever ore is on the island at load time, not restored from
a saved pointer. Our sim instead keeps the saved bound index (or its absence)
across a reload and never re-runs the lookup. This is a real behavioral
difference between the two, stated here as a finding, not a recommendation —
whether or how to change the sim to match is not this note's call.

## Q2 — AI ore check (FuncB)

`FuncB` (`CompLogic.dll`, real entry `0x10030230`, containing the documented
call site `0x100302a5`) has exactly three callers per the earlier pass's xref
sweep: `0x10010f2d`, `0x10030472`, `0x100336e9`. **Open** — none of the three
were read, by the earlier pass or in this follow-up; this note does not
fabricate content about them. What remains to trace: read each caller's
surrounding function to determine whether `FuncB`'s result (which per the
earlier pass's one-pass read of `FuncB` itself looks like it may write an
AI goal/order code conditioned on deposit remaining-amount thresholds `0xa`
and `0x14`) actually gates whether the AI issues a mine-placement build order,
or is consumed for something else entirely (e.g. a economic-value estimate on
an already-placed mine). `FuncB`'s own body is **one pass** (the earlier
research fork read it once; not re-disassembled in this follow-up).

## Q3 — quarry

`0x1008b950` (`AnnoWelt.dll`) as a terrain/height-only cursor check: **one
pass** — the earlier research fork fully disassembled it (bounds-checks a
cursor rectangle against a per-tile height byte array via `[this+0x8c]`
stride / `[this+0x94]` base) and found no ore/deposit read; this follow-up
did not re-disassemble it a second time, so it is not re-upgraded to
"verified" here.

Everything else about the quarry stays **open**, exactly as before:
- The quarry's actual stone/marble production source — unresolved because
  the quarry class's own constructor/vtable was never located (the technique
  that found Mine's vtable, `0x100c8990`, was not applied to the quarry).
- The caller(s) of "deposit slot `+0x80`" — the earlier pass could not even
  pin down precisely which class/object's `+0x80` this refers to; not
  resolved independently here either.
- Whether the quarry's HIGHMAP table entry (type `+0x234`) is structurally
  identical to the mine's class-`0x3000` path or has its own flavor — not
  compared.

## Q4 — discovery mask

**The two function tables holding `0x10063490`: verified.** Re-ran
`xref.py --reloc game/app/AnnoWelt.dll 0x10063490 0x10063491` directly in
this pass: it returns exactly two `.rdata` references, at `0x100c7588` and
`0x100c761c` — matching the earlier pass's finding exactly, now independently
re-confirmed rather than taken on faith.

**The ore object's own `+0x88` getter and the `+0x2c` discovery mask: still
open.** The ore-container class (`0x100c9b10`, whose `+0x14` is the deposit
lookup `0x100787c0`) was traced in the earlier pass and found to have many
stub slots including its own `+0x88` — but that is the *container*, not the
individual ore *object* that actually carries the remaining-amount and
discovery state. The ore object's own class/vtable was never located (would
need the same construction-call technique used for Mine in Q1, not attempted
here), so whether its `+0x88` getter reads `+0x2c` remains exactly as
unverified as before this follow-up — now for the precise, stated reason
that the wrong class has been examined, not an unread offset on the right
one.

## HE

Re-checked directly in this pass (PE header `Machine` field and
`TimeDateStamp`, both DLLs read from the actual HE Steam install,
`E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition\`):

| Module | Machine | Timestamp | Size |
|---|---|---|---|
| GOG 2.0.0.5 `AnnoWelt.dll` | `0x14c` (x86) | 2006-07-21 | 966,656 |
| HE `AnnoWelt.dll` | `0x8664` (x86-64) | 2020-08-05 | 1,310,648 |
| HE `AnnoWeltAddOn.dll` | `0x14c` (x86) | 2019-09-27 | 1,698,744 |

**Verified.** `AnnoWelt.dll`'s status for every finding above is **NOT_RUN
(architecture mismatch)** — HE's main module is a from-scratch x86-64
recompilation, not a binary-compatible build, so none of the above addresses
or instruction bytes transfer to it. `AnnoWeltAddOn.dll`, however, is still
32-bit x86 (2019, a different, later 32-bit build than GOG's 2006 one) —
**open**, not compared in this pass, and worth flagging explicitly as the one
HE binary that might actually be comparable to GOG 2.0.0.5 by the same
capstone/pefile technique used here, for a future pass.
