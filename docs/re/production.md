# Goods production and transport (`AnnoGame.dll`, `AnnoWelt.dll`)

**Status:** first pass (2026-10-17), static analysis only. Modules: `game/app/AnnoGame.dll` (building/figure type data, data-file loaders) and `game/app/AnnoWelt.dll` (world, islands, cities, buildings, figures), GOG 2.0.0.5, image base `0x10000000` in both. Tools: Ghidra 12.1.4 headless decompile (`cache/ghidra/plus/*.c`), `objdump -d` for every formula quoted here, data cross-checks against `Anlagen.dat`, `FigurenHeader.dat`, `Texte.dat`, `Data/*.dat` and one `.sze`. No code is copied; behaviour is paraphrased, pseudocode is ours. Addresses are virtual addresses in the named module.

Confidence: **confirmed** = read in the disassembly/decompile (and, where marked P, checked against the game files); **likely** = strong inference, not traced end to end; **guess** = hypothesis, also listed under "Open".

Notation: "8.8" = unsigned fixed point with 256 = 1 ton. "Step" = one production update of a building (see §1). Slot `+0xNN` = virtual-method offset in an object's vtable. All runtime arithmetic described in §3–§6 is integer unless the text says float.

## 1. Time base and scheduling

- **World time.** The world's tick (`AnnoWelt` `0x100ae040`, world vtable `0x100ccdd8` slot `+0x84`; `0x100ccd74` is the unit registry, not the world, and `+0xe8` was a stale reading, corrected 2026-10-18) adds its argument `dt` to a 64-bit accumulator (`+0x100/+0x104`) and uses **`t = accumulator >> 1`** (32 bits, `+0x10c`) as the time for everything below. **Confirmed.** `dt` comes from `AnnoFrame`; its frame tick `0x1000ca90` sums the same kind of `dt` against `599999` and compares `GetTickCount()` sums, i.e. `dt` is in milliseconds at normal speed. So **one `t` unit = 2 ms** — **likely** (the path from the frame tick to world slot `+0xe8` and any game-speed scaling are not traced). The world's slot `+0x84` (not `+0xe8`) is the entry point.
- **Order inside the world tick** (`0x100ae040`): all islands (`0x100a2e60` → island slot `+0xe4` = `0x1008b760`), then `0x100add90`, then three sub-systems at `+0x114` slot `+0x20`, `+0x118` slot `+0x24`, `+0x69c8` slot `+0x14`, then `0x1009d2a0`, `0x100ae140`. **Confirmed** (what the sub-systems are is not traced).
- **Island tick** `0x1008b760`: walks its 8 × 8-tile chunks round-robin so that every chunk is visited once per **2000** `t` (growth, §4.1) and, in a second pass, once per **15000** `t` (island slot `+0x194`, not traced); then the city ring. **Confirmed.**
- **City ring** `0x100969d0`: 16 city slots, round-robin, each city ticked about once per **50** `t` (count per call = `dt·16/50 + 1`, max 16). **Confirmed.** City tick = city vtable `0x100c2f40` slot `+0x50` = `0x1002d9a0`; first thing it does is the building scheduler.
- **Building scheduler** `0x10023a50` (city `+0x144`): five building lists (`+0x88`, `+0xa8`, `+0xc8`, `+0xe8`, `+0x108`) processed by `0x10023810`, `0x100238a0`, `0x10023780`, `0x100239c0`, `0x10023930`; each call ticks `dt·n/500 + 1` buildings of a list (capped at `n`), continuing from a stored cursor, calling building slot `+0x8c(t)`. **Confirmed.** Buildings are put into a list by PRODTYPE group (`(PRODTYPE − 1) & ~0x1f`: 0x00 production, 0x20 market/Kontor, 0x40 public, 0x60 houses, 0x80 military; `0x10024010`) — **likely** for the list ↔ group assignment (two of the five branches read).
- **Per-building gate.** Every production tick starts with: if `(u16)(t − last) < 500` return; else `last += 500` (`last` = u16 at building `+0x0a`). **Confirmed** in all ticks below. So **one step per 500 `t`** (= **1 s**, likely); `last` advances by exactly 500, not to `t`.
- Corroboration for "1 step = 1 s": the productivity getters multiply by 60 (per minute) (`0x100684e0`, `0x1006ef30`), and the `PROD3` per-minute display value equals `60 / T0` for most buildings (mill `T0` = 20 → 3.0 t/min, bakery 10 → 6.0) (P). **Likely.**

## 2. Building type data (`Anlagen.dat` → `AnnoGame` type object)

`AnnoGame` `0x10009240` (type vtable `0x1002185c`, slot `+0x70`) reads the `ANLAGEN/ENTRY` records into one type object per entry; other modules read it through getters (§2.4). **Confirmed.** Name lookups go through the game object's slot `+0x48(kind, &{index, name})` (`0x100033c0`): kind `0x1e` = PRODTYPE names (`0x100138e0`), `0x20` = goods names (`0x10013be0`, §7), `0x21` = INFRA stages (`0x10013d60`), `0x38` = research (`FORSCHTYPE`, `0x10013cf0`). **Confirmed.** Records `ADDON`/`CLASSIC` are containers (recursed / skipped).

### 2.1 `PRODTYPE` (string) → `+0x10` (u16)
Name looked up in the kind-`0x1e` table (1–416). Values used by production (P: entry ids from `Anlagen.dat`):

| No. | Name | Runtime class (§3) | Example entries |
|---|---|---|---|
| 1 | `HANDWERK` | workshop | 1001 weaver, 1002 mill, 1004 bakery, 1017 ore smelter, 1019 smith |
| 2 | `PLANTAGE` | farm | 801 forester, 811 grain farm, 812–823 plantations |
| 3 | `MINE` | mine | 1203, 1205, 1206, 1210, 1212, 1214, 1216, 1217 |
| 4, 8, 10 | `WEIDETIER`, `STEINMETZ`, `SCHAFFARM` | farm | 3011…, 803/804, 807 |
| 5 | `JAGDHAUS` | own class | 805 |
| 6 | `FISCHER` | farm (same tick `0x1006ea90`) | 810 |
| 16 | `PELZJAEGER` | own tick `0x1006e5d0` (vtable `0x100c2bf0`; corrected 2026-10-18) | 824 |
| 9 | `UNIVERSAL` | two-line farm | 809 small farm |
| 13, 14 | `STEINBRUCH`, `EISLOCH` | base (no own tick) | 1201, 1208, 1218, 1220 |
| 12 | `HANDWERKDUAL` | two-output workshop (split byte) | none (no entry of the GOG `Anlagen.dat` names it, P) |
| 17 | `HANDWERKWAFFEN` | two-line weapons workshop | 1006 bows/crossbows, 1012 muskets/ship cannon, 1036 axes/lances |
| 33–36 | `HAUPT`, `MARKT`, `KONTOR`, `MARKTKOMBI` | market/Kontor | 1301, 1302 |
| 225–227 | `ROHSTOFF`, `ROHSTWACHS`, `ROHSTDUERR` | scenery items (no building) | 712 grain field (growing), 713 ripe grain, 714/715 dry field, 104/105 forest |

