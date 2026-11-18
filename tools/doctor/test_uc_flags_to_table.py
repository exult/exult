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

"""Unit tests for uc_flags_to_table.py."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import uc_flags_to_table as conv  # noqa: E402

HEADER = """\
/*
 * const int IN_COMMENT = 99;
 */
const int STARTING_SPEECH = 0x3;
const int CURED = 71;  // Cantra is cured.
enum Temporary_flags {
	// Temporary flags are used for many things
	TEMP_FLAG_1 = 0x7,
	TEMP_FLAG_2 = 8,  // second one
	TEMP_FLAG_4 = 0xA
};
enum Split
{
	SPLIT_A = 0x10, SPLIT_B = 0x11,  // shared comment
	LAST = 1500  /* block */ // last one
};
enum More {
	WRAPPED = 0x20,  // I am Iolo! Bane of shooting-the-
	// Avatar-through-the-heart
	// with the crossbow!
	RESTATED = 256,  //0x100;
	DONE = 0x21  // done.
	// Not a continuation.
};
var not_a_flag = 5;
"""


class ParseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.flags = {f.name: f for f in conv.parse_header(HEADER)}

    def test_const(self) -> None:
        self.assertEqual(self.flags["STARTING_SPEECH"].num, 3)
        self.assertEqual(self.flags["CURED"].description, "Cantra is cured.")

    def test_enum_hex_and_decimal(self) -> None:
        self.assertEqual(self.flags["TEMP_FLAG_1"].num, 7)
        self.assertEqual(self.flags["TEMP_FLAG_2"].num, 8)
        self.assertEqual(self.flags["TEMP_FLAG_4"].num, 10)
        self.assertEqual(self.flags["LAST"].num, 1500)

    def test_comments(self) -> None:
        self.assertNotIn("IN_COMMENT", self.flags)
        self.assertNotIn("not_a_flag", self.flags)
        self.assertEqual(self.flags["TEMP_FLAG_2"].description, "Second one")
        self.assertEqual(self.flags["TEMP_FLAG_1"].description, "")
        self.assertEqual(self.flags["LAST"].description, "Last one")
        # A comment shared by two members on one line describes neither.
        self.assertEqual(self.flags["SPLIT_A"].description, "")
        self.assertEqual(self.flags["SPLIT_B"].num, 0x11)

    def test_description_cleanup(self) -> None:
        self.assertEqual(
            self.flags["WRAPPED"].description,
            "I am Iolo! Bane of shooting-the-Avatar-through-the-heart with the crossbow!",
        )
        self.assertEqual(self.flags["RESTATED"].description, "")
        self.assertEqual(self.flags["DONE"].description, "Done.")

    def test_range_filter(self) -> None:
        names, descs = conv.collect(conv.parse_header(HEADER), conv.parse_range("1500-1999"))
        self.assertEqual(names, {1500: "LAST"})
        self.assertEqual(descs, {1500: "Last one"})

    def test_first_name_wins(self) -> None:
        names, _ = conv.collect(conv.parse_header("const int A = 1;\nconst int B = 1;\n"), None)
        self.assertEqual(names, {1: "A"})


class MergeTests(unittest.TestCase):
    EXISTING = "# curated\n0x3:CURATED_SPEECH\n#0x7\tnot-in-usecode\n010:OCTAL_EIGHT\n"

    def merged(self, existing: str | None = None) -> str:
        names, descs = conv.collect(conv.parse_header(HEADER), None)
        return conv.merge_table(existing or self.EXISTING, names, descs, ["test.uc"])

    def test_merge_keeps_lines(self) -> None:
        out = self.merged().splitlines()
        self.assertEqual(out[:4], self.EXISTING.splitlines())
        self.assertNotIn("0x3:STARTING_SPEECH", out)  # curated name kept
        self.assertNotIn("0x8:TEMP_FLAG_2", out)  # 010 octal is 8
        self.assertIn("0x7:TEMP_FLAG_1", out)  # commented line is missing
        self.assertIn("0x47:CURED", out)

    def test_descriptions_section(self) -> None:
        out = self.merged().splitlines()
        start = out.index(conv.SECTION_START)
        self.assertEqual(out[-1], conv.SECTION_END)
        self.assertIn("0x47:Cantra is cured.", out[start:])
        self.assertIn("0x8:Second one", out[start:])

    def test_merge_is_idempotent_and_replaces_block(self) -> None:
        once = self.merged()
        twice = self.merged(once)
        self.assertEqual(once, twice)
        self.assertEqual(twice.count(conv.SECTION_START), 1)

    def test_existing_descriptions_kept(self) -> None:
        existing = self.EXISTING + "%%section descriptions\n0x47:Curated text\n%%endsection descriptions\n"
        out = self.merged(existing).splitlines()
        self.assertIn("0x47:Curated text", out)
        self.assertNotIn("0x47:Cantra is cured.", out)

    def test_main_output_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp, "flags.uc")
            header.write_text(HEADER, encoding="utf-8")
            out = Path(tmp, "out.txt")
            self.assertEqual(conv.main([str(header), "-o", str(out), "--range", "0-0x20"]), 0)
            text = out.read_text(encoding="utf-8").splitlines()
            self.assertTrue(text[0].startswith("#"))
            self.assertIn("0x3:STARTING_SPEECH", text)
            self.assertNotIn("0x5DC:LAST", text)


if __name__ == "__main__":
    unittest.main()
