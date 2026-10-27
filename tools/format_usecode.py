#!/usr/bin/env python3
"""Whitespace-only Usecode formatter; usable on files, directories or stdin/stdout.

No compiler/repository imports are required. Tokens mirror uclex.ll, including
atomic string-prefix literals and the actual compound operators (not C++'s).
"""
import argparse
import pathlib
import re
import sys

LEXER = re.compile(
    r'(?P<directive>^[ \t]*\#[^\n]*)|'
    r'(?P<string>"(?:\\.|[^"\\])*"\*?)|'
    r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)|'
    r'(?P<identity>\b(?:shape|object|id)\#)|'
    r'(?P<word>\$?[A-Za-z_][A-Za-z_0-9]*|0[xX][0-9a-fA-F]+|[0-9]+)|'
    r'(?P<operator>&&|\|\||==|!=|<=|>=|->|::|<<|\+=|-=|\*=|/=|%=|&=)|'
    r'(?P<punct>[^\s])', re.M)
CONTROL = {'if', 'while', 'for', 'switch', 'converse', 'catch', 'repeat', 'nested'}
OPERATORS = {'=', '+', '-', '*', '/', '%', '&', '|', '^', '<', '>',
             '&&', '||', '==', '!=', '<=', '>=', '<<', '+=', '-=', '*=', '/=', '%=', '&='}
PREFIX_CONTEXT = OPERATORS | {'(', '[', ',', ':', ';', '{', 'return', 'throw',
    'case', 'wait', 'frame', 'face', 'step', 'music', 'sfx', 'speech', 'call', 'after'}
FILE_PATTERNS = ('*.uc', '*.ucc', '*.ucpp', '*uccpp', '*.uh', '*.uhh', '*.uch', '*.uchh')


def lex(text):
    result = list(LEXER.finditer(text))
    for m in result:
        if m.group() in ('"', '#') or m.group() == '/' and text[m.start():].startswith('/*'):
            raise ValueError('unterminated literal/comment or misplaced directive at offset %d' % m.start())
    return result


def signature(text):
    return [(m.lastgroup, m.group().lstrip() if m.lastgroup == 'directive' else m.group()) for m in lex(text)]


def spacing(tokens):
    """Compute tight generic/cast/prefix edges before changing any whitespace."""
    tight_before, tight_after, stack = set(), set(), []
    for i, t in enumerate(tokens):
        prev = tokens[i - 1] if i else ''
        if t == '<' and prev in ('struct', 'class'):
            tight_before.add(i); tight_after.add(i)
            j = i + 1
            while j < len(tokens) and tokens[j] not in ('>', ';', '{'):
                j += 1
            if j < len(tokens) and tokens[j] == '>': tight_before.add(j)
        if t in ('!', '-', '+', '*', '&') and (not prev or prev in PREFIX_CONTEXT):
            tight_after.add(i)
        if t == '*' and 'repeat' in tokens[max(0, i - 8):i] and tokens[i + 2:i + 3] == [';']:
            tight_after.add(i)
        if t == '(':
            stack.append(i)
        elif t == ')':
            if not stack: raise ValueError('unmatched closing parenthesis')
            opening = stack.pop()
            inside = tokens[opening + 1:i]
            if inside in (['byte'], ['int'], ['long'], ['var']) or (
                    len(inside) == 4 and inside[0] in ('struct', 'class')
                    and inside[1] == '<' and inside[3] == '>'):
                tight_after.add(i)
    if stack: raise ValueError('unclosed parenthesis')
    return tight_before, tight_after


def needs_space(tokens, i, before, after):
    if not i: return False
    prev, t = tokens[i - 1], tokens[i]
    if i in before or i - 1 in after: return False
    if t in (')', ']', ',', ';', '.', '->', '::', '@', ':'): return False
    if prev in ('(', '[', '.', '->', '::', '@'): return False
    if t == '(':
        return prev in CONTROL or tokens[i + 1:i + 2] == ['remove'] or prev in (')', ']') or prev[:1] == '"' or (
            prev[:1].isdigit() and (i < 2 or tokens[i - 2] != '@')) or prev in OPERATORS
    if t == '[':
        return prev in OPERATORS or prev in ('return', 'case')
    return True


