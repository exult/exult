# Usecode formatter

`format_usecode.py` is a standalone Python 3 formatter. It adjusts whitespace, uses tabs for indentation, and checks that lexical tokens are unchanged. Strings, comments, and preprocessor directives retain their contents. The target line width defaults to 100 columns; atomic tokens and lines containing comments or function IDs may exceed it.

Pass files or directories. Directories are searched recursively for `*.uc`, `*.ucc`, `*.ucpp`, `*uccpp`, `*.uh`, `*.uhh`, `*.uch`, and `*.uchh`; overlapping input paths are deduplicated. Explicit files may have any extension. Without paths, the formatter reads standard input and writes standard output.

```sh
# Format all mods, compiler examples, and data examples in place.
python3 tools/format_usecode.py --write content tools/compiler data

# Check formatting without writing files.
python3 tools/format_usecode.py --check content tools/compiler data

# Preview a single file, with an optional target line width.
python3 tools/format_usecode.py --width 100 tools/compiler/test1.uc

# Run formatter regression tests.
python3 tools/tests/test_format_usecode.py
```

`--write` and `--check` are mutually exclusive. Standard output requires exactly one matching file when paths are supplied. Exit status is 0 on success, 1 when `--check` finds formatting differences, and 2 for invalid arguments, unreadable files, or rejected source. All inputs are formatted and validated before any file is written. Written files use UTF-8 and LF line endings.
