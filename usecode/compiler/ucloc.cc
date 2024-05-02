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

#include <cstring>
#include <iostream>

using std::cout;
using std::endl;
using std::strcmp;
using std::strcpy;
using std::strlen;

std::vector<char*> Uc_location::source_names;
char*              Uc_location::cur_source  = nullptr;
int                Uc_location::cur_line    = 0;
int                Uc_location::num_errors  = 0;
bool               Uc_location::strict_mode = false;

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

/*
 *  Print error for stored position.
 */

void Uc_location::error(const char* s) {
	cout << source << ':' << line + 1 << ": " << s << endl;
	num_errors++;
}

/*
 *  Print warning for stored position.
 */

void Uc_location::warning(const char* s) {
	cout << source << ':' << line + 1 << ": " << "Warning: " << s << endl;
}

/*
 *  Print error for current parse location.
 */

void Uc_location::yyerror(const char* s) {
	cout << cur_source << ':' << cur_line + 1 << ": " << s << endl;
	num_errors++;
}

void Uc_location::yywarning(const char* s) {
	cout << cur_source << ':' << cur_line + 1 << ": " << "Warning: " << s
		 << endl;
}
