# Walk tables in `AnnoWelt.dll`

**Status:** partial — PE identity, RVA map and the three table layouts are complete for GOG 2.0.0.5. This is not a general PE parser. Table *meanings* for the search live in `docs/sim/state.md` §5.9 and `docs/re/transport.md` §3.4; this spec is the file layout T33.1 reads.

The original's table bytes are **never** copied into the source or fixtures (CLAUDE.md). Tests that need them read `game/app/AnnoWelt.dll` through `anno::test::game_dir` and skip without it. Synthetic tests build their own small tables.

## Files covered
- Glob: `AnnoWelt.dll` (the world's DLL in the game directory)
- Count in GOG 2.0.0.5: 1
- Related: `docs/re/transport.md` §3.4.9, §3.4.12, §3.4.16 (d); `docs/sim/state.md` §5.9 clause 0 and "What T33 implements"

## Identification
- Magic: DOS `MZ` at offset 0, PE signature `PE\0\0` at `e_lfanew` (P: `game/app/AnnoWelt.dll`; the PE/COFF layout is the published Microsoft PE format, used here only as far as the fields below).
- GOG 2.0.0.5 build (P: the same file): COFF `TimeDateStamp` `0x44c0c353`, optional-header `SizeOfImage` `0x000f1000`. The reader refuses any other pair with a clear error. A modded DLL can be accepted later behind an explicit option that also names the RVAs and sizes (PLAN §0.1); that option is not in this reader.

## Layout
All integers little-endian. Offsets below are from the start of the named header.

### DOS header
| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | bytes | `e_magic` | `'M' 'Z'` | P: GOG `AnnoWelt.dll` |
| 0x3c | 4 | u32 | `e_lfanew` | file offset of the PE signature | P: `0xd8` in GOG 2.0.0.5 |

### PE signature and COFF header (at `e_lfanew`)
| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | bytes | signature | `'P' 'E' 0 0` | P |
| 0x06 | 2 | u16 | `NumberOfSections` | count of 40-byte section headers | P: 4 in GOG 2.0.0.5 |
| 0x08 | 4 | u32 | `TimeDateStamp` | build identity (with `SizeOfImage`) | P: `0x44c0c353` |
| 0x14 | 2 | u16 | `SizeOfOptionalHeader` | bytes of the optional header that follows | P: `0xe0` |

### Optional header (PE32, at `e_lfanew + 24`)
| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `Magic` | `0x10b` = PE32 (this DLL is 32-bit) | P |
| 0x1c | 4 | u32 | `ImageBase` | preferred load address; parsed so the header walk matches the PE layout. Table addresses in this spec are **RVAs**, not VAs. | P: `0x10000000`; D readers below add this base |
| 0x38 | 4 | u32 | `SizeOfImage` | build identity (with `TimeDateStamp`) | P: `0x000f1000` |

A file whose optional header is shorter than `0x3c` bytes, or whose magic is not PE32, is not a PE for this reader.

### Section headers (40 bytes each, after the optional header)
| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x0c | 4 | u32 | `VirtualAddress` | RVA of the section | published PE; P: four sections in GOG 2.0.0.5 |
| 0x10 | 4 | u32 | `SizeOfRawData` | bytes of the section in the file | published PE |
| 0x14 | 4 | u32 | `PointerToRawData` | file offset of those bytes | published PE |

`VirtualSize` and the name field are not read. An RVA of `n` bytes is in a section when `VirtualAddress ≤ rva` and `rva + n ≤ VirtualAddress + SizeOfRawData`; the file offset is `PointerToRawData + (rva − VirtualAddress)`. If no section contains the range: error `RVA outside section`. If the section contains it but the file ends before `n` bytes: error `short file`.

### The three tables (RVAs; GOG 2.0.0.5)
Addresses in parentheses are VAs at the preferred base `0x10000000`.

| RVA | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| `0xc1af8` | 1024 | 512 × u16 | mapper | 9-bit object mask → low-u16 geometry. Fill `0x1008ff70` indexes it at `0x1009005f`. | D: AnnoWelt `0x1009005f`; P: 512 entries, 33 distinct values, every value's bits 12–15 are 0 (`docs/sim/state.md` §5.9) |
| `0xd1140 + 0x1c00·L` | `0x1c00` each, L = 0..4 | bytes | layer block L | Walking layer L. Bytes `0..0x3ff` are admission of code `c`; byte `0x400 + 8·c + j` is cost `C_j(c)`. Setter `0x1000dd20` stores `0x100d1140 + 0x1c00·arg0`; expansion reads admission / costs at `0x10006ab0` / `0x10006ab9`. | D: `0x1000dd20`, `0x10006ab0`, `0x10006ab9`; P: five blocks |
| `0xc1160` | 180 | 5 × 9 × 4 | goal-order | Per arrival, the order of goal sub-cells. Each entry is u16 `mask`, u8 `col`, u8 `row` (stride 4; row stride `0x24`). The goal test walks one row at `0x100036fb..0x1000371c`. | D: `0x100036fb..0x1000371c`; `docs/re/transport.md` §3.4.16 (d) |

Layer count 5, mapper length 512, layer size `0x1c00` and goal-order 5×9 are the original's values used as defaults (PLAN §0.1). A later option that accepts another build names its own RVAs and sizes.

## Errors
- **not a PE** — missing `MZ` / `PE\0\0`, truncated headers, or not PE32.
- **wrong build** — `TimeDateStamp` or `SizeOfImage` is not the GOG 2.0.0.5 pair.
- **RVA outside section** — a table RVA is not in any section's raw range.
- **short file** — the section contains the RVA but the file ends before the table.

## Evidence legend
- **P** — parsed GOG 2.0.0.5 `game/app/AnnoWelt.dll` (identity fields and the §5.9 invariants; table *contents* are not in git).
- **D** — disassembly; module + address, `docs/re/transport.md`.
- **C** — community source.
- **?** — hypothesis; must link `docs/open-questions.md`.

## Variants / versions
Only GOG 2.0.0.5 is accepted. Other builds of `AnnoWelt.dll` fail `wrong build` until an explicit option exists.

## Open fields
None in the layout this reader uses. Shore / finer-route behaviour of the *search* is DRAFT in `state.md` §5.9, not a file-format gap.

## Tests
- Synthetic PE (one or two sections, made-up tables): parse → every field of `WalkTables` matches what was written; each error case (`not a PE`, `wrong build`, `RVA outside section`, `short file`).
- Corpus: `game/app/AnnoWelt.dll` through `ANNO1503_GAME_DIR` (skips when unset): 512 mapper entries, 33 distinct, bits 12–15 of every value 0; five layers; `L[7] = 0` and `L[0x80..0x86] = 0` in every layer; `L0[2] = 0xff` and `L3[2] = 0`; `C_0(0) = 10` in layers 0 and 3; `L3[4] = 0xff` with `C_0 = 40`.
- Reference dump: `anno-dump walk-tables` = `tools/annowelt_tables.py dump` (`tools/agree.py`).
