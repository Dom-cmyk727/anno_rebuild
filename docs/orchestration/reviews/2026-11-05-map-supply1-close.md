# MAP-SUPPLY-1 / CLOSE — orchestrator acceptance

Accepted for the documented headless scope on 2026-11-05, after direct review
and independent read-only correction review. First Mac branch:
`task/MAP-2-original-like`. Final executor corrections: `eda3441d`; accepted
local continuation pin: `67158af4d29d87fed8686dc6542ca2a9f4b1f75c`, the
orchestrator's docs-only removal of a remaining over-strong potato-site claim.
No gameplay integration or branch publication is implied by this acceptance.

The package demonstrates crop/grazing production, delivery, household supply,
alcohol interruption/recovery and persistence within its fixtures. The corrected
food ledger counts only the target town's four farms: **1,183 raw food**, not
the withdrawn 2,496. Purchases remain 756 with farms and 237 without. The named
causal fixture starts both city stores at 256; it asserts the control purchases
food and produces zero small-farm food in that town. The first failure, caused
by counting another town's production, remains in `S8_attempt1`.

Evidence under the executor's ignored `cache/map2-supply1/`:

| Scope | Source | Result | Log SHA-256 |
|---|---|---|---|
| Broad Debug | `437a3af6` | 1434 executed, 3 opt-in skips, 0 failures | `22878ff54467da40a0abce91f24b85ef152463a0bdb08b0ab04f74efef05fa03` |
| Broad ASan/UBSan | `437a3af6` | 282 executed, 2 opt-in skips, 0 failures | `7e6a8cb93f2919ab6ee1be42e67212a97d7cc2cd9dd4fcfac4fb677a326a5e84` |
| Corrected food Debug | `d37423a4` | 1/1 executed pass | `af80d6b55e45898fb38fc34a2da34f5aacb46e77c01912e38952bc5c25940cd6` |
| Corrected food ASan/UBSan | `d37423a4` | 1/1 executed pass | `fac7cfb126bc9650975bedeb256f3f3d41100b644ac80b4fecf50b5da468efb8` |

Broad results do not cover the later food-test edits; the focused runs do.
Later documentation does not alter engine behavior. No repeated broad gate was
needed for this correction. Stream 129 / PRNG timeline 5 is the local baseline;
`house_updates` remains false by default.

No matching potato site was found among the 460,357 examined interior footprints.
Border sites and founding-territory effects remain untested. Ship-founded alcohol
feasibility is unresolved. Balanced economies, cross-platform builds/runtime
observations, full-suite sanitizer coverage and live rendering are not established.

Next: [MAP-NEEDS-2](../tasks/map-needs-2.md), original cloth and leather chains.
Its release does not integrate this branch with main, scripting or Windows.