Table, numbering and classes **confirmed** (name table read from `AnnoGame` data; class switch in `AnnoWelt` `0x10024010`).

### 2.2 `WARE`, `ROHSTOFF` (two strings each)
- `WARE` (outputs): up to two goods names, each looked up in the goods table → byte `+0x206 + i`. **Confirmed.**
- `ROHSTOFF` (inputs): up to two names → byte `+0x200 + i`; `+0x1fb` = number of non-`UNUSED` inputs (index + 1 of the last non-zero one). **Confirmed.** `+0x1fa` is initialised to 2 (`0x1000b4c0`). Kontor and market have `WARE` = `ALL` (0x8e) (P).

### 2.3 `PROD3` (68 bytes, every entry) → type object
Loader branch `0x100094d2`–`0x1000972f`. **Confirmed** (every offset read from the disassembly); meanings in the last column are ours.

| Off | Type | → field | Conversion at load | Meaning |
|---|---|---|---|---|
| 0x00 | u16 | `+0x21c` | — | running cost A (smaller value; weaver 10) — **likely** "inactive" |
| 0x02 | u16 | `+0x21e` | — | running cost B (weaver 15) — **likely** "active" |
| 0x04 | u8 | `+0x208` | `v << 8` | storage capacity of output 0 in tons (weaver 5, mill 7) |
| 0x06 | u8 | `+0x20a` | `v << 8` | storage capacity of output 1 (small farm 4) |
| 0x05, 0x07 | u8 | — | not read | always 0 (P) |
| 0x08 | f32 `T0` | `+0x214`, `+0x210` | `trunc(T0·128)`; `trunc(256/T0 + 0.5)` | seconds per ton of line 0 (weaver 43, mill 20); `+0x210` = **output per step** in 8.8 |
| 0x0c | f32 `T1` | `+0x218`, `+0x212` | as above; `T1 = 0.0` → `T1 = T0` | time base of line 1 / input 1 |
| 0x10 | f32 `I0` | `+0x202` | `trunc(I0·256/T0)` | **input 0 consumed per step** in 8.8 (I0 ≈ tons of input per ton of output) |
| 0x14 | f32 `I1` | `+0x204` | `trunc(I1·256/T1)` | input 1 per step (ore smelter: wood) |
| 0x18, 0x1c | f32 | — | not read | always 0 (P) |
| 0x20 | f32 | `+0x20c` | `trunc(v·256)` | output per minute for display (prop. `0x468`); recomputed for growing fields, §4.1 |
| 0x24 | f32 | `+0x20e` | `trunc(v·256)` | same for line 1 |
| 0x28 + 3g | 3 × u8 | `+0x1e0 + 3g` | stored as (`b1`, offset, `b0`) | figure group g = 0, 1, 2: `b0` = number of figure types, `b1` = max figures out at once, `b2` = start index into the list |
| 0x32 | 9 × u16 | `+0x1ea` | groups concatenated | `FigurenHeader.dat` ids of the workers/carriers (203 `TRAEGER`, 101 `MAEHER`, 106 `Kleinbauer`, 220 `MARKTLER` market cart) (P) |
| 0x42, 0x43 | u8 | `+0x1fd`, `+0x1fc` | — | ? (mostly 0 / 2) |

After the table: for PRODTYPE 226/227 (`ROHSTWACHS`/`ROHSTDUERR`) with stage count `S = HEAD[0x10] > 1` (`0x100088b0`): growth period `P` (`+0x1fe`, u16) = `(+0x214 · 1000) / (2^k · S · 128)` with `k = HEAD[0x18]`, and then `+0x20c = 15 360 000 / (2^k · S · P)` (15 360 000 = 60 000 · 256). Otherwise `P = 0`. **Confirmed.** Example 712 grain field: `T0` = 310, `S` = 5, `k` = 2 → `P` = 15 500, `+0x20c` = 49 (P).

The floats (`T0`, `T1`, `I0`, `I1`, display rates) exist only in the file and in the load-time conversion; the simulation uses the integers. (Load-time float use: §8.)

**Exact order (read 2026-10-18 at `0x10009627`–`0x10009717`):**
- `q0 = 256/T0` stays on the FPU stack; `+0x210 = __ftol(q0 + 0.5)`.
- `q1 = 256/T1'` is also stored to an f32 (`fsts 0x1c(%esp)`); `+0x212 = __ftol(q1 + 0.5)`.
- `+0x202 = __ftol(q0 · I0)` with the unrounded `q0`; `+0x204 = __ftol(f32(q1) · I1)`.
- So the table's "`trunc(I·256/T)`" is shorthand for these products.

Results are stored with 16-bit moves, except the `T·128` values, which use 32-bit moves. The 24-bit-precision case is OQ-032. **Confirmed.**

### 2.4 Other `HEAD` fields used here
`HEAD` loader `0x10009334`–`0x100094cd`: byte 0x10 → `+0x66` (stage count `S`; slot `+0x7c`), byte 0x18 → `+0x6e` (`k`; slot `+0x80`), byte 0x19 → `+0x77` (fertility influence, property `0x2e`, §4.2), bytes 3/4/5/6 → `+0x20/+0x1f/+0x1e/+0x21` and u16 8 → `+0x1c` = **build cost**: bricks, wood, tools, marble, gold (slot `+0xcc(0, goods)` returns the byte for goods 0x29–0x2c, `(1)` gold, `(2, i)` the `PROD3` running costs). **Confirmed** (offsets); "build cost" is **likely** (weaver: 300 gold, 3 tools, 4 wood; Kontor 350/5/12; P).

**`HEAD` byte table (read instruction by instruction 2026-10-19 for the type registry of `src/sim`, `0x10009334`–`0x100094cd`).** The record is read into a 56-byte stack buffer that is zeroed first (`rep stosd`, 14 dwords, `0x10009351`–`0x1000935a`), so a shorter record reads as zeros beyond its end (Anlagen.dat: 36 (5 entries), 44 (35), 48 (5), 52 (142) or 56 (734) bytes, never longer, P); `HEAD[k]` is `[esp+0x34+k]` after the read. **Confirmed** (code; the offsets of the table that were earlier "likely" through Ghidra stack names are now read from the instructions):

| HEAD | Width | → type field | Meaning / getter |
|---|---|---|---|
| `[0]`, `[1]` | u8 | `+0x06`, `+0x07` | footprint width, height (`0x10009376`, `0x1000937d`) |
| `[2]` | u8 | `+0x74` | no consumer found (O-21) |
| `[3]` | u8 | `+0x20` | cost in ZIEGEL (goods `0x2b`) |
| `[4]` | u8 | `+0x1f` | cost in HOLZ (goods `0x2a`) |
| `[5]` | u8 | `+0x1e` | cost in WERKZEUG (goods `0x29`) |
| `[6]` | u8 | `+0x21` | cost in MAMOR (goods `0x2c`) |
| `[8..9]` | u16 | `+0x1c` | gold cost (`0x1000944d`–`0x10009452`) |
| `[0x0a]` | u8 | `+0x7a` | rubble pattern (slot `+0x104`) |
| `[0x10]` | u8 | `+0x66` | stage count `S` |
| `[0x11]` | u8 | `+0x65` | action radius (market search radius, §3) |
| `[0x18]` | u8 | `+0x6e` | shift `k` |
| `[0x19]` | u8 | `+0x77` | fertility influence |
| `[0x1c..0x1d]` | **u16** | `+0x12` | type flag word (`0x2000` coast, `0x1000` mountain, `0x4000` road/bridge; `0x100093bd`/`0x100093d7`, `movw`) |
| `[0x2c..0x2d]` | u16 | `+0x70` | maximum hit points |
| `[0x31]` | u8 | `+0x79` | can burn |

