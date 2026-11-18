#!/usr/bin/env python3
"""Public CLI controls on isolated copies of the actual finite inventory inputs."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / 'tools/sim_capabilities.py'
FILES = [
    'src/sim/include/anno/sim/config.hpp',
    'src/sim/include/anno/sim/command.hpp',
    'src/sim/include/anno/sim/types.hpp',
    'src/sim/include/anno/sim/seams.hpp',
    'src/sim/src/codec.cpp', 'src/sim/src/types.cpp',
    'src/sim/src/building.cpp', 'src/sim/src/scheduler.cpp',
    'docs/sim/capabilities.md',
]
DOC, CFG, CMD = FILES[-1], FILES[0], FILES[1]


def main():
    originals = {p: (ROOT/p).read_bytes() for p in FILES}
    receipts = []
    with tempfile.TemporaryDirectory(prefix='sim-capabilities-contract-') as temp:
        root = Path(temp) / 'relocated'
        elsewhere = Path(temp) / 'unrelated-cwd'
        elsewhere.mkdir()
        for p, data in originals.items():
            target = root/p
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        # Use a relocated checker too, exercising its default-root convention.
        (root/'tools').mkdir()
        shutil.copy2(CHECKER, root/'tools/sim_capabilities.py')

        def run(label, code, diagnostic='', explicit=True):
            command = [sys.executable, '-I', str(root/'tools/sim_capabilities.py')]
            if explicit:
                command += ['--root', str(root)]
            p = subprocess.run(command, cwd=elsewhere, capture_output=True, text=True)
            output = p.stdout+p.stderr
            if p.returncode != code or diagnostic not in output:
                raise AssertionError(f'{label}: wanted exit {code}, {diagnostic!r}; got {p.returncode}: {output}')
            receipts.append({'case':label, 'exit':p.returncode, 'output':output.strip()})
            print(f'ok: {label} (exit {p.returncode})')

        def changed(label, path, old, new, diagnostic):
            text = originals[path].decode('utf-8')
            if text.count(old) != 1:
                raise AssertionError(f'{label}: replacement must apply exactly once, found {text.count(old)}')
            replacement = text.replace(old, new)
            assert replacement != text
            (root/path).write_text(replacement, encoding='utf-8')
            try:
                run(label, 1, diagnostic)
            finally:
                (root/path).write_bytes(originals[path])

        run('legal relocated explicit root', 0, 'agreement')
        run('legal relocated default root', 0, 'agreement', explicit=False)
        row = next(line+'\n' for line in originals[DOC].decode().splitlines() if line.startswith('| `rule:slot_policy`'))
        changed('real inventory omission', DOC, row, '', 'missing inventory row: rule:slot_policy')
        changed('extra inventory row', DOC, row, row+'| `rule:invented` | `bool` | `false` | invented | none |\n', 'extra inventory row: rule:invented')
        changed('duplicate row', DOC, row, row+row, 'duplicate identity rule:slot_policy')
        changed('new Rules field', CFG, 'struct Rules {', 'struct Rules {\n bool new_behavior = false;', 'missing inventory row: rule:new_behavior')
        changed('changed declared default', CFG, 'bool house_updates = false;', 'bool house_updates = true;', 'drift rule:house_updates:')
        changed('string comment markers are data', CFG, 'std::vector<std::string> city_names;', 'std::vector<std::string> city_names = {"// data /* not a comment */"};', 'drift rule:city_names:')
        changed('changed referenced constant', CFG, 'kEventsMaxDispatchDefault = 1ULL << 20;', 'kEventsMaxDispatchDefault = 1ULL << 21;', 'drift constant:kEventsMaxDispatchDefault:')
        slot_start = originals[CFG].decode().index('enum class SlotPolicy')
        slot_end = originals[CFG].decode().index('};', slot_start)+2
        old_slot = originals[CFG].decode()[slot_start:slot_end]
        new_slot = old_slot.replace('Original,', 'SwapName,').replace('LowestFree,', 'Original,').replace('SwapName,', 'LowestFree,')
        changed('reordered enum', CFG, old_slot, new_slot, 'drift enum:SlotPolicy.Original:')
        changed('new Body alternative', CMD, 'using Body = std::variant<BuildCmd,', 'using Body = std::variant<NewCmd, BuildCmd,', 'missing/extra wire mapping for NewCmd')
        changed('changed wire value', CMD, 'kWireBuild = 0x64,', 'kWireBuild = 0x65,', 'drift command:BuildCmd:')
        changed('changed wire visitor', FILES[4], 'return kWireBuild;', 'return kWireDemolish;', 'WireType: unmapped definition kWireBuild')
        changed('bypassed wire visitor', FILES[4], 'return std::visit(Type{}, body);', 'return kWireBuild;', 'wire_type: unsupported dispatch')
        class_source = originals[FILES[5]].decode()
        table_start = class_source.index('std::vector<ClassInfo> original_class_table()')
        table_end = class_source.index('std::uint32_t service_mask_for_goods', table_start)
        table_body = class_source[table_start:table_end]
        changed('class mutation outside initializer', FILES[5], table_body,
                table_body.replace('return t;', 't.clear(); return t;'),
                'class table: unsupported statement outside initializer')
        changed('changed class tick', FILES[5], '{C::Workshop, T::Workshop, P::Workshop, 500}', '{C::Workshop, T::None, P::Workshop, 500}', 'drift class:Workshop:')
        changed('changed class payload', FILES[5], '{C::Workshop, T::Workshop, P::Workshop, 500}', '{C::Workshop, T::Workshop, P::Inert, 500}', 'drift class:Workshop:')
        hook = next(line+'\n' for line in originals[DOC].decode().splitlines() if line.startswith('| `hook:on_placed`'))
        changed('omitted command hook', DOC, hook, '', 'missing inventory row: hook:on_placed')
        changed('new Seams member', FILES[3], 'Seam<PlaceHook> on_placed;', 'Seam<PlaceHook> on_placed;\n Seam<PlaceHook> extra_hook;', 'missing inventory row: hook:extra_hook')
        changed('new phase call', FILES[7], 'w.route_cache_.begin_advance();', 'new_phase();\n w.route_cache_.begin_advance();', 'missing inventory row: phase:new_phase')
        changed('unsupported Rules declaration', CFG, 'struct Rules {', 'struct Rules {\n bool strange[2];', 'Rules: unsupported declaration:')
        changed('unsupported class mapping', FILES[5], '{C::Plain, T::None, P::Inert, 0}', '{C::Plain, compute_tick(), P::Inert, 0}', 'class table: unsupported row')
        for kind in ['missing','empty']:
            target = root/CFG
            if kind == 'missing':
                target.unlink()
            else:
                target.write_bytes(b'')
            try:
                run(f'{kind} required input', 1, 'required input')
            finally:
                target.write_bytes(originals[CFG])
        # A comment containing a plausible field must not acquire a capability.
        (root/CFG).write_bytes(originals[CFG].replace(b'struct Rules {', b'struct Rules {\n /* bool fake_field = true; */'))
        run('comment is not a declaration', 0, 'agreement')
        (root/CFG).write_bytes(originals[CFG])
        run('restored legal control', 0, 'agreement')
    # No production input may be changed by this test.
    for path, data in originals.items():
        assert (ROOT/path).read_bytes() == data, path
    print(f'sim_capabilities contract: {len(receipts)} cases, 0 failures; real inputs unchanged')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (AssertionError, OSError, UnicodeError) as e:
        print(f'sim_capabilities contract: FAIL: {e}', file=sys.stderr)
        sys.exit(1)
