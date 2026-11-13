#!/usr/bin/env python3
"""Check or regenerate named sim pins and current recordings through the canonical gate.

Requires a configured dev build and original assets for the complete sim selection.
Oracle programs are only compared, never invoked with a writing option.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
SELECT = r'^(anno_sim_tests\.|anno_sim_replay_|population_oracles_agree$|integration_stream_reference_)'
RECORDINGS = ('goldens.txt', 'scenario_corpus.txt', 'loop_checkpoints.txt',
              'g2.log', 'g3.log', 'g4.log', 'g5.log', 'g6.log', 'm2_found.log',
              'm2_found_wrong.log', 'cargo_load.log', 'cargo_load_wrong.log',
              'cargo_unload.log', 'cargo_unload_wrong.log')


def oracle_owned(name):
    return (name.startswith(('oracle_', 'g0_', 'g1_players_', 's5_islands_', 's6_g1_'))
            or name in ('empty_world', 'empty_registry'))


def read_pins(path):
    result = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        fields = line.split('#', 1)[0].split()
        if not fields:
            continue
        if (len(fields) != 4 or fields[0] in result or len(fields[3]) != 16
                or any(c not in '0123456789abcdefABCDEF' for c in fields[3])):
            raise ValueError('malformed/duplicate pin: ' + line)
        result[fields[0]] = (fields[1], int(fields[2]), int(fields[3], 16))
    if not result:
        raise ValueError('zero pins')
    return result


def snapshot(current):
    return {name: (hashlib.sha256((current / name).read_bytes()).hexdigest(),
                   (current / name).stat().st_mtime_ns) for name in RECORDINGS}


def run(args, execute=subprocess.run):
    recording = args.command == 'regen'
    if recording and args.jobs not in (None, 1):
        raise ValueError('regen requires --jobs 1: replay/corpus writers are serialized')
    if recording and os.environ.get('CI'):
        raise ValueError('regen refused under CI; no files changed')
    current = ROOT / 'fixtures/synthetic/sim/current'
    before = read_pins(current / 'goldens.txt')
    files_before = snapshot(current)
    env = dict(os.environ)
    env.pop('ANNO_UPDATE_GOLDEN', None)
    if recording:
        env['ANNO_UPDATE_GOLDEN'] = '1'
    jobs = args.jobs if args.jobs is not None else (1 if recording else 4)
    env['CMAKE_BUILD_PARALLEL_LEVEL'] = str(jobs)
    log = args.log or ROOT / 'cache/gates' / f'pins-{args.command}-{time.time_ns()}.log'
    command = [sys.executable, '-I', str(ROOT / 'tools/test/run_selected.py'),
               '--build-dir', str(args.build_dir or ROOT / 'build/dev'),
               '--target', 'pins_checks', '--select', args.select,
               '--require-test', 'pins_contract', '--skip-auto-contract',
               '--jobs', str(jobs), '--timeout', str(args.timeout), '--log', str(log)]
    if args.config:
        command += ['--config', args.config]
    if recording:
        for name in RECORDINGS:
            command += ['--record-fixture', str(current / name)]
    result = execute(command, cwd=ROOT, env=env)
    after = read_pins(current / 'goldens.txt')
    files_after = snapshot(current)
    moved = [name for name in sorted(before.keys() | after.keys())
             if before.get(name) != after.get(name)]
    for name in moved:
        old = before.get(name)
        new = after.get(name)
        print('pin ' + name + ': ' + ('missing' if old is None else f'{old[2]:016x}')
              + ' -> ' + ('missing' if new is None else f'{new[2]:016x}'))
    changed = [name for name in RECORDINGS if files_before[name][0] != files_after[name][0]]
    if recording:
        for name in RECORDINGS[1:]:
            if files_before[name] != files_after[name]:
                print('golden re-recorded: ' + name + (' (changed)' if name in changed else ' (unchanged)'))
    illegal = any(oracle_owned(name) for name in moved)
    if not recording and changed:
        print('FAIL: check changed a fixture', file=sys.stderr)
    if illegal:
        print('FAIL: independent oracle pin changed; never regenerate oracle values', file=sys.stderr)
    code = result.returncode or int(illegal or (not recording and bool(changed)))
    print(f'PINS {args.command}: {"FAIL" if code else "PASS"}; {len(moved)} pins moved; '
          f'{len(changed)} fixture byte changes; '
          f'scope={"full" if args.select == SELECT else "partial"}; '
          f'selection={args.select}; evidence={log}')
    return code


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('check', 'regen'))
    parser.add_argument('--area', choices=('sim',), default='sim')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--config', default='')
    parser.add_argument('--jobs', type=int)
    parser.add_argument('--timeout', type=int, default=3600)
    parser.add_argument('--log', type=Path)
    parser.add_argument('--select', default=SELECT, help='explicit bounded subset for diagnosis/proofs')
    args = parser.parse_args(argv)
    if (args.jobs is not None and args.jobs < 1) or args.timeout < 1:
        parser.error('jobs and timeout must be positive')
    try:
        return run(args)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
