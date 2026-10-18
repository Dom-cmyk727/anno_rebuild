# Simulation: goods production

Spec for `src/sim/include/anno/sim/production.hpp`. Evidence: [`docs/re/production.md`](../re/production.md) (static analysis; D = disassembly, P = game files). Units: stocks in 8.8 fixed point (256 = 1 t), time in world ticks `t` (2 ms; `docs/sim/core.md`). Integer only, apart from the one-time load conversion below.

## Building types (`production_type`)
Each `Anlagen.dat` `ANLAGEN` entry with a 68-byte `PROD3` record yields a `ProductionType` (921 in the GOG files):

| Field | Source |
|---|---|
| `prodtype`, `kind` | `PRODTYPE` string; class by name as the factory `0x10024010` (§2.1, §3): workshop `HANDWERK`, two-output workshop `HANDWERKWAFFEN`, farm `PLANTAGE`/`WEIDETIER`/`STEINMETZ`/`SCHAFFARM`/`FISCHER`/`PELZJAEGER`, `MINE`, small farm `UNIVERSAL`, market `HAUPT`/`MARKT`/`KONTOR`/`MARKTKOMBI`, growing items `ROHSTWACHS`/`ROHSTDUERR`; everything else `Other` |
| `outputs`, `inputs` | `WARE` / `ROHSTOFF`: NUL-terminated goods names up to the last one that is not `UNUSED` (the original's input count, type `+0x1fb`) |
| `capacity[i]` | `PROD3[4 + 2i] << 8` |
| `time0_128`, `time1_128` | `__ftol(T · 128)` (32-bit store) |
| `output_per_step[i]`, `input_per_step[i]`, display rates | see "Load-time conversion" (16-bit stores) |
| `figure_type_count[g]`, `max_figures[g]` | `PROD3[0x28 + 3g]`, `PROD3[0x28 + 3g + 1]` (getters `+0xbc`, `+0xb8` read type `+0x1e2 + 3g` and `+0x1e0 + 3g`, D: `AnnoGame.dll` `0x1000a4b0`, `0x1000a490`; loader `0x10009549`–`0x1000958e`) |
| `figure_ids` | 9 × u16 at `PROD3[0x32]` |
| `stages`, `stage_shift` | `HEAD[0x10]` (`S`), `HEAD[0x18]` (`k`) |
| `growth_period` | growing items with `S ≥ 2`: `time0_128 · 1000 / (2^k · S · 128)` in u32, stored as u16 (five `ROHSTDUERR` entries: 66,000 → 464); then, if not 0, display rate 0 = `15 360 000 / (2^k · S · P)` (signed) (D: `0x100088b0`, `0x100096ad`–`0x100096db`) |

### Load-time conversion
The only floats: `T0` (`PROD3[0x08]`), `T1` (`[0x0c]`, replaced by `T0` when 0.0), `I0` (`[0x10]`), `I1` (`[0x14]`), display rates (`[0x20]`, `[0x24]`). The original's x87 sequence (D: `AnnoGame.dll` `0x10009627`–`0x10009717`, read 2026-10-18):
1. `q0 = 256 / T0`, kept on the FPU stack; `rate0 = __ftol(q0 + 0.5)`.
2. `q1 = 256 / T1`, also stored to an **f32**; `rate1 = __ftol(q1 + 0.5)`.
3. display rates: `__ftol(d · 256)`.
4. `need0 = __ftol(q0 · I0)` with the unrounded `q0`; `need1 = __ftol(f32(q1) · I1)` with the f32 copy.

`__ftol` (`0x10016d60`) is a 64-bit `fistp` with truncation returning the low 32 bits; non-finite values give the indefinite `0x8000000000000000`, i.e. 0, so `T = 0` gives 0. A `T1` of 0.0 or NaN is replaced by `T0` (`fcomps` sets C3 for equal and unordered). The code computes this in `double`, which equals the x87 at 53-bit precision control (MSVC's default). Checked by exact rational emulation over all 921 entries: 53- and 64-bit precision give the same integers everywhere. 24-bit precision (which Direct3D can set) differs in two values only: `need1` of entry 1010 (`KANONENBAU`, 6 → 7) and of 3979 (`PIONIER`, 3 → 4). Which precision is in effect when the original loads `Anlagen.dat` is not traced (OQ-032). The corpus test asserts the 53-bit values and the fixture-F1 spot values of the RE note.

## Production step
Every building tick starts with the **step gate**: if `(u16)(t − last) < 500` nothing happens; otherwise `last += 500` (16-bit), so one step per 500 ticks and late ticks catch up one step at a time.

**Workshop** (`HANDWERK`, `workshop_step`; D: `0x100680b0`). In both status paths the "blocked" flag is set and the city notified on the transition; production clears it.
1. If not switched on: stop.
2. `activity ← activity · 127 >> 7`.
3. If suspended (flags bits 4/5): stop.
4. `ratio = min over inputs of (in[i] << 7) / need[i]`, starting at 128. Below 64 with inputs present: status "missing input". Else, if the output is at capacity: "storage full".
5. Otherwise produce:
   - `made = rate0 · ratio >> 7`;
   - output and `activity` grow by `made` and `made << 3`;
   - each input loses `need[i] · ratio >> 7`;
   - the status and the blocked flag are cleared.
6. Then a carrier is due if `carriers_out < max_figures[0]` and the workshop has inputs.
Fixtures F2–F5 and F7 are tests.

**Farm** (`farm_step`; D: `0x1006ea90`):
1. Gate, switch and suspension as for the workshop; `activity ← activity · 255 >> 8`.
2. A single-worker farm whose worker is out does not produce.
3. Otherwise check the capacity first, then the input — the opposite of the workshop. There is one input, the harvested raw stock; `activity += made << 4` (a u16, which wraps as in the original); "missing input" has an interval of 60,000 ticks.
4. A worker is sent if more than one may go out, or nothing was made, and `workers_out < max_figures[0]` and `384 / rate0 ≥ raw / need0`. Fixture F6.

**Status messages** (`StatusTracker`, D: `0x10072ed0`, `0x10072fb0`):
- A lower code is ignored; a higher one starts the u16 status time.
- A message is due once the condition has lasted the interval (45,000 ticks for the workshop and a full farm, 60,000 for a farm missing input, 30,000 for a mine). The time then becomes `t − interval`, so the message stays due on every later step.
- Production clears codes 1–3 only if one of their icons is shown.
- Not modelled: the checks `0x10071f40` / `0x10072390` that can turn a due message into code 3 (meaning open).

**Not implemented yet:**
- `HANDWERKDUAL` (two outputs, `0x10068a70`; split rule only "likely");
- `HANDWERKWAFFEN` (`0x10069550`);
- `PELZJAEGER` (own tick `0x1006e5d0`);
- mines (deposit rules not traced);
- the small farm, markets and carts, transport.

Harvest amounts are open.

### Deviations (the guards never trigger with the original data)
- An input `need` of 0 limits nothing (the original would divide by zero). The corpus test confirms that no workshop input of the GOG data has a need of 0.
- A farm with `rate0` or `need0` = 0 sends no worker (the dispatch rule would divide by zero there).
- Field widths: stocks and the workshop activity are 32-bit. This is safe: the workshop activity stays ≤ 1,024 × 51 = 52,224. The farm activity and the step and status timers stay 16-bit, because their wraps are part of the original's behaviour.
- `CityStock::remove` refuses goods numbers beyond the stock list; the original remover has no check.
- `figure_ids` is the raw list from the file. The original orders it by each group's start byte (`PROD3[0x2a + 3g]`), which gives the same order for all 921 entries.

## Growth (`grow`; D: `0x1008b5f0`)
On each chunk visit of a growing tile (at most every 2,000 ticks, `docs/sim/core.md`):
- If `parity = (t / (P/2)) & 1` differs from phase bit 6: store it, and `counter = (counter + 1) & 63`.
- When `counter` is a multiple of `2^k` and at least `S << k`: the object becomes the next id and the counter is reset.

Then the phase is cleared except bit 7 (`andb $0x80`, `0x1008b6ca`). Grain field 712 → 713 after 20 flips of `t / 7750` (fixture F8, 155,000 ticks). A flip is lost if two happen between visits, and that happens in the original data: entries 356–364, 706 and four more, 754, 354 and 756 have `P/2` < 2,000 (smallest 125).

## Warehouse (`CityStock`; D: `0x1002da50`, `0x1002da90`)
One stock per goods number (0–0x37 in the original; the count is data), one capacity for all (savegame `HEAD2` u16 at 0x24 in tons, 0 → 50 t). Adding is clipped so that the stock does not exceed the capacity; removing is clipped at the stock. Fixture F10.
