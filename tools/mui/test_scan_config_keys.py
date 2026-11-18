#!/usr/bin/env python3
#  Copyright (C) 2026 The Exult Team
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
"""Unit tests for scan_config_keys.py.

  python3 -m unittest discover -s tools/mui -p 'test_*.py'
"""
from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import scan_config_keys as sck  # noqa: E402


def scan(source: str, rel: str = "game.cc") -> dict[str, sck.Found]:
    found: dict[str, sck.Found] = {}
    sck.scan_text(source, rel, found)
    return found


class ScanTests(unittest.TestCase):
    def test_literal_key_and_default(self) -> None:
        found = scan('config->value("config/gameplay/cheat", cheating, "no");\n')
        self.assertEqual(list(found), ["config/gameplay/cheat"])
        self.assertEqual(found["config/gameplay/cheat"].defaults, ["no"])
        self.assertEqual(found["config/gameplay/cheat"].ops, {"value"})
        self.assertEqual(found["config/gameplay/cheat"].origins, ["game.cc:1"])

    def test_bool_and_int_defaults(self) -> None:
        found = scan(
            'config->value("config/a/flag", b, true);\n'
            'config->value("config/a/count", n, 5);\n'
        )
        self.assertTrue(found["config/a/flag"].is_bool)
        self.assertEqual(found["config/a/flag"].defaults, ["yes"])
        self.assertEqual(found["config/a/count"].defaults, ["5"])

    def test_all_operations_and_multiline_call(self) -> None:
        found = scan(
            'config->set("config/a/x", 1, true);\n'
            "if (config->key_exists(\n"
            '        "config/a/y")) {\n'
            '    config->remove("config/a/z", false);\n'
            "}\n"
        )
        self.assertEqual(sorted(found), ["config/a/x", "config/a/y", "config/a/z"])
        self.assertEqual(found["config/a/y"].origins, ["game.cc:2"])

    def test_comments_are_ignored(self) -> None:
        found = scan(
            '// config->value("config/a/old", s, "x");\n'
            '/* config->set("config/a/older", s, false); */\n'
            'config->value("config/a/new", s, "url://x");\n'
        )
        self.assertEqual(list(found), ["config/a/new"])
        self.assertEqual(found["config/a/new"].defaults, ["url://x"])

    def test_concatenation_becomes_pattern(self) -> None:
        found = scan(
            'config->value("config/disk/game/" + title + "/keys", s, "(default)");\n'
            'config->value(std::string("config/audio/midi/volume_") + name, v, 100);\n'
        )
        self.assertEqual(sorted(found), ["config/audio/midi/volume_*", "config/disk/game/*/keys"])

    def test_variable_follows_last_assignment(self) -> None:
        found = scan(
            "std::string d;\n"
            'd = "config/disk/game/" + Game::get_gametitle() + "/keys";\n'
            'config->value(d.c_str(), s, "(default)");\n'
            'd = "config/disk/game/" + Game::get_gametitle() + "/autonotes";\n'
            'config->value(d, s, "(default)");\n'
        )
        self.assertEqual(sorted(found), ["config/disk/game/*/autonotes", "config/disk/game/*/keys"])

    def test_variable_chain_and_append(self) -> None:
        found = scan(
            'const string base_cfg_path("config/disk/game/" + cfgname);\n'
            'config_path = base_cfg_path + "/path";\n'
            "config->value(config_path.c_str(), p, def.c_str());\n"
            'std::string key = "config/audio/midi/";\n'
            "key += name;\n"
            "config->value(key, val, defaultval);\n"
        )
        self.assertEqual(sorted(found), ["config/audio/midi/*", "config/disk/game/*/path"])

    def test_ternary_gives_both_keys(self) -> None:
        found = scan(
            'const string vidStr(full ? "config/video" : "config/video/window");\n'
            'config->value(vidStr + "/scale", s, 2);\n'
        )
        self.assertEqual(sorted(found), ["config/video/scale", "config/video/window/scale"])

    def test_non_config_keys_are_skipped(self) -> None:
        found = scan('modconfig.value("mod_info/mod_title", t, "");\nconfig->value(key, v);\n')
        self.assertEqual(found, {})


class TableTests(unittest.TestCase):
    TABLE = (
        "# comment\n"
        "config/gameplay/cheat|bool|no||now|Allow cheats.\n"
        "config/disk/game/*/path|path|||restart|Game folder.\n"
    )

    def test_parse_and_cover(self) -> None:
        _, rows, errors = sck.parse_table(self.TABLE)
        self.assertEqual(errors, [])
        self.assertEqual(len(rows), 2)
        self.assertTrue(sck.covered("config/gameplay/cheat", rows))
        self.assertTrue(sck.covered("config/disk/game/*/path", rows))
        self.assertTrue(sck.covered("config/disk/game/blackgate/path", rows))
        self.assertFalse(sck.covered("config/disk/game/a/b/path", rows))
        self.assertFalse(sck.covered("config/gameplay/cheats", rows))

    def test_bad_rows_are_reported(self) -> None:
        _, _, errors = sck.parse_table(
            "config/a|colour|x||now|d\nconfig/b|enum|x||now|d\nconfig/c|int|1|1-5|later|d\nfoo\n"
        )
        self.assertEqual(len(errors), 5)


class CommandTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "gumps").mkdir()
        (self.root / "mapedit").mkdir()
        (self.root / "gumps" / "a.cc").write_text(
            'config->value("config/gameplay/cheat", c, "no");\n'
            'config->value("config/gameplay/new_key", n, 3);\n'
        )
        (self.root / "mapedit" / "studio.cc").write_text('config->value("config/estudio/x", s, "");\n')
        self.table = self.root / "keys.txt"
        self.table.write_text(TableTests.TABLE)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def run_main(self, *args: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = sck.main([*args, "--root", str(self.root)])
        return code, out.getvalue()

    def test_check_reports_missing_and_todo(self) -> None:
        code, out = self.run_main("--check", str(self.table))
        self.assertEqual(code, 1)
        self.assertIn("not in table: config/gameplay/new_key", out)
        self.assertNotIn("estudio", out)

    def test_update_keeps_rows_then_check_wants_descriptions(self) -> None:
        code, _ = self.run_main("--update", str(self.table))
        self.assertEqual(code, 0)
        text = self.table.read_text()
        self.assertTrue(text.startswith(TableTests.TABLE))
        self.assertIn("config/gameplay/new_key|int|3||restart|TODO", text)
        code, out = self.run_main("--check", str(self.table))
        self.assertEqual(code, 1)
        self.assertIn("no description: config/gameplay/new_key", out)
        self.table.write_text(text.replace("|TODO", "|A new key."))
        code, out = self.run_main("--check", str(self.table))
        self.assertEqual(code, 0, out)
        # A second update adds nothing.
        self.run_main("--update", str(self.table))
        self.assertEqual(self.table.read_text(), text.replace("|TODO", "|A new key."))


if __name__ == "__main__":
    unittest.main()
