"""UI.1 layout oracle. Spec: docs/re/ui.md, data/ui/README.md.

  python -I tools/ui_layout.py write-inventory --game-dir DIR --out data/ui
  python -I tools/ui_layout.py summary --game-dir DIR
  python -I tools/ui_layout.py self-test
  python -I tools/ui_layout.py validate --layout data/ui [--game-dir DIR]
  python -I tools/ui_layout.py mock --layout data/ui
  python -I tools/ui_layout.py compare --layout data/ui --shot PNG

`--self-test` / `--validate` / `--mock` / `--compare` are accepted as well.
Derived facts only. Does not copy original pixels or UI strings.
Stdlib plus tools/text.py and tools/tex.py. compare uses Pillow when present.
"""
from __future__ import annotations

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tex  # noqa: E402
import text  # noqa: E402

DAT_UI_FILES = ("statusbar.dat", "htmlpic.dat", "ScapeIcon.dat")

# Filename stems grouped only from a Data/[FILE] pointer or a documented HUD
# graphic name (docs/re/ui.md §0). Tutorial / instant-help stems stay ungrouped.
GROUP_STEMS = {
    "hud-chrome": ("fenster",),
    "statusbar": ("statusbar", "statusbaraddon"),
    "construction": ("B_Baumenue", "B_Baukosten", "B_Betriebskosten", "B_Belohnungsbauten"),
    "demolish": ("Abreissen_Gebaeude",),
    "promotion": ("Aufstiegsbedingungen",),
    "fonts": (
        "MainSmall",
        "MainMedium",
        "MainLarge",
        "HelpNormal",
        "Startup",
    ),
}


def quote_bytes(raw: bytes) -> str:
    return text.quote(raw)


def quote_str(s: str) -> str:
    return quote_bytes(s.encode("latin1"))


def unquote_field(raw: bytes) -> bytes:
    raw = raw.strip(b" \t")
    if len(raw) >= 2 and raw[0] == 0x22 and raw[-1] == 0x22:
        return raw[1:-1]
    return raw


def parse_ints(raw: bytes) -> list[int]:
    out = []
    for f in text.fields(raw):
        if not f:
            continue
        out.append(int(f.decode("ascii")))
    return out


def parse_dat_file(path: str) -> dict:
    buf = open(path, "rb").read()
    ls = text.lines(buf)
    file_name = None
    section = None
    rec: dict | None = None
    recs = []

    def flush() -> None:
        nonlocal rec
        if rec and rec.get("access") is not None:
            recs.append(rec)
        rec = None

    for _n, t, _e in ls:
        k = text.kind(t)
        if k[0] == "section":
            flush()
            section = k[1].decode("ascii")
            continue
        if k[0] != "entry":
            continue
        key = k[1].decode("ascii")
        val = k[2]
        if key == "Name" and section == "FILE":
            file_name = unquote_field(val).decode("latin1")
        if section != "TEXTURE":
            continue
        if key == "Access":
            flush()
            rec = {
                "access": unquote_field(val).decode("latin1"),
                "pos": (0, 0),
                "size": (0, 0),
                "segments": None,
                "table": None,
                "typ": None,
            }
        elif rec is None:
            continue
        elif key == "Pos":
            nums = parse_ints(val)
            if len(nums) >= 2:
                rec["pos"] = (nums[0], nums[1])
        elif key == "Size":
            nums = parse_ints(val)
            if len(nums) >= 2:
                rec["size"] = (nums[0], nums[1])
        elif key == "Segments":
            nums = parse_ints(val)
            rec["segments"] = tuple(nums)
        elif key == "Table":
            rec["table"] = unquote_field(val).decode("latin1")
        elif key == "Typ":
            rec["typ"] = val.strip().decode("ascii")
    flush()
    return {"file": file_name, "textures": recs, "path": path}


def tex_sizes(path: str) -> dict[int, tuple[int, int]]:
    if not os.path.isfile(path):
        return {}
    buf = open(path, "rb").read()
    try:
        es = tex.entries(buf)
    except (tex.Bad, OSError):
        return {}
    out = {}
    for kind, e in es:
        if kind == "TEXTURE":
            out[e["id"]] = (e["width"], e["height"])
    return out


def parse_inc_file(path: str) -> list[tuple[bytes, int]]:
    buf = open(path, "rb").read()
    recs = tex.access_records(buf)
    return recs if recs is not None else []