The cost getter `0x1000a5a0` (case 0) returns `byte[type + goods − 0x0b] << 8`: goods `0x29` → `+0x1e` = `HEAD[5]`, `0x2a` → `+0x1f` = `HEAD[4]`, `0x2b` → `+0x20` = `HEAD[3]`, `0x2c` → `+0x21` = `HEAD[6]` (case 1 = gold `+0x1c`, case 2 = running cost `+0x21c + 2i`). The public-need mask getter `0x1000a600` reads the goods byte `WARE[0]` (type `+0x206`) and returns `1 << (g − 0x4c)` for `g` in `0x4c`–`0x5a`, except `0x51` → `0x30`, `0x53` → `0xc0`, `0x54` → `0x1c0` (0 otherwise). The width of the flag word (u16, not u32) is new; the model's `BuildingType::flags_word` reads it as such.

### 2.5 Getters (type vtable `0x1002185c`)
`+0x5c` PRODTYPE · `+0x7c` `S` · `+0x80` `k` · `+0xac(i)` output goods · `+0xb0(i)` input goods · `+0xb4(g, j)` figure type object (via game slot `+0x6c`) · `+0xb8(g)` max figures · `+0xbc(g)` figure-type count · `+0xc0` output-0 capacity (8.8) · `+0xc8(i)` display rate · `+0xdc(1)` input count · `+0xe0(i)` output per step (`+0x210 + 2i`) · `+0xe4(i)` input per step (`+0x202 + 2i`) · `+0xe8` growth period `P` · `+0xec` research id. Property getter `+0x48` (`0x1000a860`) exposes the same data by id (`0x44c` inputs/step, `0x44d` input goods, `0x44e` output/step, `0x44f` capacities in tons, `0x451/0x453` figure lists, `0x454` `T0`/`T1` as floats `= +0x214/128`, `0x457` running costs, `0x468` display rates `/256`). **Confirmed.**

## 3. Runtime building classes (`AnnoWelt`)

Factory `0x10024010` switches on PRODTYPE and constructs (size, constructor, vtable): workshop 1 → `0x30`, `0x100688f0`, `0x100c8010`; farm 2/4/8/10 → `0x38`, `0x1006f160`, `0x100c8ae0` (6 `FISCHER` → vtable `0x100c2ac4`, 16 `PELZJAEGER` → `0x100c2bf0`); mine 3 → `0x28`, `0x1006e460`, `0x100c8990`; 9 → `0x48`, `0x1006ff80`, `0x100c8d60`; 12/17 → `0x38`, `0x100693c0`, `0x100c8150` / `0x100c2d1c`; 13/14 → base `0x100734c0` with vtable `0x100c2614`; 33–35 → `0x50`, `0x1006c080`, `0x100c86c8` (36 → `0x100c2740`). Base class constructor `0x100734c0`, vtable `0x100c92b8`. **Confirmed.**

Common building fields: `+0x06/+0x08` tile x/y, `+0x0a` step timer, `+0x0c` flags (bits 0–1 rotation, bit 2 "type has > 1 stage", bit 3 "blocked" state shown to the player, bits 4 and 5 suspend production — meaning **guess**: under construction / burning), `+0x0d` status (bits 0–4 active status icons, bits 5–7 current status code), `+0x10` owning city, `+0x14` type object. Slot `+0x28` attaches objects by class id (slot `+0x14` of the argument: `0x700` type, `0x1400` city, `0x1500` figure, `0x1800`, `0x1a00`). **Confirmed** (`0x100732d0`, `0x10068750`, `0x1006b9c0`).

## 4. Production steps

### 4.1 Workshop (`HANDWERK`) — `0x100680b0` (slot `+0x8c`)
Fields: `+0x18` output stock (u16, 8.8), `+0x1a` output reserved by an incoming cart, `+0x1c/+0x1e` input stocks, `+0x20` activity average, `+0x24` "switched on" byte, `+0x28/+0x2c` carriers out. Per step (**confirmed**, integer only):

```
if (u16)(t - last) < 500: return;  last += 500
if not switched_on: return
activity = activity * 127 >> 7
if flags & (bit4 | bit5): return
n_in = type.input_count
ratio = 128
for i in 0..n_in-1:  ratio = min(ratio, (in[i] << 7) / need[i])     # need[i] = type +0x202+2i
if n_in > 0 and ratio < 64:  status(1 "raw material missing", repeat 45000 t)   -> no production
elif out >= cap0:            status(2 "storage full", repeat 45000 t)           -> no production
else:
    made   = (rate0 * ratio) >> 7                                                # rate0 = type +0x210
    out   += made;  activity += made << 3
    in[i] -= (need[i] * ratio) >> 7   for each input
    clear status bits 1..3; if blocked flag was set: clear it and notify the city
carriers_out = number of non-empty carrier slots
if carriers_out < type.max_figures(0) and n_in > 0: send a carrier (job 0x33, figure type list[0], §5.1)
```

So production is **proportional**: with 50–100 % of one step's inputs in stock, a reduced amount is made; below 50 % nothing. `out` is checked against the capacity only before the step, so it can exceed the capacity by up to one step. In both status paths, after the status call, the "blocked" flag (building `+0x0c` bit 3) is set if it was clear, and the city is notified through city slot `+0x18` → slot `+0x168` (event `{type id, x, y, rotation, blocked}`; `0x10068276`–`0x10068330`); production clears it and notifies likewise. **Confirmed.**

**Status helper `0x10072ed0` (code, `t`, repeat, input index)** (corrected 2026-10-18; read instruction by instruction):
- A lower code than the current one (`+0x0d >> 5`) is ignored.
- A higher code stores the u16 time via slot `+0x120(t)` (workshop `+0x22`, farm `+0x20`) and the code.
- `elapsed = (t − slot+0x124()) & 0xffff`. While `repeat > elapsed` nothing more happens, so a message is due only after the condition has lasted `repeat` ticks.
- When due:
  - code 1, unless the input is a crop (goods 0x38–0x4b) or an animal (0x89–0x8d), calls `0x10071f40`; code 2 calls `0x10072390`. A zero result turns the status into code 3.
  - Then `0x10073010(code, 1)` raises the icon/message, and the time is set to `t − repeat`, so the message stays due at every later report. `0x10073010(i, on)` sets icon bit `i` of `+0x0d` (bits 0–4) and replaces the icon objects. It does this only if the city owner's slot `+0xc4(3)` returns 1 (`0x1007302f`); otherwise it changes nothing (OQ-033).
- Production calls `0x10072fb0(0x0e)`. If one of the icon bits 1–3 is set, it drops the current code (when it is 1, 2 or 3) and removes those icons (`0x10073010(i, 0)`).
- The meaning of `0x10071f40` and `0x10072390` is open. **Confirmed** (control flow).

