#!/usr/bin/env python3
"""Gap patch survey (OQ-068, docs/render/rivers.md): river_qa over every river island
with `--gap-patch 0` and `--gap-patch N`, joined per island and zoom group.

Used by `tools/river_qa.py gap-patch`; the pure functions (row parsing, joining,
regression rules, table) are covered by tools/test_river_qa_gap.py.
"""
import os
import re
import subprocess
import sys

# `river_qa --per-island` row: "| <scene>_g<group>_i<island> (s=<step>) | o | covered | edge | gap
# | extra | o_comps | e_comps | split | lost | gap_comps |".
ISLAND_ROW = re.compile(
    r'^\|\s*(?P<scene>.+)_g(?P<group>\d+)_i(?P<island>\d+) \(s=(?P<s>\d+)\)\s*\|(?P<rest>.*)\|\s*$')
ISLAND_FIELDS = ('o', 'covered', 'edge', 'gap', 'extra', 'o_comps', 'e_comps', 'split', 'lost',
                 'gap_comps')

# Water outside the painted bed counts as a regression when it grows by more than this share of
# the original's river pixels. The patch deepens the trench, so some growth is expected; the
# threshold only marks the islands where it is a lot. It is a survey flag, not a game rule.
EXTRA_GROWTH_FRACTION = 0.25


def parse_island_rows(text):
    """Parse `river_qa --per-island` output into {(scene, group, island): {field: int}}."""
    rows = {}
    for line in text.splitlines():
        m = ISLAND_ROW.match(line)
        if not m:
            continue
        cells = [c.strip() for c in m.group('rest').split('|')]
        if len(cells) != len(ISLAND_FIELDS):
            raise ValueError('unexpected per-island row: ' + line)
        row = {k: int(v) for k, v in zip(ISLAND_FIELDS, cells)}
        row['s'] = int(m.group('s'))
        rows[(m.group('scene'), int(m.group('group')), int(m.group('island')))] = row
    return rows


def regressions(before, after):
    """Reasons why `after` is worse than `before` for one island and group (empty = none)."""
    out = []
    for field, name in (('e_comps', 'components'), ('split', 'split'), ('lost', 'lost'),
                        ('gap', 'gap px')):
        if after[field] > before[field]:
            out.append('%s %d -> %d' % (name, before[field], after[field]))
    if after['extra'] - before['extra'] > EXTRA_GROWTH_FRACTION * max(before['o'], 1):
        out.append('extra px %d -> %d' % (before['extra'], after['extra']))
    return out


def growth(before, after):
    """Water outside the painted bed gained by the patch, as a share of the original river px."""
    return (after['extra'] - before['extra']) / max(before['o'], 1)


def join_runs(before, after):
    """{(scene, island): {group: (before_row, after_row)}} for keys present in both runs."""
    out = {}
    for key in sorted(before):
        if key in after:
            scene, group, island = key
            out.setdefault((scene, island), {})[group] = (before[key], after[key])
    return out


def badness(groups):
    """Sort key, larger = worse: regressions first, then the gap pixels that remain after the
    patch, then the largest growth of water outside the painted bed (share of the original
    river pixels, the cost of the patch), then the components beyond one per original one."""
    reg = sum(len(regressions(b, a)) for b, a in groups.values())
    gap = sum(a['gap'] for b, a in groups.values())
    surplus = sum(max(a['e_comps'] - a['o_comps'], 0) for b, a in groups.values())
    grown = max(growth(b, a) for b, a in groups.values())
    return (reg, gap, round(grown, 3), surplus)


def worst_islands(joined, count):
    """The `count` worst islands by badness; scenes with the same pixel and component counts in
    every group (the same island template in several scenarios, e.g. the Mongolen and Indianer
    variants) count once."""
    out, seen = [], set()
    for key in sorted(joined, key=lambda k: badness(joined[k]), reverse=True):
        sig = tuple((g, b['o'], b['o_comps'], b['e_comps'], b['extra'], a['e_comps'], a['extra'])
                    for g, (b, a) in sorted(joined[key].items()))
        if sig in seen:
            continue
        seen.add(sig)
        out.append(key)
        if len(out) == count:
            break
    return out


def group_ids(joined):
    return sorted({g for per in joined.values() for g in per})


