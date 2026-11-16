#!/usr/bin/env python3
"""TRACE-COLS-1: the window trace (window_trace.cpp) and its reader (tools/bench/window_table.py)
agree on one column order, for frames.tsv, sim.tsv and wheel.tsv. Reads the header line straight
out of the C++ source (the ostringstream `<<` statement that builds it, string literals
concatenated and tab/newline-escaped the way the C++ source spells them) rather than a second,
hand-maintained copy of it, so a column added to one side and not the other fails here instead of
only when window_table.py next refuses a real trace."""
import pathlib
import re
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
TRACE_CPP = REPO / "src" / "app" / "src" / "window_trace.cpp"
sys.path.insert(0, str(REPO / "tools" / "bench"))
import window_table  # noqa: E402


def header_of(source, stream_var, needle):
    """The header line of the `stream_var << "..." "...";` statement containing `needle`, with
    the C++ source's own `\\t`/`\\n` escapes decoded to real tab/newline characters."""
    start = source.index(needle)
    stmt_start = source.rindex(f"{stream_var} << \"", 0, start)
    stmt_end = source.index(";", start)
    segment = source[stmt_start:stmt_end]
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', segment)
    if not literals:
        raise AssertionError(f"no string literal found for {stream_var} << ... containing {needle!r}")
    raw = "".join(literals)
    return raw.replace("\\t", "\t").replace("\\n", "\n")


class ColumnsContract(unittest.TestCase):
    def setUp(self):
        self.source = TRACE_CPP.read_text(encoding="utf-8")

    def test_frames_header_matches_the_writer(self):
        header = header_of(self.source, "f", "frame\\tepoch")
        self.assertEqual(header.rstrip("\n").split("\t"), window_table.COLUMNS)

    def test_sim_header_matches_the_writer(self):
        header = header_of(self.source, "t", "turn\\tt_ms")
        self.assertEqual(header.rstrip("\n").split("\t"), window_table.SIM_COLUMNS)

    def test_wheel_header_matches_the_writer(self):
        header = header_of(self.source, "w", "t_ms\\traw_y")
        self.assertEqual(header.rstrip("\n").split("\t"), window_table.WHEEL_COLUMNS)


if __name__ == "__main__":
    unittest.main()
