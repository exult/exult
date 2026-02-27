/**
 ** aniinf.cc - Animation information from 'shape_info.txt'.
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

#include "aniinf.h"

#include "exult_constants.h"
#include "ignore_unused_variable_warning.h"
#include "utils.h"

using std::istream;

/*
 *  Read in a animation-cycle-info entry from 'shape_inf.txt'.
 *
 *  Output: Shape # this entry describes.
 */

bool Animation_info::read(
		std::istream& in,         // Input stream.
		int           version,    // Data file version.
		Exult_Game    game        // Loading BG file.
) {
	ignore_unused_variable_warning(game);
	if (version < 5) {    // Not compatible with old system.
		return false;
	}
	const int ty = ReadInt(in);
	if (ty == -0xff) {    // means delete entry.
		set_invalid(true);
		return true;
	}
	set(static_cast<AniType>(ty), ReadInt(in));    // Sensible defaults.
	if (type != FA_HOURLY) {
		// We still have things to read.
		frame_delay = ReadInt(in);
		sfx_delay   = ReadInt(in);
		if (type == FA_LOOPING) {
			freeze_first = ReadInt(in);
			recycle      = ReadInt(in);
		}
	}
	return true;
}

Animation_info* Animation_info::create_from_tfa(int type, int nframes) {
	auto* inf = new Animation_info();

	switch (type) {
	case 0:
	case 1:
		inf->set(FA_TIMESYNCHED, nframes);
		break;
	case 5:
		inf->set(FA_LOOPING, nframes, 0, 20, 1, -1);
		break;
	case 6:
		inf->set(FA_RANDOM_FRAMES, nframes);
		break;
	case 8:
		inf->set(FA_HOURLY, nframes);
		break;
	case 9:
		inf->set(FA_LOOPING, nframes, 8);
		break;
	case 10:
		inf->set(FA_LOOPING, nframes, 6);
		break;
	case 11:
		inf->set(FA_LOOPING, nframes, nframes - 1, 0);
		break;
	case 12:    // Slow advance.
	case 14:    // Grandfather clock.
		inf->set(FA_TIMESYNCHED, nframes, 0, 100, 4, -1);
		break;
	case 13:
		inf->set(FA_NON_LOOPING, nframes);
		break;
	case 15:
		inf->set(FA_TIMESYNCHED, 6, 0, 100, 4, 0);
		break;
	default:
		// Not handled yet. These would be cases 2, 3, 4, 7 and 12:
		// case 2 seems to be equal to case 0/1 except that
		//      frames advance randomly (maybe 1 in 4 chance)
		// case 3 is very strange. I have noted no pattern yet.
		// case 4 seems to be for case 3 what case 2 is for case 0/1.
		// case 7 toggles bit 0 of the frame (i.e., new_frame == frame ^ 1)
		// None of these are used for any animated shape, which is why
		// I haven't bothered implementing them.
		delete inf;
		return nullptr;
	}
	inf->info_flags = static_cast<Info_bit_flags>(0);
	return inf;
}
