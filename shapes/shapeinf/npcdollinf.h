/**
 ** npcdollinf.h - NPC Paperdoll information from 'paperdol_info.txt'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_NPCDOLLINF_H
#define INCL_NPCDOLLINF_H 1

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
class Paperdoll_npc_functor;

/*
 *  Information about an NPC's paperdoll.
 */
class Paperdoll_npc : public Base_info {
	bool is_female;      // Is the NPC Female (or more specifically not male)
	bool translucent;    // If the paperdoll should be drawn translucently or
						 // not

	// Body info
	short body_shape;    // Body Shape
	short body_frame;    // Body Frame

	short head_shape;         // Head Shape
	short head_frame;         // Normal Head Frame
	short head_frame_helm;    // Frame when wearing a helm

	short arms_shape;       // Shape for Arms
	short arms_frame[3];    // Frames for arms.
public:
	friend class Shape_info;
	friend class Paperdoll_npc_functor;
	// Read in from file.
	bool read(std::istream& in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream& out, int shapenum, Exult_Game game);

	bool is_npc_female() const {
		return is_female;
	}

	void set_is_female(bool tf) {
		if (is_female != tf) {
			set_modified(true);
			is_female = tf;
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

	int get_body_shape() const {
		return body_shape;
	}

	void set_body_shape(short s) {
		if (body_shape != s) {
			set_modified(true);
			body_shape = s;
		}
	}

	int get_body_frame() const {
		return body_frame;
	}

	void set_body_frame(short s) {
		if (body_frame != s) {
			set_modified(true);
			body_frame = s;
		}
	}

	int get_head_shape() const {
		return head_shape;
	}

	void set_head_shape(short s) {
		if (head_shape != s) {
			set_modified(true);
			head_shape = s;
		}
	}

	int get_head_frame() const {
		return head_frame;
	}

	void set_head_frame(short s) {
		if (head_frame != s) {
			set_modified(true);
			head_frame = s;
		}
	}

	int get_head_frame_helm() const {
		return head_frame_helm;
	}

	void set_head_frame_helm(short s) {
		if (head_frame_helm != s) {
			set_modified(true);
			head_frame_helm = s;
		}
	}

	int get_arms_shape() const {
		return arms_shape;
	}

	void set_arms_shape(short s) {
		if (arms_shape != s) {
			set_modified(true);
			arms_shape = s;
		}
	}

	int get_arms_frame(int type) const {
		return arms_frame[type];
	}

	void set_arms_frame(int type, short s) {
		if (arms_frame[type] != s) {
			set_modified(true);
			arms_frame[type] = s;
		}
	}

	void set_arms_frames(short a0, short a1, short a2) {
		set_arms_frame(0, a0);
		set_arms_frame(1, a1);
		set_arms_frame(2, a2);
	}

	static int get_info_flag() {
		return 0x80;
	}

	enum {
		is_binary  = 0,
		entry_size = 0
	};
};

#endif
