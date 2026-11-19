# Wildlife raster: two numeric checkpoints — 2026-11-19

WILDLIFE-RASTER-FP-1; documentation and tiny private models only. D = original
instructions, H = independent hand arithmetic, C = private model execution,
U = unproved. No original runtime observation or production numeric policy.
Base `8217e187`; [raster contract](../sim/wildlife-secondary-raster.md) remains
conditional until the two checkpoints below are distinguished.

## Acceptance and hand expectations before the sensitivity model

| Check | Independent expectation / primary evidence | Discriminator | Status |
|---|---|---|---|
| Table entry versus callback entry | CRT reset writes PC only; no same-thread/full-CW inference | Record each checkpoint's thread and full control word separately | U: exact capture card supplied; review pending |
| Retained table accumulation precision | With nearest, T53[6] is 0x3e75c28f; T24[6] is 0x3e75c28e | Callback fixed at C53/nearest, full height 6 converts 6 versus 5 | C agrees; not a claim that original reaches T24; review pending |
| Table store rounding | T53/down[248] is 0x411eb851; T53/nearest[248] is 0x411eb852 | Callback fixed C53/nearest, four samples convert 247 versus 248, rounded 15 versus 16 | C agrees; review pending |
| Callback precision | Table fixed T53/nearest[5] is below exact 5*c | C53/nearest converts 4; C24/nearest rounds quotient to 5 before integer truncation | C agrees; review pending |
| Callback rounding | Fixed T53/nearest, ordered samples 255,255,255,227 | C53/nearest final sum is 41607496/2^20, down is 41607492/2^20; raw conversion 248 versus 247 | C agrees; review pending |
| Independent profiles | Table and callback are independent inputs | Opposing errors may cancel in an output; retain table bits too | C T53/down plus C24/up also yields 248; review pending |

H: c = 5368709/2^27. At the fifth retained addition, 5*c has numerator
26843545; precision 24 nearest rounds it to 26843544/2^27. Adding c again
rounds 32212253/2^27 to 32212252/2^27, one f32 ULP below exact 6*c.
Precision 53 retains these sums exactly. For table[248], exact numerator on
denominator 2^24 is 166429979; the f32 grid there is 16 units, so down chooses
166429968 and nearest chooses 166429984. These are different ORIGINAL-operation
models, not a proposed rational 1/25 replacement.

H: table[255] = 10695475/2^20, table[227] = 9521070/2^20 in the fixed
T53/nearest profile. After three 255 samples the f32 numerator rounds to
32086424 (nearest/down); adding 227 gives 41607494. Final nearest-even rounds
to 41607496, down to 41607492. Dividing nominal area 4 then c straddles raw
248; the carry/shade distinction remains after the converted-byte mask.

## 1. Two-checkpoint verdict and finite edge ledger

Verdict: **the full numeric profile is NOT statically closed at either checkpoint**.
The reset's PC53 result is proved, not the complete incoming table-store control
word or its equality to the later callback. No actual thread ids were observed
on this Mac. Model sensitivity below shows that the missing conditions matter.

| Edge / addresses (preferred VA) | Thread and entry fact | Write / normal restore / resulting knowledge |
|---|---|---|
| Loader ->Welt entry 0x100b6b78 ->CRT 0x100b6a9f | OS loader entry, not a game World call; actual thread id U | Incoming full CW U; no chosen rounding default |
| CRT hook at 0x100b921a ->0x100b5f30 ->0x100b6e12 | Local synchronous startup edge | controlfp value 0x10000 under mask 0x30000 maps to x87 PC53; deliberately persists on return, no RC/exception-mask reset |
| CRT initializer array ->slot 0x100d0d64 ->0x1007c750 ->0x1007c760 | Same executing thread locally; no equality with later Frame load established | Table writer performs f32 stores and retained additions under active CW; no CW write of its own. Earlier initializer/runtime edges are not an exhaustive restoration proof |
| Frame vtable reference 0x100879c4 ->0x1002a190 ->call 0x1002a3f8 ->0x10029100 | Normal start local direct chain; caller/thread identity at the vtable entry remains U | No explicit CW assignment in this addressed local sequence; cannot infer preservation through every outgoing call |
| Frame 0x100291af ->0x10077a56 ->IAT 0x1008604c | PE import resolves CreateAnnoSceneInstance, not an anonymous guessed function | Known cross-module edge; not a CW-preservation promise |
| Scene 0x10002e40 /0x10002ea6 ->thunk 0x1000b080 ->IAT 0x10031000 | Resolves CreateAnnoWeltInstance; Scene imports Welt statically, Frame imports Scene | World creation is distinct from one-time DLL table initialization; not evidence of same original thread/history |
| Loaded island ->world +0x1d0 ->0x100aba70 ->0x100a3cc0 | Standard world table 0x100ccdd8: +0x1d0=0x100aba70. Callback entry selected by return 0x100aba99 and flags 2 | Arithmetic entry 0x100a3cce does not set PC/RC; full callback CW U |
| Raster width/height +0x54/+0x58 ->0x100a3910/0x100a3920 | Same callback thread, mov/ret getters | No FP control write |
| Raster sample +0x5c ->0x1009db00 ->island +0x98=0x10084c20 | Resolved standard island table 0x100ca19c; sea returns zero | Lookup/fld path has no CW writer; no invented renderer or object callback |
| Raster conversion ->Welt 0x100b5f90–0x100b5fb6 | Same local caller on the normal returning path | Saves CW, sets RC=truncate at 0x100b5fa6, fistp, restores saved word at 0x100b5fac; preserves PC/RC/masks on normal exit, not every status flag or trapping exit |

