# Original building availability (MAP-BOOTSTRAP-1 B1)

Static evidence only; original runtime NOT_RUN. Originals remain read-only.
AnnoWelt SHA256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`;
AnnoGame `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`;
Anlagen `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`.
System Python pefile 2024.8.26 / capstone 5.0.7; bounded reads using tools/disasm.py.
Raw local evidence: cache/bootstrap/B1 (never commit disassembly/original assets).

## Parse and representation

AnnoGame type parser starts 10009240. INFRAMIN branch 10009871..10009908
scans kind 0x21 names, indices 0 through 31 (increment 100098f3, signed jl
against 0x20 at 100098f4). Matching writes low byte to type+0x23 at 10009905.
Getter vtable+0x64 =10007d70 zero-extends that byte (ret at10007d75).
Constructor clears it at1000b50c. Absent means0. Unknown names cause no write;
a later recognized occurrence replaces the earlier value. Unknown after a valid
occurrence therefore retains the earlier value. Strict unknown/duplicate rejection
in the rebuild is a validated-content adaptation, not the original parser's rule.

Stage vocabulary reuses the existing stage_index_of table: EVER=0,1A=1,1B=2,
1C=3,2A=4,...,TRIUMPH=29,DENKMAL=30,FORSCH=31. Original byte storage and
32-bit mask are format limits, not building-id or registry-count limits.
921 shipped entries:581 explicit EVER,99 absent,20 FORSCH; no repeated INFRAMIN.
Known examples independently decoded:807/1001/805/1023 EVER;1203/1022 1C;
1201/804 2B;1205/1017/1019 2C;819 3A;1035 3D;1034 3F.
Absence does not make a native/scenario variant a normal menu offering.

## Decisive player query

AnnoWelt player vtable+0x70 =1001bae0, complete routine through ret4 at1001bc70.
Intermediate returns are alternate exits; no bytes past that final return belong
to this routine. Context is the player, not a city/island resident count.

1. 1001baf8..1001bb46: find type id in player's tree+0x2f8. An existing node's
   byte+0xe is returned directly, before all other tests. Zero forbids; nonzero
   is truthy at the menu consumer. Key is original u16, compared unsigned.
2. 1001bb49..1001bb67: fetch stage index, load player dword+0x90, shift u32 1
   by CL, AND, normalize to0/1. A set bit immediately succeeds. EVER is bit0,
   not unconditional; mask0 fails EVER. It is not an ordinal comparison.
3. Only stage31 (cmp at1001bb6d) gets ordinary research alternatives. Type
   vtable+0xac(0) reads OUTPUT goods (AnnoGame1000a410,type+0x206+i), NOT input
   (+0xb0,type+0x200+i); WARE parser1000a1dc stores+0x206,
   ROHSTOFF parser1000a2c2 stores+0x200. Signed comparisons at1001bb84/89 include46..55.
   If first output is in that interval, consult goods byte+26*g+0x18 (research id)
   and player research slot+8; bit1 (`test al,2`) succeeds. If first research
   fails, nonzero second output's research is also tried, without another range
   test. Then type FORSCHTYPE getter+0xec is tried (1001bc00..1001bc1e).
   These are OR alternatives. A reached stage31 bypasses research entirely.
   FORSCHTYPE is not a separate mandatory AND for ordinary construction.
4. If still unavailable, original type1021 (0x3fd,1001bc28) additionally accepts
   research36 or35 (1001bc3b/57). This is the large shipyard (INFRAMIN4A), not
   a universal research rule for all building ids. A data profile can declare
   alternatives for that type; runtime eligibility must not branch on1021.

Hand-derived separating cases: stage3,mask8 ->true; stage3,mask16 ->false
(despite16>3). EVER,mask0 ->false. stage31,mask0x80000000,unresearched ->true.
For stage31/output research1: state[1]=1 ->false; state[1]=2 ->true (researchable
is not researched). Override0 with an otherwise satisfied bit ->false;
override2 with mask0 ->truthy. Research36 enables1021 with its stage4A absent,
but not a different type unless its profile declares that alternative.

## Scenario permissions and consumers

Player record ANLTRG: loader1001e4aa..1001e5ea, saver1001f21d..1001f2e2;
rows {u16 type,u8 return_value,pad}. Player setter vtable+c0=1001bca0 property1
writes overrides. Value1 also unlocks associated stage/research. AnnoScene
handlers10015fa0/10015fe0 call it. Shipped S01_03_01.sze INITBLOCK forbids
HUMAN822/718/1206/1039/824 with SETCONSTRUCTION_DRAWSTATE(FALSE).
Addon endless01.sze uses SETCONSTRUCTION_DRAWSTATE_ALL(TRUE) then2229(TRUE).
Independent scan found no stored ANLTRG in50 shipped sze files; their scripts
set it dynamically. Candidate scenario adapter neither reads ANLTRG nor runs
those scripts. Missing permission context is a real gap, not evidence of no
scenario restrictions.

View1002cbb1 calls selected player(+0x618).vtable+0x70 with the resolved type,
then stores menu availability in byte array+0x634. Player wrapper query kind7
at AnnoWelt1001a710 routes through the same query under its player context.
Full City::Build100276a0..100278b1 has no player availability call: its +0x70
call at100276c8 is the GAME registry lookup. Authoritative rebuilding enforcement
would be an explicit adaptation, not an original network-security property.
Automatic replacements/upgrades and imported buildings are separate creation
contracts; an ordinary build query does not establish their rules.

## Review and unresolved policy

Read-only reviewer /root/review_original independently derived the bytes/data
above and checked the locked/legal baseline at15b4ef02. B1 final disposition
pending the frozen-note review. Production enforcement awaits the owner's scope
decision for worlds with missing script/ANLTRG context. Pure query and immutable
facts can be implemented independently with explicit override inputs. No reviewer
can silently authorize dropping scenario permissions.

The inspected Frame mode6 path100589a0..10058a9e has no built-in stage check;
it calls City::Build at10058a67 and Place10058a7f. Its external callback
(this+0x124,100589f9..10058a20) remains unread, so this is not proof about every
possible callback. Frame SHA256 dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91.
