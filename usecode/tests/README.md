# Intrinsic argument tests

Run `python3 usecode/tests/test_intrinsic_args.py /path/to/c++` from the source tree. The checks require Python 3 and a C++17 compiler; keep assertions enabled. On Windows, put the compiler's runtime DLL directory on `PATH`.

The test compiles the production dispatcher, table-entry definition, intrinsic tables, and selected intrinsic implementations into an isolated executable with test doubles for the game services. It checks minimum argument counts across all three tables, invalid table indices, ignored inventory flags, and omitted versus supplied temporary-item and SI path flags. These checks exercise argument handling without loading game assets; they do not verify gameplay effects.
