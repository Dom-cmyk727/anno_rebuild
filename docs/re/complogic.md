# `CompLogic.dll` and `CompLogicClassic.dll` — inventory

**Status:** T38, 2026-10-26, and T40, 2026-10-26 (the per-advance call, §2 and §4). Static reading only. No behaviour of a computer player is claimed here beyond the instructions that were read. Tags as in [`annowelt.md`](annowelt.md): **D** = read at the address, **likely** = the reading fits and was not proved, **open** = not read, with the next address.

Image base of both modules is `0x10000000`. Tools: `tools/disasm.py`, `tools/xref.py --reloc`, `analysis/pe_symbols.json`, and a capstone sweep whose output stayed in the worktree `cache/` (not committed). The profile hand-over already written in [`annoscene.md`](annoscene.md) §11 is cited where this pass re-read the same address, and left standing where it was not re-walked.

## 1. PE facts

| | `CompLogic.dll` | `CompLogicClassic.dll` |
|---|---|---|
| file size | 454 656 | 438 272 |
| `SizeOfImage` | `0x72000` | `0x6e000` |
| timestamp | `0x44c0c36d`, 2006-07-21 12:07:09 UTC | `0x44c0c37f`, 2006-07-21 12:07:27 UTC |
| machine | `0x14c` (x86) | `0x14c` |
| `.text` | `0x10001000`, virtual `0x61495` | `0x10001000`, virtual `0x5e0b5` |
| `.rdata` | `0x10063000`, virtual `0x1d45` | `0x10060000`, virtual `0x1c4c` |
| `.data` | `0x10065000`, virtual `0x7260` | `0x10062000`, virtual `0x71e0` |
| `.rsrc` | `0x1006d000`, virtual `0x3b8` | `0x1006a000`, virtual `0x3b8` |
| `.reloc` | `0x1006e000`, virtual `0x30c0` | `0x1006b000`, virtual `0x2eea` |

**D.** Exports, both modules, the only two: `CompCreateInstance`, `GetAnnoComplogicDllGuid`. In `CompLogic.dll` they are `0x100163b0` and `0x100163e0`. In `CompLogicClassic.dll` they are `0x100149d0` and `0x10014a00`.

**D.** The import table of each module names `KERNEL32.dll` only (59 imports, the CRT and heap set). There is no import from `AnnoWelt.dll`, `AnnoGame.dll` or `AnnoFrame.dll`. `pe_symbols.json` agrees (`rtti` empty, the same two exports, the same one import DLL). The string `user32.dll` is present at `CompLogic.dll` `0x10064090` and is referenced from `0x100604c2`. **Open:** whether that reference is a `LoadLibrary`. Next: `0x100604c2`.

**D.** RTTI type descriptors are absent. The byte sequences `.?AV`, `.?AU` and `.?AT` occur zero times in either file. Classes below are the vtable runs in `.rdata` and the constructors that the factory calls. They have no C++ names in the file.

**D.** `COMPPROFIL2`, `COMPCAPS`, `Profiles` and `.htm` occur zero times in either file. The AI does not name the profile chunk and does not name a page file. The chunk is read by `AnnoFrame.dll` ([`annoscene.md`](annoscene.md) §11.1). What this module does store, as C strings in `.data`, are the `COMP` keys. In `CompLogic.dll`, from `0x10065340`: `SPEZFLAG`, `NATIVEBAUMODE`, `SUSPEND`, `MINEISEN`, `MINBAUMAT`, `CHEATLIMIT`, `ZIELANZAHL`, `ZIELGRUPPE`, `MAXSTADT`, `HAUPTKLIMA`, `KAMPFTYP`, `TRIBUTSPERRE`, `OPTIONAL`, `PROZENTE`, `TRANSPORT`, `HANDEL`, `LAGER`, `FLAECHE`, then `AREA`, `LOGWARE`, `EKVKWARE`, `DIPLPLAYER`, `STADTOFF`, `FIXHAUSANZ`, `NATIVEANLAGE`, `NATIVESTADTID`, `BAURECORDPOS`, `NATIVEANLAGEN`. The same keys are in `CompLogicClassic.dll` (its `KAMPFTYP` is at `0x100624c0`). These are the key names of §11.5, now with their addresses. `TYP` itself is not a separate hit of the needle `TYP=`; the key reader compares the short string, which this pass did not re-walk (**open**, next `0x10001450`, the base reader named in §11.5).

## 2. Which module the game loads

**D.** `AnnoWelt.dll` `0x1009c630` (re-read, `0x1009c630..0x1009c731`). The holder's field `+0x94` is returned immediately when it is already set (`0x1009c639..0x1009c641`). Otherwise:

- property 9 of the object at `+0x90` is read by that object's slot `+0x74` with argument 9 (`0x1009c647..0x1009c651`). A non-zero result skips the load and returns the still-null `+0x94` (`0x1009c654..0x1009c656`). §4 shows `+0x90` is the world, and world slot `+0x74` is `0x100a3380`. Case 9 of its jump table (`0x100a3480` → `0x100a340e`) returns the byte at world `+0x69ce`. A non-zero byte is the "AI not loaded" gate. The tick in §4 does not test the byte again: once `+0x94` is set, this function returns it and ignores property 9.
- when `+0x98` is null (`0x1009c65c..0x1009c664`; a set handle skips to the call of `+0x9c`), property 14 is read by slot `+0x74` of the same object, with argument `0xe`, and bit 1 of the result is tested (`0x1009c67e`, `0x1009c681`);
- bit 1 set pushes `CompLogic.dll` (`0x100dcfa8`); bit 1 clear pushes `CompLogicClassic.dll` (`0x100dcf90`);
- the module handle is stored at `+0x98` (`0x1009c6ac`);
- `CompCreateInstance` (`0x100dcf7c`) is resolved and stored at `+0x9c` (`0x1009c6c9`);
- that pointer is called with argument 0, and the result is stored at `+0x94` (`0x1009c6db`, `0x1009c6e6`);
- the new object's slot `+0x18` is called with argument 3, then its slot `+0xc` with two pointers (`0x1009c703`, `0x1009c721`).

The function is slot `+0x14` of the vtable at `0x100cb858` (the dword is at `0x100cb86c`). The holder constructor stores that vtable at `[holder+0]` (`0x1009cb3f`; the relocated immediate is `0x1009cb41`). A second relocation, `0x1009cbc1`, was not re-read. The class has no RTTI name. It is not the world vtable (`0x100ccdd8`, [`transport.md`](transport.md)). The per-advance use of this object is §4.

`CompCreateInstance` in `CompLogic.dll` is the manager constructor `0x1003d590` (cited from §11.5; this pass did not re-enter it). In `CompLogicClassic.dll`, `0x100149d0` allocates `0x38` bytes, calls `0x1003aaa0`, and stores the result at the global `0x100672a4`. **D.**

## 3. Classes and the factory

**D.** `CompLogic.dll` factory is manager slot `+0x1c` = `0x1003dc50` (vtable `0x100634f8`). It computes `type - 11`, rejects a value above 6, and jumps through `0x1003de68`:

| index | type | case | allocation | constructor |
|---|---:|---|---:|---|
| 0 | 11 | `0x1003dc87` | `0x2420` | `0x1004ed60` |
| 1 | 12 | `0x1003dcb4` | `0x1d98` | `0x1003ec80` |
| 2 | 13 | `0x1003dcb4` | `0x1d98` | `0x1003ec80` |
| 3 | 14 | `0x1003dce0` | `0x1d74` | `0x100516a0` |
| 4 | 15 | `0x1003dd06` | `0x1d9c` | `0x10053660` |
| 5 | 16 | `0x1003dd2c` | `0x1d8c` | `0x1004c0c0` |
| 6 | 17 | `0x1003de00` | `0x1da4` | `0x10001040` |

Types 12 and 13 are one case. Before that case, a third argument equal to 1 is left as 1 and any other value is replaced with 1 (`0x1003dcb4`). The constructed object then stores the type at `+0xa18` and that third argument at `+0xa14` (`0x1003dd89`, `0x1003dd8f`), and the factory calls the object's slot 0 with the player index (`0x1003dd9f`). This is the factory of §11.5, re-read, with type 17 filled in. §11.5 had left type 17 unfollowed.

Type 17's constructor `0x10001040` calls `0x10002880` and then builds a sub-object at `+0x1d74` through `0x10001000`. **Open:** which vtable `0x10002880` stores (the store is not in the first instructions). Next: `0x10002880`. Player kind 17 is the god party in [`annoscene.md`](annoscene.md) §6; that this object is that party is **likely** from the factory's type argument, and its behaviour is unread.

**D.** Player vtables in `.rdata`, each a run of code pointers. Slot `+4` of every one of them is `0x10004550`, the parameter setter of §11.5. Slot 0 is `0x10005690` except on the type-11 vtable, where it is `0x1004f050`.

| vtable | slots | factory type | constructor |
|---|---:|---|---|
| `0x10063100` | 42 | base, not a factory case of its own | — |
| `0x100634f8` | 13 | the manager | `0x1003d590` |
| `0x10063550` | 42 | 12 and 13, computer | `0x1003ec80` |
| `0x10063680` | 42 | 16, Venetian | `0x1004c0c0` |
| `0x100637f0` | 42 | 11 | `0x1004ed60` |
| `0x100638b0` | 42 | 14, native | `0x100516a0` |
| `0x10063a00` | 42 | 15, pirate | `0x10053660` |

Beside them are 32-slot runs: `0x100632e8`, `0x100633d0`, `0x10063760` (its first slot `0x1004e830` sits next to the type-11 constructor), `0x10063968` (first slot `0x100526c0`, next to the native constructor). **Likely** a second interface installed into a sub-object. **Open:** the store that writes each of them. Next: a relocation of `0x100632e8`.

**D.** `CompLogicClassic.dll` factory is manager slot `+0x1c` = `0x1003b160` (vtable `0x10060438`). It does the same `type - 11` test against 6 (`0x1003b182`) and jumps through `0x1003b310`. Indices 1 and 2 share `0x1003b1c4` (types 12 and 13, as in the other module). Index 6 and the out-of-range jump both go to `0x1003b2f9`. **D:** Classic has no separate type-17 constructor; type 17 takes the failure path. The per-type allocations were not re-read. Next: the `push` immediate at `0x1003b197`.

Classic's player vtables are 41 slots, not 42: `0x10060490`, `0x100605c0`, `0x10060730`, `0x100607f0`, `0x10060940`. Its manager vtable is 13 slots, like the other module. **Open:** which of the 42 slots is the one Classic lacks. Next: compare `0x10060490` with `0x10063550` slot by slot.

The 36-index setter body was not re-walked. §11.5 and §11.6 stand for the cases, the field offsets and the value limits. What this pass adds is the vtable proof that slot `+4` is that function on every player class, and §11.8's statement that Classic's setter `0x10002e50` has the same 36 cases is not re-checked here. Next, if a case is doubted: `0x10004550` and its jump table `0x100049e0`.

## 4. Entry points, and how the AI touches the world

**D.** Creation is §2. The per-player object is created by the factory when the player reader calls manager slot `+0x1c` (§11.5, and the slot address was re-read). The script `SET_PROFILE` path calls slot `+4` on player property 44 (§11.7). Neither of those is the per-tick call.

**D (T40, answers OQ-240).** Holder slot `+0x18` (`0x1009c740`, the dword at `0x100cb870`) is only `mov eax, [ecx+0x94]; ret`. It does not call the AI. The per-advance call is the player-table tick, slot `+0x24` of the same vtable (`0x1009c4d0`, the dword at `0x100cb87c`).

