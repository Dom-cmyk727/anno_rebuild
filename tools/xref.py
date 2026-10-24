"""Cross-references into an address range of a 32-bit PE module (read-only).

  xref.py <module.dll> <lo-hex> [hi-hex]

A linear sweep (capstone, skip-data mode) over every executable section prints,
one per line, the address of each instruction that has an immediate operand or
a memory displacement in [lo, hi) (hi defaults to lo + 1), followed by the
matched value: call and jump targets, `push imm`, `mov reg, imm`, `[disp]` and
`[reg + disp]` all count. A linear sweep can decode data between functions as
code and can miss an address the code computes (base + register), so a hit list
is evidence of the references it lists, not proof that no other reference exists.
Needs `pip install capstone pefile` (see tools/disasm.py for the `python -I` note).
Only addresses are printed, never code bytes or instruction text: findings go to
docs/re/ with addresses, nothing of the original is committed.
"""
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

if len(sys.argv) < 3:
    sys.exit("usage: xref.py <module.dll> <lo-hex> [hi-hex]")
path = sys.argv[1]
lo = int(sys.argv[2], 16)
hi = int(sys.argv[3], 16) if len(sys.argv) > 3 else lo + 1
pe = pefile.PE(path, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True
md.skipdata = True
for s in pe.sections:
    if not s.Characteristics & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
        continue
    va = base + s.VirtualAddress
    for ins in md.disasm(s.get_data(), va):
        if ins.id == 0:  # skipped data
            continue
        for op in ins.operands:
            if op.type == X86_OP_IMM:
                v = op.imm & 0xFFFFFFFF
            elif op.type == X86_OP_MEM:
                v = op.mem.disp & 0xFFFFFFFF
            else:
                continue
            if lo <= v < hi:
                print(f"{ins.address:08x} {v:08x}")
                break
