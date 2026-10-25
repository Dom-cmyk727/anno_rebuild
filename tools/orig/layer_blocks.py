"""T30b.4 — Read the five walking-layer blocks in the running original and compare
with the DLL image on disk.

The AnnoWelt.dll setter `0x1000dd20` writes the layer-block pointer. Through
class Y's +4 interface this is `[Y + 0x434]`, read by `0x10006a40`:
bytes 0..0x3ff are admission masks; bytes at 0x400 + code*8 + dir are
costs (transport.md section 3.4.12). A memory/image equality check establishes
unchanged content, not that the tables are dead data.

`--disk-summary` reads only the DLL image, prints admission counts and derived
entry summaries, and does not start the original or save extracted bytes.

Run twice: right after a new game loads, and after about 10 game minutes (reached
quickly by pressing F4 for 400 % speed and waiting for the world's game-ms counter
to advance 600 000 ms).

Output: `cache/orig/<session>/d<display>-…/layer_blocks.txt` with per-block
`equal` or `count=N first=[o1, o2, …]`. **Nothing of the original is written** —
counts and offsets only.

Standard library plus the pre-authorised Wine runner (`docs/testing/original-wine.md`).
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import backend  # noqa: E402
import gamestate  # noqa: E402
import macros  # noqa: E402

try:
    import pefile  # noqa: E402
except ImportError:
    pefile = None  # must be present; fail-fast below

ANNOWELT = "annowelt.dll"
IMAGE_BASE = 0x10000000          # preferred base of AnnoWelt.dll (DLL header)
BLOCK_BASE_VA = 0x100d1140       # the layer-table base (round-ag confirmed)
BLOCK_RVA = BLOCK_BASE_VA - IMAGE_BASE
BLOCK_SIZE = 0x1c00              # 7168 bytes per block
LAYER_COUNT = 5                  # layers 0..4; the sixth block is all zero (OQ-160)
GAME_MS_PER_WAIT = 10 * 60 * 1000  # 10 game minutes
FIRST_DIFFS = 10                 # at most this many offsets per mismatching block


def disk_block(dll_path: str, layer: int) -> bytes:
    """The `BLOCK_SIZE` bytes at `BLOCK_RVA + 0x1c00·layer` in the DLL on disk."""
    if pefile is None:
        raise RuntimeError("pefile is required; install it in the host environment")
    pe = pefile.PE(dll_path, fast_load=True)
    rva = BLOCK_RVA + BLOCK_SIZE * layer
    return pe.get_data(rva, BLOCK_SIZE)


def mem_block(b, welt_base: int, layer: int) -> bytes | None:
    """The `BLOCK_SIZE` bytes at the same RVA, read from the running process."""
    addr = welt_base + BLOCK_RVA + BLOCK_SIZE * layer
    return b.read(addr, BLOCK_SIZE)


def compare(mem: bytes | None, disk: bytes) -> dict:
    """Return a small dict for one block."""
    if mem is None:
        return {"status": "unreadable"}
    if len(mem) != len(disk):
        return {"status": "short", "mem_len": len(mem), "disk_len": len(disk)}
    diffs = [i for i in range(len(mem)) if mem[i] != disk[i]]
    if not diffs:
        return {"status": "equal"}
    return {"status": "differ", "count": len(diffs), "first": diffs[:FIRST_DIFFS]}


def wait_game_ms(b, delta_ms: int, real_timeout: float, poll: float = 2.0) -> dict:
    """Wait until the game-ms counter advances `delta_ms` or `real_timeout` elapses.
    Uses `b.state("world")` so no `regions()` is needed on the backend."""
    t0 = time.monotonic()
    deadline = t0 + real_timeout
    w0 = b.state("world").get("world")
    if w0 is None:
        return {"status": "no_world"}
    start = w0.get("counter")
    if start is None:
        return {"status": "no_counter"}
    while time.monotonic() < deadline:
        w = b.state("world").get("world")
        now = w.get("counter") if w else None
        if now is not None and now - start >= delta_ms:
            return {"status": "ok", "start_ms": start, "end_ms": now,
                    "elapsed_real_s": round(time.monotonic() - t0, 1)}
        time.sleep(poll)
    w = b.state("world").get("world")
    now = (w.get("counter") if w else None) or start
    return {"status": "timeout", "start_ms": start, "end_ms": now,
            "advanced_ms": now - start,
            "elapsed_real_s": round(time.monotonic() - t0, 1)}


def run(session: str, dll_path: str, keep: bool = False) -> dict:
    """One session: start, load a new game, read + compare, speed up, wait, read + compare again."""
    b = backend.WineBackend(session=session, keep=keep, audio=False)
    out_path = os.path.join(b.out_dir, "layer_blocks.txt")
    results: dict = {"out_dir": b.out_dir, "start": None, "runs": []}
    disk_blocks = [disk_block(dll_path, L) for L in range(LAYER_COUNT)]
    try:
        results["start"] = b.start(wait_game=180.0)
        macros.new_game(b, "citizen", timeout=240.0)
        welt_base = b.module_bases()[ANNOWELT]
        results["welt_base"] = hex(welt_base)

        def one_pass(label: str, after: dict | None = None) -> dict:
            per_block = []
            for L in range(LAYER_COUNT):
                m = mem_block(b, welt_base, L)
                per_block.append(compare(m, disk_blocks[L]))
            return {"label": label, "after": after or {}, "blocks": per_block}

        # Pass 1: right after the new game.
        results["runs"].append(one_pass("after_new_game"))
        # Speed up to 400 % (F4) and wait ~10 game minutes.
        try:
            b.key("f4")
        except backend.BackendError as ex:
            results["runs"][-1]["speed_error"] = str(ex)
        wait = wait_game_ms(b, GAME_MS_PER_WAIT, real_timeout=10 * 60.0)  # cap: 10 real minutes
        # Pass 2: after the wait.
        results["runs"].append(one_pass("after_10_game_minutes", after=wait))

        with open(out_path, "w", encoding="utf-8") as f:
            f.write(f"welt_base={hex(welt_base)}\n")
            f.write(f"RVA=0x{BLOCK_RVA:x} size=0x{BLOCK_SIZE:x} layers={LAYER_COUNT}\n")
            for r in results["runs"]:
                f.write(f"\n--- {r['label']} ({r.get('after', {})})\n")
                for i, block in enumerate(r["blocks"]):
                    f.write(f"layer {i}: {block}\n")
    finally:
        try:
            results["stop"] = b.stop()
        except Exception as ex:  # noqa: BLE001
            results["stop_error"] = str(ex)
    results["out_file"] = out_path
    return results


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--session", default=f"t30b4-{time.strftime('%Y%m%d-%H%M%S')}")
    ap.add_argument("--dll", default=r"C:\Users\altin\anno1503\game\app\AnnoWelt.dll",
                    help="path to AnnoWelt.dll on disk (for the compare)")
    ap.add_argument("--disk-summary", action="store_true",
                    help="summarize admission masks and costs on disk; no game run")
    ap.add_argument("--codes", help="comma-separated fresh codes for --disk-summary (0..255)")
    ap.add_argument("--keep", action="store_true", help="keep the run folder after stop")
    args = ap.parse_args()

    if pefile is None:
        print("pefile not importable; install it with pip", file=sys.stderr)
        return 2
    if args.disk_summary:
        for layer in range(LAYER_COUNT):
            block = disk_block(args.dll, layer)
            if len(block) != BLOCK_SIZE:
                raise ValueError("short layer block")
            codes = [code for code in range(0x400) if block[code]]
            print(f"layer {layer}: nonzero_admission={len(codes)} "
                  f"fresh_nonzero={sum(bool(v) for v in block[:0x100])} "
                  f"code7={block[7]} marked_code_admissions="
                  f"{sum(bool(block[c]) for c in range(0x80, 0x87))}")
            selected = [int(v, 0) for v in args.codes.split(',')] if args.codes else codes[:3]
            if any(code < 0 or code > 0xff for code in selected):
                raise ValueError("fresh tile codes must be in 0..255")
            for code in selected:
                costs = tuple(block[0x400 + code * 8:0x408 + code * 8])
                print(f"  code={code} admission={block[code]} costs={costs}")
            if layer in (0, 3) and not args.codes:
                for admission in sorted(set(block[:0x100])):
                    matching = [c for c in range(0x100) if block[c] == admission]
                    print(f"  fresh_admission={admission} codes={matching}")
        return 0
    try:
        r = run(args.session, args.dll, keep=args.keep)
    except Exception as ex:  # noqa: BLE001
        print(f"error: {ex}", file=sys.stderr)
        return 1
    print(f"output: {r['out_file']}")
    for pass_r in r["runs"]:
        print(f"[{pass_r['label']}]")
        for i, blk in enumerate(pass_r["blocks"]):
            print(f"  layer {i}: {blk}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