`this` of each method below is the object whose first dword is the vtable the slot was taken from. The world constructor stores `0x100ccdd8` at `[world+0]` (`0x100af442`). The holder constructor stores `0x100cb858` at `[holder+0]` (`0x1009cb3f`). The manager constructor stores `0x100634f8` at `[manager+0]` (`0x1003d594`). The computer constructor stores `0x10063550` at `[object+0]` (`0x1003ecfb`).

| step | call | this | slot | argument |
|---|---|---|---|---|
| 1 | `AnnoWelt.dll` `0x100ae0e6..0x100ae0f5`, inside `Advance` `0x100ae040..0x100ae134` | the object at world `+0x118` (the holder) | `+0x24` = `0x1009c4d0` | the new world tick at world `+0x10c` |
| 2 | `0x1009c5b3..0x1009c5c0`, inside `0x1009c4d0..0x1009c5e1` | the manager at holder `+0x94`, and only when that pointer is not null | the manager's `+0x14` | the same tick |
| 3 | `CompLogic.dll` `0x1003d8a1..0x1003d8ae`, inside manager `+0x14` = `0x1003d880..0x1003d8d0` | each object on the list at manager `+0x34`, linked by `+0x9ac`, whose byte `+0x1a89` is not 0 | a direct call, `0x10003a20`, not a slot | the signed quotient of the running value divided by the running count, then the tick |

`Advance` has no test before step 1. `0x1009c4d0` calls step 2 once per entry, after the per-player visit loop and on both sides of the `+0x8a` ring (`0x1009c55c` jumps to `0x1009c59c`, which is the call, not past it). It does not test property 9, a multiplayer flag, or a host pointer. The visit loop (slot `+0x5c` of each player) is not the AI call.

`CompLogicClassic.dll` manager slot `+0x14` is `0x1003ad90..0x1003ade0` (`ret 4`). The same list, the same division, and the same shape. The byte is `+0x1a19`, the direct call is `0x10002320`, and the call after the loop is `0x1003b660`. The repeat branch goes to the null test (`0x1003adce` → `0x1003ada3`).

**D.** The list is every object the factory linked, not only types 12 and 13. On success the factory (`0x1003ddbd..0x1003ddda`) inserts the new object at the head: its `+0x9ac` becomes the old head, the old head's `+0x9a8` becomes the new object, manager `+0x34` becomes the new object, and manager `+0x1c` is incremented. The tick reads `+0x14` into the running value and `+0x1c` into the count, divides (signed), calls, subtracts the return value from the running value, and decrements the count. A zero byte skips the call and the subtraction, and still decrements the count and follows `+0x9ac`. The repeat (`0x1003d8be`) goes to the null test at `0x1003d893`, so a null link ends the walk before the next division. The walk also ends when the running value is `< 0`, including before the first node (`0x1003d88f`). After the walk, the tick pushes the same world tick and calls `0x1003e1c0` with the manager as `this` (`0x1003d8c0..0x1003d8c7`). That function was not entered (OQ-270).

**D.** Slot 0 of the player object (`0x10005690..0x100056d9`, shared by the classes whose slot 0 is not the type-11 override) sets `+0x1a89` to 1 when its second argument is 1, and to 0 otherwise. The factory's call (`0x1003dd85..0x1003dd9f`) passes `[esp+0x2c]` as that argument. Seven prologue pushes put the factory's fourth argument at `+0x2c`, and the three pushes just above are taken back by `0x10021e20` (`ret 0xc` at `0x100223c7`, the first `ret` of a linear sweep from `0x10021e20`). The `COMP` reader pushes that fourth argument as the immediate 1 (`AnnoWelt.dll` `0x1001e423`). A new object therefore starts with `+0x1a89` = 1. `SUSPEND` clears the byte (§11.5).

**D.** The running value is manager `+0x14`. Slot `+0x18` (`0x1003d7a0..0x1003d7b5`) stores `([manager+0x10] * argument)` divided by the `0x10624dd3` / `shr 5` reciprocal. The constructor stores `0x7a120` at `+0x10` (`0x1003d5c5`). The loader calls slot `+0x18` with 3 (`0x1009c701`) before anything else writes `+0x10`, so that call stores 3000. Other writers of `+0x10` were not searched.

### What creation hands the manager

**D.** `0x1009c190` stores its argument at holder `+0x90` (`0x1009c1a3`). Its only caller (`0x100aa982`, inside world `Init` `0x100aa910`) passes the world. World slot `+0x18` (`0x100a37d0`) returns `[world+0x110]`. The loader (`0x1009c706..0x1009c721`) calls manager slot `+0xc` with `(world, [world+0x110])`.

**D.** Manager slot `+0xc` (`0x1003df00..0x1003e051`) stores those two at manager `+0x20` and `+0x24` (`0x1003df34`, `0x1003df37`) and returns at once if manager `+4` is already set. `[world+0x110]` is `Init`'s third argument, stored by `0x1009d848`. The caller read at `AnnoScene.dll` `0x10002e4e..0x10002e67` (taken when scene byte `+0x164` is not 0) passes scene `+0x154`. That field is the object `AnnoGame.dll` `CreateAnnoServerInstance` builds for kind `0x101` (`0x10013760` calls `0x10007650`, which stores vtable `0x100215a4` at `[object+0]`, `0x10007663`). Other `call [reg+0x50]` sites in `AnnoScene.dll` were not walked. Next, if a second caller passes a different pointer: `0x10001232`.

The `COMP` reader does not call the factory until the manager exists (`0x1001e3e2..0x1001e434`): `[player+8]` slot `+0xa4`, then slot `+0x14` of the result. Player slot `+0x28` (`0x100209a0..0x100209c7`) stores its argument at player `+8` when that argument's slot `+0x14` returns `0xf00`. The holder stores the world there (`0x1009c407..0x1009c412`), and world slot `+0x14` (`0x100a37c0`) returns `0xf00`. World slot `+0xa4` (`0x100a34c0`) returns `[world+0x118]`, the holder, whose slot `+0x14` is the loader. So the first `COMP` record is what loads the AI, subject to property 9.

### Slots the AI calls on those two objects

Read in the loader, in manager slot `+0xc` (`0x1003df00..0x1003e051`), and in `0x1003dac0` (the factory calls it with the world, `0x1003ddac..0x1003ddb2`). None of these functions calls `AnnoFrame.dll` `0x100589a0` or `0x10058e90`. `CompLogic.dll` still imports only `KERNEL32.dll`. A city slot is not among the calls below.

| site | object | slot | target | effect in the function that was read | command path |
|---|---|---|---|---|---|
| `0x1009c651` | world | `+0x74` (9) | `0x100a3380` | returns world `+0x69ce` | no |
| `0x1009c67e` | world | `+0x74` (`0xe`) | `0x100a3380` | returns property 14 | no |
| `0x1009c70e` | world | `+0x18` | `0x100a37d0` | returns `[world+0x110]` | no |
| `0x1003df3e` | world | `+0xa4` | `0x100a34c0` | returns the holder | no |
| `0x1003df48` | holder | `+8` | `0x1009c4b0` | returns `0x20` | no |
| `0x1003df5f`, `0x1003dad2` | world | `+0x30` (`0x1300`) | `0x100a37e0` | returns `[world+0xf4]`, the island count; any other id returns 0 | no |
| `0x1003df76`, `0x1003dafb` | world | `+0x20` (`0x1300`, index) | `0x100aee00` | returns `[world+index*4+0x6530]`; `0xc00`, `0x3800` and `0x3900` return other fields; anything else returns 0 | no |
| `0x1003e008` | world | `+0x58` | `0x100a3920` | returns `[world+0xf0]` | no |
| `0x1003e010` | world | `+0x54` | `0x100a3910` | returns `[world+0xec]` | no |

`0x100aee00` was read through `0x100aee52`. The two getters `+0x54` and `+0x58` are the fields `0x1009d84e` and `0x1009d854` store from `Init`'s first and second arguments. No store in any function in this table writes the world.

**Open (OQ-272).** The object at world `+0x110` (vtable `0x100215a4`) is saved at manager `+0x24`. `0x1003d880` does not load `+0x24`. Slot `+0xc` passes the pointer to `0x1003d8e0` (`0x1003df7d`). Next: `0x1003d8e0`.

### What the per-advance call does

**D.** `0x10003a20..0x10003ad9` clears byte `+0x1a84`, then takes records from `0x10003940` and, for a kind `0..8`, calls one slot of the player object (`cmp ecx, 8` / `ja` at `0x10003a4b`; jump table `0x10003adc`). Kind above 8 does not call. The slots on the computer vtable `0x10063550`:

| kind | call | slot | computer target |
|---|---|---|---|
| 0 | `0x10003a5a` | `+0x38` | `0x10041860` |
| 1 | `0x10003a64` | `+0x28` | `0x10006790` |
| 2 | `0x10003a6e` | `+0x34` | `0x1004a150` |
| 3 | `0x10003a78` | `+0x30` | `0x10006d30` |
| 4 | `0x10003a82` | `+0x2c` | `0x100497c0` |
| 5 | `0x10003a8c` | `+0x3c` | `0x10043aa0` |
| 6 | `0x10003a96` | `+0x40` | `0x100444c0` |
| 7 | `0x10003aa0` | `+0x44` | `0x10008080` |
| 8 | `0x10003aaa` | `+0x48` | `0x10045bb0` |

After the loop, if `[esp+0x14]` is not `> 0`, it calls `0x10003b00`. The value returned to the manager is loaded from `[esp+0x10]` (`0x10003ad2`). At entry that slot is the quotient. Whether `0x10003940` writes it was not read (OQ-270).

**Open (OQ-271).** None of the nine targets was entered. City slots `+0xa0` (Build `0x100276a0`), `+0xac` (Place `0x100279a0`), `+0xb4` (RemoveAt `0x10028fd0`) and `+0xc0` (`0x1002c830`), and `AnnoFrame.dll` `0x100589a0` modes 2, 6, 7, 8 and 9 and `0x10058e90`, are not called by any function this section read to its `ret`. They can still be called inside a kind slot. Next: `0x10041860`, then the other eight targets.

The port still cannot treat the original as already emitting turn commands: the entry does not call the command function. It also cannot yet be wrapped as commands, because the writes inside the nine slots are unread. What is settled is where the call sits: inside `Advance`, once per advance, on the manager, then on each linked object whose `+0x1a89` is set.

### Host, clock, frame, UI

**D.** Both step functions call world slot `+0x84` on the object at the net client's `+8`. Single-player `AnnoFrame.dll` `0x1005b110` reaches it at `0x1005b164..0x1005b16a` (`ecx` = `[esi+8]`, argument the piece in `ebx`). Multiplayer `0x1005fe10` reaches it at `0x1005ff08..0x1005ff12` and `0x1005ff8d..0x1005ff9a` (`edi` = `[esi+8]`, `ebp` = that object's vtable). Neither window tests a host or server pointer, and `0x1005fe10..0x1005ffc0` contains no `+0x3c8` displacement. The multiplayer function returns before those calls when byte `+0x6f4` is 0 (`0x1005fe6a..0x1005fe83`); that is the in-game flag already in [`tick-loop.md`](tick-loop.md), not a host test. `CompLogic.dll` imports no timer function (59 `KERNEL32` imports, none whose name is a clock). The argument the AI receives is the world tick. The functions read here do not read the frame timer, `GetTickCount`, or a UI field. How often `Advance` runs is the caller's: the frame step in single-player, the turn loop in multiplayer ([`tick-loop.md`](tick-loop.md) §2). A kind slot could still branch on the machine. That is the same unread range as OQ-271.

## 5. The world-PRNG slot

