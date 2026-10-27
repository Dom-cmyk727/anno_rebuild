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

`fn` is the start of the `int3`-padded region that contains the call. A region can hold more than one logical function; `0x10034162` in particular holds eight of these calls.

| call | region | use of the result |
|---|---|---|
| `0x100011d3` | `0x10001000` | `and 7` |
| `0x1000225e` | `0x10001000` | `and 7`, then a table of 8 at `0x10065300` / `0x10065320` |
| `0x10002622` | `0x1000247f` | `and 3` |
| `0x10007e6d` | `0x10007985` | `and 3` |
| `0x10007ee7` | `0x10007985` | `and 7`, then a table of 8 at `0x100654c4` / `0x10065484` |
| `0x1000e809` | `0x1000e381` | not masked in the next 8 instructions |
| `0x100217eb` | `0x100216c0` | `and 3`, then a float table |
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
| `0x1003de4f` | `0x10021610` (three pushes, no frame, `ret` not re-read) | `[esp+0x14]` is the first argument, manager `+0x24` | `0x1002161f` |

`0x1003de1f` calls `0x10001040` and `0x1003de24` keeps the result in `edi`. `0x1003de26..0x1003de34` does not write `edi`, and `0x1003de34` skips the call when `edi` is 0. `0x10001040` is the type-17 constructor: it calls `0x10002880` and stores vtable `0x10063100` (`0x100010bf`). Manager `+0x24` is the slot's second argument (`0x1003df37`). §4 reads that as `[world+0x110]`, vtable `0x100215a4` for the one `AnnoScene` caller it followed. The dword at `AnnoGame.dll` `0x100215a4+0xb4` is `0x100023a0`. `0x100023a0` was not entered past its frame. A second `Init` caller is the open note in §4, not a new guess.

`0x10021e38` then calls slot `+0x54` on the stored pointer (`0x10021e3d`), which §4 has as the world getter `0x100a3910`. So the factory path `0x1003dd79` really does store the world at `[object+4]`. The type-17 path stores the other pointer.

Classic's copy of the global is `0x10066248`. The only store is `0x1000b31c`, in `0x1000b2f0`, the same frame as `0x1000cab0`, also writing `[this+0x1d14]`. The only caller is `0x1003b4c3`. The function starts at `0x1003b3a0`: `esi` is the first argument (`0x1003b3cc`), stored at `+0x20` (`0x1003b3d4`), and is the object of slot `+0xa4` (`0x1003b3de`). The call pushes `esi` last. Same proof. The call that uses it is `0x1000d029`.

### The eleven returns

`push esi` / `pop esi` around a call is the caller's saved register when the pop is in the caller. Three pushes of values that are still on the stack at the caller's `ret` are the callee's arguments. `0x100a1740` is `ret` with none.

