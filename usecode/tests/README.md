# Intrinsic argument tests

Run `python3 usecode/tests/test_intrinsic_args.py /path/to/c++` from the source tree. The checks require Python 3 and a C++17 compiler; keep assertions enabled. On Windows, put the compiler's runtime DLL directory on `PATH`.

The test compiles the production dispatcher, table-entry definition, intrinsic tables, and selected intrinsic implementations into an isolated executable with test doubles for the game services. It checks minimum argument counts across all three tables, invalid table indices, ignored inventory flags, and omitted versus supplied temporary-item and SI path flags. These checks exercise argument handling without loading game assets; they do not verify gameplay effects.

Optional-default checks also exercise object frames, book and scroll gumps, casting shapes, BG/SI path mode, and scene fades. Missing arguments must select their documented defaults, whereas explicitly supplied zero values must remain zero.

Dispatcher checks cover zero arguments, calls below and above the old padding size, the bytecode limit of 255 arguments, and argument order. They also exercise the production seven-argument egg guard and variadic error output without padded zeroes. Egg construction is replaced by a test double after its count check; actual egg placement still requires gameplay validation.
