#!/usr/bin/env python3
"""Bounded public-command checks of the ore census with wholly synthetic files."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


def chunk(name, data):
    return name.encode().ljust(12, b'\0') + struct.pack('<I', len(data)) + data


def fixture(root):
    # Hand-specified independent records, not imported from the reference.
    pairs = [(1560, 'ERZSYMBOL', 'EISENERZ'), (1561, 'ERZSYMBOL', 'GOLD'),
             (1562, 'ERZSYMBOL', 'SALZSTEIN'), (1563, 'ERZSYMBOL', 'EDELSTEINE'),
             (1564, 'ERZSYMBOL', 'MAMORSTEIN'), (1661, 'ERZSYMBOL', 'EISENERZ'),
             (1550, 'SCHATZ', 'SCHATZ'), (1551, 'SCHATZ', 'SCHATZ')]
    entries = b''.join(chunk('ENTRY', struct.pack('<II', 0, key) +
                             chunk('BEWOHNTYPE', kind.encode() + b'\0') +
                             chunk('WARE', ware.encode() + b'\0')) for key, kind, ware in pairs)
    (root / 'FigurenHeader.dat').write_bytes(chunk('ANNO', chunk('FIGUREN', entries)))
    directory = root / 'Scenes' / 'Islands' / 'Synthetic'
    directory.mkdir(parents=True)
    def template(name, marker_types, info5):
        info = bytes([0, 0, 0, 0, 0, info5, 0, 0, 0, 0, 0, 0])
        records = b''.join(b'ENTRY2' + struct.pack('<H', 24) +
                           struct.pack('<I', (t << 14) | 7) + bytes(20) for t in marker_types)
        data = chunk('INSEL4', struct.pack('<HHI', 2, 2, 0))
        data += chunk('INFO', info) + chunk('OCCUR', records)
        path = directory / name
        path.write_bytes(chunk('ANNO', data))
        return path
    checked = template('BASIC.scp', [1560, 1661, 1550], 2)
    template('EMPTY.scp', [], 0)
    template('xPiRaTx.scp', [], 7)
    template('s_DUMMY.scp', [], 7)
    return checked


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ref', type=Path, required=True)
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop('ANNO1503_GAME_DIR', None)
    checks = 0
    def run(ref, game=None):
        cmd = [sys.executable, '-I', str(ref)]
        if game is not None:
            cmd += ['--game-dir', str(game)]
        return subprocess.run(cmd, capture_output=True, text=True, timeout=20, env=env)
    def expect(result, code, text):
        nonlocal checks
        if result.returncode != code or text not in result.stdout + result.stderr:
            raise AssertionError((code, text, result.returncode, result.stdout, result.stderr))
        checks += 1
    try:
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            game = tmp / 'game'; game.mkdir()
            site = fixture(game)
            expect(run(args.ref), 0, 'SKIP:')
            expect(run(args.ref, tmp / 'missing'), 2, 'MALFORMED:')
            expect(run(args.ref, game), 0, 'files=4 candidates=2 agree=2 ore_markers=2')
            original = site.read_bytes()
            data = bytearray(original); at = data.index(b'INFO') + 16 + 5; data[at] = 3
            site.write_bytes(data)
            expect(run(args.ref, game), 1, 'ore markers 2, INFO[5] 3')
            site.write_bytes(original[:-1])
            expect(run(args.ref, game), 2, 'MALFORMED:')
            site.write_bytes(original)
            source = args.ref.read_text()
            mutants = [('packed type', '(word >> 14) & 0xfff', '(word >> 13) & 0xfff'),
                       ('INFO byte', "ore != info[5]", "ore != info[4]"),
                       ('classification', "types[t][0] == 'ERZSYMBOL'", "t == 1560")]
            for number, (name, before, after) in enumerate(mutants):
                if source.count(before) != 1:
                    raise AssertionError(f'{name}: replacement did not apply exactly once')
                mutant = tmp / f'mutant{number}.py'
                mutant.write_text(source.replace(before, after))
                for module in ('figuren.py', 'insel.py', 'chunks.py'):
                    shutil.copyfile(args.ref.parent / module, tmp / module)
                result = run(mutant, game)
                if result.returncode == 0:
                    raise AssertionError(f'{name}: wrong reading accepted')
                checks += 1
                print(f'MUTANT {name}: applied=1 exit={result.returncode}, rejected')
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print('CONTRACT FAIL:', error)
        return 1
    print(f'CONTRACT PASS: checks={checks}, mutants=3')
    return 0


if __name__ == '__main__':
    sys.exit(main())
