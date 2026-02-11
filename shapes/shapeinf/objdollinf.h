/**
 ** objdollinf.h - Object Paperdoll information from 'paperdol_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_OBJDOLLINF_H
#define INCL_OBJDOLLINF_H 1

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

#include <array>
#include <cstring>
#include <iosfwd>

class Shape_info;

/*
 *  Information about an object's paperdoll.
 *  This is meant to be stored in a totally ordered vector.
 */
class Paperdoll_item : public Base_info {
private:
	short world_frame;    // Frame in the world (-1 for all)
	short spot;           // Spot placed in
	short type;           // For weapons, the arm frame type to use.
	// For headgear, head frame to use.
	// Meaningless for all others.
	bool translucent;    // If the paperdoll should be drawn translucently or
						 // not
	bool gender;         // Is this object gender specific

	short                shape;    // The shape (if -1 use world shape and frame)
	std::array<short, 4> frame;    // The paperdoll frame and alternates.
public:
	friend class Shape_info;
	Paperdoll_item() = default;

	Paperdoll_item(
			short w, short sp, short ty, bool tr, bool g, short sh, short fr0, short fr1, short fr2, short fr3, bool p = false,
			bool m = false, bool s = false, bool inv = false)
			: Base_info(m, p, inv, s), world_frame(w), spot(sp), type(ty), translucent(tr), gender(g), shape(sh),
			  frame{fr0, fr1, fr2, fr3} {}

	// Read in from file.
	bool read(std::istream& in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream& out, int shapenum, Exult_Game game);

	void invalidate() {
		type = -255;
		set_invalid(true);
	}

	int get_world_frame() const {
		return world_frame;
	}

	int get_object_spot() const {
		return spot;
	}

	short get_spot_frame() const {
		return type;
	}

	void set_spot_frame(short f) {
		if (type != f) {
			set_modified(true);
			type = f;
		}
	}

	bool is_translucent() const {
		return translucent;
	}

	void set_translucent(bool tf) {
		if (translucent != tf) {
			set_modified(true);
			translucent = tf;
		}
	}

	bool is_gender_based() const {
		return gender;
	}

	void set_gender(bool tf) {
		if (gender != tf) {
			set_modified(true);
			gender = tf;
		}
	}

	int get_paperdoll_shape() const {
		return shape;
	}

	void set_paperdoll_shape(short s) {
		if (shape != s) {
			set_modified(true);
			shape = s;
		}
	}

	int get_paperdoll_baseframe() const {
		return frame[0];
	}

	int get_paperdoll_frame(int num) const {
		if (num >= 0 && num < 4) {
			return frame[num];
		}
		return get_paperdoll_baseframe();
	}

	void set_paperdoll_frame(int num, short fr) {
		if (frame[num] != fr) {
			set_modified(true);
			frame[num] = fr;
		}
	}

	void set_paperdoll_frames(short f0, short f1 = -1, short f2 = -1, short f3 = -1) {
		set_paperdoll_frame(0, f0);
		set_paperdoll_frame(1, f1);
		set_paperdoll_frame(2, f2);
		set_paperdoll_frame(3, f3);
	}

	bool operator<(const Paperdoll_item& other) const {
		auto wf1 = static_cast<unsigned short>(world_frame);
		auto wf2 = static_cast<unsigned short>(other.world_frame);
		return (wf1 < wf2) || (world_frame == other.world_frame && spot < other.spot);
	}

	bool operator==(const Paperdoll_item& other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}

	bool operator!=(const Paperdoll_item& other) const {
		return !(*this == other);
	}

	void set(const Paperdoll_item& other) {
		// Assumes *this == other.
		if (this != &other) {
			// Do NOT copy modified or static flags.
			set_patch(other.from_patch());
			set_invalid(other.is_invalid());
			set_spot_frame(other.type);
			set_translucent(other.translucent);
			set_gender(other.gender);
			set_paperdoll_shape(other.shape);
			if (frame != other.frame) {
				set_modified(true);
				frame = other.frame;
			}
		}
	}

	enum {
		is_binary  = 0,
		entry_size = 0
	};
};

#endif
