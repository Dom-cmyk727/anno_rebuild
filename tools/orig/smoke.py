"""Stage 0 smoke run of the original game (docs/testing/original-harness.md, section 4).

Starts the copy's launcher (DirectDraw via cnc-ddraw, windowed on the primary monitor),
re-finds and foregrounds the game window to screenshot it (PrintWindow, PNG), enumerates the
game modules (32-bit, via a toolhelp snapshot), reads a module's bytes from the process and
compares them with the file on disk, and - when a scenario is running - scans the process heap
for the world object (its first field is the AnnoWelt world vtable) and reads the world tick
twice to show it growing. Closes the game (WM_CLOSE, then taskkill of every process in the copy
folder) and hashes the copy again. Sends no input. Standard library only (ctypes); Windows only.

Run:  python -I tools/orig/smoke.py --game C:\\anno1503-original --out cache/orig/smoke-<tag> --hold 120
"""

from __future__ import annotations

import argparse
import ctypes
import datetime
import hashlib
import json
import os
import struct
import subprocess
import sys
import time
import zlib
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)

PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
WM_CLOSE = 0x0010
PW_RENDERFULLCONTENT = 0x00000002
BI_RGB = 0
DIB_RGB_COLORS = 0
# AnnoWelt world object: vtable RVA, tick offset in the object (docs/re/tick-loop.md).
WELT_VTABLE_RVA = 0x000CCDD8
WORLD_TICK_OFF = 0x10C

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_void_p),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.c_void_p),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", ctypes.c_void_p),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * 260),
    ]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wintypes.DWORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wintypes.DWORD),
        ("Protect", wintypes.DWORD),
        ("Type", wintypes.DWORD),
    ]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


class RECT(ctypes.Structure):
    _fields_ = [("left", wintypes.LONG), ("top", wintypes.LONG), ("right", wintypes.LONG),
                ("bottom", wintypes.LONG)]


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def image_path(pid: int) -> str:
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return ""
    try:
        buf = ctypes.create_unicode_buffer(1024)
        size = wintypes.DWORD(1024)
        if kernel32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            return buf.value
        return ""
    finally:
        kernel32.CloseHandle(handle)


def pids_under(folder: str) -> list[int]:
    """Every process whose executable is inside folder (the game may start children and exit)."""
    snap = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)  # TH32CS_SNAPPROCESS
    pids: list[int] = []
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Process32FirstW(snap, ctypes.byref(entry))
        while ok:
            pid = entry.th32ProcessID
            if pid and image_path(pid).lower().startswith(folder.lower() + os.sep):
                pids.append(pid)
            ok = kernel32.Process32NextW(snap, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snap)
    return pids


def alive(pid: int) -> bool:
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return False
    try:
        code = wintypes.DWORD()
        kernel32.GetExitCodeProcess(handle, ctypes.byref(code))
        return code.value == 259  # STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


ntdll = ctypes.WinDLL("ntdll", use_last_error=True)


def _rpm(handle, addr: int, size: int) -> bytes | None:
    buf = ctypes.create_string_buffer(size)
    got = ctypes.c_size_t()
    ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, size, ctypes.byref(got))
    return buf.raw if ok and got.value == size else None


