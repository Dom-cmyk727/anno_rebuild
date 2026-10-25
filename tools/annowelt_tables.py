"""Walk tables in AnnoWelt.dll. Spec: docs/formats/annowelt-walk-tables.md.

Independent of the C++ reader; the oracle for `anno-dump walk-tables`.
PE parsing uses `struct` only (no pefile). Original table bytes are never
written by this tool except as counts in the dump.

  annowelt_tables.py dump <AnnoWelt.dll>...   same output as `anno-dump walk-tables`
"""
from __future__ import annotations

import os
import struct
import sys

PE32_MAGIC = 0x10B
DOS_E_LFANEW = 0x3C
COFF_SIZE = 20
SECTION_HEADER = 40
OPT_IMAGE_BASE = 0x1C
OPT_SIZE_OF_IMAGE = 0x38
MIN_OPTIONAL = OPT_SIZE_OF_IMAGE + 4

MAPPER_COUNT = 512
LAYER_COUNT = 5
LAYER_SIZE = 0x1C00
ADMISSION_COUNT = 0x400
COST_STRIDE = 8
GOAL_ROWS = 5
GOAL_WIDTH = 9

RVA_MAPPER = 0xC1AF8
RVA_LAYERS = 0xD1140
RVA_GOAL_ORDER = 0xC1160

GOG_TIME_DATE_STAMP = 0x44C0C353
GOG_SIZE_OF_IMAGE = 0x000F1000

ERR_NOT_PE = "not a PE"
ERR_WRONG_BUILD = "wrong build"
ERR_RVA = "RVA outside section"
ERR_SHORT = "short file"


class WalkTablesError(Exception):
    pass


def _u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def _u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def _file_offset(buf: bytes, secs: list[tuple[int, int, int]], rva: int, n: int) -> int:
    for va, raw_size, raw_ptr in secs:
        if rva < va:
            continue
        rel = rva - va
        if rel > raw_size or raw_size - rel < n:
            continue
        if raw_ptr > len(buf) or len(buf) - raw_ptr < rel + n:
            raise WalkTablesError(ERR_SHORT)
        return raw_ptr + rel
    raise WalkTablesError(ERR_RVA)


