/**
 ** objdollinf.cc - Object Paperdoll information from 'paperdol_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

/*
Copyright (C) 2008-2022 The Exult Team

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

#include "objdollinf.h"

#include "exult_constants.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

using std::istream;

bool Paperdoll_item::read(
		std::istream& in,         // Input stream.
		int           version,    // Data file version.
		Exult_Game    game        // Loading BG file.
) {
	ignore_unused_variable_warning(game);
	world_frame  = ReadInt(in);
	translucent  = ReadInt(in) != 0;
	spot         = ReadInt(in);
	const int ty = ReadInt(in);
	if (ty == -255) {
		// 'Invalid' marker.
		set_invalid(true);
		return true;    // Ignore remainder of the line.
	}
	if (spot != 0 && spot != 3) {    // Field only valid for these spots.
		type = 0;                    // Ignore it.
	} else if (version == 1) {
		switch (ty) {    // Convert old data.
		case 2:
		case 7:
			type = 1;
			break;
		case 3:
			type = 2;
			break;
		default:
			type = 0;
			break;
		}
	} else {
		type = ty;
	}
	gender   = ReadInt(in) != 0;
	shape    = ReadInt(in);
	frame[0] = ReadInt(in);
	// Not all items have all entries; those that need, do, though.
	frame[1] = ReadInt(in, -1);
	frame[2] = ReadInt(in, -1);
	frame[3] = ReadInt(in, -1);
	return true;
}