def survey_table(joined):
    """Markdown table, one row per island; per zoom group `before -> after`."""
    groups = group_ids(joined)
    head = ['island']
    for name in ('E comps', 'gap px', 'extra px'):
        head += ['%s g%d' % (name, g) for g in groups]
    head += ['extra growth g%d' % g for g in groups]
    head.append('regression')
    lines = ['| ' + ' | '.join(head) + ' |', '|' + '---|' * len(head)]
    for (scene, island), per in sorted(joined.items()):
        cells = ['%s i%d' % (scene, island)]
        for field in ('e_comps', 'gap', 'extra'):
            for g in groups:
                cells.append('%d -> %d' % (per[g][0][field], per[g][1][field]) if g in per else '')
        for g in groups:
            cells.append('%.1f %%' % (100 * growth(*per[g])) if g in per else '')
        why = ['g%d %s' % (g, '; '.join(regressions(b, a)))
               for g, (b, a) in sorted(per.items()) if regressions(b, a)]
        cells.append(', '.join(why))
        lines.append('| ' + ' | '.join(cells) + ' |')
    return '\n'.join(lines)


def survey_summary(joined, patch, coverage=None):
    """Summary lines: coverage, the cost of the patch (water outside the painted bed) first, then
    the regression count (rules in `regressions`) and the totals per field and group."""
    bad = [k for k, per in joined.items() if any(regressions(b, a) for b, a in per.values())]
    out = []
    if coverage:
        out.append('scenes: tried %(tried)d, loaded %(loaded)d, failed %(failed)d, '
                   'with river pieces %(with-river)d' % coverage)
    out.append('islands with river pieces: %d' % len(joined))
    for g in group_ids(joined):
        per = [p[g] for p in joined.values() if g in p]
        o = sum(b['o'] for b, a in per)
        b_extra = sum(b['extra'] for b, a in per)
        a_extra = sum(a['extra'] for b, a in per)
        top = max((growth(*p[g]), k) for k, p in joined.items() if g in p)
        out.append('g%d water outside the painted bed, share of original river px: %.1f %% -> '
                   '%.1f %% (largest growth %.1f %%, %s i%d)' % (
                       g, 100 * b_extra / max(o, 1), 100 * a_extra / max(o, 1), 100 * top[0],
                       top[1][0], top[1][1]))
    out.append('islands with a regression (patch %s against 0): %d' % (patch, len(bad)))
    for field, name in (('o', 'O px'), ('e_comps', 'E comps'), ('gap', 'gap px'),
                        ('edge', 'edge px'), ('extra', 'extra px'), ('split', 'split'),
                        ('lost', 'lost')):
        for g in group_ids(joined):
            b = sum(per[g][0][field] for per in joined.values() if g in per)
            a = sum(per[g][1][field] for per in joined.values() if g in per)
            out.append('g%d %s total: %d -> %d' % (g, name, b, a))
    return '\n'.join(out)


def save_runs(path, before, after, coverage=None):
    import json
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'before': {'%s|%d|%d' % k: v for k, v in before.items()},
                   'after': {'%s|%d|%d' % k: v for k, v in after.items()},
                   'coverage': coverage}, f)


def load_runs(path):
    import json
    with open(path, encoding='utf-8') as f:
        d = json.load(f)

    def key(k):
        scene, g, i = k.rsplit('|', 2)
        return (scene, int(g), int(i))
    return ({key(k): v for k, v in d['before'].items()},
            {key(k): v for k, v in d['after'].items()}, d.get('coverage'))


def run_per_island(exe, game_dir, files, patch, groups):
    """river_qa --per-island over `files` (a list file: Windows command lines are short).
    Returns (rows, coverage); coverage is the scene counts river_qa prints at the end."""
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as lst:
        lst.write(chr(10).join(files))
    try:
        res = subprocess.run([exe, game_dir, '--groups', groups, '--per-island', '--gap-patch',
                              str(patch), '@' + lst.name], capture_output=True, text=True)
    finally:
        os.unlink(lst.name)
    if res.returncode != 0:
        sys.exit('river_qa failed (%d): %s' % (res.returncode, res.stderr[-2000:]))
    return parse_island_rows(res.stdout), parse_coverage(res.stderr)


COVERAGE = re.compile(r'^scenes: tried (\d+) loaded (\d+) failed (\d+) with-river (\d+)$', re.M)


def parse_coverage(stderr):
    """{'tried', 'loaded', 'failed', 'with-river'} from river_qa's final stderr line."""
    m = COVERAGE.search(stderr)
    if not m:
        raise ValueError('river_qa printed no scene coverage line')
    return dict(zip(('tried', 'loaded', 'failed', 'with-river'), map(int, m.groups())))


