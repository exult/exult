/*
Copyright (C) 2003-2004 The Pentagram Team
Copyright (C) 2005-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// istring.h -- case insensitive stl strings

#ifndef ISTRING_H
#define ISTRING_H

#include <algorithm>
#include <cctype>
#include <string_view>

namespace Pentagram {

	int strcasecmp(const char* s1, const char* s2);
	int strncasecmp(const char* s1, const char* s2, std::size_t length);

	inline char toupper(char c) {
		return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}

	inline bool iequals(std::string_view lhs, std::string_view rhs) {
		return std::equal(
				lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
				[](char left, char right) {
					return Pentagram::toupper(left)
						   == Pentagram::toupper(right);
				});
	}
}    // namespace Pentagram

#endif
