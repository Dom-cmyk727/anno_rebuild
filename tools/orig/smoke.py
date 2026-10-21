"""Stage 0 smoke run of the original game (docs/testing/original-harness.md, section 4).

Starts the copy's launcher, waits for its window, takes one screenshot (PrintWindow, PNG),
reads one memory value from the game process and checks it against the module file on disk,
closes the game (WM_CLOSE, then taskkill of the process tree), and hashes the copy again.
Sends no input. Standard library only (ctypes); Windows only.

Run with:  python -I tools/orig/smoke.py --game C:\\anno1503-original --out cache/orig/smoke-<tag>
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
psapi = ctypes.WinDLL("psapi", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
WM_CLOSE = 0x0010
PW_RENDERFULLCONTENT = 0x00000002
BI_RGB = 0
DIB_RGB_COLORS = 0

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


class MODULEINFO(ctypes.Structure):
    _fields_ = [
        ("lpBaseOfDll", ctypes.c_void_p),
        ("SizeOfImage", wintypes.DWORD),
        ("EntryPoint", ctypes.c_void_p),
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
    _fields_ = [("left", wintypes.LONG), ("top", wintypes.LONG), ("right", wintypes.LONG), ("bottom", wintypes.LONG)]


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


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


def process_tree(root_pid: int) -> list[int]:
    """root_pid and its descendants, from a toolhelp snapshot of all processes."""
    TH32CS_SNAPPROCESS = 0x00000002
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    parents: dict[int, int] = {}
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Process32FirstW(snap, ctypes.byref(entry))
        while ok:
            parents[entry.th32ProcessID] = entry.th32ParentProcessID
            ok = kernel32.Process32NextW(snap, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snap)
    tree = [root_pid]
    grown = True
    while grown:
        grown = False
        for pid, parent in parents.items():
            if parent in tree and pid not in tree:
                tree.append(pid)
                grown = True
    return tree


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
    TH32CS_SNAPPROCESS = 0x00000002
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
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
    STILL_ACTIVE = 259
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return False
    try:
        code = wintypes.DWORD()
        kernel32.GetExitCodeProcess(handle, ctypes.byref(code))
        return code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


def windows_of(pids: list[int]) -> list[tuple[int, str]]:
    found: list[tuple[int, str]] = []

    def cb(hwnd, _lparam):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buf, length + 1)
            rect = RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rect))
            if rect.right - rect.left > 0:
                found.append((hwnd, f"{buf.value} [{rect.right - rect.left}x{rect.bottom - rect.top}]"))
        return True

    user32.EnumWindows(EnumWindowsProc(cb), 0)
    return found


def capture_screen(hwnd: int) -> tuple[int, int, bytes]:
    """The window's rectangle as it is on the desktop (BitBlt from the screen DC)."""
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
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