def wrap(line, indent, width):
    """Pack tokens deterministically; strings and comments remain atomic."""
    prefix = '\t' * indent
    if '\n' in line or '//' in line or '/*' in line or re.search(r'\b(?:shape|object|id)#', line):
        return [prefix + line]
    ms = lex(line)
    openings, ends = [], {}
    for i, m in enumerate(ms):
        if m.group() == '(':
            openings.append(i)
        elif m.group() == ')' and openings:
            ends[openings.pop()] = i
    breaks = []
    for i, m in enumerate(ms):
        t = m.group()
        large_group = t == '(' and (ms[ends[i]].end() if i in ends else len(line)) - m.start() > 40
        if (t == ',' or large_group) and i + 1 < len(ms) and ms[i + 1].group() != ')':
            breaks.append(ms[i + 1].start())
        elif t in ('&&', '||') or i and m.lastgroup == 'string' and ms[i - 1].lastgroup == 'string':
            breaks.append(m.start())
    parts, start, continuation = [], 0, False
    while len((prefix + line[start:]).expandtabs(4)) > width:
        choices = [p for p in breaks if p > start]
        if not choices: break
        fitting = [p for p in choices if len((prefix + line[start:p].rstrip()).expandtabs(4)) <= width]
        stop = max(fitting) if fitting else min(choices)
        parts.append(prefix + line[start:stop].rstrip())
        start = stop
        if not continuation:
            prefix += '\t'; continuation = True
    parts.append(prefix + line[start:])
    return parts


def format_text(text, width=100):
    ms = lex(text)
    if not ms: return '\n' if text else ''
    ts = [m.group().lstrip() if m.lastgroup == 'directive' else m.group() for m in ms]
    before, after = spacing(ts)
    out, line, delimiters, braces = [], '', [], []
    line_indent = 0
    root_gap = False

    def blank():
        if out and out[-1] != '': out.append('')

    def flush():
        nonlocal line, root_gap
        if line:
            out.extend(wrap(line, line_indent, width)); line = ''
            if root_gap: blank(); root_gap = False

    def append(t, i, indent=None):
        nonlocal line, line_indent
        if not line: line_indent = len(braces) if indent is None else indent
        elif needs_space(ts, i, before, after): line += ' '
        line += t

    for i, (m, t) in enumerate(zip(ms, ts)):
        nxt = ts[i + 1] if i + 1 < len(ts) else ''
        same_line_comment = i + 1 < len(ms) and ms[i + 1].lastgroup == 'comment' and '\n' not in text[m.end():ms[i + 1].start()]
        if m.lastgroup == 'directive':
            flush(); out.append(t); continue
        if m.lastgroup == 'comment':
            trailing = i and '\n' not in text[ms[i - 1].end():m.start()]
            if trailing and line:
                line += '  ' + t
            else:
                flush()
                paragraph = i and text[ms[i - 1].end():m.start()].count('\n') > 1
                if paragraph or not braces and out and not out[-1].lstrip().startswith('//'): blank()
                line_indent = len(braces); line = t
            if t.startswith('//') or '\n' in t or not trailing:
                flush()
            continue
        if t in ('case', 'default', 'always') and (not delimiters or delimiters[-1] == '{'):
            flush(); append(t, i, max(0, len(braces) - 1)); continue
        if t == '{':
            kind = 'block'
            if line.startswith('enum '): kind = 'enum'
            elif line.startswith('converse'): kind = 'converse'
            elif line.startswith('switch'): kind = 'switch'
            if not braces and out and out[-1] and not out[-1].endswith('*/') and not out[-1].lstrip().startswith('//'):
                blank()
            append(t, i); flush(); braces.append(kind); delimiters.append(t); continue
        if t == '}':
            flush()
            if not delimiters or delimiters.pop() != '{' or not braces: raise ValueError('unmatched closing brace')
            braces.pop(); append(t, i)
            if not braces: root_gap = True
            if nxt not in (';', ')', ']', ',', 'else', 'catch', 'nobreak', 'while', '&') and not same_line_comment:
                flush()
            continue
        if t in ('(', '['):
            delimiters.append(t)
        elif t in (')', ']'):
            expected = '(' if t == ')' else '['
            if not delimiters or delimiters.pop() != expected: raise ValueError('unbalanced delimiter')
        # Named labels, unlike case labels, need an outdent before their name.
        if nxt == ':' and t != 'converse' and not line and (not delimiters or delimiters[-1] == '{'):
            append(t, i, max(0, len(braces) - 1)); continue
        append(t, i)
        if t == ':' and (not delimiters or delimiters[-1] == '{') and not (
                line.startswith(('class ', 'converse:'))):
            flush()
        elif t == ';' and nxt != ';' and (not delimiters or delimiters[-1] == '{') and not same_line_comment:
            flush()
        elif t == ',' and delimiters and delimiters[-1] == '{' and braces[-1] == 'enum' and not same_line_comment:
            flush()
    if delimiters or braces: raise ValueError('unclosed delimiter')
    flush()
    result = '\n'.join(out).rstrip('\n') + '\n'
    if signature(text) != signature(result): raise ValueError('formatter changed lexical tokens')
    return result