def read_walk_tables(buf: bytes) -> dict:
    if len(buf) < DOS_E_LFANEW + 4 or buf[0:2] != b"MZ":
        raise WalkTablesError(ERR_NOT_PE)
    e_lfanew = _u32(buf, DOS_E_LFANEW)
    if e_lfanew + 4 > len(buf) or buf[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        raise WalkTablesError(ERR_NOT_PE)
    if len(buf) - e_lfanew < 4 + COFF_SIZE:
        raise WalkTablesError(ERR_NOT_PE)
    nsec = _u16(buf, e_lfanew + 6)
    stamp = _u32(buf, e_lfanew + 8)
    opt_size = _u16(buf, e_lfanew + 0x14)
    if opt_size < MIN_OPTIONAL:
        raise WalkTablesError(ERR_NOT_PE)
    opt = e_lfanew + 4 + COFF_SIZE
    if opt > len(buf) or len(buf) - opt < opt_size:
        raise WalkTablesError(ERR_NOT_PE)
    if _u16(buf, opt) != PE32_MAGIC:
        raise WalkTablesError(ERR_NOT_PE)
    _u32(buf, opt + OPT_IMAGE_BASE)  # parsed as specified; RVA lookup does not add it
    size_of_image = _u32(buf, opt + OPT_SIZE_OF_IMAGE)
    if stamp != GOG_TIME_DATE_STAMP or size_of_image != GOG_SIZE_OF_IMAGE:
        raise WalkTablesError(ERR_WRONG_BUILD)
    sec0 = opt + opt_size
    if nsec > 0 and (sec0 > len(buf) or (len(buf) - sec0) // SECTION_HEADER < nsec):
        raise WalkTablesError(ERR_NOT_PE)
    secs = []
    for i in range(nsec):
        off = sec0 + i * SECTION_HEADER
        secs.append((_u32(buf, off + 0x0C), _u32(buf, off + 0x10), _u32(buf, off + 0x14)))
    map_off = _file_offset(buf, secs, RVA_MAPPER, MAPPER_COUNT * 2)
    goal_off = _file_offset(buf, secs, RVA_GOAL_ORDER, GOAL_ROWS * GOAL_WIDTH * 4)
    mapper = [_u16(buf, map_off + i * 2) for i in range(MAPPER_COUNT)]
    layers = []
    for layer in range(LAYER_COUNT):
        rva = RVA_LAYERS + layer * LAYER_SIZE
        off = _file_offset(buf, secs, rva, LAYER_SIZE)
        layers.append(buf[off:off + LAYER_SIZE])
    goal_order = []
    for row in range(GOAL_ROWS):
        entries = []
        for k in range(GOAL_WIDTH):
            off = goal_off + (row * GOAL_WIDTH + k) * 4
            mask, col, row_i = struct.unpack_from("<HBB", buf, off)
            entries.append((mask, col, row_i))
        goal_order.append(entries)
    return {
        "time_date_stamp": stamp,
        "size_of_image": size_of_image,
        "mapper": mapper,
        "layers": layers,
        "goal_order": goal_order,
    }


def format_tables(t: dict) -> str:
    lines = [f"TimeDateStamp=0x{t['time_date_stamp']:08x} SizeOfImage=0x{t['size_of_image']:08x}"]
    for i, layer in enumerate(t["layers"]):
        admitted = sum(1 for c in range(min(ADMISSION_COUNT, len(layer))) if layer[c])
        parts = [f"layer {i}: admitted={admitted}"]
        for code in (0, 1, 2, 4):
            costs = layer[ADMISSION_COUNT + COST_STRIDE * code:
                          ADMISSION_COUNT + COST_STRIDE * code + COST_STRIDE]
            parts.append(f"code{code}={layer[code]}," + ",".join(str(x) for x in costs))
        lines.append(" ".join(parts))
    return "\n".join(lines) + "\n"


def dump(path: str) -> bool:
    print(f"# {path}")
    try:
        t = read_walk_tables(open(path, "rb").read())
    except WalkTablesError as e:
        print(f"NOPARSE: {e}", file=sys.stderr)
        print("NOPARSE")
        return False
    sys.stdout.write(format_tables(t))
    return True


def build_synthetic(*, stamp: int = GOG_TIME_DATE_STAMP,
                    size_of_image: int = GOG_SIZE_OF_IMAGE) -> bytes:
    """A tiny PE32 with one .rdata and one .data section holding made-up tables."""
    mapper = struct.pack("<" + "H" * MAPPER_COUNT, *[(i * 3) & 0xFFF for i in range(MAPPER_COUNT)])
    goal = bytearray()
    for row in range(GOAL_ROWS):
        for k in range(GOAL_WIDTH):
            goal += struct.pack("<HBB", row * 16 + k + 1, k % 3, row % 3)
    rdata = bytearray(0x2000)
    rdata[RVA_GOAL_ORDER - 0xC1000:RVA_GOAL_ORDER - 0xC1000 + len(goal)] = goal
    rdata[RVA_MAPPER - 0xC1000:RVA_MAPPER - 0xC1000 + len(mapper)] = mapper
    layers = bytearray(LAYER_COUNT * LAYER_SIZE)
    for layer_i in range(LAYER_COUNT):
        base = layer_i * LAYER_SIZE
        for code in (0, 1, 2, 4):
            layers[base + code] = 255
            for j in range(COST_STRIDE):
                layers[base + ADMISSION_COUNT + COST_STRIDE * code + j] = 10 + j + layer_i
    data = bytearray(0x9000)
    data[RVA_LAYERS - 0xD1000:RVA_LAYERS - 0xD1000 + len(layers)] = layers

    e_lfanew = 0x40
    nsec = 2
    opt_size = 0xE0
    opt = e_lfanew + 4 + COFF_SIZE
    sec0 = opt + opt_size
    rdata_ptr = 0x200
    data_ptr = rdata_ptr + len(rdata)
    buf = bytearray(data_ptr + len(data))
    buf[0:2] = b"MZ"
    struct.pack_into("<I", buf, DOS_E_LFANEW, e_lfanew)
    buf[e_lfanew:e_lfanew + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", buf, e_lfanew + 4, 0x14C, nsec, stamp, 0, 0, opt_size, 0)
    struct.pack_into("<H", buf, opt, PE32_MAGIC)
    struct.pack_into("<I", buf, opt + OPT_IMAGE_BASE, 0x10000000)
    struct.pack_into("<I", buf, opt + OPT_SIZE_OF_IMAGE, size_of_image)
    # .rdata
    buf[sec0:sec0 + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", buf, sec0 + 8, len(rdata), 0xC1000, len(rdata), rdata_ptr)
    # .data
    dsec = sec0 + SECTION_HEADER
    buf[dsec:dsec + 8] = b".data\0\0\0"
    struct.pack_into("<IIII", buf, dsec + 8, len(data), 0xD1000, len(data), data_ptr)
    buf[rdata_ptr:rdata_ptr + len(rdata)] = rdata
    buf[data_ptr:data_ptr + len(data)] = data
    return bytes(buf)


if __name__ == "__main__":
    sys.stdout.reconfigure(newline="\n")
    if len(sys.argv) >= 2 and sys.argv[1] == "write-synthetic" and len(sys.argv) == 3:
        open(sys.argv[2], "wb").write(build_synthetic())
        sys.exit(0)
    cmd, files = sys.argv[1:2], sys.argv[2:]
    if cmd != ["dump"] or not files:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    sys.exit(0 if all([dump(f) for f in files]) else 1)