| call | arguments | receiver | what happens to `eax` |
|---|---|---|---|
| `0x1000e809` | none | `[0x100692d0]`, the world. Target `AnnoWelt.dll` `0x100a1740` | overwritten at `0x1000e80f` (`mov eax, [esi]`) with no read. The call is skipped when `0x1000e7f8` (slot `+0xc`) returned null (`0x1000e7fd..0x1000e7ff`). One call. The cursor still moves |
| `0x1002e3f3` | 3 | return of `call [edx+0xf8]` at `0x1002e3d6`. That call's object is `[ecx]` at `0x1002e3b4`, and `0x1002e3b0`'s callers `0x1001c726` and `0x1001c831` pass `[reg+0x4c]`. Not `0x100a1740` | `mov al, 1` at `0x1002e3f9`, then `ret 0x10`. Discarded. Skipped when the `+0xf8` return is null (`0x1002e3dc`) |
| `0x1002e4f9` | 3 | `ebx`, from `call [eax+0xf8]` at `0x1002e43f` | `mov al, 1` at `0x1002e502`. Discarded. Not `0x100a1740` |
| `0x1002e63d` | 3 | `[esp+0x14]` at `0x1002e620`. The store into that slot was not walked | `mov al, 1` at `0x1002e646`. Discarded. Not `0x100a1740` |
| `0x1002e88e` | 3 | `[esp+0x30]` at `0x1002e883`. The store was not walked | not read. `dec ebp` / `jne 0x1002e879` repeats the call. The last `eax` is still live at `ret 0x18` (`0x1002e8a1`). Not `0x100a1740` |
| `0x1003435b` | 3 | `esi`, from `call [eax+0xf8]` at `0x10034269` | overwritten at `0x10034361` by the loop count. Discarded. Not `0x100a1740` |
| `0x10034471` | none | `[ebx]`, and `ebx` is `ecx` on entry of `0x10034440` (`0x1003444e`). The caller `0x10010f57` sets `ecx` from `[ebp+0x4c]` (`0x10010f53`). Writer of `+0x4c` not read (OQ-291) | pushed at `0x1003447e` into `call [edi+0x7c]` (`0x1003448b`) with two other pushes. Raw value, no mask |
| `0x10034593` | 3 | `esi`, from `call [edx+0xf8]` at `0x100344af` | overwritten at `0x10034599` by the loop count. Discarded. Not `0x100a1740` |
| `0x10035676` | none | `[[esp+0x10]]` (`0x10035669`, `0x1003566d`). The store of that slot was not walked. The call is reached only when `call [eax+0xac]` (`0x1003564b`) returned none of `0x46`, `0x38..0x44`, `0x16` | pushed at `0x10035684` into `call [esi+0x7c]` (`0x1003568b`) with `[esp+0x58]` and the immediate `0x68`. Raw value. Not shown to be `0x100a1740` |
| `0x100357dc` | 3 | `ebx`, from `call [eax+0xf8]` at `0x1003573e` | `mov al, 1` at `0x100357e4`. Discarded. Not `0x100a1740` |
| `0x10035cfb` | 3 | `[esp+0x40]` at `0x10035cef`. The walk stopped on `call [edx+4]` at `0x10035cc4`, which moves `esp`; the store of the slot was not read | not read. `dec edi` / `jne` repeats the call (`0x10035d04`). The last `eax` is live at `ret 0x14`. Not `0x100a1740` |

`0x100011d3` is not in the eleven, but it is the one masked site whose receiver is settled. `0x100011b0` (slot `+0x58` of `0x10063100`) does `lea ebx, [esi+4]`, calls `0x10027950`, and on a non-zero return loads `[ebx]` and calls slot `+0xb4` with no stack argument (`0x100011cf..0x100011d3`), then `and eax, 7` (`0x100011d9`). `[esi+4]` is the type-17 pointer from `0x10021610`, so the target is `AnnoGame.dll` `0x100023a0`, not `0x100a1740`. `0x10027950` itself does `mov ecx, [ecx]` and then slot `+0xa4`, slot `+8`, slot `+0xc`: on a type-17 object that `+0xa4` is not the world's.

### The other 32, and Classic

The global chain is proved at `0x1000e809` and `0x1000d029` only. It was not found at the other 42. A site that loads `[this+4]`, or `[this]` of a method entered by `lea ecx, [reg+4]`, is the world PRNG only when that object was initialised by `0x10021e20`, and it is `0x100023a0` when the object was initialised by `0x10021610`. Those two stores are the writers. The class of each remaining call was not walked onto one of them (OQ-290). The expressions actually read:

| call | object expression | result |
|---|---|---|
| `0x1000225e` | `[ebx+4]`, `ebx` = `ecx` on entry. Callers `0x100021f4` and `0x10002215` forward `ecx` and push two dwords; the class was not read. The call itself has no further push. `and eax, 7`, and `ebp` counts down from 4 (`0x10002254`) so the call can repeat | open |
| `0x10002622` | `[ebp]`. `ebp` was live from outside the straight block. `and eax, 3` | open |
| `0x10007e6d` | `[edi+4]`, `edi` = `ecx` of `0x10007e50`. The `push esi` at `0x10007e64` is popped by the caller at `0x10007e87`. `and eax, 3`, then that many steps along `[esi]` (`0x10007e78`). Conditional on `0x1000c3a0` returning non-null. The only caller of the wrapper is `0x10007ec9`, and that function's callers are `0x10007e20` and `0x10007ffe`; neither was tied to an initializer | open |
| `0x10007ee7` | `[eax+4]` at `0x10007ee2`. `eax` is `[esp+0x10]` (`0x10007ed8`). The walk stopped on `call 0x10007e50` (`0x10007ec9`) because that call moves `esp`; the store of the slot was not read. `and eax, 7` | open |
| `0x100217eb` | `[esi]`, `esi` = `ecx` of `0x10021610`, which has just stored manager `+0x24` there (`0x1002161f`). No stack argument. `and eax, 3`. This is the type-17 pointer, so the target is `0x100023a0` if the vtable is `0x100215a4` | not the world PRNG |
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