### 4.1a Two-output workshops: `HANDWERKDUAL` and `HANDWERKWAFFEN` (read instruction by instruction 2026-10-18)
Both are built by constructor `0x100693c0` (size `0x38`): outputs `+0x18`/`+0x1a` (u16), inputs `+0x20`/`+0x22`, activity per line `+0x24`/`+0x26` (u16), line switches `+0x2a`/`+0x2b` (both 1 after construction, `movw $0x101`), split byte `+0x2c` (`0x40`), carrier slots `+0x30`/`+0x34`. PRODTYPE 12 gets vtable `0x100c8150`, 17 `0x100c2d1c` (§3). Both vtables' input priority (`+0xd8`) is the base class's `0x10072870`: like the workshop's `0x10068500` (§5.1) but from the stock alone (`stock / need` below 30 → `0x80`, below 60 → `0x60`, else −1), never 0, so a carrier goes whenever a slot is free. Both ticks share the workshop's gate, suspension test, input ratio, "missing input" path (code 1, 45,000 t), carrier rule, consumption and status clearing (§4.1), with these differences. **Confirmed** (D; disassembled with capstone, `tools/disasm.py`).

**`HANDWERKDUAL` tick `0x10068a70`** (no entry uses it; documented, not implemented):
- The tick returns after the gate when both switches are 0. Each switched-on line ages its activity (`· 127 >> 7`). The switches gate nothing else.
- Every line is checked against the output-0 capacity `+0xc0`, not its own. A line that is not full makes `rate_j · (s_j · ratio >> 7) >> 7`, with `s_0 = split` and `s_1 = 128 − split`. The activity grows by `made << 4` (u16).
- A full line reports code 2 (45,000 t) and sets the blocked flag.
- The inputs are used up with the full `ratio` as soon as one line made something (`0x10068d16`).

**`HANDWERKWAFFEN` tick `0x10069550`** (implemented, `weapons_workshop_step`):
- Line *j* **works** when its switch is on and its output goods `g = type+0xac(j)` is available to the owner (`0x10069580`–`0x10069640`):
  - `owner = city+0x10 → slot +0x1c`;
  - `b = byte at (owner → slot +0x6c → slot +0x08) + 0x18 + 26·g`;
  - available if `b = 0`, or if `owner → slot +0xbc → slot +0x08(b)` has bit 1 set.
  - The meaning of the requirement is **likely** research (OQ-034).
- No working line: the tick returns after the gate. Only working lines age their activity.
- The split byte is **not** read. A working line gets the share `0x40` when the other line works, else `0x80`. It is computed as `(((other ? 1 : 0) − 1) & 0x40) + 0x40` on the low byte (`0x1006973d`, `0x10069824`), and that low byte does not depend on the register's upper bits.
- `made = rate_j · ((share · ratio >> 7) & 0xff) >> 7`, against the output-0 capacity as in DUAL. A full line reports code 2 and sets the blocked flag whether it works or not. A free line that does not work does nothing.
- If line 0 is full and line 1 produces, the blocked flag is set and cleared in the same step, so the city is notified twice.


### 4.2 Farm family (`PLANTAGE`, `WEIDETIER`, `STEINMETZ`, `SCHAFFARM`, `FISCHER`) — `0x1006ea90`
Fields: `+0x18` output, `+0x1a` reserved, `+0x1c` raw stock (harvested, filled by the workers via slot `+0xc0` = `0x1006ee70`), `+0x1e` activity, `+0x22` switched on, `+0x24…+0x34` up to 5 workers out. Per step (**confirmed**):

```
gate, switched_on, flags as in 4.1;  activity = activity * 255 >> 8
max_w = type.max_figures(0)
if max_w == 1 and a worker is out: skip production
elif out >= cap0: status 2 (45000 t)
else:
    ratio = min(128, (raw << 7) / need0)
    if ratio < 64: status 1 (60000 t)
    else: raw -= (need0*ratio) >> 7;  made = (rate0*ratio) >> 7
          (optional graphic-stage update by fill level, slot +0x11c)
          out += made;  activity += made << 4
dispatch (if max_w > 1 or nothing was made):
    if workers_out < max_w and 384 / rate0 >= raw / need0:
        send worker (figure list[0], job from the figure type's slot +0x54) to harvest input 0
```

Grain farm 811: `rate0` = 37, `need0` = 80 per step (P, §9). The condition `384/rate0 ≥ raw/need0` keeps the raw stock at about one and a half output-ton-times of work (**likely** reading).

### 4.2a Fur trapper (`PELZJAEGER`, 824) — `0x1006e5d0` (read instruction by instruction 2026-10-18)
The farm's fields (§4.2); its own tick on vtable `0x100c2bf0`. **Confirmed** (D, `tools/disasm.py`).

```
gate, switched_on, activity = activity * 255 >> 8, flags as in 4.2
ratio = (any of the 5 worker slots +0x24..+0x34 set) ? 0 : 0x80     # setne/dec/and 0x80
if ratio != 0:                                                       # else: no production, no status
    ratio = min(ratio, min((raw << 7) / need0, 128))
    if ratio < 64:            status 1 (45000 t), blocked             # input first, as the workshop
    elif out >= cap0 (u16):   status 2 (45000 t), blocked
    else: made = rate0 * ratio >> 7; out += made; activity += made << 4 (u16)
          raw -= need0 * ratio >> 7; clear 0x0e; unblock
if ratio < 0x80 and no worker slot is set and 384 / rate0 >= raw / need0:
    target = 0x10071da0(8)          # nearest figure of the input goods in a radius from the building size and 8 (float distances)
    if target: figure = this+0x118(job 0x0d, type+0xb4(0, 0), t); hand it the input goods (type+0xb0(0)) and the target
```

Differences from the farm: input before capacity; "missing input" repeats after 45,000 ticks instead of 60,000; nothing is made while any trapper is out (the farm only stops for a single-worker farm); `max_figures` is not read: at most one trapper is out. Since `ratio < 0x80` with no trapper out means `raw < need0`, `raw / need0` is 0 there and the 384 test never fails in practice. 824: rate 26, need 25, 5 t (P).

### 4.3 Growing fields and trees — island chunk scan `0x1008b5f0`
Fields, forests and other `ROHSTWACHS`/`ROHSTDUERR` items are not buildings: they are tile objects in the island's 8-byte-per-tile grid (`+0x0c`: u16 object id at +0, phase byte at +3 = bits 0–5 counter, bit 6 parity, bit 7 kept). On every chunk visit (≥ every 2000 `t`, §1), for every tile whose object type has `P ≠ 0` (§2.3) (**confirmed**):

```
parity = (t / (P/2)) & 1
if parity != stored bit 6:
    store parity; counter = (counter + 1) & 63
    if counter % 2^k == 0:
        if counter >= S << k:  object id += 1;  phase &= 0x80     # next growth object, e.g. 712 -> 713 (counter and parity bit cleared)
        notify island slot +0x188(position, 0xce)                 # redraw
```

