# Script city objects and KILLED (SO-1a, 2026-11-15)

Reference: GOG 2.0.0.5, AnnoScene.dll SHA-256
`5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38`,
AnnoWelt.dll `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
**D** = original instructions/table read; **S** = existing string catalogue;
**A** = hand-derived consequence; **T** = reference/test; **U** = unproved.
Every address below names its module. No original listings, bytes or script text
are reproduced. Independent orchestrator review is pending.

Reuse [BIGBROTHER §3.1/§6](../formats/bigbrother.md),
[script-runtime.md](script-runtime.md), [fisher.md §8](fisher.md#8-script-object-inventory-and-proposed-next-packet-2026-11-15)
and [city creation](../sim/city-creation.md). This read does not implement SO-1b.

## 1. Definition and name resolution

**S/D.** AnnoScene's OBJECT_STADT string `0x10040244` is compared at
`0x10023642`–`0x10023651` inside reader `0x10023530`–`0x100237f8`.
Its branch creates a descriptor with name pointer at +0, kind byte 2 at +4,
the **low 16 bits** of the numeric operand at +8, and zero at +0xc
(`0x10023697`–`0x100236c5`). Numeric helper `0x1000b2e0`–`0x1000b306`
passes a base-0 integer conversion result to a 32-bit destination; the descriptor
store narrows it. The reference takes the already parsed signed-32 value and
models this low-word store, not CRT overflow or the complete numeric lexer.

**D.** This is a symbolic definition, not a city-pointer binding. The reader
only allocates/parses/appends descriptors (`AnnoScene 0x100236cd`–`0x10023706`).
Its immediate load-time postprocessing (`0x10016dca`–`0x10016e19`) walks the
list but only kind **1** uses the figure manager; kind **2** is skipped.
No city-presence test, world city lookup or INIT action occurs on this path.
An absent city therefore does not invalidate the city definition at parse time.

**D/A.** Name resolver `AnnoScene 0x10023f40`–`0x10023f73` walks the descriptor
list in declaration order and returns the first full-name match. Comparator
`0x10027310`–`0x10027361` folds ASCII A–Z; its locale-dependent alternate path
starts at `0x10027363` and is not part of this oracle. There is no player-mapper
`@` prefix stripping here. The query parser calls that resolver at `0x100212f7`:
missing name returns a failed parse at `0x100212fc`–`0x1002135f`, not a false
KILLED value. A successful lookup is retained as the wrapper's descriptor pointer
at +4 (`0x10021362`–`0x1002136b`); wrapper table `0x1003bb34` has Boolean
query slot +8 = `0x100117e0`.

## 2. The numeric namespace, proved through both getters

**D.** In the city arm of KILLED, `AnnoScene 0x10011898`–`0x100118ce` reads
byte +8, calls world slot +0x20 with property `0x1300` and that byte, then
reads byte +9 and calls the returned island's slot +0xf8. Thus:

- `island = (n & 0xffff) & 0xff`;
- `city_slot = ((n & 0xffff) >> 8) & 0xff`.

**D.** The receiver proof is AnnoWelt's world table `0x100cc92c`: slot +0x14
is class getter `0x100a37c0` (class `0xf00`), and slot +0x20 is
`0x100a3820`–`0x100a383a`, which returns indexed `world +0x6530` for property
`0x1300`. Registration `0x100a19af`–`0x100a19db` obtains the island's slot
+0x40, keeps its low byte and stores that island pointer at this indexed table.
Island table `0x100cac2c`, slot +0x40 = `0x1008c690`–`0x1008c699`, reads
its number at +0x98. This is an island **registry index**, not file order.

**D.** Island table `0x100cac2c`, slot +0xf8 = AnnoWelt
`0x100863e0`–`0x100863e8`, reads `island +0x38 + 4*city_slot`.
The ring is embedded at island +0x38 (`0x100861d5`–`0x100861e0`);
creation stores its city at the chosen ring index (`0x100968a4`–`0x100968b0`).
The second byte is therefore the city number **within that island**, neither
player number nor global city index.

**A/P.** The two operands inventoried in fisher.md §8, 4 and 18, consequently
mean `(island 4, city 0)` and `(island 18, city 0)`. They happen to fit one byte;
that coincidence is not evidence for an island-only rule. Operand 258 separates
the readings: `(island 2, city 1)`. Operand 65540 narrows to `(4,0)`.

**P.** Independent original-file corroboration: S01_02_01's STADT2 prefixes at
file offsets `0x1e0ee9` and `0x1e74e7` both give city slot 0, on islands 4 and
18 respectively (their witness tiles are located through the INSEL5 rectangles).
Both are enclosed in player 2's records, not players 4/18. The prefix layout
reuses [sze.md](../formats/sze.md); this is corroboration, not the namespace proof.

## 3. Query truth and lifetime

**S/D.** AnnoScene compares the KILLED string `0x10040114` at
`0x10021578`–`0x10021588`; the node receives query selector `0x16` and the
wrapper pointer at `0x100215da`–`0x100215f3`. Common evaluation
`0x100108a9`–`0x100108b7` invokes wrapper slot +8; `0x100117e8`–`0x100117f5`
dispatches selector `0x16`. The city arm is `0x10011898`–`0x100118dd`.

**D/A.** The only truth predicate is **city-slot pointer == null**: the test at
`AnnoScene 0x100118ce`–`0x100118d6` returns false for a non-null slot and true
for null. It does not call the city's owner getter, inspect buildings or read
HP. An extant city with a changed owner or zero buildings therefore still gives
false *if the slot remains populated*. This does not prove which gameplay
transactions remove a city's slot.

**D/A.** Both lookups run every evaluation. These getters are leaves
(`AnnoWelt 0x100a3820`–`0x100a383a`, `0x100863e0`–`0x100863e8`), and the
city query branch stores no descriptor value or result. It draws nothing and
has no killed latch. Null at the first evaluation gives true even if no city
has ever occupied that slot; a later populated slot gives false. The query
follows the **slot**, not the lifetime identity of a formerly bound city.

**D.** A concrete removal path confirms the null representation: AnnoWelt
island removal `0x100863b5`–`0x100863c2` passes its ring and city to
`0x10096990`; `0x10096997`–`0x1009699c` reads the city's slot and zeros that
entry. Re-creation at the same index uses `0x100968a4` again.
No exhaustive ownership-transfer or last-building-removal caller read is claimed.

**D/U.** Unlike a missing city on an existing island, a **null island** result
is not guarded: AnnoScene dereferences it at `0x100118b8` before invoking
+0xf8. The city getter also contains no bounds test. The oracle refuses missing
islands or indices outside its explicit readable city-slot domain; it does not
invent a Boolean for invalid memory. SO-1b needs an explicit safe disposition
for those inputs. Locale-dependent non-ASCII matching remains outside this read.

## 4. Saving and reloading

**D.** AnnoScene reader `0x10016c76`–`0x10016d1d` copies source into script
+0xc/+8. Its SIGNALS arm sets the prefix boundary at `0x10017015`–`0x10017027`;
`0x10016420`–`0x10016461` keeps the text before that boundary, including the
earlier object definitions. Saver `0x10016470`–`0x1001650f` writes this prefix
(or the source fallback), calls signal writer `0x10016240`–`0x1001635c`, and
camera writer `0x10016150`. Signal iteration at `0x100162e5`–`0x10016349`
reads the separate signal list at +0x20, not the object list at +0x54.

**D/A.** The numeric definition survives as program text. This saver has no
object-pointer/Boolean-query-state writer; reloading reparses the definition and
subsequent KILLED reads the restored world's current city slot. A script can
separately latch a result into a signal, whose value is saved; that is not a
hidden latch in KILLED. Scheduling/cursor differences and signal restoration
reuse [script-runtime.md](script-runtime.md), not a newly invented save policy.

## 5. Reference, controls and limits

**T/A.** `tools/script_objects_ref.py` uses synthetic `query LABEL key=value`
records and explicit world snapshots. Inputs: ordered `objects=name:n`, `ref`,
`islands`, `city_slots`, `initial` (cities present at parse time), and `frames`
(cities at successive query evaluations). A city tuple is
`island:slot:owner:building_count`; commas separate cities and `/` separates
frames; `-` denotes an empty table. Names are portable ASCII. Outputs are
`compiled`, `island`, `city`, `killed`, `parse_reads`, `city_reads`, `writes`.
`parse_reads` counts **city-slot lookups** during parsing, not the unconditional
figure-manager accessor before the kind-1 postprocess loop. Input-schema
validation is not a modeled world lookup. `city_reads` counts the live city getter.
`killed=-` is a failed name resolution, not a false evaluation. Counters cover
the query leaf; surrounding IF actions are not modeled. The original fixed-size
name allocation and CRT overflow/locale behavior are not memory-safety emulations.

**A.** Hand-derived vectors distinguish alive, destroyed, absent at parse,
absent on first evaluation, same-slot recreation, owner/building-only changes,
byte order, low-word narrowing, negative narrowing, whole-name lookup and
first duplicate declaration. A changed expected value must disagree; applied
source mutants must distinguish swapped bytes, cached presence and an owner/
building predicate while legal controls still execute.

**U.** Original live execution and History Edition are NOT_RUN. This is not an
exhaustive city-destruction, ownership-transfer or locale study, nor an engine
port. Missing/unsafe island or city-domain inputs produce reference exit 2,
not a guessed original Boolean. Existing section versions, state 139, pins and
recordings stay unchanged. OQ-625's other query/action and canary blockers remain.
