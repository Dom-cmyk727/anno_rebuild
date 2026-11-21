# Review: UI-STATUS-1 stages B and C (the status bar for the tile under the cursor)

Reviewer: the orchestrator, 2026-11-17. Branch `task/UI-STATUS-1`, head `0ee20000`:
- stage B's code at `9e189e28`;
- after it, docs only: stage C and its corrections, with OQ-701/702 answered, 704-706 answered with open parts,
  and 707 new.

Packet: `tasks/ui-status-1.md` on that branch.

**Verdict: ACCEPTED once CI is green on `0ee20000`.** It then lands under the owner's standing rule.

## The owner's decisions it implements

- The go, "Yes, write it", in the UI lane's session.
- **OQ-703** "Like original" on all three points:
  - the city's territory, not the island;
  - fertility over every discovered island;
  - a foreign city's residents when that city knows the player.
- The original's rule replaces UI-WINDOW-1's "Always show my city" (OQ-622), with no rules option. That
  follows the owner's "like original".

## What was checked (code at `9e189e28`)

- **`status_bar.hpp/.cpp`:** a pure decision over the published `TerritoryView` and the cursor tile. Every rule
  carries its AnnoFrame address:
  - the counters by territory owner, with the armed building's corner rule;
  - the fertility segments from the climate mask, skipping the always-set bits `0xF009` and segment 7, up to 4
    slots;
  - the 2000 ms hide delay on the window's clock, never sim time.
- **The rule's numbers are data:** `StatusBarRule` holds them with the original values as defaults.
- **`PlayRunner::publish`:**
  - read-only: it publishes the cities whose figures can show (own cities, and foreign ones whose
    `player_mask` holds the local player; goods only for own cities);
  - the money now publishes without a city (gold before founding, as GOG's `hud_idle_running` shows);
  - nothing in `sim` changes.
- **`collect_territory_view`** adds the island index, climate and `discovered_by`, which is `players_present`
  plus the cities' owners, sorted and unique.
- **Tests:** `status_bar_test.cpp` covers the rules without a window. The wiring script `ui_status_island`
  covers the live path. The gate passed 144/144, plus 59/59 for the gold fix.

**The bars stay a named placeholder** until ISLAND-CROPS-1 gives the crop list (OQ-702).

## Evidence

- CI green on `9e189e28` (run 44054287950). CI on `0ee20000` is running (44062827710).
- **The owner's tests:**
  - `ui_7.mp4`, on `9e189e28` directly: gold only on a ship start, which the owner accepts ("you have to settle
    on an island ...");
  - `ui_8.mp4`, on the combined local build: the same behaviour.
- **The History Edition's own behaviour** in `he_01.mp4` (0:50-1:36) agrees: three crop icons over a
  discovered island, and nothing over another island.
