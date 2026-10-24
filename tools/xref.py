"""Cross-references into an address range of a 32-bit PE module (read-only).

  xref.py <module.dll> <lo-hex> [hi-hex]          (the code sweep)
  xref.py --reloc <module.dll> <lo-hex> [hi-hex]  (the relocation table)

Both print one line per reference whose value lies in [lo, hi) (hi defaults to
lo + 1): the referencing address, the value, and (with --reloc) the section.

The code sweep (capstone, skip-data mode) over the executable sections lists each
instruction with an immediate operand or a memory displacement in the range:
call and jump targets, `push imm`, `mov reg, imm`, `[disp]`, `[reg + disp]`. It
finds small offsets too (a field `+0x430` as `0x430 0x431`), which the relocation
table cannot. It misses every reference held outside the executable sections
(vtables, `.rdata` tables) and inside data the sweep decodes as code or skips
(jump tables inlined in `.text`), and an address the code computes (base +
register): a sweep hit list is evidence of what it lists, not proof that no other
reference exists.

--reloc reads the module's base-relocation table (IMAGE_REL_BASED_HIGHLOW): every
absolute address the loader patches, in every section, code and data alike. For a
relocatable DLL this is the complete list of absolute references to a range; it
does not see addresses formed at run time (base + offset in a register) or
relative call/jump targets (use the sweep for those).

Needs `pip install capstone pefile` (see tools/disasm.py for the `python -I` note).
Only addresses are printed, never code bytes or instruction text: findings go to
docs/re/ with addresses, nothing of the original is committed.
"""
import sys

import pefile

USAGE = "usage: xref.py [--reloc] <module.dll> <lo-hex> [hi-hex]  (hi > lo)"

args = sys.argv[1:]
reloc = bool(args) and args[0] == "--reloc"
if reloc:
    args = args[1:]
if len(args) < 2:
    sys.exit(USAGE)
path = args[0]
lo = int(args[1], 16)
hi = int(args[2], 16) if len(args) > 2 else lo + 1
if hi <= lo:
    sys.exit(USAGE)

if reloc:
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_BASERELOC"]]
    )
    base = pe.OPTIONAL_HEADER.ImageBase
    if not hasattr(pe, "DIRECTORY_ENTRY_BASERELOC"):
        sys.exit(f"{path}: no base-relocation table")
    for block in pe.DIRECTORY_ENTRY_BASERELOC:
        for e in block.entries:
            if e.type != pefile.RELOCATION_TYPE["IMAGE_REL_BASED_HIGHLOW"]:
                continue
            v = pe.get_dword_at_rva(e.rva)
            if v is not None and lo <= v < hi:
                sec = pe.get_section_by_rva(e.rva)
                name = sec.Name.rstrip(b"\0").decode("ascii", "replace") if sec else "?"
                print(f"{base + e.rva:08x} {v:08x} {name}")
    sys.exit(0)

from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM  # noqa: E402

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
