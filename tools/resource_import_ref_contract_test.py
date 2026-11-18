#!/usr/bin/env python3
"""Public command contract; source replacements must apply exactly once."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def run(ref, path, expected):
    p = subprocess.run([sys.executable, '-I', '-B', str(ref), str(path)], capture_output=True, text=True, timeout=10)
    if p.returncode != expected or 'Traceback' in p.stdout+p.stderr:
        raise AssertionError(f'{path.name}: {p.returncode}: {p.stdout}{p.stderr}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', required=True, type=Path)
    parser.add_argument('--vectors', required=True, type=Path)
    args = parser.parse_args()
    text = args.vectors.read_text()
    with tempfile.TemporaryDirectory() as folder:
        root = Path(folder)
        def trial(name, content, code, ref=args.ref):
            path = root / name
            path.write_text(content)
            run(ref, path, code)
        run(args.ref, args.vectors, 0)
        trial('changed', text.replace('consumed=1', 'consumed=0', 1), 1)
        trial('empty', '# none\n', 2)
        trial('malformed', 'w=1 => consumed=0', 2)
        run(args.ref, root/'missing', 2)
        first = next(x for x in text.splitlines() if x and not x.startswith('#'))
        trial('exhausted', first.replace('draws=4', 'draws=-'), 2)
        trial('missing-following', next(x for x in text.splitlines() if '104:226:' in x).replace(';105:291:1:1:9:70:404', ''), 2)
        trial('unsupported', first.replace(':41:', ':8:'), 2)
        trial('geometry', first.replace('plane=171,12,13,14', 'plane=171'), 2)
        stone = next(x for x in text.splitlines() if 'anchor=1,0,0,0,0,0,0,0,0' in x)
        trial('expected-anchor-geometry', stone.replace('anchor=1,0,0,0,0,0,0,0,0', 'anchor=1'), 2)
        # The original placement helper is shared by the Python reference only.
        (root/'placement_ref.py').write_text((args.ref.parent/'placement_ref.py').read_text())
        source = args.ref.read_text()
        for name, old, new in [
            ('mask', '(plane[i] & 15) | (variant << 4)', '(plane[i] & 240) | variant'),
            ('skip-draw', 'consumed += 1', 'consumed += 0'),
            ('overwrite-anchor', 'if overlay_scenery and objects[i] and reg[objects[i]].flags & 16 and anchors[i]:', 'if False:'),
            ('ignore-earlier', 'objects[i], phase[i], item_height[i] = tid, state & 63, height', 'objects[i], phase[i], item_height[i] = 0, state & 63, height'),
        ]:
            if source.count(old) != 1:
                raise AssertionError(f'{name}: replacement did not apply once')
            # Legal control before mutation, then reject on the authored expectations.
            control = root/'control.py'
            control.write_text(source)
            run(control, args.vectors, 0)
            mutant = root/f'{name}.py'
            mutant.write_text(source.replace(old, new))
            if name == 'ignore-earlier':
                trial('overlap-control', next(x for x in text.splitlines() if 'status=accepted,occupied' in x).replace('draws=-', 'draws=0'), 0)
                run(mutant, root/'overlap-control', 1)
            else:
                run(mutant, args.vectors, 1)
            print(f'APPLIED {name}: legal control 0, mutant 1')
        # Existing flags, not incoming flags: admit an installed cliff by the
        # legal path first, then prove the ordinary resource still refuses.
        placement = (args.ref.parent/'placement_ref.py').read_text()
        old = 'if e.flags & 0x20:'
        if placement.count(old) != 1:
            raise AssertionError('cliff-replaceability: replacement did not apply once')
        control = root/'control.py'
        control.write_text(source)
        run(control, args.vectors, 0)
        (root/'placement_ref.py').write_text(placement.replace(old, 'if e.flags & 0x30:'))
        run(control, args.vectors, 1)
        print('APPLIED cliff-replaceability: legal control 0, mutant 1')
        # Restore the private imported module before the next legal control.
        old = '0: max(cellmin)'
        if placement.count(old) != 1:
            raise AssertionError('rock-mode: replacement did not apply once')
        (root/'placement_ref.py').write_text(placement)
        run(control, args.vectors, 0)
        (root/'placement_ref.py').write_text(placement.replace(old, '0: min(cellmin)'))
        run(control, args.vectors, 1)
        print('APPLIED rock-mode: legal control 0, mutant 1')
    print('resource import contract: 23 public checks passed')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