[`tick-loop.md`](tick-loop.md) counted 35 calls of slot `+0xb4` in this module and said the count is an upper bound, because `+0xb4` is a common slot, and that a following mask is what makes a site look like the world draw. A capstone sweep finds **43** instructions `call dword ptr [reg+0xb4]` in `CompLogic.dll` and **39** in `CompLogicClassic.dll`. **D** for both counts, re-checked in T44: every `FF /2` encoding with a disp32 of `0xb4` (43 in `CompLogic.dll`, 39 in `CompLogicClassic.dll`) is reached by a recursive descent from `int3` entries, direct-call targets and relocations. The disp32 immediate `0xb4` itself occurs 96 times in `CompLogic.dll`'s executable bytes. One that is not the call is `0x1000cad8`, inside `mov edi, [esp+0xb4]`. A 44th call encoding was not found; the earlier "44 byte hits, one not an instruction" is that wider immediate, not a second call. None of the 43 containing regions is itself a slot of the vtables in §3. One function inside a region is: `0x100011b0` is slot `+0x58` of vtable `0x10063100` (the dword is at `0x10063158`).

Of the 43, 32 use the returned value as a mask or a division in the next few instructions. 11 do not. A mask does not make the receiver the world. T44 traces the register. World slot `+0xb4` is the world PRNG ([`tick-loop.md`](tick-loop.md) §3), re-read: the dword at `0x100ccdd8+0xb4` is `0x100a1740`, and that function ends `mov ax, [edx*2+0x100cb8f8]; ret` with no stack argument. A call that pushes three values and does not adjust `esp` afterwards is not that function.

`fn` is the start of the `int3`-padded region that contains the call. A region can hold more than one logical function; `0x10034162` in particular holds eight of these calls. T44 F1: `0x100217eb` is listed at `0x10021770`, the function that begins after `ret 0xc` at `0x1002176d`. The earlier label `0x100216c0` is an address inside `0x10021610`.

| call | region | use of the result |
|---|---|---|
| `0x100011d3` | `0x10001000` | `and 7` |
| `0x1000225e` | `0x10001000` | `and 7`, then a table of 8 at `0x10065300` / `0x10065320` |
| `0x10002622` | `0x1000247f` | `and 3` |
| `0x10007e6d` | `0x10007985` | `and 3` |
| `0x10007ee7` | `0x10007985` | `and 7`, then a table of 8 at `0x100654c4` / `0x10065484` |
| `0x1000e809` | `0x1000e381` | not masked in the next 8 instructions |
| `0x100217eb` | `0x10021770` | `and 3`, then a float table |
| `0x100289b3` | `0x100260ae` | low 6 bits (`and 0x3f`) |
| `0x100289e6` | `0x100260ae` | low 6 bits |
| `0x10028b3f` | `0x100260ae` | low 6 bits |
| `0x10028b57` | `0x100260ae` | low 6 bits |
| `0x1002e3f3` | `0x1002bde4` | not masked; the region returns |
| `0x1002e4f9` | `0x1002bde4` | not masked; the region returns |
| `0x1002e63d` | `0x1002bde4` | not masked; the region returns |
| `0x1002e88e` | `0x1002bde4` | not masked |
| `0x10030fbb` | `0x10030e36` | `div`, remainder kept |
| `0x100341ba` | `0x10034162` | `div`, remainder kept |
| `0x1003435b` | `0x10034162` | not masked |
| `0x100343df` | `0x10034162` | `div`, remainder kept |
| `0x10034471` | `0x10034162` | not masked; a later call is slot `+0x7c` |
| `0x10034593` | `0x10034162` | not masked |
| `0x10034614` | `0x10034162` | `div`, remainder kept |
| `0x1003478e` | `0x10034162` | `div`, remainder kept |
| `0x10035419` | `0x10034162` | `div`, remainder kept |
| `0x10035676` | `0x10034162` | not masked |
| `0x100357dc` | `0x10034162` | not masked; the region returns |
| `0x10035cfb` | `0x10034162` | not masked |
| `0x1003f00d` | `0x1003e3f3` | `and 0xf` |
| `0x10043f47` | `0x10041f67` | `and 3` |
| `0x100488df` | `0x10047bc1` | `and 0xf` |
| `0x10048b70` | `0x10047bc1` | `and 0xf` |
| `0x1004b7ef` | `0x1004a251` | `and 7`, then a float table at `0x10065fa4` |
| `0x1004df0f` | `0x1004cf41` | masked by a register |
| `0x1004e2bf` | `0x1004cf41` | `and 7` |
| `0x100517d7` | `0x10050fb1` | `and 7` |
| `0x10051b25` | `0x10050fb1` | `and 7` |
| `0x10053849` | `0x10053301` | `and 7` |
| `0x100547ed` | `0x10053301` | `and 0xf` |
| `0x10054b98` | `0x10054b35` | `and 7`, then a `u16` table at `0x100662a0` |
| `0x10055041` | `0x10054b35` | `and 7`, then a jump table at `0x10055208` |
| `0x10056d96` | `0x10056493` | `and 7`, then the float table at `0x10065fa4` |
| `0x10056ef4` | `0x10056dcc` | `and 7`, then the same float table |
| `0x10057013` | `0x10056dcc` | `and 7`, then the same float table |

The two sites `tick-loop.md` names are in this table: `0x100011d3` is the `and 7` it places in `0x100011b0`, and `0x10007e6d` is the `and 3` it places in `0x10007e50`. The region starts are coarser than those inner labels.

What each draw decides is not named here. The plain uses above are only the arithmetic. T44 follows the register and the return value.

### Where the two pointers come from

**D.** `0x100692d0` is the world. Its only store is `0x1000cadc`, in `0x1000cab0`: after `sub esp, 0xa0` and `push esi`, `[esp+0xa8]` is the first stack argument, and that dword is written to the global and to `[this+0x1d14]` (`0x1000cae2`). The only caller is `0x1003e023`, inside manager slot `+0xc` (`0x1003df00..0x1003e051`, `ret 8`). That slot's first argument is `esi` (`0x1003df2c`), stored at manager `+0x20` (`0x1003df34`) and used as the object of slot `+0xa4` (`0x1003df3e`). The call pushes `esi` last, so it is `0x1000cab0`'s first argument. §4 already reads that argument as the world.

**D.** `[object+4]` is not one pointer. Two functions are called with `lea ecx, [edi+4]` and the same three pushes (a shifted 1, then manager `+0x20`, then manager `+0x24`):

| call | function | what `[esp+…]` is after the prologue | stored at `[this]` by |
|---|---|---|---|
| `0x1003dd79` | `0x10021e20` (`sub esp, 0xa4`, three pushes, `ret 0xc`) | `[esp+0xb8]` is the second argument, manager `+0x20` | `0x10021e38` |
| `0x1003de4f` | `0x10021610` (three pushes, no frame, `ret 0xc` at `0x1002176d`) | `[esp+0x14]` is argument 2, manager `+0x20` | world at `[this]` by `0x1002161f`; manager `+0x24` at `[this+4]` by `0x10021621` |

`0x1003de1f` calls `0x10001040` and `0x1003de24` keeps the result in `edi`. `0x1003de26..0x1003de34` does not write `edi`, and `0x1003de34` skips the call when `edi` is 0. `0x10001040` is the type-17 constructor: it calls `0x10002880` and stores vtable `0x10063100` (`0x100010bf`). Manager `+0x24` is the slot's second argument (`0x1003df37`). §4 reads that as `[world+0x110]`, vtable `0x100215a4` for the one `AnnoScene` caller it followed. A second `Init` caller is the open note in §4, not a new guess.

**D (T44 F1).** `0x10021610` reads argument 1 from `[esp+4]` before `push ebx` / `push esi` / `push edi` (`0x10021610..0x10021616`). After those three pushes `[esp+0x10]` is argument 1 and `[esp+0x14]` is argument 2. The factory pushes the mask, then `[esi+0x20]`, then `[esi+0x24]` (`0x1003de49..0x1003de4b`) and calls with `lea ecx, [edi+4]` (`0x1003de4c`). Argument 2 is manager `+0x20`, the world from `0x1003df34`, and it is stored at `[this]` (`0x1002161f`), so at `[object+4]`. Argument 1 is manager `+0x24` and it is stored at `[this+4]` (`0x10021621`), so at `[object+8]`. Both factory paths store the world at `[object+4]`. The earlier reading that `[esp+0x14]` was argument 1, and that this path stored manager `+0x24` at `[object+4]`, is withdrawn.

`0x10021e38` then calls slot `+0x54` on the stored pointer (`0x10021e3d`), which §4 has as the world getter `0x100a3910`. That agrees with the world store at `[object+4]`.

Classic's copy of the global is `0x10066248`. The only store is `0x1000b31c`, in `0x1000b2f0`, the same frame as `0x1000cab0`, also writing `[this+0x1d14]`. The only caller is `0x1003b4c3`. The function starts at `0x1003b3a0`: `esi` is the first argument (`0x1003b3cc`), stored at `+0x20` (`0x1003b3d4`), and is the object of slot `+0xa4` (`0x1003b3de`). The call pushes `esi` last. Same proof. The call that uses it is `0x1000d029`.

### The eleven returns

`push esi` / `pop esi` around a call is the caller's saved register when the pop is in the caller. Three pushes of values that are still on the stack at the caller's `ret` are the callee's arguments. `0x100a1740` is `ret` with none.

| call | arguments | receiver | what happens to `eax` |
|---|---|---|---|
| `0x1000e809` | none | `[0x100692d0]`, the world. Target `AnnoWelt.dll` `0x100a1740` | overwritten at `0x1000e80f` (`mov eax, [esi]`) with no read. The call is skipped when `0x1000e7f8` (slot `+0xc`) returned null (`0x1000e7fd..0x1000e7ff`). One call. The cursor still moves |
| `0x1002e3f3` | 3 | return of `call [edx+0xf8]` at `0x1002e3d6`. That call's `this` is the return of `call [eax+0x20]` (`0x1002e3d4`). `0x1002e3b4` is `mov ecx, [ecx]`, the object of slot `+0x20`, not of slot `+0xf8`. `0x1001c726` and `0x1001c831` are `call 0x1002e3b0`; the loads are `0x1001c718` and `0x1001c82c` (§9). Not `0x100a1740` | `mov al, 1` at `0x1002e3f9` writes AL only; the other exit is `xor al, al` at `0x1002e3fe` (the `ret 0x10` is at `0x1002e400`). Both callers return that EAX (`ret 8` at `0x1001c72f` in `0x1001c610`, and `ret 8` at `0x1001c83a`). `0x10005230` and `0x10005293` are instructions inside a larger function and return it again (`ret 4`). Bits 8..31 at the next caller were not read |
| `0x1002e4f9` | 3 | `ebx`, from `call [eax+0xf8]` at `0x1002e43f` | `mov al, 1` at `0x1002e502` writes AL only. The only caller, `0x1001bdf7`, tests AL (`0x1001bdfc`) and on this path executes `xor eax, eax` at `0x1001be05` before a use. Not `0x100a1740` |
| `0x1002e63d` | 3 | `[esp+0x14]` at `0x1002e620`. The store into that slot was not walked | `mov al, 1` at `0x1002e646` writes AL only. The only caller, `0x100109e2`, replaces EAX at `0x100109e7` (`mov eax, [esi+0x24]`) with no read. Not `0x100a1740` |
| `0x1002e88e` | 3 | `[esp+0x30]` at `0x1002e883`. The store was not walked | not read. `dec ebp` / `jne 0x1002e879` repeats the call. The last `eax` is still live at `ret 0x18` (`0x1002e8a1`). Not `0x100a1740` |
| `0x1003435b` | 3 | `esi`, from `call [eax+0xf8]` at `0x10034269` | overwritten at `0x10034361` by the loop count. Discarded. Not `0x100a1740` |
| `0x10034471` | none | `[ebx]`, and `ebx` is `ecx` on entry of `0x10034440` (`0x1003444e`). The caller `0x10010f57` sets `ecx` from `[ebp+0x4c]` (`0x10010f53`). The stored pointer is `&player+4` and its first dword is the world (§10). This no-argument slot is `AnnoWelt.dll` `0x100a1740` (`ret` at `0x100a1770`) | pushed at `0x1003447e` into `call [edi+0x7c]` (`0x1003448b`) with two other pushes. Raw value, no mask |
| `0x10034593` | 3 | `esi`, from `call [edx+0xf8]` at `0x100344af` | overwritten at `0x10034599` by the loop count. Discarded. Not `0x100a1740` |
| `0x10035676` | none | `[[esp+0x10]]` (`0x10035669`, `0x1003566d`). The store of that slot was not walked. The call is reached only when `call [eax+0xac]` (`0x1003564b`) returned none of `0x46`, `0x38..0x44`, `0x16` | pushed at `0x10035684` into `call [esi+0x7c]` (`0x1003568b`) with `[esp+0x58]` and the immediate `0x68`. Raw value. Not shown to be `0x100a1740` |
| `0x100357dc` | 3 | `ebx`, from `call [eax+0xf8]` at `0x1003573e` | `mov al, 1` at `0x100357e4` writes AL only. `0x1000fe52`, `0x10010136` and `0x1001043c` replace EAX with no read. `0x1001c454` and `0x1001c4ad` reach `xor eax, eax` at `0x1001c4dd` with no read. `0x10010788` is `mov al, [esi+0x64]` and `0x10010792` is `push eax`; `0x100112f6` is `mov al, [edi+0x68]` and `0x10011300` is `push eax`. Those pushes are arguments of `0x10034100` and `0x10034220`, and the first reads are bytes (§9). Not `0x100a1740` |
| `0x10035cfb` | 3 | `[esp+0x40]` at `0x10035cef`. The walk stopped on `call [edx+4]` at `0x10035cc4`, which moves `esp`; the store of the slot was not read | not read. `dec edi` / `jne` repeats the call (`0x10035d04`). The last `eax` is live at `ret 0x14`. Not `0x100a1740` |

