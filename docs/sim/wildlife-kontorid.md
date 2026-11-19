# Ordered transient KONTORID counter import

WILDLIFE-KONTORID-1, 2026-11-19. D = GOG instructions/data, P = original
parsed file, C = authored execution. Independent orchestrator review pending.
Scope: one header in the [ordered prefix](wildlife-prefix.md), after
[DISCOVER](wildlife-discover.md). No city creation or id allocation.

## Primary contract and acceptance written before implementation

D: AnnoWelt `0x10089bb1–0x10089bbf` compares KONTORID (string `0x100dceb4`)
through the exact-byte comparator `0x100b4b20–0x100b4b6c`. On match,
`0x10089bc1–0x10089bdd` invokes payload reader `0x100b4c40–0x100b4c5f`.
That helper obtains the declared length from header +0xc via `0x100b4b10` and
calls the supplied stream read slot +4. Track ESP from before the arguments:
count pointer = original [esp+0x10]; payload pointer = original [esp+0x18].
After three pushes, store 1 at [esp+0x24] (`0x10089bd5`) initializes payload
scratch, not the reported count. The arm adds the reported read count to its
caller accumulator (`0x10089be2–0x10089beb`), loads the scratch word at
`0x10089bee`, and REPLACES island u16 +0xc6 at `0x10089bf4`.
Return is immediate at `0x10089bfb–0x10089c09` with success 1.
No validation of the loaded value, increment, allocation, signed comparison,
resource/occurrence/cell write, gameplay callback or world draw occurs here.
Original stream failure/count is unchecked by the arm; its malformed I/O is not
emulated or declared safe. The scalar transfer is not a market's nested KONTID.

D: saver `0x1008a6aa–0x1008a6e3` names KONTORID, chooses length 2 at
`0x1008a6b8`, and writes from island +0xc6 (`0x1008a6c8`). Its surrounding
flag test was established for the preceding DISCOVER saver. Reset stores word
1 at `0x1008c7fd`; constructor independently stores word 1 at `0x1008cbe9`.
These are initialization, not evidence that an absent header was processed.
Present zero replaces 1 without a refusal here. Encoded width is 16 unsigned
bits; the projection widens to u32 and does not impose an engine allocation cap.
65535 stays 65535: this header performs no addition, so no overflow or wrap.
Repeated headers replace in file order, reusing the proven source-order caller.
[City creation E12–E14](city-creation.md) and the ordinary loader's nonzero/default
policy are distinct consumers; neither is changed or copied into this importer.

C defensive framing: require exactly two payload bytes before a new effect.
Other sizes refuse with a labelled incomplete diagnostic, retaining prior state.
This is not emulation of the original unchecked scratch-buffer read. Optional
owned u32 state and per-header completion distinguish absent, explicit zero and
failure. No whole-map copy or borrowed scalar storage. Existing derived-cache
omissions retain their future rebuild obligations.

| Requirement / primary | Hand-derived distinguishing control | Disposition |
|---|---|---|
| Unsigned LE word / 0x10089bee,bf4 | Authored bytes 2,129 -> 33026; wrong byte read -> 2, swapped -> 641 | pending |
| Replacement, not OR/add/increment | First 33026 then 7 -> 7; wrong union -> 33031, increment -> 8 | pending |
| No normal-loader zero policy | Absent optional versus engaged zero; maximum 65535 remains unsigned | pending |
| Defensive framing | Sizes 0,1,3,4,8 refuse before effect, preserve previous 33026 and exact failed source | pending |
| No unrelated mutation/draw | Surround resource, OCCUR, DISCOVER, ANBAU, fertility, HIRSCH; retained masks, quantities, source chronology and draws | pending |
| Ownership | Destroy input buffers after success; scalar and per-header values remain readable, original input unchanged | pending |
| Actual-code cause | Legal chronology succeeds, private actual assignment-to-union mutant fails final-counter assertion | pending |
| Original once | Independently derive raw header 54 value/width, retain prior effects and stop exact next header | pending |

No normal loader, World, native/save section, state/tag/PRNG identity, registry,
default, founding, market allocation, visibility or UI change. No wildlife
lifecycle or later header implementation. Original runtime/History Edition and
post-island/global callbacks are NOT_RUN.
