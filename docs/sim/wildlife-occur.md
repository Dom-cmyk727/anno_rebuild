# Transient ore occurrence import

WILDLIFE-OCCUR-1, 2026-11-19. D = GOG AnnoWelt instructions; P = original parsed
input; C = authored execution. Scope: reached ERZSYMBOL/ore class only, in the
[ordered prefix](wildlife-prefix.md), with [BERGWERK lists](wildlife-bergwerk.md).
No World publication, scene assignment, mining production or normal loader change.
Independent review pending; original records/listings stay private.

## Primary contract and acceptance before code

D: OCCUR arm `0x10089714–0x1008974e` calls island +0x28/table `0x100caf34`
reader +0x18 `0x10095eb0–0x10096201`. It handles small tagged ENTRY2; other tags
skip. First 16 bytes are packed u32 plus local binary32 X,Z,height. Type is bits
14–25, resolved as a figure descriptor, never a building/goods id. Ore's property
+0x4c is the no-op `0x10001020`, so bits 26–29 and creation property 0x23 have
no ore state effect. Low 14 bits and bits 30–31 are not used on this path.
Class 0x91/145 is the bounded supported factory arm; unknown types/classes stop.

D: `0x10095fd0–0x10096032` adds island origin to X/Z at scale 1.0
(`0x100cadf0`) and stores binary32; height is copied unchanged. The loader checks
one enumerated candidate (`0x10095770–0x10095ac4`): first occupied chunk at or
after the target's local chunk in row-major scan (later rows restart at column 0),
then first kind-6 node in that chunk. Only matching descriptor class and matching
truncated X/Z (low u16 comparison) skip creation/state loading. It is not a search
for every identical object; a different head can hide an older duplicate.
Bitmap row helper `0x10078df0–0x10078e1a` only scans nonzero bitmap words.

D: create `0x10095100–0x100952a0`, factory ore arm `0x10094e34–0x10094e68`,
constructor `0x100637a0–0x100637c4` -> base `0x100648a0–0x100648c3`.
Default quantity Q = 512000 raw (2000 tons × 256), reserve R = 1, discovery = 0.
Attach `0x10063750–0x10063793` stores the figure descriptor, then island through
base `0x10064740–0x10064789`; descriptor id 1661 specifically sets Q = 102400
and R = 0 before state load. Position setter `0x100320e0–0x100320f7` copies the
three binary32 components. WARE is the descriptor's goods getter through
`0x10063640`; support here is parsed goods 1–8, not an invented amount.

D: insertion `0x10095175–0x10095289` prepends one kind-6 node to the SAME chunk
list as BERGWERK kind 1, writes unsigned local offsets mod 8, ORs presence bit
0x40, sets its coarse occupancy bit and increments occurrence count. The bitmap
is derived from this list presence in the transient model, not a second saved
index. List kind getters and next setters are pure; no additional factory or draw.

D: state loader `0x100634e0–0x100635c6` reads state ONLY when remaining length is
8: u16 tons at 0 becomes Q = tons << 8, padding u16 is unused, discovery u32 at 4.
For goods 1–8, R != 0 and Q < 409600 (1600 tons), reset Q to 512000. Equality
is retained, not reset; R is never loaded. Other state lengths are skipped,
leaving constructor/attach defaults and discovery 0. Goods 0x84 has another
branch, outside this packet's proved goods domain. No later scene retyping or
world depletion rule is applied. Accepted ore research at
`b12d22b39caee90429aef77b52659aa2bfedb38e` supplies the separate later semantics.

D: AFTER insertion and state loading, `0x10096124–0x10096171` calls BERGWERK
visitor `0x100785a0–0x100787ae` over u16(trunc(world X/Z) − 7), size 15 × 15.
Visitor visits local chunks from clipped rectangle−3, then only kind-1 nodes
whose reconstructed anchor lies inside the original half-open rectangle. Anchor
uses signed nibble offsets plus actual visited chunk base, not entry source X/Z.
Callback `0x1007c0d0–0x1007c27a` resets mask to 0x10, queries occurrence kind mask
1 through `0x10095480–0x100956a7`, and ORs `1 << (goods−1)` for nearby sources.
It ignores Q/R/discovery, including zero-quantity sources. The query clips its
u16 rectangle, visits chunks y/x, traverses kind-6 nodes in list order and tests
half-open integer positions reconstructed from local chunk-byte coordinates.
Row-radius bounds are 7,7,7,6,6,5,4,2; negative row distance transforms to
1−distance−footprint height, then clamps to 0; column analogously transforms with
footprint width and only tests <= row bound. Footprint follows node rotation.
No world draw occurs in this finite reached chain. Post-island/global callbacks
remain outside scope; no complete cursor or mineral-economy claim.

| Requirement | Hand-derived distinguishing control | Disposition |
|---|---|---|
| Typed packed fields / binary32 load boundary | Authored type 501 at local (6.5,7.5), origin (16,24), upper bits set; world (22.5,31.5), tiles (22,31) | pending |
| Constructor/attach/load threshold | Reserved tons 1599 -> Q512000, 1600 ->409600, 1601 ->409856; type1661/R0 with tons0 ->0 | pending |
| Default state lengths | No 8-byte state leaves Q512000/R1 or Q102400/R0, discovery0, never invents file tons | pending |
| Shared insertion / duplicates | Kind6 prepends over kind1; identical head skips load; different head in same chunk permits another older-position duplicate | pending |
| Mask refresh | Goods1 plus3 ->0x15, duplicate1 ->0x11; zero-Q deep source still contributes; outside radius resets only reached anchors | pending |
| Framing / ownership | Fewer than16 base bytes, unknown type/class/goods, malformed tags, nonfinite/out-of-bounds positions refuse diagnostically; source destruction safe | pending |
| Actual-code negative | Wrong <=1600 reset must fail equality assertion after legal creation | pending |
| Same original once | One ore record loads, shared node/head and BERGWERK masks checked; stop next unknown header | pending |

Raw binary32 components are typed uint32 bit patterns in the owned transient
result; integer tile coordinates drive the reached list/query rules. Content
boundary conversion reuses the project's existing binary32 load convention;
no float enters saved/checksummed sim state. Nonfinite/out-of-range and unsafe
matrix coordinates are defensive refusals before the corresponding entry effect.
Original malformed I/O/allocator failure is not emulated. Dynamic query traversal
has no original stack-buffer capacity; no undefined buffer-overflow behavior is
claimed. Existing derived caches/other flag categories retain their prior future
consumer initialization obligation, not an omitted resource-mask effect.

## Pre-gate discriminators

Authored controls pass 782 assertions plus 70 radius/edge assertions. At anchor
(23,30), source (16,28) has dx 7 / row distance 2 and contributes; source (16,27)
has row distance 3 / row bound 6 and does not. The actual visitor excludes the
right-column duplicate nodes in both cases. At tile (0,0), u16(0−7) wraps and
refreshes no BERGWERK node; no signed clamp is invented.

One actual adapter-TU mutant changes only `< 409600` to `<= 409600`. Legal
threshold case passes 426 assertions; mutant fails exactly the equality Q check
(512000 versus 409600), with 425 controls passing. Canonical source is untouched
and rebuilt afterward. No inherited mutant labels or second campaign.

Quantity is stored Q, not the shallow extractable amount: Q 512000 / R 1 leaves
409600 reserved in the separate existing getter contract. No extraction or later
scene retyping/depletion is executed here. Position additions use read-only
GOG `.rdata` scale literals 1.0; no runtime scale-setting contract is inferred.
