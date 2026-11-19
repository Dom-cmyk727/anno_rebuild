# WILDLIFE-RASTER-CONTRACT-1 — Mac orchestrator review, 2026-11-19

**LOCAL REVIEW APPROVED within the conditional research scope.** Exact-head CI
is pending; this is not yet ACCEPTED and authorizes no runtime raster port.

- Head `8217e1879aead7fa3b451c155b060f0a05405874`;
  tree `df33087d82cdd7b6685e0240b17abdb8b5d62fa8`.
- Base `74d50bfeedf1a48ab7e140cc66e36859e0d0a9c3`; production unchanged
  from FILL `889143ba`.
- [Exact-head CI 44461780207](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44461780207)
  in progress at review. Five documentation files only.

## Independent checks

Primary reviewer `/root/review_docs_sim_dispatch` independently checked original
bytes for the PC53 reset, retained table accumulator, borrowed minimap plane/
generation and full-byte palette reader. The reset effect is correctly separated
from table-store rounding and the later caller control word. APPROVED within the
declared research scope, with no blocking findings; the next bounded FP packet
also passed independent scope review.

Evidence reviewer `/root/review_town_ci_close` approved the 31 checks and their
counterexamples: 10 merge, 12 sample/merge, 3 rectangle/generation, 5 flag and
1 wrap checks. Wrong nibble masking fails exactly four carry checks, including
modeled legal prior colour 8; skipping the empty increment fails the aligned
empty case. Unaligned empty still visits a bucket. Independently derived the
fractional scale 3.125, 49 samples / nominal 39.0625, converted 319 and shade 4;
clipped one-sample height 100 retains nominal denominator and converts to 25.
The six ARM float cases are conditional transcriptions, not original execution
or general equivalence. No actionable evidence findings.

Root verified 72 manifest hashes and all 25 published receipt rows, exact head/
tree and documentation-only diff. No gate was repeated or invented. Producer,
reader and scope claims are separated from modeled arithmetic. Neither the
sampled agreement nor the CRT precision reset closes runtime rounding/thread
history. Original observations, broad consumer/save closure and full World/live
wildlife remain NOT_RUN or unresolved, as disclosed.

## Next bounded step

[WILDLIFE-RASTER-FP-1](../tasks/wildlife-raster-fp-1.md) is independently scope-
reviewed and released on this head. Trace the table-initialization and first
load-callback numeric conditions separately. Stop at a precise Windows capture
card if a reachable external/thread edge prevents closure; do not repeat an
unbounded static search or assume nearest rounding from precision 53. The packet
makes no production, compatibility, World or default change. Owner navigation
arithmetic/compatibility decision remains separate and pending.

The executor ordinary-pushes its task branch and reports directly to this
orchestrator for review and the next bounded task. Main untouched.

## Terminal CI and acceptance

RASTER-CONTRACT `8217e1879aead7fa3b451c155b060f0a05405874` is now **ACCEPTED**
within the conditional research scope. CI 44461780207 has all 12 jobs successful
at that exact head, including all five C++ jobs and the aggregate gate. Hosted
jobs receipt SHA-256:
`64f467fae84f1d679d8a2247a829835a576cc1965265490d5d696f2ad0de9a1e`.
No local gate repeated. Original runtime FP conditions remain unobserved; the
subsequent reviewed capture card is a request, not completed Windows evidence.