`0x100011d3` is not in the eleven. `0x100011b0` is slot `+0x58` of vtable `0x10063100` (the dword is at `0x10063158`). It does `lea ebx, [esi+4]` (`0x100011bc`), calls `0x10027950` with that pointer as `this`, and on a non-zero return loads `[ebx]` and calls slot `+0xb4` with no stack argument (`0x100011cf..0x100011d3`), then `and eax, 7` (`0x100011d9`). `[object+4]` is the world stored by `0x1002161f`, and world slot `+0xb4` is `0x100a1740`. The classification of this call as `AnnoGame.dll` `0x100023a0` is withdrawn. `0x10027950` does `mov ecx, [ecx]` and then slot `+0xa4`, slot `+8` and slot `+0xc` on that same world pointer.

### The other 32, and Classic

The world PRNG is proved at `0x1000e809`, Classic `0x1000d029`, `0x100011d3`, `0x10002622` and `0x100217eb`. A site that loads `[object+4]`, or `[this]` of a method entered by `lea ecx, [reg+4]` on an object initialised by `0x10021e20` or `0x10021610`, is `0x100a1740` when that dword is the world stored above. The earlier split, world for `0x10021e20` and `0x100023a0` for `0x10021610`, is withdrawn. The class of each remaining call was not walked onto one of those two writers (OQ-290). The expressions actually read:

| call | object expression | result |
|---|---|---|
| `0x1000225e` | `[ebx+4]`, `ebx` = `ecx` on entry. Callers `0x100021f4` and `0x10002215` forward `ecx` and push two dwords; the class was not read. The call itself has no further push. `and eax, 7`, and `ebp` counts down from 4 (`0x10002254`) so the call can repeat | open |
| `0x10002622` | `[ebp]`. In `0x10002520` (slot `+0x38` of `0x10063100`, the dword at `0x10063138`) `ebp` is set by `lea ebp, [esi+4]` at `0x100025b6`, and `esi` is `ecx` on entry. `[object+4]` is the world. No stack argument. `and eax, 3` | `0x100a1740` |
| `0x10007e6d` | `[edi+4]`, `edi` = `ecx` of `0x10007e50`. The `push esi` at `0x10007e64` is popped by the caller at `0x10007e87`. `and eax, 3`, then that many steps along `[esi]` (`0x10007e78`). Conditional on `0x1000c3a0` returning non-null. The only caller of the wrapper is `0x10007ec9`, and that function's callers are `0x10007e20` and `0x10007ffe`; neither was tied to an initializer | open |
| `0x10007ee7` | `[eax+4]` at `0x10007ee2`. `eax` is `[esp+0x10]` (`0x10007ed8`). The walk stopped on `call 0x10007e50` (`0x10007ec9`) because that call moves `esp`; the store of the slot was not read. `and eax, 7` | open |
| `0x100217eb` | `[esi]` in `0x10021770`, which starts after `ret 0xc` at `0x1002176d`. The only caller is `0x10002646`: `mov ecx, ebp` with `ebp` from `0x100025b6`, so `this` is `object+4` and `[esi]` is the world. No stack argument. `and eax, 3`. Registers from `0x10021610` are not live here | `0x100a1740` |
| `0x100289b3`, `0x100289e6` | `[esi]`, `esi` = `ecx` of `0x100289a0`. Callers `0x1004da73` and `0x1004db85`. `0x1004da70` is `lea ecx, [ebp+4]`. `ebp` was not tied to either initializer. Result moved to `ebp` and masked with `0x3f` | open |
| `0x10028b3f`, `0x10028b57` | `[esi]`, `esi` from `ecx` at `0x10028a96`. `and eax, 0x3f` | open |
| `0x10030fbb` | `[esi]`, two reaching defs (`[esp+0x2c]` and `ecx` at `0x10030e70`). `div` | open |
| `0x100341ba`, `0x100343df`, `0x10034614`, `0x1003478e` | `[edi]`, `[ebx]`, `[ebx]`, `[edi]`, each live-in. `div` | open |
| `0x10035419` | `[esi]`. `esi` is `[esp+0x2c]` at `0x100353e6`; the walk stopped on `call [edx+0x140]` (`0x100353d6`). `div` | open |
| `0x1003f00d` | `[ebx]`, live-in. `and eax, 0xf` | open |
| `0x10043f47` | `[ebx+4]`, `ebx` = `ecx` at `0x10043e09`. Result moved to `edi` and masked with 3 | open |
| `0x100488df` | `[esi+4]` after `esi` was loaded from `[esi+4]` (`0x100488d8`, `0x1004889e`). `and eax, 0xf` | open |
| `0x10048b70` | `[[esp+0x20]+4]`. The `esp` def on the straight walk was a `pop`. `and eax, 0xf` | open |
| `0x1004b7ef` | `[esi+8]`. `and eax, 7` | open |
| `0x1004df0f` | `[esi]`. `esi` is `[esp+0x1c]` at `0x1004def8`; the walk stopped on `call 0x1000c0c0` (`0x1004dec7`). `and eax, edi` | open |
| `0x1004e2bf` | `[[esp+0x24]]`. `and eax, 7` | open |
| `0x100517d7` | `[ebx+4]`, `ebx` = `ecx` at `0x100517ba`. `and eax, 7` | open |
| `0x10051b25`, `0x10053849` | `[ebx]`, live-in. `and eax, 7` | open |
| `0x100547ed` | `[edi+4]`, `edi` = `ecx` at `0x100547bf`. `and eax, 0xf` | open |
| `0x10054b98` | `[esi+4]`. `and eax, 7` | open |
| `0x10055041` | `[ebp]`. `and eax, 7` | open |
| `0x10056d96`, `0x10056ef4`, `0x10057013` | `[[esi+0x68]]` or `[[edi+0x68]]`. A store of `+0x68` is `0x1003ee7a`; the value in `eax` there was not read. `and eax, 7` | open |

Classic has the same 39 call encodings, each an instruction. The ten unmasked shapes other than the global have one Classic site each, with the same slot chain: `0x1002bb63`, `0x1002bc69`, `0x1002bdad`, `0x1002bffe`, `0x10031abb`, `0x10031bd1`, `0x10031cf3`, `0x10032dd6`, `0x10032f3c`, `0x1003345b`. Four of those returns are `mov al, 1` (`0x1002bb69`, `0x1002bc72`, `0x1002bdb6`, `0x10032f44`). That writes AL only. Their callers were not read, so those four Classic discards are not proved. `0x10031ac1` and `0x10031cf9` are full `mov eax, [esp+…]` and do replace EAX. The git-ignored scratch script `cache/t44_trace.py` did fold `mov al` into a full EAX write: `PARENT` mapped AL, AX and AH onto EAX, `writes()` treated that as a definition of EAX, and `follow_eax` printed `OVERWRITE`. Its `cache/t44_out.txt` from 2026-10-27 08:44 is retired (round bt F2). Re-read the same day, those four Classic sites and CompLogic `0x1002e3f9`, `0x1002e502`, `0x1002e646` and `0x100357e4` are `mov al, 1`, and `0x10034361`, `0x10034599`, Classic `0x10031ac1` and `0x10031cf9` are `mov eax, [esp+…]`. The script now records `PARTIAL` for an AL/AX/AH access and `OVERWRITE` only for a write of the whole parent. It is not a source of new targets. `0x10034361` and `0x10034599` remain full replacements, and those two discards stand. Classic has no vtable `0x10063100` (§3: no type 17), so `0x100011d3` and the `0x10021610` store have no counterpart there. `0x100217eb` therefore has none either. Matching the remaining masked sites one to one was not done; Classic's list is four calls shorter, and it also has a pair (`0x10051acb`, `0x10051adb`) on one `[edi+4]` where this module's inventory has a single call at `0x100547ed`. Forcing the counts to agree would invent a match.

No name was given to a decision. A mask, a `div`, or what the next instruction does with `eax` is the use. `mov al, 1` leaves bits 8..31 of the return in EAX (the four rows above and OQ-291). OQ-270..272 are untouched.

## 6. How big each class is

**Likely.** Direct `call rel32` targets were followed from each vtable's slots, through the `int3` regions. A callee used only through a register is missing; an immediate the sweep decoded as a call is included. The closure of the base vtable is already 113 regions and about 318 KB of the 398 KB `.text`, so following callees does not separate the classes. The slot lists do.

Slots whose function is on no other vtable in §3:

| class | vtable | slots not shared | where to start |
|---|---|---:|---|
| computer (12, 13) | `0x10063550` | 20 | `+0x14` `0x10041090`, `+0x18` `0x10040b00` (the computer key reader, §11.5), `+0x2c` `0x100497c0`, `+0x34` `0x1004a150`, `+0x38` `0x10041860`, `+0x3c` `0x10043aa0`, `+0x40` `0x100444c0`, `+0x48` `0x10045bb0`, `+0x54` `0x1003f070`, `+0x58` `0x1003ef10`, `+0x5c` `0x100410f0`, `+0x60` `0x10040230`, `+0x64` `0x10040790`, `+0x68` `0x1003fe60`, `+0x78` `0x10049c80`, `+0x7c` `0x10042c10`, `+0x80` `0x10042ba0`, `+0x90` `0x1003f5b0`, `+0x94` `0x1003ed80`, `+0x98` `0x1003edf0` |
| Venetian (16) | `0x10063680` | 11 | `+0x14` `0x1004cfc0`, `+0x18` `0x1004c9a0`, `+0x2c` `0x1004d730`, `+0x34` `0x1004d580`, `+0x38` `0x1004d1a0`, `+0x58` `0x1004c140`, `+0x5c` `0x1004c580`, `+0x60` `0x1004e310`, `+0x84` `0x1004c310`, `+0x94` `0x1004c110`, `+0x98` `0x1004c1f0` |
| type 11 | `0x100637f0` | 10 | `+0x00` `0x1004f050`, `+0x14` `0x10003b30`, `+0x18` `0x1000b200`, `+0x1c` `0x1004eeb0`, `+0x30` `0x1004f0f0`, `+0x38` `0x1004f260`, `+0x58` `0x1004f030`, `+0x5c` `0x100511b0`, `+0x84` `0x1004ee30`, `+0x94` `0x1004ee00` |
| native (14) | `0x100638b0` | 6 | `+0x18` `0x10051da0`, `+0x34` `0x100524d0`, `+0x38` `0x10052390`, `+0x58` `0x100517b0`, `+0x5c` `0x100519d0`, `+0x94` `0x10051780` |
| pirate (15) | `0x10063a00` | 12 | `+0x18` `0x10053d50`, `+0x24` `0x10054710` (the pirate `TYP` override, §11.6), `+0x28` `0x10055230`, `+0x2c` `0x10055a30` (the attack walk, §11.6), `+0x34` `0x10055690`, `+0x38` `0x10054e00`, `+0x4c` `0x100545c0`, `+0x50` `0x100547b0`, `+0x58` `0x100537b0`, `+0x5c` `0x100538a0`, `+0x60` `0x10055eb0`, `+0x94` `0x10053780` |