The [Windows loader contract](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)
places initialization in the thread that caused loading. That does not identify
which original thread later invokes the Frame start interface. Frame really
creates a worker at 0x10019350–0x1001938a (CreateThread IAT 0x100860f8, entry
0x10019230); this is NOT evidence that the worker runs scenario loading.
Neither invent a thread handoff nor assume one thread from these facts.

The unresolved boundary is concrete: full CW entering the loader/static
initializer and full CW/thread entering Frame's start interface after original
runtime/external history. The local chain alone neither mutates nor proves
preservation across that history. No unbounded caller search is needed to state
the missing observation, and no policy decision can replace it.

## 2. Writer/restore inventory and its limits

D/search: all executable sections of the six pinned GOG modules Welt, Frame,
Scene, Game, View and Display were swept, skip-data enabled, for fldcw, finit,
fninit, frstor, fxrstor, xrstor and ldmxcsr. There are 58 fldcw sites: 11 each
in Welt/Frame/Game/View/Display, 3 in Scene; no other listed instruction was
resolved. Every site's address/mnemonic/operand and relevant thread imports are
retained in fp-writers-and-thread-imports.json. This is an instruction inventory,
not a declaration that all 58 execute on this load path. Data decoded as code,
unread computed instructions and other modules remain outside that scope.

Directly resolved relevant wrappers:

- Welt control87 0x100ba3c0–0x100ba3f0 reads old CW, replaces only the requested
  abstract mask, maps it and loads the result at 0x100ba3ed; it does NOT restore
  old CW. controlfp wrapper 0x100ba3f5 masks denormal-control bit 0x80000 out of
  its mask. The resolved direct caller is reset 0x100b6e12. This is a persistent
  precision change, not temporary truncation.
- Scene has the same resolved reset shape at 0x1002851a–0x1002852b, calling
  controlfp 0x1002d8d8 ->control87 0x1002d8a3; fldcw 0x1002d8d0 persists the
  requested masked value. Direct-call hits are reset 0x10028524 and wrapper
  0x1002d8e6. It does not establish callback nearest rounding either.
- Scene conversion 0x10027a90–0x10027ab6 sets truncate at 0x10027aa6 and
  restores at 0x10027aac. Frame conversion 0x10077da8–0x10077dce does the same
  at 0x10077dbe/0x10077dc4. Welt's actual raster conversion is the matching
  0x100b5fa6/0x100b5fac pair. Each read has one normal return after restoration.

PC bits are CW &0x0300 (0=24, 0x0200=53, 0x0300=64; reserved encoding is
not modeled). RC bits are CW &0x0c00 (nearest/down/up/toward-zero =
0/0x0400/0x0800/0x0c00). Exception masks are the distinct low six bits.
Welt startup fnclex 0x100b5f44 clears exception STATUS, not an RC/PC assignment.
No inference that an exception-mask value selects nearest, or that CW restoration
restores all status flags, is made. Unmasked traps and abnormal exits remain
outside the normal-return proof; capture the full CW and status too.

The other inventoried CRT/math sites are not proved reachable or universally
restoring on this concrete path. Nor are foreign/system/driver calls certified
to preserve CW by a negative scan. We stop at those bounded facts rather than
claim every possible process history has been audited.

## 3. Executed sensitivity, not original observation

The unchanged prior exact model first re-executes all 31 original-profile checks,
exit 0. New sensitivity.py independently selects table precision/RC and callback
precision/RC, retaining the accumulator, rounding f32 memory stores under the
relevant profile, preserving sample order, nominal area, c bits and integer
truncation. It reproduces the six prior sample/count/merge controls first.

```text
MODEL executed=31 failed=0 profile=precision53_nearest_f32stores original_execution=NOT_RUN
FP_MODEL executed=19 failed=0 independent_profiles=1 original_execution=NOT_RUN
```

19 = six prior sample controls + nine profile rows + four table-bit checks.
Four paired rows vary ONE dimension at a time and produce the hand-predicted
differences above: T precision, T rounding, C precision, C rounding. The fifth
profile row uses T53/down and C24/up: it also produces raw 248/merged 0x90,
despite different table[248] bits. Output agreement alone can hide two different
profiles. Additional unasserted samples explicitly report up/toward-zero and
precision 64; they are not extra accepted checks or a universal equivalence proof.

All four modes/24/53/64 model configurations are mathematical sensitivity
alternatives; this is NOT evidence that the original reaches each one. Neither
table and callback profiles nor their likely equality is chosen by this packet.
No original executable, x87 emulator, new numeric policy or public reference
mechanism was installed or changed. Full static closure stops here with the
[bounded Windows capture card](../testing/wildlife-raster-fp-capture.md).

## 4. Next independent bounded task, not released here

While that capture is coordinated, WILDLIFE-PREPROCESS-CONTRACT-1 could read
only the first callback's c2-zero preprocessing (Welt 0x10084370–0x10084818)
and its gate at 0x100a1810: climate/kernel/source inputs, ordering and exact
effects before the already proved fill. This is an existing unresolved postload
dependency independent of choosing the raster FP policy; record any conversion
dependency separately. About two hours, research only, no broad corpus or World
publication. It advances the remaining initialization contract; it is not
automatic permission to start or to implement live wildlife.
