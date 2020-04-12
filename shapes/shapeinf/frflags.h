/**
 ** frflags.h - Frame- and quality-based flags from 'shape_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_FRFLAGS_H
#define INCL_FRFLAGS_H  1

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

namespace Frame_flags {
enum Enum_Frame_power_flags {
    fp_poison_safe = 0,
    fp_charm_safe,
    fp_sleep_safe,
    fp_paralysis_safe,
    fp_curse_safe,
    fp_power_safe,
    fp_death_safe,
    fp_cant_die,
    fp_cold_immune,
    fp_doesnt_eat,
    fp_swamp_safe,
    fp_force_usecode,
    fp_infinite_reagents
};
enum Enum_Frame_Flags {
    poison_safe = (1u << fp_poison_safe),
    charm_safe = (1u << fp_charm_safe),
    sleep_safe = (1u << fp_sleep_safe),
    paralysis_safe = (1u << fp_paralysis_safe),
    curse_safe = (1u << fp_curse_safe),
    power_safe = (1u << fp_power_safe),
    death_safe = (1u << fp_death_safe),
    cant_die = (1u << fp_cant_die),
    cold_immune = (1u << fp_cold_immune),
    doesnt_eat = (1u << fp_doesnt_eat),
    swamp_safe = (1u << fp_swamp_safe),
    force_usecode = (1u << fp_force_usecode),
    infinite_reagents = (1u << fp_infinite_reagents)
};
}

class Frame_flags_info : public Base_info {
	short           frame;      // Frame for which this applies or -1 for any.
	short           quality;    // Quality for which this applies or -1 for any.
	unsigned int    m_flags;    // Bit field with the relevant flags.
public:
	friend class Shape_info;
	Frame_flags_info() = default;
	Frame_flags_info(short fr, short q, unsigned int fl, bool p = false,
	                 bool m = false, bool s = false, bool inv = false) {
		set(fr, q, fl, p, m, s, inv);
	}
	Frame_flags_info(const Frame_flags_info &other)
		: Base_info(other), frame(other.frame), quality(other.quality), m_flags(other.m_flags) {
		info_flags = other.info_flags;
	}
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	void set(short fr, short q, unsigned int fl, bool p = false,
	         bool m = false, bool s = false, bool inv = false) {
		frame = fr;
		quality = q;
		m_flags = fl;
		set_patch(p);
		set_modified(m);
		set_static(s);
		set_invalid(inv);
	}
	void invalidate() {
		m_flags = 0;
		set_invalid(true);
	}
	int get_frame() const {
		return frame;
	}
	int get_quality() const {
		return quality;
	}
	bool get_flag(int tf) const {
		return (m_flags & (1 << tf)) != 0;
	}
	int get_flags() const {
		return m_flags;
	}
	void set_flags(unsigned int fl) {
		if (m_flags != fl) {
			set_modified(true);
			m_flags = fl;
		}
	}
	bool operator<(const Frame_flags_info &other) const {
		unsigned short qual1 = static_cast<unsigned short>(quality);
		unsigned short qual2 = static_cast<unsigned short>(other.quality);
		unsigned short frame1 = static_cast<unsigned short>(frame);
		unsigned short frame2 = static_cast<unsigned short>(other.frame);
		return (frame1 == frame2 && qual1 < qual2) || (frame1 < frame2);
	}
	bool operator==(const Frame_flags_info &other) const {
		return this == &other || (!(*this < other) && !(other < *this));
	}
	bool operator!=(const Frame_flags_info &other) const {
		return !(*this == other);
	}
	Frame_flags_info &operator=(const Frame_flags_info &other) {
		if (this != &other) {
			frame = other.frame;
			quality = other.quality;
			m_flags = other.m_flags;
			info_flags = other.info_flags;
		}
		return *this;
	}
	void set(const Frame_flags_info &other) {
		// Assumes *this == other.
		// No need to guard against self-assignment.
		// Do NOT copy modified or static flags.
		set_patch(other.from_patch());
		set_invalid(other.is_invalid());
		set_flags(other.m_flags);
	}
	enum { is_binary = 0, entry_size = 0 };
};

#endif
