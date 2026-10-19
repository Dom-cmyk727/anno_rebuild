#!/usr/bin/env python3
"""QA harness for enhanced river water ("river gaps", owner report 2026-10-19).

Enhanced water must carry water wherever the original draws river water: the
original has no partly dry riverbeds, so every stretch of the painted bed must
be wet from source to mouth. This script runs the matrix of scenes x zoom
groups and measures it; the measuring itself is the C++ tool `river_qa`
(src/tools/river_qa/main.cpp, which documents the definitions: original river
sprites O, enhanced water W, gap = O without W within a few pixels, connected
components of O and of the enhanced river water).

  tools/river_qa.py measure --exe build/msvc/src/tools/river_qa/Release/river_qa.exe \
      --game-dir game/app [--groups 0,1,2] [--climates Dschungel,Nord,...] \
      [--scenarios 0200Single,0201SingleAddOn] [--only-rivers] [--out table.md] [files...]
      one Markdown row per scene and group (islands without river pieces are
      skipped by the tool; scenes without rivers print nothing with
      --only-rivers). Exit code 1 if any gap pixel remains and --max-gap-pct
      (default 0.1) is exceeded in a row.

  tools/river_qa.py shots --exe ... --app build/msvc/src/app/Release/anno1503.exe \
      --game-dir game/app --file Islands/Dschungel/D_256x256_0045_5B.scp \
      --group 1 --rect X,Y,W,H --out DIR
      renders the rectangle (scene pixels at that group) of the original water
      (`anno1503 --png`), the enhanced water, and the gap overlay (original river
      sprites not covered by enhanced water in red, from `river_qa --rect`), and
      writes original | enhanced | overlay side by side (needs Pillow).

The CPU path (`anno1503 --png`, `FrameRenderer`) and the GPU compositor both
read the same per-pixel water field (`Scene::water_field_from`), which is
what `river_qa` measures; the GPU only shades it differently. Compare the two
renderers with `bench_frame --renderer gpu --check` (BENCH_DUMP=<dir> keeps
both images).
"""
import argparse
import os
import re
import subprocess
import sys


def island_files(game_dir, climates):
    out = []
    for c in climates:
        d = os.path.join(game_dir, 'Scenes', 'Islands', c)
        if not os.path.isdir(d):
            continue
        out += sorted(os.path.join(d, f) for f in os.listdir(d)
                      if f.lower().endswith('.scp') and 'dummy' not in f.lower())
    return out


def scenario_files(game_dir, folders):
    out = []
    for c in folders:
        d = os.path.join(game_dir, 'Scenes', c)
        if not os.path.isdir(d):
            continue
        out += sorted(os.path.join(d, f) for f in os.listdir(d) if f.lower().endswith('.sze'))
    return out


def measure(args):
    files = list(args.files)
    if args.climates:
        files += island_files(args.game_dir, args.climates.split(','))
    if args.scenarios:
        files += scenario_files(args.game_dir, args.scenarios.split(','))
    if not files:
        sys.exit('no scene files')
    rows = []
    header = None
    # Batches keep the command line short; one process per batch loads the
    # game data once per file anyway.
    for i in range(0, len(files), 40):
        cmd = [args.exe, args.game_dir, '--groups', args.groups] + files[i:i + 40]
        res = subprocess.run(cmd, capture_output=True, text=True)
        for line in res.stdout.splitlines():
            if line.startswith('| scene'):
                header = header or line
            elif line.startswith('|---'):
                continue
            elif line.startswith('|'):
                rows.append(line)
    bad = 0
    out = [header, '|' + '---|' * header.count('|') if header else '']
    out[1] = '|' + '---|' * (header.count('|') - 1)
    for r in rows:
        cells = [c.strip() for c in r.strip('|').split('|')]
        o_px = int(cells[3])
        if args.only_rivers and o_px == 0:
            continue
        out.append(r)
        # gap % is column 7 of the table
        if float(cells[7]) > args.max_gap_pct:
            bad += 1
    text = '\n'.join(out)
    print(text)
    if args.out:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write(text + '\n')
    return 1 if bad else 0


def shots(args):
    from PIL import Image
    Image.MAX_IMAGE_PIXELS = None
    os.makedirs(args.out, exist_ok=True)
    scene = os.path.join(args.game_dir, 'Scenes', args.file)
    x, y, w, h = [int(v) for v in args.rect.split(',')]
    # anno1503 --png renders around a tile; the rectangle is in scene pixels, so
    # render the whole scene (small scenes only) and crop; use river_qa --rect
    # for the overlay (scene pixels, same origin as scene.bounds).
    stem = os.path.splitext(os.path.basename(scene))[0]
    paths = {}
    for water in ('original', 'enhanced'):
        p = os.path.join(args.out, '%s_g%s_%s.png' % (stem, args.group, water))
        res = subprocess.run([args.app, '--view', scene, '--game-dir', args.game_dir,
                              '--group', str(args.group), '--water', water, '--png', p],
                             capture_output=True, text=True)
        paths[water] = p
    res = subprocess.run([args.exe, args.game_dir, '--groups', str(args.group), '--rect',
                          '%s,%d,%d,%d,%d' % (args.group, x, y, w, h), '--rect-out',
                          os.path.join(args.out, 'mask.png'), scene], capture_output=True, text=True)
    # river_qa prints the scene bounds (the origin of the --png image).
    m = re.search(r'scene bounds x (-?\d+) y (-?\d+)', res.stderr)
    if not m:
        sys.exit('cannot read the scene bounds')
    bx, by = int(m.group(1)), int(m.group(2))
    box = (x - bx, y - by, x - bx + w, y - by + h)
    org = Image.open(paths['original']).convert('RGB').crop(box)
    enh = Image.open(paths['enhanced']).convert('RGB').crop(box)
    mask = Image.open(os.path.join(args.out, 'mask.png')).convert('RGB')
    over = enh.copy()
    px, mp = over.load(), mask.load()
    for j in range(h):
        for i in range(w):
            if mp[i, j][0] == 255:
                px[i, j] = (255, 0, 255)
    sheet = Image.new('RGB', (3 * w + 8, h))
    for k, im in enumerate((org, enh, over)):
        sheet.paste(im, (k * (w + 4), 0))
    out = os.path.join(args.out, '%s_g%s_compare.png' % (stem, args.group))
    sheet.save(out)
    print(out)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    m = sub.add_parser('measure')
    m.add_argument('--exe', required=True)
    m.add_argument('--game-dir', required=True)
    m.add_argument('--groups', default='0,1,2')
    m.add_argument('--climates', default='')
    m.add_argument('--scenarios', default='')
    m.add_argument('--only-rivers', action='store_true')
    m.add_argument('--max-gap-pct', type=float, default=0.1)
    m.add_argument('--out')
    m.add_argument('files', nargs='*')
    s = sub.add_parser('shots')
    s.add_argument('--exe', required=True)
    s.add_argument('--app', required=True)
    s.add_argument('--game-dir', required=True)
    s.add_argument('--file', required=True, help='relative to <game-dir>/Scenes')
    s.add_argument('--group', type=int, required=True)
    s.add_argument('--rect', required=True, help='x,y,w,h in scene pixels at that group')
    s.add_argument('--out', required=True)
    args = ap.parse_args()
    sys.exit(measure(args) if args.cmd == 'measure' else shots(args))


if __name__ == '__main__':
    main()
