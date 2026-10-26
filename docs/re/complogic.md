# `CompLogic.dll` and `CompLogicClassic.dll` — inventory

**Status:** T38, 2026-10-26. Static reading only. No behaviour of a computer player is claimed here beyond the instructions that were read. Tags as in [`annowelt.md`](annowelt.md): **D** = read at the address, **likely** = the reading fits and was not proved, **open** = not read, with the next address.

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

**D.** `AnnoWelt.dll` `0x1009c630` (re-read). The holder's field `+0x94` is returned immediately when it is already set. Otherwise:

- property 14 is read by slot `+0x74` of the object at `+0x90`, with argument `0xe`, and bit 1 of the result is tested (`0x1009c67e`, `0x1009c681`);
- bit 1 set pushes `CompLogic.dll` (`0x100dcfa8`); bit 1 clear pushes `CompLogicClassic.dll` (`0x100dcf90`);
- the module handle is stored at `+0x98` (`0x1009c6ac`);
- `CompCreateInstance` (`0x100dcf7c`) is resolved and stored at `+0x9c` (`0x1009c6c9`);
- that pointer is called with argument 0, and the result is stored at `+0x94` (`0x1009c6db`, `0x1009c6e6`);
- the new object's slot `+0x18` is called with argument 3, then its slot `+0xc` with two pointers (`0x1009c703`, `0x1009c721`).

The function is slot `+0x14` of the vtable at `0x100cb858` (the dword is at `0x100cb86c`). The only relocations of that vtable address are the stores at `0x1009cb41` and `0x1009cbc1`. **Open:** the class, which has no RTTI. Next: those two stores. It is not the world vtable (`0x100ccdd8`, [`transport.md`](transport.md)).

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

**Open (OQ-240).** No function was found that, once per advance, calls the manager at holder `+0x94` or the object at player `+0xc`. The property-44 getter `AnnoWelt.dll` `0x10020ab9` has a single `.text` relocation, `0x10020c30`, which is inside the property dispatcher rather than a tick. Holder slot 0 (`0x1009c330`) was read for 500 bytes and does not touch `+0x94`. Next: holder slot `+0x18` at `0x1009c740`, then world advance `0x100ae040`, looking for a load of `+0x94` or of player `+0xc` followed by an indirect call.

**D, the wiring that can already be decided.** `CompLogic.dll` has no import and no embedded address of `AnnoFrame.dll` `0x100589a0`, the function single-player and the turn dispatcher both use to apply a player command ([`tick-loop.md`](tick-loop.md) §4). It cannot put a command on the turn stream by calling that function. Anything it changes, it changes through a vtable slot of an object it was given (the two pointers passed to manager slot `+0xc` at load, and whatever slot 0 of the per-player object stored).

Call displacements of `+0xa0` and `+0xac` are common in the module (115 and 82 indirect calls). Those are the city slots the command function uses for build and place, and they are also ordinary field offsets. A displacement does not say which object is in the register. **Open:** whether any of those calls use a city object. Next: one call of `[reg+0xa0]` whose register is loaded from a known city pointer; the first such site has not been isolated.

So the port is not "the AI already emits commands". W7 step 1.2's choice is still open in one direction and closed in the other: either every peer runs these calls, or the port wraps them as commands. The original does not wrap them.

## 5. The world-PRNG slot

[`tick-loop.md`](tick-loop.md) counted 35 calls of slot `+0xb4` in this module and said the count is an upper bound, because `+0xb4` is a common slot, and that a following mask is what makes a site look like the world draw. A capstone sweep finds **43** instructions `call dword ptr [reg+0xb4]` in `CompLogic.dll` and **39** in `CompLogicClassic.dll`. A raw search for the disp32 encodings in `CompLogic.dll` finds 44 byte hits, so one hit is not an instruction boundary. **D** for both counts. None of the 43 containing regions is itself a slot of the vtables in §3, so the class of each is **open** until its callers are walked up to a slot.

Of the 43, 32 use the returned value as a mask or a division in the next few instructions. 11 do not. The object in the register was not traced at any site (OQ-241). World slot `+0xb4` is the world PRNG ([`tick-loop.md`](tick-loop.md) §3); a mask after the call is **likely** that draw, and a site whose result is not used that way may be a different object's slot.

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

What each draw decides is the later sub-task. The plain uses above are only the arithmetic.

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
| T-AI.2 | the per-tick entry, and whether world changes go through the command slots (OQ-240) | `AnnoWelt.dll` `0x1009c740`, then `0x100ae040` |
| T-AI.3 | the 43 draws: which object, which decision (OQ-241) | the table in §5, the 11 unmasked sites first |
| T-AI.4 | the computer class. Settling, building, growth, trade and military are inside these 20 slots until a slot is read and moved out | `0x10041090` and the rest of the computer row in §6. Military already has `0x10043210` and `0x10004df0` from §11.6 |
| T-AI.5 | pirates | `0x10053660`, then the 12 pirate slots. `0x10055a30` and `0x10054710` are the two already named |
| T-AI.6 | Venetians | `0x1004c0c0`, then the 11 Venetian slots, first `0x1004cfc0` |
| T-AI.7 | natives | `0x100516a0`, then the 6 native slots, first `0x10051da0` |
| T-AI.8 | type 11, the factory's other player class | `0x1004ed60`, slot 0 `0x1004f050` |
| T-AI.9 | type 17, present in `CompLogic.dll` only | `0x10001040`, then `0x10002880` |

Classic is not a separate behaviour task. It is the same factory with no type 17 and one fewer slot. A task that changes a shared reading checks the Classic address as it goes.

## 8. Open

- **OQ-240.** Which function calls the AI on each advance.
- **OQ-241.** Whether each `call [reg+0xb4]` is the world PRNG, and what the 11 unmasked sites do.
- The holder's class (`0x1009cb41`).
- Type 17's vtable (`0x10002880`).
- The 32-slot interfaces (`0x100632e8`).
- Which slot Classic dropped (`0x10060490` against `0x10063550`).
- `user32.dll` at `0x100604c2`.
