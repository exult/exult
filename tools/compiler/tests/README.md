# UCC diagnostic regression tests

Build UCC, then run `python3 tools/compiler/tests/test_diagnostics.py /path/to/ucc` from the source tree. Python 3 and the compiler executable are the only requirements; on Windows, ensure UCC's runtime DLL directory is on `PATH`. The tests create isolated temporary inputs and outputs, clear `UCC_INCLUDE`, and place a timeout on each compiler invocation.

The standalone location test checks nested context-stack growth, restoration during exception unwinding, and interned filename stability and reuse. Build it from the repository root with `c++ -std=c++17 -Iheaders -Itools/compiler tools/compiler/tests/test_locations.cc tools/compiler/ucloc.cc -o /tmp/ucc-test-locations`, then run `/tmp/ucc-test-locations` (use a suitable temporary executable path on Windows). Keep assertions enabled.

The suite checks diagnostic locations and excerpts, include and line-directive handling, color controls, invalid command-line options, failure exit status, statement recovery, restoration of parser and lexer contexts, and warning controls. Warning suppression must preserve bytecode and must not suppress hard errors. Recovery tests include independent errors in following statements and functions to detect premature termination and state leakage.

Command-line tests also check detailed `-h` help on stdout, successful exit without opening the source or touching output, help hints for invalid options and arguments, and `--` handling for filenames beginning with a hyphen.

For compiler changes, also compile `tools/compiler/test1.uc`, BG Keyring's `content/bgkeyring/src/usecode.uc`, and SI Fixes' `content/sifixes/src/usecode.uc`, each with its corresponding include directory and a temporary output path. Compare their diagnostics and bytecode against the baseline for changes that should not affect code generation. `test1.uc` deliberately triggers warnings.

Recovery synchronizes statements, global declarations, and class/struct members at semicolons and closing braces. Recovery restores lexer modes and parser declaration/scope state. A malformed signature can be discarded so later declarations are checked. Missing semicolons after complete declarations can retain the next declaration; incomplete expressions can require discarding through the next delimiter.

Every warning site has a named category. The suite checks category suppression against unchanged bytecode and verifies promotion with `-Werror=NAME`. `-Werror`, `-Wno-error=NAME`, `-Wall`, `-Wno-all`, and `-u` share the same warning policy. See `docs/ucc.1` for the complete category list and command-line options.

Compatible declarations with different names sharing an explicit function ID are accepted silently by default. `-Wfunction-id-alias` enables a warning with a previous-declaration note. Tests check that both names remain callable, definitions retain their own parameter names, incompatible kinds/parameter types/return types mention both names and the shared ID, and multiple definitions remain errors.

The diagnostic audit covers the lexer, parser, symbol tables, classes, expressions, statements, code generation, and command-line/file I/O. Semantic actions establish a scoped source location without changing the lexer cursor; AST nodes and symbols retain that location for delayed diagnostics. Declaration, parameter, label, default-case, and ambiguous-string diagnostics include related-location notes. Typo suggestions require a unique nearby visible name and exclude recovery placeholders.

The negative tests exercise literal overflow, division/remainder overflow, invalid intrinsic indices, conflicting return types and parameters, duplicate initializers, repeated invalid variable uses, malformed signatures and members, bad directives, missing includes, invalid bytes, unterminated strings/comments, source remapping, and code-generation failures. A deterministic corpus deletes or replaces every token in a complete sample covering classes, structs, functions, loops, exceptions, conversations, and scripts; each compiler invocation must terminate normally and produce output only on success.

Compatibility checks compare the two mods and `test1.uc` against their pre-change bytecode. The stricter return-type check exposed BG Keyring's `spellitemGetNPCIndex` declaration as `extern void` despite its `var` definition; correcting the declaration preserves its compiled bytecode. Compiler errors, including promoted warnings and code-generation errors, preserve any previous output file. Diagnostics use stderr; status messages use stdout.
