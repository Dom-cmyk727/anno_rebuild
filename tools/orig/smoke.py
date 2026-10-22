"""Stage 0 smoke run of the original game (docs/testing/original-harness.md, section 4).

Starts the copy's launcher (DirectDraw via cnc-ddraw, windowed on the primary monitor),
re-finds and foregrounds the game window to screenshot it (PrintWindow, PNG), enumerates the
game modules (32-bit, by walking the WOW64 PEB loader list), reads a module's bytes from the
process and cross-checks each base against the file's PE identity (TimeDateStamp/SizeOfImage/CheckSum), and - when a scenario is running - scans the process heap
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


def verify_copy_unchanged(game_dir: str) -> list[str]:
    """SHA-256 every .exe/.dll of the copy against the matching file in the repo's game/app; return
    the names that differ or are unreadable (empty = the copy is byte-identical to game/app). Wrapper
    files (e.g. dgVoodoo2's DDraw.dll/D3DImm.dll) have no game/app counterpart and are skipped, so a
    difference only ever means a game file changed."""
    here = os.path.dirname(os.path.abspath(__file__))
    ref = os.path.join(os.path.abspath(os.path.join(here, "..", "..")), "game", "app")
    diffs: list[str] = []
    for n in sorted(os.listdir(game_dir)):
        if not n.lower().endswith((".exe", ".dll")):
            continue
        rp = os.path.join(ref, n)
        if not os.path.exists(rp):
            continue
        try:
            if sha256(os.path.join(game_dir, n)) != sha256(rp):
                diffs.append(n)
        except OSError:
            diffs.append(n + " (unreadable)")
    return diffs


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
    loader list over ReadProcessMemory. This is the primary method because a TH32CS_SNAPMODULE32
    snapshot came back empty in our runs (see enum_modules_toolhelp); EnumProcessModulesEx with
    LIST_MODULES_32BIT is the documented alternative for a 64-bit caller on a WOW64 target."""
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


def pe_identity(header: bytes | None) -> dict | None:
    """(TimeDateStamp, SizeOfImage, CheckSum) from a PE32 image header blob that starts at the
    DOS header (offset 0). These three fields differ between the game's DLLs, unlike the shared
    MSVC 6 DOS stub, so matching them memory-vs-disk identifies the module at a base. None if the
    blob is too short or has no PE signature."""
    if not header or len(header) < 0x40:
        return None
    e_lfanew = struct.unpack_from("<I", header, 0x3C)[0]
    if e_lfanew + 0x5C > len(header) or header[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        return None
    # COFF header at e_lfanew+4 (TimeDateStamp at +4 of it); PE32 optional header at e_lfanew+24
    # (SizeOfImage at +0x38 of it, CheckSum at +0x40).
    return {
        "timedatestamp": struct.unpack_from("<I", header, e_lfanew + 8)[0],
        "sizeofimage": struct.unpack_from("<I", header, e_lfanew + 0x50)[0],
        "checksum": struct.unpack_from("<I", header, e_lfanew + 0x58)[0],
    }


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


def find_world_candidates(pid: int, welt_base: int) -> tuple[list[dict], int | None]:
    """Every heap location whose first DWORD is the AnnoWelt world vtable (welt_base + 0xccdd8),
    with the 64-bit ms counter at +0x100, the DWORD at +0x108 and the tick at +0x10c of each.
    A candidate is accepted only when its counter is non-zero and tick == counter >> 1
    (docs/re/tick-loop.md): that rejects a stray match of the vtable value. Returns
    (candidate list, the accepted object's address or None)."""
    candidates: list[dict] = []
    accepted: int | None = None
    handle = open_read(pid)
    if not handle:
        return candidates, None
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
                        fields = read_memory(handle, obj + 0x100, 0x10)
                        if fields is not None:
                            counter = struct.unpack_from("<Q", fields, 0)[0]
                            at_108 = struct.unpack_from("<I", fields, 0x08)[0]
                            tick = struct.unpack_from("<I", fields, WORLD_TICK_OFF - 0x100)[0]
                            ok = counter != 0 and tick == (counter >> 1)
                            candidates.append({"addr": hex(obj), "counter": counter, "at_0x108":
                                               at_108, "tick": tick, "accepted": ok})
                            if ok and accepted is None:
                                accepted = obj
                        i = data.find(vtable, i + 4)
            addr = region + size
        return candidates, accepted
    finally:
        kernel32.CloseHandle(handle)


def read_tick_fields(pid: int, obj: int) -> tuple[int, int, int] | None:
    """(counter at +0x100, DWORD at +0x108, tick at +0x10c) of the world object."""
    handle = open_read(pid)
    if not handle:
        return None
    try:
        fields = read_memory(handle, obj + 0x100, 0x10)
        if fields is None:
            return None
        return (struct.unpack_from("<Q", fields, 0)[0], struct.unpack_from("<I", fields, 0x08)[0],
                struct.unpack_from("<I", fields, WORLD_TICK_OFF - 0x100)[0])
    finally:
        kernel32.CloseHandle(handle)


def enum_modules_toolhelp(pid: int) -> tuple[list[tuple[str, int, int]], int]:
    """Cross-check of enum_modules: a 32-bit-module toolhelp snapshot (TH32CS_SNAPMODULE32).
    Documented to read a WOW64 target's 32-bit modules from a 64-bit caller, but it can fail with
    ERROR_PARTIAL_COPY (299) early in the target's life. Returns (modules, last_error); last_error
    is the GetLastError of the final failed call, 0 when the snapshot was read."""
    out: list[tuple[str, int, int]] = []
    snap = -1
    last_error = 0
    for _ in range(50):
        snap = kernel32.CreateToolhelp32Snapshot(0x00000010, pid)  # TH32CS_SNAPMODULE32
        if snap != -1:
            break
        last_error = ctypes.get_last_error()
        time.sleep(0.05)
    if snap == -1:
        return out, last_error
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Module32FirstW(snap, ctypes.byref(entry))
        if not ok:
            last_error = ctypes.get_last_error()
        while ok:
            out.append((entry.szModule, int(entry.modBaseAddr or 0), int(entry.modBaseSize)))
            ok = kernel32.Module32NextW(snap, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snap)
    return out, last_error


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

    # Physical pixels: GetWindowRect and BitBlt must match the game's real resolution 1:1 (stage 3
    # compares pixels), whatever the desktop scaling is. PER_MONITOR_AWARE_V2 (-4), else the older
    # system-DPI-aware call.
    try:
        user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass
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
        dpi = user32.GetDpiForWindow(win[0]) or 96
        log["primary_display_scaling_percent"] = round(dpi * 100 / 96)
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
            # Once a scenario is running, the world object exists: locate it (all vtable matches,
            # one accepted) and read its tick several times. The heap scan is heavy, so attempt it
            # only every `scan_every` seconds.
            if not world_found and now >= next_scan:
                next_scan = now + args.scan_every
                for pid in pids_under(game_dir):
                    base = module_base(pid, "AnnoWelt.dll")
                    if not base:
                        continue
                    cands, accepted = find_world_candidates(pid, base[0])
                    if not cands:
                        continue
                    entry = {"pid": pid, "welt_base": hex(base[0]),
                             "vtable_value": hex(base[0] + WELT_VTABLE_RVA), "candidates": cands}
                    if accepted is not None:
                        reads = []
                        for k in range(4):  # at least three, a few seconds apart
                            if k:
                                time.sleep(2.5)
                            fv = read_tick_fields(pid, accepted)
                            if fv:
                                reads.append({"real_s": round(time.time() - t0, 3),
                                              "counter": fv[0], "at_0x108": fv[1], "tick": fv[2],
                                              "at_108_eq_tick": fv[1] == fv[2]})
                        entry["object"] = hex(accepted)
                        entry["reads"] = reads
                        if len(reads) >= 2:
                            dt = reads[-1]["real_s"] - reads[0]["real_s"]
                            dtick = reads[-1]["tick"] - reads[0]["tick"]
                            rate = dtick / dt if dt > 0 else 0.0
                            entry["ticks_per_real_s"] = round(rate, 1)
                            entry["implied_game_speed"] = round(rate / 500.0, 3)  # 500 ticks/s at 1x
                            entry["grew"] = dtick > 0
                        world_found = True
                    tick_reads.append(entry)
                    if world_found:
                        break
            # Module bases, refreshed each loop so an early close keeps the last good set
            # (do the DLLs load at their preferred 0x10000000 base? AnnoDisplay does; the rest
            # share that base and rebase).
            def want(n: str) -> bool:
                low = n.lower()
                return low.startswith("anno") or low in ("filegame.dll", "view.dll",
                                                         "complogic.dll", "ddraw.dll", "d3dimm.dll")

            for pid in pids_under(game_dir):
                walk = [(n, b, s) for n, b, s in enum_modules(pid) if want(n)]
                if not walk:
                    continue
                th, th_err = enum_modules_toolhelp(pid)
                handle = open_read(pid)
                mods = {}
                for n, b, s in walk:
                    rec = {"base": hex(b), "size": s}
                    # Cross-check the base against the file. The headers page is mapped from the
                    # file, so the PE identity at the base must equal the file's. The first 0x40
                    # bytes are only the DOS stub, identical across the MSVC 6 DLLs, so they cannot
                    # tell the modules apart; compare TimeDateStamp / SizeOfImage / CheckSum at
                    # e_lfanew instead (pe_identity).
                    path = os.path.join(game_dir, n)
                    if os.path.exists(path):
                        on_disk = open(path, "rb").read(0x100)
                        in_mem = read_memory(handle, b, 0x100) if handle else None
                        disk_id = pe_identity(on_disk)
                        mem_id = pe_identity(in_mem)
                        rec["pe_identity_disk"] = disk_id
                        rec["pe_identity_memory"] = mem_id
                        rec["identity_matches_disk"] = mem_id is not None and mem_id == disk_id
                    mods[n] = rec
                if handle:
                    kernel32.CloseHandle(handle)
                anno_mods = {
                    "method": "wow64_peb_walk (ReadProcessMemory on the 32-bit loader list)",
                    "modules": mods,
                    # EnumProcessModulesEx(LIST_MODULES_32BIT) and TH32CS_SNAPMODULE32 are the
                    # documented ways to read a WOW64 target's 32-bit modules from a 64-bit caller;
                    # in our runs the snapshot came back empty (count 0, error below), so the PEB
                    # walk is primary and the per-module identity_matches_disk flag (PE COFF fields,
                    # not the shared DOS stub) is the real cross-check of each base.
                    "toolhelp_snapmodule32_count": len([m for m in th if want(m[0])]),
                    "toolhelp_snapmodule32_error": th_err,
                }
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
