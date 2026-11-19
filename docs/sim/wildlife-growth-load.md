# Original growth lookup — bounded source loader

WILDLIFE-GROWTH-LOAD-1, contract before implementation, 2026-11-19.
D = decoded GOG AnnoGame 2.0.0.5; H = hand-derived authored expectation;
P = parsed original file; C = executed conditional implementation; U = unproved.
See [preprocessing](wildlife-preprocess.md), [numerical leaf](wildlife-ground-prepare.md)
and [generic text format](../formats/data-text.md). Generic `text::ints` and its
six-value diagnostic are unchanged; they are not original growth execution.

## Addressed producer and supported grammar

D: loader `0x10006a80`, section match/setup `0x10006af6–0x10006b89`, clears
**0x100 DWORDs = 256 integers per array**, at `0x10006b4b–0x10006b6b`.
Default Size is 6×6. Id `0x10006be3–0x10006bfd` changes the signed integer id
and resets Proz/Bridge row counters, not staging. Size `0x10006c3c–0x10006c64`
changes dimensions; Proz `0x10006ca3–0x10006cd4` writes just the parsed prefix
at row*width. When height rows have arrived, `0x10006cee–0x10006d0a` allocates
and zeroes the 16 KiB primary map only if it does not already exist.

D: commit `0x10006d19–0x10006ddb` visits rows then columns, writes low parsed
byte at `((id & 255)<<6) | ((column & 7)<<3) | (row & 7)`. Later writes win,
including duplicate ids/packed aliases. GFX rotation bits are not in this index.
The row counter resets at `0x10006ddd`; no implicit incomplete-record commit
exists at section/EOF. `0x100072c1–0x100072eb` advances to the next line/section;
another WACHSTUM reenters setup, clearing staging/defaults, retaining the map.

D: exact-case identifier reader `0x10016250–0x1001629d`, start/comment scanners
`0x100161a0–0x100161dc`/`0x10016230–0x1001624f`, line skip
`0x10016210–0x10016228`. Numeric list `0x10016360–0x100163b9` continues only
at a comma after whitespace scan `0x100161e0–0x10016203`. A missing comma
stops that row; common line skip discards its remainder, not more list values.
Integer wrapper `0x10016300–0x10016326` uses base 0 through
`0x100173b2–0x100173c8`; signed decimal branch/sign/base selection at
`0x10017411–0x1001748e`, signed bounds/sign application at `0x10017557–0x100175b9`.

D: Bridge `0x10006dea–0x10006f61` uses the separate second staging/map (+4);
Klima `0x10006f66–0x100072b9` writes only map +8 and restores height at
`0x100072b5`. This loader returns the primary Proz lookup only. Validated
sidecar rows are counted, not exposed as recreated bridge/climate outputs.

Explicit modern validation boundary (U/unsafe variants refuse, not emulate):

- LF or CRLF lines, final newline, exact-case [WACHSTUM] section(s) and final
  [END]; blanks and semicolon comments allowed. No bracket inside comments,
  embedded NUL, quoted code, unknown section/key or non-ASCII code token.
- Colon keys Id, Size, Proz, Bridge, Klima. Canonical signed decimal int32 only
  (optional +/−); leading-zero multi-digit, hex/octal, overflow and malformed
  signs refuse, despite the original CRT's broader base-0 domain.
- One optional Size before the first Id of each section; positive dimensions,
  width*height ≤256 to fit the proved original staging array. This bounds the
  decoded original profile, not engine island/type sizes or the leaf's lookup.
- Id requires the preceding record complete. Exactly height Proz rows per Id;
  each has 1..width parsed values. Missing-comma numeric tails are discarded;
  malformed tails, trailing comma, extra values/rows and incomplete Id refuse.
  A new complete Id preserves all staging; repeated WACHSTUM clears it.
- Bridge numeric rows use the same checked prefix grammar and separate counter;
  incomplete sidecar batches refuse. Klima rows contain 1 or 2 uppercase ASCII
  identifier tokens. Their semantic outputs are outside this primary loader.
- Required structure, at least one complete Id and final END; no partial result
  is published. Original malformed EOF scanning/allocation failure is not copied.

P: supplied unchanged file has 1 WACHSTUM, final END, 146 Id, 876 Proz, 30 Bridge,
92 Klima and one Size 6×6. These observed counts are corpus evidence, not limits.
The real missing-comma row retains three zeros. Only an authored NONZERO prior
stage can distinguish correct retention from a wrong zero-fill.

## Reference-first acceptance

Authored source files and sparse expected bytes construct the COMPLETE 16 KiB
expected map (all unlisted bytes zero), never a digest-only core expectation.
Id 1, Size 2×2, rows [1,2]/[3,4] -> indices 64:1,72:2,65:3,73:4.
Id change, Size 3×1: [10,20,30] then Id 2 short row `1 9,8` ->
128:1,136:20,144:30, while Id 1's values remain. Repeated section instead
clears the retained stage, not the already written map. Signed low bytes and
duplicate/packed-alias overwrite have separate hand-derived cases.

