# Transient empty EVENTINSEL completion

WILDLIFE-EVENTINSEL-1, 2026-11-19. D = GOG instructions/data, P = original
parsed file, C = authored execution. Independent orchestrator review pending.
Scope: the reached empty EVENTINSEL in the [ordered prefix](wildlife-prefix.md),
after [empty PROD2](wildlife-prod2.md) and the supported HIRSCH load records.
No event runtime or living wildlife integration.
The wildlife +0xdc handler delegates non-HIRSCH records to base `0x10089130`
at `0x10094848`, reusing the established source-order call chain.

## Primary contract and acceptance, before code

D: AnnoWelt `0x10089751–0x10089779` compares EVENTINSEL at string
`0x100dce90`, using exact comparator `0x100b4b20`. Match delegates to island
pointer +0x30, container slot +8 at `0x10089776`. Initialization
`0x1008c28b–0x1008c297` obtains this container through island slot +0x1b4.
Both concrete wildlife table 0x100cac2c and base table 0x100ca19c resolve that
slot to `0x1008bff0–0x1008c074`: allocate 0x3c bytes, construct through
`0x10079ad0`, then no-op success initializer `0x10001060–0x10001062`.
Constructor installs final table 0x100c9b68 at `0x10079bc6`; +0x24 keeps the
island pointer, +0x20/+0x22 counters initialize zero at `0x10079bbd–0x10079bcf`.
Earlier list setup is constructor work, not an effect of an empty header.
This remains the existing successful-allocation/fresh-island domain; global
constructor/caller PRNG alignment is not inferred from local-header execution.

D: final table +8 points to reader `0x10079460–0x1007980d`, +0xc to writer
`0x10079810`. Reader zeroes its scratch count at `0x10079491` and caller-reported
body count at `0x10079499`, then compares EVENTINSEL itself. For match,
`0x100794ad–0x100794ba` obtains declared payload length via `0x100b4b10` and
branches on signed <= 0 directly to `0x100797f6`. For the supported checked
zero-length record, it returns 1 through `0x100797fb–0x1007980d`, restoring its
stack exception bookkeeping only. No container reset, object allocation/deletion,
event insertion, island/resource/spawn mutation, payload read, gameplay callback
or world draw is reached. It preserves existing container contents; it does not
mean EVENTINSEL is globally ignored or that the container is necessarily empty.

D: outer arm returns through shared check `0x100896bd–0x100896e9`: false
propagates failure; true adds reported body count (zero here) and returns 1.
There is no fallthrough into another header or post-island work. The INSEL5
source-order caller and callbacks remain as proved in the prefix contract.
The positive-length reader starts nested record parsing at `0x100794c0`, with
object allocation/state paths later in the reader; none is released here. A
nonmatching supplied tag takes `0x100797d9–0x100797f3`, helper
`0x100b4cc0–0x100b4cdb` reports declared length and invokes stream seek +8 with
origin 1. That generic delegated skip is distinct from recognized empty success;
this packet does not add arbitrary header skipping to the prefix.

D: writer `0x10079810` is context only, not required as a new codec:
its addressed start `0x10079831–0x10079851` initializes output count and enumerates
its list; `0x10079954–0x10079980` names EVENTINSEL and writes a header with the
accumulated body length. Empty body is a real recognized container record, not
an inferred absence. No event serializer or nonempty event semantics is ported.

C defensive scope: accept only checked empty payloads. All nonempty payloads,
including otherwise valid nested records, malformed nested headers and raw bytes,
stop before their unimplemented effects. Large declared lengths whose original
signed test is nonpositive are not an emulated malformed-I/O domain. Outer chunk
framing is still checked by the existing parser, before prefix effects. Owned
per-header enum EmptyPreserved distinguishes absent, completed empty and unsupported
without storing raw payloads or inventing an event count/result/runtime state.

| Requirement / primary | Positive proof and smallest counterexample | Disposition |
|---|---|---|
| Recognized empty preserves / 0x100794ba -> 0x100797f6 | Empty success marker with records 0, zero added draws/effects; absence has no marker | pending |
| Source order / no reset | Two empty headers around fertility/HIRSCH preserve raw spawn fields, list heads, scalar/resource/ore state after input destruction | pending |
| No unrelated callback | Water-only island remains mask 0, not GELAENDE OR 511 | pending |
| No inferred nonempty no-op | Positive byte or authored nested record stops at EVENTINSEL item 0 with exact reason, prior empty completion retained | pending |
| Defensive malformed scope | Partial nested header/raw payload refuses as unsupported, malformed outer chunk refuses before diagnostic effects | pending |
| Actual-code cause | Legal boundary control versus private unsupported-guard removal reaches later MYSTERY and fails precise EVENTINSEL/source/reason/completion assertion | pending |
| Same raw original once | Independently read one island/73 headers/empty EVENTINSEL72 last; retain 156 owned spawn records/4745 draws, complete local records only | pending |

Local-record success does not execute original post-island callbacks
`0x100a2fc8`, `0x100a2fd7` or `0x100a2fdf`, global scene initialization, another
island path or living figure creation/tick. No World/native/state/tag/PRNG identity,
registry/default/normal-loader/event runtime/UI change. Prior omitted derived caches
retain their future rebuild obligation. Original runtime/HE are NOT_RUN.