`int3` regions in `.text`: 256 in `CompLogic.dll`, 257 in `CompLogicClassic.dll`. **Likely** a function count (padding is not proof of one function per region).

The exclusive callee closures, same sweep, are much smaller than the slot lists suggest once shared helpers are removed, and they under-count for the same reason: computer 12 regions, about 48 KB; human 1 region, about 8 KB; pirate 3 regions, about 3 KB; Venetian and native 0. A Venetian-only slot that some other class calls is not in that exclusive set. The slot table is the one to split work by. The closure numbers are only a warning that most of the code is shared.

## 7. Sub-tasks

The behaviour areas in W7 (settling, building, growth, trade, military) are not separated yet. No slot was read far enough to name it as one of those, apart from the two pirate slots and the one computer reader already in §11.6. The split follows the classes, which is what the factory actually builds. Each later task reads its slots and only then says which area a slot belongs to.

| id | what | start here |
|---|---|---|
| T-AI.2 | done for the entry (§4). The nine kind slots and the command-slot question remain OQ-271 | `0x10041860`, then the other eight targets in §4 |
| T-AI.3 | done for the receiver of `0x1000e809`, `0x1000d029`, `0x100011d3`, `0x10002622` and `0x100217eb` (all `0x100a1740`), and for the exclusion of every 3-argument call (§5). The class of the other masked sites is OQ-290; the `+0x4c` / `+0xf8` object is OQ-291. No decision is named | the table in §5 |
| T-AI.4 | the computer class. Settling, building, growth, trade and military are inside these 20 slots until a slot is read and moved out | `0x10041090` and the rest of the computer row in §6. Military already has `0x10043210` and `0x10004df0` from §11.6 |
| T-AI.5 | pirates | `0x10053660`, then the 12 pirate slots. `0x10055a30` and `0x10054710` are the two already named |
| T-AI.6 | Venetians | `0x1004c0c0`, then the 11 Venetian slots, first `0x1004cfc0` |
| T-AI.7 | natives | `0x100516a0`, then the 6 native slots, first `0x10051da0` |
| T-AI.8 | type 11, the factory's other player class | `0x1004ed60`, slot 0 `0x1004f050` |
| T-AI.9 | type 17, present in `CompLogic.dll` only | `0x10001040`, then `0x10002880` |

Classic is not a separate behaviour task. It is the same factory with no type 17 and one fewer slot. A task that changes a shared reading checks the Classic address as it goes.

## 8. Open

- **OQ-240.** Answered in §4. `Advance` `0x100ae0f5` calls holder slot `+0x24` (`0x1009c4d0`), which calls manager slot `+0x14` once.
- **OQ-241.** Narrowed again in §5 (T44 F1) and §9 (T46). `0x1000e809` and Classic `0x1000d029` are the world PRNG, and those two returns are discarded by a later full write of EAX. `0x100011d3`, `0x10002622` and `0x100217eb` are the same function: `[object+4]` is the world for an object initialised by `0x10021610` as well as by `0x10021e20`. Every call that pushes three arguments is not `0x100a1740`. The other masked sites are OQ-290. The live store of field `+0x4c` is §10. The object slot `+0xf8` is called on is OQ-330. The two pushes at `0x10010792` and `0x10011300` are read as bytes (§9).
- **OQ-270.** What `0x1003e1c0` does after the list walk, and whether `0x10003940` changes the dword `0x10003a20` returns (`0x10003ad2` loads `[esp+0x10]`). Next: `0x1003e1c0`, then `0x10003940`.
- **OQ-271.** Whether any of the nine kind slots calls a city command slot or `AnnoFrame.dll` `0x100589a0` / `0x10058e90`. Next: `0x10041860`.
- **OQ-272.** Calls on the object at world `+0x110` (vtable `0x100215a4`, stored at manager `+0x24`). Next: `0x1003d8e0`.
- The holder's vtable is `0x100cb858` (§4). Its RTTI name was not read (`0x1009cb3f`).
- Type 17's vtable (`0x10002880`).
- The 32-slot interfaces (`0x100632e8`).
- Which slot Classic dropped (`0x10060490` against `0x10063550`).
- `user32.dll` at `0x100604c2`.

## 9. The `+0x4c` receiver (T46)

Re-read from `CompLogic.dll` on 2026-10-27. Where a T44 sentence names `0x1001c726` or `0x1001c831` as the load of `[reg+0x4c]`, the bytes differ: those two addresses are `call 0x1002e3b0`. The loads are earlier. Where §5 said the object of slot `+0xf8` is `[ecx]` at `0x1002e3b4`, the bytes differ: that instruction is the object of slot `+0x20`.

**The outer object.** `0x1001c610` begins `mov esi, ecx` (`0x1001c613`). `0x1001c740` begins `mov edi, ecx` (`0x1001c744`). Both are `ret 8`. `0x1001c718` is `mov ecx, [esi+0x4c]` and `0x1001c82c` is `mov ecx, [edi+0x4c]`, each immediately before `call 0x1002e3b0`. `0x10010f53` is `mov ecx, [ebp+0x4c]` and `0x10010f57` is `call 0x10034440`. The same outer object is what `0x1000522e` passes (`mov ecx, esi`) after walking `[esi+0x50]`, and what `0x10005291` passes the same way.

Constructor `0x10016950` is `mov esi, ecx` (`0x10016953`), `xor ebx, ebx` (`0x1001695e`), then `mov dword ptr [esi], 0x100633d0` (`0x100169f0`; the relocation of that immediate is `0x100169f2`). It then stores `ebx` at `[esi+0x4c]` (`0x10016a0f`), so the field is 0. The same function stores `[esi+0x50]`, writes `0xff` to `[esi+0x120]` and `[esi+0x121]` (`mov al, 0xff` at `0x1001698e`), stores the word at `[esi+0x122]`, `[esi+0xfbfc]` and `[esi+0x2134]`. A second write of the same vtable is `0x10016bb6` (`mov esi, ecx` at `0x10016bb1`). The bytes read there do not store `+0x4c`. The table at `0x100633d0` holds 32 code addresses, slot `+0x00` `0x10016b90` through slot `+0x7c` `0x10017720`. The next dword, at `+0x80`, is `0x3d4ccccd`, not a code address. Each slot was disassembled from its entry until a `ret` followed by `0xcc` or `0x90`. Slot `+0x38` (`0x1001cf60`) reaches that `ret` at `0x1001dcee`. The accesses of `[this+0x4c]` in those slots are loads. The stores of displacement `0x4c` use another register: `esi` loaded from a stack slot (`0x100192e9`, `0x10019be9`, `0x10017125`, `0x10016f36`, `0x100173b5`, `0x10017726`), or `edi` at `0x1001d7ec` (`mov [edi+0x4c], edx` after `movsx edx, bp`) while `this` is `esi`. None of those stores is this object's field. The non-zero store of this field was not in the constructor or in those 32 slots. T48 finds it at `0x1005135f`, and the derived object's store at `0x100413d9` (§10). `AnnoWelt.dll` `0x100aae00` was not entered: the object in the field is not shown to be the world.

`0x1002e3b0` (`ret 0x10`, four stack arguments), with `this` equal to the `+0x4c` pointer:

| at the call | what it is |
|---|---|
| `0x1002e3b4` `mov ecx, [ecx]` | first dword of the `+0x4c` object. This is the object of slot `+0x20` |
| `0x1002e3b6` `and edx, 0xff`, then `push edx` and `push 0x1300` | low byte of the third stack argument. At `0x1001c723` that argument is `push eax` after `mov ax, word ptr [esi+0x120]` at `0x1001c71c`, so bits 16..31 are still the high half of the `0x1003d240` return. At `0x1001c82b` it is `push ecx` after `mov cx, word ptr [edi+0x120]`, so bits 16..31 are whatever `0x1003d240` left in `ecx`, not the return (`push eax` at `0x1001c82a` carries the return). The two `and 0xff` reads do not use bits 16..31. The immediate is `0x1300` |
| `0x1002e3c4` `call [eax+0x20]` | two stack arguments. Before those pushes the third argument is at `[esp+0xc]`. Two pushes still on the stack would put its second byte at `[esp+0x15]`. The next instruction is `mov ecx, [esp+0xd]` / `and ecx, 0xff` (`0x1002e3c7`), so the call removed 8 bytes. The callee itself was not read |
| `0x1002e3d4` `mov ecx, eax` and `0x1002e3d6` `call [edx+0xf8]` | `this` is the return of slot `+0x20`. One stack argument, the masked byte |
| `0x1002e3f1` `mov ecx, eax` and `0x1002e3f3` `call [edx+0xb4]` | `this` is the return of slot `+0xf8`, when that return is not zero. Three stack arguments: the caller's first argument, its second argument, and the full dword returned by `0x1003d240` (`0x1001c713` / `0x1001c81e`, `this` = the `+0x4c` pointer, no stack argument). Slot `+0x20`'s callee is `AnnoWelt.dll` `0x100aee00` (§10). The vtable of the `+0xf8` return is OQ-330 |

`0x10034440` takes that same `+0x4c` pointer as `this` (`mov ebx, ecx` at `0x1003444e`). `0x10034466` is `mov ecx, [ebx]` and `0x10034471` is `call [edx+0xb4]` with no push between them. Zero stack arguments, on the first dword of the `+0x4c` object. The shape matches a no-argument slot. §10 names the object as the world and the slot as `AnnoWelt.dll` `0x100a1740`. The return is pushed at `0x1003447e` into `call [edi+0x7c]` (`0x1003448b`), and `edi` is the vtable of `[ebx+4]` (`0x1003446a`, `0x1003446f`). `0x100341ba`, inside the callee of the byte push below, is the same zero-argument slot on `[this]` of `0x10034100`, and that `this` is again the `+0x4c` pointer (`0x100107a1` `mov ecx, [edi+0x4c]`, `0x10034116` `mov edi, ecx`). `0x100341c0` clears `edx`, so the dividend's high half is 0 and its low half is that return. `0x100341c4` is `div dword ptr [esp+0x1c]`. `0x100341ca` is `mov edi, edx`, the remainder. The other three-argument exclusions in §5 stay exclusions.

