/**
 ** expinf.h - Explosion information from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_EXPINF_H
#define INCL_EXPINF_H 1

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

#include "baseinf.h"
#include "exult_constants.h"

#include <iosfwd>

class Shape_info;

/*
 *  Information about explosions.
 */
class Explosion_info : public Base_info {
	int sprite;    // Explosion sprite.
	int sfxnum;    // SFX to play or 255 for none.
public:
	friend class Shape_info;
	// Read in from file.
	bool read(std::istream& in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream& out, int shapenum, Exult_Game game);

	void set(int shape, int sfx) {
		if (sfxnum != sfx || sprite != shape) {
			set_modified(true);
			sfxnum = sfx;
			sprite = shape;
		}
	}

	int get_sprite() const {
		return sprite;
	}

	int get_sfx() const {
		return sfxnum;
	}

	static int get_info_flag() {
		return 0x10;
	}

	enum {
		is_binary  = 0,
		entry_size = 0
	};
};

#endif
