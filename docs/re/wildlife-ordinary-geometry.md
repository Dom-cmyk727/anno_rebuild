# Ordinary wildlife creation geometry — 2026-11-19

WILDLIFE-ORDINARY-GEOMETRY-1. D = addressed GOG instructions; P = raw-file
parse; H = hand arithmetic; C = conditional exact-rational model; U = unobserved.
Research/reference only. Independent contract review pending. No production FP policy.
[Earlier helper](wildlife.md#2-creation-helper-0x10092ca0),
[import](../sim/wildlife-import-plan.md), [creation effects](../sim/wildlife-create-effects.md),
[table/CW qualification](wildlife-raster-fp.md), [existing Windows card](../testing/wildlife-raster-fp-capture.md).

## G0–G1: ordinary path and horizontal arithmetic

D: `AnnoWelt.dll 0x10092cad–0x10092cf9` uses island base I (ESI), not a manager.
Zero-extend origin words I+4/+6, chunk bytes +0/+1, and packed spawn byte +8;
`tx = origin_x + 8*chunk_x + (packed & 7)`,
`tz = origin_z + 8*chunk_z + ((packed >> 3) & 7)`. Packed high bits ignored.
The additions use 32-bit registers; the source-width maximum is 67582, so no
signed overflow occurs on this actual byte/word profile. This is not an engine cap.
The already reviewed completed cell has actual object 419, kind 225, output-0
goods 70 (independent final-field oracle row 47), so it is ordinary, not 0x145. Selection
ultimately yields figure type 528: the horizontal branch precedes species selection,
so 528 does not itself choose the position. Receiver remains the same island.

D: original binary32 operands: a at 0x100c1294 = 11184811/67108864;
one at 0x100c1278 = 1; horizontal scale at 0x100ca6f8 = 1.
Let Q be an arithmetic operation rounded to the incoming x87 precision/RC,
and S a binary32 store rounded with that RC. FILD of these coordinates is exact.
Each multiplication by scale is still an original operation, not a new unit choice.

| Draw & 3 / jump | X operations | Z operations | Binary32 stores |
|---|---|---|---|
| 0 / 0x10092da4 | (tx+a)*scale | (tz+a)*scale | X 0x10092db4, Z 0x10092e6f |
| 1 / 0x10092dcd | (tx+1)*scale - scale*a | (tz+a)*scale | X 0x10092deb, Z 0x10092e6f |
| 2 / 0x10092e01 | (tx+1)*scale - scale*a | (tz+1)*scale - scale*a | X 0x10092e1f, Z 0x10092e35 |
| 3 / 0x10092e3d | (tx+a)*scale | (tz+1)*scale - scale*a | X 0x10092e4d, Z 0x10092e6f |

D: arithmetic between the indicated stores remains in x87 registers. Outcome 2
retains scale*a across both subtractions. Do not replace the high expression with
`tile + binary32(5/6)` or with a mathematical exact rational 5/6.
H: at tile 1, high expression is 2 - 11184811/2^26. Nearest binary32 gives
0x3feaaaab. Adding the separately rounded high offset 0x3f555555 to 1 lands
exactly halfway, choosing even 0x3feaaaaa instead: a distinguishing wrong shortcut.
For this source-width domain, horizontal intermediate sums need at most 43
significand bits. PC53/64 retain them exactly; PC24 performs the same one effective
rounding as the final store (scale 1 is exact). At fixed RC all three PCs agree
on X/Z. RC still matters. This invariant is not a coordinate restriction for the engine.

## G2: the actual island height receiver

D: calls 0x10092d77–0x10092d7b (shared cave edge only) and
0x10092e7b–0x10092e87 receive I and the stored X,Z words. Standard tables
0x100ca19c, 0x100ca530 and actual load table 0x100cac2c all have +0xa0 =
0x10084cc0. Loaded-island constructor sets 0x100cac2c at 0x10094c68;
base constructor sets 0x100ca19c at 0x1008cbd9. This is not world+0xa0.

D: 0x10084cc0–0x10084d38 divides X/Z by binary32 scale 1 at 0x100c9c24,
then calls 0x100b5f90 to truncate each toward zero. That helper saves CW, sets
RC=truncate for FISTP qword, restores CW on normal return (0x100b5f97–0x100b5fac),
and returns the low signed dword used here. It does not impose nearest on arithmetic.
In the admitted nonnegative source domain, ix=trunc(X), iz=trunc(Z).
Read four unsigned bytes from I+0x14 with row stride u16[I+8]+1:
A=(ix,iz), B=(ix+1,iz), C=(ix,iz+1), D=(ix+1,iz+1), after subtracting
origin I+4/+6 from the index. Lookup each byte in float table 0x100e2d90.
No tile-object stored height, GFX, fertility, four-corner mean or renderer sampler is used.

D: 0x10084d3f–0x10084dd6 builds stored binary32 XYZ vertices with X0=ix*scale,
Z0=iz*scale, X1=X0+scale, Z1=Z0+scale. Query Y starts at stored +0. Try
triangle (A,C,B) first through 0x100b5390; on false try (B,C,D), then return
query Y (0x10084ddd–0x10084e00). The second Boolean is ignored: if both fail,
Y remains +0, not an average. Helper failures do not write query Y.

D: helper 0x100b5390–0x100b569b stably sorts the three vertices by Z, rejects
query Z outside the inclusive range or zero total Z span, then computes edge
intersections and inclusive X admission. It is piecewise triangular interpolation,
with binary32 spills and asymmetric height arithmetic, not bilinear interpolation.
For these axis-aligned unit cells the reached schedule is below. Operators round
through Q; named S stores round to binary32, and operands already stored stay stored.

| First triangle A/B/C, reached 0x100b5561–0x100b5687 | Exact operation/spill schedule |
|---|---|
| span | d=S(Cz-Az); unused middle delta S(Bz-Az)=0; e=S(Cz-Bz) |
| intersections | t=S(qz-Az); L=S(((Cx-Ax)*t)/d+Ax); u=S(qz-Bz); R=S(((Cx-Bx)*u)/e+Bx) |
| admission | L<=R here; accept L<=qx<=R, including equality; otherwise return false |
| left height | v=S(((Cy-Ay)*t)/d+Ay), at 0x100b5646 |
| right height | w=((Cy-By)*u)/e+By, retained x87 |
| final height | if L==R use w; else S(((w-v)*(qx-L))/(R-L)+v), at 0x100b5687 |

| Second triangle B/C/D, reached 0x100b544b–0x100b54e2 | Exact operation/spill schedule |
|---|---|
| span | d=S(Dz-Bz); e=S(Cz-Bz) |
| intersections | t=S(qz-Bz); R=S(((Dx-Bx)*t)/d+Bx); L=S(((Cx-Bx)*t)/e+Bx) |
| admission | L<R for ordinary interior positions; accept L<=qx<=R |
| right height | w=((Dy-By)*t)/d+By, retained |
| left height | v=((Cy-By)*t)/e+By, retained |
| final height | S(((w-v)*(qx-L))/(R-L)+v), at 0x100b54e2 |

D: comparisons use the stored edge words; do not choose a triangle solely from
exact fractions before those stores. Equality goes to first triangle if it succeeds.
The formulas above cover ordinary interior offsets, not arbitrary degenerate
triangles or nonfinite inputs. No branch-boundary tolerance is introduced.

D: source table initializer 0x1007c760–0x1007c780 stores accumulator as binary32,
then adds binary32 c=5368709/2^27 (0x100c9c20), retaining the accumulator;
384 iterations. Corner bytes reach only indices 0..255. Table[h] is NOT simply
h/25 or a fresh binary32 h*c multiplication. Table-init precision/RC is independent
of callback precision/RC; reuse the existing raster FP evidence, not an inferred default.

P/D: accepted PreparedIsland::terrain owns W*H raw HEIGHT bytes, as built by
wildlife_prefix.cpp setup. Current resource tiles are separate and supply no heights.
Fresh logical corner grid copies interior HEIGHT unchanged, final row/column zero
([fisher §5](fisher.md#5-height-corner-grid-and-water-census-fisher-re-1c-2026-11-13)).
The physical extra row is not represented. For an admitted interior cell all four
bytes already exist in prepared terrain. A fresh logical edge can use the proved
zero border; arbitrary later corner mutations, saved custom borders and physical
padding cannot be inferred from this compact Terrain and must refuse or take
explicit proved current-corner facts. The provider itself has NO bounds check:
negative/outside coordinates can alias memory. That is not a safe border policy.

## G3–G4: conditional numeric examples before reference

H: use authored table entries [0,1,2,4] for A/B/C/D to separate terrain inputs,
triangle and scale independently of the table initializer. At tile (0,0), PC53/nearest:
low stored coordinate a; high stored coordinate b=0x3f555555.
First triangle mathematically gives x+2z, second 2x+3z-1; actual schedule above
retains Q operations and spills. Four ordinary outcomes exercise both triangles.
Exact diagonal q=(1/4,3/4) belongs to the first triangle; a single binary32 ULP
above X=1/4 at fixed Z moves to the second. Wrong diagonal selection must be
challenged with an asymmetric/spill-sensitive height patch, not a flat example.
At tile 1 the high-offset shortcut discriminator above is independent of terrain.
Large admitted tile 67582 is formed by origin 65535/chunk 255/packed component 7.
This remains an authored width-bound example, not a real island location.

P/H: actual Tutorial imported position 47/chunk (10,2), origin (24,24), yields
tile (111,45), local (87,21); four raw corners are [48,48,48,48]. Source SHA256
225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6.
Authored position draw 6 chooses outcome 2, then species draw 0 with saved goods 0
reaches type 528 after 3 inspections. Position/height occur before that second draw.
This is source-derived terrain under authored draws/context, NOT original runtime.
Flat interpolation returns stored table[48] exactly under the conditional profiles;
it does not settle table-init CW. Specific word examples and checks follow after modeling.

U: full incoming CW/thread at creation remains unobserved; no operation here sets
precision/rounding. Known startup PC53 is not proof of callback CW. Arithmetic
also depends on previously initialized table words. Normal masked finite profiles
PC24/53/64 × four RC choices can be modeled independently; no one is proclaimed
faithful by this packet. Unmasked exceptions/abnormal exits are outside normal return.

## Proposed next bounded implementation contract

Input: supported ordinary selection capability/position outcome, source-backed island
origin/chunk/packed position and current four-corner provenance; explicit table words
or separately proved table-init profile; explicit callback precision/RC and normal
finite masked profile. Require same source/terrain identity as the imported plan.
Never substitute selected species id for the actual tile kind used by the helper.
Own output: X/Y/Z binary32 words, tile/home integer projection, exact provenance and
numeric qualification. Feed those words to existing creation-effects projector;
manager success/id/time remain separate supplied context. No World or allocation.

Errors: incompatible geometry/source, inaccessible corners, missing table/CW profile,
nonfinite/unmodelled numeric domain, outside admitted terrain; do not invent zero or
host-FP fallback. Preserve imported-prefix/maintenance/selector-only counts and add
no geometry draw: it consumes the already selected first draw, while omitted native
callbacks remain explicitly outside those counts. No native creation success inferred.

Acceptance: four offsets, tile-1 wrong-shortcut control, original flat-48 source,
nonflat triangles, exact/ULP diagonal, large source-width coordinates, independent
PC/RC table/callback cases, unknown-context refusal, owner destruction and unchanged
inputs. A software integer/rational rounding implementation may reproduce explicit
profiles; choosing the original runtime profile requires Windows observation, not
an executor policy change. Conditional geometry can be implemented separately from
that observation only if the orchestrator accepts the explicitly supplied profile.

## Conditional reference and finite boundary

C/H: `tools/wildlife_geometry_ref.py` takes a JSON array of kind/inputs/expected/
derivation records. It uses signed Fraction arithmetic, explicit significand
rounding (24/53/64 bits), nearest-even/down/up/toward-zero and binary32 stores;
no Python float, host FPU or reimplemented world generator. `position` consumes
one supplied raw outcome draw; `height` and `table` consume zero. These are local
model counts, not a new whole-helper/global cursor or a claim about omitted callbacks.
`table` has its own explicit profile; `height` receives four already stored table
words and a separate callback profile. No default original CW is chosen.

H/C: 25 prior hand-derived vectors: 14 position, 4 table, 7 height. Four offsets
on the [0,1,2,4] patch yield nearest Y words 0x3f000000/0x3f955555/
0x404aaaaa/0x3feaaaaa, triangles 1/1/2/1. Boundary (1/4,3/4) is triangle 1,
Y=7/4; one X ULP above is triangle 2 with exact 7/4+2^-24, tying to even 7/4.
The large high-offset significand is 8650603 at exponent 16. Original flat-48
source gives Y 0x3ff5c28f under table53/nearest, independent of triangle choice.
Table6 distinguishes PC53 0x3e75c28f from PC24 0x3e75c28e (existing hand proof
in raster FP note); this does not identify the actual initialization profile.

Reference admission deliberately excludes subnormal/nonfinite words, negative
source table heights, non-interior query positions and the signed-zero result
under downward callback rounding. Its rational zero does not model an x87 zero's
sign: that endpoint refuses instead of inventing a word. The addressed native
schedule remains specified; a later bit-exact implementation must handle that
sign or preserve the same explicit refusal. No claim of all native input coverage.
The proposed creation adapter should obtain four corners from the prepared terrain
and the proved fresh border, not let arbitrary supplied table words masquerade as
source binding. This research reference makes that numeric input boundary visible.

C: public CLI agrees with all 25 vectors; wrong expected value exits 1,
empty/malformed/missing/profile errors exit 2. Contract rejects three applied
actual-source mutants (separately rounded high offset, doubled table scale, strict
rather than inclusive diagonal). An echo-only program fails the contract on the
changed expected value, before source-mutation checks. Exact focused gate and
identity receipts are in the handoff; original execution and original CW remain U.

[Additional Windows observation card](../testing/wildlife-ordinary-geometry-capture.md)
reuses existing identity/context safeguards. No separate Windows assignment or
new owner FP policy is requested here. The orchestrator may release a conditional
source-geometry implementation or obtain those runtime facts first.

## Acceptance reconciliation (independent review pending)

| Requirement | Distinguishing evidence | Current result |
|---|---|---|
| Real ordinary receiver | island tables +0xa0 all 0x10084cc0; actual 419/kind225, selected528 distinct | D/P, no manager substitution |
| Four horizontal choices | authored four offsets; tile-1 high shortcut mutant | 14 position vectors agree; mutant rejected |
| Terrain provider | I+0x14/W+1, raw corners vs object height, two triangle edge spills | D, 7 height vectors agree |
| Branch boundary | exact diagonal versus next X ULP; strict-edge actual mutant | first versus second decision distinguished |
| Stored table/profile | retained c additions; independent PC24/53 table6 | 4 table vectors agree; wrong scale rejected |
| No chosen original CW | explicit independent profiles, unknown/malformed profile refused | conditional only; Windows NOT_RUN |
| Oracle cannot echo | changed expected fails, echo-only fails contract | executed negative control |
| Source witness | raw Tutorial four corners48/current tile419; authored draw6 | raw decode plus flat-height hand example, not original execution |
| No engine/state changes | no src delta, unchanged pins/import artifacts | preservation receipts retained |

### Focused review correction: callback profile and left-height spill

2026-11-19, P2 from independent evidence review. The first 25 vectors left all
height callbacks at PC53/nearest. They did not distinguish ignored callback PC/RC,
and omitting the first-triangle left-height store also survived. Those receipts
remain valid for their narrower scope; they did not prove these three predicates.
All 25 existing expectations are preserved; append 4 hand-derived height vectors.

H: let a=11184811/2^26, Rn=13981013/2^24, Ru=13981014/2^24.
All cases use stored X=Z=a in the first triangle, not a recomputed exact 1/6.
- RC up, corners [0,1,2,4]: R=Ru. Before the final store the exact scheduled
  expression minus 1/2 is 7456541/1250999956144128, between 0 and one f32 ULP.
  PC53 directed-operation errors cannot bridge either boundary. Final up word
  is 0x3f000001; the existing nearest control gives 0x3f000000.
- PC24/nearest, corners [0,3,0,0]: -3*a rounds to -1/2, right height 5/2.
  Product 55924055/2^27 rounds to 13981014/2^25. Dividing by Rn gives
  6990507/13981013, above midpoint 1/2+2^-25 by 2796203/469124949999616;
  final nearest word 0x3f000001. PC53's unrounded scheduled quotient is
  1876499889476949/3752999599996928, below that midpoint by
  22369619/3752999599996928; final word 0x3f000000. Both controls are included.
- Required left spill, PC53/nearest, corners [1,0,2,0]: stored left is
  9786709/2^23, right 2*a. Scheduled interpolant is
  1876499727297197/1876499799998464, below midpoint 1-2^-25 by
  16777215/1876499799998464, hence 0x3f7fffff. Omitting the spill instead
  places it above the midpoint by 100663293/3752999599996928, hence 0x3f800000.
  A conservative total PC53 operation-error bound 2^-48 is smaller than all the
  separating gaps above (the finite operations here stay below magnitude 4).

C: now 29 vectors (14 position/4 table/11 height). Contract adds separate
height-local ignored-RC, ignored-PC and omitted-spill source mutants. Each must
fail its specific appended Y assertion with exactly one disagreement while all
other controls execute. Existing high-offset/height-table/diagonal mutants remain.
Reference algorithm unchanged; only evidence/contract coverage corrected. Original
CW/runtime and all previously stated admission boundaries remain unobserved.
