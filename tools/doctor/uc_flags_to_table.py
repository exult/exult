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

"""Convert usecode flag headers into an Exult global_flags.txt table.

Reads 'const int NAME = N;  // text' declarations and enum members
'NAME = N,  // text' (decimal or 0x hex) from one or more UCC headers and
writes a Text_msg_file table: the global section holds '0xN:NAME' lines and
an optional '%%section descriptions' block holds '0xN:text' lines.

With --merge-into FILE the existing table is kept line for line: names are
only added for numbers missing from its global section, and the trailing
descriptions block is written or replaced (existing descriptions are kept).
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

SECTION_START = "%%section descriptions"
SECTION_END = "%%endsection descriptions"

_NUMBER = r"(0[xX][0-9A-Fa-f]+|\d+)"
_CONST_RE = re.compile(r"^\s*const\s+int\s+([A-Za-z_]\w*)\s*=\s*" + _NUMBER + r"\s*;")
_MEMBER_RE = re.compile(r"([A-Za-z_]\w*)\s*=\s*" + _NUMBER + r"\s*(?:,|$)")
_ENUM_START_RE = re.compile(r"^\s*enum\b[^{]*(\{)?")
_TABLE_LINE_RE = re.compile(r"^\s*(0[xX][0-9A-Fa-f]+|\d+)\s*:(.*)$")


@dataclass
class Flag:
    """One named flag read from a header."""

    num: int
    name: str
    description: str = ""


def _strip_block_comments(text: str) -> str:
    """Remove /* */ comments, keeping the newlines they span."""
    return re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)


def _split_comment(line: str) -> tuple[str, str]:
    """Split a line into code and the text of a trailing // comment."""
    pos = line.find("//")
    if pos < 0:
        return line, ""
    return line[:pos], line[pos + 2 :].strip()


def _clean_description(text: str) -> str:
    """Drop comments that only restate the number; capitalise the rest."""
    text = " ".join(text.split())
    if _NUMBER_ONLY_RE.match(text):
        return ""
    return text[:1].upper() + text[1:]


_NUMBER_ONLY_RE = re.compile(r"^(0[xX])?[0-9A-Fa-f]+;?$")


def parse_header(text: str) -> list[Flag]:
    """Return the flags declared in a UCC header, in source order."""
    flags: list[Flag] = []
    in_enum = False
    pending_enum = False  # 'enum X' seen, '{' still to come
    last_described: Flag | None = None  # may continue on comment-only lines
    for raw in _strip_block_comments(text).splitlines():
        code, comment = _split_comment(raw)
        if not code.strip():
            # A comment-only line continues an unfinished trailing comment.
            if comment and last_described and not last_described.description.endswith((".", "!", "?", ")")):
                joiner = "" if last_described.description.endswith("-") else " "
                last_described.description += joiner + comment
            else:
                last_described = None
            continue
        last_described = None
        comment = _clean_description(comment)
        if not in_enum:
            match = _CONST_RE.match(code)
            if match:
                flags.append(Flag(int(match.group(2), 0), match.group(1), comment))
                last_described = flags[-1] if comment else None
                continue
            enum = _ENUM_START_RE.match(code)
            if enum:
                pending_enum = True
                code = code[enum.end() :] if enum.group(1) else ""
                if enum.group(1):
                    pending_enum, in_enum = False, True
            elif pending_enum and "{" in code:
                pending_enum, in_enum = False, True
                code = code[code.index("{") + 1 :]
            if not in_enum:
                continue
        closing = code.find("}")
        body = code if closing < 0 else code[:closing]
        members = list(_MEMBER_RE.finditer(body))
        for member in members:
            # A trailing comment describes the member only when it is alone.
            desc = comment if len(members) == 1 else ""
            flags.append(Flag(int(member.group(2), 0), member.group(1), desc))
            last_described = flags[-1] if desc else None
        if closing >= 0:
            in_enum = False
    return flags


def parse_range(value: str) -> tuple[int, int]:
    """Parse 'LO-HI' (decimal or 0x hex, inclusive)."""
    try:
        lo, hi = (int(part, 0) for part in value.split("-", 1))
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"bad range '{value}', expected LO-HI") from exc
    if lo > hi:
        raise argparse.ArgumentTypeError(f"bad range '{value}': LO > HI")
    return lo, hi


