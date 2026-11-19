# Transient empty PROD2 block and nonempty boundary

WILDLIFE-PROD2-1, 2026-11-19. D = GOG instructions/data, P = original parsed
file, C = authored execution. Independent orchestrator review pending.
Scope: the proved empty block in the [ordered prefix](wildlife-prefix.md), after
[KONTORID](wildlife-kontorid.md). No building factory or nonempty placement port.

## Primary contract and acceptance written before code

D: AnnoWelt `0x1008957d–0x1008958b` compares PROD2 (string `0x100dcef4`) with
`0x100b4b20`. At `0x10089591–0x100895b8`, get declared payload length through
`0x100b4b10`, read SIX bytes from stream slot +4 into stack block header, and add
reported count to the caller accumulator. Header is u16 local X/Y plus two size
bytes, matching [block format](../formats/insel.md). At `0x100895bb–0x100895c8`,
add island X/Y to the scratch words, with u16 wrapping; no tile or shared state
is written. With a successful six-byte read, `0x100895cd–0x100895d6` computes
floor((declared length - 6) / 6) using unsigned multiply/shift. Zero items branch
at `0x100895d9–0x100895db` directly to shared success return
`0x10089d6c–0x10089d7b`. No other call or island mutation occurs on this path.
Width/height are not validated or used to visit cells on the empty branch.

D: unlike GELAENDE's zero-item branch and +0x198 water epilogue at
`0x100894a9–0x100894b2`, PROD2's empty return reaches NO writer, water mask,
empty-tile fill, resource list, height-bound/dirty cache callback or PRNG draw.
Only local scratch and caller stream accounting change. Completed empty header
provenance is owned; no gameplay raw-payload stash stands in for applied effects.

D: nonempty body `0x100895e5–0x10089692` reads six-byte items. Packed u16 type
uses 14 bits (`0x1008960d`), rotation high two (`0x1008964e–0x10089654`). State
low six bits supply phase, upper byte height (`0x1008965a–0x10089664`). The item
X/Y bytes are signed (`0x10089634`, `0x1008963f`), added to the adjusted block
origin and stored as words; generic PlacedObject's 12-bit/unsigned interpretation
is not a proof for this path. Descriptor lookup +0x70 precedes placement; null
skips the original item at `0x10089617–0x10089619`. Known type calls island +0x78
with flag 0x04000000 at `0x10089638–0x10089676`.
Concrete wildlife table 0x100cac2c +0x78 -> 0x1008ec90; base table 0x100ca19c
+0x78 -> 0x10082040. Wrapper `0x1008ec90–0x1008ecd3` does the type lookup and
calls the lower writer unless bit 0x80000000, then follows the fresh +0xc8 path.
The [corrected rock callback contract](wildlife-rock.md#writer-callback-correction-2026-11-19)
retains lower-writer cache callbacks and its full endpoint 0x10082b0a. This is NOT
resource admission equivalence or proof of a nonempty PROD2 implementation.
Nonempty tile copies are also distinct from city PROD/ENTRY building creation
([scenario-city read](../re/annowelt.md)). All nonempty PROD2, including unknown
types, remain unsupported BEFORE lookup/effects in this bounded projection.

D: tail arithmetic alone is insufficient for successful source-order continuation.
INSEL5 caller `0x100a2f8c–0x100a2fbc` subtracts actual reported header/body bytes,
then immediately reads the next header. Header reader `0x100b4c20–0x100b4c35`
directly reads 16 bytes; neither site skips a partial item tail. Thus a 7–11-byte
payload may take the local zero-item return but leaves unread bytes in the stream.
No safe whole-sequence equivalence or tail consumption is established. Defensive
projection refuses a short block header or a partial-only item tail; it does not
silently complete it because floor(count) is zero. A whole unsupported item takes
its item-0 boundary even if followed by a partial tail. Original short I/O and
underflow/unsafe malformed streams are not emulated.

| Requirement / primary | Hand-derived control and counterexample | Disposition |
|---|---|---|
| Six-byte empty success / 0x100895d9–db -> 0x10089d6c | Nonzero block coordinates/sizes complete with 0 records; absence has no completion marker | pending |
| No unrelated epilogue | Water-only island remains mask 0 after empty PROD2, unlike GELAENDE water OR 511 | pending |
| Ordered owned evidence | Two empty headers, including outside-map/wrapping scratch coordinates, retain source and copied geometry after input destruction | pending |
| Explicit nonempty boundary | Known/unknown id, nonzero phase/height and signed offsets stop item 0 before effects; earlier empty marker retained | pending |
| Defensive framing | Header lengths 0–5 refuse; partial-only tails 1–5 refuse; a whole item plus tail stays unsupported item 0 | pending |
| Actual-code causal check | Legal boundary control executes; private removal of nonempty refusal reaches later MYSTERY and fails precise PROD2-boundary assertion | pending |
| Original once | Independently parse consecutive block counts, preserve prior state, then run already-supported HIRSCH if reached and stop next unsupported header | pending |

No World/native/state/tag/registry/default/PRNG identity, normal-loader, production,
city factory, UI or other-lane change. Prior omitted derived caches retain future
rebuild obligations. Original runtime/HE and post-island/global callbacks NOT_RUN.
