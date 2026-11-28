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

"""Compare two Exult game doctor state dumps.

A dump has one 'key: value' fact per line; lines starting with '#' are
comments. Prints '+ key: value' for added facts, '- key: value' for removed
ones and '~ key: old -> new' for changed ones, in key order. Keys starting
with an --ignore prefix are skipped (default: 'meta.' and 'clock').

Exit status: 0 when the dumps match, 1 when they differ, 2 on an error.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

DEFAULT_IGNORE = ("meta.", "clock")


def parse_dump(text: str) -> dict[str, str]:
    """Return the facts of a dump by key."""
    facts: dict[str, str] = {}
    for line in text.splitlines():
        line = line.rstrip("\r")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, sep, value = line.partition(":")
        if not sep:
            raise ValueError(f"not a 'key: value' line: {line!r}")
        facts[key.strip()] = value.strip()
    return facts


def diff_dumps(before: dict[str, str], after: dict[str, str], ignore: tuple[str, ...]) -> list[str]:
    """Return the difference lines, sorted by key."""
    lines: list[str] = []
    for key in sorted(set(before) | set(after)):
        if key.startswith(ignore):
            continue
        if key not in after:
            lines.append(f"- {key}: {before[key]}")
        elif key not in before:
            lines.append(f"+ {key}: {after[key]}")
        elif before[key] != after[key]:
            lines.append(f"~ {key}: {before[key]} -> {after[key]}")
    return lines


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument(
        "--ignore",
        action="append",
        metavar="PREFIX",
        help="skip keys starting with PREFIX (repeatable; replaces the default 'meta.' and 'clock')",
    )
    parser.add_argument("--all", action="store_true", help="compare every key, the volatile ones included")
    args = parser.parse_args(argv)
    ignore: tuple[str, ...] = () if args.all else tuple(args.ignore) if args.ignore else DEFAULT_IGNORE
    try:
        before = parse_dump(args.before.read_text(encoding="utf-8", errors="replace"))
        after = parse_dump(args.after.read_text(encoding="utf-8", errors="replace"))
    except (OSError, ValueError) as exc:
        print(f"doctor_dump_diff: {exc}", file=sys.stderr)
        return 2
    lines = diff_dumps(before, after, ignore)
    for line in lines:
        print(line)
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main())