def stem_eight(filename: str) -> tuple[str, int, str]:
    base, ext = os.path.splitext(filename)
    eight = 1 if base.endswith("8") else 0
    stem = base[:-1] if eight else base
    return stem, eight, ext.lower()


def group_of(stem: str) -> str | None:
    for group, stems in GROUP_STEMS.items():
        if stem in stems:
            return group
    return None


def write_lines(path: str, lines: list[str]) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_inventory(game_dir: str, out_dir: str) -> dict:
    data_dir = os.path.join(game_dir, "Data")
    tg = os.path.join(game_dir, "ToolGfx")
    dat_lines = [
        "# Derived ACCESS tables from Data/*.dat. No original pixels.",
        "anno1503-ui-dat 1",
    ]
    dat_summary = []
    for fn in DAT_UI_FILES:
        parsed = parse_dat_file(os.path.join(data_dir, fn))
        file_field = parsed["file"] or "-"
        dat_lines.append(f"dat {fn} file {quote_str(file_field)} textures {len(parsed['textures'])}")
        dat_summary.append((fn, file_field, len(parsed["textures"])))
        for rec in parsed["textures"]:
            seg = rec["segments"]
            if not rec["segments"]:
                seg_s = "-"
            else:
                seg_s = " ".join(str(x) for x in rec["segments"])
            dat_lines.append(
                "tex access {acc} pos {x} {y} size {w} {h} segments {seg}".format(
                    acc=quote_str(rec["access"]),
                    x=rec["pos"][0],
                    y=rec["pos"][1],
                    w=rec["size"][0],
                    h=rec["size"][1],
                    seg=seg_s,
                )
            )

    inc_lines = [
        "# Derived ACCESS records from ToolGfx/*.inc plus .tex sizes. No original pixels.",
        "anno1503-ui-inc 1",
    ]
    files = sorted(f for f in os.listdir(tg) if f.lower().endswith(".inc"))
    pair_tex = {"both": 0, "only": 0, "only8": 0}
    stems_plain, stems_eight = set(), set()
    n_records = 0
    n_empty = 0
    n_named = 0
    for fn in files:
        stem, eight, _ext = stem_eight(fn)
        (stems_eight if eight else stems_plain).add(stem.lower())
        inc_path = os.path.join(tg, fn)
        recs = parse_inc_file(inc_path)
        tex_path = os.path.splitext(inc_path)[0] + ".tex"
        sizes = tex_sizes(tex_path)
        group = group_of(stem) or "-"
        inc_lines.append(
            "inc {name} stem {stem} eight {eight} group {group} records {n}".format(
                name=quote_str(fn),
                stem=quote_str(stem),
                eight=eight,
                group=group,
                n=len(recs),
            )
        )
        if not recs:
            n_empty += 1
        n_records += len(recs)
        n_named += 1 if recs else 0
        for src, num in recs:
            wh = sizes.get(num)
            if wh is None:
                w_s, h_s = "-", "-"
            else:
                w_s, h_s = str(wh[0]), str(wh[1])
            inc_lines.append(
                "access {src} id {num} width {w} height {h}".format(
                    src=quote_bytes(src),
                    num=num,
                    w=w_s,
                    h=h_s,
                )
            )
    both = stems_plain & stems_eight
    pair_tex = {
        "both": len(both),
        "only": len(stems_plain - stems_eight),
        "only8": len(stems_eight - stems_plain),
        "plain": len(stems_plain),
        "eight": len(stems_eight),
    }

    group_lines = [
        "# Filename groups with evidence in docs/re/ui.md section 0. Ungrouped stems are omitted.",
        "anno1503-ui-groups 1",
    ]
    for group, stems in GROUP_STEMS.items():
        group_lines.append(f"group {group} stems {len(stems)}")
        for stem in stems:
            group_lines.append(f"stem {quote_str(stem)} group {group}")

    write_lines(os.path.join(out_dir, "dat.txt"), dat_lines)
    write_lines(os.path.join(out_dir, "inc.txt"), inc_lines)
    write_lines(os.path.join(out_dir, "groups.txt"), group_lines)
    return {
        "dat": dat_summary,
        "inc_files": n_named + n_empty,
        "inc_records": n_records,
        "inc_empty": n_empty,
        "eight": pair_tex,
        "out": out_dir,
    }


