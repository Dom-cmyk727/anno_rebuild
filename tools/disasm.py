"""Linear-sweep disassembly of one function of a 32-bit PE module (read-only).

  disasm.py <module.dll> <va-hex> [max_bytes]

For hosts where Ghidra cannot load the 32-bit modules (Windows, CLAUDE.md);
needs `pip install capstone pefile`. Prints address, bytes and instruction
from `va` until `max_bytes` (default 4096) or the first `ret` followed by
int3/nop padding (the end of an MSVC 6 function; a function with code after
an early `ret` and no padding is printed past its end, check the output).
The output is derived from the original binaries: like cache/ghidra/, it is
never committed; findings go to docs/re/ with addresses.
"""
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

path, va = sys.argv[1], int(sys.argv[2], 16)
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 4096
pe = pefile.PE(path, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
rva = va - base
data = pe.get_data(rva, limit)
md = Cs(CS_ARCH_X86, CS_MODE_32)
for ins in md.disasm(data, va):
    print(f'{ins.address:08x}  {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}')
    if ins.mnemonic.startswith('ret'):
        nxt = ins.address + ins.size - va
        if nxt < len(data) and data[nxt] in (0xcc, 0x90):
            break