def enum_modules(pid: int) -> list[tuple[str, int, int]]:
    """(name, base, size) of every 32-bit module of a WOW64 process, by walking the 32-bit PEB
    loader list over ReadProcessMemory. The Win32 module APIs return only the 64-bit shim modules
    of a WOW64 target when the caller is 64-bit, so they cannot see the Anno DLLs; this can."""
    out: list[tuple[str, int, int]] = []
    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not handle:
        return out
    try:
        # ProcessWow64Information (26) returns the address of the process's 32-bit PEB.
        peb32 = ctypes.c_ulonglong(0)
        status = ntdll.NtQueryInformationProcess(handle, 26, ctypes.byref(peb32),
                                                 ctypes.sizeof(peb32), None)
        if status != 0 or peb32.value == 0:
            return out
        peb = peb32.value
        ldr_b = _rpm(handle, peb + 0x0C, 4)  # PEB32.Ldr
        if not ldr_b:
            return out
        ldr = struct.unpack("<I", ldr_b)[0]
        head = ldr + 0x0C  # PEB_LDR_DATA32.InLoadOrderModuleList (a LIST_ENTRY32)
        first = _rpm(handle, head, 4)
        if not first:
            return out
        node = struct.unpack("<I", first)[0]
        seen = set()
        while node and node != head and node not in seen:
            seen.add(node)
            ent = _rpm(handle, node, 0x34)  # LDR_DATA_TABLE_ENTRY32 (InLoadOrderLinks at +0)
            if not ent:
                break
            flink = struct.unpack_from("<I", ent, 0)[0]
            base = struct.unpack_from("<I", ent, 0x18)[0]
            size = struct.unpack_from("<I", ent, 0x20)[0]
            name_len = struct.unpack_from("<H", ent, 0x2C)[0]
            name_buf = struct.unpack_from("<I", ent, 0x30)[0]
            name = ""
            if name_buf and 0 < name_len <= 520:
                nb = _rpm(handle, name_buf, name_len)
                if nb:
                    name = nb.decode("utf-16-le", "replace")
            if base:
                out.append((name, base, size))
            node = flink
        return out
    finally:
        kernel32.CloseHandle(handle)


def module_base(pid: int, name: str) -> tuple[int, int] | None:
    for n, base, size in enum_modules(pid):
        if n.lower() == name.lower():
            return base, size
    return None


def find_game_window(folder: str) -> tuple[int, str, int, int] | None:
    """The largest visible top-level window of a process whose exe is in folder (found fresh)."""
    pids = set(pids_under(folder))
    best: list[tuple[int, int, str, int, int]] = []  # (area, hwnd, title, w, h)

    def cb(hwnd, _lp):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            rect = RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rect))
            w, h = rect.right - rect.left, rect.bottom - rect.top
            if w > 50 and h > 50:
                length = user32.GetWindowTextLengthW(hwnd)
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buf, length + 1)
                best.append((w * h, hwnd, buf.value, w, h))
        return True

    user32.EnumWindows(EnumWindowsProc(cb), 0)
    if not best:
        return None
    best.sort(reverse=True)
    _, hwnd, title, w, h = best[0]
    return hwnd, title, w, h


def capture(hwnd: int) -> tuple[int, int, bytes]:
    """Bring the window to the front, then BitBlt its on-screen rectangle (works for a
    Direct3D-rendered dgVoodoo2 window, where PrintWindow returns black)."""
    user32.ShowWindow(hwnd, 5)  # SW_SHOW
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)
    user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0002)  # HWND_TOPMOST, NOMOVE|NOSIZE
    time.sleep(0.6)
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        return 0, 0, b""
    screen_dc = user32.GetDC(None)
    mem_dc = gdi32.CreateCompatibleDC(screen_dc)
    bmp = gdi32.CreateCompatibleBitmap(screen_dc, w, h)
    old = gdi32.SelectObject(mem_dc, bmp)
    SRCCOPY = 0x00CC0020
    gdi32.BitBlt(mem_dc, 0, 0, w, h, screen_dc, rect.left, rect.top, SRCCOPY)
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = w
    info.bmiHeader.biHeight = -h
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = BI_RGB
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem_dc, bmp, 0, h, buf, ctypes.byref(info), DIB_RGB_COLORS)
    gdi32.SelectObject(mem_dc, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(None, screen_dc)
    return w, h, buf.raw


def nonblack(bgra: bytes) -> int:
    return sum(1 for i in range(0, len(bgra), 4 * 37) if bgra[i] or bgra[i + 1] or bgra[i + 2])


def write_png(path: str, w: int, h: int, bgra: bytes) -> None:
    rows = bytearray()
    stride = w * 4
    for y in range(h):
        rows.append(0)
        row = bgra[y * stride:(y + 1) * stride]
        rgba = bytearray(len(row))
        rgba[0::4] = row[2::4]
        rgba[1::4] = row[1::4]
        rgba[2::4] = row[0::4]
        rgba[3::4] = b"\xff" * w
        rows += rgba

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def open_read(pid: int):
    return kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)


