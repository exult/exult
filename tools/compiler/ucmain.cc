/**
 ** Ucmain.cc - Usecode Compiler
 **
 ** Written: 12/30/2000 - JSF
 **/

/*
Copyright (C) 2000-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "endianio.h"
#include "ucfun.h"
#include "ucloc.h"
#include "ucsymtbl.h"

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iosfwd>
#include <new>
#include <sstream>
#include <string_view>
#include <vector>

using std::ios;
using std::string_view_literals::operator""sv;

// THIS is what the parser produces.
extern std::vector<Uc_design_unit*> units;

std::vector<char*> include_dirs;    // -I directories.

static void print_help() {
	std::cout << R"(Usage: ucc [OPTIONS] [FILE]
Compile Exult Usecode source into bytecode.

Input and output:
  FILE                  Read one source file; omit FILE to read standard input.
  -o FILE               Write bytecode to FILE. Without -o, replace the input
                        filename's extension with .uco (or append .uco if it
                        has no extension). Standard input defaults to a.ucout.
  -I DIRECTORY          Add an include search directory. May be repeated.
                        UCC_INCLUDE adds one further include directory.
  -s                    Select Serpent Isle intrinsics. A source #game directive
                        overrides this choice; otherwise Black Gate is assumed
                        when no game is selected.
  -b                    Omit the symbol table from the output.

Diagnostics:
  -c MODE               Color diagnostics: always, never, or auto (the default).
                        Auto enables color when standard error is a terminal.
  -WNAME                Enable warning category NAME; -W NAME also works.
  -Wno-NAME             Disable warning category NAME.
  -Wall                 Enable all warning categories, including optional ones.
  -Wno-all              Disable all warning categories.
  -Werror               Treat enabled warnings as errors; does not enable them.
  -Wno-error            Restore ordinary severity, except explicit -Werror=NAME.
  -Werror=NAME          Enable category NAME and treat its warnings as errors.
  -Wno-error=NAME       Exempt category NAME from -Werror; does not enable it.
  -u                    Disable all warning categories (same as -Wno-all).

Warning categories (enabled by default except function-id-alias):
  goto                  Use of goto statements.
  integer-coercion      Integer literal range and signedness conversions.
  shape-to-function     Shape/function ID conversions and unverified script calls.
  constant-condition    Conditions whose result is known at compile time.
  unreachable-code      Statements that cannot be reached.
  implicit-return-type  Function declarations without an explicit return type.
  implicit-function-kind
                        Function IDs implicitly treated as shape# or object#.
  autonumber            Automatic function IDs without an #autonumber directive.
  missing-game          Intrinsics used without selecting a game.
  missing-itemref       Object/shape function calls without an explicit item.
  constructor-arguments Missing class constructor arguments, filled with zero.
  unknown-directives    Unrecognized source directives.
  unknown-escape-sequence
                        Unrecognized escape sequences in strings.
  function-id-alias     Compatible declarations with different names sharing an
                        explicit function ID (disabled by default).
  script-mismatch      Concatenation of script and non-script operands.
  general               Warnings without a more specific category.

Warning options are processed in order. An explicit per-category severity
overrides the global -Werror/-Wno-error setting. Disabling warnings never
disables hard errors.

Other options:
  -h                    Display this help and exit successfully, without
                        compiling or writing output.
  --                    End options, for example: ucc -- -source.uc

Examples:
  ucc -o example.uco example.uc
  ucc -I content/bgkeyring/src -o usecode content/bgkeyring/src/usecode.uc
  ucc -Werror -Wno-error=goto -o example.uco example.uc
  ucc -o example.uco < example.uc

Diagnostics go to standard error; help and compilation status go to standard
output. Exit status is zero on success and nonzero on failure. Compilation
errors preserve any previous output file.
)";
}

static int command_line_failure() {
	std::cerr << "Try 'ucc -h' for more information.\n";
	return EXIT_FAILURE;
}

static void compilation_status(const std::string& message, bool success) {
	if (Uc_location::get_color_output()) {
		std::cout << (success ? "\033[1;32m" : "\033[1;31m");
	}
	std::cout << message;
	if (Uc_location::get_color_output()) {
		std::cout << "\033[0m";
	}
	std::cout << '\n';
}

/*
 *  MAIN.
 */

