# Island records: evidence for the S5 state model (AnnoWelt.dll)

Findings of the two S5 review passes over `AnnoWelt.dll` (GOG 2.0.0.5, image base `0x10000000`), read with `tools/disasm.py` logic. They back the island scope of `docs/sim/state.md` (§4.8, §5.8, §9.3, O-11, O-54, O-55, OQ-061). Confidence: **D** = read from disassembly, addresses given.

## Drought object (constructor `0x10087280`, tick `0x1008aeb0`)

- Constructor: object of `0x18` bytes. `x`, `y` words at `+0`/`+2` floored to 8; target ring dword (radius / 8); `ring` dword at `+8` = 0; `city_mask` dword at `+0xc` = 0; `phase`, `step`, `state` bytes at `+0x10`, `+0x11`, `+0x12` with `state` = 1; `counter` word at `+0x14`; `stamp` word at `+0x16` from the world tick (slot `+0x74`, 4 bytes). **D**
- Tick: the u16 gates are `0x32` (strip period) and `0x3e8` (plateau period). The limit compares are **unsigned below** (`jb`), not equality: `cmp byte [d+0x11], 0x40 / jb` at `0x1008b4f7` (step), `cmp byte [d+0x10], 4 / jb` at `0x1008b516` (phase), `cmp word [d+0x14], 0x78 / jb` at `0x1008b5be` (plateau counter). The reset happens in the same tick, so a stored drought never has step 64, phase 4 or counter 120. The ring compares are signed (`jge`). **D**
- Consequence for the model: `validate_islands` accepts `phase < drought_phases`, `step < drought_steps * chunk_edge`, `counter < drought_plateau_count`, `0 <= ring <= target_ring`; `docs/sim/state.md` §5.8 states the machine with `>=` (equal on reachable states, and a loaded record at the edge cannot make an `==` machine miss its reset forever).

## Island sweep state

- Growth and fauna sweep positions are words at `+0x9c`/`+0x9e` and `+0xa0`/`+0xa2`; they start at 0 (`0x1008cb72..0x1008cb87`) and advance by `add edi, 8` with a wrap to 0 (`0x1008b7d9..0x1008b7ef` growth, `0x1008b851..0x1008b867` fauna): they are always multiples of 8. `+0x9a` is the stamp = the world tick at island initialisation (`0x1008c2e6`); the constructor `0x1008cb30` zeroes `+0xa4` (the 300 s event stamp). **D**
- The visit `0x1008b5f0` loops 8 x 8 tiles without a bounds check (A14 / F20 of the spec). **D**

## Tile record

- Island reset `0x1008c740` initialises every 8-byte tile record as `word@0 = 0`, `byte@2 = 0`, `byte@3 = 0`, `dword@4 = 0x1e000200` (`0x1008c80d`, `0x1008c818..0x1008c821`). Besides the owner nibble 15 (`0x1e000000`, `>> 25 & 0xf`, the neutral owner) a fresh tile carries bit `0x200`. The model's `aux = 0` default is a stand-in (O-11). **D**
- `byte@3` is the growth phase in its **low 6 bits** (`& 0x3f`, wraps at 64; `0x1008b68d..0x1008b6c6`), bit 6 is a parity flag the visit toggles, bit 7 another flag. `TileLayer::phase` and `TileObjectSpec::phase` are `u8` in the model and accept 64..255, which the original cannot hold; the range is decided when S7 / S14 read the bits. **D**

## Island members not modelled (O-54, OQ-061)

- Island property setter `0x10087b90` (jump table `0x10087c04`): key 7 -> `+0xc0` (climate, modelled), key 8 -> `+0xc1` (byte), `0xb` -> `+0xac` (dword), `0xe` -> `+0xa6` (word), `0xf` -> `+0xa8` (word), `0x10` -> `+0xc3` (byte), `0x11` -> `+0xc6` (word, default 1). The reset zeroes `+0xb0..+0xbf`, `+0xc2`, `+0xc4`. No consumer found. **D**
- City ring (`island + 0x38`): `+0x40` dword (last added slot), `+0x44` word (stamp, modelled), `+0x46` byte (cursor, wraps at 16, modelled), `+0x47` signed byte (highest used slot, initialised to `0xff`; `0x100967d0`, `0x100968b3`, `0x100969bb`): `+0x40` and `+0x47` are derivable from the city table. **D**
- Object manager: stamp word at `+0x14` (init 0), cursor words at `+0x24`/`+0x26` (`0x10095d40`, constructor `0x10096445`). The model keeps a stand-in index into `Island::deposits` (A10). **D**
- 300 s event (`0x10093980`): stamp word at island `+0xa4`, `t / 500`, gate `0x12c`, climate byte `+0xc0`, scan of 15 slots, one draw. **D**
- PRNG seed: cursor = seed mod table length (unsigned division, `0x100a32ac`). **D**