| Requirement | Positive / wrong interpretation | Evidence planned | Review |
|---|---|---|---|
| Comma stop and retained staging | Nonzero prior stage, short and missing-comma rows | full-map authored comparison; actual wrong source | pending |
| Section/id lifetimes and order | Default 6×6; repeated section; duplicate Id; row/column index | independent reference before C++, full zero-region checks | pending |
| Signed/packed bytes | Negative/over-255 values, Id low byte, rotation exclusion | hand byte/index results and numerical-leaf composition | pending |
| Precise defensive refusals | Legal controls, then one malformed dimension/record/grammar change | unchanged source and no partial result | pending |
| Ownership/provenance | Result survives source destruction, owned diagnostics/source hash | C++ tests | pending |
| Real source | Exact unchanged file, full lookup compared with independent producer | opt-in bounded one-file comparison, hashes/raw map private | pending |
| UNWIRED | No original city/kernel/header/World binding; generic parser unchanged | caller/scope audit and focused canonical gates | pending |

Each call starts a fresh owned primary map; an original reload into an already
allocated catalogue buffer is outside this API. Repeated sections within one
source retain the map as proved above. No original tables, bytes or listings in git. No city proof, FRUCHTBAR capability,
fill composition, state/Rules/default/registry change or live wildlife claim.

## Implemented unit and proof boundary

`load_wildlife_growth(formats::Bytes)` in the focused sim_load header/source
returns an owned GrowthLookup: primary values, signed Id/dimension/source-line
record diagnostics, short-row diagnostics, sidecar counts and source SHA256.
The hash identifies source bytes; it is not a city, scene or completed-loader seal.
The generic text library is reused for lines/key classification, never its ints.

Thirteen authored complete-map cases precede C++ at `076170fa`; the public
contract runs 32 CLI controls, including preserving existing output on parse
failure and refusing source/output path aliasing. C++ compares every byte of each
zero-initialized expected map plus sparse hand values. Four direct cases pass
1,206 assertions with the real files and independent private byte oracle.

Five actual wrong-production-TU variants fail intended full-map assertions:
whitespace continuation 1; clearing staging per Id 2; zero-filling short rows 2;
wrong packing 7; clamping instead of low-byte storage 1. Legal private control
passes all 972 assertions; all 13 rows reach successful parsing. Canonical
source and binary are unchanged by those private probes. Echo-only agreement
is separately rejected by the reference contract's changed expected byte.

The one real-file case targets unchanged GOG growth source SHA256
`0b8c4416dd43fb76ea2dca6e82bfd822b0b861f15440e83479a8aa56b8d92e24`;
complete lookup SHA256 `760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7`
was independently computed before C++. With ANNO_WILDLIFE_GROWTH_ORACLE pointing
to the private reference output, it also compares all 16,384 bytes directly.
Without that optional artifact it reports this additional comparison unavailable
and checks the known full-map SHA; no hidden new prerequisite for ordinary runs.
No game directory means an explicit required-asset skip.

Reproduce the private artifact before the asset gate:

```sh
python3 -I tools/wildlife_growth_ref.py --source "$ANNO1503_GAME_DIR/Data/Wachstum.dat" \
  --write-lookup cache/wildlife-growth-load-1/original-reference.private
```

The asset gate supplies the absolute artifact path through the oracle environment
variable. This is a one-file proof, not another edition or full data-file grammar
claim. The original file itself is never changed.

Authored-only leaf composition uses Id 1 / rotation 3 and p 20 -> b 2 -> low 2.
Real-source composition uses Id 11 / rotation 3, p 100 -> b 10 -> low 10, with
AUTHOR-DEFINED object-null fact, 1×1 geometry, centre-only kernel and old ground.
It proves one source input reaches numerical computation, not an original island,
climate kernel, city, FRUCHTBAR branch or postload/fill sequence.

### Final evidence correction and gate reuse

After the valid first Debug 13/13 gate at `6e980f79`, the optional full-byte asset
comparison changed from CHECK to REQUIRE at `0dc52331`. This prevents an
agreement line after a mismatch. A corrupted private oracle now fails that exact
comparison with no agreement line; the legal oracle passes. Only this Debug
case reran (1/1); the other 12 unchanged outcomes are reused. ASan/UBSan executes
the entire focused 13/13 selection for the first time on `0dc52331`. Both broad
profiles separately run growth and ground reference contracts, each 1/1.
No whole gate repeats, original data changes or recreated historical failures.
The production loader, vector testcase and generic text library are unchanged
by this test-reporting correction. Exact counts/identities/hashes are in the
handoff evidence. No local independent approval is claimed.

### Focused discarded-tail correction (2026-11-19)

Independent review found that the Python reference's ignored-tail branch checked
syntax but missed int32 bounds. Before fixing it, the public source command
accepted one-over/one-under and a bad later comma token with exit 0. These actual
results are retained; no original-runtime or production defect is inferred.
The corrected reference invokes its bounded numeric parser on the tail and
DISCARDS the resulting values. Every ignored token must fit int32, matching the
existing C++ refusal domain; production/generic text/data are unchanged.

Legal INT32_MIN/MAX and a multi-token legal tail produce the same complete map
as the no-tail control. One-over/under and a bad later positive/negative token
refuse, preserving source and output. The updated public contract has 39
controls (initial candidate: 32), and the exact old check fails its new tail-over
control after the legal boundaries execute. C++ uses its existing generic
unsupported-numeric-tail diagnostic for these nested failures.

Only the boundary/refusal cases and authored agreement rerun: canonical Debug
3/3 and ASan/UBSan 3/3, plus the changed required reference contract each 1/1.
All 13 authored maps and the unchanged original lookup digest remain identical.
Previously reviewed production, original-file, ownership/ground/text and five
causal controls are reused: their source, data and expected results are unchanged.
See the [focused evidence](../orchestration/evidence/wildlife-growth-load-1-fix.md).
