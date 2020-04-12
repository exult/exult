/**
 ** frnameinf.h - Frame name information from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_FRNAMEINF_H
#define INCL_FRNAMEINF_H    1

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
 *  Information about frame names.
 *  This is meant to be stored in a totally ordered vector.
 */
class Frame_name_info : public Base_info {
	short   frame;      // Frame for which this applies or -1 for any.
	short   quality;    // Quality for which this applies or -1 for any.
	short   type;       // How the entry is used.
	int     msgid;      // Item name index in misc_names.
	int     othermsg;   // Suffix/prefix or default message, depending on type
public:
	friend class Shape_info;
	Frame_name_info() = default;
	Frame_name_info(short f, short q, short ty, int msg, int ot, bool p = false,
	                bool m = false, bool s = false, bool inv = false) {
		set(f, q, ty, msg, ot, p, m, s, inv);
	}
	Frame_name_info(const Frame_name_info &other)
		: Base_info(other), frame(other.frame), quality(other.quality), type(other.type),
		  msgid(other.msgid), othermsg(other.othermsg) {
		info_flags = other.info_flags;
	}
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	void set(short f, short q, short ty, int msg, int ot, bool p = false,
	         bool m = false, bool s = false, bool inv = false) {
		frame = f;
		quality = q;
		type = ty;
		msgid = msg;
		othermsg = ot;
		set_patch(p);
		set_modified(m);
		set_static(s);
		set_invalid(inv);
	}
	void invalidate() {
		type = -255;
		set_invalid(true);
	}
	int get_frame() const {
		return frame;
	}
	int get_quality() const {
		return quality;
	}
	short get_type() const {
		return type;
	}
	void set_type(short c) {
		if (type != c) {
			set_modified(true);
			type = c;
		}
	}
	int get_msgid() const {
		return msgid;
	}
	void set_msgid(int msg) {
		if (msgid != msg) {
			set_modified(true);
			msgid = msg;
		}
	}
	int get_othermsg() const {
		return othermsg;
	}
	void set_othermsg(int msg) {
		if (othermsg != msg) {
			set_modified(true);
			othermsg = msg;
		}
	}
	bool operator<(const Frame_name_info &other) const {
		unsigned short qual1 = static_cast<unsigned short>(quality);
		unsigned short qual2 = static_cast<unsigned short>(other.quality);
		unsigned short frame1 = static_cast<unsigned short>(frame);
		unsigned short frame2 = static_cast<unsigned short>(other.frame);
		return (frame1 == frame2 && qual1 < qual2) || (frame1 < frame2);
	}
	bool operator==(const Frame_name_info &other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}
	bool operator!=(const Frame_name_info &other) const {
		return !(*this == other);
	}
	Frame_name_info &operator=(const Frame_name_info &other) {
		if (this != &other) {
			frame = other.frame;
			quality = other.quality;
			type = other.type;
			msgid = other.msgid;
			othermsg = other.othermsg;
			info_flags = other.info_flags;
		}
		return *this;
	}
	void set(const Frame_name_info &other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_type(other.type);
		set_msgid(other.msgid);
		set_othermsg(other.othermsg);
	}
	enum { is_binary = 0, entry_size = 0 };
};

#endif