def print_summary(game_dir: str) -> None:
    tg = os.path.join(game_dir, "ToolGfx")
    files = [f for f in os.listdir(tg) if os.path.isfile(os.path.join(tg, f))]
    print(f"toolgfx files={len(files)}")
    info = write_inventory(game_dir, os.path.join(os.path.dirname(HERE), "cache", "ui-inv-tmp"))
    print(
        "inc files={n} records={r} empty={e} eight_both={b} eight_only_plain={p} eight_only8={q}".format(
            n=info["inc_files"],
            r=info["inc_records"],
            e=info["inc_empty"],
            b=info["eight"]["both"],
            p=info["eight"]["only"],
            q=info["eight"]["only8"],
        )
    )
    for fn, tex_file, n in info["dat"]:
        print(f"dat {fn} file={tex_file} textures={n}")


ALLOWED = set(range(0x20, 0x7F)) | {0x09, 0x0A, 0x0D}
REPO = os.path.dirname(HERE)
CATEGORY_KEYS = (
    "public",
    "roads",
    "farms",
    "workshops",
    "coastal",
    "mines",
    "military",
    "ornamental",
)


class LayoutError(ValueError):
    pass


def control_byte_errors(buf: bytes, path: str) -> list[str]:
    """Bytes below 0x20 are refused, except LF and CR immediately before LF."""
    errors = []
    n = len(buf)
    i = 0
    while i < n:
        b = buf[i]
        if b >= 0x20 or b == 0x0A:
            i += 1
            continue
        if b == 0x0D and i + 1 < n and buf[i + 1] == 0x0A:
            i += 1
            continue
        errors.append(f"{path}: byte 0x{b:02x} at {i}")
        i += 1
    return errors


def markdown_table_errors(text: str, path: str) -> list[str]:
    errors = []
    cols = None
    header_line = 0
    for n, line in enumerate(text.splitlines(), 1):
        raw = line.rstrip("\r")
        if raw.startswith("|"):
            c = raw.count("|") - 1
            if cols is None:
                cols = c
                header_line = n
            elif c != cols:
                errors.append(
                    f"{path}:{n}: table row has {c} columns, table at line {header_line} has {cols}"
                )
        else:
            cols = None
    return errors


def ui1_paths(root: str) -> list[str]:
    paths = [os.path.join(root, "docs", "re", "ui.md")]
    data = os.path.join(root, "data", "ui")
    if os.path.isdir(data):
        for name in sorted(os.listdir(data)):
            p = os.path.join(data, name)
            if os.path.isfile(p):
                paths.append(p)
    return paths


def check_ui1_repo(root: str) -> list[str]:
    errors: list[str] = []
    for p in ui1_paths(root):
        with open(p, "rb") as fh:
            buf = fh.read()
        errors.extend(control_byte_errors(buf, p))
        if p.endswith(".md"):
            errors.extend(markdown_table_errors(buf.decode("utf-8"), p))
    oq = os.path.join(root, "docs", "open-questions.md")
    if os.path.isfile(oq):
        with open(oq, "rb") as fh:
            buf = fh.read()
        text = buf.decode("utf-8")
        mark = "## OQ-535"
        i = text.find(mark)
        if i >= 0:
            slice_text = text[i:]
            tag = oq + "#OQ-535"
            errors.extend(control_byte_errors(slice_text.encode("utf-8"), tag))
            errors.extend(markdown_table_errors(slice_text, tag))
    return errors


def check_ascii(buf: bytes, path: str) -> None:
    for i, b in enumerate(buf):
        if b not in ALLOWED:
            raise LayoutError(f"{path}: byte 0x{b:02x} at {i} is outside the layout alphabet")


def layout_lines(path: str) -> list[tuple[int, str]]:
    with open(path, "rb") as fh:
        buf = fh.read()
    check_ascii(buf, path)
    out = []
    for n, raw in enumerate(buf.split(b"\n"), 1):
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        line = raw.decode("ascii")
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        out.append((n, stripped))
    return out


def expect_version(lines: list[tuple[int, str]], kind: str, path: str) -> list[tuple[int, str]]:
    if not lines:
        raise LayoutError(f"{path}: empty")
    want = f"anno1503-ui-{kind} 1"
    if lines[0][1] != want:
        raise LayoutError(f"{path}:{lines[0][0]}: expected {want!r}, got {lines[0][1]!r}")
    return lines[1:]


