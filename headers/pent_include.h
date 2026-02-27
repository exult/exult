/*
 *  pent_include.h - Basic Pentagram types, and precompiled header
 *
 *  Copyright (C) 2002, 2003  The Pentagram Team
 *  Copyright (C) 2005-2022  The Exult Team
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

#ifndef PENT_INCLUDE_H
#define PENT_INCLUDE_H

// Include config.h first if we have it
#ifdef HAVE_CONFIG_H
#	include <config.h>    // IWYU pragma: export
#endif

#include "common_types.h"    // IWYU pragma: export
#include "istring.h"         // IWYU pragma: export

#include <cassert>     // IWYU pragma: export
#include <iostream>    // IWYU pragma: export

#define pout std::cout
#define perr std::cerr

//
// Debugging
//
#ifdef DEBUG
#	define POUT(x)                 \
		do {                        \
			pout << x << std::endl; \
			pout.flush();           \
		} while (0)
#	define PERR(x)                 \
		do {                        \
			perr << x << std::endl; \
			perr.flush();           \
		} while (0)
#else
#	define POUT(x) \
		do {        \
		} while (0)
#	define PERR(x) \
		do {        \
		} while (0)
#endif

////////////////////
//                //
// Class Wrappers //
//                //
////////////////////

//
// DataSources
//

#include "databuf.h"    // IWYU pragma: export

#endif    // PENT_INCLUDE_H
