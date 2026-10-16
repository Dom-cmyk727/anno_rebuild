"""Generic parser for Max Design 'ANNO' chunk files (1503).
Chunk = 12-byte NUL-terminated name (rest may be garbage) + uint32 LE payload size + payload.
A payload is treated as a container if it parses exactly into >=1 valid chunks."""
import struct, sys, re, collections
NAME = re.compile(rb'^[A-Z][A-Z0-9_]{1,11}$')
def name_of(b):
    n = b[:12].split(b'\0', 1)[0]
    return n.decode() if NAME.match(n) else None
def parse(buf, off=0, end=None, depth=0, maxdepth=6):
    end = len(buf) if end is None else end
    out = []
    while off < end:
        if end - off < 16: return None
        n = name_of(buf[off:off+12]); sz = struct.unpack_from('<I', buf, off+12)[0]
        if n is None or off + 16 + sz > end: return None
        kids = parse(buf, off+16, off+16+sz, depth+1) if depth < maxdepth and sz >= 16 else None
        out.append((n, off, sz, kids)); off += 16 + sz
    return out if off == end else None
def walk(tree, path='', acc=None):
    acc = collections.Counter() if acc is None else acc
    for n, off, sz, kids in tree or []:
        p = f'{path}/{n}'; acc[p] += 1
        if kids: walk(kids, p, acc)
    return acc
def parse_file(buf):
    """Whole file -> (tree, trailing_offset). Exact chunk sequence, else root chunk + trailing bytes
    (as in .scp). Mirrors anno_formats::chunk::parse_file."""
    t = parse(buf)
    if t: return t, len(buf)
    if len(buf) < 16 or name_of(buf[:12]) is None: return None, None
    end = 16 + struct.unpack_from('<I', buf, 12)[0]
    if end > len(buf): return None, None
    kids = parse(buf, 16, end, 1) if end - 16 >= 16 else None
    return [(name_of(buf[:12]), 0, end - 16, kids)], end
def tree_lines(tree, depth=0):
    for n, off, sz, kids in tree:
        yield f"{'  ' * depth}{n} off={off} size={sz}"
        if kids: yield from tree_lines(kids, depth + 1)
def print_tree(f):
    buf = open(f, 'rb').read(); t, end = parse_file(buf)
    print(f'# {f}')
    if t is None: print('NOPARSE'); return
    for line in tree_lines(t): print(line)
    if end < len(buf): print(f'TRAILING off={end} len={len(buf) - end}')
if __name__ == '__main__':
    # `chunks.py tree <file>...` prints the tree in the format of `anno-dump chunks`.
    if sys.argv[1:2] == ['tree']:
        for f in sys.argv[2:]: print_tree(f)
    else:
        for f in sys.argv[1:]:
            buf = open(f, 'rb').read(); t = parse(buf)
            print(f, 'OK' if t else 'NOPARSE')
