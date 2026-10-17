"""Reference reader for ANNO 1503 plain-text data files: the keyed files in
Data/ (BGruppen.dat, Wachstum.dat, ...) and Texte.dat.
Independent of crates/formats; spec: docs/formats/data-text.md.

  text.py keyed <file>...              same output as `anno-dump keyed`
  text.py texte [--full] <Texte.dat>... same output as `anno-dump texte [--full]`
"""
import re, sys

KEY = re.compile(rb'([A-Za-z_][A-Za-z0-9_]*)[ \t]*:')

def fnv64(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'

def quote(raw):
    return '"' + ''.join(chr(b) if 0x20 <= b <= 0x7e and b not in (0x22, 0x5c) else f'\\x{b:02x}'
                         for b in raw) + '"'

def lines(buf):
    """-> [(number, text, ending)] with ending 'crlf', 'lf' or None (last line)."""
    out = []
    parts = buf.split(b'\n')
    for k, p in enumerate(parts):
        last = k == len(parts) - 1
        if last:
            out.append((k + 1, p, None))
        elif p.endswith(b'\r'):
            out.append((k + 1, p[:-1], 'crlf'))
        else:
            out.append((k + 1, p, 'lf'))
    return out

def strip_comment(t):
    quoted = False
    for i, c in enumerate(t):
        if c == 0x22: quoted = not quoted
        elif c == 0x3b and not quoted: return t[:i]
    return t

def kind(t):
    """-> ('blank',) | ('section', name) | ('entry', key, value) | ('other', code)"""
    code = strip_comment(t).strip(b' \t')
    if not code: return ('blank',)
    if code.startswith(b'[') and code.endswith(b']') and len(code) > 2 and b']' not in code[1:-1]:
        return ('section', code[1:-1])
    m = KEY.match(code)
    if m: return ('entry', m.group(1), code[m.end():].strip(b' \t'))
    return ('other', code)

def fields(value):
    if not value: return []
    out, start, quoted = [], 0, False
    for i, c in enumerate(value):
        if c == 0x22: quoted = not quoted
        elif c == 0x2c and not quoted:
            out.append(value[start:i].strip(b' \t')); start = i + 1
    out.append(value[start:].strip(b' \t'))
    return out

def summary(ls):
    crlf = sum(1 for _, _, e in ls if e == 'crlf')
    return f'LINES count={len(ls)} crlf={crlf} final={int(ls[-1][1] == b"")}'

def keyed(path):
    print(f'# {path}')
    buf = open(path, 'rb').read()
    ls = lines(buf)
    print(summary(ls))
    for n, t, _ in ls:
        k = kind(t)
        if k[0] == 'section': print(f'SECTION line={n} name={quote(k[1])}')
        elif k[0] == 'entry': print(f'ENTRY line={n} key={quote(k[1])} fields={len(fields(k[2]))} value={quote(k[2])}')
        elif k[0] == 'other': print(f'OTHER line={n} text={quote(k[1])}')
    return True

def header(t):
    if t.startswith(b'[') and t.endswith(b']') and len(t) > 2 and b']' not in t[1:-1]:
        return t[1:-1]
    return None

def texte(path, full):
    print(f'# {path}')
    buf = open(path, 'rb').read()
    ls = lines(buf)
    print(summary(ls))
    body = ls[:-1] if ls[-1][1] == b'' and ls[-1][2] is None else ls
    preamble, sections = [], []
    for n, t, _ in body:
        h = header(t)
        if h is not None: sections.append((n, h, []))
        elif sections: sections[-1][2].append(t)
        else: preamble.append(t)
    print(f'TEXTE sections={len(sections)} preamble={len(preamble)}')
    ok = True
    for n, name, texts in sections:
        h = fnv64(b''.join(t + b'\n' for t in texts))
        print(f'SECTION line={n} name={quote(name)} lines={len(texts)} hash={h}')
        if name == b'ANLAGEN':
            for t in texts:
                m = re.fullmatch(rb'(\d+):(.*)', t, re.S)
                if m: print(f'  ID id={int(m.group(1))} text={quote(m.group(2))}')
                else: print(f'  ID BAD text={quote(t)}'); ok = False
        if full:
            for t in texts: print(f'  L {quote(t)}')
    return ok

if __name__ == '__main__':
    sys.stdout.reconfigure(newline='\n')  # same bytes as anno-dump on Windows too
    cmd, args = sys.argv[1:2], sys.argv[2:]
    full = args[:1] == ['--full']
    files = args[1:] if full else args
    if cmd == ['keyed'] and files and not full:
        sys.exit(0 if all([keyed(f) for f in files]) else 1)
    if cmd == ['texte'] and files:
        sys.exit(0 if all([texte(f, full) for f in files]) else 1)
    print(__doc__, file=sys.stderr); sys.exit(2)
