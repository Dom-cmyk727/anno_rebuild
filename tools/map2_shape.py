"""Shared geometry for the MAP.2 candidate profile.

Land is a tile whose HEIGHT byte is >= 16. That is the fixed-map tool's
count (`tools/fixed_map.py` `_land_count`) and the render waterline
(`anno/render/iso.hpp` `kWaterline` = 16). It is not the original land
classifier, which uses an average above 19 (`docs/render/iso.md`, AnnoWelt
`0x100900c0`).

No original bytes and no file I/O.
"""

# Climate bands on the 1024-edge reference `03`, world-scaling.md §3.
# The second number is the published "y through", fed to §7 as y1.
BANDS_1024 = (
    (0, 'polar', 16, 216),
    (1, 'tundra', 88, 352),
    (2, 'nord', 32, 624),
    (3, 'praerie', 120, 792),
    (4, 'dschungel', 400, 984),
    (5, 'steppe', 168, 960),
)

CLIMATE_NAME = ('polar', 'tundra', 'nord', 'praerie', 'dschungel', 'steppe', 'wueste')
PEOPLE_MARKERS = ('MONGOLEN', 'PIRATEN', 'AZTEKEN', 'AFRIKA', 'SUEDSEE')
LAND_HEIGHT = 16
LARGE = 256


def isqrt(n):
    """Floor of the integer square root. n >= 0."""
    if n <= 0:
        return 0
    x = 1 << ((n.bit_length() + 1) // 2)
    while True:
        y = (x + n // x) // 2
        if y >= x:
            return x
        x = y


def land_count(height):
    return sum(1 for value in height if value >= LAND_HEIGHT)


def people_marker(name):
    upper = name.upper()
    for marker in PEOPLE_MARKERS:
        if marker in upper:
            return marker
    return None


def scaled_band(y0_1024, y1_1024, edge):
    """world-scaling.md §7. The returned y1 is exclusive."""
    y0 = (y0_1024 * edge // 1024) & ~7
    y1 = (y1_1024 * edge // 1024) & ~7
    if y1 <= y0:
        y1 = y0 + 8
    return y0, y1


def bands_for(edge):
    return tuple(
        (climate, name, *scaled_band(y0, y1, edge))
        for climate, name, y0, y1 in BANDS_1024
    )


def north_centre_ok(origin_y, height, edge):
    """Centre row is on or above the middle. Integer: 2 * centre <= edge."""
    return 2 * (origin_y + height // 2) <= edge


def south_top_ok(origin_y, edge):
    """Top row is on or below the middle. Integer: 2 * top >= edge."""
    return 2 * origin_y >= edge


def rot90(buf, n):
    """Quarter turn clockwise. buf is row-major, length n*n."""
    out = bytearray(n * n)
    for y in range(n):
        row = y * n
        for x in range(n):
            nx = n - 1 - y
            ny = x
            out[ny * n + nx] = buf[row + x]
    return bytes(out)


def reflect_x(buf, n):
    """Mirror left to right."""
    out = bytearray(n * n)
    for y in range(n):
        row = y * n
        src = buf[row:row + n]
        out[row:row + n] = src[::-1]
    return bytes(out)


def dihedral(buf, n):
    """Eight orientations: four turns of the grid and of its left-right mirror."""
    found = []
    cur = bytes(buf)
    for _ in range(4):
        found.append(cur)
        found.append(reflect_x(cur, n))
        cur = rot90(cur, n)
    return found


def canonical(buf, n):
    return min(dihedral(buf, n))


def land_mask(height, n):
    return bytes(1 if value >= LAND_HEIGHT else 0 for value in height)


def shape_ids(height, n):
    """Canonical land mask and the height grid in that same orientation.

    The height orientation is the one that made the land mask lexicographically
    smallest, so two copies report the same height hash when the relief matches
    and a different one when only the coastline matches.
    """
    mask = land_mask(height, n)
    best = None
    best_height = None
    grids = dihedral(height, n)
    masks = dihedral(mask, n)
    for oriented_mask, oriented_height in zip(masks, grids):
        if best is None or oriented_mask < best:
            best = oriented_mask
            best_height = oriented_height
    return best, best_height


def sha256_hex(buf):
    import hashlib
    return hashlib.sha256(buf).hexdigest()


def within_mean(values, permille):
    """Each value is within ±permille/1000 of the mean, ties included.

    Compared without division: |n*v - sum| * 1000 <= permille * sum.
    An empty list does not pass. All zeros pass (they are equal). A mix of
    zeros and non-zeros cannot have a zero sum, so no extra branch is required
    for non-negative measures.
    """
    if not values:
        return False
    total = sum(values)
    n = len(values)
    limit = permille * total
    for value in values:
        if abs(n * value - total) * 1000 > limit:
            return False
    return True


def worst_permille(values):
    """Smallest integer permille that would pass, or None when undefined.

    Ceiling of max |v - mean| / mean * 1000. Zero when every value is 0.
    None when the list is empty. Non-negative values only.
    """
    if not values:
        return None
    total = sum(values)
    n = len(values)
    if total == 0:
        return 0
    worst = max(abs(n * value - total) for value in values)
    return (worst * 1000 + total - 1) // total


def point_tile_dist2(px, py, tx, ty):
    """Squared distance from an integer point to the unit square [tx, tx+1]²."""
    if px < tx:
        cx = tx
    elif px > tx + 1:
        cx = tx + 1
    else:
        cx = px
    if py < ty:
        cy = ty
    elif py > ty + 1:
        cy = ty + 1
    else:
        cy = py
    dx = px - cx
    dy = py - cy
    return dx * dx + dy * dy


def milli_of(dist2):
    """floor(euclidean * 1000) via isqrt(dist2 * 1000000)."""
    return isqrt(dist2 * 1000000)


def edge_gap(a, b):
    """Separation of two rectangles (x, y, w, h).

    None when the rectangles share a tile, 0 when they only touch, otherwise
    the floored Euclidean gap in tiles. An axial gap is exact. A corner gap
    is isqrt(dx^2 + dy^2).
    """
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    sep_x = max(ax, bx) - min(ax + aw, bx + bw)
    sep_y = max(ay, by) - min(ay + ah, by + bh)
    if sep_x < 0 and sep_y < 0:
        return None
    dx = sep_x if sep_x > 0 else 0
    dy = sep_y if sep_y > 0 else 0
    if dx == 0 and dy == 0:
        return 0
    return isqrt(dx * dx + dy * dy)


def rect_overlap(a, b):
    return edge_gap(a, b) is None
