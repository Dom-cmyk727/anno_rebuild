# Ordered transient DISCOVER scalar import

WILDLIFE-DISCOVER-1, 2026-11-19. D = GOG instructions/data, P = parsed
original file, C = authored execution. Independent orchestrator review pending.
Scope: the reached header in the [local prefix](wildlife-prefix.md), following
[ANBAU](wildlife-anbau.md) and [OCCUR](wildlife-occur.md). No visibility policy.

## Primary contract and acceptance, before implementation

D: AnnoWelt `0x10089977–0x10089985` compares the supplied header with DISCOVER
(string `0x100dcec8`, exact comparison `0x100b4b20–0x100b4b6c`). On a match,
`0x10089987–0x100899a4` reads its payload into local scratch through
`0x100b4c40–0x100b4c5f`. That helper obtains the declared length from header
+0xc (`0x100b4b10–0x100b4b13`) and calls the supplied stream's read slot +4.
Correction, 2026-11-19: `0x1008999b` initializes payload scratch to zero,
not the read-count local: after three pushes `[esp+0x20]` is the original
`[esp+0x14]`; the read-count pointer is the original `[esp+0x10]`.
The arm adds the stream-reported count to
the caller's consumed-byte accumulator at `0x100899a7–0x100899b2`, loads only
the scratch's LOW WORD at `0x100899ab`, and REPLACES island u16 +0xa6 at
`0x100899b6`. It returns success immediately at `0x100899bd–0x100899cb`.
There is no direct resource/occurrence/cell/crop write, callback or world draw.
The stream read is I/O, not a gameplay callback; this contract does not claim
behavior of every supplied stream implementation on a short read. The arm
itself neither checks the read return/count nor validates the declared length.
Malformed short or oversized records therefore have no safe equivalence claim.

D: the saver `0x1008a669–0x1008a6a0` names DISCOVER, sets length FOUR at
`0x1008a677`, and writes from island +0xa6 (`0x1008a687`). The adjacent upper
word is serialized too, but the loader ignores it. Legal supported framing is
exactly four bytes; defensive refusal of all other lengths happens before any
new scalar effect. This is an adaptation, not emulation of unsafe stack I/O.
The scalar getter zero-extends +0xa6 at `0x10087c62–0x10087c6b`; its setter
stores a word at `0x10087bca–0x10087bd6`. No engine player-slot clamp belongs
to this raw transfer. The ordinary scenario loader's player-slot validation and
later visibility/recount policies remain unchanged and outside this packet.

D: the source-order INSEL5 caller, island slot +0xdc and delegation are already
proved in [load boundary](wildlife-load-boundary.md). This arm returns after the
single store, without deduplication or union. The reset routine `0x1008c740–0x1008c7c0` zeroes EBX and writes its word to
+0xa6 at `0x1008c7b9`; that is initialization, not header completion.
No header means no observed import
write. Output uses optional widened u32, not a fabricated default; explicit zero
is a completed write. Per-header optional evidence distinguishes completion
from an unsupported/malformed boundary and retains source ordinal. Owned scalar
values outlive all input buffers. Existing derived-cache omissions and future
consumer rebuild obligations remain as documented in the prefix.

| Requirement / primary | Positive proof and smallest counterexample | Disposition |
|---|---|---|
| Low word only, u16 store / 0x100899ab,b6 | 0xabcd8001 -> 32769; full-word interpretation -> 2882371585 | pending |
| Replacement / same store | 0x8001 then 2 -> 2, wrong union -> 32771 | pending |
| Zero, maximum, absence | absent optional; explicit 0 engaged; 0x1234ffff -> 65535, no player clamp | pending |
| Defensive framing | lengths 0,1,2,3,5,8 refuse, prior scalar preserved, failed header uncompleted | pending |
| Owned order/unrelated state | resource/OCCUR/ANBAU/fertility/HIRSCH surrounding writes and draws retained after input destruction | pending |
| Actual-code causal check | legal chronology passes; private replacement-to-union mutant fails intended second-header assertion | pending |
| Same raw Tutorial once | independently read header 53 width/value, unchanged earlier state and cursor, exact next header | pending |

No World member, native field/tag, state number, registry/default/PRNG identity,
normal loading, fog/UI, diplomacy or wildlife lifecycle change. The original
runtime, History Edition and post-island global callbacks are NOT_RUN.
