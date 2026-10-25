"""Checks that an `anno-dump` build prints exactly what the Python references
in tools/ print (ADR 0001: the language-neutral contract).

  agree.py --dump <anno-dump> [--commands chunks,tex,...] [--game-dir <dir>]

Runs every case of the selected commands on the synthetic fixtures
(fixtures/synthetic/, our own files, always available) and, when a game
directory is given (or ANNO1503_GAME_DIR is set), on the original corpus.
Compares stdout and the exit code. Exit 0 if all agree, 1 otherwise.
"""
import argparse, os, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / 'tools'
FIX = ROOT / 'fixtures' / 'synthetic'


def files(base, *exts, names=None, exclude=()):
    out = []
    for p in sorted(base.rglob('*')):
        if not p.is_file() or p.name in exclude:
            continue
        if names and p.name not in names:
            continue
        if exts and p.suffix.lower().lstrip('.') not in exts:
            continue
        out.append(p)
    return out


def fixture(name):
    return sorted(p for p in (FIX / name).iterdir() if p.is_file())


# (command, python script, python args, dump args, files) — files: a function
# of the game dir (None for fixtures-only cases), returning paths/args.
def cases(game):
    c = []
    # chunks
    c.append(('chunks', 'chunks.py', ['tree'], ['chunks'], lambda: fixture('tex_synthetic') + fixture('sze_synthetic')))
    if game:
        c.append(('chunks', 'chunks.py', ['tree'], ['chunks'],
                  lambda: files(game, 'sze', 'scp', 'tex', 'inc', 'zei') + [game / 'Anlagen.dat']))
    # tex / inc
    c.append(('tex', 'tex.py', ['dump'], ['tex'], lambda: fixture('tex_synthetic') + fixture('tex_rejects')))
    c.append(('tex', 'tex.py', ['dump', '--full'], ['tex', '--full'], lambda: fixture('tex_synthetic')))
    c.append(('inc', 'tex.py', ['inc'], ['inc'], lambda: fixture('inc_synthetic')))
    if game:
        c.append(('tex', 'tex.py', ['dump'], ['tex'], lambda: files(game, 'tex')))
        c.append(('inc', 'tex.py', ['inc'], ['inc'], lambda: files(game, 'inc')))
        c.append(('tex', 'tex.py', ['dump', '--full'], ['tex', '--full'], lambda: [game / 'Textures' / 'debug.tex']))
    # colors
    c.append(('colors', 'tex.py', ['colors'], ['colors'], lambda: fixture('colors_synthetic')))
    c.append(('colors', 'tex.py', ['colors', '--full'], ['colors', '--full'], lambda: fixture('colors_synthetic')))
    if game:
        c.append(('colors', 'tex.py', ['colors'], ['colors'], lambda: [game / 'Anlagen.dat']))
        c.append(('colors', 'tex.py', ['colors', '--full'], ['colors', '--full'], lambda: [game / 'Anlagen.dat']))
    # figuren / frames
    figdir = FIX / 'figuren_synthetic'
    c.append(('figuren', 'figuren.py', ['dump', '--full'], ['figuren', '--full'], lambda: [figdir / 'FigurenHeader.dat', figdir / 'bad.dat']))
    c.append(('figuren', 'figuren.py', ['dump'], ['figuren'], lambda: [figdir / 'FigurenHeader.dat']))
    c.append(('frames', 'figuren.py', ['frames'], ['frames'], lambda: [figdir, '7', '1', '99']))
    if game:
        c.append(('figuren', 'figuren.py', ['dump', '--full'], ['figuren', '--full'], lambda: [game / 'FigurenHeader.dat']))
        c.append(('frames', 'figuren.py', ['frames'], ['frames'], lambda: [game, '101', '103', '1510', '1539', '1546', '1574']))
    # anlagen
    c.append(('anlagen', 'figuren.py', ['anlagen', '--full'], ['anlagen', '--full'], lambda: fixture('anlagen_synthetic')))
    c.append(('anlagen', 'figuren.py', ['anlagen'], ['anlagen'], lambda: fixture('anlagen_synthetic')))
    if game:
        c.append(('anlagen', 'figuren.py', ['anlagen', '--full'], ['anlagen', '--full'], lambda: [game / 'Anlagen.dat']))
    # insel
    c.append(('insel', 'insel.py', ['dump', '--full'], ['insel', '--full'], lambda: fixture('insel_synthetic')))
    c.append(('insel', 'insel.py', ['dump'], ['insel'], lambda: fixture('insel_synthetic')))
    if game:
        c.append(('insel', 'insel.py', ['dump'], ['insel'], lambda: files(game / 'Scenes', 'scp', 'sze')))
        c.append(('insel', 'insel.py', ['dump', '--full'], ['insel', '--full'], lambda: [game / 'Scenes' / '0200Single' / 'Single_01.sze']))
    # sze
    c.append(('sze', 'sze.py', ['dump', '--full'], ['sze', '--full'], lambda: fixture('sze_synthetic')))
    c.append(('sze', 'sze.py', ['dump'], ['sze'], lambda: fixture('sze_synthetic')))
    if game:
        c.append(('sze', 'sze.py', ['dump'], ['sze'], lambda: files(game / 'Scenes', 'sze')))
        c.append(('sze', 'sze.py', ['dump', '--full'], ['sze', '--full'], lambda: files(game / 'Scenes', 'sze')))
    # text
    tdir = FIX / 'text_synthetic'
    c.append(('keyed', 'text.py', ['keyed'], ['keyed'], lambda: [tdir / 'keyed.dat']))
    c.append(('texte', 'text.py', ['texte', '--full'], ['texte', '--full'], lambda: [tdir / 'texte.dat']))
    c.append(('texte', 'text.py', ['texte'], ['texte'], lambda: [tdir / 'texte.dat']))
    if game:
        c.append(('keyed', 'text.py', ['keyed'], ['keyed'], lambda: files(game / 'Data', 'dat', 'bat', exclude=('Debug.dat',))))
        c.append(('texte', 'text.py', ['texte'], ['texte'], lambda: [game / 'Texte.dat']))
        c.append(('texte', 'text.py', ['texte', '--full'], ['texte', '--full'], lambda: [game / 'Texte.dat']))
    # zei
    c.append(('zei', 'zei.py', ['dump', '--full'], ['zei', '--full'], lambda: fixture('zei_synthetic')))
    c.append(('zei', 'zei.py', ['dump'], ['zei'], lambda: fixture('zei_synthetic')))
    if game:
        c.append(('zei', 'zei.py', ['dump'], ['zei'], lambda: files(game, 'zei')))
        c.append(('zei', 'zei.py', ['dump', '--full'], ['zei', '--full'], lambda: files(game, 'zei')))
    # bigbrother (the scenario script): the accepted fixtures alone (exit 0), then all with the
    # refused bad_* ones (exit 1 on both sides)
    bbfix = lambda: [p for p in fixture('bigbrother_synthetic') if not p.name.startswith('bad_')]
    c.append(('bigbrother', 'bigbrother.py', ['dump'], ['bigbrother'], bbfix))
    c.append(('bigbrother', 'bigbrother.py', ['dump', '--full'], ['bigbrother', '--full'], bbfix))
    c.append(('bigbrother', 'bigbrother.py', ['dump'], ['bigbrother'], lambda: fixture('bigbrother_synthetic')))
    if game:
        c.append(('bigbrother', 'bigbrother.py', ['dump'], ['bigbrother'], lambda: files(game / 'Scenes', 'sze')))
        c.append(('bigbrother', 'bigbrother.py', ['dump', '--full'], ['bigbrother', '--full'],
                  lambda: files(game / 'Scenes', 'sze')))
    return c


