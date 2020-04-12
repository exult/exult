/**
 ** continf.h - Container rule information from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_CONTINF_H
#define INCL_CONTINF_H  1

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
 *  Information about shapes accepted/rejected by containers.
 *  This is meant to be stored in a totally ordered vector.
 */
class Content_rules : public Base_info {
	int     shape;
	bool    accept;
public:
	friend class Shape_info;
	Content_rules() = default;
	Content_rules(int sh, bool a, bool p = false, bool m = false,
	              bool st = false, bool inv = false) {
		set(sh, a, p, m, st, inv);
	}
	Content_rules(const Content_rules &other)
		: Base_info(other), shape(other.shape), accept(other.accept) {
		info_flags = other.info_flags;
	}
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	void set(int sh, bool a, bool p = false, bool m = false,
	         bool st = false, bool inv = false) {
		shape = sh;
		accept = a;
		set_patch(p);
		set_modified(m);
		set_static(st);
		set_invalid(inv);
	}
	void invalidate() {
		accept = true;
		set_invalid(true);
	}
	int get_shape() const {
		return shape;
	}
	bool accepts_shape() const {
		return accept;
	}
	void set_accept(bool tf) {
		if (accept != tf) {
			set_modified(true);
			accept = tf;
		}
	}
	bool operator<(const Content_rules &other) const {
		unsigned short shp1 = static_cast<unsigned short>(shape);
		unsigned short shp2 = static_cast<unsigned short>(other.shape);
		return shp1 < shp2;
	}
	bool operator==(const Content_rules &other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}
	bool operator!=(const Content_rules &other) const {
		return !(*this == other);
	}
	Content_rules &operator=(const Content_rules &other) {
		if (this != &other) {
			shape = other.shape;
			accept = other.accept;
			info_flags = other.info_flags;
		}
		return *this;
	}
	void set(const Content_rules &other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_accept(other.accept);
	}
	enum { is_binary = 0, entry_size = 0 };
};

#endif