def parse_hud(path: str) -> dict:
    body = expect_version(layout_lines(path), "hud", path)
    rec: dict = {"elements": [], "clicks": [], "resolution": None, "shot": None}
    for n, line in body:
        f = line.split()
        if not f:
            continue
        if f[0] == "resolution" and len(f) == 3:
            rec["resolution"] = (int(f[1]), int(f[2]))
        elif f[0] == "shot" and len(f) == 2:
            rec["shot"] = f[1]
        elif f[0] == "element":
            rec["elements"].append(_parse_element(f, path, n))
        elif f[0] == "click":
            rec["clicks"].append(_parse_click(f, path, n))
        else:
            raise LayoutError(f"{path}:{n}: unknown hud field {f[0]!r}")
    if rec["resolution"] is None:
        raise LayoutError(f"{path}: missing resolution")
    return rec


def _kv_after(fields: list[str], start: int) -> dict:
    out: dict = {}
    i = start
    while i < len(fields):
        key = fields[i]
        if i + 1 >= len(fields):
            out[key] = True
            break
        if key in ("pos", "size") and i + 2 < len(fields):
            out[key] = (int(fields[i + 1]), int(fields[i + 2]))
            i += 3
            continue
        out[key] = fields[i + 1]
        i += 2
    return out


def _parse_element(f: list[str], path: str, n: int) -> dict:
    if len(f) < 2:
        raise LayoutError(f"{path}:{n}: element needs a name")
    kv = _kv_after(f, 2)
    kv["name"] = f[1]
    return kv


def _parse_click(f: list[str], path: str, n: int) -> dict:
    if len(f) < 2:
        raise LayoutError(f"{path}:{n}: click needs a name")
    if len(f) == 3:
        return {"name": f[1], "x": int(f[2])}
    kv = _kv_after(f, 2)
    kv["name"] = f[1]
    return kv


def parse_construction(path: str) -> dict:
    body = expect_version(layout_lines(path), "construction", path)
    rec: dict = {
        "categories": {},
        "cells": [],
        "occupancy": {},
        "resolution": None,
        "shot": None,
        "grid_click_x": [],
        "grid_click_y": [],
        "open_key": None,
        "leave_key": None,
        "demolish": None,
        "grid": {},
    }
    for n, line in body:
        f = line.split()
        if f[0] == "resolution" and len(f) == 3:
            rec["resolution"] = (int(f[1]), int(f[2]))
        elif f[0] == "shot" and len(f) == 2:
            rec["shot"] = f[1]
        elif f[0] == "open_key" and len(f) == 2:
            rec["open_key"] = f[1]
        elif f[0] == "leave_key" and len(f) == 2:
            rec["leave_key"] = f[1]
        elif f[0] == "grid":
            rec["grid"] = _kv_after(f, 1)
        elif f[0] == "grid_click_x":
            rec["grid_click_x"] = [int(x) for x in f[1:]]
        elif f[0] == "grid_click_y":
            rec["grid_click_y"] = [int(x) for x in f[1:]]
        elif f[0] == "category":
            kv = _kv_after(f, 2)
            kv["key"] = f[1]
            rec["categories"][f[1]] = kv
        elif f[0] == "cell":
            if len(f) < 4:
                raise LayoutError(f"{path}:{n}: cell needs category col row")
            kv = _kv_after(f, 4)
            kv["category"] = f[1]
            kv["col"] = int(f[2])
            kv["row"] = int(f[3])
            rec["cells"].append(kv)
        elif f[0] == "occupancy":
            if len(f) < 10:
                raise LayoutError(f"{path}:{n}: occupancy needs 8 row strings")
            rows = f[2:10]
            for r, row in enumerate(rows):
                if len(row) != 4 or any(ch not in "UL-" for ch in row):
                    raise LayoutError(f"{path}:{n}: occupancy row {r} {row!r}")
            rec["occupancy"][f[1]] = rows
        elif f[0] == "demolish":
            rec["demolish"] = _kv_after(f, 1)
        else:
            raise LayoutError(f"{path}:{n}: unknown construction field {f[0]!r}")
    if rec["resolution"] is None:
        raise LayoutError(f"{path}: missing resolution")
    return rec


