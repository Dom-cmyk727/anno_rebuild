# The view: mouse, hovered tile and camera of the original (observed by running)

**Status:** live first pass plus static T15.B1 stop, 2026-10-25 (orchestrator, owner driving session; T15 play layer stage B,
`docs/testing/play-original.md` §3 item 1). Method: read-only memory diffing of the running copy
(`tools/orig/memdiff.py`, snapshots of every committed writable region, narrowed by "changed on this
input, unchanged on that one"), live reads along controlled mouse sweeps, screenshots. Session
outputs in `cache/orig/drive-20261025*/` (git-ignored). Host: the owner's Windows 11 PC, the copy at
`C:\anno1503-original\` through dgVoodoo2, single-player Citizen continuous game, zoom and rotation
as the game starts (not changed in this session). Addresses below are **process addresses of this
run** unless written as `module + RVA`; the two variables found live on thread stacks, so they are
stable within one run of the game loop and must be re-found per run (§4).

## 1. The game's mouse position (D, by running)

- Two `i32` at `stack 0x1af950` (x) and `stack 0x1af954` (y) hold the cursor in **screen pixels
  of the game's client area** (the window sits at (0, 0), so client = screen here): every scripted
  move of the cursor by 64 px changed exactly one of them by 64 (`memdiff find --delta s0:s1:64`),
  and a move to client (512, 384) read back (512, 384).
- The game tracks the mouse **relatively** and warps the cursor itself: with no input at all the
  physical cursor alternated about once a second between the centre of the 800 × 600 surface
  (400, 300) and the screen centre (1280, 720) while the game window was minimized, and after a
  scripted click it lands 20–25 px from the click point. `0x8affa70` / `0xd1c326c` held the last
  move's delta (64). Consequence for `tools/orig/control.py`: the "cursor moved since the last
  action" check now treats a cursor that stayed **inside the game's client area** as the game's
  own warp and only a cursor outside it as the owner's hand (the foreground and target checks are
  unchanged).

## 2. The conversion

### 2.1 The hovered tile (D, by running)

Two `u16` at `stack 0x1acfbc` (x) and `stack 0x1acfbe` (y) hold the **tile under the cursor**.
Along a horizontal sweep at a fixed screen y (32 px steps, 192 px in all) they went
(673, 498) → (669, 503); along a vertical sweep at a fixed x (16 px steps, 96 px) (668, 495) →
(673, 500). So, at the game's start zoom and rotation:

| Screen move | Tile change | Tile size implied |
|---|---|---|
| one tile width to the right | (x − 1, y + 1) | about 40 px wide (192 px ≈ 4–5 tiles) |
| one tile height down | (x + 1, y + 1) | about 20 px high (96 px ≈ 5 tiles) |

i.e. the usual 2:1 isometric projection with +x towards the lower right and +y towards the lower
left of the screen (the same orientation `docs/render/` uses for our renderer; the exact pixel
pitch per zoom group is still to be measured against OQ-026). The value is written when a mouse
event is processed: after a keyboard scroll with the mouse still it read (0, 0) until the next
mouse move. The coordinates are world tile coordinates (the player's start island sat around
tile (670, 500)).

**Where it lives.** Both copies of the pair that survive three mouse moves are on stacks: the main
thread's (allocation `0xb0000`–`0x1b0000`, 1 MB, `MEM_PRIVATE`) and a second thread's
(`0xbd68xxx`; the same frame shifted by 0x2000 between two reads, with return addresses into
`AnnoFrame.dll` and `AnnoDisplay.dll` next to it). Near the main-thread copy the frame holds return
addresses `AnnoWelt + 0x23164`, `AnnoWelt + 0x23ca4`, `AnnoWelt + 0x76829` and
`AnnoFrame + 0x50386`: the screen-to-tile conversion is called from around `AnnoWelt + 0x23164`
(RVA; load base `0x2520000` in this run, preferred `0x10000000`, so `0x10023164` in the Ghidra
project). No long-lived object field holding the pair was found (the two heap-looking hits of a
first scan were transient buffers). ~~**Next (static, executor):** disassemble the function that returns into `AnnoWelt + 0x23164`
and its callee as the screen-to-tile converter.~~ **T15.B1 correction:** proximity of those words
to the observed pair did not establish a live call chain or a converter. The static read below
contradicts that identification; the live mouse-sweep observations above are retained.

### 2.2 T15.B1 static trace: the supplied addresses do not identify the converter

**D, AnnoWelt.dll, preferred image base `0x10000000` throughout this subsection.** Read with
`tools/disasm.py`, a bounded capstone continuation across return/padding boundaries, `pefile` and
`xref.py --reloc`; no runtime stack unwind was performed. Re-read each relevant function from
its verified start before assigning a role.

- **Return address `0x10023164`:** function **`0x100230d0`**, after the preceding `ret 4` at
  `0x100230c5` and NOP padding through `0x100230cf`, ends at `0x1002317b`. The call at
  **`0x10023161`** is virtual **`[edx+0x58]`**. Its receiver comes from the pointer array at
  `[this+4]`, indexed by the binary-search position (`0x10023159..0x1002315f`); no stack
  arguments are pushed for this call. The containing function takes **one integer argument**
  (`ret 4`), computes argument−8 (`0x100230f6..0x100230fa`), compares that with virtual +0x58
  results, and returns an index or −1. This is the y-sorted building-list search already
  documented in `docs/sim/state.md` §4.8/F3, not an identified screen-pixel conversion.
- **Return address `0x10023ca4`:** function **`0x10023c80`**, after `ret 4` at `0x10023c6e`
  and NOP padding through `0x10023c7f`; final `ret 0xc` at `0x10023d7c`. The direct call at
  **`0x10023c9f`** targets **`0x10023180`**, with two pushed arguments and
  `ecx=this+0x178` (`0x10023c97..0x10023c9f`). It is mask-selected **BuildingIndex tile lookup**:
  pefile gives `0x100c2e50 = 0x10023c80`, slot +8 in vtable `0x100c2e48`, installed at the
  object's base by `0x100269bd` (relocation at `0x100269bf`). `state.md` §4.8 identifies that
  index at city+0x144. This receiver is the index itself, not an assumed camera or Y interface.
- **Return address `0x10076829`:** function **`0x10076670`**, after `ret 8` at `0x10076668`
  and NOP padding through `0x1007666f`, with epilogues at `0x10076942` / `0x10076950`. The
  call at **`0x10076823`** is virtual **`[eax+0xc4]`**, on the function's argument0 object
  (`0x1007681c..0x10076823`), with one pushed integer (`ebp`, `0x10076822`). The containing
  function is the **house goods-choice method**, documented in `population.md` under choice
  between offered goods: pefile gives `0x100c9a2c = 0x10076670`, slot +0xd4 of house vtable
  `0x100c9958`, installed at house base by `0x1007722e`. This is house-base `this`, not an
  adjusted camera interface. It obtains two values through argument0's +0x74 object's +0xac
  (`0x10076677..0x100766a2`), computes scores, tests stock via +0xa4 and eligibility via +0xc4,
  optionally writes a selected integer through argument1 (`0x1007682d..0x10076835`), and
  returns a score. Those fields and arithmetic do not establish pixel pitch, zoom or rotation.

**The confirmed nesting is** `0x10023c80 -> 0x10023180 -> 0x100230d0`, via calls
`0x10023c9f` and `0x1002318c`; there is an intermediate return address `0x10023191`.
`0x10023180..0x10023203` searches candidates and tests a footprint through virtual
+0x58/+0x60 then +0x54/+0x5c (`0x100231ac`, `0x100231bb`, `0x100231cc`, `0x100231db`),
returning an index or −1. This matches the already decoded building x/y/w/h slots in
`state.md` §4.8. The first two supplied addresses could therefore occur together during a
building lookup; their proximity to a hovered pair is insufficient to identify the preceding
screen-to-world computation. No direct nesting with the third address was established. Whether
these stack words were live return addresses, older stack contents, or frames from some other
operation remains open; static code alone cannot choose among those explanations.

**Virtual target limit.** The actual list-entry vtable at `0x10023161` was not recorded, so
there is no uniquely selected callee to decode as the converter. A checked example is the house
vtable `0x100c9958` +0x58 = `0x10071bc0`, which only returns u16 `[this+8]`
(`0x10071bc0..0x10071bc6`); it is the y getter, not a transform. That example does not prove
the live list entry was a house. Likewise, the supplier vtable for `0x10076823` is not fixed by
the house vtable: the receiver is argument0, not the house itself.

**Step-2 stop:** `0x10023161`, dynamic building getter +0x58. No screen→tile target, its
mouse arguments, output pointer, zoom-group table, rotation arithmetic or projection constants
were established from these addresses. No converter pseudo-code is justified. The constants 8
in the list bound and the house scoring constants are not projection constants.

**Copy check (read-only, not a live-module check).** The test file
`C:/Users/altin/anno1503/game/app/AnnoWelt.dll` and the driving copy
`C:/anno1503-original/AnnoWelt.dll` both hash to SHA-256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
A difference between these on-disk copies does not explain the mismatch. The module-base
arithmetic in the original note is also consistent: `0x02520000 + 0x23164 = 0x02543164`;
replacing that load base with the preferred base gives `0x10023164`. This validates the sum,
not the original classification of the stack word.


## 3. The camera (unanchored; T15.B1 stopped)

Arrow keys scroll the view (the manual's cursor keys); `P` opens a modal "Pause!" scroll with a
"Continue game" button at client (0.40, 0.69), during which the arrow keys do nothing and the
hovered tile is not updated (the first diffing pass was taken under that dialog and found only
dialog state). Unpaused, a 0.2 s hold of RIGHT then DOWN moved the hovered tile under a fixed
cursor from (672, 499) to (676, 511), so the scroll is not quantized to tiles per key press and a
"changed on RIGHT, unchanged while still, changed on DOWN" search over 182 MiB returned dozens of
candidates in the stack region `0x1a1xxx`–`0x1a9xxx` and UI state, none clean. **Owner's hint
(2026-10-25):** the white rectangle on the minimap (top right, `screens.json` `hud.minimap`) shows
the screen position; read it from a screenshot as the ground truth and search memory for values
that follow it (tile units or pixels), with the mouse parked outside the map. Not done in this
session (the disk filled with snapshots; §4).

**T15.B1 static outcome (2026-10-25).** Step 3 cannot follow a converter that step 1 did not
identify. Neither the list receiver in AnnoWelt `0x100230d0` nor the house receiver in
`0x10076670` is evidence of a camera object. No camera vtable, global/world anchor, scroll
units, zoom offset or rotation offset was found from the supplied chain. Exact unresolved
virtual sites: `0x10023161` (list entry +0x58) and `0x10076823` (supplier +0xc4); neither is
proven to be a conversion call. Per the task's unanchored branch, **no `camera(mem,bases)` reader
or test was added**, and no test skip output is claimed.

**Evidence needed to resume:** a validated writer/call stack for the hovered tile on a mouse
move, recording the module load base, live instruction address and dynamic receiver/vtable;
or the owner's minimap calibration described above to provide an independent camera anchor.
This is the remaining question, not a request to manufacture a reader from a stack address.
No new original-game session or large snapshot set was started for this static task.

## 4. Method notes

- `tools/orig/memdiff.py`: `snap` reads every committed writable region ≤ 64 MiB (≈ 182 MiB,
  0.1 s) to `cache/orig/<session>/mem/`; `find` narrows by `--changed a:b` (optionally
  `--maxdelta`), `--same a:b` and `--delta a:b:d`; `read` prints live values. Snapshots are large:
  delete them when mined (drive C: was at 1.5 GB free on 2026-10-25).
- Counters are the main false positives (several values step by 1 or 2 on every mouse event, e.g.
  `0xa57d48`, `0x2ee002c`, `0x39f87c`): a 1-px move that must leave the value unchanged removes
  them.
- The quick save (F10) writes `SaveGame\ANNOnymous\Lastgame.sav` (2.7 MB) within 3 s; F9 reloads it
  (the screenshots after both show the same scene). After a quick load the heap holds **two**
  player objects with the human's vtable, name and money (`0x2a1ea08` and `0x324dbe68`): one is
  the freed pre-load copy (`gamestate.players` lists both; prune by reachability from the world,
  `play-original.md` §2 item "freed objects keep their vtable").
- The in-game volumes come from the profile's `VOLUME` chunk (`tools/orig/userdat.py`,
  `docs/testing/original-wine.md` §2.2); set to (−250, −250) on this PC's copy on 2026-10-25 for
  the owner's "without sound" runs. There is no effects slider.
