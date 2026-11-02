"""Labelled top-down and isometric previews of a MAP.2 candidate.

  python3 -I tools/map2_preview.py <file> --game-dir <game/app> --out-dir <dir>

Land is HEIGHT >= 16, the same threshold as the profile. The pictures are
derived and stay out of git. Seat-to-south lines are the measured assignment,
straight geometric reach, not a shipping route.
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixed_map  # noqa: E402
import map2_profile as profile  # noqa: E402
import map2_shape as shape  # noqa: E402

SEA = (18, 48, 82)
LINE = (180, 200, 220)
CENTRE = (255, 255, 255)
SEAT = (
    (255, 214, 40),
    (230, 48, 48),
    (245, 245, 245),
    (255, 140, 32),
    (70, 210, 255),
)
# 5x5 glyphs, row-major '1' bits. Enough for the legend and the seat numbers.
GLYPH = {
    '0': ('111', '101', '101', '101', '111'),
    '1': ('010', '110', '010', '010', '111'),
    '2': ('111', '001', '111', '100', '111'),
    '3': ('111', '001', '111', '001', '111'),
    '4': ('101', '101', '111', '001', '001'),
    '5': ('111', '100', '111', '001', '111'),
    'A': ('010', '101', '111', '101', '101'),
    'C': ('111', '100', '100', '100', '111'),
    'D': ('110', '101', '101', '101', '110'),
    'E': ('111', '100', '111', '100', '111'),
    'G': ('111', '100', '101', '101', '111'),
    'H': ('101', '101', '111', '101', '101'),
    'I': ('111', '010', '010', '010', '111'),
    'J': ('111', '001', '001', '101', '111'),
    'L': ('100', '100', '100', '100', '111'),
    'M': ('101', '111', '101', '101', '101'),
    'N': ('110', '101', '101', '101', '101'),
    'O': ('111', '101', '101', '101', '111'),
    'P': ('111', '101', '111', '100', '100'),
    'R': ('111', '101', '111', '110', '101'),
    'S': ('111', '100', '111', '001', '111'),
    'T': ('111', '010', '010', '010', '010'),
    'U': ('101', '101', '101', '101', '111'),
    ' ': ('000', '000', '000', '000', '000'),
}
NAMES = ('POLAR', 'TUNDRA', 'NORD', 'PRAERIE', 'DSCHUNGEL', 'STEPPE')


def nice():
    try:
        os.nice(10)
    except OSError:
        pass


def canvas(width, height, colour):
    return bytearray(bytes(colour) * width * height)


def put(rgb, width, height, x, y, colour):
    if 0 <= x < width and 0 <= y < height:
        i = (y * width + x) * 3
        rgb[i:i + 3] = bytes(colour)


def text(rgb, width, height, x, y, message, colour, scale=2):
    cursor = x
    for ch in message:
        glyph = GLYPH.get(ch, GLYPH[' '])
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit != '1':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        put(rgb, width, height, cursor + gx * scale + sx, y + gy * scale + sy, colour)
        cursor += (3 + 1) * scale


def line(rgb, width, height, x0, y0, x1, y1, colour):
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    x, y = x0, y0
    for _ in range(dx + dy + 1):
        put(rgb, width, height, x, y, colour)
        if x == x1 and y == y1:
            break
        e2 = err * 2
        if e2 > -dy:
            err -= dy
            x += sx
        if e2 < dx:
            err += dx
            y += sy


def paint_land(rgb, width, height, islands, catalog, project):
    for island in islands:
        template = catalog[island['key']]
        colour = fixed_map._CLIMATE_RGB.get(template['climate'], (255, 0, 255))
        grid = template['height']
        tw, th = template['w'], template['h']
        ox, oy = island['x'], island['y']
        for ty in range(th):
            row = ty * tw
            for tx in range(tw):
                if grid[row + tx] < shape.LAND_HEIGHT:
                    continue
                px, py = project(ox + tx, oy + ty)
                put(rgb, width, height, px, py, colour)


def legend(rgb, width, height, x0):
    text(rgb, width, height, x0, 8, 'CLIMATE', CENTRE, 2)
    for index, name in enumerate(NAMES):
        y0 = 36 + index * 28
        colour = fixed_map._CLIMATE_RGB[index]
        for y in range(y0, y0 + 14):
            for x in range(x0, x0 + 18):
                put(rgb, width, height, x, y, colour)
        text(rgb, width, height, x0 + 24, y0, name, CENTRE, 2)
    y = 36 + 6 * 28 + 8
    text(rgb, width, height, x0, y, 'SEAT', CENTRE, 2)
    for seat, colour in enumerate(SEAT):
        yy = y + 24 + seat * 22
        for row in range(yy, yy + 12):
            for col in range(x0, x0 + 18):
                put(rgb, width, height, col, row, colour)
        text(rgb, width, height, x0 + 24, yy - 2, str(seat), colour, 2)
    y = y + 24 + 5 * 22 + 12
    text(rgb, width, height, x0, y, 'LINE', LINE, 2)
    text(rgb, width, height, x0, y + 20, 'CENTRE', CENTRE, 2)


def markers(rgb, width, height, mp, assignment, project, label_scale):
    edge = mp['width']
    mid = edge // 2
    # North/south midline and the map centre. Drawn on top of land.
    for x in range(0, edge, 2):
        px, py = project(x, mid)
        put(rgb, width, height, px, py, LINE)
    cx, cy = project(mid, mid)
    for d in range(-8, 9):
        put(rgb, width, height, cx + d, cy, CENTRE)
        put(rgb, width, height, cx, cy + d, CENTRE)
    nx, ny = project(mid, 24)
    text(rgb, width, height, nx - 8, ny, 'N', CENTRE, label_scale)
    south_slots = assignment['south_slots']
    by_slot = {island['slot']: island for island in mp['islands']}
    for start in mp['starts']:
        seat = start['seat']
        colour = SEAT[seat % 5]
        south = by_slot[south_slots[seat]]
        x0, y0 = project(start['x'] + 2, start['y'] + 2)
        x1, y1 = project(south['x'] + 128, south['y'] + 128)
        line(rgb, width, height, x0, y0, x1, y1, colour)
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                put(rgb, width, height, x0 + dx, y0 + dy, colour)
        text(rgb, width, height, x0 + 6, y0 - 4, str(seat), colour, label_scale)


def render_pair(mp, catalog, report, out_dir, stem):
    edge = mp['width']
    legend_w = 220
    top_w = edge + legend_w
    top_h = edge
    top = canvas(top_w, top_h, SEA)
    paint_land(top, top_w, top_h, mp['islands'], catalog, lambda x, y: (x, y))
    markers(top, top_w, top_h, mp, report['assignment'], lambda x, y: (x, y), 2)
    legend(top, top_w, top_h, edge + 12)
    text(top, top_w, top_h, edge + 12, top_h - 40, 'TOP', CENTRE, 2)

    iso_w = edge * 2 + legend_w
    iso_h = edge + 40
    iso = canvas(iso_w, iso_h, SEA)

    def project(x, y):
        return (x - y + edge, (x + y) // 2 + 8)

    paint_land(iso, iso_w, iso_h, mp['islands'], catalog, project)
    markers(iso, iso_w, iso_h, mp, report['assignment'], project, 2)
    legend(iso, iso_w, iso_h, edge * 2 + 12)
    text(iso, iso_w, iso_h, edge * 2 + 12, iso_h - 40, 'ISO', CENTRE, 2)

    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    top_path = out / f'{stem}-top.png'
    iso_path = out / f'{stem}-iso.png'
    top_path.write_bytes(fixed_map._png(top_w, top_h, top))
    iso_path.write_bytes(fixed_map._png(iso_w, iso_h, iso))
    return top_path, iso_path


def main(argv):
    nice()
    if len(argv) != 6 or argv[2] != '--game-dir' or argv[4] != '--out-dir':
        print('usage: map2_preview.py <file> --game-dir <dir> --out-dir <dir>', file=sys.stderr)
        return 2
    path, game, out_dir = argv[1], argv[3], argv[5]
    data = open(path, 'rb').read()
    mp, err = fixed_map.parse(data)
    if err:
        print(f'error: line {err[0]}: {err[1]}')
        return 1
    keys = {island['key'] for island in mp['islands']}
    catalog = profile.load_templates(game, keys)
    report = profile.validate(mp, catalog, profile.default_profile())
    if report.get('assignment') is None:
        print('no assignment; structural rules failed')
        return 1
    stem = Path(path).stem
    top_path, iso_path = render_pair(mp, catalog, report, out_dir, stem)
    print(top_path)
    print(iso_path)
    print(profile.format_report(report), end='')
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
