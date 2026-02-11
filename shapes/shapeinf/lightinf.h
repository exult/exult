/*
 * Copyright (C) 2020-2022 The Exult Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef INCL_LIGHTINF_H
#define INCL_LIGHTINF_H 1

#include "baseinf.h"
#include "exult_constants.h"

#include <iosfwd>

class Shape_info;

/*
 *  Information about object light.
 *  This is meant to be stored in a totally ordered vector.
 */
class Light_info : public Base_info {
	short frame;
	char  light;

public:
	friend class Shape_info;
	Light_info() = default;

	Light_info(short f, char l, bool p = false, bool m = false, bool s = false, bool inv = false)
			: Base_info(m, p, inv, s), frame(f), light(l) {}

	Light_info(const Light_info& other) : Base_info(other), frame(other.frame), light(other.light) {
		info_flags = other.info_flags;
	}

	// Read in from file.
	bool read(std::istream& in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream& out, int shapenum, Exult_Game game);

	void invalidate() {
		light = 0;
		set_invalid(true);
	}

	int get_frame() const {
		return frame;
	}

	int get_light() const {
		return light;
	}

	void set_light(int f) {
		if (light != f) {
			set_modified(true);
			light = f;
		}
	}

	bool operator<(const Light_info& other) const {
		return static_cast<unsigned short>(frame) < static_cast<unsigned short>(other.frame);
	}

	bool operator==(const Light_info& other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}

	bool operator!=(const Light_info& other) const {
		return !(*this == other);
	}

	Light_info& operator=(const Light_info& other) {
		if (this != &other) {
			frame      = other.frame;
			light      = other.light;
			info_flags = other.info_flags;
		}
		return *this;
	}

	void set(const Light_info& other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_light(other.light);
	}

	enum {
		is_binary  = 0,
		entry_size = 0
	};
};

#endif