def capture_window(hwnd: int) -> tuple[int, int, bytes]:
    """The window's client-independent full image as BGRA rows (top-down), via PrintWindow."""
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    screen_dc = user32.GetDC(None)
    mem_dc = gdi32.CreateCompatibleDC(screen_dc)
    bmp = gdi32.CreateCompatibleBitmap(screen_dc, w, h)
    old = gdi32.SelectObject(mem_dc, bmp)
    ok = user32.PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT)
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = w
    info.bmiHeader.biHeight = -h  # top-down
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = BI_RGB
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem_dc, bmp, 0, h, buf, ctypes.byref(info), DIB_RGB_COLORS)
    gdi32.SelectObject(mem_dc, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(None, screen_dc)
    if not ok:
        raise RuntimeError("PrintWindow failed")
    return w, h, buf.raw


def write_png(path: str, w: int, h: int, bgra: bytes) -> None:
    rows = bytearray()
    stride = w * 4
    for y in range(h):
        rows.append(0)  # filter: none
        row = bgra[y * stride : (y + 1) * stride]
        # BGRA -> RGBA
        rgba = bytearray(len(row))
        rgba[0::4] = row[2::4]
        rgba[1::4] = row[1::4]
        rgba[2::4] = row[0::4]
        rgba[3::4] = b"\xff" * (w)
        rows += rgba

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def module_base(pid: int, name: str) -> tuple[int, int] | None:
    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not handle:
        return None
    try:
        mods = (ctypes.c_void_p * 1024)()
        needed = wintypes.DWORD()
        psapi.EnumProcessModules(handle, mods, ctypes.sizeof(mods), ctypes.byref(needed))
        for i in range(needed.value // ctypes.sizeof(ctypes.c_void_p)):
            buf = ctypes.create_unicode_buffer(260)
            psapi.GetModuleBaseNameW(handle, ctypes.c_void_p(mods[i]), buf, 260)
            if buf.value.lower() == name.lower():
                info = MODULEINFO()
                psapi.GetModuleInformation(handle, ctypes.c_void_p(mods[i]), ctypes.byref(info), ctypes.sizeof(info))
                return int(mods[i] or 0), int(info.SizeOfImage)
        return None
    finally:
        kernel32.CloseHandle(handle)


def read_memory(pid: int, address: int, size: int) -> bytes:
    handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not handle:
        raise RuntimeError(f"OpenProcess failed: {ctypes.get_last_error()}")
    try:
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf, size, ctypes.byref(got))
        if not ok or got.value != size:
            raise RuntimeError(f"ReadProcessMemory at {address:#x}: {ctypes.get_last_error()}")
        return buf.raw
    finally:
        kernel32.CloseHandle(handle)


def pe_file_bytes(path: str, rva: int, size: int) -> bytes:
    """The bytes at an RVA of a PE file on disk (sections mapped by file offset)."""
    with open(path, "rb") as f:
        data = f.read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = pe + 24
    sec = opt + struct.unpack_from("<H", data, pe + 20)[0]
    for i in range(nsec):
        off = sec + i * 40
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, off + 8)
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            start = rawptr + (rva - vaddr)
            return data[start : start + size]
    raise RuntimeError(f"RVA {rva:#x} not in any section of {path}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True, help="the copy's folder (never game/app)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--launcher", default="1503Startup.exe")
    ap.add_argument("--wait", type=float, default=45.0, help="seconds to wait for a window")
    ap.add_argument("--hold", type=float, default=10.0, help="seconds to keep it open")
    ap.add_argument("--env", action="append", default=[], help="KEY=VALUE for the game (wrapper tests)")
    ap.add_argument("--settle", type=float, default=3.0, help="seconds between the window and the screenshot")
    ap.add_argument("--method", choices=["printwindow", "screen"], default="printwindow")
    ap.add_argument("--arg", action="append", default=[], help="a command-line argument for the launcher (e.g. /w)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    log: dict = {"started": datetime.datetime.now().isoformat(timespec="seconds"), "game": args.game}
    game_dir = os.path.abspath(args.game)
    exe = os.path.join(game_dir, args.launcher)
    log["launcher_sha256"] = sha256(exe)
    env = dict(os.environ)
    for kv in args.env:
        k, v = kv.split("=", 1)
        env[k] = v
    t0 = time.time()
    log["arguments"] = args.arg
    proc = subprocess.Popen([exe, *args.arg], cwd=game_dir, env=env)
    log["pid"] = proc.pid
    win = None
    tree: list[int] = [proc.pid]
    while time.time() - t0 < args.wait:
        time.sleep(0.5)
        tree = sorted(set(process_tree(proc.pid)) | set(pids_under(game_dir)))
        found = windows_of(tree)
        if found:
            win = found[0]
            break
    log["process_tree"] = tree
    log["window_found_s"] = round(time.time() - t0, 1) if win else None
    if win is None:
        log["result"] = "no window"
    else:
        hwnd, title = win
        log["window"] = title
        time.sleep(args.settle)  # let the first frame come up
        w, h, bgra = capture_screen(hwnd) if args.method == "screen" else capture_window(hwnd)
        log["capture_method"] = args.method
        shot = os.path.join(args.out, "screenshot.png")
        write_png(shot, w, h, bgra)
        nonblack = sum(1 for i in range(0, len(bgra), 4 * 97) if bgra[i] or bgra[i + 1] or bgra[i + 2])
        log["screenshot"] = {"file": shot, "size": [w, h], "sampled_nonblack": nonblack}
        # One memory value: the game module's code bytes at a function in memory and on disk.
        # The game's modules loaded at the menu, for the record.
        loaded: list[str] = []
        for pid in tree:
            handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
            if not handle:
                continue
            mods = (ctypes.c_void_p * 1024)()
            needed = wintypes.DWORD()
            psapi.EnumProcessModules(handle, mods, ctypes.sizeof(mods), ctypes.byref(needed))
            for i in range(needed.value // ctypes.sizeof(ctypes.c_void_p)):
                nm = ctypes.create_unicode_buffer(260)
                psapi.GetModuleBaseNameW(handle, ctypes.c_void_p(mods[i]), nm, 260)
                loaded.append(f"{pid}:{nm.value}")
            kernel32.CloseHandle(handle)
        log["modules_loaded"] = loaded
        # One memory value: the code bytes of AnnoFrame.dll's main loop (RVA 0x87f0, docs/re/tick-loop.md)
        # read from the process and compared with the file on disk.
        for module, rva in (("AnnoFrame.dll", 0x000087F0),):
            mod_path = os.path.join(game_dir, module)
            base = None
            for pid in tree:
                b = module_base(pid, module)
                if b:
                    base = b
                    log["module"] = {"name": module, "pid": pid, "base": hex(b[0]), "size": b[1]}
                    break
            if base is None:
                log["memory_check"] = {"module": module, "result": "module not loaded"}
                continue
            mem = read_memory(next(p for p in tree if module_base(p, module)), base[0] + rva, 32)
            disk = pe_file_bytes(mod_path, rva, 32)
            log["memory_check"] = {
                "module": module,
                "rva": hex(rva),
                "in_memory": mem.hex(),
                "on_disk": disk.hex(),
                "equal": mem == disk,
            }
        time.sleep(args.hold)
        # Close: the game's own WM_CLOSE, then taskkill of the recorded tree.
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        for _ in range(60):
            if proc.poll() is not None:
                break
            time.sleep(0.5)
        log["closed_by_wm_close"] = proc.poll() is not None
    # Stop every game process left (the launcher may have exited with the game still running).
    leftover = sorted(set(pids_under(game_dir)) | set(tree))
    for pid in leftover:
        if alive(pid):
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
    log["taskkill_pids"] = leftover
    time.sleep(1.0)
    left = [p for p in tree if alive(p)]
    log["left_running"] = left
    log["copy_sha256_after"] = {n: sha256(os.path.join(game_dir, n)) for n in ("1503Startup.exe", "AnnoWelt.dll")}
    log["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
    with open(os.path.join(args.out, "log.json"), "w", encoding="utf-8") as f:
        json.dump(log, f, indent=2)
    print(json.dumps(log, indent=2))
    return 0 if win else 1


if __name__ == "__main__":
    sys.exit(main())