So an object advances to the next id after `S · 2^k` parity flips, i.e. after `S · 2^k · P/2 = T0 · 500` `t` (= `T0` seconds, likely): 712 grain field → 713 after 310 s, forest 104 → 105 after 480 s (P: `T0` values; arithmetic confirmed). Because only the parity is compared, a flip is lost if two happen between visits (`P/2 < 2000`). That happens in the original data (corrected 2026-10-18; the earlier claim of a minimum of 7750 was wrong). `P` is a u16 (`movw` at `0x100088fd`, computed in u32), giving these small `P/2` values:
- 125: entries 356–364;
- 232: the `ROHSTDUERR` entries 706, 718, 726, 734, 738, whose 66,000 is stored as 464;
- 500: entry 754;
- 1,500: entry 354;
- 1,750: entry 756.

(P) Ripe objects (713 `ROHSTOFF`, `WARE` = `GETREIDE`; 105, `WARE` = `BAUM`) are what the farm workers harvest (**likely**; harvest amount not traced, Open). `ROHSTDUERR` (714/715) is the drought variant (`k` = 0, `P` = 62 000) (P).

**Fertility.** `0x10084370` rebuilds the island's fertility nibble grid (`+0x18`, low 4 bits, 0–10 — the `FRUCHTBAR` scale) from `Wachstum.dat`: per tile `p = Proz[GFXID >> 2]` (the `Wachstum.dat` table built by `AnnoGame` `0x10006a80` indexed by texture id and cell `u`, `v` — the `GFXID` bits without rotation; exposed as game property `0x2e`); if the tile holds a raw-material object (PRODTYPE 225–256) whose `HEAD[0x19]` value `v ≠ 0`, `p` is blended towards `v` (`(p + 3v)/4` if `p < v`, else `(v + 3p)/4`); then `b = (p + 5) / 10`, and the stored value is `min(10, Σ w·b / 12)` over a 5 × 5 neighbourhood with per-climate weights at `0x100ca008` (25 bytes per climate; climate byte at island `+0xc0`). **Confirmed** (code + table). Which code reads fertility for planting or growth is not traced (Open).

### 4.4 Mine (`MINE`) — `0x1006def0` (read instruction by instruction 2026-10-18)
Fields: `+0x18` output, `+0x1c` activity (u16), `+0x20` switched on, `+0x21` deep, `+0x24` deposit object. **Confirmed** (D, `tools/disasm.py`; the earlier decompile reading is replaced by this).

```
gate; if not switched_on: return            # no suspension test (flags bits 4/5 are not read)
activity = activity * 63 >> 6
if out >= cap0 (u16):        status 2 (45000 t), blocked; return
if deposit == 0:             status 1 (30000 t), blocked; return
rate = type+0xe0(0); left = deposit+0x88(deep); got = deposit+0x8c(rate, deep)
if (got < rate and left != 0) or (got != 0 and rate >= left):        # 0x1006df9a–0x1006dfac
    if not find_again():                                             # 0x1006ddf0: deposit = best in reach, true if its +0x88(deep) != 0
        if type+0xac(0) == 1 (EISENERZ):
            player message 0x15b if the (new) deposit's +0x88(1) != 0, else 0x19   (via city+0x10 → +0x1c, → +0x18 → +0x18, slot +0xd0)
if got == 0:                 status 1 (30000 t), blocked; return
fill-level graphic: if flags bit 2 clear, stage = (out·(S−1) + cap/2) / cap, changed → slot +0x11c (render only)
out += got (u16); activity += got << 4 (u16); clear 0x0e; unblock
```

**Deep** (`+0x21`, set when the type is attached, `0x1006e270`): for a mine whose output 0 is goods 1 (`EISENERZ`), `deep = (type+0xec research id == 0x39 TIEFE_ERZMINE)`; every other mine has `deep = 1`. When the city is attached, the mine looks up its deposit (`0x1006ddf0`). **Confirmed.**

**Deposit lookup** `0x1006ddf0` → island (`city+0x18`) slot `+0x114` (`0x1008bec0`: island `+0x24`) → slot `+0x14` = `0x100787c0`, vtable `0x100c9b10`. It is called as `(rect, output goods 0, deep, island+0x28)`, where `rect` = the building's tile x, y and its size w × h by rotation (type slots `+0x50`/`+0x54`). The find:
- queries the island objects in the rect grown by 7 tiles on each side (4th argument, slot `+0x08`, kind 1);
- keeps those whose goods (slot `+0x84`) match;
- converts each one's float position (slots `+0x54` x, `+0x5c` y, divided by the constant at `0x100c9b08`, `__ftol`) to tiles;
- keeps those within a circle of radius 7 around the footprint: the row distance beyond the footprint is `dy` ≤ 7, and the column distance `dx` must not exceed the half-width `[7, 7, 7, 6, 6, 5, 4, 2][dy]`. The table is built by the loop `0x100787cb`–`0x10078805` and emulated exactly.
- Of those it returns the one with the largest `+0x88(deep)`; the first one is taken unconditionally, a later one only if it holds strictly more.
- The result is stored in `+0x24` even if it is empty.

**Confirmed.**

**Deposit class** (constructor `0x100637a0`, vtable `0x100c7508`; island-object base `0x100648a0`). It holds the remaining amount at `+0x28` (u32, 8.8; **`0x7d000` = 2,000 t** when created) and the reserve flag at `+0x30` (1 when created). **Confirmed.**
- Goods (`+0x84`, `0x10063640`): the object type's slot `+0x80`.
- `+0x88(deep)` (`0x10063650`): `remaining − 0x64000` if the flag is set and the mine is not deep, else `remaining`. 0x64000 is **1,600 t**.
- `+0x8c(rate, deep)` (`0x10063670`):
  - with the flag set and a deep mine: return `rate`, nothing used up.
  - with the flag set and a shallow mine: if world setting 14 bit 1 is clear (`deposit+8 → +0x18 → +0x74(0x0e) & 2`), return `rate`. Otherwise take `rate`, or `remaining − 0x64000` when `remaining − rate < 0x64000` (unsigned), so the 1,600 t reserve is never touched.
  - without the flag: the same setting test; then `min(rate, remaining)`.

So a shallow iron mine can use 400 t of a fresh deposit; deep iron mines and all other mines never use a flagged deposit up; and with setting 14 bit 1 clear, nothing runs out at all. World setting 14 is the edition: bit 1 is the `ANNOADDON1` flag (AnnoScene `0x1000265e`–`0x1000268a` sets it from the root name; AnnoWelt `0x10063686` reads it), so depletion exists only in the add-on edition and is not an owner option (OQ-035 a, resolved 2026-10-18; residual doubt: `[deposit+8]` is the island). Who clears the reserve flag and whether files set other amounts are open (OQ-035 b, c).

A second class (constructor `0x100637f0`, vtable `0x100c759c`, derived from it) returns goods `0x84` as a constant and starts with `0x7d0` (7.8 t). Its meaning is open.

