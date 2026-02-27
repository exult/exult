/**
 ** sfxinf.h - Sound Effect information from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

/*
Copyright (C) 2008-2025 The Exult Team

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

#ifndef INCL_SFXINF_H
#define INCL_SFXINF_H 1

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
 *  Information about shape sound effects.
 */
class SFX_info : public Base_info {
	int  sfxnum;
	bool random;    // sfx in range are to be randomly chosen.
	int  range;     // # of sequential sfx to be used.
	int  chance;    // % chance of playing the SFX.
	int  extra;     // For grandfather clock.
	int  volume;    // For volume of individual SFX
	int  frame;     // Frame number to play the SFX on

public:
	friend class Shape_info;
	// Read in from file.
	bool read(std::istream& in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream& out, int shapenum, Exult_Game game);

	int get_sfx() const {
		return sfxnum;
	}

	void set_sfx(int f) {
		if (sfxnum != f) {
			set_modified(true);
			sfxnum = f;
		}
	}

	bool play_sequentially() const {
		return !random;
	}

	bool play_randomly() const {
		return random;
	}

	void set_play_randomly(bool f) {
		if (random != f) {
			set_modified(true);
			random = f;
		}
	}

	int get_chance() const {
		return chance;
	}

	void set_chance(int f) {
		if (chance != f) {
			set_modified(true);
			chance = f;
		}
	}

	bool play_horly_ticks() const {
		return extra > -1;
	}

	int get_volume() const {
		return volume;
	}

	void set_volume(int vol) {
		if (volume != vol) {
			set_modified(true);
			volume = vol;
		}
	}

	int get_frame() const {
		return frame;
	}

	void set_frame(int fr) {
		if (frame != fr) {
			set_modified(true);
			frame = fr;
		}
	}

	int get_extra_sfx() const {
		return extra;
	}

	void set_extra_sfx(int f) {
		if (extra != f) {
			set_modified(true);
			extra = f;
		}
	}

	int get_sfx_range() const {
		return range;
	}

	void set_sfx_range(int f) {
		if (range != f) {
			set_modified(true);
			range = f;
		}
	}

	bool time_to_play() const;
	int  get_next_sfx(int& last) const;

	static int get_info_flag() {
		return 0x20;
	}

	enum {
		is_binary  = 0,
		entry_size = 0
	};
};

#endif
