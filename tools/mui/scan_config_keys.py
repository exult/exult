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
"""Keep data/mui_config_keys.txt in step with the exult.cfg keys in the code.

The table drives the Advanced tab of the themed Options window
(gumps/Mui_config_meta.cc). Each row is
  path|type|default|range-or-choices|apply|description
where '*' in a path stands for one dynamic path segment (or part of one).

The scanner walks the Exult sources (*.cc *.h *.cpp *.mm; Exult Studio,
the stand-alone tools and the Windows installer helper are left out) for
config->value(), config->set(), config->key_exists() and config->remove()
calls whose key is a string literal starting with "config/". Literal
defaults are captured. Keys built at run time ("config/a/" + name + "/b",
std::string("config/a/") + var) become patterns with '*'. A key held in a
local variable is followed back to its string assignment in the same file
when that is a literal or such a concatenation.

  scan_config_keys.py --list               keys found, with origin
  scan_config_keys.py --update TABLE       add new keys (description TODO),
                                           keep every existing row
  scan_config_keys.py --check [TABLE]      exit 1 if a key in the code has
                                           no row, or a row is still TODO

Paths default to the worktree that holds this script and to
data/mui_config_keys.txt in it. Needs only the python3 standard library.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TABLE = ROOT / "data" / "mui_config_keys.txt"
SOURCE_SUFFIXES = {".cc", ".h", ".cpp", ".mm"}
# Not part of the Exult engine: Exult Studio, the stand-alone tools, the
# Configuration test program and the Windows installer helper.
EXCLUDED_DIRS = ("mapedit/", "tools/", "msvcstuff/", "ios/Exult.xcodeproj/")
EXCLUDED_FILES = ("conf/xmain.cc", "win32/exconfig.cc")
OPS = ("value", "set", "key_exists", "remove")
TYPES = ("bool", "enum", "int", "float", "path", "string")
APPLIES = ("now", "restart")
TODO = "TODO"

CALL_RE = re.compile(r"\b\w*config\w*\s*(?:->|\.)\s*(" + "|".join(OPS) + r")\s*\(")
IDENT_RE = re.compile(r"[A-Za-z_]\w*")


@dataclass
class Found:
    """A config key found in the code."""

    key: str
    ops: set[str] = field(default_factory=set)
    defaults: list[str] = field(default_factory=list)
    origins: list[str] = field(default_factory=list)
    is_bool: bool = False


@dataclass
class Row:
    """One row of the metadata table."""

    path: str
    type: str
    default: str
    domain: str
    apply: str
    description: str

    def line(self) -> str:
        return "|".join(
            (self.path, self.type, self.default, self.domain, self.apply, self.description)
        )


def strip_comments(text: str) -> str:
    """Blank // and /* */ comments, keeping strings and line numbers."""
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end]))
            i = end
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i : j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def split_args(text: str, start: int) -> tuple[list[str], int]:
    """Split the call arguments that begin at text[start] (after '(')."""
    args: list[str] = []
    depth = 0
    cur: list[str] = []
    i = start
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            cur.append(text[i : j + 1])
            i = j + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                args.append("".join(cur).strip())
                return args, i
            depth -= 1
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    args.append("".join(cur).strip())
    return args, n


def split_top(expr: str, sep: str) -> list[str]:
    """Split expr at top-level occurrences of the one-character sep."""
    parts: list[str] = []
    depth = 0
    cur: list[str] = []
    i = 0
    while i < len(expr):
        c = expr[i]
        if c == '"':
            j = i + 1
            while j < len(expr) and expr[j] != '"':
                j += 2 if expr[j] == "\\" else 1
            cur.append(expr[i : j + 1])
            i = j + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur).strip())
    return parts


def unquote(literal: str) -> str:
    """Join adjacent C string literals and resolve simple escapes."""
    pieces = re.findall(r'"((?:[^"\\]|\\.)*)"', literal)
    return "".join(pieces).replace('\\"', '"').replace("\\\\", "\\")


def strip_wrappers(expr: str) -> str:
    """Drop .c_str(), std::string(...) and redundant parentheses."""
    expr = expr.strip()
    expr = re.sub(r"\.c_str\(\)$", "", expr).strip()
    while True:
        m = re.fullmatch(r"(?:std::)?string\s*\((.*)\)", expr, re.S)
        if m and split_top(m.group(1), ",")[0] == m.group(1).strip():
            expr = m.group(1).strip()
            continue
        if expr.startswith("(") and expr.endswith(")"):
            inner = expr[1:-1]
            if split_args(expr, 1)[1] == len(expr) - 1:
                expr = inner.strip()
                continue
        return expr