# Windows' CreateProcess takes at most 32,767 characters of command line
# (WinError 206 beyond); a corpus case passes ~3,000 paths. Longer argument
# lists are split into batches below this length, run one after another, and
# their outputs joined. Both sides are split the same way, so the comparison
# stays exact even for output that is not per file.
ARGS_LIMIT = 24_000


def batches(args):
    out, cur, size = [], [], 0
    for a in args:
        n = len(str(a)) + 3
        if cur and size + n > ARGS_LIMIT:
            out.append(cur)
            cur, size = [], 0
        cur.append(a)
        size += n
    return out + [cur]


def run(prefix, args):
    code, stdout = 0, b''
    for batch in batches(args):
        r = subprocess.run([str(a) for a in (*prefix, *batch)], capture_output=True)
        code = max(code, r.returncode)
        stdout += r.stdout
    return code, stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', required=True, help='the anno-dump executable to check')
    ap.add_argument('--commands', default='', help='comma-separated commands (default: all)')
    ap.add_argument('--game-dir', default=os.environ.get('ANNO1503_GAME_DIR'))
    a = ap.parse_args()
    game = Path(a.game_dir) if a.game_dir else None
    if game and not (game / 'Anlagen.dat').is_file():
        # The same check as anno::test::game_dir (src/test_support).
        problem = ('the folder does not exist' if not game.exists() else
                   'not a folder' if not game.is_dir() else
                   'no Anlagen.dat: not the game folder')
        print(f'game directory {game}: {problem}. Point it at the installed game '
              '(the folder with Anlagen.dat), or unset ANNO1503_GAME_DIR for the fixtures only.')
        return 1
    only = set(filter(None, a.commands.split(',')))
    failed = checked = 0
    for cmd, script, py_args, dump_args, paths in cases(game):
        if only and cmd not in only:
            continue
        args = paths()
        py = run([sys.executable, '-I', TOOLS / script, *py_args], args)
        cc = run([a.dump, *dump_args], args)
        checked += 1
        where = 'corpus' if game and any(str(game) in str(p) for p in args) else 'fixtures'
        label = f"{' '.join(dump_args)} ({where}, {len(args)} args)"
        if py == cc:
            print(f'ok   {label}')
            continue
        failed += 1
        print(f'FAIL {label}: exit {cc[0]} vs {py[0]}')
        a_lines, b_lines = cc[1].splitlines(), py[1].splitlines()
        for i, (x, y) in enumerate(zip(a_lines, b_lines)):
            if x != y:
                print(f'  line {i + 1}:\n    dump:   {x[:200]!r}\n    python: {y[:200]!r}')
                break
        else:
            print(f'  lengths differ: {len(a_lines)} vs {len(b_lines)} lines')
    print(f'{checked - failed} of {checked} cases agree' + ('' if game else ' (no game dir: fixtures only)'))
    return 1 if failed or not checked else 0


if __name__ == '__main__':
    sys.exit(main())
