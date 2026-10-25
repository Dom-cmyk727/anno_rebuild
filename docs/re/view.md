# The view: mouse, hovered tile and camera of the original

**Status:** live first pass plus static T15.B1/T15.B1b stops, 2026-10-25 (orchestrator, owner driving session; T15 play layer stage B,
**Status:** live first pass plus static T15.B1/T15.B1c findings, 2026-10-25 (orchestrator, owner driving session; T15 play layer stage B,
`docs/testing/play-original.md` §3 item 1). Method: read-only memory diffing of the running copy
(`tools/orig/memdiff.py`, snapshots of every committed writable region, narrowed by "changed on this
input, unchanged on that one"), live reads along controlled mouse sweeps, screenshots. Session
outputs in `cache/orig/drive-20261025*/` (git-ignored). Host: the owner's Windows 11 PC, the copy at
`C:\anno1503-original\` through dgVoodoo2, single-player Citizen continuous game, zoom and rotation
as the game starts (not changed in this session). Addresses in the live observations are **process addresses of this
run** unless written as `module + RVA`; static sections explicitly use preferred VAs; the two variables found live on thread stacks, so they are
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


### 2.3 The converter: T15.B1b outcome (2026-10-25)

**Open.** The arrow-key sweep in §3.1 did not select a scroll method or camera global.
Consequently there is no anchored field-loader list to filter for mouse inputs or two integer
arguments, and **no converter candidate is established by this route**. This is a dependency
stop before the task's two-function follow-up, not a failed decoding of a selected converter.
No projection constants, zoom-group pitch table, rotation formula or output pair is assigned
from the unrelated comparisons. The live slopes in §2.1 remain observations only; §2.2's
building getters remain rejected as converters. T15.B2's frame walk/minimap calibration is
still the prescribed independent route to obtain a live anchor.

### 2.3 The converter: bounded T15.B1c candidate list (2026-10-25)

**Not decoded.** The scroll fields and HWND anchor are established (§3.2–3.3), but the
screen→tile writer is not selected within the task's three-function follow-up. All addresses
in this list are preferred VAs in **AnnoFrame.dll**; none is assigned to View.dll.

1. `0x1000b3a0..0x1000b48b`, receiver V, reads two integer input coordinates, divides them
   by V+0x1eb8 and tests UI regions through the minimap object V+0x1658 and other UI objects
   (`0x1000b3bf`, `0x1000b410`, `0x1000b449`, `0x1000b476`). It returns a category1..5,
   not a tile pair. It is a mouse-coordinate consumer, not the selected converter.
2. `0x100041f0..0x10004220`, receiver S=V+0x4c, **does return an out-pointer u16 pair**:
   if `[S+8]` is non-null, call its dynamic slots+0x68/+0x6c at `0x10004203` and
   `0x10004212`, store low16 bits at out+0/+2 (`0x1000420a`, `0x10004215`), return1;
   otherwise return0. It takes no mouse coordinates and does no projection arithmetic.
   **Exact stop:** the dynamic receiver `[S+8] = [V+0x54]` is not identified; neither
   slot target nor the producer of those values was established. Do not equate this pair
   to the live stack pair in §2.1 solely because both contain two u16s.
3. `0x10071a50..0x10071c30`, receiver T=S+0x1424=V+0x1470, reached through the
   matrix refresh `0x100041a0 → 0x10071f00 → 0x10071a50`. It accepts a position pointer,
   rotation and scale, builds view geometry using viewport bounds, angle tables and
   four normalized vectors (loop `0x10071b2a..0x10071c1d`). It is not a selected
   two-pixel-input→tile-output function. T is value/matrix storage addressed by LEA,
   not a new vtable interface; `0x100041b9` establishes its displacement.

These three were read to their final returns; no fourth candidate body was followed.
The nearby **zoom selector** is D: group0/1/other → scale1/0.5/0.25 (§3.2). This is
not yet the pixel pitch per group. `0x10071d70` uses float59.16330337524414 from
`0x1008e1a0` to build its matrix (`0x10071dbe`); that constant alone is not a tile width
or height. Rotation is a byte0..3 in the named setter, but complete picking, terrain
intersection, rounding and final output ownership remain open. T15.B2 can now obtain
V from the game HWND and validate the fields while recording the live hovered-pair frame.

## 3. The camera (static fields known; live calibration open)

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

### 3.1 T15.B1b: literal virtual-key sweep and stop

**Method/scope (2026-10-25).** Read-only static inspection of the licensed files under
`game/app`; preferred image base `0x10000000` for all addresses below, **module names are
essential**. Scratch capstone scripts `cache/t15b1b_sweep.py` and
`cache/t15b1b_candidates.py` use pefile `.text`, x86-32 detail mode and skipdata, following
`tools/xref.py` / `tools/disasm.py`. Outputs remain ignored under cache; none are fixtures.

The sweep selects comparisons whose second immediate is `0x25..0x28`, checks pairs within
64 bytes, and looks for `sub reg,0x25` followed within 16 instructions by an indexed jump.
It also checks equivalent literal LEA/ADD minus 0x25 index formation. **P: 37 comparison
sites**, reproduced on a second run (12+3+7+5+10); zero literal minus 0x25 index sites and
therefore zero matching four-case-or-larger switches of that form. The full hit list is:

- **AnnoFrame.dll (12):** `0x10010cae`, `0x1001206d`, `0x100147d9`, `0x1003f29c`,
  `0x10049b2b`, `0x10049ca5`, `0x1004b468`, `0x1004b604`, `0x1006e52a`,
  `0x10072ec3`, `0x1007e819`, `0x1007f143`.
- **AnnoGame.dll (3):** `0x10005d6b`, `0x10013b6f`, `0x10013c2b`.
- **AnnoScene.dll (7):** `0x1000b179`, `0x1000b17d`, `0x1000b254`, `0x1000b2b4`,
  `0x10011b70`, `0x10022c84`, `0x10022ca4`.
- **AnnoDisplay.dll (5):** `0x10001b8f`, `0x10009942`, `0x1000a2b2`, `0x1002be99`,
  `0x1002bf4e`.
- **AnnoWelt.dll (10):** `0x10019d6e`, `0x1002a92b`, `0x1002aa6c`, `0x1002d815`,
  `0x1002d87e`, `0x1002dac9`, `0x1003d100`, `0x10064a94`, `0x10064ace`, `0x10080340`.

**P, table fallback:** a literal contiguous `25,26,27,28` sequence, separately as bytes and
little-endian u32s, has zero matches in `.data`/`.rdata` of each of the five modules.
This does **not** exclude a binding table with intervening action ids, another ordering,
a runtime-loaded table, scan codes, a wider switch range, or a handler in another module.
Linear-sweep instruction alignment/inline data is another search limitation. There is no
claim that the original does not handle arrow keys.

**Candidate reads (step 2).** Function starts were checked against padding, direct call
and relocation targets; inlined switch labels are not function boundaries. In particular,
AnnoFrame `0x10072ebc` is inside `0x10072bb0`; AnnoScene `0x10011b2a` is inside
`0x10011a10`; AnnoWelt `0x10019cb4` is inside `0x10019c20` (the small preceding
`0x10019c10` getter ends with a tail jump at `0x10019c1a`). For long bodies, read the
first 150 instructions plus the hit neighbourhoods, rather than claiming a whole-function
proof. Short bodies were read through their final ret, including branches after early rets.
The comparisons' dataflow was re-read before assigning the narrow findings below.

- **AnnoFrame:** `0x10010ca0..0x10010ce6` and `0x1006e510..0x1006e608` dispatch on
  argument0 with a second argument used as a value/output record. Their `0x26` cases write
  one flag or return one field; they do not establish four key-dependent scroll directions.
  `0x10011020` first 150 end at `0x100111f3`, with the later loop-bound neighbourhood
  `0x1001204d..0x10012093`; `0x10014410` first 150 end at `0x10014620`, plus
  `0x100147c2..0x100147ed`: their `0x28` compares bound incrementing loops, not four
  key cases. `0x10049950` first 150 through `0x10049b1f` and `0x1004b290` through
  `0x1004b45c`, plus the comparison windows through `0x10049b52` / `0x1004b489` and `0x10049c8d..0x10049ccf` / `0x1004b5ed..0x1004b630`:
  the `0x27` values cap coordinate-difference-derived counts at 39. These are not proven
  camera clamp fields. `0x1003f0e0` first 150 through `0x1003f27b`, plus
  `0x1003f289..0x1003f2b8`, tests a character in a string containing formatting markers;
  `0x25` is percent here. `0x1007e789` first 150 through `0x1007e92c`, plus
  `0x1007f12f..0x1007f162`, likewise parses characters, with percent and format-width/type
  handling. `0x10072bb0` first 150 through `0x10072d82`, plus
  `0x10072eab..0x10072ef1`: the nearby switch subtracts 3 at `0x10072ec0`, not 0x25;
  no four-arrow scroll target is selected from the inspected portion.
- **AnnoGame:** `0x10005b30` first 150 through `0x10005cda`, plus
  `0x10005d4d..0x10005d97`, increments the compared counter by 3 in a table/string loop.
  `0x10013b40..0x10013bd0` and `0x10013be0..0x10013ce7` return values from indexed
  tables across several numeric ranges; `0x28` is an upper range bound. Neither contains
  four direction-specific calls.
- **AnnoScene:** `0x1000b150..0x1000b1b4`, `0x1000b230..0x1000b277` and
  `0x1000b280..0x1000b2d7` scan/copy string tokens. `0x10022bf0` first 150 through
  `0x10022d86` counts opening/closing parentheses at `0x10022ca4..0x10022cad` and
  recursively processes their contents. Thus both close pairs
  (`0x1000b179/0x1000b17d`, `0x10022c84/0x10022ca4`) are character parsing,
  not cursor-key dispatch. `0x10011a10` first 150 through `0x10011baa` uses a switch
  based on argument−10 (`0x10011a18`); at `0x10011b70` the compared edi still derives
  from that adjusted argument. Its two-stage table is not evidence of a VK_UP comparison.
- **AnnoDisplay:** `0x10001b80..0x10001bf9`, `0x10009930..0x1000999c` and
  `0x1000a2a0..0x1000a310` have selector/value-record branches; the 0x26 case returns
  a field or constant through argument1+4. `0x1002be30` first 150 through
  `0x1002bfe7` compares a file/mapping size and header length with40 at
  `0x1002be99/0x1002bf4e`, not an arrow-key code.
- **AnnoWelt:** `0x10019c20` first 150 through `0x10019de7` bounds a value from a type
  getter in 26..40 at `0x10019d69..0x10019d71`. `0x1002a5e0` first 150 through
  `0x1002a7c4`, plus `0x1002a916..0x1002a954` and `0x1002aa58..0x1002aa96`,
  uses incrementing range counters. `0x1002d3b0` first 150 through `0x1002d5a7`,
  plus `0x1002d801..0x1002d834` / `0x1002d86b..0x1002d89d`, and the complete
  `0x1002dac0..0x1002daff`, use the26..40 numeric range for indexed quantities.
  `0x1003c6b0` first 150 through `0x1003c889`, plus
  `0x1003d0e8..0x1003d12d`, increments a byte and resets it at 40.
  `0x10064a90..0x10064abb` and `0x10064ac0..0x10064b21` are **two different
  methods**: their adjacent 0x26 sites are a get/set selector pair for a byte at relative
  this+0x100, not four keys in one handler. `0x1007f960` first 150 through
  `0x1007fb1c`, plus `0x10080326..0x10080362`, advances a stride 10 offset until40.

No receiver in this list is identified as the camera. Relative fields in rejected methods
must not be combined across receivers: their installed interface offsets were not resolved
because there is no selected scroll method. In particular, neither the AnnoFrame count cap39
nor AnnoWelt relative this+0x100 is promoted to a camera coordinate or global anchor.

**Stop/result (steps 2–5).** No candidate establishes the required four-key branch mapping,
and no key-code-to-action table reader is anchored. There is therefore **no verified scroll
method, camera object/global chain, coordinate units, map clamp, zoom field or rotation field**
from this pass. The static dependency chain stops at handler selection, with the explicit
rejected candidates above; it does not stop at an invented virtual camera call. No
`camera(mem,bases)` reader or skipping test was possible, so none was added and no skip output
is claimed. The sweep result is useful negative evidence for this literal-pattern route,
not completion of the camera task. Resume from T15.B2's live frame/camera anchor or a verified
key-binding/action dispatcher; no original-game input was sent in T15.B1b.
### 3.2 The scroll methods and the camera fields (T15.B1c, 2026-10-25)

**D unless P is stated; AnnoFrame.dll, preferred VA base0x10000000.** Addresses such as
`0x10006850` are preferred VAs; their RVAs omit0x10000000. Static inspection used pefile,
`tools/disasm.py` and `tools/xref.py`, with the relevant arithmetic re-read. No live camera
calibration was performed in this task. The earlier unanchored B1 outcome above is historical.

**Receiver identity first:** V is the **base** of tFrameGame or tFrameGameFull. Their
base vtables `0x10086428` / `0x1008664c` contain the pollers in slot+0x6c (entries
`0x10086494` / `0x100866b8`); constructors install them at `[V]` at
`0x100094e8` / `0x1000de5c`. Pollers preserve ecx in esi and call the scroll methods
with ecx=esi (`0x10005a1f`, `0x10005a9c`; `0x1000b09c`, `0x1000b128`).
All four leaf methods therefore have this=V. Their direct helper calls use **S=V+0x4c**,
whose vtable `0x10086360` is installed by `0x1000457a` after the parent constructor's
`lea ecx,[V+0x4c]` at `0x1000949d`. No class-Y interface displacement applies here.

**Polled keys and corrected step.** The GetAsyncKeyState IAT slot `0x10086208` is loaded
by `0x10005a10` and `0x1000b090`; the four keys are pushed as arguments, which explains
B1b's negative comparison search. Up/down select mask4/8, left/right mask1/2; the code
uses `test ax,ax` (not an isolated high-bit test), with up preferred to down and left to
right. Full starts from V+0x1e7c. **Two corrections to the round-as first note:**

- Full's test `0x1000b0d8..0x1000b0df` skips keyboard scrolling when V+0x16fc is
  **not**−1. A value−1 enables that block.
- `0x10005a79..0x10005a89` loads capped integer dt first, then zoom*10 and executes
  `fdivp st(1)`: **step=min(dt,50)/(zoom*10)**, not zoom*10/dt. Full repeats it at
  `0x1000b105..0x1000b115`. P: constants `0x1008641c=10.0f`,
  `0x10086418=0.1f`, `0x10086414=0.2f`. Horizontal argument is step*0.1,
  vertical argument step*0.2. V+0x15f4 is the **zoom scale**, not a separately proved
  user scroll-speed setting. Frame dt is milliseconds (the loop's timer, `tick-loop.md` §1).

**Four complete methods.** Each takes one f32 argument a (ret4). Paraphrase of the x87
arithmetic, with stores rounded to f32; no simulation rule is implemented:

```text
# this=V; C=(f32[V+1608], f32[V+1610]); H=(V+1614,V+1618); D=(V+161c,V+1620)
left(a):  C -= a*H       # 0x10006850..0x10006883; ret4 0x100068cc
right(a): C += a*H       # 0x100068d0..0x100068ff; ret4 0x10006948
up(a):    C -= a*D       # 0x10006950..0x10006983; ret4 0x100069cc
down(a):  C += a*D       # 0x100069d0..0x100069ff; ret4 0x10006a48
# After each pair of stores, the same ordered calls:
clamp(S)                # 0x10004020; call sites 6889/6905/6989/6a05 (prefix0x100)
minimap_update(V+1658, trunc(C.x), trunc(C.z))
                        # 0x10002990; calls 68ac/6928/69ac/6a28 after 0x10077da8
refresh_matrices(S)      # 0x100041a0; calls 68b3/692f/69b3/6a2f
publish_matrix(S, [V+1820]) # 0x10003be0; calls 68c1/693d/69c1/6a3d
byte[V+28] |= 0x10       # 68c6/6942/69c6/6a42
```

The truncation helper returns an integer towards zero. `0x10002990..0x100029cb` compares
the integer pair with **module-global caches** `0x10094a24/0x10094a28`, passes its negation
to the minimap-related object through `0x10033ad0`, then updates the caches. These are
last integer coordinates shared by that path, **not a global pointer to V** or a guarantee
of current float position. The first note's claim that the integer copy lives at V+0x1658
is superseded by this distinction.

**Fields, absolute and sub-object-relative:**

| Field at V | Type / meaning | Addressed evidence |
|---|---|---|
| Receiver V; S=V+0x4c | Base vtable vs embedded view object | Installers/call sites above; all following S offsets translated to V. |
| +0x1608 / +0x1610 (S+0x15bc/+0x15c4) | f32 x/z translation in **negative world-tile units** | Four stores above; S getter `0x10004170..0x1000418e` copies the three-float position. |
| +0x160c (S+0x15c0) | f32 vertical translation | Initialized0 at `0x10004580`; not changed by the four scroll leaves. |
| +0x1614/+0x1618 (S+0x15c8/+0x15cc) | f32 horizontal direction vector | Used by left/right; rotation cases below supply±1. |
| +0x161c/+0x1620 (S+0x15d0/+0x15d4) | f32 vertical-screen direction vector | Used by up/down; rotation cases supply±1. |
| +0x58 (S+0xc) | bound world pointer W; its dimensions supply clamp limits | Bind `0x10008ea9 → 0x10003b8b`; clamp calls+0x54/+0x58 at `0x1000406f/0x100040ae`. |
| +0x15f0 (S+0x15a4) | u8 rotation | Getter `0x100044e0`; T+0x180 written by `0x10071ed9`; setter `0x10003d20`. |
| +0x15f4 (S+0x15a8) | f32 zoom scale | Passed to T in `0x100041a3..0x100041c3`, stored at T+0x184 by `0x10071edc`; poller divides by scale*10. |
| +0x1604 (S+0x15b8) | u8 zoom-group selector | `0x10003c30..0x10003c66`:0→1.0,1→0.5,other→0.25; group stored at `0x10003ca3`. |
| +0x1624 (S+0x15d8) | u32 view dirty flags | Matrix refresh OR0x20 at `0x100041cf..0x100041d9`; zoom/rotation setters OR0x60. |
| +0x16ec | u32 frame/update mask, not zoom | Init0xfb7f at `0x10009518`, Full0x893f at `0x1000de62`; ORed with derived flags at `0x10005b02..0x10005b0a` / `0x1000b19f..0x1000b1a7`. |
| +0x16fc | i32−1 sentinel or u32 GetTickCount stamp; gates scrolling | Init−1 at `0x10009560/0x1000de51`; timestamp store `0x1000cd10`, elapsed comparison `0x1000ca96..0x1000cacb`; precise UI-mode name not assigned. |
| +0x1e7c (Full only) | u32 pre-set edge-scroll direction mask | Zero at `0x1000de36`; mouse-edge tests/set/clear at `0x1000b54c..0x1000b5b2`; consumed at `0x1000b09e`. |
| +0x4c | Embedded view object S | Constructor `0x10004530..0x100045c4`; both post-scroll functions run with this=S. |

**Clamp and units (D).** `0x10004020..0x100040e0` clamps C.x to[-W.width,0] and
C.z to[-W.height,0], comparing **negated** translation to0 and to W's dimension getters.
At equality the bound may be stored again; the result is the same. W's slots+0x54/+0x58
are AnnoWelt `0x100a3910/0x100a3920`, returning world+0xec/+0xf0. World x/z use one
unit per tile (`annowelt.md`, terrain vertices, `0x1009b920`, constant `0x100cb840=1.0`).
No viewport width/height is subtracted by this clamp. This establishes tile-space translation,
not screen-pixel or fixed-point coordinates; the corresponding positive world x/z location is the
negation of the stored translation. Their exact relation to the observed minimap rectangle awaits B2.

**Rotation and zoom.** Setter `0x10003d20..0x10003ff6` includes cold case bodies beyond
its first return. Jump table `0x1000400c` maps new rotations0/1/2/3 to
`0x10003f77/0x10003f31/0x10003e50/0x10003fb9`. Respectively (H,D) are
`((1,-1),(-1,-1))`, `((1,1),(1,-1))`, `((-1,1),(1,1))`,
`((-1,-1),(-1,1))`. The setter also adjusts translation around a height-dependent pivot;
this is not a complete picking formula. Zoom setter `0x10003c30..0x10003d1a` and matrix
builder `0x10071d70..0x10071ef8` establish scale/group and rotation fields; full per-group
pixel pitch remains open (§2.3). T=S+0x1424=V+0x1470 is matrix/value storage; direct LEA
at `0x100041b9` establishes this address, not another virtual interface.

**The two post-scroll functions, read to final return (step 3).**

- `0x100042d0..0x100042ec`, this=S, returns
  `(flags&0x20 ? 0x20000 : 0) | (flags&0x40 ? 0x10000 : 0)` from S+0x15d8
  (=V+0x1624). It converts dirty bits; it does not calculate a visible tile window.
- `0x10004340..0x100044ba`, this=S, is a **temporary position-jitter refresh**.
  S+0x15fc (=V+0x1648) enables it. It accumulates dt at S+0x1600 (=V+0x164c),
  waits until more than50ms after S+0x1604 (=V+0x1650), and requires the negated x/z
  position inside the integer rectangle S+0x15ec..0x15f8 (=V+0x1638..0x1644).
  It computes sine(time*0.004)*amplitude (Full passes0.8f at `0x1000b188`), scales two
  independent rand()%101 values by0.01 and adds those offsets to x/z
  (`0x100043ea..0x10004456`). It saves all three position floats, clamps and refreshes
  the temporary position, then **restores the original position** (`0x10004457..0x100044b0`).
  The refreshed matrix can retain the visual jitter. This is not the persistent scroll step
  or the screen→tile writer, and the identically numbered fields relative to S are not
  the parent V's zoom fields.

### 3.3 The view object and its anchor (T15.B1c)

**D, AnnoFrame.dll.** The poller has no direct caller found by the instruction xref; it is
slot+0x6c of the installed base vtable. The loop `0x100087f0` invokes that slot at `0x100088d0` on its
unchanged V receiver (the Full loop wrapper `0x100096e0..0x1000976b` calls it at
`0x1000975f`). Construction is **CreateAnnoFrameGameInstance** `0x10066290..0x1006632a`:
kind1 allocates0x1ec0 bytes and calls Full constructor `0x1000dd70` (which calls base
`0x10009470` at `0x1000dd8f`); kind0/2 allocates0x1898 bytes and uses the base constructor.
The factory returns V through its fourth stack argument at `0x10066325` (cdecl; no ret-pop).

Checked callers: campaign startup `0x100201f3..0x10020203` passes its own+0x88 field as
that out-pointer; normal startup `0x1002a37b..0x1002a398` first receives it in a local,
then invokes the object's virtual methods. These are concrete ownership paths, but neither
has been traced to a persistent module-global pointer in this task. Constructor writes
must not be mistaken for globals holding an instance.

**Usable runtime anchor:** `0x100087c0..0x100087de`, called with ecx=V by the loop at
`0x100087fe`, takes the game HWND and does SetWindowLongA(hwnd,−21,V) at `0x100087d0`,
then installs window procedure `0x10007f20` via index−4 at `0x100087da`. The IAT entries
`0x10086238/0x1008623c` name SetWindowLongA/GetWindowLongA (P, import table). The installed
window procedure retrieves V with GetWindowLongA(hwnd,−21) at `0x10007f2b`.
Thus the chain is:

```text
game HWND → GWL_USERDATA(-21) → V
[V] must be AnnoFrame base+0x86428 or base+0x8664c
V+0x4c → embedded S; V+0x1608/+0x1610 → f32 negative tile-space x/z
V+0x15f0 → u8 rotation; V+0x15f4 → f32 scale; V+0x1604 → u8 group
```

This is a **window anchor**, not `module+RVA → pointer → V`. A live HWND and validation
of the installed vtable are needed; no process address from the old driving session is
hardcoded. The small global integer caches in §3.2 cannot recover the object pointer,
rotation or zoom.

**Reader condition (step 5):** no `camera(mem,bases)` or skipping test was added, because
the specified module-global-to-object anchor was not established. A reader accepting an
explicit V, or a backend helper using the HWND's userdata, could use the decoded fields;
that API is left to the already planned T15.B2 calibration. No reader skip line or live
camera validation is claimed. No original-game input was sent in this static task.

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