def access_names_from_inc(path: str) -> set[str]:
    names: set[str] = set()
    if not os.path.isfile(path):
        return names
    for _n, line in layout_lines(path):
        f = line.split()
        if f and f[0] == "access" and len(f) >= 2:
            raw = f[1]
            if raw.startswith('"') and raw.endswith('"'):
                names.add(raw[1:-1])
            else:
                names.add(raw)
    return names


def access_names_from_dat(path: str) -> set[str]:
    names: set[str] = set()
    if not os.path.isfile(path):
        return names
    for _n, line in layout_lines(path):
        f = line.split()
        if f and f[0] == "tex" and "access" in f:
            i = f.index("access")
            if i + 1 < len(f):
                raw = f[i + 1]
                if raw.startswith('"') and raw.endswith('"'):
                    names.add(raw[1:-1])
                else:
                    names.add(raw)
    return names


def validate_layout(layout_dir: str) -> list[str]:
    errors: list[str] = []
    hud_path = os.path.join(layout_dir, "hud.txt")
    con_path = os.path.join(layout_dir, "construction.txt")
    try:
        hud = parse_hud(hud_path)
    except LayoutError as e:
        errors.append(str(e))
        hud = None
    try:
        con = parse_construction(con_path)
    except LayoutError as e:
        errors.append(str(e))
        con = None
    inc_names = access_names_from_inc(os.path.join(layout_dir, "inc.txt"))
    dat_names = access_names_from_dat(os.path.join(layout_dir, "dat.txt"))
    all_names = inc_names | dat_names
    if hud:
        w, h = hud["resolution"]
        for el in hud["elements"]:
            pos = el.get("pos")
            size = el.get("size")
            if pos and (pos[0] < 0 or pos[1] < 0 or pos[0] >= w or pos[1] >= h):
                errors.append(f"hud element {el['name']} pos {pos} outside {w}x{h}")
            if size and (size[0] <= 0 or size[1] <= 0):
                errors.append(f"hud element {el['name']} size {size}")
            acc = el.get("access")
            if acc and all_names and acc not in all_names:
                errors.append(f"hud element {el['name']} ACCESS {acc} not in inventory")
        for cl in hud["clicks"]:
            pos = cl.get("pos")
            if pos and (pos[0] < 0 or pos[1] < 0 or pos[0] >= w or pos[1] >= h):
                errors.append(f"hud click {cl['name']} pos {pos} outside {w}x{h}")
    if con:
        w, h = con["resolution"]
        if set(con["categories"]) != set(CATEGORY_KEYS):
            errors.append(f"construction categories {sorted(con['categories'])} != {list(CATEGORY_KEYS)}")
        if len(con["grid_click_x"]) != 4 or len(con["grid_click_y"]) != 8:
            errors.append("construction grid_click must be 4 x and 8 y")
        seen = set()
        occ = con["occupancy"]
        for cell in con["cells"]:
            key = (cell["category"], cell["col"], cell["row"])
            if key in seen:
                errors.append(f"duplicate cell {key}")
            seen.add(key)
            if cell["category"] not in CATEGORY_KEYS:
                errors.append(f"cell category {cell['category']}")
            if not (0 <= cell["col"] < 4 and 0 <= cell["row"] < 8):
                errors.append(f"cell {key} out of 4x8")
            rows = occ.get(cell["category"])
            if rows:
                mark = rows[cell["row"]][cell["col"]]
                if cell.get("state") == "unlocked" and mark != "U":
                    errors.append(f"cell {key} state unlocked but occupancy {mark}")
        for cat, rows in occ.items():
            if cat not in CATEGORY_KEYS:
                errors.append(f"occupancy category {cat}")
            if len(rows) != 8:
                errors.append(f"occupancy {cat} has {len(rows)} rows")
        for x in con["grid_click_x"]:
            if x < 0 or x >= w:
                errors.append(f"grid click x {x} outside width {w}")
        for y in con["grid_click_y"]:
            if y < 0 or y >= h:
                errors.append(f"grid click y {y} outside height {h}")
    return errors


