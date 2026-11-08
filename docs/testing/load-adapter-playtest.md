# The developer Load adapter: playtest card

For the owner's personal playtest of W4-EXPAND-LIVE-1's Load key (`task/W4-expand-live-1`).
**Status: automated completion shown on this build; your own playtest is NOT_RUN.** This is a
developer adapter posting the sim's existing `LoadCmd` — it is **not** the original's warehouse
trade panel (UI.1 F2.2, OQ-547), and the window says so.

## What it is

The same ship-start opening as [`ship-opening-playtest.md`](ship-opening-playtest.md) (candidate A,
`data/maps/candidates/ship-start-v2/lan5-ship-a.json`, seat 0, no city at launch), with one new key:
`L` takes cargo from a warehouse back aboard a ship, mirroring `U` (Unload) in reverse.

## Launch

Same as the ship-opening card: `anno1503.exe --ship-start <manifest.json> --seat 0 --speed 200
--game-dir <game folder>`.

## Controls (new: `L`; everything else is unchanged from the ship-opening card)

| Do | Effect |
|---|---|
| `L` | load mode: `Tab` next hold (every hold, including an empty one — unlike `U`, which only visits holds that already carry something), `-` / `+` (or `[` `]`, `Page Down` / `Page Up`) quantity -1/+1 ton (`Shift`: 10), `Enter` sends exactly that hold and quantity from your nearest warehouse (left click one of your warehouses to choose it as the source); the default request is 1 t |

The title's advisory line works the same way as Unload's: `hold [n] <goods> request <k> t: <advisory>`.
For an empty hold, the goods shown is the source warehouse's most-stocked real good, not a developer
choice — there is no key yet to pick a different one.

## A walk through (seat 0)

1. Follow the ship-opening card's steps 1-3 (sail to `(257,134)`, found at `(255,138)`): a warehouse
   now stands with zero stock, and hold 0 already dropped from 50 t to 38 t (founding paid 12 t of
   it away).
2. `U`, `Enter`: unload the remaining 38 t of hold 0 into the warehouse. The title says
   `store 42: 9728` (38 t raw units) and `ship.1.hold0` goes to 0.
3. `L`: the title switches to `mode LOAD`, request defaults to 1 t.
4. `]` `]` (or `Shift+]` once): request becomes 3 t.
5. `Enter`: `load sent: 3 t of HOLZ`, then `load #n: accepted: moved 3 t of 3 requested; hold 0 ->
   768`. The warehouse's store drops to 8960 (35 t).
6. `Tab`: the hold cycles even though every other hold is still empty — Load, unlike Unload, can
   fill any hold, not only one that already carries something.
7. `Esc`: loading cancelled, back to `mode select`, nothing sent.

## Known limits (say so rather than guess)

- A developer adapter, like the rest of this card's keys: no text overlay beyond the title bar, no
  sounds, no goods-picker UI. Picking which goods to load into an **empty** hold is the source
  warehouse's single most-stocked real good (automatic, not a developer choice); topping up a hold
  that already carries cargo keeps that cargo's own goods.
- The UI does not know a ship's per-hold capacity (not published in `OpeningView` yet — flagged as a
  gap in the D1/D3 report, not fixed here): the advisory bounds the move by the warehouse's stock
  only, never by how much room the hold actually has. The sim is authoritative either way; a request
  the hold cannot take is capped by the sim's own result, same as any other command here.
- Everything the ship-opening card's "Known limits" section says still applies unchanged (no network,
  no trade routes, no AI, saves go to `saves\`, the window uses the RTX 4090, etc.).
- Verified here: `viewer_wiring_expand_live_load` (the scripted session) on this build. Not verified:
  a live, hands-on playtest by the owner.
