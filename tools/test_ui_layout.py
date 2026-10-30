"""Synthetic checks for tools/ui_layout.py. No original files required.

  python -I tools/test_ui_layout.py
"""
from __future__ import annotations

import os
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

    def test_ui1_files_have_no_control_bytes_or_ragged_tables(self) -> None:
        errs = ui_layout.check_ui1_repo(str(ROOT))
        self.assertEqual(errs, [])

    def test_control_bytes_refuse_tab_and_bare_cr(self) -> None:
        self.assertEqual(ui_layout.control_byte_errors(b"ok\n", "x"), [])
        self.assertEqual(ui_layout.control_byte_errors(b"ok\r\n", "x"), [])
        self.assertTrue(ui_layout.control_byte_errors(b"ok" + bytes([9]) + b"x", "x"))
        self.assertTrue(ui_layout.control_byte_errors(b"ok\rx", "x"))

    def test_markdown_tables_need_matching_columns(self) -> None:
        good = "| a | b |\n|---|---|\n| 1 | 2 |\n"
        bad = "| a | b |\n|---|---|\n| 1 |\n"
        self.assertEqual(ui_layout.markdown_table_errors(good, "t"), [])
        self.assertTrue(ui_layout.markdown_table_errors(bad, "t"))

    def test_compare_images_respects_mask(self) -> None:
        from PIL import Image

        mock = Image.new("RGB", (4, 4), (1, 2, 3))
        shot = mock.copy()
        shot.putpixel((2, 2), (200, 0, 0))
        rec = ui_layout.compare_images(mock, shot, masked=set())
        self.assertGreater(rec["bad"], 0)
        rec2 = ui_layout.compare_images(mock, shot, masked={(2, 2)})
        self.assertEqual(rec2["bad"], 0)

    def test_cell_icon_tex_id_formula(self) -> None:
        self.assertEqual(ui_layout.cell_icon_tex_id(154, 0, 0), 154)
        self.assertEqual(ui_layout.cell_icon_tex_id(154, 2, 0), 160)
        self.assertEqual(ui_layout.cell_icon_tex_id(154, 3, 1), 175)
        self.assertEqual(ui_layout.cell_icon_tex_id(277, 0, 1), 289)

    def test_negative_controls_fail_when_shots_present(self) -> None:
        root = ROOT
        shots = root / "cache" / "orig" / "ui-1" / "shots"
        game = os.environ.get("ANNO1503_GAME_DIR", r"E:\anno1503\game\app")
        pub = shots / "r2_construction_public.png"
        roads = shots / "r2_construction_roads.png"
        if not pub.is_file() or not roads.is_file():
            self.skipTest("Wine construction shots not on this machine")
        if not os.path.isfile(os.path.join(game, "ToolGfx", "fenster.tex")):
            self.skipTest("game files not present")
        cases = ui_layout.negative_control_results(
            str(root / "data" / "ui"), game, str(shots)
        )
        self.assertEqual(len(cases), 3)
        for rec in cases:
            self.assertFalse(
                rec["pass"],
                f"{rec['name']} fraction {rec['fraction']:.4f} should fail",
            )

    def test_validate_against_game_missing_tex(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            ui_layout.write_lines(
                os.path.join(td, "hud.txt"),
                [
                    "anno1503-ui-hud 1",
                    "resolution 1024 768",
                    "element statusbar pos 0 733 size 746 35 file toolgfx/statusbar.tex access STATUSBAR tex_id 0",
                ],
            )
            cats = [
                f"category {k} index {i} y 1 banner_key {k} tooltip_section - shot r2"
                for i, k in enumerate(ui_layout.CATEGORY_KEYS, 1)
            ]
            ui_layout.write_lines(
                os.path.join(td, "construction.txt"),
                [
                    "anno1503-ui-construction 1",
                    "resolution 1024 768",
                    "grid_click_x 833 885 937 989",
                    "grid_click_y 330 382 432 484 536 588 640 692",
                    *cats,
                ],
            )
            gdir = os.path.join(td, "g")
            os.makedirs(gdir)
            errs = ui_layout.validate_against_game(td, gdir)
            self.assertTrue(any("missing tex" in e for e in errs), errs)

    def test_non_ascii_is_refused(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "x.txt"
            path.write_bytes(b"anno1503-ui-hud 1\n\xff\n")
            with self.assertRaises(ui_layout.LayoutError):
                ui_layout.layout_lines(str(path))


if __name__ == "__main__":
    unittest.main()
