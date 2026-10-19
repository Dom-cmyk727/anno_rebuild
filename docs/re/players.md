# `AnnoWelt.dll` — the player table and the player object

**Status:** S4 review round (2026-10-19), static analysis only (disassembly with `tools/disasm.py`). Module: `AnnoWelt.dll` (GOG 2.0.0.5, image base `0x10000000`). Confidence as in [`tick-loop.md`](tick-loop.md): **confirmed** = read in the disassembly. The rebuild's model is `docs/sim/state.md` §4.5 and §5.10 (`src/sim/src/player.cpp`).

## Player table tick `0x1009c4d0` (world vtable `0x100cb858` slot `+0x24`)

- **Confirmed:** called by the world advance (`0x100ae040`) through the object at `+0x118`, with the tick from `+0x10c`, after the figure registry and before the timers. `d = u16(tick - [+0x88])`; the visit count is `d·32/50 + 1`, capped at `0x20`; a do-while calls the per-player tick (vtable `+0x5c` = `0x1001b900` in vtable `0x100c2238`) for every non-null slot, advancing a cursor (a word at `+0x8c`) that wraps at `0x20`; then the AI at `[+0x94]`; the stamp `[+0x88]` is stored last (`0x1009c5d8`).
- **Two brackets around the AI call, not modelled:** before the AI call (`0x1009c5a4..0x1009c5b0`) the function calls through `[esi+0x90]` -> vtable `+0xb0` -> the object's vtable `+4` with argument `6`; after it (`0x1009c5d1..0x1009c5d5`) the matching vtable `+8` call. The world advance uses the same shape around its phases (`push 2`, `push 0xa`, `push 5` with `+4` and `+8` on `[esi+0x124]`, `0x100ae083..0x100ae0e3`), which looks like a profiler or phase bracket (begin/end of phase 6). No state change is assumed and the callee was not read. **Open:** if it is not a profiler, S8 (the AI hook) must reproduce it; listed in `docs/open-questions.md` OQ-059.
- The `+0x8a` stamp and the `+0x8e` ring drive `0x10001020`, a bare `ret 8` (no behaviour).

## Per-player tick `0x1001b900` and upkeep `0x1001b840`

- **Confirmed:** gate `sub ax,[esi+0xa4]; cmp ax,0x7d0; jb` (16-bit unsigned, period 2000); booking `S = [+0x8c] + [+0x88] + u16[+0xa2]` in a 32-bit register, charge `S/15` (`mul 0x88888889 >> 3`) passed to vtable `+0x68` (`sub [ecx+0x74], arg`, `0x1001cfe0`), remainder `div 15` stored as a word at `+0xa2`, stamp = `bx` (the low word of the tick). Statistics `+0x80` then `+0x84` become `(13·x << 3) / 105` in 32 bits (`mul 0x38138139`, `0x1001b8aa..0x1001b8e8`): the product wraps from `x >= 41 297 763` (OQ-057).
- **Confirmed:** `0x1001b900`: neutral test `cmp dword [esi+4],0x1f; je`; state test `test byte [esi+0x28d]; jne` (any non-zero byte, so every non-zero state is "not alive", OQ-058); money read via vtable `+0x60` (`mov eax,[ecx+0x74]`, `0x1001cfc0`); bankrupt test `cmp eax,0xfffff448; jg` (signed, money <= -3000); deadline `[+0x290] == 0` -> `t + 0x249f0` (150000) in 32 bits (`0x1001b941`), else `cmp edi,eax; jbe` (strict unsigned `>`, `0x1001b94d`; the jump at `0x1001b93f` makes setting and testing exclusive within one visit); recovery clears `+0x290`; the defeat byte `+0x295` is cleared before the call to `0x10020550`; the jump at `0x1001b97e` skips both steps for the neutral and the non-alive player.

## Player constructor `0x10021600..0x10021836`

- **Confirmed:** money `0x2710` (`+0x74`), kind word `8` (`+0x78`), volk word `1` (`+0x7a`), flags dword `1` (`+0x90`); `+0x80..+0x8c`, `+0xa2`, `+0xa4`, `+0x290`, `+0x295` zero. **`+0xa8` (the colour slot) is stored from `bl` with `ebx = 0` (`xor ebx,ebx` at `0x1002164e`, the zero register of every other zero store of the function), and only `+0xa9` (the default colour) gets `0xff` (`0x1002183c`)**; the setter at `0x1001be54` confirms `+0xa8` as the colour slot. The earlier spec line "both 0xff" was wrong (S4 review round).
- **Not modelled (OQ-059):** the constructor also stores `+0xa6 = 0xffff` and `+0x28c = 0x1f`; their consumers are unread.
- **State byte `+0x28d`:** the property setter (`0x1001be0d..0x1001be1f`) stores any value and keeps 8 sticky (`cmp byte [edi+0x28d],8` unless the new value is 7); the record loader (`0x1001d37a..0x1001d382`) stores the record byte and maps 7 to 8 (OQ-058).

## Research object `0x10019190` (player `+0xb0`)

- **Confirmed:** 65 state bytes at player `+0x136` initialised from a table at `0x100e0bf6` (stride 8; read: all zero) and 65 progress words at `+0xb4`. Record loading (`0x1001da19..0x1001db4a`): record bit 0 sets state bit 1 (researched), record bit 1 (after the prerequisite check) sets state bit 0 (researchable), the progress word is a byte shifted left by 8.
- **Not modelled (OQ-059):** the constructor also sets `word [obj+0xc8] = 0` (`0x100191f9`, player `+0x178`) and fills 0x30 bytes with `0x80` at `obj+0xca..+0xfa` (`0x100191c0..0x100191f6`, player `+0x17a..+0x1aa`). If they are queue or order state they are missing from the player stream; the consumer is unread.
