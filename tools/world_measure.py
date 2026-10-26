"""Read-only measurements for the world-generator spec (docs/sim/world-scaling.md).

Prints counts and coordinates. Writes nothing.

  world_measure.py inventory <Islands dir>
  world_measure.py world <file.sze>

Evidence: docs/re/annoscene.md §2–§5, docs/formats/sze.md, docs/formats/insel.md.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import insel  # noqa: E402
import sze  # noqa: E402

# First match wins (D AnnoScene 0x10009f38–0x10009f73, table 0x1003b328).
PREFIXES = ("PO_", "T_", "N_", "PR_", "D_", "S_", "W_")
CLIMATE = ("po", "t", "n", "pr", "d", "s", "w")

# Competing parties (PLAN §0.6, OQ-100): human 8 and 11, computer 12.
COMPETING = {8, 11, 12}
KIND = {
    2: "neutral",
    8: "human",
    11: "human",
    12: "computer",
    14: "native",
    15: "pirate",
    16: "venetian",
    17: "god",
}

# ROHSTOFF bytes 0..9 (D 0x1000a076–0x1000a140). ERZE bytes (D 0x1000a19f).
RAW_KINDS = (1, 2, 4, 5, 6, 7, 8, 9, 10, 11)
ORE_KINDS = (0, 1, 2, 3, 5, 6)

# Climate admission (D 0x10009010, 0x100090b0). Kind bit in the mask.
RAW_MASK = (0x18000, 0xF009, 0xF949, 0xF06B, 0xF6B9, 0xF04D, 0xF04D)
ORE_MASK = (0x00, 0x05, 0x25, 0x01, 0x03, 0x0B, 0x0B)


def climate_of(file_name):
    upper = file_name.upper()
    for i, pre in enumerate(PREFIXES):
        if upper.startswith(pre):
            return i
    return None


def cstr(buf):
    end = buf.find(b"\0")
    raw = buf if end < 0 else buf[:end]
    return raw.decode("latin-1")


def pct10(part, whole):
    """Tenths of a percent, half up. 0 when whole is 0."""
    if whole <= 0:
        return 0
    return (part * 1000 + whole // 2) // whole


def fmt_pct(tenths):
    return f"{tenths // 10}.{tenths % 10}%"


def median_low(values):
    if not values:
        return None
    xs = sorted(values)
    return xs[(len(xs) - 1) // 2]


def ore_kinds_for(climate):
    """Ore kinds 0..6 the climate admits (D 0x100090b0). Kind 6 tests bit 0."""
    if not isinstance(climate, int) or climate < 0 or climate >= len(ORE_MASK):
        return []
    mask = ORE_MASK[climate] & 0xFF
    out = []
    for kind in range(7):
        bit = 0 if kind == 6 else kind
        if mask & (1 << bit):
            out.append(kind)
    return out


def raw_kinds_for(climate):
    if not isinstance(climate, int) or climate < 0 or climate >= len(RAW_MASK):
        return []
    return [kind for kind in range(20) if RAW_MASK[climate] & (1 << kind)]


def point_rect(px, py, x, y, w, h):
    """Distance from a point to the tile rectangle [x, x+w] x [y, y+h].

    An island head covers tiles x .. x+w-1, and a tile column i covers [i, i+1].
    """
    cx = min(max(px, x), x + w)
    cy = min(max(py, y), y + h)
    return math.hypot(px - cx, py - cy)


def tiles_overlap(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def count_height(isl):
    """Tiles with HEIGHT > 0. HEIGHT is a block grid (insel.md)."""
    try:
        grid = insel.grid(isl, "HEIGHT", 1)
    except insel.Bad:
        return None
    return sum(1 for v in grid if v > 0)


def count_fertile(isl):
    """Tiles with FRUCHTBAR > 0. The record is a raw width*height grid, not blocks."""
    for name, payload in isl["records"]:
        if name != "FRUCHTBAR":
            continue
        if len(payload) != isl["width"] * isl["height"]:
            return None
        return sum(1 for b in payload if b > 0)
    return None


def inventory(directory):
    paths = []
    for dirpath, _dirs, files in os.walk(directory):
        for name in files:
            if name.lower().endswith(".scp"):
                paths.append(os.path.join(dirpath, name))
    paths.sort()
    print(f"inventory files={len(paths)}")

    # key (climate index or -1, w, h) -> lists
    groups = {}
    bad = 0
    prefix_mismatch = 0
    name_size_mismatch = 0
    for path in paths:
        file_name = os.path.basename(path)
        try:
            isl = insel.islands(open(path, "rb").read())
        except (insel.Bad, OSError) as exc:
            bad += 1
            print(f"BAD file={file_name} reason={exc.__class__.__name__}")
            continue
        if len(isl) != 1:
            bad += 1
            print(f"BAD file={file_name} islands={len(isl)}")
            continue
        one = isl[0]
        info = b""
        for rec_name, payload in one["records"]:
            if rec_name == "INFO":
                info = payload
                break
        info8 = info[8] if len(info) > 8 else None
        info9 = info[9] if len(info) > 9 else None
        info0 = info[0] if info else None
        info2 = info[2] if len(info) > 2 else None
        upper = file_name.upper()
        pirat = "PIRAT" in upper
        dummy = upper.endswith("DUMMY.SCP")
        prefix = climate_of(file_name)
        if prefix is not None and info0 is not None and prefix != info0:
            prefix_mismatch += 1
        # File-name size token, e.g. 080X080, against the head (insel.md).
        token = upper.split("_")
        if len(token) >= 2 and "X" in token[1]:
            dims = token[1].split("X")
            if len(dims) == 2 and dims[0].isdigit() and dims[1].isdigit():
                if int(dims[0]) != one["width"] or int(dims[1]) != one["height"]:
                    name_size_mismatch += 1
        height_land = count_height(one)
        fertile = count_fertile(one)
        key = (prefix if prefix is not None else -1, one["width"], one["height"])
        groups.setdefault(key, []).append(
            {
                "pirat": pirat,
                "dummy": dummy,
                "info8": info8,
                "info9": info9,
                "info2": info2,
                "height_land": height_land,
                "fertile": fertile,
                "area": one["width"] * one["height"],
            }
        )

    print(
        f"summary bad={bad} prefix_info_mismatch={prefix_mismatch} "
        f"name_size_mismatch={name_size_mismatch}"
    )
    keys = sorted(groups, key=lambda k: (k[0], k[1], k[2]))
    for climate, w, h in keys:
        rows = groups[(climate, w, h)]
        label = CLIMATE[climate] if 0 <= climate < len(CLIMATE) else "none"
        scan = [r for r in rows if not r["pirat"] and not r["dummy"]]
        eligible = [
            r
            for r in scan
            if r["info8"] == 0 and r["info9"] == 0
        ]
        info8 = sum(1 for r in rows if r["info8"] not in (0, None))
        pirat = sum(1 for r in rows if r["pirat"])
        dummy = sum(1 for r in rows if r["dummy"])
        special = sum(1 for r in eligible if r["info2"] not in (0, None))
        land = [r["height_land"] for r in eligible if r["height_land"] is not None]
        fert = [r["fertile"] for r in eligible if r["fertile"] is not None]
        area = w * h

        def share(values):
            if not values:
                return "na"
            low = median_low(values)
            return (
                f"min={values and min(values)} med={low} max={max(values)} "
                f"med_pct={fmt_pct(pct10(low, area))}"
            )

        print(
            f"group climate={label} size={w}x{h} files={len(rows)} "
            f"pirat={pirat} dummy={dummy} info8={info8} "
            f"scan={len(scan)} eligible={len(eligible)} special={special} "
            f"height_gt0[{share(land)}] fruchtbar_gt0[{share(fert)}]"
        )


def load_players(kids):
    by_index = {}
    for name, payload in kids:
        if name != "SPIELER" or len(payload) < 8:
            continue
        flag, index = struct.unpack_from("<II", payload, 0)
        recs = sze.tagged(payload[8:])
        slot = by_index.setdefault(index, {"flag": flag, "recs": []})
        slot["recs"].extend(recs)
    return by_index


def rects_of(kids, buf_islands):
    links = []
    for name, payload in kids:
        if name != "INSELLNK":
            continue
        if len(payload) < 13 or not payload.endswith(b"\0"):
            continue
        x, y, w, h, number = struct.unpack_from("<4HI", payload, 0)
        path = cstr(payload[12:])
        file_name = path.split("\\")[-1]
        links.append(
            {
                "kind": "link",
                "x": x,
                "y": y,
                "w": w,
                "h": h,
                "number": number,
                "climate": climate_of(file_name),
            }
        )
    islands = []
    for one in buf_islands:
        info0 = None
        for rec_name, payload in one["records"]:
            if rec_name == "INFO" and payload:
                info0 = payload[0]
                break
        islands.append(
            {
                "kind": "insel5",
                "x": one["x"],
                "y": one["y"],
                "w": one["width"],
                "h": one["height"],
                "number": one["number"],
                "climate": info0,
            }
        )
    return links, islands


def on_rect(px, py, rect):
    return rect["x"] <= px < rect["x"] + rect["w"] and rect["y"] <= py < rect["y"] + rect["h"]


def world(path):
    data = open(path, "rb").read()
    edition, kids = sze.scene(data)
    islands = insel.islands(data)
    head = next((p for n, p in kids if n == "HEAD"), b"")
    width = struct.unpack_from("<I", head, 0)[0] if len(head) >= 8 else 0
    height = struct.unpack_from("<I", head, 4)[0] if len(head) >= 8 else 0
    prop0 = head[8] if len(head) > 8 else None
    auftrag = next((cstr(p) for n, p in kids if n == "AUFTRAGTEXT"), "")
    limits = next((p for n, p in kids if n == "LIMITS"), b"")
    ship_cap = struct.unpack_from("<H", limits, 0)[0] if len(limits) >= 2 else None
    soldier_cap = struct.unpack_from("<H", limits, 2)[0] if len(limits) >= 4 else None
    roh = next((p for n, p in kids if n == "ROHSTOFF"), b"")
    erze = next((p for n, p in kids if n == "ERZE"), b"")
    schatz = next((p for n, p in kids if n == "SCHATZ"), b"")
    treasures = struct.unpack_from("<H", schatz, 0)[0] if len(schatz) >= 2 else None

    print(f"world file={os.path.basename(path)} edition={edition}")
    print(
        f"size width={width} height={height} area={width * height} "
        f"prop0={prop0} auftrag={auftrag} limits={ship_cap}/{soldier_cap}"
    )
    raw_parts = []
    for i, kind in enumerate(RAW_KINDS):
        raw_parts.append(f"k{kind}={roh[i] if i < len(roh) else 'na'}")
    ore_parts = []
    for i, kind in enumerate(ORE_KINDS):
        ore_parts.append(f"k{kind}={erze[i] if i < len(erze) else 'na'}")
    special = erze[6] if len(erze) > 6 else None
    print(f"raw {' '.join(raw_parts)} sum={sum(roh[:10]) if len(roh) >= 10 else 'na'}")
    print(f"ore {' '.join(ore_parts)} special={special} treasures={treasures}")

    links, placed = rects_of(kids, islands)
    print(f"islands links={len(links)} insel5={len(placed)}")
    berg = berg_bytes = occur = 0
    for one in islands:
        for rec_name, payload in one["records"]:
            if rec_name == "BERGWERK2":
                berg += 1
                berg_bytes += len(payload)
            elif rec_name == "OCCUR":
                occur += 1
    print(f"island_records bergwerk2={berg} bytes={berg_bytes} occur={occur}")
    # Distances and the land share use every island rectangle. The size mix below
    # is printed twice when a file has both, because a link and a placed island
    # are different sources (annoscene §3: a non-dummy path is placed as itself).
    bodies = links + placed
    area = sum(r["w"] * r["h"] for r in bodies)
    outside = sum(
        1
        for r in bodies
        if r["x"] + r["w"] > width or r["y"] + r["h"] > height or r["x"] < 0 or r["y"] < 0
    )
    boxes = [(r["x"], r["y"], r["w"], r["h"]) for r in bodies]
    overlaps = 0
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            if tiles_overlap(boxes[i], boxes[j]):
                overlaps += 1
    cross = 0
    for link in links:
        lb = (link["x"], link["y"], link["w"], link["h"])
        for isle in placed:
            if tiles_overlap(lb, (isle["x"], isle["y"], isle["w"], isle["h"])):
                cross += 1
    unaligned = sum(
        1
        for r in bodies
        if r["x"] % 8 or r["y"] % 8 or r["w"] % 8 or r["h"] % 8
    )
    print(
        f"land tiles={area} share={fmt_pct(pct10(area, width * height))} "
        f"outside={outside} overlap_pairs={overlaps} link_insel5_overlaps={cross} "
        f"not_multiple_of_8={unaligned}"
    )
    for isle in placed:
        label = (
            CLIMATE[isle["climate"]]
            if isinstance(isle["climate"], int) and 0 <= isle["climate"] < len(CLIMATE)
            else isle["climate"]
        )
        print(
            f"insel5 number={isle['number']} climate={label} "
            f"x={isle['x']} y={isle['y']} size={isle['w']}x{isle['h']}"
        )

    by_size = {}
    by_climate = {}
    for r in bodies:
        by_size.setdefault((r["w"], r["h"]), []).append(r)
        by_climate.setdefault(r["climate"], []).append(r)
    for (w, h), rows in sorted(by_size.items()):
        tiles = sum(r["w"] * r["h"] for r in rows)
        n_link = sum(1 for r in rows if r["kind"] == "link")
        n_isle = sum(1 for r in rows if r["kind"] == "insel5")
        print(
            f"size {w}x{h} count={len(rows)} links={n_link} insel5={n_isle} "
            f"tiles={tiles} share={fmt_pct(pct10(tiles, area))}"
        )
    for climate, rows in sorted(by_climate.items(), key=lambda kv: min(r["y"] for r in kv[1])):
        label = CLIMATE[climate] if isinstance(climate, int) and 0 <= climate < len(CLIMATE) else climate
        ys = [r["y"] for r in rows]
        ye = [r["y"] + r["h"] for r in rows]
        tiles = sum(r["w"] * r["h"] for r in rows)
        print(
            f"band climate={label} count={len(rows)} ymin={min(ys)} ymax_end={max(ye)} "
            f"tiles={tiles}"
        )

    players = load_players(kids)
    starts = []
    kind_counts = {}
    for index in sorted(players):
        recs = players[index]["recs"]
        head_rec = next((p for n, _long, p in recs if n == "HEAD"), None)
        kind = struct.unpack_from("<H", head_rec, 0)[0] if head_rec and len(head_rec) >= 2 else None
        money = struct.unpack_from("<I", head_rec, 4)[0] if head_rec and len(head_rec) >= 8 else None
        # Index 31 is skipped by every game-start loop (D AnnoFrame 0x10029100).
        label = "neutral" if index == 31 else KIND.get(kind, "other")
        kind_counts[label] = kind_counts.get(label, 0) + 1
        ships = []
        cities = []
        soldiers = 0
        comp_typ = None
        comp_kampf = None
        for n, _long, payload in recs:
            if n == "SCHIFF" and len(payload) >= 16:
                _uid, fig, _owner, attr, _res, xb, yb = struct.unpack_from("<HHBBHII", payload, 0)
                x = struct.unpack("<f", struct.pack("<I", xb))[0]
                y = struct.unpack("<f", struct.pack("<I", yb))[0]
                ships.append((fig, attr, x, y))
            elif n == "SOLDAT":
                soldiers += 1
            elif n == "STADT2" and len(payload) >= 6:
                num, _zero, cx, cy = struct.unpack_from("<BBHH", payload, 0)
                cities.append((num, cx, cy))
            elif n == "COMP" and len(payload) > 8:
                for part in payload[8:].split(b"\0"):
                    if part.startswith(b"TYP="):
                        comp_typ = part.decode("latin-1")
                    elif part.startswith(b"KAMPFTYP="):
                        comp_kampf = part.decode("latin-1")
        ship_text = ",".join(f"{fig}@{x:.2f},{y:.2f}" for fig, _a, x, y in ships) or "-"
        typ_text = ""
        if comp_typ:
            typ_text += f" {comp_typ}"
        if comp_kampf:
            typ_text += f" {comp_kampf}"
        print(
            f"player index={index} kind={kind} ({label}) money={money} "
            f"ships={len(ships)} [{ship_text}] soldiers={soldiers} cities={len(cities)}{typ_text}"
        )
        if index != 31 and kind in COMPETING and len(ships) == 1:
            _fig, _attr, x, y = ships[0]
            starts.append({"index": index, "kind": kind, "x": x, "y": y, "cities": cities})
        elif index != 31 and kind in COMPETING:
            print(f"start_unclear index={index} ships={len(ships)} cities={len(cities)}")

    counts = " ".join(f"{k}={v}" for k, v in sorted(kind_counts.items()))
    competing_n = kind_counts.get("human", 0) + kind_counts.get("computer", 0)
    print(f"parties {counts} competing={competing_n} one_ship={len(starts)}")

    if len(starts) >= 2:
        xs = [s["x"] for s in starts]
        ys = [s["y"] for s in starts]
        pairs = []
        for i, a in enumerate(starts):
            for b in starts[i + 1 :]:
                pairs.append(math.hypot(a["x"] - b["x"], a["y"] - b["y"]))
        print(
            f"cluster min_pair={min(pairs):.2f} max_pair={max(pairs):.2f} "
            f"bbox={max(xs) - min(xs):.2f}x{max(ys) - min(ys):.2f}"
        )

    if len(starts) != competing_n or not starts or not bodies:
        print("fairness skipped")
        return

    for s in starts:
        ranked = sorted(
            (
                (
                    point_rect(s["x"], s["y"], r["x"], r["y"], r["w"], r["h"]),
                    r["number"],
                    r["climate"],
                )
                for r in bodies
            )
        )
        dist, number, climate = ranked[0]
        label = CLIMATE[climate] if isinstance(climate, int) and 0 <= climate < len(CLIMATE) else climate
        rivals = [
            math.hypot(s["x"] - o["x"], s["y"] - o["y"]) for o in starts if o["index"] != s["index"]
        ]
        nearest_rival = min(rivals) if rivals else None
        rival_text = f"{nearest_rival:.2f}" if nearest_rival is not None else "na"
        print(
            f"reach index={s['index']} nearest_island={number} climate={label} "
            f"dist={dist:.2f} nearest_rival={rival_text}"
        )

    # Each island to the nearest start. Tie: lowest player index.
    owned = {s["index"]: [] for s in starts}
    for r in bodies:
        best = None
        for s in starts:
            d = point_rect(s["x"], s["y"], r["x"], r["y"], r["w"], r["h"])
            key = (d, s["index"])
            if best is None or key < best[0]:
                best = (key, s["index"])
        owned[best[1]].append(r)
    n = len(starts)
    for s in starts:
        rows = owned[s["index"]]
        tiles = sum(r["w"] * r["h"] for r in rows)
        climates = sorted({r["climate"] for r in rows})
        admitted_raw = set()
        admitted_ore = set()
        for c in climates:
            admitted_raw.update(raw_kinds_for(c))
            admitted_ore.update(ore_kinds_for(c))
        clabels = ",".join(
            CLIMATE[c] if isinstance(c, int) and 0 <= c < len(CLIMATE) else str(c) for c in climates
        )
        print(
            f"share index={s['index']} islands={len(rows)} tiles={tiles} "
            f"of_land={fmt_pct(pct10(tiles, area))} equal={fmt_pct(pct10(1, n))} "
            f"climates={clabels or '-'} raw_admitted={sorted(admitted_raw)} "
            f"ore_admitted={sorted(admitted_ore)}"
        )


def main(argv):
    if len(argv) == 3 and argv[1] == "inventory":
        inventory(argv[2])
        return 0
    if len(argv) == 3 and argv[1] == "world":
        world(argv[2])
        return 0
    print(__doc__.strip(), file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.stdout.reconfigure(newline="\n")
    try:
        sys.exit(main(sys.argv))
    except (sze.Bad, insel.Bad, OSError) as exc:
        print(f"NOPARSE: {exc}", file=sys.stderr)
        sys.exit(1)