def find_scene(game_dir, scene):
    for root, _, names in os.walk(os.path.join(game_dir, 'Scenes', 'Islands')):
        if scene + '.scp' in names:
            return os.path.join(root, scene + '.scp')
    return None


def contact_sheet(exe, game_dir, patch, scene, groups, out_path):
    """Before | after mask images of one island, one row per group, cropped to the river (Pillow)."""
    import tempfile
    from PIL import Image, ImageDraw
    Image.MAX_IMAGE_PIXELS = None
    path = find_scene(game_dir, scene)
    if path is None:
        return False
    width = 700
    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        for g in groups:
            row = []
            for p in (0, patch):
                d = os.path.join(tmp, 'p%s_g%d' % (p, g))
                # The dump is skipped above 16 Mi mask samples: river_qa samples coarser then.
                subprocess.run([exe, game_dir, '--groups', str(g), '--gap-patch', str(p),
                                '--max-mask-px', str(16 << 20), '--dump', d, path],
                               capture_output=True, text=True)
                pngs = sorted(os.listdir(d)) if os.path.isdir(d) else []
                img = Image.open(os.path.join(d, pngs[0])).convert('RGB') if pngs else None
                if img is not None:
                    img.load()
                row.append(img)
            rows.append((g, row))
    scaled = []
    for g, row in rows:
        have = [im for im in row if im is not None]
        if not have:
            scaled.append((g, [Image.new('RGB', (width, 30), (60, 60, 60))] * 2))
            continue
        # One box for both columns of a group: before and after stay aligned. The masks are
        # black outside the river's island rectangle's water, so the box is the union.
        boxes = [im.getbbox() for im in have if im.getbbox()]
        w0, h0 = have[0].size
        box = (max(min(b[0] for b in boxes) - 8, 0), max(min(b[1] for b in boxes) - 8, 0),
               min(max(b[2] for b in boxes) + 8, w0), min(max(b[3] for b in boxes) + 8, h0))             if boxes else (0, 0, w0, h0)
        k = width / (box[2] - box[0])
        size = (width, max(1, round((box[3] - box[1]) * k)))
        scaled.append((g, [im.crop(box).resize(size, Image.NEAREST) if im is not None
                           else Image.new('RGB', size, (60, 60, 60)) for im in row]))
    h = 20 + sum(scaled_h(r) + 18 for _, r in scaled)
    sheet = Image.new('RGB', (2 * width + 12, h), (30, 30, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((4, 4), '%s | left: gap patch 0, right: gap patch %s | red = gap, green = sprite '
              'with water, blue = enhanced river water only, orange = sprite without water'
              % (scene, patch), fill=(255, 255, 255))
    y = 20
    for g, r in scaled:
        draw.text((4, y), 'group %d' % g, fill=(255, 255, 255))
        y += 14
        for k, img in enumerate(r):
            sheet.paste(img, (k * (width + 12), y))
        y += scaled_h(r) + 4
    sheet.crop((0, 0, sheet.width, y)).save(out_path)
    return True


def scaled_h(images):
    return max(i.height for i in images)


def gap_patch(args, island_files, all_islands):
    files = island_files(args.game_dir, args.climates.split(',')) if args.climates \
        else all_islands(args.game_dir)
    if args.cache and os.path.isfile(args.cache):
        before, after, coverage = load_runs(args.cache)
    else:
        before, cov0 = run_per_island(args.exe, args.game_dir, files, 0, args.groups)
        after, cov1 = run_per_island(args.exe, args.game_dir, files, args.patch, args.groups)
        if cov0 != cov1:
            sys.exit('the two runs covered different scenes: %s against %s' % (cov0, cov1))
        coverage = cov0
        if args.cache:
            save_runs(args.cache, before, after, coverage)
    joined = join_runs(before, after)
    if not joined:
        sys.exit('no island with river pieces found')
    text = survey_summary(joined, args.patch, coverage) + '\n\n' + survey_table(joined)
    print(text)
    if args.table:
        with open(args.table, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text + '\n')
    if args.sheets:
        os.makedirs(args.sheets, exist_ok=True)
        worst = worst_islands(joined, args.worst)
        for scene, island in worst:
            out = os.path.join(args.sheets, '%s_i%d_gap-patch-%s.png' % (scene, island, args.patch))
            ok = contact_sheet(args.exe, args.game_dir, args.patch, scene,
                               sorted(joined[(scene, island)]), out)
            print('sheet %s: %s badness %s' % (out if ok else 'FAILED', scene,
                                               badness(joined[(scene, island)])))
    return 0
