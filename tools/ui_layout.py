"""UI.1 layout oracle. Spec: docs/re/ui.md, data/ui/README.md.

  python -I tools/ui_layout.py write-inventory --game-dir DIR --out data/ui
  python -I tools/ui_layout.py summary --game-dir DIR

Derived facts only. Does not copy original pixels or UI strings.
Stdlib plus tools/text.py and tools/tex.py.
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


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    w = sub.add_parser("write-inventory")
    w.add_argument("--game-dir", required=True)
    w.add_argument("--out", required=True)
    s = sub.add_parser("summary")
    s.add_argument("--game-dir", required=True)
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
    return 2


if __name__ == "__main__":
    sys.exit(main())
