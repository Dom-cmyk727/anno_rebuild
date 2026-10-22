"""The original's player profile `SaveGame/<name>/user.dat`: read it, and set the music and speech
volume (T16: silent test runs, owner request 2026-10-22).

Layout (measured 2026-10-22 in the Wine runner; generic chunks of docs/formats/chunk-container.md:
12-byte NUL-padded name, u32 size, payload):

  USDAT                      the file
    PROFIL
      USGUID   16 bytes      profile GUID
      SONGS    n x (u16 song id, u16 enabled)    the Music tab's playlist ticks
      VOLUME   i32 music?, i32 speech?           0 = slider at the right end (full);
                                                 -250 = slider at the left end (what the game wrote
                                                 after both sliders were dragged fully left; the two
                                                 were changed together, so which i32 is which is
                                                 not separated)
      SPEECH   u32 bitmask   which speech messages play (all set: 0xffffffff)
      HELPPLAYER u32

How -250 maps to DirectSound attenuation is not traced; it is the game's own minimum.
Standard library only; import-safe under `python -I`.
"""

from __future__ import annotations

import struct

VOLUME_MIN = -250        # the game's value for a slider at its left end (measured, see above)
CONTAINERS = {"USDAT", "PROFIL"}


def chunks(data: bytes, off: int = 0, end: int | None = None, path: str = ""):
    """Yield (path, name, payload_offset, size) depth-first for the known containers."""
    end = len(data) if end is None else end
    while off + 16 <= end:
        raw = data[off:off + 12]
        name = raw.split(b"\0", 1)[0].decode("ascii", "replace")
        size = struct.unpack_from("<I", data, off + 12)[0]
        if off + 16 + size > end:
            raise ValueError(f"chunk {name!r} at {off:#x} overruns its parent")
        p = f"{path}/{name}"
        yield p, name, off + 16, size
        if name in CONTAINERS:
            yield from chunks(data, off + 16, off + 16 + size, p)
        off += 16 + size


def read(data: bytes) -> dict:
    out: dict = {}
    for p, name, at, size in chunks(data):
        if name == "VOLUME" and size == 8:
            out["volume"] = list(struct.unpack_from("<ii", data, at))
        elif name == "SPEECH" and size == 4:
            out["speech_mask"] = struct.unpack_from("<I", data, at)[0]
        elif name == "SONGS":
            out["songs"] = [list(struct.unpack_from("<HH", data, at + 4 * i)) for i in range(size // 4)]
    return out


def set_volume(data: bytes, values=(VOLUME_MIN, VOLUME_MIN)) -> bytes:
    for _p, name, at, size in chunks(data):
        if name == "VOLUME" and size == 8:
            b = bytearray(data)
            struct.pack_into("<ii", b, at, *values)
            return bytes(b)
    raise ValueError("no VOLUME chunk of 8 bytes in this profile")
