# Ordered transient ANBAU bitmap import

WILDLIFE-ANBAU-1, 2026-11-19. D = GOG instructions, P = parsed original input,
C = authored execution. Scope: one local header in the
[ordered prefix](wildlife-prefix.md), not crop placement, production, dry-field
policy or World integration. Independent orchestrator review pending.

## Primary contract written before code

D: AnnoWelt ANBAU arm `0x10089871–0x10089974`. The dispatcher initializes EDI
and its local count to 0 at `0x1008914e–0x10089157`. `0x10089885–0x100898a1`
REPLACES island dword +0xac with 0 if climate byte +0xc0 is 0, otherwise 0xf009.
INFO supplies that byte: `0x100897a6–0x10089802` reads its payload then stores
byte 0 to +0xc0 at `0x100897ee` (citation corrected 2026-11-19). This reuses the prefix's explicit leading INFO and late-setup
refusal; it is not inferred from the map name. Absence of ANBAU does not establish
an initial +0xac value in this transient contract.

D: raw-byte helper `0x100b4c40–0x100b4c5f` reads the whole payload; the arm treats
it as consecutive NUL-terminated byte strings, not tagged records or a map grid.
At `0x100898d2–0x1008995f`, process while pointer < payload end, at most 13
strings (counter 0..12). Each consumes its NUL even if empty/unknown. After the
13th, remaining bytes are ignored even if not a terminated string. This is the
addressed original reader rule, not an engine population/content capacity.

D: each token tests game slot +0x48(kind 0x20, index 56..68) in ascending order
at `0x100898dc–0x1008992f`; exact byte comparison, case-sensitive, no Unicode or
ASCII folding. First match wins, ORing `1 << (goods_id - 56)` at
`0x10089933–0x1008993d`. Unknown tokens set nothing, but count. Duplicate names
in the header do not toggle/increment bits. Later ANBAU headers reset to the
climate seed again, not union with the previous header's bitmap.

D: concrete Game dispatcher `0x100033c0`, jump selection for kind 0x20 ->
`0x1000346b–0x100034ad` -> goods-name getter `0x10013be0`; indices 56..68 use
pointer table `0x100251f8` at `0x10013c52–0x10013c63`. Null pointers fall back to
string `0x100261e8` at `0x10013cde–0x10013ce7`. These are names from the supplied
supported original-image data, not mod registry names, figure ids or building ids.
Reuse the existing checked PE table view to read the bounded pointer range and
owned strings, including fallback; no compiled copy of the original names.
The supported range/13-token rule is image/code-layout metadata; modern state
uses vectors/owned strings and a fixed-width original bitmap, not a fixed array
of all crop types. The existing image identity checks remain mandatory.

D: after the final token `0x10089965–0x10089974` only returns. A zero-length
payload takes the shared return at `0x10089d6c–0x10089d7b` after applying the
seed. There is no tile/resource/occurrence/fertility write, name-dependent callback
or world-random draw on this finite arm/helper/name-lookup path. Concrete island
+0xdc delegates as already established; source-order loop continues unchanged.
Post-island/global callbacks remain outside scope. The mask is an observed loaded
scalar; consumer farming rules and the meanings of preseeded bits are not ported.

| Requirement / primary | Independent distinguishing control | Status |
|---|---|---|
| Bitmap seed replaces, width u32 | Climate 0/1/2 empty payload -> 0/0xf009/0xf009; absent header remains absent | pending |
| Exact name/index mapping | Authored PE names at slots 56,57,68 map to bits 0,1,12; registry-name substitution must not work | pending |
| First match / byte equality | Duplicate PE name maps to lowest slot; Latin-1 byte differs from UTF-8/lookalike; lower-case token unknown | pending |
| Source chronology/reset | First header bits 0/12, next bit 1 -> final 2, not accumulated 0x1003; final empty header resets 0 | pending |
| Original token count | 13 unknown tokens followed by a known name set no bit; leftover unterminated bytes ignored after 13 | pending |
| Defensive framing/PE errors | Unterminated processed token, invalid name pointer/string/identity refuses before corresponding header effects, retaining prior accepted mask/tokens | pending |
| Ownership / unaffected state | Surround with resource/fertility/HIRSCH; input immutable, names/digests owned after destruction; exactly original draw assignments | pending |
| Actual-code causal check | Legal duplicate-header control versus wrong union with prior bitmap fails the reset assertion | pending |
| Same raw original once | Independent raw payload + original PE names derive token count/mask; preserve prior masks/Q/cursor, stop next header | pending |

Defensive malformed-input refusal validates processed-token termination and name
metadata before applying the header. This is not a claim of transactional/safe
original stack I/O. No original scratch-buffer capacity is copied into the engine.
Trace completion distinguishes seed-only zero success from boundary-before-effect.
Prepared output owns token bytes, optional matched goods ids, final mask and per-
header seed/mask/ignored-byte evidence. Raw Terrain fertility and effective merged
variation stay distinct and unchanged. Existing derived-cache omissions and future
consumer rebuild obligation remain, without omitting a gameplay bitmap effect.
No normal loader, registry identity, native/tag/state/PRNG/default or other-lane
farming/crop policy change.

## Pre-gate checks

Four authored adapter cases initially pass 426 assertions; the refined empty
seed-only path then passes its 180-assertion control even with no name-table
section. Nonempty headers require a complete checked name window; malformed PE
metadata refuses conservatively before effects, not emulated original bad pointers.
The checked name data is memoized per preparation, never copied per token/map.

One private actual adapter-TU mutation changes only reset to union with the prior
bitmap. The legal chronology case passes 70 assertions; the mutant fails exactly
its second-header reset (4099 versus 2), with 69 other controls passing. Canonical
source is untouched and rebuilt. The initial compile's test-local shadow name
was corrected before freeze; its original Werror log is retained.
