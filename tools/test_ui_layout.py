"""Synthetic checks for tools/ui_layout.py. No original files required.

  python -I tools/test_ui_layout.py
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_layout


ROOT = Path(__file__).resolve().parents[1]


class SelfTest(unittest.TestCase):
    def test_self_test_command(self) -> None:
        self.assertEqual(ui_layout.self_test(), 0)

    def test_flag_alias_self_test(self) -> None:
        self.assertEqual(ui_layout.main(["--self-test"]), 0)

    def test_committed_layout_validates(self) -> None:
        layout = ROOT / "data" / "ui"
        if not (layout / "hud.txt").is_file():
            self.skipTest("data/ui/hud.txt missing")
        errs = ui_layout.validate_layout(str(layout))
        self.assertEqual(errs, [])

    def test_mock_names_statusbar_candidates(self) -> None:
        layout = ROOT / "data" / "ui"
        if not (layout / "hud.txt").is_file():
            self.skipTest("data/ui/hud.txt missing")
        lines = ui_layout.mock_lines(str(layout))
        joined = "\n".join(lines)
        self.assertIn("rect statusbar 0 733 746 35", joined)
        self.assertIn("candidate statusbar_fenster 0 725 780 43", joined)
        self.assertIn("candidate weltkarte 779 0 245 248", joined)

    def test_compare_idle_shot_when_present(self) -> None:
        shot = ROOT / "cache" / "orig" / "ui-1" / "shots" / "hud_idle_running.png"
        layout = ROOT / "data" / "ui"
        if not shot.is_file():
            self.skipTest("Wine HUD shot not on this machine")
        self.assertEqual(ui_layout.compare_shot(str(shot), str(layout)), [])

    def test_non_ascii_is_refused(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "x.txt"
            path.write_bytes(b"anno1503-ui-hud 1\n\xff\n")
            with self.assertRaises(ui_layout.LayoutError):
                ui_layout.layout_lines(str(path))


if __name__ == "__main__":
    unittest.main()
