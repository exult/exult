/**
 ** effhpinf.h - Effective HP information from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_EFFHPINF_H
#define INCL_EFFHPINF_H 1

/*
Copyright (C) 2008 The Exult Team

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
 *  Information about effective HPs.
 *  This is meant to be stored in a totally ordered vector.
 */
class Effective_hp_info : public Base_info {
	short   frame;
	short   quality;
	char    hps;
public:
	friend class Shape_info;
	Effective_hp_info() = default;
	Effective_hp_info(short f, short q, char h, bool p = false, bool m = false,
	                  bool s = false, bool inv = false) {
		set(f, q, h, p, m, s, inv);
	}
	Effective_hp_info(const Effective_hp_info &other)
		: Base_info(other), frame(other.frame), quality(other.quality), hps(other.hps) {
		info_flags = other.info_flags;
	}
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	void set(short f, short q, char h, bool p = false, bool m = false,
	         bool s = false, bool inv = false) {
		frame = f;
		quality = q;
		hps = h;
		set_patch(p);
		set_modified(m);
		set_static(s);
		set_invalid(inv);
	}
	void invalidate() {
		hps = 0;
		set_invalid(true);
	}
	int get_frame() const {
		return frame;
	}
	int get_quality() const {
		return quality;
	}
	int get_hps() const {
		return hps;
	}
	void set_hps(int f) {
		if (hps != f) {
			set_modified(true);
			hps = f;
		}
	}
	bool operator<(const Effective_hp_info &other) const {
		unsigned short qual1 = static_cast<unsigned short>(quality);
		unsigned short qual2 = static_cast<unsigned short>(other.quality);
		unsigned short frame1 = static_cast<unsigned short>(frame);
		unsigned short frame2 = static_cast<unsigned short>(other.frame);
		return (frame1 == frame2 && qual1 < qual2) || (frame1 < frame2);
	}
	bool operator==(const Effective_hp_info &other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}
	bool operator!=(const Effective_hp_info &other) const {
		return !(*this == other);
	}
	Effective_hp_info &operator=(const Effective_hp_info &other) {
		if (this != &other) {
			frame = other.frame;
			quality = other.quality;
			hps = other.hps;
			info_flags = other.info_flags;
		}
		return *this;
	}
	void set(const Effective_hp_info &other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_hps(other.hps);
	}
	enum { is_binary = 0, entry_size = 0 };
};

#endif