int main(int argc, char** argv) try {
	extern int                  yyparse();
	extern FILE*                yyin;
	const char*                 src;
	std::string                 outbuf;
	const char*                 outname        = nullptr;
	bool                        want_sym_table = true;
	static const char*          optstring      = "ho:I:sbc:uW:";
	Uc_function::Intrinsic_type ty             = Uc_function::unset;
	opterr                                     = 0;    // Don't let getopt() print errs.
	Uc_location::set_color_output(isatty(fileno(stderr)));
	int optchr;
	while ((optchr = getopt(argc, argv, optstring)) != -1) {
		switch (optchr) {
		case 'h':
			print_help();
			return EXIT_SUCCESS;
		case 'o':    // Output to write.
			outname = optarg;
			break;
		case 'I':    // Include dir.
			include_dirs.push_back(optarg);
			break;
		case 'c':    // Output colorization mode.
			if (optarg == "always"sv) {
				Uc_location::set_color_output(true);
			} else if (optarg == "never"sv) {
				Uc_location::set_color_output(false);
			} else if (optarg == "auto"sv) {
				Uc_location::set_color_output(isatty(fileno(stderr)));
			} else {
				std::cerr << "error: invalid color mode '" << optarg << "'; expected 'always', 'never', or 'auto'\n";
				return command_line_failure();
			}
			break;
		case 's':
			ty = Uc_function::si;
			break;
		case 'b':
			want_sym_table = false;
			break;
		case 'W':
			if (!Uc_location::set_warning_option(optarg)) {
				std::cerr << "error: unknown warning option '-W" << optarg << "'\n";
				return command_line_failure();
			}
			break;
		case 'u':
			Uc_location::set_all_warnings(false);
			break;
		case '?': {
			const char* option = optopt ? strchr(optstring, optopt) : nullptr;
			if (option && option[1] == ':') {
				std::cerr << "error: missing argument to '-" << static_cast<char>(optopt) << "'\n";
			} else {
				const char* argument = optind < argc ? argv[optind] : argv[optind - 1];
				if (strncmp(argument, "--", 2) != 0) {
					argument = argv[optind - 1];
				}
				if (strncmp(argument, "--", 2) == 0) {
					std::cerr << "error: unknown option '" << argument << "'\n";
				} else {
					std::cerr << "error: unknown option '-" << static_cast<char>(optopt) << "'\n";
				}
			}
			return command_line_failure();
		}
		}
	}
	if (argc - optind > 1) {
		std::cerr << "error: expected a single input file\n";
		return command_line_failure();
	}
	char* env = getenv("UCC_INCLUDE");
	if (env) {
		include_dirs.push_back(env);
	}
	if (optind < argc) {    // Filename?
		src  = argv[optind];
		yyin = fopen(argv[optind], "r");
		if (!yyin) {
			std::cerr << "error: cannot open input file '" << src << "': " << strerror(errno) << "\n";
			return 1;
		}
		if (!outname) {    // No -o option?
			// Set up output name.
			outbuf               = src;
			const auto separator = outbuf.find_last_of("/\\");
			const auto basename  = separator == std::string::npos ? 0 : separator + 1;
			const auto extension = outbuf.find_last_of('.');
			if (extension != std::string::npos && extension > basename) {
				outbuf.resize(extension);
			}
			outbuf += ".uco";
			outname = outbuf.c_str();
		}
	} else {
		src  = "<stdin>";
		yyin = stdin;
		if (!outname) {
			outname = "a.ucout";
		}
	}
	Uc_location::register_source(src, src);
	Uc_location::set_cur(src, 0);
	Uc_function::set_intrinsic_type(ty);
	const int parse_result = yyparse();
	if (yyin != stdin) {
		fclose(yyin);
	}
	const int errs = Uc_location::get_num_errors();
	if (parse_result != 0 || errs > 0) {    // Check for errors.
		compilation_status("Usecode compilation failed!", false);
		return EXIT_FAILURE;
	}
	// Code generation can diagnose errors too. Do not truncate an existing
	// output or leave a partial binary when one of those checks fails.
	std::ostringstream out(ios::binary | ios::out);
	if (want_sym_table) {
		little_endian::Write4(out, UCSYMTBL_MAGIC0);    // Start with symbol table.
		little_endian::Write4(out, UCSYMTBL_MAGIC1);
		auto* symtbl = new Usecode_symbol_table;
		for (auto* unit : units) {
			symtbl->add_sym(unit->create_sym());
		}
		symtbl->write(out);
		delete symtbl;
	}
	for (auto* unit : units) {
		unit->gen(out);    // Generate function.
	}

	const int final_errs = Uc_location::get_num_errors();
	if (final_errs != 0 || out.fail()) {
		compilation_status("Usecode compilation failed!", false);
		return EXIT_FAILURE;
	}
	const auto    bytecode = out.str();
	std::ofstream output(outname, ios::binary | ios::out);
	if (!output) {
		std::cerr << "error: cannot open output file '" << outname << "': " << strerror(errno) << '\n';
		return EXIT_FAILURE;
	}
	output.write(bytecode.data(), bytecode.size());
	output.close();
	if (output.fail()) {
		std::cerr << "error: failed to write output file '" << outname << "'\n";
		compilation_status("Usecode compilation failed!", false);
		return EXIT_FAILURE;
	}
	compilation_status(std::string("Usecode successfully compiled to ") + outname + "!", true);
	return EXIT_SUCCESS;
} catch (const std::bad_alloc&) {
	std::cerr << "error: memory exhausted\n";
	return EXIT_FAILURE;
} catch (const std::exception& error) {
	std::cerr << "error: internal compiler error: " << error.what() << '\n';
	return EXIT_FAILURE;
}
