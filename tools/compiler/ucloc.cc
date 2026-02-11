/**
 ** Ucloc.cc - Source location.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ucloc.h"

#include "ignore_unused_variable_warning.h"

#include <cstdarg>
#include <cstring>
#include <iostream>
#include <string_view>

using std::cout;
using std::endl;
using std::strcmp;
using std::strcpy;
using std::strlen;

std::vector<char*> Uc_location::source_names;

char* Uc_location::cur_source    = nullptr;
int   Uc_location::cur_line      = 0;
int   Uc_location::num_errors    = 0;
bool  Uc_location::strict_mode   = false;
bool  Uc_location::color_output  = false;
bool  Uc_location::goto_warn     = true;
bool  Uc_location::integer_warn  = true;
bool  Uc_location::shapefun_warn = true;

/*
 *  Set current source and line #.
 */

void Uc_location::set_cur(const char* s, int l) {
	cur_line   = l;
	cur_source = nullptr;    // See if already here.
	for (auto* name : source_names) {
		if (strcmp(s, name) == 0) {
			cur_source = name;
			break;
		}
	}
	if (!cur_source) {    // 1st time.
		const int len = strlen(s);
		cur_source    = new char[len + 1];
		strcpy(cur_source, s);
		source_names.push_back(cur_source);
	}
}

void Uc_location::assemble_message(const char* msg, const char* source_file, int line_num, bool is_error) {
	// For colorization.
	constexpr static const std::string_view error_prefix   = "\033[1;31merror:\033[0m ";
	constexpr static const std::string_view warning_prefix = "\033[1;35mwarning:\033[0m ";
	constexpr static const std::string_view source_prefix  = "\033[1m";
	constexpr static const std::string_view line_prefix    = "\033[1;34m";
	constexpr static const std::string_view reset_style    = "\033[0m";
	if (color_output) {
		cout << source_prefix << source_file << ":" << reset_style << line_prefix << line_num << ": " << reset_style;
		if (is_error) {
			cout << error_prefix;
		} else {
			cout << warning_prefix;
		}
		cout << msg << reset_style << endl;
	} else {
		cout << source_file << ':' << line_num << ": ";
		if (is_error) {
			cout << "error: ";
		} else {
			cout << "warning: ";
		}
		cout << msg << endl;
	}
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(preformatted_t tag, const char* s) {
	ignore_unused_variable_warning(tag);
	assemble_message(s, cur_source, cur_line + 1, true);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(preformatted_t tag, const char* s) {
	ignore_unused_variable_warning(tag);
	assemble_message(s, cur_source, cur_line + 1, false);
}

constexpr const size_t BufferSize = 2048;

/*
 *  Print error for stored position.
 */

void Uc_location::error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	char s[BufferSize + 1]{};
	vsnprintf(s, BufferSize, format, args);
	assemble_message(s, source, line + 1, true);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for stored position.
 */

void Uc_location::warning(const char* format, ...) {
	va_list args;
	va_start(args, format);
	char s[BufferSize + 1]{};
	vsnprintf(s, BufferSize, format, args);
	assemble_message(s, source, line + 1, false);
	va_end(args);
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(const char* format, ...) {
	va_list args;
	va_start(args, format);
	char s[BufferSize + 1]{};
	vsnprintf(s, BufferSize, format, args);
	assemble_message(s, cur_source, cur_line + 1, true);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(const char* format, ...) {
	va_list args;
	va_start(args, format);
	char s[BufferSize + 1]{};
	vsnprintf(s, BufferSize, format, args);
	assemble_message(s, cur_source, cur_line + 1, false);
	va_end(args);
}

/*
 *  Print error for stored position.
 */

void Uc_location::error(std::string& buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer.data(), buffer.length(), format, args);
	assemble_message(buffer.c_str(), source, line + 1, true);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for stored position.
 */

void Uc_location::warning(std::string& buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer.data(), buffer.length(), format, args);
	assemble_message(buffer.c_str(), source, line + 1, false);
	va_end(args);
}

/*
 *  Print error for current parser location.
 */

void Uc_location::yyerror(std::string& buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer.data(), buffer.length(), format, args);
	assemble_message(buffer.c_str(), cur_source, cur_line + 1, true);
	va_end(args);
	num_errors++;
}

/*
 *  Print warning for current parser location.
 */

void Uc_location::yywarning(std::string& buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer.data(), buffer.length(), format, args);
	assemble_message(buffer.c_str(), cur_source, cur_line + 1, false);
	va_end(args);
}
