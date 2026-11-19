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
`0x10017411–0x1001748e`, signed bounds at `0x10017557–0x100175a2`.

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

No original tables, bytes or listings in git. No city proof, FRUCHTBAR capability,
fill composition, state/Rules/default/registry change or live wildlife claim.