### 4.5 Small farm (`UNIVERSAL`, 809) — `0x1006f660` (read instruction by instruction 2026-10-18)
Fields (class `0x1006ff80`, size `0x48`): `+0x18`/`+0x1a` outputs, `+0x1c`/`+0x1e` activity (u16), `+0x24`/`+0x26` raw stocks, `+0x2a`–`+0x2e` figure counters (byte each: 0 = line-0 workers, 1–4 = animals of kind 106 + i), `+0x2f`/`+0x30` line switches. 809: alcohol from potatoes (rate 13, need 128) and food from grass (rate 4, need 50), 4 t, figure group 0 max 3 (P). **Confirmed** (D, `tools/disasm.py`).

```
gate; if both switches are 0: return
if flags bit 4/5: age both activities (· 255 >> 8); return              # switched on or not
if switch 0: age activity 0; made0 = line(0, rate0, need0, t)
    if made0 == 0 and counter0 < 1 and 384 / rate0 >= raw0 / need0:
        figure = this+0x118(job 0x2a, type+0xb4(0, 0), t); give it input goods 0
if switch 1: age activity 1; made1 = line(1, rate1, need1, t)
    kind = u16 table 0x100c8d48[draw & 7]                 # draw = world PRNG: building+0x10 (city) -> slot +0x18 (island) -> slot +0x18 (world) -> slot +0xb4 = 0x100a1740 (AnnoWelt 0x1006f810-0x1006f838)
                                                          # table 107 110 107 109 107 109 107 109: NOT the island climate (an earlier reading took slot +0xb4 for a climate getter;
                                                          # 2026-10-18 correction). One draw per step with line 1 on, after the gate, before the limit tests below.
    i = kind − byte 0x100c8d58 (106); if byte 0x100c8d59[i] (limits 1 2 0 1 1 0 0 32) <= counter[i] − (made1 != 0): return
    if max_figures(0) <= counter1 + … + counter4: return
    if 384 / rate1 >= raw1 / need1: figure = this+0x118(job 0x2e, world figure type kind (… → +0x6c(kind)), t); give it input goods 1
```