**The two byte pushes.** `0x10010788` is `mov al, [esi+0x64]`. `0x10010792` is `push eax`. Six pushes follow the byte load, and the call is `0x100107a4` `call 0x10034100` (`ret 0x18`). The last push is the first argument. On entry it is at `[esp+4]`; after `sub esp, 0xac` it is at `[esp+0xb0]`, and `0x10034106` / `0x1003410d` mask that dword to a byte. It is the word pushed at `0x100107a0`, not the EAX from `0x10010792`. The push at `0x10010792` is the fifth argument, at `[esp+0x14]` on entry and at `[esp+0xc0]` after `sub esp, 0xac`. Three register pushes move it to `[esp+0xcc]`. The two pushes of slot `+0x20` are gone by `0x10034125`: that instruction reads `[esp+0xbd]`, the second byte of the first argument, which would sit at `[esp+0xc5]` if those two pushes were still there. The reads at `0x10034147` through `0x1003415e` then use `[esp+0xc0]`, `[esp+0xc4]`, `[esp+0xc8]` and `[esp+0xcc]`, arguments 2 through 5 under the three register pushes alone, so the slot `+0xf8` call has also removed its one push. The fifth argument's first read is `mov cl, byte ptr [esp+0xcc]` at `0x1003415e`. The byte is copied to `[esp+0x12]` and `[esp+0x13]` is set to 0 (`0x10034174`, `0x10034178`). The listing has no second access to offset `0xcc`. `[esp+0xd0]` at `0x100341db` is the sixth argument, the push at `0x1001078e`. Bits 8..31 of the fifth argument are not the byte at `0xcc`.

`0x100112f6` is `mov al, [edi+0x68]`. `0x10011300` is `push eax`. Six pushes, and the call is `0x10011312` `call 0x10034220`, also `ret 0x18`. `0x10034220` reads `[esp+4]` and masks it before `sub esp, 0x604`, so that is the first argument, the word pushed at `0x1001130e`. The push at `0x10011300` is the fifth. After the subtraction and four register pushes it is at `[esp+0x628]`. `0x10034255` reads `[esp+0x619]`, the second byte of the first argument. That byte would be higher by 12 if the two pushes of slot `+0x20` and the one push of slot `+0x70` were still there, so those calls have removed them. The first read of the fifth argument is `mov al, byte ptr [esp+0x628]` at `0x10034298`, then `test al, 1` at `0x100342a6`. The same byte is read again at `0x1003436d`, copied to `[esp+0x1e]`, and `[esp+0x1f]` is set to 0 (`0x1003438b`, `0x1003439e`). `[esp+0x62c]` at `0x10034348` is the sixth argument. The listing does not read `[esp+0x629]`, `[esp+0x62a]` or `[esp+0x62b]`. Bits 8..31 are not those byte reads. Classic was not compared. The type-11 vtable correspondence, and which of the 42 slots Classic dropped, are still open (§3), so slot `+0x5c` was not assumed to be the same method.

The return through `0x10005239` and `0x1000529c` is unchanged. A sweep for the immediate `0x10005230` finds no call, which agrees with those addresses being instructions inside the function that also contains `0x1000522e`, not entries. The caller of that function was not identified.

**Orchestrator review addendum (round bv, 2026-10-27).** The bounded scan above did not cover all creation callers. Fresh reads found `0x10051304` calling `0x10016950`, with its result retained in ESI at `0x10051309`. `0x10051354..0x1005135b` computes `EDI ? EBP : 0` (`mov eax,edi; neg eax; sbb eax,eax; and eax,ebp`), and `0x1005135f` stores that value at `[esi+0x4c]`. `0x10051378` stores ESI at `[edi+0x9b8]`. This identifies a conditional writer; it does not yet prove the value in EBP or its interface. Also, `0x10016950` is used as a base constructor: `0x1000f983` calls it, then `0x1000f98a` replaces the vtable with `0x100632e8`. The field investigation must include derived objects, not only objects still holding `0x100633d0`. OQ-310 and T48 start with the concrete writer and creation provenance. The byte-reader conclusions above remain unchanged.

## 10. The writer of `+0x4c` (T48)

Re-read from `CompLogic.dll` and `AnnoWelt.dll` on 2026-10-27. `CompLogic.dll`, `CompLogicClassic.dll`, `AnnoWelt.dll` and `AnnoFrame.dll` all have image base `0x10000000`. The store at `0x1005135f` is in `CompLogic.dll` (`89 46 4c`). The same virtual address in the other three modules is different bytes. `AnnoWelt.dll` `0x100aae00` was not entered.

**Which object executes the store.** `0x100511af` is a `nop`. The function is `0x100511b0` (`push -1`, `ret 0x10` at `0x1005155d`). Its only relocation is `0x1006384c` in `.rdata`. That is slot `+0x5c` of the type-11 vtable `0x100637f0` (`0x100637f0 + 0x5c`). A code sweep finds no direct call. `this` is `ecx`.

**EBP and EDI.** `0x100511e2` is `mov edi, ecx`. `0x100511f0` is `lea ebp, [edi + 4]`. No instruction in this function writes either register again before the store. The next write of EDI is `xor edi, edi` at `0x100514c3`, after both paths have passed the store. EBP is popped at `0x10051551`. The calls between the `lea` and the store (`0x100208c0`, `0x10020880`, `0x10024fe0`, `0x10059c6f`, `0x10016950`, `0x100037f0`) are not naked and do not change the two registers inside this function. The stored value is not one of the four stack arguments (`ret 0x10`).

**The bytes at the store.** They are not four contiguous instructions ending at `0x1005135b`:

| address | instruction |
|---|---|
| `0x10051354` | `mov eax, edi` |
| `0x10051356` | `neg eax` |
| `0x10051358` | `sbb eax, eax` |
| `0x1005135a` | `push edx` |
| `0x1005135b` | `and eax, ebp` |
| `0x1005135d` | `mov ecx, edi` |
| `0x1005135f` | `mov dword ptr [esi + 0x4c], eax` |

`neg` / `sbb eax, eax` is `0` when EDI is `0` and `0xffffffff` otherwise. The `push` does not change EAX. The stored value is `edi ? ebp : 0`, which is `&player+4` when the type-11 object is not null. ESI is the object `0x10051304` built with `call 0x10016950` and `0x10051309` kept in ESI. That constructor still writes vtable `0x100633d0` and `0` at `+0x4c`. This function writes no vtable immediate between that call and the store. `0x100037f0` (`ret 0x18`) is called with the player as `this` and the new node as its first argument. `0x1000385c` puts a different allocation in ESI, `0x1000386e` stores EBX there, and `0x100038af` links that pointer at `[player+0x9a0]`. It does not write the node's first dword. The caller's ESI is callee-saved, so the node still holds `0x100633d0` at the store. `0x10051378` is `mov dword ptr [edi + 0x9b8], esi`.

**What `&player+4` holds.** Type 11's constructor `0x1004ed60` calls `0x10002880` (`0x1004ed64`). That does `lea ecx, [esi + 4]` (`0x100028a0`) and `call 0x10020230`. `0x10020242` stores `0` at `[ecx]` and `0x10020244` stores `0` at `[ecx + 4]`, after `xor eax, eax` at `0x10020235`. `0x100028ae` is `xor ebx, ebx`. `0x10002abe` stores that zero at `[esi + 0x9b8]`. The factory then overwrites the first dword. Type 11 joins it by `jmp 0x1003dd54` at `0x1003dcaf`, after `call 0x1004ed60`. Type 12 does the same with `call 0x1003ec80` (`0x1003dcd9`) and `jmp 0x1003dd54` (`0x1003dcde`). At the join, `0x1003dd6c` loads `[manager + 0x24]` into EDX and `0x1003dd73` loads `[manager + 0x20]` into ECX. The three pushes are the shifted EAX, then ECX, then EDX, and `0x1003dd79` is `lea ecx, [edi + 4]` before `call 0x10021e20`. In `0x10021e20`, EAX is the first stack argument (`mov eax, [esp + 4]` before `sub esp, 0xa4`). EDI is the second: three callee-saved pushes after `sub esp, 0xa4` are `0xb0` bytes, and `mov edi, [esp + 0xb8]` is original `[esp + 8]`. `0x10021e34` is `mov esi, ecx`. `0x10021e38` stores EDI at `[esi]` and `0x10021e3a` stores EAX at `[esi + 4]`. So `[player + 4]` is manager `+0x20`, and `[player + 8]` is manager `+0x24`.

Manager slot `+0xc` is `0x1003df00`. Seven pushes precede `0x1003df2c`, so `[esp + 0x20]` is the first argument and `[esp + 0x24]` is the second. `0x1003df34` stores the first at `[ebx + 0x20]` and `0x1003df37` stores the second at `[ebx + 0x24]`. The caller is holder slot `+0x14`. The holder constructor `0x1009cac0` is `mov edx, ecx` (`0x1009cac1`). It writes `0x100cb888` at `0x1009cad3`, then `0x1009cb3f` writes `0x100cb858` on that same EDX, and `0x1009cb4e` returns it. `0x100cb858 + 0x14` is `0x100cb86c`, and that dword is `0x1009c630`. Inside it, `0x1009c71a` loads `[esi + 0x90]` into EAX and `0x1009c720` pushes it and `0x1009c719` pushes the return of slot `+0x18` on that same pointer (`0x1009c70e`). `0x1009c721` calls manager slot `+0xc`. World `Init` is slot `+0x50`: the constructor stores vtable `0x100ccdd8` at `0x100af442`, and `0x100ccdd8 + 0x50` is `0x100cce28`, which holds `0x100aa910`. A code sweep finds no direct call. `0x100aa932` is `mov esi, ecx`. `0x100aa97f` pushes ESI and `0x100aa982` calls `0x1009c190` with EDI, the object `0x1009cac0` returned, as `this`. `0x1009c196` reads `[esp + 0x14c]` after `sub esp, 0x148`, which is the first argument, and `0x1009c1a3` stores it at `[ebx + 0x90]`. The constructor's earlier store of EDI at `[edx + 0x90]` (`0x1009cb24`) is before that. The value the loader pushes is therefore `Init`'s `this`, the world. `[player + 4]`, the first dword of the stored `&player+4`, is that world.

**Derived object.** `0x1000f980` is `mov esi, ecx`, `call 0x10016950` (`0x1000f983`), then `mov dword ptr [esi], 0x100632e8` (`0x1000f98a`). It does not store `+0x4c`, so the base constructor's `0` remains until a later writer. Its only caller is `0x1004127f`, inside `0x100410f0`. That entry's relocation is `0x100635ac`, slot `+0x5c` of the type-12 vtable `0x10063550`. `0x1004112a` is `mov ebp, ecx`. `0x1004113c` is `lea ebx, [ebp + 4]`, and nothing in the function writes EBX again before the store. `0x10041284` keeps the `0x1000f980` return in ESI. The two calls to `0x100037f0` before the store are the same function as above and do not write that ESI, so the vtable remains `0x100632e8`. The store is the same idiom, with the `push` after the `and`:

| address | instruction |
|---|---|
| `0x100413b7` | `mov eax, ebp` |
| `0x100413cb` | `neg eax` |
| `0x100413cd` | `sbb eax, eax` |
| `0x100413d6` | `and eax, ebx` |
| `0x100413d8` | `push edx` |
| `0x100413d9` | `mov dword ptr [esi + 0x4c], eax` |

`0x1004149f` loads the old `[ebp + 0x9b8]`, `0x100414a9` stores it at `[esi + 0x50]`, and `0x100414b0` stores ESI at `[ebp + 0x9b8]`. Other stores of `+0x9b8` (`0x10004334`, `0x10004389`, `0x1000bcd3`, `0x1004c728`, `0x10051b90`, `0x10053a49`) were not entered.