def mock_lines(layout_dir: str) -> list[str]:
    hud = parse_hud(os.path.join(layout_dir, "hud.txt"))
    lines = ["anno1503-ui-mock 1"]
    w, h = hud["resolution"]
    lines.append(f"resolution {w} {h}")
    for el in hud["elements"]:
        pos = el.get("pos")
        size = el.get("size")
        if pos and size:
            lines.append(
                "rect {name} {x} {y} {w} {h}".format(
                    name=el["name"], x=pos[0], y=pos[1], w=size[0], h=size[1]
                )
            )
        elif size:
            lines.append(f"size {el['name']} {size[0]} {size[1]}")
    # OQ-536 candidates
    lines.append("candidate statusbar_dat 0 733 746 35")
    lines.append("candidate statusbar_fenster 0 725 780 43")
    lines.append("candidate weltkarte 779 0 245 248")
    return lines


def _is_water(rgb: tuple[int, ...]) -> bool:
    r, g, b = rgb[0], rgb[1], rgb[2]
    return b > 90 and r < 80 and g < 140


def measure_hud_shot(path: str) -> dict:
    try:
        from PIL import Image
    except ImportError as e:
        raise LayoutError("Pillow is required for compare") from e
    im = Image.open(path).convert("RGB")
    pix = im.load()
    w, h = im.size
    return {"width": w, "height": h, "pix": pix, "im": im}


def compare_shot(shot: str, layout_dir: str) -> list[str]:
    hud = parse_hud(os.path.join(layout_dir, "hud.txt"))
    meas = measure_hud_shot(shot)
    errors = []
    w, h = hud["resolution"]
    if (meas["width"], meas["height"]) != (w, h):
        errors.append(f"shot size {meas['width']}x{meas['height']} != {w}x{h}")
        return errors
    pix = meas["pix"]
    bar = next((e for e in hud["elements"] if e["name"] == "statusbar"), None)
    panel = next((e for e in hud["elements"] if e["name"] == "panel"), None)
    if bar:
        x, y = bar["pos"]
        bw, _bh = bar["size"]
        if y <= 0 or y >= h:
            errors.append(f"bar y {y} outside shot")
        else:
            if _is_water(pix[20, y]):
                errors.append(f"bar_top {y} is water in the shot")
            if y > 0 and not _is_water(pix[20, y - 1]):
                errors.append(f"row {y - 1} above the bar is not water")
            if x + bw > w + 2:
                errors.append(f"bar width {bw} overruns {w}")
    if panel:
        px, _py = panel["pos"]
        if _is_water(pix[px, 100]):
            errors.append(f"panel_x {px} y=100 is water")
    return errors


def _write_synthetic_inc(path: str) -> None:
    # Minimal .inc ACCESS records matching tex.access_records: len-prefixed?
    # write-inventory is tested with a fake game dir of .dat only for self-test
    # of layout parse; inventory self-test uses a tiny keyed dat file.
    pass


