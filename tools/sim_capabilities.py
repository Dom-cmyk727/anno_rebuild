#!/usr/bin/env python3
"""Read-only finite C++/capabilities table reconciliation (not a C++ analyzer).

Supports the named aggregate declarations, enum/variant lists, wire_type visitor,
original_class_table initializer, payload override and Scheduler::advance call names.
No behavior evaluation, preprocessing, arbitrary expression evaluation or prose inference.
Exit 0: agreement; 1: drift or unsupported/missing/empty/duplicate input. --dump emits
source-derived JSON for an explicit human documentation edit; it never writes a file.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DOC = 'docs/sim/capabilities.md'
CONFIG = 'src/sim/include/anno/sim/config.hpp'
COMMAND = 'src/sim/include/anno/sim/command.hpp'
TYPES = 'src/sim/include/anno/sim/types.hpp'
SEAMS = 'src/sim/include/anno/sim/seams.hpp'
# Finite shared aggregate types owned by Rules. Unknown owned types fail closed.
AGGREGATES = {'Rules', 'NavRules', 'CargoRules', 'FreightRules', 'NavClassCost'}
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[A-Za-z_]\w*|0[xX][\da-fA-F]+|\d+|::|==|!=|<<|>>|->|&&|\|\||[^\s]')


class Invalid(ValueError):
    pass


def uncomment(text):
    # Tokenize strings before comments, retaining line boundaries for diagnostics.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: ' ' if m[0].startswith(('//', '/*')) else m[0], text)


def norm(text):
    return ' '.join(TOKEN.findall(text))


def unique(rows, label):
    if not rows:
        raise Invalid(f'{label}: empty result')
    names = [r[0] for r in rows]
    dup = sorted(k for k, v in Counter(names).items() if v > 1)
    if dup:
        raise Invalid(f'{label}: duplicate identity {", ".join(dup)}')
    return rows


def block(text, pattern, label):
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise Invalid(f'{label}: expected one required block, found {len(matches)}')
    start = text.find('{', matches[0].start())
    depth = 1
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[start+1:]):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[start+1:start+1+token.start()]
    raise Invalid(f'{label}: unterminated block')


def split(text, delimiter, angles=False):
    result, start, stack = [], 0, []
    pairs = {'(': ')', '[': ']', '{': '}'}
    if angles:
        pairs['<'] = '>'
    # Actual initializers use nested template closings, not comparisons. Expressions
    # with shifts are never passed to angle-aware splitting.
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|.', text, re.S):
        t = m[0]
        if t in pairs:
            stack.append(pairs[t])
        elif stack and t == stack[-1]:
            stack.pop()
        elif t == delimiter and not stack:
            result.append(text[start:m.start()].strip())
            start = m.end()
    if stack:
        raise Invalid('unsupported/unbalanced declaration')
    if text[start:].strip():
        result.append(text[start:].strip())
    return result


def members(text, name):
    body = block(text, rf'\bstruct\s+{name}\s*\{{', name)
    # Explicit supported method forms; any new declaration form must be reviewed.
    body = re.sub(r'friend\s+(?:constexpr\s+)?bool\s+operator==\([^;]*?=\s*default\s*;', '', body)
    body = re.sub(r'\[\[nodiscard\]\]\s+std::(?:string|uint64_t)\s+(?:validate|digest|problem)\([^;]*;', '', body)
    if name == 'Seams':
        method = block(body, r'\bstatic\s+Seams\s+sized_for\([^)]*\)\s*\{', 'Seams::sized_for')
        start = body.index('static Seams sized_for')
        end = body.index('{', start) + len(method) + 2
        body = body[:start] + body[end:]
    fields, constants = [], []
    for declaration in split(body, ';'):
        if not declaration:
            continue
        if name == 'Seams' and re.fullmatch(r'using\s+\w+\s*=\s*[\s\S]+', declaration):
            continue
        static = re.fullmatch(r'static constexpr\s+([\w:]+)\s+(\w+)\s*=\s*(.+)', declaration, re.S)
        if static:
            constants.append((f'{name}::{static[2]}', norm(static[1]), norm(static[3])))
            continue
        m = re.fullmatch(r'([\w:]+(?:\s*<[\w:<> ,]+>)?)\s+(\w+[\s\S]*)', declaration)
        if not m:
            raise Invalid(f'{name}: unsupported declaration: {norm(declaration)}')
        typ = re.sub(r'\s*::\s*', '::', norm(m[1])).replace('>>', '> >')
        for entry in split(m[2], ','):
            d = re.fullmatch(r'(\w+)(?:\s*=\s*([\s\S]+))?', entry)
            if not d:
                raise Invalid(f'{name}: unsupported declaration: {norm(declaration)}')
            fields.append((d[1], typ, norm(d[2]) if d[2] else '{}'))
    return unique(fields, name), constants


def enumerators(text, name):
    body = block(text, rf'\benum(?:\s+class)?\s+{name}\s*[^{{;]*\{{', name)
    rows = []
    for item in split(body, ','):
        m = re.fullmatch(r'(\w+)(?:\s*=\s*(0x[\da-fA-F]+|\d+))?', item)
        if not m:
            raise Invalid(f'{name}: unsupported enumerator: {norm(item)}')
        rows.append((m[1], m[2] or 'implicit'))
    return unique(rows, name)


def variants(text, name):
    matches = list(re.finditer(rf'\busing\s+{name}\s*=\s*std::variant<([^;]+)>;', text))
    if len(matches) != 1:
        raise Invalid(f'{name}: missing/duplicate variant')
    m = matches[0]
    rows = [(s.strip(),) for s in m[1].split(',')]
    if any(not re.fullmatch(r'\w+', r[0]) for r in rows):
        raise Invalid(f'{name}: unsupported alternative')
    return [r[0] for r in unique(rows, name)]


def extract(root):
    inputs = {}
    def read(path):
        try:
            text = (root/path).read_text(encoding='utf-8')
        except (OSError, UnicodeError) as e:
            raise Invalid(f'{path}: unreadable required input: {e.strerror if isinstance(e, OSError) else "encoding"}') from e
        if not text.strip():
            raise Invalid(f'{path}: empty required input')
        inputs[path] = uncomment(text)
        return inputs[path]
    cfg, cmd, types, seams = [read(p) for p in [CONFIG, COMMAND, TYPES, SEAMS]]
    codec = read('src/sim/src/codec.cpp')
    classes = read('src/sim/src/types.cpp')
    building = read('src/sim/src/building.cpp')
    scheduler = read('src/sim/src/scheduler.cpp')
    out = {}
    definitions = {}
    for name in sorted(AGGREGATES):
        definitions[name], constants = members(cfg, name)
        if constants:
            unique(constants, name+' constants')
        for key, typ, value in constants:
            out[f'constant:{key}'] = [typ, value]
    def descend(name, prefix):
        for field, typ, default in definitions[name]:
            path = f'{prefix}.{field}' if prefix else field
            out[f'rule:{path}'] = [typ, default]
            if typ in AGGREGATES:
                descend(typ, path)
            elif typ == 'std::vector < NavClassCost >':
                descend('NavClassCost', path+'[]')
            elif not re.fullmatch(r'bool|std::(?:u?int(?:16|32|64)_t|vector < (?:std::uint32_t|std::string|std::vector < std::uint32_t >) >)|SlotPolicy|SpatialOrder|PrngMode|IslandCandidateOrder', typ):
                raise Invalid(f'Rules: unsupported owned type {typ} at {path}')
    descend('Rules', '')
    # Record referenced named defaults instead of evaluating C++ expressions.
    for _, typ, value in definitions['Rules']:
        if re.fullmatch(r'k\w+', value):
            m = re.findall(rf'inline constexpr\s+([\w:]+)\s+{value}\s*=\s*([^;]+);', cfg)
            if len(m) != 1:
                raise Invalid(f'{value}: missing/duplicate referenced constant')
            out[f'constant:{value}'] = [norm(m[0][0]), norm(m[0][1])]
    for enum in ['SlotPolicy','SpatialOrder','PrngMode','IslandCandidateOrder']:
        for index, (name, value) in enumerate(enumerators(cfg, enum)):
            out[f'enum:{enum}.{name}'] = [f'position {index}; initializer {value}', 'declaration only; validation is reviewed']
    bodies = variants(cmd, 'Body')
    wires = dict(enumerators(cmd, 'WireType'))
    visitor = block(codec, r'\bstd::uint16_t\s+wire_type\([^)]*\)\s*\{', 'wire_type')
    visitor_body = block(visitor, r'\bstruct\s+Type\s*\{', 'wire_type visitor')
    expected_dispatch = 'struct Type {' + visitor_body + '}; return std::visit(Type{}, body);'
    if norm(visitor) != norm(expected_dispatch):
        raise Invalid('wire_type: unsupported dispatch (expected return std::visit(Type{}, body))')
    mappings = re.findall(r'operator\(\)\(const\s+(\w+)&(?:\s+\w+)?\)\s+const\s*\{\s*return\s+([\w.]+)\s*;\s*\}', visitor)
    unique(mappings, 'wire_type mappings')
    method_pattern = r'std::uint16_t\s+operator\(\)\(const\s+\w+&(?:\s+\w+)?\)\s+const\s*\{\s*return\s+[\w.]+\s*;\s*\}'
    if re.sub(method_pattern, '', visitor_body).strip():
        raise Invalid('wire_type: unsupported visitor declaration')
    if set(dict(mappings)) != set(bodies):
        raise Invalid('Body: missing/extra wire mapping for '+', '.join(sorted(set(dict(mappings)) ^ set(bodies))))
    used = set()
    for i, body in enumerate(bodies):
        wire = dict(mappings)[body]
        if body == 'LegacyRawCmd' and wire == 'c.type':
            value = 'dynamic (non-native only)'
        elif wire in wires:
            value = wires[wire]
            used.add(wire)
        else:
            raise Invalid(f'{body}: missing wire definition {wire}')
        out[f'command:{body}'] = [str(i), f'{wire} = {value}']
    if used != set(wires):
        raise Invalid('WireType: unmapped definition '+', '.join(sorted(set(wires)-used)))
    enum_classes = [r[0] for r in enumerators(types, 'RuntimeClass') if r[0] != 'Count_']
    ticks = {r[0] for r in enumerators(types, 'TickKind')}
    payloads = {r[0] for r in enumerators(types, 'PayloadTag')}
    table = block(classes, r'\bstd::vector<ClassInfo>\s+original_class_table\(\)\s*\{', 'original_class_table')
    init = block(table, r'\bstd::vector<ClassInfo>\s+t\s*=\s*\{', 'class table rows')
    expected_table = ('using C = RuntimeClass; using T = TickKind; using P = PayloadTag; '
                      'std::vector<ClassInfo> t = {' + init + '}; return t;')
    if norm(table) != norm(expected_table):
        raise Invalid('class table: unsupported statement outside initializer')
    rows = []
    for row in split(init, ','):
        m = re.fullmatch(r'\{\s*C::(\w+)\s*,\s*T::(\w+)\s*,\s*P::(\w+)\s*,\s*(\d+)\s*\}', row)
        if not m:
            raise Invalid('class table: unsupported row '+norm(row))
        rows.append(tuple(m.groups()))
    unique(rows, 'class table')
    if [r[0] for r in rows] != enum_classes:
        raise Invalid('RuntimeClass: missing/extra/reordered class row')
    for i, (cls, tick, payload, gate) in enumerate(rows):
        if tick not in ticks or payload not in payloads:
            raise Invalid(f'{cls}: missing tick/payload mapping')
        out[f'class:{cls}'] = [str(i), f'{tick} / {payload} / {gate}']
    # This short actual override is an explicit review trigger, not behavior inference.
    override = block(building, r'\bPayloadTag\s+payload_tag_of\([^)]*\)\s*\{', 'payload_tag_of')
    out['override:payload_tag_of'] = ['source expression', norm(override)]
    for field, typ, default in members(seams, 'Seams')[0]:
        if 'Seam <' not in typ:
            raise Invalid(f'Seams: unsupported member {field}: {typ}')
        out[f'hook:{field}'] = [typ, default]
    advance = block(scheduler, r'\bstd::uint64_t\s+Scheduler::advance\([^)]*\)\s*\{', 'Scheduler::advance')
    calls = re.findall(r'(?<![\w:])((?:\w+(?:::|\.|->))*\w+)\s*\(', advance)
    # All named calls, including condition/loop calls and fault cleanup: no fixed
    # phase whitelist that could silently omit a newly added function call.
    calls = [c for c in calls if c not in {'if','for','while','switch','sizeof'}]
    for index, (name, count) in enumerate(Counter(calls).items()):
        out[f'phase:{name}'] = ['Scheduler::advance call surface', f'first-seen {index}; sites {count}']
    return out


def document(root):
    try:
        text = (root/DOC).read_text(encoding='utf-8')
    except (OSError, UnicodeError) as e:
        raise Invalid(f'{DOC}: unreadable required input') from e
    if not text.strip():
        raise Invalid(f'{DOC}: empty required input')
    rows = []
    for line in text.splitlines():
        if line.startswith('| `'):
            cells = [c.strip() for c in line.strip('|').split('|')]
            if len(cells) != 5 or not all(cells) or any(not c.startswith('`') or not c.endswith('`') for c in cells[:3]):
                raise Invalid(f'{DOC}: unsupported inventory row: {line}')
            rows.append((cells[0][1:-1], [c[1:-1] for c in cells[1:3]]))
    return dict(unique(rows, DOC))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, default=ROOT)
    ap.add_argument('--dump', action='store_true', help='print extracted JSON; no writes')
    args = ap.parse_args(argv)
    try:
        actual = extract(args.root)
        if args.dump:
            print(json.dumps(actual, indent=2))
            return 0
        expected = document(args.root)
        errors = []
        for name in sorted(actual.keys()-expected.keys()):
            errors.append(f'missing inventory row: {name}')
        for name in sorted(expected.keys()-actual.keys()):
            errors.append(f'extra inventory row: {name}')
        for name in sorted(actual.keys() & expected.keys()):
            if actual[name] != expected[name]:
                errors.append(f'drift {name}: source={actual[name]!r}, document={expected[name]!r}')
        if errors:
            raise Invalid('\n'.join(errors))
        counts = Counter(k.split(':',1)[0] for k in actual)
        print('sim_capabilities: agreement; '+', '.join(f'{k}={v}' for k,v in sorted(counts.items())))
        print('Semantic descriptions/reachability/gating require independent review; not mechanically proved.')
        return 0
    except Invalid as e:
        print(f'sim_capabilities: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