**Line step** `0x1006f440(line, rate, need, t)` (returns the amount made):
- capacity first: `cap = type+0xc0` (output 0's, also for line 1); `out[line] ≥ cap` → status 2 (45,000 t), blocked, 0.
- `q = (raw << 7) / need`; above 128 → 128; below 64 → status 1 with repeat **`0x124f8` = 75,000 t**, blocked, 0.
- `used = ratio · need >> 7`; if its u16 is 0, return 0 without a status.
- `raw −= used`; `made = ratio · rate >> 7`; `out += made`; `activity += made << 4` (u16).
- The status is cleared (`0x10072fb0(0x0e)`) and the flag unblocked unless the **other** line is switched on and full (`0x1006f4da`–`0x1006f4f0`).

**The 75,000 never comes due:** the status helper compares the interval signed with the elapsed time masked to 16 bits (`0x10072f1a`–`0x10072f22`). Since 75,000 > 65,535, the small farm never raises its "missing input" message (the status code is still set). **Confirmed** (D); ISS-150.

### 4.6 Market and Kontor — `0x1006b6d0`
Per step: if fewer than `max_figures(0)` carts (slots `+0x20/+0x24`) are out, send one (figure list[0] = 220 market cart, job `0x34`); group 2 figures (e.g. ships' crews) are sent analogously. The building has no stock of its own: its add/remove slots (`+0xc0`, `+0xa8`) forward to the city (slots `+0x80`, `+0x84`). **Confirmed.**

## 5. Transport

### 5.1 Sending figures
Base slot `+0x118` (`0x10072d20`, args job id, figure type, `t`): refused while flags bits 4/5 are set or the building has no reachable entrance (`0x10072b50`); creates the figure at the entrance through the figure factory, which is **world** slot `+0xa0` (`0x100a34d0`, returns `[world+0x114]` = the unit registry; corrected 2026-10-18, it is not an island slot), links building ↔ figure ↔ island, creates the job object (figure slot `+0xa4`, factory `0x1003b0e0`: job `0x33` workshop carrier, vtable `0x100c45e8`; `0x34` market cart, `0x100c4568`; `0x29`/`0x2a`, `0x2b`, `0x2c`, `0x2d`/`0x2e`, `0x35`, `0x41` other workers) and assigns it. **Confirmed.** Jobs are lists of steps (step type codes 7 harvest, 9, 0xb "repeat while cargo < capacity", …), saved as `TASK`/`ENTRY` (`0x1005b7a0`/`0x1005b9a0`). **Likely.**

Workshop input priority (slot `+0xd8`, `0x10068500`): for input goods `g`, `(stock + amount carried or reserved by the building's carriers) / need_per_step`; below 30 steps → `0x80`, below 60 → `0x60`, else −1. **Confirmed**; the workshop tick dispatches whenever the result is non-zero, so −1 also dispatches — how the job uses the value is open.

### 5.2 Market cart (job `0x34`, step `0x1005ad10`)
1. Candidates: buildings in the market's area (rectangle = market footprint grown by a radius from the type, `0x1005ad10`) that have at least **1 t (256)** of output 0 or output 1 available (`out − reserved`, slot `+0xa4`) — filter `0x1005aca0`. **Confirmed.**
2. Path search over the road net (searcher slot `+0x7c`, node callback `0x1005a900`); for each candidate reached, for each of its outputs `g` for which the city stock is below the city capacity, a score (**confirmed**):
   - goods 1–25 (raw and intermediate): `q = cap·3/4`; `score = ((avail − q) << 7) / (cap − q)` (fill above 75 %);
   - other goods: `score = (avail << 7) / cap`, only if `avail ≥ 256`;
   - the best score wins (searcher slots `+0x8c/+0x88/+0x90`); the search stops early when `score ≥ 128` and (for goods 1–25) `2·cap ≥` city stock of `g`.
3. The chosen building reserves the cart's load (building slot `+0xb4(goods, amount)`, workshop `0x10068460`: `reserved += min(amount, out − reserved)`), with `amount` = the cart figure's slot `+0x84`. **Confirmed**; that slot returning the figure type's capacity is **likely** (5.4).

The initial best score of the searcher (whether a raw-material building below 75 % fill can ever be chosen) is not traced (Open).

### 5.3 City (warehouse) stock — city vtable `0x100c2f40`
Per goods number `g < 0x38`: 12-byte record at city `+0x334 + 12g`: u32 stock (8.8), u16 `+4` consumed by residents, u16 `+6` added, u16 `+8` removed, byte `+0xa` bit 0 sell/buy flag, byte `+0xb` trade-list index. Add (slot `+0x80`, `0x1002da50`): refused for `g > 0x37`; if stock + amount (u32) exceeds the **capacity** `+0x624`, the amount becomes capacity − stock (wrapping, so a stock above the capacity is lowered to it; `0x1002da7d`, corrected 2026-10-18); remove (slot `+0x84`, `0x1002da90`) clipped at the stock; residents' consumption (slot `+0x88`, `0x1002dac0`) only for goods 0x1a–0x28. The capacity is the same for all goods. **Confirmed.** Savegame city `STADT2` (`0x1002e170`): `WARE` = 8-byte entries (u16 goods number, u16 tons, 4 bytes not used here) → stock = tons << 8; `HEAD2` u16 at offset 0x24 = capacity in tons, **0 → 50 t**; `BAUMAT` → `+0x48c + 8g`; `VK`/`EK` sell/buy lists; `PROD/ENTRY` = buildings (4-byte position + the building's own record). **Confirmed** (offsets from the stack layout). How the capacity changes with Kontor level is not traced (Open).

### 5.4 Figures: capacity and speed
`FigurenHeader.dat` figure type (`AnnoGame` `0x1000fe00`, vtable at `0x10021cf8`): `HEAD` f32 at 4 → `+0x10` (slot `+0x78(x)` returns `x · speed`), f32 at 0 → `+0x2c`; `LOGIC` byte 0x11 → `+0xf2` = `v << 8` (slot `+0x9c`). **Confirmed.** Reading: `+0xf2` = carry capacity (`TRAEGER` 203 = 3 t, market cart 220 = 5 t, most carriers 3 t; P) — **likely**; `HEAD` f32 4 = walking speed (carriers 0.012, market cart 0.016, camels 0.008; P) — **likely**, unit open. Harvest jobs loop "harvest" while cargo < the figure's capacity (`0x1005c260`, step type `0xb`) — **confirmed** for the loop, capacity source **likely**.

**`wegspeed.dat` is not read.** No module (`*.dll`, `1503Startup.exe`) contains the strings `wegspeed`, `speed`, `Mask` or a key `Un`; the only `Ob` is unrelated (P, byte search). The same holds for `Highmap.dat`. **Likely** (a file read through a computed name would still need its keys). Road speed, if any, is elsewhere (Open).

## 6. Status and running state that stops production (summary)
Switched off (`+0x24`/`+0x22`/`+0x20` byte, set by slot `+0xc8` → `0x10072a60`), flags bit 4/5, output at capacity (status 2), inputs below half a step (status 1), deposit empty (mines), single-worker farm with the worker away. Status repeat intervals (time a condition must last before its message, §4.1): 45000 `t` (workshop; farm full), 60000 `t` (farm, missing), 30000 `t` (mine, missing). **Confirmed.**

## 7. Goods numbers and names

The goods number is the index into `AnnoGame`'s goods-name table (`0x10013be0`, pointer tables at `0x100251d0`–`0x10025400`, numbers 0–0x8e). **Confirmed.** Numbers with a name (others return `UNUSED`):

| No. | Names |
|---|---|
| 1–6 | `EISENERZ GOLD SALZSTEIN EDELSTEINE STEINE MAMORSTEIN` |
| 9–23 | `WOLLE ZUCKER TABAK FLEISCH KORN MEHL EISEN FLACHS HOPFEN TIERHAEUTE SEILE SEIDE FARBSTOFFE WALSPECK HOLZKOHLE` |
| 26–39 | `NAHRUNG LEDER SALZ ALKOHOL TABAKWAREN GEWUERZE HEILKRAEUTER STOFFE SEIDESTOFFE LAMPENOEL KLEIDUNG SCHMUCK PELZE WEIN` |
| 41–44 | `WERKZEUG HOLZ ZIEGEL MAMOR` |
| 46–53 | `SCHWERT BOGEN ARMBRUST MUSKETE RUESTUNG LANZE AXT KANONESHIP` |
| 56–72 | crops/resources: `GETREIDE TABAKBAUM GEWUERZBAUM FLACHSBAUM ZUCKERROHR BAUMWOLLE WEINTRAUBEN KAKAOBAUM HOPFENBAUM SEIDENRAUPE FARBBAUM KRÄUTER KARTOFFEL GRAS BAUM FISCH EISFISCH` |
| 76–89 | public needs: `WIRTSHAUS … GALGEN` (14) |
| 96–117 | units: `SPIESSTRAEGER … WELTBEHERRSCHER` |
| 122–129 | ships: `HANDELSCHIFF1 … PIRATSCHIFF`; 132 `SCHATZ`; 137–139 `FLEISCHTIER PELZTIER WALFISCH`; 142 `ALL` |

- Warehouse goods are numbers 0–0x37 (§5.3). Producer/carrier logic treats 1–25 as raw/intermediate (§5.2), 0x1a–0x28 as consumer goods (§5.3), 0x38–0x4b as crops (§4.1). **Confirmed.**
- Saves and scenarios carry a `WARETYPE` record = list of (u16 number, name); on load `AnnoWelt` `0x100a18d0` maps every file number to the current number **by name** (written by `0x100a1840` for numbers 0–0x37). **Confirmed**; P: `00_1024_easyentry_01.sze` holds `0 UNUSED, 1 EISENERZ, 2 GOLD, …` matching the table.
- `BGruppen.dat [WAREN]` `Id` names are looked up in the same table (`AnnoGame` `0x10014c25` ff.; 26-byte record per goods; `Preis` parsed as an integer and copied into the price slots, `PreisLevel` overrides). **Confirmed.**
- `Texte.dat` `[WARE_*]` lists are positional over the named goods of a range: `[WARE_ROHSTOFF]` 21 = goods 1–6, 9–23; `[WARE_BEDARF]` 14 = 26–39; `[WARE_BAUMAT]` 4 = 41–44; `[WARE_WAFFEN]` 8 = 46–53; `[WARE_ANBAU]` 12 = 56–67; `[WARE_INFRA]` 14 = 76–89; `[WARE_SOLDAT]` 20 = 96–115; `[WARE_SCHIFF]` 8 = 122–129. P (counts and English names line up one by one, e.g. 12 `FLEISCH` = "Cattle"); the View code walks the goods table with kind `0x20` while reading these sections (`View` ~`0x10087880`) — **likely** (partly resolves OQ-029 f).

## 8. Floating point (determinism)
- **Load time only, production data:** `AnnoGame` `0x100095c7`–`0x10009717` converts the `PROD3` floats with x87 `fmul`/`fdiv`/`fadd 0.5` and `__ftol` (a 64-bit `fistp` returning the low 32 bits: non-finite values give 0; `0x10016d60`, truncation) — constants 128.0 (`0x10021850`), 256.0 (`0x1002184c`), 0.5 (`0x100214ac`). The results are integers in the type object; production, growth, fertility and city stock code use no floats (**confirmed** for `0x100680b0`, `0x1006ea90`, `0x1006def0`, `0x1008b5f0`, `0x10084370`, `0x1002da50`; `0x1006f660`/`0x10068a70` decompile shows none). A rebuild should precompute the integers exactly from the file's binary floats (power-of-two products are exact; `256/T + 0.5` and `I·256/T` need care near integers) and check them against fixture F1.
- **Display only:** properties `0x454`, `0x455`, `0x468` and the per-minute value (`+0x20c`) as floats.
- **Simulation-relevant floats:** figure types keep speed and other `LOGIC` values as floats (`AnnoGame` `0x1000fe00`–`0x10010394`; slot `+0x78` multiplies by the speed), and the road search callback converts float positions with `__ftol` (`AnnoWelt` `0x1005a914`/`0x1005a926`, `__ftol` = `0x100b5f90`, truncation). Figure movement is therefore float-based in the original; `AnnoWelt` has 584 `__ftol` calls and ~7,100 x87 instructions overall (P, `objdump` count). This is the determinism risk for transport.

## 9. Fixture spec (for `anno-diff` against the original)
Units: stocks in 8.8 (256 = 1 t), time in steps (1 step = 500 `t`; expected 1 s at normal speed). Every fixture states the start state, which can be set up in a scenario or save, and the values to read back (debugger on the fields named, or the UI: building info panel, warehouse).

| # | Setup | Expected |
|---|---|---|
| F1 | Load all 921 `ANLAGEN` entries | Per entry, the type object holds `+0x210 = trunc(256/T0 + 0.5)`, `+0x202 = trunc(I0·256/T0)`, `+0x204 = trunc(I1·256/T1')` (`T1' = T1 or T0`), `+0x208 = PROD3[4] << 8`, `+0x1fe`/`+0x20c` as in §2.3. Spot values: 1001 → 6/11/0, cap 1280; 1002 → 13/15; 1004 → 26/25; 1017 → 17/21/13; 1019 → 9/8/2; 811 → 37/80; 1205 → 21; 712 → `P` 15500, `+0x20c` 49; 104 → `P` 24000; 714 → `P` 62000 |
| F2 | Weaver's hut 1001, switched on, wool 1280 (5 t), cloth 0, no carriers | each step: cloth +6, wool −11; after 116 steps cloth 696, wool 4; step 117: no production, status 1 ("raw material missing") |
| F3 | Weaver, wool 8, cloth 0 | one step: ratio 93 → cloth +4, wool −7 (→ 1); next step: none, status 1 |
| F4 | Weaver, wool plenty, cloth 1279 | step 1: cloth 1285 (exceeds cap by 5); step 2: none, status 2 ("storage full") |
| F5 | Ore smelter 1017, ore 1280, wood 25 | step 1: iron +17, ore −21, wood −13 (→ 12); step 2: ratio 118 → iron +15, ore −19, wood −11 (→ 1); step 3: none, status 1 |
| F6 | Grain farm 811, max workers 1, worker at home, grain (raw) 160, corn 0 | step 1: raw 80, corn 37; step 2: raw 0, corn 74; step 3: none, status 1 |
| F7 | Workshop activity: weaver producing every step from activity 0 | activity after n steps follows `a ← (a·127 >> 7) + 48`; info-panel value = `a·60 >> 10` |
| F8 | Grain field 712 placed (counter 0) | becomes object 713 after 20 parity flips of `t / 7750`, i.e. ≈ 155 000 `t` (≈ 310 s); forest 104 → 105 after ≈ 240 000 `t` |
| F9 | Ore mine 1205 on a fresh iron deposit, ore 0 | +21 per step until ≥ 1792 (7 t), then status 2; record the deposit's remaining amount over time (unknown rule) |
| F10 | City with capacity 50 t (`HEAD2` capacity 0), wool 12 700, add 300 | added 100, stock 12 800 |
| F11 | Market 1301 with two producers in range: A cloth 300/1280, B cloth 200/1280 (both goods 33 > 25) | cart goes to A (score 30 vs. 20); a producer with 255 available is never chosen; with A at ≥ 1280 the search stops at A |
| F12 | Market and a grain farm with corn (goods 13 ≤ 25) at 900/1792 vs. 1400/1792 | score `((avail − 1344)·128)/448` (division truncates towards zero): −126 vs. 16 → the second is chosen; record whether the first is ever chosen when alone (initial best score) |
| F13 | Load a savegame, then read `WARETYPE` mapping | every file number maps to the number with the same name (§7) |
| F14 | Timing: measure wall-clock seconds for F2's 116 steps at normal game speed | expected 116 s if 1 step = 1 s |

## 10. Additions from the S3 review round (2026-10-19)
Read in the S3 review of the type registry; each is **D** (disassembly) unless stated.
- **Name loops of the type reader** (`AnnoGame.dll` `0x10009240`): the WARE branch (`0x1000a156`-`0x1000a205`) and the ROHSTOFF branch read exactly two names each (the loops `cmp eax, 2` at `0x1000a1f4` and `0x1000a2e2`) and store the goods bytes at `+0x206+i` / `+0x200+i`. Names after the second are never looked up. Only the ROHSTOFF branch stores a count (`+0x1fb` = index + 1 of the last non-UNUSED name, `0x1000a2c9`-`0x1000a2cf`); the WARE branch has none. The corpus has exactly two names in all 921 records. The rebuild reads two names (`kWareRecordNames`).
- **Stamp gates of the unported class ticks** (the compare constant of `(u16)(t - last) >= period`): `0x1006b4e0` (PublicService and School share it) `cmp dx, 0x1f4` at `0x1006b4ec` = 500, Hunter `0x10069f40` 500 at `0x10069f5d`, Whaler `0x10073720` 500 at `0x10073737`, Shipyard `0x10074e90` 500 at `0x10074ea1`, CannonTower `0x1006ab50` `0xa6` = 166 at `0x1006ab6a`. The shared tick at `0x1006b4e0` is 13 instructions and is the MARKTSTAND rule (add the period to `last` once the difference reaches it). The Castle tick `0x1006f1d0` has no stamp gate (byte counter at `+0x38`). The values are in `ClassInfo::gate_period`; `docs/sim/state.md` Appendix C lists them with the earlier ones.
- **Name scans** (state.md F25): the PRODTYPE scan runs index `0..0x1a0` (`0x100097b7`, names `0x100138e0`), the research scan ends at `0x8f` (`0x10013cf0`), the goods scan covers `0..0x8e` (`0x10013be0`); a name that matches nothing leaves the loop without a store, so the field keeps 0 (UNUSED).
- **HEAD buffer** (state.md F26): 14 dwords are zeroed (`rep stosd`, `0x10009351`) and the record is read into the 56-byte buffer (`0x10009365`).

## Open
- Unit of `dt` (ms assumed) and game-speed scaling between `AnnoFrame` and world slot `+0x84` (Advance); confirm with F14.
- Flags bits 4/5 at building `+0x0c` (what suspends production); what slot `+0xc8`/`0x10072a60` changes (running cost switch?).
- Running costs: confirm `PROD3` 0x00/0x02 as inactive/active cost and where they are charged.
- Harvest amount per field and what a harvested field becomes (713 → ?); which code reads fertility (planting, growth chance); meaning of `Wachstum.dat` `Klima`/`Bridge` tables beyond being loaded (`0x10006a80`).
- Deposit class: amounts per `BERGWERK2` deposit, the extraction rule behind slots `+0x88/+0x8c`, effect of "deep" mining.
- Carrier and cart capacity: confirm figure slot `+0x84` = type `+0xf2`; unit of the speed float; which code (if any) slows figures on roads/slopes, given that `wegspeed.dat` is not read.
- Market search: initial best score, area radius source (type byte `+0x65`?), tie-breaking (path order), group-2 figures of the market.
- Workshop carrier job `0x33`: where inputs are fetched from (warehouse only, or producers too), use of the priority values `0x80`/`0x60`/−1.
- `PROD3` bytes 0x42/0x43 (`+0x1fd`/`+0x1fc`); fill-level graphic stage (`+0x11c`) condition in the farm tick.
- Kontor level ↔ city capacity (`+0x624`), and the second `t`-period (15000) island pass (slot `+0x194`).
- `STEINBRUCH`/`EISLOCH` (base class, no own tick): where quarries produce (probably in the worker job).
