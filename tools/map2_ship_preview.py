"""Schematic previews of a ship-start manifest. Not an engine view.

Land is HEIGHT >= 16. The pale outline is the island rectangle. Sea spawns
are ships, not Kontors. A choice line joins a spawn to the water cell of one
validated site. It is a straight relationship to that cell, not the sailed
path and not a line to the island centre. The site marker is the 4x4 origin.

  python3 -I tools/map2_ship_preview.py <manifest> --game-dir <dir> --out-dir <dir>
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixed_map  # noqa: E402
import map2_manifest as mf  # noqa: E402
import map2_preview as prev  # noqa: E402
import map2_shape as shape  # noqa: E402
import map2_ship as ship  # noqa: E402

SEA = (18, 48, 82)
RECT = (210, 205, 180)
INK = (255, 255, 255)
EXTRA = {
    '6': ('111', '100', '111', '101', '111'),
    '7': ('111', '001', '001', '010', '010'),
    '8': ('111', '101', '111', '101', '111'),
    '9': ('111', '101', '111', '001', '111'),
}


def glyph(ch):
    return EXTRA.get(ch, prev.GLYPH.get(ch, prev.GLYPH[' ']))


def text(rgb, width, height, x, y, message, colour, scale=2):
    cursor = x
    for ch in message:
        rows = glyph(ch)
        for gy, row in enumerate(rows):
            for gx, bit in enumerate(row):
                if bit != '1':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        prev.put(rgb, width, height, cursor + gx * scale + sx, y + gy * scale + sy, colour)
        cursor += (3 + 1) * scale


def outline(rgb, width, height, x, y, w, h, project, colour):
    corners = (project(x, y), project(x + w - 1, y), project(x + w - 1, y + h - 1), project(x, y + h - 1))
    for a, b in zip(corners, corners[1:] + corners[:1]):
        prev.line(rgb, width, height, a[0], a[1], b[0], b[1], colour)


def paint(rgb, width, height, doc, catalog, project):
    for island in doc['islands']:
        template = catalog[island['key']]
        outline(rgb, width, height, island['x'], island['y'], template['w'], template['h'], project, RECT)
        colour = fixed_map._CLIMATE_RGB.get(template['climate'], (255, 0, 255))
        grid = template['height']
        tw, th = template['w'], template['h']
        for ty in range(th):
            row = ty * tw
            for tx in range(tw):
                if grid[row + tx] < shape.LAND_HEIGHT:
                    continue
                px, py = project(island['x'] + tx, island['y'] + ty)
                prev.put(rgb, width, height, px, py, colour)
        if island['id'] < 10:
            px, py = project(island['x'] + 8, island['y'] + 8)
            text(rgb, width, height, px, py, str(island['id']), INK, 2)


def spawns(rgb, width, height, doc, project):
    for spawn in doc['spawns']:
        tile_x, tile_y = spawn['x'] // mf.MILLI, spawn['y'] // mf.MILLI
        px, py = project(tile_x, tile_y)
        colour = prev.SEAT[spawn['seat']]
        for dy in range(-4, 5):
            for dx in range(-4, 5):
                prev.put(rgb, width, height, px + dx, py + dy, colour)
        text(rgb, width, height, px + 8, py - 6, str(spawn['seat']), colour, 2)


def choices(rgb, width, height, doc, catalog, report, project):
    by_id = {island['id']: island for island in doc['islands']}
    for item in report.get('witness_detail') or ():
        colour = prev.SEAT[item['seat']]
        sx, sy = item['spawn']
        wx, wy = item['water']
        x0, y0 = project(sx, sy)
        x1, y1 = project(wx, wy)
        prev.line(rgb, width, height, x0, y0, x1, y1, colour)
        ox, oy = item['site']
        for dy in range(4):
            for dx in range(4):
                px, py = project(ox + dx, oy + dy)
                prev.put(rgb, width, height, px, py, colour)
    for island_id in report['shares']:
        if island_id >= 10:
            continue
        island = by_id[island_id]
        px, py = project(island['x'] + 24, island['y'] + 36)
        text(rgb, width, height, px, py, 'S', INK, 2)


def legend(rgb, width, height, x0, overlay):
    text(rgb, width, height, x0, 8, 'CLIMATE', INK, 2)
    for index, name in enumerate(prev.NAMES):
        y0 = 36 + index * 28
        colour = fixed_map._CLIMATE_RGB[index]
        for y in range(y0, y0 + 14):
            for x in range(x0, x0 + 18):
                prev.put(rgb, width, height, x, y, colour)
        text(rgb, width, height, x0 + 24, y0, name, INK, 2)
    y = 36 + 6 * 28 + 12
    text(rgb, width, height, x0, y, 'SHIP', INK, 2)
    for seat, colour in enumerate(prev.SEAT):
        yy = y + 24 + seat * 22
        for row in range(yy, yy + 12):
            for col in range(x0, x0 + 18):
                prev.put(rgb, width, height, col, row, colour)
        text(rgb, width, height, x0 + 24, yy - 2, str(seat), colour, 2)
    y = y + 24 + 5 * 22 + 16
    text(rgb, width, height, x0, y, 'RECT', RECT, 2)
    text(rgb, width, height, x0, y + 22, 'LAND', INK, 2)
    text(rgb, width, height, x0, y + 44, '1 TILE', INK, 2)
    for x in range(x0, x0 + 100):
        prev.put(rgb, width, height, x, y + 70, INK)
        prev.put(rgb, width, height, x, y + 71, INK)
    text(rgb, width, height, x0, y + 78, '100', INK, 2)
    if overlay:
        text(rgb, width, height, x0, y + 110, 'LINE TO SEA', INK, 2)
        text(rgb, width, height, x0, y + 132, 'SITE', INK, 2)
        text(rgb, width, height, x0, y + 154, 'S SHARE', INK, 2)
        text(rgb, width, height, x0, y + 176, 'NO CLAIM', INK, 2)


def render(doc, catalog, report, out_dir, stem):
    edge = doc['edge'][0]
    legend_w = 240
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    written = []

    def frame(width, height, project, overlay, label):
        rgb = prev.canvas(width, height, SEA)
        paint(rgb, width, height, doc, catalog, project)
        if overlay:
            choices(rgb, width, height, doc, catalog, report, project)
        spawns(rgb, width, height, doc, project)
        legend(rgb, width, height, width - legend_w + 12, overlay)
        text(rgb, width, height, width - legend_w + 12, height - 28, label, INK, 2)
        return rgb

    top_w, top_h = edge + legend_w, edge
    top = frame(top_w, top_h, lambda x, y: (x, y), False, 'TOP')
    path = out / f'{stem}-top.png'
    path.write_bytes(fixed_map._png(top_w, top_h, top))
    written.append(path)
    top_c = frame(top_w, top_h, lambda x, y: (x, y), True, 'TOP CHOICES')
    path = out / f'{stem}-top-choices.png'
    path.write_bytes(fixed_map._png(top_w, top_h, top_c))
    written.append(path)

    iso_w, iso_h = edge * 2 + legend_w, edge + 48

    def project(x, y):
        return (x - y + edge, (x + y) // 2 + 16)

    iso = frame(iso_w, iso_h, project, False, 'ISO')
    path = out / f'{stem}-iso.png'
    path.write_bytes(fixed_map._png(iso_w, iso_h, iso))
    written.append(path)
    return written


def main(argv):
    try:
        os.nice(10)
    except OSError:
        pass
    if len(argv) != 6 or argv[2] != '--game-dir' or argv[4] != '--out-dir':
        print('usage: map2_ship_preview.py <manifest> --game-dir <dir> --out-dir <dir>', file=sys.stderr)
        return 2
    game, out_dir = argv[3], argv[5]
    doc, report, catalog = ship.load_and_check(argv[1], game)
    for path in render(doc, catalog, report, out_dir, doc['name']):
        print(path)
    sys.stdout.write(ship.format_report(report))
    return 0 if not report['problems'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
