/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003 The Pentagram Team
Copyright (C) 2005-2022 The Exult Team

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

// istring.cpp -- case insensitive stl strings

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "istring.h"

#include <cctype>
#include <limits>

namespace Pentagram {

	int strncasecmp(const char* s1, const char* s2, std::size_t length) {
		sint32 c1;

		do {
			if ((length--) == 0) {
				return 0;    // strings are equal until end point
			}
			c1        = static_cast<unsigned char>(*s1++);
			sint32 c2 = static_cast<unsigned char>(*s2++);

			if (c1 != c2) {
				c1 = std::toupper(c1);
				c2 = std::toupper(c2);
				if (c1 != c2) {
					return (c1 < c2) ? -1 : 1;    // strings not equal
				}
			}
		} while (c1 != 0);

		return 0;    // strings are equal
	}

	int strcasecmp(const char* s1, const char* s2) {
		return strncasecmp(s1, s2, std::numeric_limits<std::size_t>::max());
	}

}    // namespace Pentagram
