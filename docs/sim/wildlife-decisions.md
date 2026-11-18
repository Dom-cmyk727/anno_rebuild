# WILDLIFE-1a: frozen pure-decision contract

Base b5c0ada209d94a0082bc88d793c917c7222e6a21, 2026-11-18. This packet ports only the ten local integer decisions of [wildlife.md sections 1–11](../re/wildlife.md) and the unchanged `tools/wildlife_ref.py` oracle. It is not a whole-tick, lifecycle, spawning, movement, hunt or transitive-draw implementation. No World, DrawSource, clock, mutable global, initializer, I/O, state field, default, extension or registry change is authorized.

Baseline direct checks before implementation: 65 vectors / 0 disagreements; reference contract 31 checks. Counts: density 4, habitat 5, replenish 3, create 3, load 3, animal 13, walk 4, route_old 3, wolf 19, prey 8. The original reference and vectors stay unchanged.

All draw inputs are bounded spans of raw u32 world outputs, not seeds. Results report the number consumed; unused trailing values are allowed. Typed errors distinguish invalid input, exhausted supplied draws and the named unsupported OQ cases. Errors retain the count consumed before refusal. Caller buffers are never mutated; no retry can outlive the finite candidate/draw input. Semantic byte/word inputs mirror the original decoded operations, not engine-wide registry or player limits. Quantities are unscaled raw integer words, not converted tons; timestamp base is in the original 1000-input-tick units. Resolved type/goods identities use widened engine ids. No helper calls a runtime PRNG.

| Family / evidence in AnnoWelt | Inputs and units | Outputs and direct draws | Hand-derived distinguishing boundary |
|---|---|---|---|
| density, 0x100938a0–0x10093963 | qualifying cells in the 16-cell sweep | target record count; 0 draws | n 13 gives 4, n 14 gives 5, unlike floor(n/3) |
| habitat, 0x10093030–0x10093472 | resolved centre and 8 neighbour id/goods/kind facts; property 7 | Boolean; 0 draws | centre score 1 + neighbour matching two predicates 2 totals 3 and fails ordinary threshold 4; property 7 = 5 passes |
| replenish, 0x100935e0–0x10093725 | ordered unique candidate tokens, at most 16 geometry samples; byte deficit; occupied tokens | chosen and remaining lists in exact swap-pop order; one draw per attempted candidate, including collisions | [10,20,30], occupied 20, draws [1,1] chooses 30 after 2 draws; [10,20,30,40] with [0,0] chooses 10 then 40 |
| create, 0x10092cfd–0x10092f30 | cave flag/entrance count, caller-resolved climate's 32 type/goods pairs, saved goods | position selector (-1 = first cave entrance), species index/type, inspected count; non-cave position draw then species draw | cave omits position draw; unmatched goods from index 31 inspect 18 entries and stop at 16, not a new draw |
| load, 0x100947d3–0x10094816 | entry byte 4, already divided timestamp base 0–65, mode byte, saved goods | timestamp byte and discriminator; 1 jitter draw iff bit 0 clear | base 65 + (63 & 31) = 96; do not divide base again; mode bit 0 gives sentinel 255 |
| animal, 0x100519d0/0x10051b00 | event, state byte, resolved success, post-action quantity/capacity u16, idle variants 1–3 | next state, optional idle variant; exact completion/idle draws, fallback none | quantity 5 > 2*floor(8/3) overrides selected search to walk after consuming draw; equality 4 does not |
| walk, 0x10051515/0x10058360–0x100583ff | attempt/behavior/facing bytes | three radii in sixteenths, angle-table indices, wrapped attempt byte; radius then angle for each candidate, 6 draws plus conditional behavior-7 draw | facing 2 wraps base 256 to byte 0 before widening; attempt 255 becomes 0; no float coordinates |
| route_old, 0x1005e2b0–0x1005e37f | post-action quantity u16 | kind 12 if nonzero, no draw; empty: odd to 7/even to 12 with 1 draw | quantity 1 skips supplied draw entirely |
| wolf, 0x100532b0/0x10053410 | same local transition inputs, supplied callee outcome | replacement next-state rules, not animal rules plus extra draws | walk draw 5 uses %7 => walk 27, while New %5 => idle 28; idle modulus 1 still consumes |
| prey, 0x100536b0–0x10053732 and 0x10098513 | predator/prey widened ids and kind; supplied short/extended membership data; resolved zero-HP gap flag | mask/id admission Boolean, 0 draws | predator 503 admits short prey 117, not extra prey 118; prey 503 is not admitted; kind 49/64 inside, 48/65 outside |

Creation's climate byte is validated by the vector adapter; the helper receives that climate's already resolved table, not an unused selector or a global content lookup. The 32-entry ring, 8 neighbours and 3 walk candidates are the addressed algorithm's geometry/index semantics; a different algorithm would need an explicit new contract. They do not bound world figures, content ids or climate counts. Prey membership lists are supplied data, not a production registry of original ids.

## Acceptance table frozen before implementation

| Requirement | Positive proof | Negative/control proof | Evidence | Review |
|---|---|---|---|---|
| Ten exact decisions | all 65 existing vectors through C++ helpers; every specified key compared | unknown/missing/empty records refuse | wildlife vector adapter | Pending orchestrator |
| Draw order/count | successful and collision paths, cave omission, idle %1, quantity override | bounded underflow records preserve consumed count | helper tests and raw gates | Pending |
| Original-width arithmetic | byte facing before angle widening and attempt increment | separate wrong-facing and wrong-attempt mutations | named vector assertions | Pending |
| Membership/order | ordered replenish output and supplied prey lists | collision/swap-pop and predator-503 mutations | legal control plus intended failures | Pending |
| Explicit unsupported results | OQ-572 exhaustion, OQ-574 cave without entrance, OQ-585 zero-HP prey | no fabricated outputs or infinite retry | typed refusal tests | Pending |
| State-free dependency | runtime never references helpers; no initializer or saved field | base diff, fixture hashes, partial identity/pins selection | canonical gate and reuse proof | Pending |

OQ-578 transitive callbacks, OQ-582 float/x87 positions, allocation/lifecycle reachability, target resolution and complete actions/ticks remain unproved. Replenish selection order is not the later prepended-record visit order. Creation stops before manager construction and attach. Completion quantities/outcomes are already resolved inputs. This packet closes none of those questions and reserves no tag.