def self_test() -> int:
    import tempfile

    n_ok = 0

    def check(cond: bool, msg: str) -> None:
        nonlocal n_ok
        if not cond:
            raise AssertionError(msg)
        n_ok += 1

    with tempfile.TemporaryDirectory() as td:
        hud = os.path.join(td, "hud.txt")
        con = os.path.join(td, "construction.txt")
        write_lines(
            hud,
            [
                "anno1503-ui-hud 1",
                "resolution 1024 768",
                "shot hud_idle_running",
                "element statusbar pos 0 733 size 746 35 access STATUSBAR method pixel-row",
                "element panel pos 779 0 size 245 733 access MAINUNTERGRUND method atlas-right",
                "click tab_construction pos 819 219",
            ],
        )
        occ = "occupancy public ULUU LLLU LLLL LLLL LLLL LLLL LLLL L---"
        write_lines(
            con,
            [
                "anno1503-ui-construction 1",
                "resolution 1024 768",
                "open_key B",
                "leave_key ESC",
                "grid cols 4 rows 8 cell_px 52",
                "grid_click_x 833 885 937 989",
                "grid_click_y 330 382 432 484 536 588 640 692",
                "category public index 1 y 325 banner_key public tooltip_section - shot r2",
                "category roads index 2 y 373 banner_key roads tooltip_section - shot r2",
                "category farms index 3 y 418 banner_key farms tooltip_section - shot r2",
                "category workshops index 4 y 463 banner_key workshops tooltip_section - shot r2",
                "category coastal index 5 y 508 banner_key coastal tooltip_section - shot r2",
                "category mines index 6 y 553 banner_key mines tooltip_section - shot r2",
                "category military index 7 y 598 banner_key military tooltip_section - shot r2",
                "category ornamental index 8 y 643 banner_key ornamental tooltip_section - shot r2",
                "cell public 0 0 id 501 state unlocked shot r2_info_pioneer_house",
                "cell public 2 0 id 1301 state unlocked shot r2_info_market_1301",
                occ,
                "occupancy roads UL-- LLL- L--- ---- ---- ---- ---- ----",
                "occupancy farms UULL UUUU LLLL LLLL LLLL LLLL LLLL LLL-",
                "occupancy workshops U--- ULLL LL-- LLL- LLL- LLL- ---- ----",
                "occupancy coastal ULL- UL-- LL-- ---- ---- ---- ---- ----",
                "occupancy mines LLL- LLL- LL-- LL-- LL-- ---- ---- ----",
                "occupancy military LLL- LLLL LLL- LLLL LLL- ---- ---- ----",
                "occupancy ornamental LLLL LLLL LLLL LLLL LLLL LLLL LLLL LL--",
            ],
        )
        parsed = parse_hud(hud)
        check(parsed["resolution"] == (1024, 768), "hud resolution")
        check(parsed["elements"][0]["name"] == "statusbar", "hud element")
        check(parsed["elements"][0]["pos"] == (0, 733), "hud pos")
        parsed_c = parse_construction(con)
        check(parsed_c["cells"][0]["id"] == "501", "cell id")
        check(parsed_c["occupancy"]["public"][0] == "ULUU", "occupancy")
        errs = validate_layout(td)
        check(errs == [], f"validate clean, got {errs}")
        # occupancy vs unlocked mismatch
        write_lines(
            os.path.join(td, "construction.txt"),
            [
                "anno1503-ui-construction 1",
                "resolution 1024 768",
                "grid_click_x 833 885 937 989",
                "grid_click_y 330 382 432 484 536 588 640 692",
                "category public index 1 y 325 banner_key public tooltip_section - shot r2",
                "category roads index 2 y 373 banner_key roads tooltip_section - shot r2",
                "category farms index 3 y 418 banner_key farms tooltip_section - shot r2",
                "category workshops index 4 y 463 banner_key workshops tooltip_section - shot r2",
                "category coastal index 5 y 508 banner_key coastal tooltip_section - shot r2",
                "category mines index 6 y 553 banner_key mines tooltip_section - shot r2",
                "category military index 7 y 598 banner_key military tooltip_section - shot r2",
                "category ornamental index 8 y 643 banner_key ornamental tooltip_section - shot r2",
                "cell public 1 0 id - state unlocked shot x",
                occ,
                "occupancy roads UL-- LLL- L--- ---- ---- ---- ---- ----",
                "occupancy farms UULL UUUU LLLL LLLL LLLL LLLL LLLL LLL-",
                "occupancy workshops U--- ULLL LL-- LLL- LLL- LLL- ---- ----",
                "occupancy coastal ULL- UL-- LL-- ---- ---- ---- ---- ----",
                "occupancy mines LLL- LLL- LL-- LL-- LL-- ---- ---- ----",
                "occupancy military LLL- LLLL LLL- LLLL LLL- ---- ---- ----",
                "occupancy ornamental LLLL LLLL LLLL LLLL LLLL LLLL LLLL LL--",
            ],
        )
        errs = validate_layout(td)
        check(any("unlocked but occupancy" in e for e in errs), f"mismatch not caught: {errs}")
        bad = os.path.join(td, "bad.txt")
        with open(bad, "wb") as fh:
            fh.write(b"anno1503-ui-hud 1\n\xff\n")
        try:
            layout_lines(bad)
            check(False, "non-ascii must raise")
        except LayoutError:
            n_ok += 1
        mock = mock_lines(td)
        check(mock[0] == "anno1503-ui-mock 1", "mock header")
        check(any(l.startswith("rect statusbar 0 733 746 35") for l in mock), "mock statusbar")
        check(any("candidate statusbar_fenster 0 725 780 43" in l for l in mock), "mock fenster")
        try:
            from PIL import Image
        except ImportError:
            print("self-test skip compare (no Pillow)")
        else:
            im = Image.new("RGB", (1024, 768), (25, 86, 136))
            for y in range(733, 768):
                for x in range(0, 746):
                    im.putpixel((x, y), (214, 210, 206))
            for y in range(0, 733):
                for x in range(779, 1024):
                    im.putpixel((x, y), (140, 120, 100))
            png = os.path.join(td, "hud.png")
            im.save(png)
            write_lines(
                hud,
                [
                    "anno1503-ui-hud 1",
                    "resolution 1024 768",
                    "element statusbar pos 0 733 size 746 35 access STATUSBAR method pixel-row",
                    "element panel pos 779 0 size 245 733 access MAINUNTERGRUND method atlas-right",
                ],
            )
            cerr = compare_shot(png, td)
            check(cerr == [], f"compare clean, got {cerr}")
            for yy in range(733, 738):
                im.putpixel((20, yy), (25, 86, 136))
            im.save(png)
            cerr = compare_shot(png, td)
            check(any("bar_top" in e for e in cerr), f"bar_top mismatch not caught: {cerr}")
        check(control_byte_errors(b"ok\n", "x") == [], "lf ok")
        check(control_byte_errors(b"ok\r\n", "x") == [], "crlf ok")
        check(len(control_byte_errors(b"ok" + bytes([9]) + b"no", "x")) == 1, "tab refused")
        check(
            markdown_table_errors("| a | b |\n|---|---|\n| 1 | 2 |\n", "t") == [],
            "table ok",
        )
        check(
            markdown_table_errors("| a | b |\n|---|---|\n| 1 |\n", "t") != [],
            "table bad",
        )
        repo_errs = check_ui1_repo(REPO)
        check(repo_errs == [], "ui1 repo files: " + "; ".join(repo_errs[:8]))
    print(f"self-test ok checks={n_ok}")
    return 0