Variables = dict[str, list[tuple[int, str, bool]]]


def resolve_variable(name: str, pos: int, variables: Variables, seen: frozenset[str]) -> list[str]:
    """Patterns held by a variable at text position pos.

    Uses the last plain assignment before pos; a '+=' between that
    assignment and pos appends a dynamic part.
    """
    base: tuple[int, str] | None = None
    appended = False
    for apos, value, is_append in variables.get(name, []):
        if apos >= pos:
            break
        if is_append:
            appended = True
        else:
            base = (apos, value)
            appended = False
    if base is None:
        return ["*"]
    options = key_patterns(base[1], variables, base[0], seen | {name}, keep_all=True)
    if not options:
        return ["*"]
    return [o + "*" for o in options] if appended else options


def key_patterns(
    expr: str,
    variables: Variables,
    pos: int = 1 << 62,
    seen: frozenset[str] = frozenset(),
    keep_all: bool = False,
) -> list[str]:
    """Return the key patterns an expression can produce ('*' = dynamic)."""
    expr = strip_wrappers(expr)
    if not expr:
        return []
    # A ternary with two key alternatives.
    q = split_top(expr, "?")
    if len(q) == 2:
        alts = split_top(q[1], ":")
        if len(alts) == 2:
            return key_patterns(alts[0], variables, pos, seen, keep_all) + key_patterns(
                alts[1], variables, pos, seen, keep_all
            )
    terms = split_top(expr, "+")
    results = [""]
    for term in terms:
        term = strip_wrappers(term)
        if term.startswith('"'):
            text = unquote(term)
            results = [r + text for r in results]
        elif IDENT_RE.fullmatch(term) and term in variables and term not in seen:
            options = resolve_variable(term, pos, variables, seen)
            results = [r + o for r in results for o in options]
        else:
            results = [r + "*" for r in results]
    out = []
    for r in results:
        r = re.sub(r"\*+", "*", r)
        if keep_all or r.startswith("config/"):
            out.append(r)
    return out


ASSIGN_RE = re.compile(r"\b([A-Za-z_]\w*)\s*(\+?=)(?!=)\s*([^;{}]*?);", re.S)
# string name("..." + x); as a declaration (not a function call).
CTOR_RE = re.compile(r"\bstring\s*&?\s*([A-Za-z_]\w*)\s*\(([^;{}]*?)\)\s*;", re.S)


def find_variables(text: str) -> Variables:
    """Collect string assignments (name = ...; name += ...; string name(...);)."""
    found: list[tuple[int, str, str, bool]] = []
    for m in ASSIGN_RE.finditer(text):
        found.append((m.start(), m.group(1), m.group(3).strip(), m.group(2) == "+="))
    for m in CTOR_RE.finditer(text):
        found.append((m.start(), m.group(1), m.group(2).strip(), False))
    variables: Variables = {}
    for pos, name, value, is_append in sorted(found):
        if not is_append and '"' not in value and "+" not in value:
            continue
        variables.setdefault(name, []).append((pos, value, is_append))
    return variables


def literal_default(expr: str) -> tuple[str | None, bool]:
    """Return (default text, is a bool literal) for a literal default arg."""
    expr = strip_wrappers(expr)
    if expr.startswith('"'):
        return unquote(expr), False
    if expr in ("true", "false"):
        return ("yes" if expr == "true" else "no"), True
    if re.fullmatch(r"-?\d+", expr):
        return expr, False
    return None, False


def is_source(rel: str) -> bool:
    if Path(rel).suffix not in SOURCE_SUFFIXES:
        return False
    if rel.startswith(EXCLUDED_DIRS) or rel in EXCLUDED_FILES:
        return False
    return True


def scan_text(text: str, rel: str, found: dict[str, Found]) -> None:
    """Add the keys used by one source file to found."""
    text = strip_comments(text)
    variables = find_variables(text)
    for m in CALL_RE.finditer(text):
        op = m.group(1)
        args, _ = split_args(text, m.end())
        if not args or not args[0]:
            continue
        line = text.count("\n", 0, m.start()) + 1
        for key in key_patterns(args[0], variables, m.start()):
            entry = found.setdefault(key, Found(key))
            entry.ops.add(op)
            entry.origins.append(f"{rel}:{line}")
            if op == "value" and len(args) >= 3:
                default, is_bool = literal_default(args[2])
                if default is not None and default not in entry.defaults:
                    entry.defaults.append(default)
                entry.is_bool |= is_bool


def scan_tree(root: Path) -> dict[str, Found]:
    found: dict[str, Found] = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        if not is_source(rel):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"warning: cannot read {rel}: {exc}", file=sys.stderr)
            continue
        scan_text(text, rel, found)
    return found