**The two `0x1002e3b0` callers are the type-12 list, not the type-11 writer.** `0x100051f1` and `0x10005254` load `[esi + 0x9b8]` and follow `[node + 0x50]`. Both sites compare `[player + 0xa18]` with `0xb` and jump to the epilogue (`0x100051e2`, `0x10005245`), then require `0xc` (`0x100051e8`, `0x1000524b`). Type 11 does not reach `0x1000522e` `mov ecx, esi` / `call 0x1001c610`, or `0x10005291` / `call 0x1001c740`. Those are callers of the two wrappers; the direct calls to `0x1002e3b0` are at `0x1001c726` and `0x1001c831`. `0x1001c613` is `mov esi, ecx` and `0x1001c744` is `mov edi, ecx`. The loads named in §9 were re-read: `0x1001c718` is `mov ecx, [esi + 0x4c]` and `0x1001c726` is `call 0x1002e3b0`; `0x1001c82c` is `mov ecx, [edi + 0x4c]` and `0x1001c831` is the same call. Each function also loads `+0x4c` once earlier, at `0x1001c710` and `0x1001c81b`, as `this` of `0x1003d240`. The node those calls receive is the derived object published at `0x100414b0`. Its `+0x4c` was written at `0x100413d9`, not at `0x1005135f`. The type-11 publication at `0x10051378` has no proved reader in these two functions. Other loads of `+0x9b8` were not entered; the first outside this section's functions is `0x100030e7`.

**`0x10010f53` is the derived object.** It sits in `0x10010d80` (`sub esp, 0x90` at the entry, `mov ebp, ecx` at `0x10010d90`, `ret 4` at `0x10010e88` and again at `0x10011025`; the earlier returns do not end the function). The relocation is `0x10063358`, slot `+0x70` of `0x100632e8`. A code sweep finds no direct call. EBP is that method's `this`, so the receiver runs when the derived vtable is current. It does not run on an object that still holds `0x100633d0`. The creation path is `0x1000f980`, and the store it relies on is `0x100413d9`.

**Downstream, after the pointer is proved.** `0x1002e3b0` was re-read. `0x1002e3b4` `mov ecx, [ecx]` is the first dword of `&player+4`, the world, and it is the object of slot `+0x20`. `0x1002e3be` pushes the masked byte and `0x1002e3bf` pushes `0x1300`. `0x1002e3c4` is `call [eax + 0x20]`. World vtable `0x100ccdd8` slot `+0x20` is the dword at `0x100ccdf8`, `AnnoWelt.dll` `0x100aee00`, `ret 8` at `0x100aee26`, `0x100aee34`, `0x100aee3d`, `0x100aee49` and `0x100aee52`. The first argument is compared with `0x1300` at `0x100aee14`. That case, `0x100aee1b`, returns `[world + second_argument*4 + 0x6530]`. The second argument is the masked byte. `0x1002e3d4` `mov ecx, eax` and `0x1002e3d6` `call [edx + 0xf8]` pass that return as `this`, with the masked byte as the one stack argument. `0x1002e3f1` `mov ecx, eax` and `0x1002e3f3` `call [edx + 0xb4]` pass the `+0xf8` return as `this` when it is not zero, with three stack arguments. The vtable of the array element is settled in §11: a non-null slot inserted by the callers of `0x100a19a0` holds the island `0x10094c60` builds, vtable `0x100cac2c`. OQ-330 records that. The three-argument slot is not `0x100a1740`.

`0x10034440` was re-read from the derived receiver. `0x1003444e` is `mov ebx, ecx` (`this` is the `+0x4c` pointer). `0x10034458` reloads the first dword, `0x1003445b` pushes the masked byte, `0x1003445c` pushes `0x1300`, and `0x10034463` calls slot `+0x20` on it. The same `0x1300` case applies. `0x10034466` loads the first dword again, not the return (`0x10034468` keeps the return in ESI). `0x1003446d` loads its vtable and `0x10034471` is `call [edx + 0xb4]` with no push between those two instructions. World slot `+0xb4` is the dword at `0x100cce8c`, `0x100a1740`. That function's `ret` at `0x100a1770` pops nothing of the caller's arguments. It reads the word at `[world + 0xfc]`, divides the incremented word by the word at `[world + 0xfe]` (`xor edx, edx` at `0x100a1753`, `div esi` at `0x100a1756`), writes the remainder back to `[world + 0xfc]` (`0x100a175b`), and returns the word at `[remainder*2 + 0x100cb8f8]` (`0x100a1768`). EAX was cleared at `0x100a1758`, so the high half of the return is `0`. This write is a world-field update on the no-argument path. It is not the three-argument slot.

The byte pushes at `0x10010792` and `0x10011300` were not reopened. OQ-290 and OQ-270..272 stay open. OQ-310 is the provenance above. OQ-331..339 were not used.

**Orchestrator review addendum (round bw, 2026-10-27).** Existing `docs/sim/state.md` S14 island mapping already identifies world insertion `0x100a19a0` and its `+0x6530` table; fresh reads confirm the store at `AnnoWelt.dll` `0x100a19d4`. Existing island/city evidence also identifies `0x100863e0`: its bytes return `[island + argument*4 + 0x38]` and `ret 4`. Fresh table reads confirm `0x100cac2c + 0xf8 = 0x100863e0` and `0x100c2f40 + 0xb4 = 0x10028fd0`, the RemoveAt function already covered by `transport.md` §3.4.17. These are reusable evidence. The T49 section below is the creation, publication and argument reconciliation. OQ-330 records the result.

## 11 The three-argument call is RemoveAt on the published city (T49, 2026-10-27)

AnnoWelt unless a module is named. `CompLogic.dll` and `AnnoWelt.dll` both prefer image base `0x10000000`. `CompLogic.dll`'s `SizeOfImage` is `0x72000`, so `0x100aee00` is not in that image. At `0x1002e3f3` CompLogic is `call dword ptr [edx + 0xb4]` (`ff 92 b4 00 00 00`); AnnoWelt at that address is `c0 74 06 88 85 d8`. Listings stayed in this worktree's `cache/t49/` and are not committed.

**The island in the world table.** `0x100a19a0` (`ret 4` at `0x100a1a9e`) takes the object in its one argument (`mov esi, [esp + 0x20]` at `0x100a19a6`; `this` is `edi`). `0x100a19b1` calls that object's slot `+0x40`. The low byte of the return is the index (`mov byte ptr [esp + 0x24], al` at `0x100a19ba`, `and eax, 0xff` at `0x100a19c2`). `0x100a19d4` stores the object at `[world + index*4 + 0x6530]`. If the index is not below `[world + 0xf4]`, `0x100a19ce` raises that dword to `index + 1`.

World slot `+0x1c4` is `0x100aed70` (`ret 8` at `0x100aedee`; vtable `0x100ccdd8`). It allocates `0xcc` bytes (`0x100aed88`). A null allocation returns null (`0x100aedb2`). Otherwise `0x100aeda9` calls `0x10094c60`, which writes vtable `0x100cac2c` at `0x10094c68` and returns the same pointer (`0x10094c6e`). On the in-game vtable, slot `+0x40` is `0x1008c690`: `xor eax, eax`; `mov ax, [ecx + 0x98]`; `ret`. The insertion index is the low byte of that word.

A code sweep finds five calls of `0x100a19a0`. `0x100a1f21`, `0x100a2044` and `0x100a2623` each call slot `+0x1c4` and skip the insertion when the return is null (`0x100a1f2b`, `0x100a204e`, `0x100a262d`). `0x100a2f6a` calls world slot `+0x88` (`0x100a24d0`), whose body contains the `0x100a2623` call, and inserts that return when it is not null (`0x100a2f74`). `0x100af2da` copies each non-null pointer already in the table (`lea ebx, [esi + 0x6530]` at `0x100af2cf`) and `0x100af3bf` inserts those same pointers. The non-null pointer stored for the AI's key is the object `0x10094c60` returned. `0x100aee00`'s `0x1300` case (`0x100aee1b`) returns that slot and does not test it. `0x1002e3cb` then loads `[eax]`. Two other instructions write an immediate 0 at displacement `0x6530` (`0x100a29e6`, base `ebx` from `[esp + 0x10]`; `0x100a2cf2`, base `ebp`). Those bases were not followed back to the world in this pass.

**The city in the island ring.** Island vtable `0x100cac2c` slot `+0xf8` is `0x100863e0` (the dword at `0x100cad24`): `mov eax, [ecx + eax*4 + 0x38]`; `ret 4`. Slot `+0xf4` of the same vtable is `0x100861c0` (the dword at `0x100cad20`). `0x100861c6` is `mov edi, ecx` and `0x100861d5` is `lea ecx, [edi + 0x38]` before `call 0x100967e0`.

`0x100967e0` keeps `this` in `edi` (`0x1009680c`). A negative index searches sixteen slots for a null pointer and wraps (`cmp eax, 0x10` at `0x10096822`, `sub ecx, 0x40` at `0x1009682a`). A non-negative index is used as it stands, with no compare against 16 on that path (`jge 0x10096852` at `0x10096814`). `0x10096874` calls `0x1002f990` on a `0x30ec`-byte allocation. A null allocation jumps to the failure return before the store (`0x1009688e`). `0x1002f990` writes vtable `0x100c3058` at `0x1002f999` and vtable `0x100c2f40` at `0x1002fa36`. The jumps between those two stores are the fill loops back to themselves. `0x1002fb84` is `mov eax, ebp` and `0x1002fb88` is `ret`. `0x100968a4` stores that pointer at `[edi + ebx*4]`, which is `[island + index*4 + 0x38]`. `0x100968a7` stores the index at `[edi + 0x40]`. City vtable `0x100c2f40` slot `+0xb4` is `0x10028fd0` (the dword at `0x100c2ff4`), RemoveAt (`transport.md` §3.4.17). The ring tick `0x100969d0` reads the same sixteen pointers at `[edi + cursor*4]` and compares the cursor with `0x10` (`0x10096a2b`).

The AI path calls `+0xf8` on the table element and `+0xb4` on that return. For a non-null element inserted by the calls above, the element is the `0x100cac2c` island, `+0xf8` is `0x100863e0`, and the pointer at the indexed ring slot is the object `0x1002f990` returned when that index was published. `0x1002e3dc` skips `+0xb4` when the ring slot is null (`je 0x1002e3fe`, and `0x1002e3fe` returns 0). The call does not compare the vtable.

**Which byte selects the slot.** `0x1002e3b0` is `ret 0x10`. Its third stack argument is masked with `and edx, 0xff` (`0x1002e3b6`) and passed to world slot `+0x20`. `0x1002e3c7` reads a dword at `[esp + 0xd]`, one byte into that same argument, and `0x1002e3cd` keeps the low byte for slot `+0xf8`. Both wrappers push the word at `[node + 0x120]` in the low half of that argument (`mov ax, [esi + 0x120]` at `0x1001c71c`, then `push eax`; `mov cx, [edi + 0x120]` at `0x1001c823`, then `push ecx`). The island index is bits 0..7 of that word. The city index is bits 8..15. On the `0x1001c610` path the high half of the pushed dword is the high half of the `0x1003d240` return, because `mov ax` leaves bits 16..31 in place. On the `0x1001c740` path that high half is whatever `0x1003d240` left in `ecx`. Neither `and 0xff` reads it. The constructor cited in §10 writes `0xff` to `[node + 0x120]` and `[node + 0x121]`. The writer is identified in the review addendum below; the source and validity of the pair remain OQ-340.

**Review addendum (round bx, 2026-10-27): the index writer.** In the type-12 slot `+0x5c` function `CompLogic.dll` `0x100410f0`, `0x10041329` writes `AX` to `[esi + 0x120]`, before publication at `0x100414b0`. `0x1004131a` loads `EAX` from `[esp + 0x8c]` while the `push 0x1420` at `0x1004130f` is outstanding. The entry's three SEH pushes, `sub esp, 0x68`, and four saved registers make the baseline displacement of argument 1 `+0x88`; that outstanding push makes it `+0x8c`. Thus the store copies the low 16 bits of the first entry argument. `0x10041321` and `0x10041325` also copy `AX` into the two records' `+0x34` fields. This identifies the store, not the caller's range or non-null guarantees. OQ-340 now follows the producer of that first argument.

