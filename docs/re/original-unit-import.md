# Original unit import — bounded primary contract (2026-11-05)

A contract, not production acceptance. Scope/class limits and authored vectors:
[acceptance ledger](../sim/original-unit-import.md). Original bytes/dumps remain
ignored. Input SHA256: AnnoWelt.dll
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`;
scenario/endless source hashes are retained by SCRIPT2 and the new inventory.
This is static instruction/record proof on the original GOG modules, not a runtime
capture. Ghidra is unavailable at the installed-tool path on this host; existing
pefile/Capstone read-only tools inspect the bounded reader/caller closure.

| Original module/function | Interface/caller | Verified effect |
|---|---|---|
|Welt1001dc7a /1001df31|enclosing player state reader1001d240; world registry create|SOLDAT/SCHIFF16-byte prefixes; enclosing owner wins; savedu16 ID selects one global registry slot|
|Welt100ae2c9..100ae350|world chunk MAIN handler; registry subobject|zero-fill16, MAINu32+4 nonzero→resize100980e0; zero leaves constructor size|
|Welt10098cd6 /10098d06|registry constructor|initial1, hint−1,cursor0|
|Welt10097347..1009739a|registry10097250, requested rawID argument|explicit slot/holes; grow from requestedslot, default min100; update hint tothat slot|
|Welt10031870|root unit vtable common reader, slot+0x6c|HEAD dispatch+0x140; WAREN lowbyte mappedgoods, u16tons+2×256/capacity clamp; PRIVAT dispatch+0x12c|
|Welt1009a110 +tables1009a7e8/1009a774|registry factory; root objects returned|53→donkeyctor10037e70/vtable100c3a30;97→scoutctor100374d0/vtable100c3758;113→shipctor10043600/vtable100c4da4;114..128 default/no actor|
|Welt10042a40|ship root from vtable100c4da4 +0x140|byte9 HP ratio/128 (zero→max), HEAD1 sign→20, HEAD10bit0→100, HEAD8/non97 qualifying crew→8000|
|Welt100498e0|scout/donkey root vtables+0x140|signedi16HEAD+8, nonpositive→max thenclamp; HEAD1 sign→20|
|Welt10035ce0|base root constructor used by scout/donkey chain|flags8; facing0; +14/+18/+1c are zeroed context pointers, not ownxyz|
|Welt1001dde5→10035260|top-level SOLDAT initialsegment+e0|newmode0 vsconstructormode19 clears8 at10035336; not called for nestedCREW|
|Welt10041959..1004196c|ship root crew attach+0xb0|childflags=(old&AE) OR80, stop4/1; constructor8 retained|
|Welt10042e68..10042fa1|ship root PRIVAT/CREW loader|ENTRYu16ID@0,kind@2,slotbyte4; nullpos creation→attach→ownerbind→setscoutpos toship+14→commonload|
|Welt1004c688/68d/6aa|crew writer, local6-byte output buffer|memberID@0,slot@4,kind@2; earlier SCRIPT2 prose swapped ID/slot|
|Welt10036ca0 /10037012|scout root PRIVAT ESEL helper|held parent80→nullpos child and(parentflags&AF) OR80; ESELkind@0,ID@2; held x/yignored; prop1f parentpointer|
|Welt1001c817..1001c8aa|owner mode0 attach, called with same player root|context/color/typeaccount binding, no clearing constructor8|
|Welt100afde0|scout/donkey commonload final+A0|stub; no flag change|

Examples independently derived from records/instructions: endless01 source creation
IDs0..4, registry100/hint4/cursor0, five same-owner actors; ship/scout ownpos351.5,
383.5; held donkey parent scout, with original ownxyz uninitialized on this path;
modern cached childposition normalized to knownparentposition (explicit nonspatial
normalization, not a primary originalposition fact); shipdir4, nestedface0; shipnavHP12000,
scoutHP350/donkeyHP400; shipstate8000, held actors88; eight ship holds50t in source
order. The ledger distinguishes source file traversal, slot order and modern
stable engine IDs. No simultaneous flat-list/parent reinterpretation is assumed.

Separating vectors cover crewID4660/slot3, nonmonotone rawslots37/2, requested100
slotgrowth125, half shipHP6000, smallratio1→93, signedwalkerHP−1→max, lowbyte goods
remap to90001, amount768 andcapacity512. Wrongdecoders remain necessary negative
controls after implementation; a matching note/reference alone is not acceptance.

Unresolved/outside closure: nonempty auto-route/task execution, other actor classes,
unheld ESEL placement, hostile-avoidance stance behavior and later global postload
passes not comprehensively traced. Refusal/warning boundaries are explicit; no
COMPLETE original scenario or audible playback claim follows from this contract.

Flag correction during B review:10042b34 JNE class97 reaches moval1 at10042ae7;
class97 falls through tocontinue. Qualifying fallback needs nonzero slot+9c AND
class!=97; attach100419e7 repeats the inequality. Earlier A/source0 was incorrect
and its flag approval retracted. Source HEAD8=0 retains8000 with scouts. A legal
HEAD8=1 within other-crew capacity clears8000. The class97-only crew closure needs
no extra qualifying-fact metadata; do not normalize this bit away.

Abstract-crew type input correction during B/C review: slot+88's getter AnnoGame
1000fb60 reads byte+f6+mode, so mode2 is+f8. LOGIC buffer begins stack+30;
100102f3 loads EAX at stack+55 (LOGIC25..28), and EAX remains live until10010352
stores AH to+f8. Therefore other_crew is LOGIC26, not15. The separate LOGIC15 write
at10010274 goes to+101, read by1000fcb4 (cannon capacity). New type-catalog decoding
requires the established44-byte zero-filled LOGIC buffer. Observed source other
crew:1503=6,1502=0,717=0. Old manually declared synthetic metadata remains a valid
configured fixture but cannot establish raw field extraction.
