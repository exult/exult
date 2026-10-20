/**
 ** Ucloc.h - Source location.
 **
 ** Written: 1/0/01 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef INCL_UCLOC
#define INCL_UCLOC

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#undef FORMAT_STRING
#undef PRINTF_FORMAT
#ifdef __GNUG__
#	define FORMAT_STRING(p) p
#	ifdef __clang__
#		define PRINTF_FORMAT(fmtarg, firstvararg) __attribute__((__format__(printf, fmtarg, firstvararg)))
#	else
#		define PRINTF_FORMAT(fmtarg, firstvararg) __attribute__((__format__(gnu_printf, fmtarg, firstvararg)))
#	endif
#elif defined(_MSC_VER)
#	if _MSC_VER >= 1400
#		include <sal.h>
#		if _MSC_VER > 1400
#			define FORMAT_STRING(p) _Printf_format_string_ p
#		else
#			define FORMAT_STRING(p) __format_string p
#		endif /* FORMAT_STRING */
#	else
#		define FORMAT_STRING(p) p
#		define PRINTF_FORMAT(fmtarg, firstvararg)
#	endif /* _MSC_VER */
#endif

/*
 *  Location in source code.
 */
class Uc_location {
public:
	enum class Warning {
		goto_statement,
		integer_coercion,
		shape_to_function,
		constant_condition,
		unreachable_code,
		implicit_return_type,
		implicit_function_kind,
		autonumber,
		missing_game,
		missing_itemref,
		constructor_arguments,
		unknown_directives,
		unknown_escape_sequence,
		function_id_alias,
		general,
		count
	};

private:
	static std::array<bool, static_cast<size_t>(Warning::count)> enabled_warnings;
	static std::array<int, static_cast<size_t>(Warning::count)>  promoted_warnings;
	static bool                                                  warnings_as_errors;
	static std::set<std::string>                                 source_names;    // Interned filenames; stable addresses.

	static const char*              cur_source;    // Source filename.
	static int                      cur_line;      // Line #.
	static int                      cur_column;    // Byte column, 1-based.
	static int                      cur_length;    // Length of token on its first line.
	static int                      next_line;
	static int                      next_column;
	static std::vector<Uc_location> diagnostic_context;
	static int                      num_errors;      // Total #.
	static bool                     strict_mode;     // If all blocks are required to have braces
	static bool                     color_output;    // If output should be colorized.

	const char* source;
	int         line;
	int         column;
	int         length;

	enum class Severity {
		error,
		warning,
		note
	};
	static void assemble_message(const char* msg, const char* source_file, int line_num, int column, int length, Severity severity);
	void        emit_warning(Warning category, const std::string& message) const;

public:
	class Context;

	struct preformatted_t {};

	constexpr static preformatted_t preformatted = {};

	// Use current location.
	Uc_location() : source(cur_source), line(cur_line), column(cur_column), length(cur_length) {
		if (!diagnostic_context.empty()) {
			*this = diagnostic_context.back();
		}
	}

	Uc_location(const char* s, int l, int c, int n) : source(s), line(l), column(c), length(n) {}

	static void set_cur(const char* s, int l, int c = 1);

	// Called once per lexer match, including whitespace and multiline strings.
	static void        advance(const char* text, int size);
	static Uc_location next_location();
	static void        register_source(const char* name, const char* path);

	static void set_strict_mode(bool tf) {
		strict_mode = tf;
	}

	static bool get_strict_mode() {
		return strict_mode;
	}

	int get_line() const {
		return line;
	}

	int get_column() const {
		return column;
	}

	int get_length() const {
		return length;
	}

	Uc_location at_offset(const char* text, size_t bytes, int token_length = 1) const;

	const char* get_source() const {
		return source;
	}

	static void set_color_output(bool tf) {
		color_output = tf;
	}

	static bool get_color_output() {
		return color_output;
	}

	static bool get_goto_warn() {
		return warning_enabled(Warning::goto_statement);
	}

	static void set_goto_warn(bool tf) {
		enabled_warnings[static_cast<size_t>(Warning::goto_statement)] = tf;
	}

	static bool get_integer_warn() {
		return warning_enabled(Warning::integer_coercion);
	}

	static void set_integer_warn(bool tf) {
		enabled_warnings[static_cast<size_t>(Warning::integer_coercion)] = tf;
	}

	static bool get_shapefun_warn() {
		return warning_enabled(Warning::shape_to_function);
	}

	static void set_shapefun_warn(bool tf) {
		enabled_warnings[static_cast<size_t>(Warning::shape_to_function)] = tf;
	}

	static bool warning_enabled(Warning category) {
		return enabled_warnings[static_cast<size_t>(category)];
	}

	static void set_all_warnings(bool enabled);
	static bool set_warning_option(std::string_view option);

	PRINTF_FORMAT(3, 4)
	void warning(Warning category, FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(2, 3)
	static void yywarning(Warning category, FORMAT_STRING(const char* format), ...);

	static void yyerror(preformatted_t tag, const char* s);
	static void yywarning(preformatted_t tag, const char* s);

	// Variadic versions.
	// On stack, fixed buffer.
	PRINTF_FORMAT(2, 3)
	void error(FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(2, 3)
	void warning(FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(2, 3)
	void note(FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(1, 2)
	static void yyerror(FORMAT_STRING(const char* format), ...);
	PRINTF_FORMAT(1, 2)
	static void yywarning(FORMAT_STRING(const char* format), ...);

	// On heap, user-specified buffer.
	PRINTF_FORMAT(3, 4)
	void error(std::string& buffer, FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(3, 4)
	void warning(std::string& buffer, FORMAT_STRING(const char* format), ...) const;
	PRINTF_FORMAT(2, 3)
	static void yyerror(std::string& buffer, FORMAT_STRING(const char* format), ...);
	PRINTF_FORMAT(2, 3)
	static void yywarning(std::string& buffer, FORMAT_STRING(const char* format), ...);

	static int get_num_errors() {
		return num_errors;
	}
};

// Semantic checks must not change the scanner's physical cursor. A context
// redirects diagnostics and default AST/symbol locations for its lifetime.
// Values are owned by the stack; no references into it survive a push/pop.
class Uc_location::Context {
public:
	explicit Context(Uc_location loc) {
		diagnostic_context.push_back(loc);
	}

	~Context() {
		diagnostic_context.pop_back();
	}

	Context(const Context&)            = delete;
	Context& operator=(const Context&) = delete;
};

#undef FORMAT_STRING
#undef PRINTF_FORMAT

#endif
