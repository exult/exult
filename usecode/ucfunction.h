/*
 *  ucfunction.h - Usecode function
 *
 *  Copyright (C) 2002  The Exult Team
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

#ifndef UCFUNCTION_H
#define UCFUNCTION_H

#include <iosfwd>
#include <vector>
#include "useval.h"

class Usecode_function {
public:
	int id;             // The function #.  (Appears to be the
	//   game item # the function gets
	//   called for.)
	Usecode_function *orig;     // If this was from 'patch', orig will
	//   be the function this replaced (if
	//   it existed).
	int len;            // Length.

	bool extended; // is this an 'extented' function? (aka 32 bit function)
	unsigned char *code;        // The code.
	std::vector<Usecode_value> statics; // Local statics.
	// Create from file.
	Usecode_function(std::istream &file);
	inline ~Usecode_function() {
		delete [] code;
	}
};

#endif
