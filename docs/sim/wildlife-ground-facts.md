# Source-bound wildlife ground facts

WILDLIFE-GROUND-FACTS-1, 2026-11-19. Contract before implementation. D = fresh
GOG instruction/table read; P = independent original-file parse; H = authored
hand derivation; C = executed conditional check; U = unproved. This binds inputs
of the [numerical leaf](wildlife-ground-prepare.md), never original city/INFO/HEAD
provenance for a whole scene. See [preprocessing](wildlife-preprocess.md).

## Concrete source profile

D: Welt 0x1008452b–0x10084546 zero-extends the WHOLE u16 object word for catalogue
+0x70 lookup, tests null, calls type +0x5c, then tests ((kind-1)&~31)+1 ==225.
Only raw kinds 225–256 reach property 0x2e (0x1008454d–0x10084565); query failure
or zero retains p. The engine interface keeps its widened u32 whole words; no
14-bit mask or original unsafe 4000-pointer array limit is imported.

D: fresh catalogue table Game 0x100215a4 +0x70 ->0x100037e0–0x100037eb returns
+0x214+4*key; constructor 0x10003834–0x10003843 zeroes that pointer array. Entry
reader 0x10001df4–0x10001e49 looks up its full key, creates kind 0x700 through
+4 when absent, then inserts/sets its id. Actual +4 ->0x100063a0–0x1000644d,
kind 0x700 allocates 0x294 and calls 0x1000c240–0x1000c28f, installing table
0x10021a88 after base construction. This profile applies to standard Anlagen
entries, not just names the rebuild calls resources. Substituted dynamic
catalogues/getters are U and unsupported here. RuntimeClass describes a modern
world building, NOT the original descriptor's property dispatch.

D: concrete table 0x10021a88 +0x5c ->0x10007d20–0x10007d26 zero-extends type
u16 +0x10. +0x48 ->0x1000c0c0–0x1000c0d4 specializes only property 0x17 and
forwards 0x2e to common 0x1000a860. Common table 0x1002185c also has that getter.
0x1000a88d–0x1000a896 selects 0x2e; 0x1000a979–0x1000a991 zero-extends byte
+0x77 into output DWORD +4 and returns 1 EVEN FOR ZERO. No draw/callback occurs
on these finite leaves. Base initialization 0x1000b541 starts +0x77 at zero.

D: HEAD arm 0x10009334–0x100094cd zeros 14 DWORDs/56 bytes at
0x10009348–0x1000935a, reads the payload through 0x10015ab0–0x10015acd (payload
length, no destination bound), loads stack buffer[0x19] at 0x100093d3 and stores
it to +0x77 at 0x100093ed. Present short HEAD is zero-extended; >56 is unsafe and
refused by this adapter. PRODTYPE 0x10009734–0x100097cc scans name ordinals
0–416, exact case-sensitive first-NUL comparison, and stores the matched raw
kind. Name-table arms freshly read at 0x100138e0–0x10013a1d. Common parser
forwarding is 0x1000b887–0x1000b8b0; repeated records are processed in order and
later HEAD stores replace prior values. Repeated entry keys reuse their type.
The binder instead REFUSES repeated needed entries/HEAD/PRODTYPE as ambiguous,
a declared defensive boundary, not an assertion of original duplicate refusal.

## Binder input and publication contract

One required `GroundDescriptorProfile::GogAnlagen` selects the concrete standard
factory profile above; unknown profiles refuse, not a default dispatch. It is a
transcription contract for supplied parsed data, not a hash/Boolean authenticity
certificate or a live original object. Full fresh source catalogue entries and
its immutable registry are paired inputs. Referenced nonzero metadata missing
from either refuses; do not fabricate a null from a modern missing optional.
Object 0 produces N only when BOTH source entry 0 and registry type 0 are absent
under this initialized fresh-catalogue profile. A defined zero entry is bound as
an ordinary descriptor; no unconditional zero-id shortcut. Other missing words
refuse conservatively even though the original complete array could contain null.

Index requested distinct whole words and needed source entries once. Return
owned facts sorted/distinct by whole word. Unrelated malformed entries do not
become a global restriction. Resolve a needed id against registry id, exact raw
PRODTYPE and normalized 56-byte HEAD, plus derived width/height/flag fields.
Require one HEAD and PRODTYPE, a terminated known kind name, source HEAD <=56,
and registry HEAD exactly56 with matching contents. First NUL ends the name;
trailing bytes are ignored as in the original string comparison.

All bound standard descriptors emit V(actual unsigned normalized HEAD[0x19]),
including V(0). N/A/F semantics of the leaf are unchanged; this supported getter
never manufactures A or F. For out-of-blend kinds the binder still supplies the
proved pure common byte fact; the unchanged leaf short-circuits before blending.
Decoding an immutable field is not an extra dynamic getter call or draw. No claim
is made for unproved concrete getters merely because their modern RuntimeClass
or a registry digest matches. All validation precedes result publication; errors
leave inputs unchanged, no partial facts. No I/O, registry mutation or World caller.

## Authored acceptance, before implementation

Authored source IDs/kinds/values: 104/225/100,105/225/0,16384/225/20,
16488/225/60,90001/321/90. HEAD[0x18]=4 and [0x1a]=77 differ from property
byte [0x19]; HEAD widths1/heights1/flags0x29. Explicit zero is N when absent.
These are authored records, not copied original data. First/second whole words
104 and16488 collide under &0x3fff; so do 0 and16384, but facts remain distinct.

| Requirement | Positive and distinguishing wrong control | Planned proof | Review |
|---|---|---|---|
| Correct getter/zero | V100,V0; neighbouring HEAD bytes4/77 | exact facts and wrong-offset/zero-state variants | pending |
| Whole words | 104/16488 and0/16384; data-defined90001 | distinct sorted facts; actual masking variant | pending |
| Missing versus null | known absent0 N; unknown nonzero refuses after legal control | default-missing variant rejected | pending |
| Source/registry pairing | exact raw kind/full normalized HEAD; short present HEAD zero-extends | mismatches/duplicate/truncation/unsupported profile refuse | pending |
| Ownership/order | destroy/mutate source+registry owners; permute/repeat requests/entries | same owned sorted output/source equality | pending |
| Both blend branches/zero/outside group | p20/v100 and p100/v20 ->b8; V0 p20 ->b2; raw321/v90 p20 ->b2 | centre kernel6, old a7,b7,c7,d7 ->a4,b4,c1,d1 | pending |
| Original catalogue | independent file/name-table parse before C++ | 921 fields against private oracle,291 blend kinds,214 nonzero values | pending |
| Scope | UNWIRED data binding only | focused Debug/ASan, unchanged fixtures/state/caller audit | pending |

NOT_RUN at contract checkpoint: C++, original/HE execution, substituted descriptor
profiles, city/INFO source selection, prepared FRUCHTBAR/fill/World integration.