Classic has the same 39 call encodings, each an instruction. The ten unmasked shapes other than the global have one Classic site each, with the same slot chain and the same use of `eax`: `0x1002bb63`, `0x1002bc69`, `0x1002bdad`, `0x1002bffe`, `0x10031abb`, `0x10031bd1`, `0x10031cf3`, `0x10032dd6`, `0x10032f3c`, `0x1003345b`. Classic has no vtable `0x10063100` (§3: no type 17), so `0x100011d3` and the `0x10021610` store have no counterpart there. `0x100217eb` therefore has none either. Matching the remaining masked sites one to one was not done; Classic's list is four calls shorter, and it also has a pair (`0x10051acb`, `0x10051adb`) on one `[edi+4]` where this module's inventory has a single call at `0x100547ed`. Forcing the counts to agree would invent a match.

No name was given to a decision. A mask, a `div`, or a discarded `eax` is what the instructions do. OQ-270..272 are untouched.

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
| T-AI.3 | done for the receiver of `0x1000e809`, `0x1000d029` and `0x100011d3`, and for the negative on every 3-argument call (§5). The class of the other masked sites is OQ-290; the `+0x4c` / `+0xf8` object is OQ-291. No decision is named | the table in §5 |
| T-AI.4 | the computer class. Settling, building, growth, trade and military are inside these 20 slots until a slot is read and moved out | `0x10041090` and the rest of the computer row in §6. Military already has `0x10043210` and `0x10004df0` from §11.6 |
| T-AI.5 | pirates | `0x10053660`, then the 12 pirate slots. `0x10055a30` and `0x10054710` are the two already named |
| T-AI.6 | Venetians | `0x1004c0c0`, then the 11 Venetian slots, first `0x1004cfc0` |
| T-AI.7 | natives | `0x100516a0`, then the 6 native slots, first `0x10051da0` |
| T-AI.8 | type 11, the factory's other player class | `0x1004ed60`, slot 0 `0x1004f050` |
| T-AI.9 | type 17, present in `CompLogic.dll` only | `0x10001040`, then `0x10002880` |

Classic is not a separate behaviour task. It is the same factory with no type 17 and one fewer slot. A task that changes a shared reading checks the Classic address as it goes.

## 8. Open

- **OQ-240.** Answered in §4. `Advance` `0x100ae0f5` calls holder slot `+0x24` (`0x1009c4d0`), which calls manager slot `+0x14` once.
- **OQ-241.** Narrowed in §5. `0x1000e809` and Classic `0x1000d029` are the world PRNG, and the return is discarded. `0x100011d3` and `0x100217eb` call `AnnoGame.dll` `0x100023a0` when `[world+0x110]` has vtable `0x100215a4`. Every call that pushes three arguments is not `0x100a1740`. The other masked sites are OQ-290. The 3-argument receiver and the 0-argument call on `[reg+0x4c]` are OQ-291.
- **OQ-270.** What `0x1003e1c0` does after the list walk, and whether `0x10003940` changes the dword `0x10003a20` returns (`0x10003ad2` loads `[esp+0x10]`). Next: `0x1003e1c0`, then `0x10003940`.
- **OQ-271.** Whether any of the nine kind slots calls a city command slot or `AnnoFrame.dll` `0x100589a0` / `0x10058e90`. Next: `0x10041860`.
- **OQ-272.** Calls on the object at world `+0x110` (vtable `0x100215a4`, stored at manager `+0x24`). Next: `0x1003d8e0`.
- The holder's vtable is `0x100cb858` (§4). Its RTTI name was not read (`0x1009cb3f`).
- Type 17's vtable (`0x10002880`).
- The 32-slot interfaces (`0x100632e8`).
- Which slot Classic dropped (`0x10060490` against `0x10063550`).
- `user32.dll` at `0x100604c2`.
