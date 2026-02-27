/**
 ** npcdollinf.cc - NPC Paperdoll information from 'paperdol_info.txt'.
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

#include "npcdollinf.h"

#include "exult_constants.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

using std::istream;

bool Paperdoll_npc::read(
		std::istream& in,         // Input stream.
		int           version,    // Data file version.
		Exult_Game    game        // Loading BG file.
) {
	ignore_unused_variable_warning(version, game);
	const int sexflag = ReadInt(in);
	if (sexflag == -0xff) {    // means delete entry.
		set_invalid(true);
		return true;
	}
	is_female       = sexflag != 0;
	translucent     = ReadInt(in) != 0;
	body_shape      = ReadInt(in);
	body_frame      = ReadInt(in);
	head_shape      = ReadInt(in);
	head_frame      = ReadInt(in);
	head_frame_helm = ReadInt(in);
	arms_shape      = ReadInt(in);
	arms_frame[0]   = ReadInt(in);
	arms_frame[1]   = ReadInt(in);
	arms_frame[2]   = ReadInt(in);
	return true;
}
