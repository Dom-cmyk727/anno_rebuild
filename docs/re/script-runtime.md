# MAP-SCRIPT-1 original execution/permission evidence

Static original-byte/data evidence; original runtime NOT_RUN. Originals remain
read-only. System Python pefile2024.8.26/capstone5.0.7; bounded disasm/xref reads.
Module SHA256:
- Scene:5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38
- Welt:f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2
- Game:ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc
- Frame:dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91
Raw derived reads/inventory stay in cache/script1 (not committed).

## Real caller, scheduling and restoration

Earlier annoscene§12/state O-34 misattributed Frame1005fe2e: it calls the network
connection at client+3c4, not the script. Correct chain independently checked:
Scene kind0 stores script+15c, creates it at06278->0b090->160b0, vt1003b7b0.
Scene getter+a8=02ef0 returns it. Frame29209..29222,2083b..2085e,585a4..585b5
pass it to client+ec=5b0f0, storing client+4. SP5b161 and MP5fef6/5ff7e call its
slot+20 with world property4 ABSOLUTE CURRENT TICK before world+84 advances
5b16a/5ff12/5ff9a. Commands ExecuteTurn precede MP simulation steps. SP skips
nonpositive pieces at5b138; a rebuild zero-visit API is an explicit adaptation.

Scene step16b70 through its final return records arg+70 even when globally
paused, checks pause, then cursor. First cursor-null call runs INIT list+14 when
SCHUETTI is nonzero, clears it16bbe, sets first IF16bc5..c8. Constructor16108
sets origin+3c=0; independently scanned kind0 methods160b0..170b5 contain no
other writer.16bcc..16bf3 computes signed32 wrapped product/add, signed quotient
truncating toward zero /250. Clamp QUOTIENT<0 or>count tocount, not numerator<0.
16bf9..16c1e visits that many IFs round-robin, saving/wrapping cursor+40.
Separating count4 ticks31/32/94/157/219 ->q0/1/2/3/4. Count1,arg-126 ->q0;
arg-375 ->negativequotient, clamp1. Count4,arg1073741793 wraps numerator to1,
q0. A delta-budget or origin-update transcription is wrong.

Kernel reader1c9e0 creates implicit SCHUETTI and SET defaults. Section reader
16c30..170b5 calls it16fb6; SIGNALS parser23800 at1706a queues saved bool rows.
Only after all sections,1708c calls16360..1641d: lookup23e00 at16384, stored
saved value ->live symbol+4 at163d1/d4. First visit therefore sees restored flags.
Native save16470..1650f writes kernel text/signals/camera. Signal writer162e5..
16349 writes names/truth values, no cursor/origin. Original text reload resets
cursor(first IF) but restored false SCHUETTI suppresses INIT. Native cursor
persistence in the rebuild is the task's explicit continuity adaptation.

Load-before-first-visit proof: SceneCreate1150 invokes load vtable50 at01232,
returns scene only1248/124f. Kind0 load08460->025a0 dispatches children at0270f/
0286f; unknown-recorda396 calls Worldvt78=ae270 synchronously. SPIELER handling
ae3cc..ae472 resolves/creates players and calls player vt54=1d240 before return.
Frame291af calls imported CreateScene (thunk77a56 ->IAT8604c), attaches world
291c5..291f5 and only then script29209->29222. BIGBROTHER parse062e5 executes
no INIT. Player import is complete before any first script step.

## Expression and variable semantics

Condition parser22bf0 recurses over entire remaining text at22fbd/23091;
connectives equal precedence/right-associated, parentheses recurse22d01.
Evaluator10290 evaluates left once;102af..b4 returns immediately for false AND,
including suppression of ELSE on that root. OR always evaluates right102c1.
A=false,B=true,C=true: A&&B||C ->false/no-branch, not C-precedence true.
A=true in A||B still reads B. Pure truth-table helpers are insufficient to prove
callback evaluation counts; runtime vectors must count reads.

SET1cf6b/1cfd7/1d076/1d093 creates known Boolean signals (defaultfalse);
1d0db writes canonical dword0/1. Variable parser22940 rejects unknown symbols;
bare/==TRUE positive,==FALSE negates22aee. Query117c0 returns truth of dword+4;
leaf10840 chooses lists. Blockassignment parser1b5ab..1b740 requires existing
symbol, '=' and bool; action1c5f0..fd writes immediately. Oversize/duplicate
original SET can log/skip; strict compile refusal is a safety adaptation.

Numeric leaves10490..10525 (stop before jump table10528) compare signed32 query
results/literals for==,!=,>,<; first slice retains parser-supported==,>,<.
PLAYTIME query11f00..1b uses signed currentcallarg+70 /500, not a wall clock.
TIMER_i query11ed0..ee calls World's timer object. Welt getter9cd70..82 uses
unsigned counter/500. Timer set9cd90..a8 stores wrap32(seconds*500), no flag
change; start9cdc0/stop9cdb0 toggle a byte. Scene TIMER_SET branch183eb..184c7
of parent action parser170d0 builds vt1003b8f4(+4=13d40..5f) calling timer+4.
START branch184ca..18527 builds vt3b8ec(+4=13d70..8d), timer+c; STOP branch
1852a..18587 builds vt3b8e4(+4=13d90..dab), timer+8. These ranges are branches
of the action reader, not standalone functions. Safety validates timer indices
and bounded integers rather than reproducing out-of-range original reads.

