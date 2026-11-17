# UI-GHOST-1: the placement ghost as the original shows it

**Released 2026-11-17** on the owner's go, relayed by the orchestrator ("Yes" to UI-INFO-2d, UI-GHOST-1, UI-BUILDCAT-1, in
that order). Branch `task/UI-GHOST-1`, base origin/main `4dfdd9b7`, worktree `E:\anno1503-worktrees\anno1503-UI-GHOST-1`.
Stage A (docs only, about 1 h) may run now. Stage B (code, about 2 h) starts after UI-INFO-2d and A's review.

**Reference:** `docs/BUILDINGSOBJECTS.md` on `task/BUILDINGS-REF-1` (`cc2c6e80`), quick answer 4 and part B §5 (cost
and the ghost), part C §2. It is the owner's standing reference for building work; read it first, and update it.

## Why

- The owner's test `ui_9.mp4` (2026-11-17; report `E:\1503_test_videos\ui_9_work\ui_9-report.md`): the rebuild shows no
  ghost where a building can't be placed or paid for, while the original shows feedback.
- HE shows the ghost appearing and disappearing for lack of resources (`ui_9` 4:08-4:25; `he_01` around 4:00-4:11).
- The rebuild's window check covers bounds, occupancy and cost only, and always draws (BUILDINGSOBJECTS gap 7).

What is known (part B, D unless marked):
- **A refused site:** `tCursorAnlageDefault`'s draw `0x10048450` draws a yellow footprint outline (`0x10041050`,
  colour `0xffff00`) and no ghost, when the site byte `B+0x2e4` (from the input handler `0x10047470`) is 0.
- **A valid ghost:** the building, the action-radius outline (`0x100443c0`, yellow in GOG, green in HE; follow GOG), and,
  for crop-input types, the fertility bar (UI-PRODBAR-1).
- **Unaffordable:**
  - The preview ignores cost (flag `0x2000000`), and the click checks it (City::Build `0x100275d0`).
  - An unaffordable ghost goes into a separate chunk mask (`+0x218`). The chunk draw `0x1009e1d0` shows the real
    ground whenever a draw-context byte (`+0x140c`) is set, so the ghost blinks. HE measures about 0.75 s on and 0.5 s
    off. The writer of `+0x140c` is unread (OQ-730).
  - Road and wall drags show the affordable part normally and the rest blinking.

## Stage A: settle what B needs (docs only, about 1 h)

1. **The blink:**
   - Find the writer of `+0x140c` and its period (OQ-730). Search AnnoDisplay and View too.
   - If it can't be found, take HE's measured period as a labelled adaptation and leave OQ-730 open.
2. **The yellow footprint:** its exact shape (the footprint outline at ground level? per rotation?), its line width,
   and whether it shows over water or off-island (`f13_place_water`).
3. **The radius outline:** which types draw it (radius HEAD `[0x11]` > 0?), its shape, and its yellow colour.
4. **When the ghost hides entirely:** the pointer over chrome (OQ-700).
5. Write it into `docs/BUILDINGSOBJECTS.md` part B §5 if that file is on your branch (merge `origin/task/BUILDINGS-REF-1`
   first; it is docs-only), else into `docs/re/ui.md` §4.
6. OQ ids: OQ-756..758 (corrected by the UI lane on 2026-11-17; OQ-754 belongs to another lane); ask the UI lane for more.
7. Report, then stop. B starts on the UI lane's go.

## Stage B: draw it (code, about 2 h; after UI-INFO-2d)

- The window's ghost follows A:
  - a refused site gives the yellow footprint outline and no building;
  - a valid site gives the building plus the yellow radius outline;
  - unaffordable gives the building blinking with A's period, on the window's clock only, never sim state.
- The placement test the window uses must be the sim's rule (`placement_rule` plus territory, read-only), not the
  window's partial check. Expose a read-only query if needed. No sim rule changes: if one is needed, report it.
- Facts for scripts: `tool.ghost` = `valid`, `refused`, `unaffordable` or `hidden`.
- Tests: a unit test for the state choice; a wiring script that arms a building over water, over its own territory, and
  with an empty stock (dev command), reading `tool.ghost`; a pixel check of the outline colour. Failure proof.
- Gate as UI-INFO-2d's packet describes, plus the `construction` and `selection` app cases.
- The owner's viewer test, with **default rules** (the developer city; NOT `--rules living`): arm a house, move it over
  water, then over land; empty the stock with a dev command and watch it blink.
