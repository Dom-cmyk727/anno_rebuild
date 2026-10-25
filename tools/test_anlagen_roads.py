"""Synthetic checks for the T30b.3n static prediction helper; no original files."""
import sys
import struct
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from anlagen_roads import walk_code, footprint_mask


def head(first, shape, flags=0):
    data = bytearray(56)
    data[0x2f:0x31] = bytes((first, shape))
    struct.pack_into('<H', data, 0x1c, flags)
    return data


class WalkPredictionTests(unittest.TestCase):
    def test_head_code_and_flags_are_independent(self):
        self.assertEqual(walk_code(head(0, 0), 0), 4)
        for flags in (0x1186, 0x4186):
            for rotation in range(4):
                self.assertEqual(walk_code(head(0, 3, flags), rotation), 0)
        self.assertEqual(walk_code(head(4, 3), 0), 4)

    def test_connection_rotations(self):
        cases = {
            (10, 0): (0xd2, 0xaa, 0xd2, 0xaa),
            (8, 1): (0xc8, 0x98, 0xb0, 0xe0),
            (8, 2): (0xd8, 0xb8, 0xf0, 0xe8),
            (8, 3): (0xf8, 0xf8, 0xf8, 0xf8),
            (8, 4): (0x90, 0xa0, 0xc0, 0x88),
        }
        for inputs, expected in cases.items():
            self.assertEqual(tuple(walk_code(head(*inputs), r) for r in range(4)), expected)

    def test_non_square_footprint_rotates_one_blocked_corner(self):
        # Own synthetic 2x1 footprint: one bit in its first of three subcell rows.
        rows = struct.pack('<3I', 1, 0, 0)
        expected = ((0, 0, 1), (0, 1, 64), (1, 0, 256), (0, 0, 4))
        for rotation, (x, y, mask) in enumerate(expected):
            width, height = ((1, 2) if rotation & 1 else (2, 1))
            result = {(dx, dy): footprint_mask(rows, 2, 1, rotation, dx, dy)
                      for dy in range(height) for dx in range(width)}
            self.assertEqual(result[x, y], mask)
            self.assertEqual(sum(v.bit_count() for v in result.values()), 1)


if __name__ == '__main__':
    unittest.main()
