/*
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include "common_types.h"

#include <cstring>
#include <unordered_map>    // IWYU pragma: export
#include <unordered_set>    // IWYU pragma: export

/*
 *  Hash function for strings:
 */
struct hashstr {
	uint32 operator()(const char* str) const noexcept {
		const uint32 m      = 4294967291u;
		uint32       result = 0;
		for (; *str != '\0'; ++str) {
			result = ((result << 8) + *str) % m;
		}
		return result;
	}
};

/*
 *  For testing if two strings match:
 */
struct eqstr {
	bool operator()(const char* s1, const char* s2) const {
		return std::strcmp(s1, s2) == 0;
	}
};

#endif /* _HASH_UTILS_H_ */
