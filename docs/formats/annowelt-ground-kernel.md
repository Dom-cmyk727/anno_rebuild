# Selected AnnoWelt ground kernel

WILDLIFE-KERNEL-LOAD-1, 2026-11-19. Contract and authored builder before reader
implementation. D = decoded original GOG instructions; P = original file/PE
metadata; H = hand-derived authored expectations; C = executed conditional check;
U = unproved. [Ground preparation](../sim/wildlife-preprocess.md) is the consumer.
This is an UNWIRED data reader, never module loading/execution/relocation.

## Source and supported profile

D: Welt `0x10084670–0x10084698` zero-extends island +0xc0 and computes
`0x100ca008 + 25*selector`, a 5×5 unsigned-byte kernel. The bounded supported
selectors are the seven evidenced values 0–6 (Scene selector table 0x1003b328,
[link selection](../re/annoscene.md)). This is NOT the kernel table's total extent
or a global climate limit. Unknown selectors refuse even if more bytes exist.
Selected selector is a caller argument, not proof of original INFO/island choice.

P: GOG AnnoWelt PE32/I386 (magic 0x10b, machine 0x14c), TimeDateStamp
0x44c0c353, SizeOfImage 0xf1000, ImageBase 0x10000000. Kernel RVA 0xca008.
The real backing .rdata has VA/raw pointer 0xc1000, virtual size 57995, raw size
61440, characteristics 0x40000040. Actual selected file offsets happen to equal
RVAs; authored controls deliberately do not. Independent pefile extraction
retains seven full rows privately and publishes only counts/source/result hashes.

These metadata fields identify a supported LAYOUT, not an unchanged original
DLL. Authored images with these fields are deliberately accepted as layout
controls; source SHA identifies supplied bytes, not city/scene/authenticity.
The real-file test separately pins known original source SHA
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
No other edition or original executable behavior is claimed.

## Checked window policy

Reuse existing checked PE parsing/header bounds and stamp/size error priority.
Require machine/base above, then supported selector. Validate all 25 bytes before
copying. Widen every offset/end calculation. Add only virtual size and section
characteristics to the private section view plus a separate checked window query;
existing first-match offset mapping and old-reader error order stay unchanged.

The new query conservatively counts every section whose declared interval
`[VA, VA+max(VirtualSize,RawSize))` intersects any part of the selected window.
Exactly one is required: even partial overlaps or virtual-only contenders refuse.
That interval is an ambiguity guard, not a general Windows loader emulation.
The full window must fit BOTH VirtualSize and RawSize, and the entire selected
backing raw section must exist in the file. Raw padding beyond VirtualSize,
virtual zero-fill beyond RawSize, overflow/beyond-image extents and truncation
refuse. Zero VirtualSize is unsupported by this conservative profile.

Require MEM_READ 0x40000000 and no MEM_WRITE 0x80000000. Section names are not
trusted, and execute permission is not used to execute anything. Flag meanings:
[Microsoft PE/COFF specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-flags).
These checks are a narrow selected-data profile, not complete PE verification.
No new global trust/edition policy or unrelated hardening is introduced.

Return owned 25 bytes plus selector, RVA/raw offset and image metadata/source SHA.
No borrowed image survives the call; errors expose no partial kernel, source
bytes stay unchanged. Generic numerical leaf and weights remain data-driven.

## Authored expectations and acceptance

Builder: `anno/test/wildlife_kernel_fixture.hpp`, a single test-only layout,
not a general PE framework. PE at 0x40, optional at 0x58, section at 0x138;
VA 0xca000 maps to raw 0x300. First selected byte is file 0x308, last supported
row starts file 0x39e / RVA 0xca09e and ends at file 0x3b7. Prefix bytes 0xd1
and trailing bytes 0xe2 are distinct sentinels, not original data.

H: authored row 0 is exactly
`[3,10,17,24,31,38,45,52,59,66,73,80,87,94,101,108,115,122,129,136,143,150,157,164,171]`.
Rows 1–5 follow authored `(31*row+7*i+3) mod 256`. Authored row 6 is zero
except centre index 12=12, east index 13=24, south index 17=12.
All 25 expected bytes are compared; no original coefficients are public.

| Requirement | Positive / specific counterexample | Planned proof | Review |
|---|---|---|---|
| Selection/stride/translation | Full row 0 and row 6, non-identity RVA/raw; ignored selector/stride 24 gives wrong bytes within valid backing | authored bytes and actual wrong-source controls | pending |
| Unknown selector | Extended legal backing has room for row 7, still refuses | explicit domain check, not incidental OOB | pending |
| Complete initialized window | End-of-section/EOF exact legal case; shorten file/raw/virtual extent one byte | positive sibling then precise refusal/source equality | pending |
| Unique read-only section | READ only accepted; writable/non-readable or duplicate/partial-overlap refused in either order | one-precondition controls; bypass-query mutant | pending |
| Header/arithmetic | Signature/magic/machine/stamp/size/base/table bounds, huge RVA/raw fields | bounded small images, no large allocation | pending |
| Owned result | Returned bytes survive image mutation/destruction | explicit lifetime/immutability test | pending |
| Original bytes | Seven rows against independent pefile extraction, known module SHA | opt-in required-asset check; raw bytes private | pending |
| Numerical connection | Authored row 6 feeds 3×3 integer leaf | unsaturated hand outputs below | pending |
| Existing behavior | Additive private query only | old AnnoWelt/AnnoGame synthetic controls | pending |

H: numerical b rows `1,2,3 / 4,1,2 / 2,3,1`, decoded authored row 6, origin
(17,-11): centre+2east+south gives `9,9,5 / 8,8,3 / 8,5,1` without saturation.
With old `[a7,b7,c7,d7,e7,f7,17,27,37]` hex, expected output is
`[a9,b9,c5,d8,e8,f3,18,25,31]`. Other GFX/object/ground/kernel facts are authored;
this is not original island preparation or a source-scene capability.

NOT_RUN at contract checkpoint: implementation, original/HE execution, original
city/INFO/common-HEAD binding, FRUCHTBAR/fill/World integration. Owner navigation
decision and Windows raster observations remain separate. Prior growth ASan
failure remains UNKNOWN; no CI retry or repair campaign is part of this packet.