**T50: the producer of that argument.** `0x100410f0` has no direct caller. Its only reloc is `0x100635ac`, slot `+0x5c` of the type-12 vtable `0x10063550`. The call that reaches it is inside `0x10004e60`, which is slot `+0x10` of that same vtable (the dword at `0x10063560`). `0x10004e60` is `push ebx` at `0x10004e60`, `mov ebp, [esp + 0xc]` at `0x10004e62`, then `push esi` and `push edi`. `ebp` is the one stack argument, the record. `esi` is `this`. `0x10004e6a` sign-extends the word at `[ebp]`, subtracts 2, and `0x10004e8c` jumps through `0x10005310`. The dword at index 2 is `0x10004e93`, so the arm runs when that word is 4. The same function is also slot `+0x10` of other player vtables (`0x10063690`, `0x10063800`, `0x100638c0`, `0x10063a10`, and two further relocs at `0x10063110` and `0x100631e0`). Only the type-12 vtable's `+0x5c` is `0x100410f0`, so only that class stores the pair from this arm. This arm does not compare `[player + 0xa18]` with `0xc`.

At delta `0x10`, `0x10004e7e` and `0x10004e82` write `[ebp + 0x14]` and `[ebp + 0x15]` into the low two bytes of `[esp + 0x14]`. That slot is the record pointer (`[E + 4]`). Bits 16..31 of the pointer stay. In the word-4 arm, `0x10004e98` sign-extends the word at `[ebp + 4]` and `0x10004e9e` sign-extends the word at `[ebp + 2]`. After the first two pushes the delta is `0x18`, and `0x10004ea2` loads `[esp + 0x1c]`, which is the same overwritten slot. The four pushes are that dword, then the word at `+2`, then the word at `+4`, then `ecx`. `0x10004e93` wrote only `cl` from `[ebp + 0x16]`; the earlier `mov cl` at `0x10004e71` is the only other write of `ecx`, so the upper bits of the fourth push are still the upper bits of `this`. `0x100410f0` is `ret 0x10`, which matches the four pushes. Its store at `0x10041329` keeps `AX` only, so the stored low byte is the record byte at `+0x14` and the stored next byte is the record byte at `+0x15`. The leftover high half of the record pointer is not stored. The later readers still mask bits 0..7 as the island index and bits 8..15 as the city index (`0x1001c71c` `mov ax, [esi + 0x120]`, re-read; `0x1001c823` `mov cx, [edi + 0x120]`, re-read). Publication of the created node is still `0x100414b0`.

This arm does not compare either byte with an island count or with 16. `0x100208c0`, called with argument 1, does `and edx, 0xff` and calls world slot `+0x20` with key `0x1300`, then `0x100208da` loads `[esi]` with no null test. The city byte is compared with existing nodes' `+0x121` and stored; it is not compared with 16. `[this + 0x9b4] >= 8` returns 0 before the store (`0x10041251`). A matching node jumps to `0x1004158b` and does not create another. A null allocation returns 0. A null record is not tested; the arm reads `[ebp + 0x14]` directly. The function does not keep the record pointer. Who writes the record, including bytes `+0x14` and `+0x15`, is the caller of slot `+0x10`. A code sweep of `0x10004e60` finds no direct call. Next address: that virtual call on the type-12 object. The two bytes are not shown to be in range or to name a live island and city. The fourth argument's use inside `0x100410f0` was not ledgered; later calls sit between the entry and `0x10041638`.

**The three arguments of `0x1002e3f3`.** After `+0xf8` returns, the stack is the entry stack. `0x1002e3e0` loads argument 4, `0x1002e3e7` loads argument 2, and `0x1002e3ec` loads argument 1. The three pushes are argument 4, then argument 2, then argument 1, so RemoveAt's first argument is argument 1, its second is argument 2, and its third is argument 4.

Both wrappers are `ret 8`. `0x1001c634` / `0x1001c630` load their two stack arguments into `ebx` and `edi`, and `0x1001c725` / `0x1001c724` push them as arguments 1 and 2. `0x1001c760` / `0x1001c764` do the same for `0x1001c740`, pushed at `0x1001c830` / `0x1001c82f`. The only callers are `0x10005230` and `0x10005293`, inside `0x10004e60` (`push ebx` at `0x10004e60`, `mov ebp, [esp + 0xc]` at `0x10004e62`: `ebp` is the one stack argument). `0x10005224` and `0x10005287` sign-extend the word at `[ebp + 4]`. `0x10005228` and `0x1000528b` sign-extend the word at `[ebp + 2]`. Those two dwords are RemoveAt's first and second arguments, in the order `(word at +2, word at +4)`.

`0x10004e6a` sign-extends the word at `[ebp]`, subtracts 2, and rejects a result above `0x18` (`0x10004e7b`). `0x10004e8c` jumps through the dword table at `0x10005310`. Index 13, the dword at `0x10005344`, is `0x100051d9` and runs when the word is 15. Index 14, the dword at `0x10005348`, is `0x1000523c` and runs when the word is 16. §10 already limits both arms to `[player + 0xa18] == 0xc`.

Argument 4 is the full dword returned by `0x1003d240` (`push eax` at `0x1001c71b` and at `0x1001c82a`). `0x1003d240` loads the world (`mov ecx, [ecx]` at `0x1003d240`), passes key `0x23` and a `0xa0`-byte buffer to world slot `+0x48`, and returns the dword at `[buffer + 4]` (`mov eax, [esp + 4]` at `0x1003d254` after the callee's `ret 8`). Slot `+0x48` is `0x100aeeb0` (the dword at `0x100cce20`). Key `0x23` has index `0x15` in the byte table at `0x100af038`; that byte is 4, and index 4 of the table at `0x100af018` is `0x100aeed0`. That case writes `[world + 0x10c]` to `[buffer + 4]` (`0x100aeed0`, `0x100aeeda`) and returns 1 with `ret 8`. Advance `0x100ae040` writes that field at `0x100ae07b` from `0x100b6a80` with `ecx = 1`. For a shift below 32, `0x100b6a8a` is `shrd eax, edx, cl` and `0x100b6a8d` is `shr edx, cl`, applied to the 64-bit value at world `+0x100`. `docs/sim/state.md` names the stored low half the world tick. RemoveAt's third argument on this path is that dword.

**What RemoveAt does with them.** Re-read of `0x10028fd0`: `0x10028fd0` loads the first argument into `edx`, and `0x10028fdd` loads the second into `edi`. `0x10028fed` calls `[[city + 0x1c] + 0x94]` with that pair. A null return jumps to `0x1002945c` (`0x10028ff7`) and does not reach the clear. `0x10029011` calls `[edx + 0x5c]` on the lookup return (`ebp`), before the clear. `0x100292e4` pushes 0, then the second argument, then the first (`0x100292d8`, `0x100292e7`), and `0x100292ef` calls `[[city + 0x1c] + 0x7c]`. `0x100292f2` is `mov edx, [ebp]`. `0x100292f7` calls `[edx + 0x5c]` again. The clear therefore uses the two record words and a literal 0. Other reads of `[esp + 0x2f8]` in this function (`0x100290cb`, `0x1002922e`, `0x100292ba`, `0x10029409`) sit after virtual calls whose stack cleanup was not measured, so they are not assigned to the third argument here (OQ-341). T50, in the next paragraph, assigns `0x100290cb`. The bookkeeping between the two `+0x5c` calls is the one `transport.md` §3.4.17 already records; this note does not add it to the sim.

**T50: which of those reads is argument 3.** Entry delta after `push ebp` / `push esi` / `push edi` is `0x2e8`: argument 1 at `[esp + 0x2ec]`, argument 2 at `[esp + 0x2f0]`, argument 3 at `[esp + 0x2f4]`. Island slot `+0x94` is `0x1008cad0` on both `0x100cac2c` and `0x100ca19c`, `ret 8`, two pushes, net 0. A null return still jumps to `0x1002945c` and does not read `[esp + 0x2f8]`. `push ebx` at `0x10029002` makes the delta `0x2ec`, so argument 3 is at `[esp + 0x2f8]` until a call changes the stack. Island slot `+0x18` is `0x1008c680` on both vtables: `mov eax, [ecx + 0x20]`; `ret`. No push, net 0. The following `call [edx + 0x5c]` pushes nothing. Its target is AnnoGame `0x10007d20` (`xor eax, eax`; `mov ax, [ecx + 0x10]`; `ret`) when the lookup return has one of the three type vtables whose `+0x5c` relocates there (`0x1002185c`, `0x10021a88`, `0x10021bbc`). That is net 0 for those vtables. A different `+0x5c` stops the ledger at `0x10029011`. Island slot `+0x90` is `0x100839a0` on both island vtables; every `ret` in it is `ret 0x10`, and the call pushes four dwords, net 0.

On the path where the PRODTYPE test leaves `ebx` nonzero, the object at `city + 0x144` has vtable `0x100c2e48` (the store at `0x100269bd`; `0x100c2e80` at `0x10026874` is the base written earlier in that constructor). Its slot `+8` is `0x10023c80`, every `ret` is `ret 0xc`, and the call pushes three dwords, net 0. The returned object's slot `+0x104` is `0x10072cf0` (`ret 4`) on the building vtables that relocate to it, including `0x100c92b8`, `0x100c9958`, `0x100c8990`, `0x100c8010`, `0x100c86c8` and `0x100c8d60`. The call pushes one dword, net 0 for those classes. Another `+0x104` stops the ledger at `0x100290b2`.

Under that prefix the delta is still `0x2ec` at `0x100290bc`, so `[esp + 0x2f4]` is argument 2. `push 1` and `push` of that value make the delta `0x2f4`, and `[esp + 0x2f8]` at `0x100290cb` is argument 1. It is pushed as the first stack argument of island slot `+0x150` (`0x10086c70` on `0x100cac2c`), with argument 2 and the literal 1 above it. That read is the x coordinate, not the third argument. No instruction in this measured prefix reads the slot that still holds argument 3.

The other three reads stay unassigned. `0x1002922e` and `0x100292ba` are in the block at `0x10029201`. The `ebx == 0` path enters it through `call [edx + 0x48]` at `0x1002920d` on `[city + 0x20]`, two pushes, and that `ret` was not read. The constructor stores 0 there (`xor ebx, ebx` at `0x1002f9d0`, `mov [ebp + 0x20], ebx` at `0x1002fa40`). The later writer was not read. The `ebx != 0` path reaches `0x10029201` only after island slot `+0x150` at `0x100290d3`, whose `ret` was not read, and after further virtual calls. `0x10029409` is after both of those and after the second `+0x5c` jump table. The ledger stops at `0x1002920d` on the zero-`ebx` path and at `0x100290d3` on the other path. This does not show that the third argument is unused. The clear's literal 0 is unchanged; this pass does not re-ledger the clear across those unread calls, and `transport.md` was not edited.

**When the call runs.** In `0x1001c610`, `0x1002d720` returning non-zero jumps to the epilogue (`0x1001c651`). The early returns at `0x1001c6c8` and `0x1001c6e5` leave before `0x1002e3b0`. The call block requires the byte at `[node + 0xfbfc]` to be 0 (`0x1001c6f6`) and `0x10022b00` to return non-zero (`0x1001c70e`). In `0x1001c740` the call block is the path taken when `0x10024370` returns 0 (`je 0x1001c7fd` at `0x1001c75a`); a non-zero return goes to the slot loop, which returns at `0x1001c7dd` or `0x1001c7fa` or to the epilogue at `0x1001c836`. The call then also requires `0x10022b00` to return non-zero (`0x1001c819`).

**The dispatch observed on this path.** `0x10004e60` switches on the record word. The two arms above are that switch's cases 15 and 16, and §10 already requires type 12. This pass found no call from these arms to `AnnoFrame.dll` `0x100589a0` or `0x10058e90`. The world change this path can reach is the `+0xb4` call above, and only when the island slot and the city slot are both non-null and RemoveAt's lookup returns non-null.
