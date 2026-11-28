#!/usr/bin/env python3
#
# Copyright (C) 2026 The Exult Team
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Unit tests for doctor_dump_diff.py."""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import doctor_dump_diff as dd  # noqa: E402

BEFORE = """\
# exult-doctor-dump 1
meta.exult: Exult version 1.13.1git
clock: day 1 09:00
avatar: 100,200,0 map 0
flag.0x0003: 1 STARTING_SPEECH
npc.0001: alive | Iolo | 1,2,0 map 0 | follow_avatar | good | in party | hp 20/20 | items 3
"""

AFTER = """\
# exult-doctor-dump 1
meta.exult: Exult version 1.13.2git
clock: day 2 10:30
avatar: 100,200,0 map 0
flag.0x0004: 1 BANES_RELEASED
npc.0001: dead | Iolo | body 5,6,0 map 0 | wait | good | - | hp 0/20 | items 0
"""


class DiffTests(unittest.TestCase):
    def test_parse_ignores_comments_and_blank_lines(self) -> None:
        facts = dd.parse_dump("# c\n\nkey: a: b\n")
        self.assertEqual(facts, {"key": "a: b"})

    def test_parse_rejects_bad_line(self) -> None:
        with self.assertRaises(ValueError):
            dd.parse_dump("no separator here\n")

    def test_diff_default_ignore(self) -> None:
        lines = dd.diff_dumps(dd.parse_dump(BEFORE), dd.parse_dump(AFTER), dd.DEFAULT_IGNORE)
        self.assertEqual(
            lines,
            [
                "- flag.0x0003: 1 STARTING_SPEECH",
                "+ flag.0x0004: 1 BANES_RELEASED",
                "~ npc.0001: alive | Iolo | 1,2,0 map 0 | follow_avatar | good | in party | hp 20/20 | items 3"
                " -> dead | Iolo | body 5,6,0 map 0 | wait | good | - | hp 0/20 | items 0",
            ],
        )

    def test_same_dump_is_empty(self) -> None:
        self.assertEqual(dd.diff_dumps(dd.parse_dump(BEFORE), dd.parse_dump(BEFORE), dd.DEFAULT_IGNORE), [])

    def run_main(self, before: str, after: str, *extra: str) -> tuple[int, str]:
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp, "a.txt"), Path(tmp, "b.txt")
            a.write_text(before, encoding="utf-8")
            b.write_text(after, encoding="utf-8")
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
                status = dd.main([str(a), str(b), *extra])
            return status, out.getvalue()

    def test_exit_status(self) -> None:
        self.assertEqual(self.run_main(BEFORE, BEFORE)[0], 0)
        self.assertEqual(self.run_main(BEFORE, AFTER)[0], 1)
        self.assertEqual(self.run_main(BEFORE, "bad line\n")[0], 2)

    def test_volatile_keys_only_differ(self) -> None:
        after = BEFORE.replace("day 1 09:00", "day 3 01:00").replace("1.13.1git", "x")
        self.assertEqual(self.run_main(BEFORE, after), (0, ""))
        status, out = self.run_main(BEFORE, after, "--all")
        self.assertEqual(status, 1)
        self.assertIn("~ clock: day 1 09:00 -> day 3 01:00", out)

    def test_custom_ignore(self) -> None:
        status, out = self.run_main(BEFORE, AFTER, "--ignore", "npc.", "--ignore", "flag.")
        self.assertEqual(status, 1)
        self.assertIn("~ clock:", out)
        self.assertNotIn("npc.0001", out)

    def test_missing_file(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(dd.main(["/nonexistent/a", "/nonexistent/b"]), 2)


if __name__ == "__main__":
    unittest.main()