def _normalize_argv(argv: list[str] | None) -> list[str] | None:
    if argv is None:
        argv = sys.argv[1:]
        owned = True
    else:
        owned = False
        argv = list(argv)
    aliases = {
        "--self-test": "self-test",
        "--validate": "validate",
        "--mock": "mock",
        "--compare": "compare",
    }
    if argv and argv[0] in aliases:
        argv = [aliases[argv[0]]] + argv[1:]
    return argv if owned or argv is not None else None


def main(argv: list[str] | None = None) -> int:
    argv = _normalize_argv(argv)
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    w = sub.add_parser("write-inventory")
    w.add_argument("--game-dir", required=True)
    w.add_argument("--out", required=True)
    s = sub.add_parser("summary")
    s.add_argument("--game-dir", required=True)
    sub.add_parser("self-test")
    cf = sub.add_parser("check-files")
    cf.add_argument("--root", default=REPO)
    v = sub.add_parser("validate")
    v.add_argument("--layout", default=os.path.join(REPO, "data", "ui"))
    v.add_argument("--game-dir")
    m = sub.add_parser("mock")
    m.add_argument("--layout", default=os.path.join(REPO, "data", "ui"))
    c = sub.add_parser("compare")
    c.add_argument("--layout", default=os.path.join(REPO, "data", "ui"))
    c.add_argument("--shot", required=True)
    args = p.parse_args(argv)
    if args.cmd == "write-inventory":
        info = write_inventory(args.game_dir, args.out)
        print(
            "wrote {out} dat={d} inc_files={n} inc_records={r} empty_inc={e} eight_pairs={b}".format(
                out=info["out"],
                d=len(info["dat"]),
                n=info["inc_files"],
                r=info["inc_records"],
                e=info["inc_empty"],
                b=info["eight"]["both"],
            )
        )
        return 0
    if args.cmd == "summary":
        print_summary(args.game_dir)
        return 0
    if args.cmd == "self-test":
        return self_test()
    if args.cmd == "check-files":
        errs = check_ui1_repo(args.root)
        if errs:
            for e in errs:
                print("check-files:", e, file=sys.stderr)
            return 1
        print(f"check-files ok {args.root}")
        return 0
    if args.cmd == "validate":
        errs = validate_layout(args.layout)
        if errs:
            for e in errs:
                print("validate:", e, file=sys.stderr)
            return 1
        print(f"validate ok {args.layout}")
        return 0
    if args.cmd == "mock":
        for line in mock_lines(args.layout):
            print(line)
        return 0
    if args.cmd == "compare":
        errs = compare_shot(args.shot, args.layout)
        if errs:
            for e in errs:
                print("compare:", e, file=sys.stderr)
            return 1
        print(f"compare ok {args.shot}")
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