## Permissions, ALL filter and raw import

Availability predicate remains build-availability.md. Setter1bca0: property1
stores low8 of raw third argument at tree node+e;1bce4 grants ONLY rawu32==1.
Raw257 stores1 without grants. Individual Scene15fa0 normalizes compiledbool.
Ordinary stage grant1bda3..bdb4 directly ORs1<<stage into player+90, without
population bit26 propagation. Stage31 marks FORSCHTYPE including0, then only
output0 when signed46..55 and goods.Forsch nonzero. It never grants output1.
1006TRUE marks4, not9, though availability OR accepts{4,9};1012 marks38 and12;
1341 marks49 despite UNUSED output0. FALSE afterTRUE keeps grants but overrides0.

ALL handler15fe0..160ac uses player+c4(3)=Volk (Welt1beb3,u16+7a), bit1<<Volk;
iterates Gamecount(0x700)=4000, find037e0, skips null slots. Type+d0=Game0a5f0
reads u16+14: loader0942e..449 derives (HEAD25&1)|(HEAD2e<<8). TestAH,3
excludes HEAD2e&3. Typeproperty1c->0a8d8 reads u32+1d8, loaded093c9..cd from
HEAD0c..0f. Filter is that folk bit AND no exclusion, not flags_word+12.
Original Volk1 selects461/921types;801,501,1001 qualify;2801 lacks folk1;
2229 has mask3 but HEAD2e=2 and is excluded. Individual2229TRUE still grants
stage14. ALL updates one mapped player in ascending type id; no all-player sweep.

Mapping parser23400..23527 resolves signedslot viaworld players at234a8;
null falls back31 at234b4..bb. Lookup23ea0..23f37 is case-insensitive, prefix
before '@', first matching declaration. Unknown name or individual unknown
type refuses the kernel (1a67c..1a7f0); only TRUE/FALSE action values compile.
Engine safety must not silently map failures to the host.

Player ctor21600 creates empty tree2170e..16 and defaultsVolk1/flags1 at21774..
78. Full player loader1d240 ends1e8df (alternate failure1e8ee). Child records
are processed on-disk order. ANLTRG branch1e4aa..1e5ea reads4-byte rows into
scratch zeroed only once; signed remaining>0, actual read count subtraction.
Short rows can reuse earlier bytes; no registry validation occurs. Unsignedu16
keys, duplicates last-wins1e5da; raw value byte, pad ignored. Import never calls
setter side effects. HEAD/research restore are independent record handlers.
Full saver1e920 contains ANLTRG branch1f21d..1f2e2: rowbuffer+24..27 zeroed at
1f234 (after PUSH offset adjustment), key/value overwrite first3 bytes, pad0.
Strict row-length/known-id validation is an explicit safety adaptation.

Independent S1 reviewers: /root/script_semantics_review and
/root/permission_semantics_review derived these cases read-only. Formal frozen
contract disposition pending; no production implementation is released yet.

### Final independently derived edges

SET declaration visibility is sequential: condition22940 and assignment1b5b5
look up immediately through23e00. Signal vt1003b944+18=1d590..1d5e7 compares
literal bytes, so Foo and FOO are distinct. Mapper action resolution similarly
uses already appended declarations; PLAYERMAPPER_DEFINE23400 appends,23ea0
finds first case-insensitive match after prefix stripping. Forward references
are not supported. Portable ASCII folding is an explicit definition for mapper
names and keywords, not a blanket normalization of symbols.

INIT checks cursor16b89 before SCHUETTI; when no IF exists, the original cursor
remains null and a later rearmed signal could permit another INIT. The explicit
native started state intentionally prevents that edge as a labelled policy
adaptation. Scene global43438 has only constructor1611f zero, step16b7d read,
and action155c5/155f5 writes1. Action155a0/155d0 implement WON/LOST; these are
unsupported whole-program dependencies, so the flag is unreachable in the slice.

## Independent direct-registry identity derivation

Permission reviewer independently packed PRM1 from original Anlagen HEAD fields,
INFRAMIN and FORSCHTYPE; Game raw name-pointer table100250f8 is read by getter
10013cf0..13d47 and parser100097d5..9869 (index0..64, field+24). This is the
EMPTY availability-profile direct build, not the production original adapter's
output research profile. Base digestb62eba128fce9259 extends to3610424ea558d5cd.
827nondefaultrows,1173folkmemberships,20basegrants:22151bytes =
12+827*21+1173*4+20*4. Extension SHA256
053df2abffb2ec56da4df369516d8c41c198b6f60695203679337460ec3d5fc9.
AnlagenSHA256b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75.
This profile gives1012 only research38; production adapter additionally declares
output0 research12. No new C++ digest output served as the reference.
