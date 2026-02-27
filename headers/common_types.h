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

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

//
// {s,u}int{8,16,32,ptr}
//

#include <cstdint>

// Note: u?int{8,16,32,64,ptr}_t types from <cstdint> are mandatory in any
// systems which directly support them.

using uint8   = uint8_t;
using uint16  = uint16_t;
using uint32  = uint32_t;
using uint64  = uint64_t;
using uintptr = uintptr_t;
using sint8   = int8_t;
using sint16  = int16_t;
using sint32  = int32_t;
using sint64  = int64_t;
using sintptr = intptr_t;

// Debug
#ifdef DEBUG
#	ifndef COUT
#		define COUT(x)                      \
			do {                             \
				std::cout << x << std::endl; \
				std::cout.flush();           \
			} while (0)
#	endif
#	ifndef CERR
#		define CERR(x)                      \
			do {                             \
				std::cerr << x << std::endl; \
				std::cerr.flush();           \
			} while (0)
#	endif
#else
#	ifndef COUT
#		define COUT(x) \
			do {        \
			} while (0)
#	endif
#	ifndef CERR
#		define CERR(x) \
			do {        \
			} while (0)
#	endif
#endif

#endif    // COMMON_TYPES_H
