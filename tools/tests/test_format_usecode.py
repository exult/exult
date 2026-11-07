#!/usr/bin/env python3
"""Run with: python3 tools/tests/test_format_usecode.py"""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import format_usecode as formatter


class FormatUsecode(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='format-usecode-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def run_formatter(self, *args, source=''):
        return subprocess.run(
            [sys.executable, formatter.__file__, *map(str, args)],
            input=source.encode('utf8'), stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=15)

    def test_recursive_discovery_and_overlapping_paths(self):
        nested = self.directory / 'nested'
        nested.mkdir()
        names = ('a.uc', 'b.ucc', 'c.ucpp', 'duccpp', 'e.uh', 'f.uhh', 'g.uch', 'h.uchh')
        expected = []
        for name in names:
            path = nested / name
            path.write_text('void f(){}', encoding='utf8')
            expected.append(path)
        (nested / 'ignore.h').write_text('not usecode', encoding='utf8')
        (nested / 'directory.uc').mkdir()
        self.assertEqual(formatter.discover_files([self.directory, nested, expected[0]]), expected)

    def test_check_write_and_stdout(self):
        path = self.directory / 'example.uc'
        original = 'void f(){var x=1;}'
        path.write_text(original, encoding='utf8')
        self.assertEqual(self.run_formatter('--check', self.directory).returncode, 1)
        self.assertEqual(path.read_text(encoding='utf8'), original)
        result = self.run_formatter(path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.decode('utf8'), formatter.format_text(original))
        self.assertEqual(self.run_formatter('--write', self.directory).returncode, 0)
        self.assertEqual(path.read_bytes(), result.stdout)
        self.assertEqual(self.run_formatter('--check', self.directory).returncode, 0)

    def test_stdin_and_width(self):
        source = 'void f(){say(' + ','.join('"a long argument"' for _ in range(8)) + ');}'
        result = self.run_formatter('--width', 40, source=source)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.decode('utf8'), formatter.format_text(source, 40))
        self.assertEqual(self.run_formatter('--check', source=result.stdout.decode('utf8')).returncode, 1)
        self.assertEqual(self.run_formatter('--check', '--width', 40, source=result.stdout.decode('utf8')).returncode, 0)

    def test_tokens_and_idempotence(self):
        source = '''#game "blackgate"
// Leading comment
struct Position { var x; var y; }
void example shape#(0x400) () {
var value=-1; value+=2; // trailing comment
var prefix="Hello"*;
say("escaped \\"quote\\"", "/* string */", "// string");
if (!value && value<3) { value=-(value+1); }
var position=(struct<Position>) value;
converse (["name", "bye"]) { case "name" (remove): say("Avatar"); default: break; }
script item after 2 ticks { repeat 3 { frame 1; } }
/* Multiline
   comment */
}
'''
        result = formatter.format_text(source)
        self.assertEqual(formatter.signature(source), formatter.signature(result))
        self.assertEqual(formatter.format_text(result), result)
        self.assertIn('value += 2;  // trailing comment\n', result)

    def test_errors_do_not_partially_write(self):
        good = self.directory / 'a.uc'
        bad = self.directory / 'b.uc'
        original = 'void f(){}'
        good.write_text(original, encoding='utf8')
        bad.write_text('void broken(){', encoding='utf8')
        result = self.run_formatter('--write', self.directory)
        self.assertEqual(result.returncode, 2)
        self.assertIn(str(bad).encode('utf8'), result.stderr)
        self.assertEqual(good.read_text(encoding='utf8'), original)

    def test_usecode_declaration_and_conversation_layout(self):
        source = '''class Child: Parent {};
class<Child> identity(class<Child> value) { return value; }
void f() { converse : nested (["bye"]) { case topic (remove): break; }
try { identity(0); } catch (error) { say(error); } }
'''
        result = formatter.format_text(source)
        self.assertIn('class Child: Parent {', result)
        self.assertIn('identity(class<Child> value) {', result)
        self.assertIn('\tconverse: nested (["bye"]) {', result)
        self.assertIn('case topic (remove):', result)
        self.assertIn('} catch (error) {', result)
        self.assertEqual(formatter.format_text(result), result)

    def test_invalid_arguments(self):
        for args in (('--write', '--check'), ('--width', '39'), ('--write',),
                     (self.directory,), (self.directory / 'missing.uc',)):
            result = self.run_formatter(*args)
            self.assertEqual(result.returncode, 2, result.stderr)


if __name__ == '__main__':
    unittest.main()