def edits(text):
    """Lossless presentation records containing only changed whitespace gaps."""
    result = format_text(text)
    old, new = lex(text), lex(result)
    changes = []
    a = b = 0
    for left, right in zip(old, new):
        if text[a:left.start()] != result[b:right.start()]:
            changes.append((a, left.start(), result[b:right.start()]))
        if left.group() != right.group():
            changes.append((left.start(), left.end(), right.group()))
        a, b = left.end(), right.end()
    if text[a:] != result[b:]: changes.append((a, len(text), result[b:]))
    return changes


def discover_files(paths):
    """Expand directories recursively, in stable order, without duplicate files."""
    files, seen = [], set()
    for path in paths:
        if path.is_dir():
            candidates = sorted(p for p in path.rglob('*') if p.is_file()
                                and any(p.match(pattern) for pattern in FILE_PATTERNS))
        else:
            candidates = [path]
        for candidate in candidates:
            resolved = candidate.resolve()
            if resolved not in seen:
                seen.add(resolved)
                files.append(candidate)
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='*', type=pathlib.Path, metavar='PATH',
                        help='files or directories (searched recursively for usecode sources and headers)')
    parser.add_argument('--write', action='store_true', help='format files in place')
    parser.add_argument('--check', action='store_true', help='exit with status 1 if formatting would change files')
    parser.add_argument('--width', type=int, default=100, help='target line width (default: 100)')
    args = parser.parse_args()
    if args.write and args.check: parser.error('--write and --check are exclusive')
    if args.width < 40: parser.error('--width must be at least 40')
    if not args.files:
        if args.write: parser.error('--write requires paths')
        text = sys.stdin.buffer.read().decode('utf8'); result = format_text(text, args.width)
        if args.check: return int(text != result)
        sys.stdout.buffer.write(result.encode('utf8')); return 0
    files = discover_files(args.files)
    if not args.write and not args.check and len(files) != 1: parser.error('stdout requires exactly one matching file')
    results = []
    for path in files:
        try:
            old = path.read_text(encoding='utf8')
            results.append((path, old, format_text(old, args.width)))
        except (ValueError, UnicodeError, OSError) as error:
            raise ValueError('%s: %s' % (path, error)) from error
    for path, old, new in results:
        if args.write and old != new:
            with path.open('w', encoding='utf8', newline='\n') as stream: stream.write(new)
        elif not args.check and not args.write: sys.stdout.buffer.write(new.encode('utf8'))
    return int(args.check and any(old != new for _, old, new in results))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, UnicodeError, OSError) as error:
        print('format_usecode: ' + str(error), file=sys.stderr)
        sys.exit(2)