def read_memory(handle, address: int, size: int) -> bytes | None:
    buf = ctypes.create_string_buffer(size)
    got = ctypes.c_size_t()
    ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf, size, ctypes.byref(got))
    if not ok or got.value != size:
        return None
    return buf.raw


def pe_file_bytes(path: str, rva: int, size: int) -> bytes:
    with open(path, "rb") as f:
        data = f.read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    sec = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
    for i in range(nsec):
        off = sec + i * 40
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            start = rawptr + (rva - vaddr)
            return data[start:start + size]
    raise RuntimeError(f"RVA {rva:#x} not in any section of {path}")


def find_world_tick(pid: int, welt_base: int) -> tuple[int, int] | None:
    """Scan the writable heap for the world object (first field = the AnnoWelt vtable); return
    (object address, tick at +0x10c). Best effort: returns None if not found."""
    handle = open_read(pid)
    if not handle:
        return None
    try:
        vtable = struct.pack("<I", welt_base + WELT_VTABLE_RVA)
        addr = 0
        mbi = MEMORY_BASIC_INFORMATION()
        while addr < 0x7FFF0000:
            if not kernel32.VirtualQueryEx(handle, ctypes.c_void_p(addr), ctypes.byref(mbi),
                                           ctypes.sizeof(mbi)):
                break
            region = int(mbi.BaseAddress or addr)
            size = int(mbi.RegionSize)
            # MEM_COMMIT (0x1000) and writable (PAGE_READWRITE 0x04 / EXECUTE_READWRITE 0x40).
            if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and size <= (64 << 20):
                data = read_memory(handle, region, size)
                if data:
                    i = data.find(vtable)
                    while i != -1:
                        obj = region + i
                        # The world object: tick (+0x10c) == the 64-bit ms counter (+0x100) >> 1
                        # (docs/re/tick-loop.md). That check rejects a stray vtable-value match.
                        fields = read_memory(handle, obj + 0x100, 0x10)
                        if fields is not None:
                            counter = struct.unpack_from("<Q", fields, 0)[0]
                            val = struct.unpack_from("<I", fields, WORLD_TICK_OFF - 0x100)[0]
                            if val == (counter >> 1) and counter < (1 << 40):
                                return obj, val
                        i = data.find(vtable, i + 4)
            addr = region + size
        return None
    finally:
        kernel32.CloseHandle(handle)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--launcher", default="1503Startup.exe")
    ap.add_argument("--wait", type=float, default=60.0)
    ap.add_argument("--hold", type=float, default=120.0,
                    help="seconds to let the owner reach a scenario")
    ap.add_argument("--shot-every", type=float, default=10.0)
    ap.add_argument("--scan-every", type=float, default=15.0, help="seconds between world-tick heap scans")
    ap.add_argument("--arg", action="append", default=[])
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    game_dir = os.path.abspath(args.game)
    exe = os.path.join(game_dir, args.launcher)
    log: dict = {
        "started": datetime.datetime.now().isoformat(timespec="seconds"),
        "game": game_dir,
        "launcher_sha256": sha256(exe),
        "arguments": args.arg,
    }
    t0 = time.time()
    proc = subprocess.Popen([exe, *args.arg], cwd=game_dir)
    log["launcher_pid"] = proc.pid
    win = None
    while time.time() - t0 < args.wait:
        time.sleep(0.5)
        win = find_game_window(game_dir)
        if win:
            break
    if win is None:
        log["result"] = "no window"
    else:
        log["window_found_s"] = round(time.time() - t0, 1)
        log["window"] = f"{win[1]} [{win[2]}x{win[3]}]"
        shots: list[dict] = []
        tick_reads: list[dict] = []
        best_shot = None
        next_shot = time.time()
        next_scan = time.time() + 8.0
        world_found = False
        anno_mods: dict = {}
        end = time.time() + args.hold
        while time.time() < end:
            now = time.time()
            if now >= next_shot:
                w = find_game_window(game_dir)
                if w:
                    cw, ch, bgra = capture(w[0])
                    if not (cw and ch):
                        shots.append({"t_s": round(now - t0, 1), "found": f"{w[1]} [{w[2]}x{w[3]}]",
                                      "captured": [cw, ch], "note": "capture empty"})
                    if cw and ch:
                        nb = nonblack(bgra)
                        name = os.path.join(args.out, f"shot-{int(now - t0):03d}s.png")
                        write_png(name, cw, ch, bgra)
                        rec = {"t_s": round(now - t0, 1), "file": name, "size": [cw, ch],
                               "nonblack": nb}
                        shots.append(rec)
                        if best_shot is None or nb > best_shot["nonblack"]:
                            best_shot = rec
                next_shot = now + args.shot_every
            # Once a scenario is running, the world object exists: read its tick twice. The heap
            # scan is heavy, so attempt it only every `scan_every` seconds.
            if not world_found and now >= next_scan:
                next_scan = now + args.scan_every
                for pid in pids_under(game_dir):
                    base = module_base(pid, "AnnoWelt.dll")
                    if not base:
                        continue
                    found = find_world_tick(pid, base[0])
                    if found:
                        obj, tick1 = found
                        time.sleep(3.0)
                        handle = open_read(pid)
                        again = read_memory(handle, obj + WORLD_TICK_OFF, 4)
                        kernel32.CloseHandle(handle)
                        tick2 = struct.unpack("<I", again)[0] if again else None
                        tick_reads.append({"pid": pid, "welt_base": hex(base[0]),
                                           "object": hex(obj), "tick1": tick1, "tick2": tick2,
                                           "grew": tick2 is not None and tick2 > tick1})
                        world_found = True
                        break
            # Module bases, refreshed each loop so an early close keeps the last good set
            # (do the DLLs load at their preferred 0x10000000 base? AnnoDisplay does; the rest
            # share that base and rebase).
            for pid in pids_under(game_dir):
                cur = {}
                for n, base, size in enum_modules(pid):
                    low = n.lower()
                    if low.startswith("anno") or low in ("filegame.dll", "view.dll",
                                                         "complogic.dll", "ddraw.dll", "d3dimm.dll"):
                        cur[n] = {"base": hex(base), "size": size}
                if cur:
                    anno_mods = cur
            time.sleep(1.0)
        log["screenshots"] = shots
        log["best_shot"] = best_shot
        log["tick_reads"] = tick_reads
        log["anno_module_bases"] = anno_mods
        # One memory value: a game module's code bytes, in memory vs on disk.
        for pid in pids_under(game_dir):
            base = module_base(pid, "AnnoFrame.dll")
            if base:
                handle = open_read(pid)
                mem = read_memory(handle, base[0] + 0x87F0, 32)
                kernel32.CloseHandle(handle)
                disk = pe_file_bytes(os.path.join(game_dir, "AnnoFrame.dll"), 0x87F0, 32)
                log["memory_check"] = {"module": "AnnoFrame.dll", "pid": pid, "rva": "0x87f0",
                                       "in_memory": mem.hex() if mem else None,
                                       "on_disk": disk.hex(), "equal": mem == disk}
                break
        # Close: the game window first, then force-kill whatever is left.
        w = find_game_window(game_dir)
        if w:
            user32.PostMessageW(w[0], WM_CLOSE, 0, 0)
        for _ in range(40):
            if not pids_under(game_dir):
                break
            time.sleep(0.5)
    leftover = sorted(set(pids_under(game_dir)) | {proc.pid})
    for pid in leftover:
        if alive(pid):
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
    time.sleep(1.0)
    log["left_running"] = [p for p in leftover if alive(p)]
    log["copy_sha256_after"] = {n: sha256(os.path.join(game_dir, n))
                                for n in ("1503Startup.exe", "AnnoWelt.dll", "AnnoFrame.dll")}
    log["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
    with open(os.path.join(args.out, "log.json"), "w", encoding="utf-8") as f:
        json.dump(log, f, indent=2)
    print(json.dumps({k: v for k, v in log.items() if k != "screenshots"}, indent=2))
    return 0 if win else 1


if __name__ == "__main__":
    sys.exit(main())
