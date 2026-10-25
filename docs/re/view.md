# The view: mouse, hovered tile and camera of the original (observed by running)

**Status:** first pass, 2026-10-25 (orchestrator, owner driving session; T15 play layer stage B,
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

## 2. The hovered tile (D, by running)

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
first scan were transient buffers). **Next (static, executor):** disassemble the function that
returns into `AnnoWelt + 0x23164` and its callee: the projection constants (tile width and height
per zoom group), the camera fields it reads, and the object it stores the result in, if any.

## 3. The camera (open)

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