def parse_table(text: str) -> tuple[list[str], list[Row], list[str]]:
    """Parse the table into (raw lines, rows, errors)."""
    lines = text.splitlines()
    rows: list[Row] = []
    errors: list[str] = []
    for num, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        parts = stripped.split("|", 5)
        if len(parts) != 6:
            errors.append(f"line {num}: expected 6 '|' separated fields")
            continue
        row = Row(*(p.strip() for p in parts))
        if not row.path.startswith("config/"):
            errors.append(f"line {num}: path must start with config/")
        if row.type not in TYPES:
            errors.append(f"line {num}: unknown type '{row.type}'")
        if row.apply not in APPLIES:
            errors.append(f"line {num}: apply must be now or restart")
        if row.type == "enum" and not row.domain:
            errors.append(f"line {num}: enum without choices")
        if row.type in ("int", "float") and row.domain and not re.fullmatch(
            r"-?[\d.]+\.\.-?[\d.]+", row.domain
        ):
            errors.append(f"line {num}: range must be min..max")
        rows.append(row)
    return lines, rows, errors


def pattern_regex(pattern: str) -> re.Pattern[str]:
    return re.compile("".join("[^/]*" if c == "*" else re.escape(c) for c in pattern) + r"\Z")


def covered(key: str, rows: list[Row]) -> bool:
    """True if a table row describes the (possibly patterned) key."""
    for row in rows:
        if row.path == key or pattern_regex(row.path).match(key):
            return True
    return False


def guess_row(entry: Found) -> Row:
    default = entry.defaults[0] if entry.defaults else ""
    last = entry.key.rsplit("/", 1)[-1]
    if entry.is_bool or default in ("yes", "no"):
        kind = "bool"
    elif re.fullmatch(r"-?\d+", default):
        kind = "int"
    elif "path" in last or "dir" in last or last.endswith("_file"):
        kind = "path"
    else:
        kind = "string"
    return Row(entry.key, kind, default, "", "restart", TODO)


def cmd_list(found: dict[str, Found]) -> int:
    for key in sorted(found):
        e = found[key]
        defaults = ",".join(e.defaults)
        print(f"{key}\t{'/'.join(sorted(e.ops))}\t{defaults}\t{' '.join(e.origins)}")
    return 0


def cmd_update(found: dict[str, Found], table: Path) -> int:
    text = table.read_text(encoding="utf-8") if table.exists() else ""
    lines, rows, errors = parse_table(text)
    for err in errors:
        print(f"{table}: {err}", file=sys.stderr)
    new = [found[k] for k in sorted(found) if not covered(k, rows)]
    if not new:
        print("table is up to date")
        return 0
    if lines and lines[-1].strip():
        lines.append("")
    lines.append("# Added by scan_config_keys.py --update: fill in the type, apply and description.")
    for entry in new:
        lines.append(f"# origin: {', '.join(entry.origins[:3])}")
        lines.append(guess_row(entry).line())
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"added {len(new)} key(s) to {table}")
    return 0


def cmd_check(found: dict[str, Found], table: Path) -> int:
    if not table.exists():
        print(f"{table}: missing", file=sys.stderr)
        return 1
    _, rows, errors = parse_table(table.read_text(encoding="utf-8"))
    problems = [f"{table.name}: {err}" for err in errors]
    for key in sorted(found):
        if not covered(key, rows):
            problems.append(f"not in table: {key} ({found[key].origins[0]})")
    for row in rows:
        if row.description == TODO or not row.description:
            problems.append(f"no description: {row.path}")
    seen: set[str] = set()
    for row in rows:
        if row.path in seen:
            problems.append(f"duplicate row: {row.path}")
        seen.add(row.path)
    for p in problems:
        print(p)
    if problems:
        print(f"{len(problems)} problem(s)")
        return 1
    print(f"ok: {len(found)} key(s) in the code, {len(rows)} row(s) in the table")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--list", action="store_true", help="print the keys found in the code")
    mode.add_argument("--update", metavar="TABLE", type=Path, help="merge new keys into TABLE")
    mode.add_argument("--check", metavar="TABLE", type=Path, nargs="?", const=DEFAULT_TABLE, help="check TABLE")
    parser.add_argument("--root", type=Path, default=ROOT, help="source tree (default: this worktree)")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    found = scan_tree(root)
    if args.list:
        return cmd_list(found)
    if args.update:
        table = args.update if args.update.is_absolute() else root / args.update
        return cmd_update(found, table)
    table = args.check if args.check.is_absolute() else root / args.check
    return cmd_check(found, table)


if __name__ == "__main__":
    sys.exit(main())
