#!/usr/bin/env python3
"""Run with: python3 tools/compiler/tests/test_diagnostics.py /path/to/ucc"""

import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


UCC = str(Path(sys.argv.pop(1)).resolve())


class Diagnostics(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ucc-diagnostics-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.output = self.directory / "output.uco"

    def compile(self, source, *options, color="never"):
        path = self.directory / "input.uc"
        path.write_text(source, encoding="ascii")
        return self.run_ucc("-c", color, "-o", str(self.output), *options, str(path))

    def run_ucc(self, *args):
        env = os.environ.copy()
        env.pop("UCC_INCLUDE", None)
        return subprocess.run(
            [UCC, *args], input="", stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True,
            cwd=str(self.directory), env=env, timeout=15,
        )

    def assert_failed(self, result, message):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(message, result.stdout)
        self.assertFalse(self.output.exists(), result.stdout)

    def test_plain_success(self):
        result = self.compile("void example 0x900 () {}\n")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())
        self.assertNotIn("\033[", result.stdout)

    def test_plain_failure(self):
        result = self.compile("void example 0x900 () { var x = ; }\n")
        self.assert_failed(result, "syntax error")
        self.assertNotIn("\033[", result.stdout)

    def test_raw_script_expressions(self):
        source = '''#game "blackgate"
var command 0x901 () { return 0x27; }
void example 0x900 (var value) {
 script item {
  raw(0x27);
  raw(-1);
  raw((long) -1);
  raw((byte) +0x27);
  raw(value);
  raw(value + 1);
  raw(command());
  raw(UI_get_random(10));
  raw([0x27, value]);
 }
}
'''
        result = self.compile(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())

    def test_raw_script_expression_bytecode(self):
        source = '''#game "blackgate"
var duration 0x901 () { return 2; }
void example 0x900 (var value) {
 script item { %s }
}
'''
        result = self.compile(source % 'wait value + duration();')
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        for command in (
            'raw((byte) 0x27); raw(value + duration());',
            'raw([(byte) 0x27, value + duration()]);',
        ):
            with self.subTest(command=command):
                result = self.compile(source % command)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(self.output.read_bytes(), expected)

    def test_raw_script_void_call(self):
        result = self.compile('''#game "blackgate"
void command 0x901 () {}
void example 0x900 () { script item raw(command()); }
''')
        self.assert_failed(result, "void function 'command' does not return a value")

    def script_source(self, setup, command, parameters="var condition"):
        return '#game "blackgate"\nvoid example 0x900 (' + parameters + ') {\n' + setup + '\n script item { ' + command + ' }\n}\n'

    def assert_script_offset(self, setup, length, command="repeat 3 *s;", options=()):
        source = self.script_source(setup, command)
        result = self.compile(source, *options)
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        offset = '(long) %d' % -length if length > 32768 else str(-length)
        result = self.compile(self.script_source(setup,
            'raw(s & new script { raw((byte) 0x0b); raw(%s); raw(3); });' % offset), *options)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_script_dereference_offsets(self):
        for setup, size in (
            ('var s = new script { nop; };', 1),
            ('var s = new script { wait 1; nop; };', 3),
            ('var s = new script { raw([]); };', 0),
            ('var a = [1, 2]; var b = [a, 3]; var s = new script { raw(b); };', 3),
            ('var s = new script nop;;', 1),
            ('var s = new script { nop; }; var t = new script { wait 1; }; s = s & t;', 3),
            ('var s = new script { nop; }; var t = new script { wait 1; }; s &= t;', 3),
            ('var s = new script { nop; }; s << { wait 1; } << new script { nop; };', 4),
            ('var s = new script { nop; }; var alias a = s; a &= new script { wait 1; };', 3),
            ('var s = new script { nop; }; s[1] = 0x21;', 1),
            ('var s = new script { nop; }; s[3] = 0x21;', 3),
            ('var s = new script { nop; }; var t = s; s &= s; s = t;', 1),
        ):
            with self.subTest(setup=setup):
                self.assert_script_offset(setup, size)

    def test_script_dereference_parentheses_and_count(self):
        setup = 'var s = new script { nop; }; var t = new script { wait 1; };'
        for expression, count, size in (('s', '3 * 2', 1), ('(s & t)', '(1 + 2)', 3)):
            source = self.script_source(setup, 'repeat %s *%s;' % (count, expression))
            result = self.compile(source)
            self.assertEqual(result.returncode, 0, result.stdout)
            expected = self.output.read_bytes()
            result = self.compile(self.script_source(setup,
                'raw(%s & new script { raw((byte) 0x0b); raw(%d); raw(%s); });' % (expression, -size, count)))
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(self.output.read_bytes(), expected)

    def test_script_dereference_large_lengths(self):
        setup = 'var a = [0]; ' + 'a &= a; ' * 15
        for extra, size in (('', 32768), ('a << 0; ', 32769)):
            with self.subTest(size=size):
                self.assert_script_offset(setup + extra + 'var s = new script { raw(a); };', size)

    def test_script_repeat_concat_evaluation_order(self):
        for setup in ('var s = new script { nop; };', 'static var s; s = new script { nop; };'):
            result = self.compile(self.script_source(setup, 'repeat UI_get_random(3) *s;'))
            self.assertEqual(result.returncode, 0, result.stdout)
            expected = self.output.read_bytes()
            result = self.compile(self.script_source(setup,
                'raw(s & new script { raw((byte) 0x0b); raw(-1); raw(UI_get_random(3)); });'))
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(self.output.read_bytes(), expected)

    def test_script_dereference_control_flow(self):
        for setup in (
            'var s; if (condition) { s = new script { nop; }; } else { s = new script { nohalt; }; }',
            'var s = new script { nop; }; while (condition) { s = new script { nohalt; }; }',
            'var s = new script { nop; }; do { s = new script { nohalt; }; } while (condition);',
            'var s = new script { nop; }; again: if (condition) { s = new script { nohalt; }; goto again; }',
            'var s = new script { nop; }; if (condition) { s = new script { wait 1; }; return; }',
            'var s; switch (condition) { case 1: s = new script { nop; }; break; default: s = new script { nohalt; }; }',
        ):
            with self.subTest(setup=setup):
                self.assert_script_offset(setup, 1)

    def test_script_dereference_unknown_length(self):
        for setup in (
            'var s = new script { raw(condition); };',
            'var s; if (condition) { s = new script { nop; }; } else { s = new script { wait 1; }; }',
            'var s = new script { nop; }; if (condition) { s &= new script { nop; }; }',
            'var s = new script { nop; }; while (condition) { s &= new script { nop; }; }',
            'var s = new script { nop; }; again: if (condition) { s &= new script { nop; }; goto again; }',
            'var s = new script { nop; }; s[condition] = 0x21;',
            'var s; switch (condition) { case 1: s = new script { nop; }; break; default: s = new script { wait 1; }; }',
            'var s = new script { nop; }; s &= [condition];',
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'))
                self.assert_failed(result, "cannot repeat script: array length is unknown")

    def test_script_dereference_requires_script(self):
        for setup in (
            'var s = [0x21];',
            'var s = 0x21;',
            'var s;',
            'var s = new script { nop; }; s = [0x21];',
            'var s = new script { nop; }; if (condition) { s = [0x21]; }',
            'var s = new script { nop; }; var values = [1]; for (s in values) {}',
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'))
                self.assert_failed(result, "script dereference requires a value tracked as a script")

    def test_script_dereference_error_spans(self):
        for body in ('animationCommands', '(animationCommands)', '(animationCommands & animationCommands)'):
            for command in ('repeat 2 *%s;', 'repeat 2, 3 *%s;', 'repeat forever *%s;',
                            'repeat 3 { repeat 2 *%s; };'):
                with self.subTest(body=body, command=command):
                    line = '\tvar unusedScriptResult = script item { ' + command % body + ' };'
                    source = ('#game "blackgate"\nvoid example 0x900 (var value) {\n'
                              ' var animationCommands = new script { raw(value); };\n' + line + '\n}\n')
                    result = self.compile(source)
                    self.assert_failed(result, 'cannot repeat script: array length is unknown')
                    column = line.index(body) + 1
                    self.assertIn('input.uc:4:%d: error: cannot repeat script' % column, result.stdout)
                    expanded = line.expandtabs(8)
                    start = expanded.index(body)
                    self.assertIn('  ' + expanded + '\n  ' + ' ' * start + '^' + '~' * (len(body) - 1) + '\n', result.stdout)

    def test_script_dereference_type_error_span(self):
        line = ' script item { repeat 2 *(commands); }'
        result = self.compile('#game "blackgate"\nvoid example 0x900 () {\n var commands = [1];\n' + line + '\n}\n')
        self.assert_failed(result, 'script dereference requires a value tracked as a script')
        start = line.index('(commands)')
        self.assertIn('input.uc:4:%d: error: script dereference' % (start + 1), result.stdout)
        self.assertIn('  ' + line + '\n  ' + ' ' * start + '^~~~~~~~~~\n', result.stdout)

    def test_script_concat_warnings(self):
        for statement in ('s = s & a;', 's = a & s;', 's &= a;', 's << a;'):
            with self.subTest(statement=statement):
                setup = 'var s = new script { nop; }; var a = [1, 2]; ' + statement
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'))
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertIn('[-Wscript-mismatch]', result.stdout)
                self.assert_script_offset(setup, 3, options=('-Wno-script-mismatch',))
                self.output.unlink()
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'), '-Werror=script-mismatch')
                self.assert_failed(result, '[-Werror=script-mismatch]')

    def test_script_concat_matching_operands(self):
        result = self.compile(self.script_source(
            'var a = [1]; var b = [2]; a &= b; a << b; '
            'var s = new script { nop; }; var t = new script { nohalt; }; s &= t; s << t;',
            'repeat 3 *s;'))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotIn('script-mismatch', result.stdout)

    def test_script_concat_warning_spans(self):
        for expression in (
            's & [1, 2]',
            '((s)) & ([1, 2])',
            '(new script { nop; }) & [1, 2]',
            '(new script raw((byte) 0x59);) & [1, 2]',
            's & [(byte) 0x0b, -2, 1]',
        ):
            with self.subTest(expression=expression):
                line = '\tvar value = UI_get_array_size(' + expression + ');'
                source = '#game "blackgate"\nvoid example 0x900 () {\n var s = new script { nop; };\n' + line + '\n}\n'
                result = self.compile(source)
                self.assertEqual(result.returncode, 0, result.stdout)
                column = line.index(expression) + 1
                self.assertIn('input.uc:4:%d: warning: concatenating a script' % column, result.stdout)
                expanded = line.expandtabs(8)
                start = expanded.index(expression)
                self.assertIn('  ' + expanded + '\n  ' + ' ' * start + '^' + '~' * (len(expression) - 1) + '\n', result.stdout)

    def test_script_assignment_concat_warning_spans(self):
        for expression in ('s &= [1, 2]', '(s) << [1, 2]'):
            with self.subTest(expression=expression):
                line = ' ' + expression + ';'
                source = '#game "blackgate"\nvoid example 0x900 () {\n var s = new script { nop; };\n' + line + '\n}\n'
                result = self.compile(source)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertIn('input.uc:4:2: warning: concatenating a script', result.stdout)
                self.assertIn('  ' + line + '\n   ^' + '~' * (len(expression) - 1) + '\n', result.stdout)

    def test_script_concat_multiline_warning_span(self):
        result = self.compile('''#game "blackgate"
void example 0x900 () {
 var value = (new script {
  nop;
 }) & [1, 2];
}
''')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('input.uc:3:14: warning: concatenating a script', result.stdout)
        self.assertIn('   var value = (new script {\n               ^~~~~~~~~~~~~\n', result.stdout)

    def test_script_dereference_nested_repeat(self):
        setup = 'var s = new script { nop; wait 1; };'
        result = self.compile(self.script_source(setup, 'repeat 2 { repeat 3 *s; };'))
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        result = self.compile(self.script_source(setup,
            'raw(s & new script { raw((byte) 0x0b); raw(-3); raw(3); }); '
            'raw((byte) 0x0b); raw(-6); raw(2);'))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_script_dereference_reset_count_and_forever(self):
        setup = 'var s = new script { nop; };'
        for command, reference in (
            ('repeat 3, 4 *s;', 'raw(s & new script { raw((byte) 0x0c); raw(-1); raw(3); raw(4); });'),
            ('repeat forever *s;', 'raw(s & new script { raw((byte) 0x0b); raw(-1); raw(255); });'),
        ):
            result = self.compile(self.script_source(setup, command))
            self.assertEqual(result.returncode, 0, result.stdout)
            expected = self.output.read_bytes()
            result = self.compile(self.script_source(setup, reference))
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(self.output.read_bytes(), expected)

    def test_new_script_uses_normal_repeat(self):
        setup = 'var s = new script { repeat 2 { nop; }; };'
        result = self.compile(self.script_source(setup, 'raw(s);'))
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        reference = ('var s = new script { nop; raw((byte) 0x0b); '
                     'raw(-1); raw(2); };')
        result = self.compile(self.script_source(reference, 'raw(s);'))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_script_dereference_rejects_repeat_bodies(self):
        for setup in (
            'var s = new script { repeat 2 { nop; }; };',
            'var s = new script { repeat 2, 3 { nop; }; };',
            'var s = new script { repeat 2 { repeat 3 { nop; }; }; };',
            'var s = new script { nop; repeat 2 { nop; }; nohalt; };',
            'var t = new script { repeat 2 { nop; }; }; var s = t;',
            'var t = new script { repeat 2 { nop; }; }; var alias s = t;',
            'var t = new script { repeat 2 { nop; }; }; var s = new script { nop; }; s &= t;',
            'var t = new script { repeat 2 { nop; }; }; var s = t & new script { nop; };',
            'var s = new script { nop; }; s << { repeat 2 { nop; }; };',
            'var s = new script { nop; nop; nop; nop; }; '
            'if (condition) { s = new script { repeat 2 { nop; }; }; }',
            'var s = new script { nop; nop; nop; nop; }; '
            'while (condition) { s = new script { repeat 2 { nop; }; }; }',
            'var s = new script { repeat 2 { nop; }; }; s = new script { raw(s); };',
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'), '-Wno-all')
                self.assert_failed(result, 'cannot dereference a script containing a repeat inside another repeat')

    def test_script_repeat_metadata_reassignment(self):
        self.assert_script_offset('var s = new script { repeat 2 { nop; }; }; '
                                  's = new script { nop; };', 1)

    def test_new_script_repeat_context_recovery(self):
        result = self.compile('''#game "blackgate"
void example 0x900 () {
 var s = new script repeat 2 { raw(; };
 script item repeat forever { nop; };
 var following = ;
}
''')
        self.assert_failed(result, 'syntax error')
        self.assertNotIn("'repeat forever' cannot be nested", result.stdout)
        self.assertIn('input.uc:5:', result.stdout)

    def test_script_metadata_shared_variables_and_exceptions(self):
        for setup, message in (
            ('static var s; s = new script { nop; };', None),
            ('static var s; s = new script { nop; }; UI_get_random(1);', 'requires a value tracked as a script'),
            ('var s = new script { nop; }; try { s &= new script { nop; }; } catch () {}', 'requires a value tracked as a script'),
            ('var s; try { s = new script { nop; }; } catch () { s = new script { nohalt; }; }', None),
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'), '-Wno-all')
                if message:
                    self.assert_failed(result, message)
                else:
                    self.assertEqual(result.returncode, 0, result.stdout)
                    self.output.unlink()

    def test_script_dereference_scope_and_recovery(self):
        for malformed in ('*s;', 'repeat 3;', 'repeat 3 *;'):
            with self.subTest(malformed=malformed):
                result = self.compile(self.script_source('var s = new script { nop; };', malformed))
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertFalse(self.output.exists())

    def test_assume_script_preserves_bytecode(self):
        result = self.compile(self.script_source('assume_script(s, 26);', 'repeat 3 *s;', 'var s'))
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        result = self.compile(self.script_source('',
            'raw(s & new script { raw((byte) 0x0b); raw(-26); raw(3); });', 'var s'), '-Wno-script-mismatch')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_assume_script_alone_emits_no_code(self):
        result = self.compile('void example 0x900 (var s) { ; }\n')
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        result = self.compile('void example 0x900 (var s) { assume_script(s, 26); }\n')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_assume_script_refinement(self):
        for setup, size in (
            ('declare var s; assume_script(s, 26);', 26),
            ('var s; assume_script(s, 0);', 0),
            ('var s = [0x21]; assume_script(s, 1);', 1),
            ('var s = new script { nop; }; assume_script(s, 1);', 1),
            ('var s; var alias a = s; assume_script(a, 2);', 2),
            ('var s; const int length = 13; assume_script(s, length * 2);', 26),
            ('var s; if (condition) { s = new script { nop; }; } assume_script(s, 1);', 1),
            ('var s; if (condition) { s = new script { nop; }; } '
             'else { s = new script { wait 1; }; } assume_script(s, 2);', 2),
        ):
            with self.subTest(setup=setup):
                self.assert_script_offset(setup, size)

    def test_assume_script_contradictions(self):
        for setup, message in (
            ('var s = 1; assume_script(s, 1);', "contradicts known scalar value of 's'"),
            ('var s = [1, 2]; assume_script(s, 1);', "length 1 contradicts known length 2 of 's'"),
            ('var s = new script { nop; }; assume_script(s, 26);', "length 26 contradicts known length 1 of 's'"),
            ('var s; assume_script(s, 26); assume_script(s, 1);', "length 1 contradicts known length 26 of 's'"),
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'nop;'), '-Wno-all')
                self.assert_failed(result, message)

    def test_assume_script_validates_arguments(self):
        for statement, message in (
            ('assume_script(s, -1);', 'length must be nonnegative'),
            ('assume_script(s, condition);', 'length must be a compile-time integer constant'),
            ('assume_script(s, UI_get_random(3));', 'length must be a compile-time integer constant'),
            ('assume_script(s, "26");', 'length must be a compile-time integer constant'),
            ('assume_script(missing, 26);', "use of undeclared identifier 'missing'"),
            ('assume_script(1, 26);', 'syntax error'),
            ('assume_script(s, );', 'syntax error'),
        ):
            with self.subTest(statement=statement):
                result = self.compile(self.script_source('var s; ' + statement, 'nop;'))
                self.assert_failed(result, message)
        result = self.compile('class Widget {} void example 0x900 () { class<Widget> s; assume_script(s, 0); }\n')
        self.assert_failed(result, 'assume_script requires a non-class variable')

    def test_assume_script_preserves_repeat_restriction(self):
        result = self.compile(self.script_source(
            'var s = new script { repeat 2 { nop; }; }; assume_script(s, 4);', 'repeat 3 *s;'))
        self.assert_failed(result, 'cannot dereference a script containing a repeat inside another repeat')

    def test_assume_script_follows_control_flow_and_reassignment(self):
        for setup, message in (
            ('var s; if (condition) { assume_script(s, 1); }', 'requires a value tracked as a script'),
            ('var s; if (condition) { assume_script(s, 1); } else { assume_script(s, 2); }', 'array length is unknown'),
            ('var s; assume_script(s, 26); s = [1];', 'requires a value tracked as a script'),
            ('static var s; assume_script(s, 26); UI_get_random(1);', 'requires a value tracked as a script'),
        ):
            with self.subTest(setup=setup):
                result = self.compile(self.script_source(setup, 'repeat 3 *s;'))
                self.assert_failed(result, message)

    def test_token_column_and_caret(self):
        result = self.compile("void example 0x900 () {\n    var value = ;\n}\n")
        self.assert_failed(result, "input.uc:2:17: error: syntax error, unexpected ';'")
        self.assertIn("      var value = ;\n                  ^\n", result.stdout)

    def test_tab_expansion(self):
        result = self.compile("void example 0x900 () {\n\tvar value = ;\n}\n")
        self.assert_failed(result, "input.uc:2:14: error:")
        self.assertIn("          var value = ;\n                      ^\n", result.stdout)

    def test_multiline_string_location(self):
        result = self.compile('void example 0x900 () {\n say("first\nsecond");\n var value = ;\n}\n')
        self.assert_failed(result, "input.uc:4:14: error:")

    def test_multiline_comment_location(self):
        result = self.compile("/* comment\n * continued\n */\nvoid example 0x900 () {\n var value = ;\n}\n")
        self.assert_failed(result, "input.uc:5:14: error:")

    def test_eof_location(self):
        result = self.compile("void example 0x900 () {\n")
        self.assert_failed(result, "input.uc:2:1: error: syntax error, unexpected end of file")

    def test_parser_stack_growth(self):
        source = 'void example 0x900 () { var value = ' + '(' * 300 + '1' + ')' * 300 + '; }\n'
        result = self.compile(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())

    def test_include_excerpt_and_restore(self):
        include = self.directory / "includes"
        include.mkdir()
        (include / "helper.uc").write_text("void helper 0x901 () { missing = 1; }\n", encoding="ascii")
        result = self.compile('#include "helper.uc"\nvoid example 0x900 () {\n var value = ;\n}\n', "-I", str(include))
        self.assert_failed(result, "helper.uc:1:24: error: use of undeclared identifier 'missing'")
        self.assertIn("  void helper 0x901 () { missing = 1; }\n", result.stdout)
        self.assertIn("input.uc:3:14: error:", result.stdout)

    def test_line_directive(self):
        result = self.compile('#line 42 "generated.uc"\nvoid example 0x900 () { var value = ; }\n')
        self.assert_failed(result, "generated.uc:42:37: error:")

    def test_recover_at_semicolon(self):
        result = self.compile("void example 0x900 () {\n var a = ;\n var b = ;\n missing = 1;\n}\n")
        self.assert_failed(result, "input.uc:2:10: error:")
        self.assertIn("input.uc:3:10: error:", result.stdout)
        self.assertIn("use of undeclared identifier 'missing'", result.stdout)
        self.assertEqual(result.stdout.count("error:"), 3, result.stdout)

    def test_recover_at_closing_brace(self):
        result = self.compile("void first 0x900 () { var a = }\nvoid second 0x901 () { var b = ; }\n")
        self.assert_failed(result, "unexpected '}'")
        self.assertIn("input.uc:2:", result.stdout)
        self.assertEqual(result.stdout.count("error:"), 2, result.stdout)

    def test_recovery_contexts(self):
        for malformed in (
            "const long value = ;",  # Constant operand width stack.
            "extern var helper (;",  # Function ID lexer mode and return type.
            "while (true) var value = ;",  # Implicit scope and loop mode.
            "for (value in ;",  # Array loop scope.
            "if (true) var value = ;",  # Implicit scope and nested-if tracking.
            "script item repeat 2 repeat 3 call ;",  # Script mode and repeat nesting.
            "var value = new script call ;",  # Script mode within an expression.
            "switch ( ;",  # Switch scope before its case list starts.
            "class<Widget> broken = ;",  # Class declaration type.
            "struct<Record> broken = ;",  # Struct declaration type.
        ):
            with self.subTest(malformed=malformed):
                result = self.compile(
                    '#game "blackgate"\nclass Widget {} struct Record { var field; } '
                    + 'void example 0x900 () {\n ' + malformed
                    + '\n var repeat = 1;\n break;\n continue;\n'
                    + ' var choice = user_choice;\n var following = ;\n}\n'
                    + 'void following_function 0x901 () { var another = ; }\n'
                )
                self.assert_failed(result, "syntax error")
                self.assertIn("'break' statement not allowed", result.stdout)
                self.assertIn("'continue' statement not allowed", result.stdout)
                self.assertIn("'user_choice' is only valid", result.stdout)
                self.assertIn("input.uc:8:", result.stdout)
                self.assertIn("input.uc:10:", result.stdout)
                self.assertNotIn("undeclared identifier 'repeat'", result.stdout)

    def test_recover_inside_scopes(self):
        source = '''#game "blackgate"
void example 0x900 () {
 var outer = 1;
 while (outer) {
  var value = ;
  break;
 }
 try { var value = ; } catch (exception) { var value = ; }
 switch (outer) { case 1: var value = ; break; default: break; }
 converse (0) { case "topic": var value = ; break; }
 outer = 2;
 break;
}
void following_function 0x901 () { var another = ; }
'''
        result = self.compile(source)
        self.assert_failed(result, "syntax error")
        self.assertEqual(result.stdout.count("syntax error"), 6, result.stdout)
        self.assertEqual(result.stdout.count("'break' statement not allowed"), 1, result.stdout)
        self.assertNotIn("undeclared identifier 'outer'", result.stdout)

    def test_identifier_location(self):
        result = self.compile('#game "blackgate"\nvoid example 0x900 () {\n missing = 1;\n}\n')
        self.assert_failed(result, "input.uc:3:2: error: use of undeclared identifier 'missing'")
        self.assertIn("   missing = 1;\n   ^~~~~~~\n", result.stdout)

    def test_previous_declaration_note(self):
        result = self.compile("void example 0x900 () {\n var value;\n var value;\n}\n")
        self.assert_failed(result, "input.uc:3:6: error: redefinition of 'value'")
        self.assertIn("input.uc:2:6: note: previous declaration is here", result.stdout)

    def test_semantic_errors_recover_without_crashing(self):
        for invalid in (
            "var duplicate = 1; var duplicate = 2;",
            "struct<Record> duplicate = 1; struct<Record> duplicate = 2;",
            "class<Widget> duplicate = new Widget(); class<Widget> duplicate = new Widget();",
            "var value = -new Widget();",
            "class<Widget> value = unknown();",
            "var value = new Widget()->Unknown::method();",
            "(@65535)();",
            "item->(@65535)();",
            "struct<Unknown>;",
        ):
            with self.subTest(invalid=invalid):
                source = ('#game "blackgate"\nclass Widget {} struct Record { var field; }\n'
                          + 'void example 0x900 () {\n ' + invalid
                          + '\n var following = ;\n}\n')
                result = self.compile(source)
                self.assert_failed(result, "error:")
                self.assertIn("input.uc:5:", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_indirect_call_has_untyped_result(self):
        result = self.compile('#game "blackgate"\nvoid example 0x900 (callback) { var value = (*callback)(); }\n')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_global_declaration_recovery(self):
        for invalid in (
            "const long broken = ;", "enum Broken { value = };",
            "extern var broken (;", "void broken (;", "void broken ( { }",
            "static class<Unknown> broken;", "@ invalid tokens;",
        ):
            with self.subTest(invalid=invalid):
                result = self.compile('#game "blackgate"\n' + invalid
                                      + '\nvoid following 0x900 () { var value = ; }\n')
                self.assert_failed(result, "error:")
                self.assertIn("input.uc:3:", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_member_declaration_recovery(self):
        source = '''#game "blackgate"
class Widget {
 var broken = ;
 void method(;
 var following;
 void valid() { var broken = ; }
}
struct Record {
 var ;
 @ invalid tokens;
 var field;
}
void following 0x900 () { var broken = ; }
'''
        result = self.compile(source)
        self.assert_failed(result, "error:")
        for line in (3, 4, 6, 9, 10, 13):
            self.assertIn("input.uc:%d:" % line, result.stdout)
        self.assertEqual(result.returncode, 1, result.stdout)

    def test_member_closing_brace_recovery(self):
        result = self.compile('class Widget { var broken = }\nstruct Record { var }\n'
                              + 'void following 0x900 () { var broken = ; }\n')
        self.assert_failed(result, "error:")
        self.assertIn("input.uc:2:", result.stdout)
        self.assertIn("input.uc:3:", result.stdout)

    def test_missing_declaration_semicolon_preserves_next_declaration(self):
        for declaration in ("const long value = 1", "extern void helper 0x901 ()", "static var value"):
            with self.subTest(declaration=declaration):
                result = self.compile(declaration + '\nvoid following 0x900 () { var broken = ; }\n')
                self.assert_failed(result, "error:")
                self.assertEqual(result.stdout.count("error:"), 2, result.stdout)

    def test_missing_local_semicolon_recovers_at_next_delimiter(self):
        result = self.compile("void example 0x900 () {\n var value = 1\n var following = ;\n}\n"
                              + "void following 0x901 () { var broken = ; }\n")
        self.assert_failed(result, "error:")
        self.assertEqual(result.stdout.count("error:"), 2, result.stdout)
        self.assertIn("input.uc:5:", result.stdout)

    def test_missing_include_recovers(self):
        result = self.compile('#include "missing.uc"\nvoid example 0x900 () { var broken = ; }\n')
        self.assert_failed(result, "cannot open include file 'missing.uc'")
        self.assertIn("input.uc:2:", result.stdout)

    def test_directive_at_eof(self):
        result = self.compile('void example 0x900 () {}\n#strictbraces "true"')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_bad_directives_recover(self):
        for directive in ('#include', '#game "unknown"', '#line 0 "fake.uc"',
                          '#autonumber 0', '#autonumber 999999999999999999999',
                          '#autonumber 0xC00oops', '#strictbraces "yes"'):
            with self.subTest(directive=directive):
                result = self.compile(directive + '\nvoid example 0x900 () { var broken = ; }\n')
                self.assert_failed(result, "error:")
                self.assertIn("input.uc:2:", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_integer_literal_errors(self):
        for literal in ('4294967296', '0x100000000', '999999999999999999999', '0xGG', '12abc'):
            with self.subTest(literal=literal):
                result = self.compile('void example 0x900 () { var value = ' + literal
                                      + ';\n var following = ; }\n')
                self.assert_failed(result, "integer literal")
                self.assertIn("input.uc:2:", result.stdout)

    def test_unterminated_comment_in_include(self):
        (self.directory / "helper.uc").write_text("\n/* unfinished\n", encoding="ascii")
        result = self.compile('#include "helper.uc"\nvoid example 0x900 () { var broken = ; }\n')
        self.assert_failed(result, "helper.uc:2:1: error: unterminated block comment")
        self.assertIn("input.uc:2:", result.stdout)

    def test_unterminated_string(self):
        result = self.compile('void example 0x900 () { say("unfinished\n')
        self.assert_failed(result, "input.uc:1:29: error: unterminated string literal")

    def test_bad_rune_escapes(self):
        for escape in (r"\{xx}", r"\{xyz}", r"\{unfinished"):
            with self.subTest(escape=escape):
                result = self.compile('void example 0x900 () { say("' + escape + '");\n var following = ; }\n')
                self.assert_failed(result, "rune escape sequence")
                self.assertIn("input.uc:2:", result.stdout)

    def test_invalid_bytes_recover(self):
        for byte in (b"\x00", b"\x01", b"\xff"):
            with self.subTest(byte=byte):
                source = self.directory / "input.uc"
                source.write_bytes(b'void example 0x900 () { ' + byte + b' var broken = ; }\n')
                result = self.run_ucc("-c", "never", "-o", str(self.output), str(source))
                self.assert_failed(result, "invalid byte")
                self.assertIn("syntax error", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_codegen_location_and_no_output(self):
        result = self.compile('void helper 0x900 () {}\nvoid example 0x901 () {\n var value = helper();\n}\n')
        self.assert_failed(result, "input.uc:3:14: error: void function 'helper' does not return a value")

    def test_preserve_previous_output_on_codegen_error(self):
        self.output.write_bytes(b"previous successful build")
        result = self.compile("void example 0x900 () { goto missing; }\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("undeclared label", result.stdout)
        self.assertEqual(self.output.read_bytes(), b"previous successful build")

    def test_untruncated_diagnostic(self):
        name = "unknown_" + "x" * 3000
        result = self.compile("void example 0x900 () { " + name + " = 1; }\n")
        self.assert_failed(result, "use of undeclared identifier '" + name + "'")

    def test_diagnostics_use_stderr(self):
        result = subprocess.run([UCC, "-c", "never", "-o", str(self.output)],
                                input="void example 0x900 () { var value = ; }\n",
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                universal_newlines=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error:", result.stderr)
        self.assertNotIn("error:", result.stdout)

    def test_goto_warning_control(self):
        source = "void example 0x900 () { goto target; target: return; }\n"
        enabled = self.compile(source)
        self.assertEqual(enabled.returncode, 0, enabled.stdout)
        self.assertIn("input.uc:1:25: warning: use of 'goto' statement [-Wgoto]", enabled.stdout)
        expected = self.output.read_bytes()
        disabled = self.compile(source, "-Wno-goto")
        self.assertEqual(disabled.returncode, 0, disabled.stdout)
        self.assertNotIn("[-Wgoto]", disabled.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_integer_warning_control_preserves_bytecode(self):
        source = '''#game "blackgate"
void example 0x900 () {
 var value = 70000, coerced = 65535;
 switch (value) { case 70000: break; case 65535: break; }
}
'''
        enabled = self.compile(source)
        self.assertEqual(enabled.returncode, 0, enabled.stdout)
        self.assertEqual(enabled.stdout.count("[-Winteger-coercion]"), 4, enabled.stdout)
        expected = self.output.read_bytes()
        for options in (("-Wno-integer-coercion",), ("-u",)):
            with self.subTest(options=options):
                disabled = self.compile(source, *options)
                self.assertEqual(disabled.returncode, 0, disabled.stdout)
                self.assertNotIn("warning:", disabled.stdout)
                self.assertEqual(self.output.read_bytes(), expected)

    def test_shape_warning_control_keeps_errors(self):
        source = '''#game "blackgate"
void helper 0x900 () {}
void example 0x901 () { var callback = &helper; script item call callback; }
'''
        for options in ((), ("-Wno-shape-to-function",), ("-u",)):
            with self.subTest(options=options):
                result = self.compile(source, *options)
                self.assert_failed(result, "not declared as 'shape#' or 'object#'")

    def test_forced_color(self):
        for source in ("void example 0x900 () {}\n", "void\n"):
            with self.subTest(source=source):
                result = self.compile(source, color="always")
                self.assertIn("\033[", result.stdout)

    def test_unknown_warning(self):
        self.assert_failed(self.compile("void example 0x900 () {}\n", "-Wtypo"),
                           "unknown warning option '-Wtypo'")

    def test_help_does_not_compile_or_touch_output(self):
        self.output.write_bytes(b'previous successful build')
        result = self.run_ucc('-h', '-o', str(self.output), 'nonexistent.uc')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('Usage: ucc [OPTIONS] [FILE]', result.stdout)
        self.assertIn('Warning categories', result.stdout)
        self.assertIn('function-id-alias', result.stdout)
        self.assertIn('disabled by default', result.stdout)
        self.assertIn('UCC_INCLUDE', result.stdout)
        self.assertIn('a.ucout', result.stdout)
        self.assertIn('Examples:', result.stdout)
        self.assertNotIn('error:', result.stdout)
        self.assertNotIn('Usecode successfully compiled', result.stdout)
        self.assertEqual(self.output.read_bytes(), b'previous successful build')
        self.assertEqual(sorted(path.name for path in self.directory.iterdir()), ['output.uco'])

    def test_help_uses_stdout_even_with_forced_color(self):
        result = subprocess.run([UCC, '-c', 'always', '-h'], input='',
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, cwd=str(self.directory), timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Usage: ucc', result.stdout)
        self.assertEqual(result.stderr, '')
        self.assertNotIn('\033[', result.stdout)
        self.assertEqual(list(self.directory.iterdir()), [])

    def test_command_line_errors_suggest_help(self):
        for arguments in (('-Q',), ('--unknown-option',), ('-o',), ('-I',),
                          ('-c',), ('-W',), ('-c', 'sometimes'), ('-Wtypo',),
                          ('first.uc', 'second.uc')):
            with self.subTest(arguments=arguments):
                result = self.run_ucc(*arguments)
                self.assert_failed(result, "Try 'ucc -h' for more information.")
                self.assertIn('error:', result.stdout)
                self.assertNotIn('Usage:', result.stdout)

    def test_option_terminator_allows_hyphenated_input(self):
        source = self.directory / '-h'
        source.write_text('void example 0x900 () {}\n', encoding='ascii')
        result = self.run_ucc('-o', str(self.output), '--', '-h')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())
        self.assertNotIn('Usage:', result.stdout)

    def test_unknown_option(self):
        self.assert_failed(self.run_ucc("-Q"), "unknown option '-Q'")

    def test_missing_option_argument(self):
        for option in ("-o", "-I", "-c", "-W"):
            with self.subTest(option=option):
                self.assert_failed(self.run_ucc(option), "missing argument to '" + option + "'")

    def test_extra_input(self):
        self.assert_failed(self.run_ucc("first.uc", "second.uc"), "expected a single input file")

    def test_many_errors_still_fail(self):
        # Each distinct undeclared name produces one error.
        source = "void example 0x900 () {\n" + "".join(
            "missing%d = 1;\n" % i for i in range(256)
        ) + "}\n"
        self.assert_failed(self.compile(source), "Usecode compilation failed!")


    def test_warning_categories(self):
        examples = {
            "constant-condition": "void example 0x900 () { if (1) { return; } }",
            "implicit-return-type": "example 0x900 () {}",
            "implicit-function-kind": "void example 0x100 () {}",
            "autonumber": "void example () {}",
            "missing-game": "void example 0x900 () { var value = item->get_item_shape(); }",
            "missing-itemref": "void helper object#(0x900) () {} void example 0x901 () { helper(); }",
            "constructor-arguments": "class Widget { var field; } void example 0x900 () { class<Widget> value = new Widget(); }",
            "unknown-directives": "#unknown\nvoid example 0x900 () {}",
            "unknown-escape-sequence": r'void example 0x900 () { say("\q"); }',
            "shape-to-function": "void example 0x900 () { var callback = item->get_item_shape(); script item call callback; }",
            "unreachable-code": "void example 0x900 (condition) { var value; do { if (condition) { break; } else { goto target; } } while (true) nobreak { value = 1; target: } }",
        }
        for category, body in examples.items():
            with self.subTest(category=category):
                source = ('' if category == 'missing-game' else '#game "blackgate"\n') + body + '\n'
                enabled = self.compile(source)
                self.assertEqual(enabled.returncode, 0, enabled.stdout)
                self.assertIn("[-W" + category + "]", enabled.stdout)
                expected = self.output.read_bytes()
                disabled = self.compile(source, "-Wno-" + category)
                self.assertEqual(disabled.returncode, 0, disabled.stdout)
                self.assertNotIn("[-W" + category + "]", disabled.stdout)
                self.assertEqual(self.output.read_bytes(), expected)
                self.output.unlink()
                promoted = self.compile(source, "-Werror=" + category)
                self.assert_failed(promoted, "[-Werror=" + category + "]")

    def test_global_warning_controls(self):
        source = "void example 0x900 () { goto target; target: return; }\n"
        result = self.compile(source, "-Werror")
        self.assert_failed(result, "[-Werror=goto]")
        result = self.compile(source, "-Werror", "-Wno-error=goto")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("[-Wgoto]", result.stdout)

        result = self.compile(source, "-u")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotIn("warning:", result.stdout)
        result = self.compile(source, "-u", "-Wall")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("[-Wgoto]", result.stdout)

    def test_argument_count_and_declaration_note(self):
        result = self.compile('void helper 0x900 (value) {}\nvoid example 0x901 () { helper(); }\n')
        self.assert_failed(result, "too few arguments to 'helper': expected 1, got 0")
        self.assertIn("input.uc:1:6: note: function declared here", result.stdout)

    def test_method_argument_index_and_parameter_note(self):
        # Use a complete declared type, then pass a non-class first argument.
        source = ('class Other {}\nclass Widget { void method(class<Other> value) {} }\n'
                  + 'void example 0x900 () { class<Widget> obj = new Widget(); obj->method(1); }\n')
        result = self.compile(source)
        self.assert_failed(result, "argument 1 to 'method' requires class 'Other'")
        self.assertIn("passing argument to parameter 'value' here", result.stdout)

    def test_conflicting_return_types(self):
        for first, second in (("var", "void"), ("void", "var"),
                              ("class<Widget>", "struct<Record>")):
            with self.subTest(first=first, second=second):
                result = self.compile('class Widget {} struct Record { var field; }\n'
                                      + 'extern ' + first + ' helper 0x900 ();\n'
                                      + 'extern ' + second + ' helper 0x900 ();\n')
                self.assert_failed(result, "conflicting return type for function 'helper'")
                self.assertIn("input.uc:2:", result.stdout)
                self.assertIn("note: previous declaration is here", result.stdout)

    def test_conflicting_parameter_type(self):
        result = self.compile('class Widget {}\nextern void helper 0x900 (var value);\n'
                              + 'extern void helper 0x900 (class<Widget> value);\n')
        self.assert_failed(result, "conflicting type for parameter 1")
        self.assertIn("previous parameter declaration is here", result.stdout)

    def test_function_id_alias_names_and_definition(self):
        source = ('#game "blackgate"\nextern var original 0x900 (old_parameter);\n'
                  'extern var alternate 0x900 (alias_parameter);\n'
                  'var alternate 0x900 (value) { return value; }\n'
                  'void example 0x901 () { var first = original(1); var second = alternate(2); }\n')
        result = self.compile(source, '-Werror', '-b')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotIn('warning:', result.stdout)
        self.assertNotIn('previous declaration', result.stdout)
        source = source.replace('alternate(2)', 'original(2)')
        result = self.compile(source, '-b')
        self.assertEqual(result.returncode, 0, result.stdout)
        expected = self.output.read_bytes()
        result = self.compile(source.replace('original(1)', 'alternate(1)').replace('original(2)', 'alternate(2)'), '-b')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_function_id_alias_declaration_orders(self):
        for declarations in (
            'var original 0x900 (value) { return value; }\nextern var alternate 0x900 (other);\n',
            'extern var original 0x900 (value);\nextern var alternate 0x900 (other);\n'
            'extern var third 0x900 (third_parameter);\nextern var alternate 0x900 (again);\n'
            'var original (value) { return value; }\n',
        ):
            with self.subTest(declarations=declarations):
                result = self.compile('#game "blackgate"\n' + declarations
                                      + 'void example 0x901 () { var a = original(1); var b = alternate(2); }\n', '-Werror')
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_automatic_id_collision_remains_an_error(self):
        result = self.compile('#autonumber 0x900\nextern void original 0x900 ();\nvoid alternate () {}\n')
        self.assert_failed(result, "function 'alternate' conflicts with previously declared function 'original'")

    def test_function_id_alias_compatible_signatures(self):
        for signature in ('void NAME 0x900 (class<Widget> value)',
                          'class<Widget> NAME 0x900 (struct<Record> value)',
                          'struct<Record> NAME 0x900 ()',
                          'void NAME object#(0x900) ()',
                          'void NAME shape#(0x100) ()'):
            with self.subTest(signature=signature):
                result = self.compile('class Widget {} struct Record { var field; }\nextern '
                                      + signature.replace('NAME', 'original') + ';\nextern '
                                      + signature.replace('NAME', 'alternate') + ';\n', '-Wfunction-id-alias')
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertIn('[-Wfunction-id-alias]', result.stdout)

    def test_function_id_alias_warning_controls(self):
        source = 'extern void original 0x900 ();\nextern void alternate 0x900 ();\n'
        for options, enabled in (((), False), (('-Werror',), False),
                                 (('-Wfunction-id-alias',), True), (('-Wall',), True),
                                 (('-Wall', '-Wno-function-id-alias'), False),
                                 (('-Wfunction-id-alias', '-u'), False),
                                 (('-Werror=function-id-alias', '-Wno-error=function-id-alias'), True)):
            with self.subTest(options=options):
                result = self.compile(source, *options)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual('[-Wfunction-id-alias]' in result.stdout, enabled)
                self.assertEqual('note: previous declaration is here' in result.stdout, enabled)
                if enabled:
                    self.assertIn("function 'alternate' shares usecode ID 0x900", result.stdout)
                    self.assertIn("previously declared function 'original'", result.stdout)
        self.output.unlink()
        result = self.compile(source, '-Werror=function-id-alias')
        self.assert_failed(result, '[-Werror=function-id-alias]')

    def test_incompatible_function_id_aliases(self):
        for first, second, message in (
            ('void original 0x900 ()', 'void alternate 0x900 (value)', 'expected 0 parameters, got 1'),
            ('void original 0x900 (var value)', 'void alternate 0x900 (class<Widget> value)', 'conflicting type for parameter 1'),
            ('void original 0x900 (class<Widget> value)', 'void alternate 0x900 (class<Other> value)', 'conflicting type for parameter 1'),
            ('void original 0x900 (struct<Record> value)', 'void alternate 0x900 (var value)', 'conflicting type for parameter 1'),
            ('void original 0x900 ()', 'var alternate 0x900 ()', 'conflicting return type'),
            ('class<Widget> original 0x900 ()', 'class<Other> alternate 0x900 ()', 'conflicting return type'),
            ('struct<Record> original 0x900 ()', 'var alternate 0x900 ()', 'conflicting return type'),
            ('void original 0x900 ()', 'void alternate object#(0x900) ()', 'conflicting kind'),
        ):
            with self.subTest(first=first, second=second):
                result = self.compile('class Widget {} class Other {} struct Record { var field; }\n'
                                      + 'extern ' + first + ';\nextern ' + second + ';\n', '-Wfunction-id-alias')
                self.assert_failed(result, message)
                self.assertIn("function 'alternate'", result.stdout)
                self.assertIn("usecode ID 0x900", result.stdout)
                self.assertIn("previously declared as 'original'", result.stdout)
                self.assertIn('note: previous', result.stdout)
                self.assertNotIn('[-Wfunction-id-alias]', result.stdout)

    def test_function_id_alias_does_not_allow_multiple_definitions(self):
        for source in (
            'void original 0x900 () {}\nvoid alternate 0x900 () {}\n',
            'extern void original 0x900 ();\nvoid alternate 0x900 () {}\nvoid original 0x900 () {}\n',
            'extern void original 0x900 ();\nvoid original 0x900 () {}\nvoid alternate 0x900 () {}\n',
        ):
            with self.subTest(source=source):
                result = self.compile(source)
                self.assert_failed(result, 'redefinition of function')
                self.assertIn('usecode ID 0x900 is already defined as', result.stdout)
                line = 2 if source.startswith('extern') else 1
                self.assertIn(f'input.uc:{line}:6: note: previous definition is here', result.stdout)

    def test_duplicate_label_and_default_notes(self):
        for body, message, note in (
            ("label: ; label: ;", "redefinition of label 'label'", "previous label is here"),
            ("switch (event) { default: break; default: break; }",
             "multiple default labels", "previous default label is here"),
        ):
            with self.subTest(body=body):
                result = self.compile('void example 0x900 () { ' + body + '\n var following = ; }\n')
                self.assert_failed(result, message)
                self.assertIn(note, result.stdout)
                self.assertIn("input.uc:2:", result.stdout)

    def test_constant_division_errors(self):
        for expression in ("1 / 0", "1 % 0", "0x80000000 / -1", "0x80000000 % -1"):
            with self.subTest(expression=expression):
                for prefix in ("const long value = ", "void example 0x900 () { var value = "):
                    with self.subTest(prefix=prefix):
                        source = prefix + expression + (";\n" if prefix.startswith("const") else "; }\n")
                        result = self.compile(source)
                        self.assert_failed(result, "constant")
                        self.assertEqual(result.returncode, 1, result.stdout)

    def test_syntax_error_includes_spelling(self):
        result = self.compile("void example 0x900 () { var x unexpected; }\n")
        self.assert_failed(result, "unexpected identifier 'unexpected'")

    def test_typo_suggestions(self):
        result = self.compile('void example 0x900 () { var counter; coutner = 1; }\n')
        self.assert_failed(result, "did you mean 'counter'?")
        self.assertIn("note: 'counter' declared here", result.stdout)
        result = self.compile('void calculate 0x900 () {}\nvoid example 0x901 () { calcualte(); }\n')
        self.assert_failed(result, "did you mean 'calculate'?")

    def test_ambiguous_and_placeholder_names_are_not_suggested(self):
        for body in ("var cat, cut; cot = 1;", "missng = 1; missing = 1;"):
            with self.subTest(body=body):
                result = self.compile('void example 0x900 () { ' + body + ' }\n')
                self.assert_failed(result, "undeclared identifier")
                self.assertNotIn("did you mean", result.stdout)

    def test_repeated_nonvariable_use_recovers(self):
        result = self.compile('const int value = 1;\nvoid example 0x900 () {\n'
                              + ' var first = value[1];\n var second = value[2];\n}\n')
        self.assert_failed(result, "'value' is not a variable")
        self.assertIn("input.uc:4:", result.stdout)
        self.assertEqual(result.returncode, 1, result.stdout)

    def test_return_conflict_avoids_secondary_return_errors(self):
        result = self.compile('extern void helper 0x900 ();\nvar helper 0x900 () { return 1; }\n')
        self.assert_failed(result, "conflicting return type")
        self.assertEqual(result.stdout.count("error:"), 1, result.stdout)

    def test_file_io_errors(self):
        result = self.run_ucc("-c", "never", "-o", str(self.output), "missing.uc")
        self.assert_failed(result, "error: cannot open input file 'missing.uc'")
        source = self.directory / "input.uc"
        source.write_text("void example 0x900 () {}\n", encoding="ascii")
        result = self.run_ucc("-c", "never", "-o", str(self.directory / "missing" / "output.uco"), str(source))
        self.assert_failed(result, "error: cannot open output file")

    def test_default_output_in_directory_with_dots(self):
        directory = self.directory / "directory.with.dots"
        directory.mkdir()
        source = directory / "source"
        source.write_text("void example 0x900 () {}\n", encoding="ascii")
        result = self.run_ucc("-c", "never", str(source))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue((directory / "source.uco").exists())

    def test_invalid_color_mode(self):
        self.assert_failed(self.run_ucc("-c", "sometimes"), "error: invalid color mode 'sometimes'")

    def test_unknown_long_option_spelling(self):
        self.assert_failed(self.run_ucc("--unknown-option"), "unknown option '--unknown-option'")

    def test_string_prefix_diagnostics(self):
        result = self.compile('void example 0x900 () { say("missing"*); }\n')
        self.assert_failed(result, "string prefix 'missing' matches no string")
        result = self.compile('void example 0x900 () {\n say("text one", "text two");\n say("text"*);\n}\n')
        self.assert_failed(result, "string prefix 'text' is ambiguous")
        self.assertEqual(result.stdout.count("candidate string starts here"), 2, result.stdout)

    def test_include_diagnostic_uses_resolved_path(self):
        include = self.directory / "includes"
        include.mkdir()
        (include / "helper.uc").write_text("void helper 0x901 () { var broken = ; }\n", encoding="ascii")
        result = self.compile('#include "helper.uc"\n', "-I", str(include))
        self.assert_failed(result, "helper.uc:1:")
        self.assertIn(str(include / "helper.uc").replace("\\", "/") + ":1:", result.stdout.replace("\\", "/"))

    def test_nul_in_directive_is_diagnosed(self):
        result = self.compile('#unknown\x00ignored\nvoid example 0x900 () { var broken = ; }\n')
        self.assert_failed(result, "NUL byte in directive")
        self.assertIn("input.uc:2:", result.stdout)

    def test_nul_in_string_recovers(self):
        result = self.compile('void example 0x900 () { say("text\x00continued");\n var broken = ; }\n')
        self.assert_failed(result, "NUL byte in string literal")
        self.assertIn("input.uc:2:", result.stdout)

    def test_nonascii_escape_diagnostic_is_printable(self):
        source = self.directory / "input.uc"
        source.write_bytes(b'void example 0x900 () { say("\\\xff"); }\n')
        result = self.run_ucc("-c", "never", "-o", str(self.output), str(source))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn(r"unknown escape sequence '\xFF'", result.stdout)

    def test_malformed_token_corpus_does_not_crash_or_hang(self):
        source = '''#game "blackgate"
#autonumber 0x900
class Widget { var field; void method(var value) { field = value; } }
struct Record { var field; }
var helper(var value) { return value + 1; }
void example(var condition) {
 var value = helper(1);
 if (condition) { value = 2; } else { value = 3; }
 while (condition) { if (value) { break; } value -= 1; }
 try { throw value; } catch (exception) { say("text"); }
 converse (0) { case "topic": break; }
 script item { repeat 2 { call 0x100; wait 1; }; }
}
'''
        baseline = self.compile(source)
        self.assertEqual(baseline.returncode, 0, baseline.stdout)
        tokens = list(re.finditer(r'"[^"\n]*"|\w+|[^\s\w]', source))
        # Mutate each token in the complete language sample, alternating
        # deletion and replacement so lexer and semantic recovery both run.
        for index, token in enumerate(tokens):
            with self.subTest(token=token.group(), index=index):
                if self.output.exists():
                    self.output.unlink()
                replacement = "@" if index % 2 else ""
                mutated = source[:token.start()] + replacement + source[token.end():]
                result = self.compile(mutated)
                self.assertIn(result.returncode, (0, 1), result.stdout)
                self.assertEqual(self.output.exists(), result.returncode == 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
