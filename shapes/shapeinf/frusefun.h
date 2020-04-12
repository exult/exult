/**
 ** frusefun.h - Frame- and quality-dependent usecode.
 **
 ** Written: 20/03/2009 - Marzo
 **/

#ifndef INCL_FRUSEFUN_H
#define INCL_FRUSEFUN_H 1

/*
Copyright (C) 2009 The Exult Team

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
#include <string>

#include <iosfwd>

class Shape_info;

/*
 *  Information about frame names.
 *  This is meant to be stored in a totally ordered vector.
 */
class Frame_usecode_info : public Base_info {
	short   frame;      // Frame for which this applies or -1 for any.
	short   quality;    // First quality for which this applies or -1 for any.
	int     usecode;    // Usecode function of the frame/quality at hand,
	// or -1 for default shape usecode.
	std::string usecode_name;       // Name of usecode fun explicitly assigned.
public:
	friend class Shape_info;
	Frame_usecode_info() = default;
	Frame_usecode_info(short f, short q, int ui, const char *nm, bool p = false,
	                   bool m = false, bool s = false, bool inv = false) {
		set(f, q, ui, nm, p, m, s, inv);
	}
	Frame_usecode_info(const Frame_usecode_info &other)
		: Base_info(other), frame(other.frame), quality(other.quality), usecode(other.usecode),
		  usecode_name(other.usecode_name) {
		info_flags = other.info_flags;
	}
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	void set(short f, short q, int ui, const char *nm, bool p = false,
	         bool m = false, bool s = false, bool inv = false) {
		frame = f;
		quality = q;
		usecode = ui;
		usecode_name = nm ? nm : "";
		set_patch(p);
		set_modified(m);
		set_static(s);
		set_invalid(inv);
	}
	void invalidate() {
		usecode = -1;
		usecode_name.clear();
		set_invalid(true);
	}
	int get_frame() const {
		return frame;
	}
	int get_quality() const {
		return quality;
	}
	int get_usecode() const {
		return usecode;
	}
	std::string get_usecode_name() const {
		return usecode_name;
	}
	void set_usecode(int ui, const char *nm) {
		if (usecode != ui ||
		        (!nm && usecode_name.length()) || (nm && usecode_name != nm)) {
			set_modified(true);
			usecode = ui;
			if (nm)
				usecode_name = nm;
			else
				usecode_name.clear();
		}
	}
	bool operator<(const Frame_usecode_info &other) const {
		unsigned short qual1 = static_cast<unsigned short>(quality);
		unsigned short qual2 = static_cast<unsigned short>(other.quality);
		unsigned short frame1 = static_cast<unsigned short>(frame);
		unsigned short frame2 = static_cast<unsigned short>(other.frame);
		return (frame1 == frame2 && qual1 < qual2) || (frame1 < frame2);
	}
	bool operator==(const Frame_usecode_info &other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}
	bool operator!=(const Frame_usecode_info &other) const {
		return !(*this == other);
	}
	Frame_usecode_info &operator=(const Frame_usecode_info &other) {
		if (this != &other) {
			frame = other.frame;
			quality = other.quality;
			usecode = other.usecode;
			usecode_name = other.usecode_name;
			info_flags = other.info_flags;
		}
		return *this;
	}
	void set(const Frame_usecode_info &other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_usecode(other.usecode, other.usecode_name.c_str());
	}
	enum { is_binary = 0, entry_size = 0 };
};

#endif