def collect(flags: list[Flag], num_range: tuple[int, int] | None) -> tuple[dict[int, str], dict[int, str]]:
    """Return (names, descriptions) by number; the first name for a number wins."""
    names: dict[int, str] = {}
    descs: dict[int, str] = {}
    for flag in flags:
        if num_range and not num_range[0] <= flag.num <= num_range[1]:
            continue
        names.setdefault(flag.num, flag.name)
        if flag.description and flag.num not in descs:
            descs[flag.num] = " ".join(flag.description.split())
    return names, descs


def _parse_index(text: str) -> int:
    """Parse a table index as Text_msg_file_reader does (0x hex, 0 octal, decimal)."""
    if text[:2].lower() == "0x":
        return int(text, 16)
    if len(text) > 1 and text.startswith("0"):
        return int(text, 8)
    return int(text)


def _descriptions_block(descs: dict[int, str]) -> list[str]:
    lines = [SECTION_START]
    lines += [f"0x{num:X}:{text}" for num, text in sorted(descs.items())]
    lines.append(SECTION_END)
    return lines


def render_table(names: dict[int, str], descs: dict[int, str], sources: list[str]) -> str:
    """Render a fresh table."""
    lines = [f"# Generated by tools/doctor/uc_flags_to_table.py from {', '.join(sources)}."]
    lines += [f"0x{num:X}:{name}" for num, name in sorted(names.items())]
    if descs:
        lines += _descriptions_block(descs)
    return "\n".join(lines) + "\n"


def merge_table(existing: str, names: dict[int, str], descs: dict[int, str], sources: list[str]) -> str:
    """Merge names and descriptions into an existing table, only adding."""
    lines = existing.splitlines()
    # Split off a trailing descriptions block, if any.
    old_descs: dict[int, str] = {}
    start = next((i for i, line in enumerate(lines) if line.strip() == SECTION_START), None)
    if start is not None:
        end = next(
            (i for i in range(start + 1, len(lines)) if lines[i].strip().startswith("%%endsection")),
            len(lines) - 1,
        )
        for line in lines[start + 1 : end]:
            match = _TABLE_LINE_RE.match(line)
            if match:
                old_descs[_parse_index(match.group(1))] = match.group(2)
        tail = lines[end + 1 :]
        lines = lines[:start]
        if any(line.strip() for line in tail):
            # Keep anything after the block before the new block.
            lines += tail
    while lines and not lines[-1].strip():
        lines.pop()

    # Global section: every numbered line outside a %%section block.
    present: set[int] = set()
    in_section = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("%%section"):
            in_section = True
        elif stripped.startswith("%%endsection"):
            in_section = False
        elif not in_section:
            match = _TABLE_LINE_RE.match(stripped)
            if match:
                present.add(_parse_index(match.group(1)))

    missing = {num: name for num, name in names.items() if num not in present}
    if missing:
        lines.append(f"# Added by tools/doctor/uc_flags_to_table.py from {', '.join(sources)}.")
        lines += [f"0x{num:X}:{name}" for num, name in sorted(missing.items())]

    merged = dict(old_descs)
    for num, text in descs.items():
        merged.setdefault(num, text)
    if merged:
        lines += _descriptions_block(merged)
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("headers", nargs="+", type=Path, help="UCC header files to read")
    out = parser.add_mutually_exclusive_group(required=True)
    out.add_argument("-o", "--output", type=Path, help="write a fresh table to OUT ('-' for stdout)")
    out.add_argument("--merge-into", type=Path, metavar="FILE", help="merge into an existing table in place")
    parser.add_argument("--range", type=parse_range, metavar="LO-HI", help="keep only flags LO..HI (inclusive)")
    args = parser.parse_args(argv)

    flags: list[Flag] = []
    try:
        for header in args.headers:
            flags += parse_header(header.read_text(encoding="utf-8", errors="replace"))
        names, descs = collect(flags, args.range)
        sources = [header.name for header in args.headers]
        if args.merge_into:
            existing = args.merge_into.read_text(encoding="utf-8")
            args.merge_into.write_text(merge_table(existing, names, descs, sources), encoding="utf-8")
        else:
            table = render_table(names, descs, sources)
            if str(args.output) == "-":
                sys.stdout.write(table)
            else:
                args.output.write_text(table, encoding="utf-8")
    except OSError as exc:
        print(f"uc_flags_to_table: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
