/**
 ** ammoinf.h - Information from 'ammo.dat'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_AMMOINF_H
#define INCL_AMMOINF_H  1

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
 *  Info. from 'ammo.dat':
 */
class Ammo_info : public Base_info {
private:
	static Ammo_info default_info;  // For shapes not found.
	int family_shape;       // I.e., burst-arrow's is 'arrow'.
	int sprite;             // What the missile should look like.
	unsigned char damage;       // Extra damage points.
	unsigned char powers;       // Same as for weapons.
	unsigned char damage_type;  // Same as for weapons.
	bool m_no_blocking;     // Can move through walls.
	unsigned char drop_type;    // What to do to missile when it hits/misses
	bool m_autohit;         // Weapon always hits.
	bool m_lucky;           // Easier to hit with.
	bool m_returns;         // Boomerang, magic axe.
	bool homing;        // For Energy Mist/Death Vortex.
	bool m_explodes;        // Burst arrows.
public:
	enum Drop_types {       // Determines what happens when the missile misses
	    drop_normally = 0,
	    never_drop = 1,
	    always_drop = 2
	};
	friend class Shapes_vga_file;
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	enum { is_binary = 1, entry_size = 13 };
	static const Ammo_info *get_default();
	int get_family_shape() const {
		return family_shape;
	}
	void set_family_shape(int f) {
		if (family_shape != f) {
			set_modified(true);
			family_shape = f;
		}
	}
	int get_sprite_shape() const {
		return sprite;
	}
	void set_sprite_shape(int f) {
		if (sprite != f) {
			set_modified(true);
			sprite = f;
		}
	}
	int get_damage() const {
		return damage;
	}
	int get_damage_type() const {
		return damage_type;
	}
	void set_damage(int dmg, int dtype) {
		if (damage != dmg || damage_type != dtype) {
			set_modified(true);
			damage = dmg;
			damage_type = dtype;
		}
	}
	unsigned char get_powers() const {
		return powers;
	}
	void set_powers(unsigned char p) {
		if (powers != p) {
			set_modified(true);
			powers = p;
		}
	}
	bool no_blocking() const {
		return m_no_blocking;
	}
	void set_no_blocking(bool tf) {
		if (m_no_blocking != tf) {
			set_modified(true);
			m_no_blocking = tf;
		}
	}
	unsigned char get_drop_type() const {
		return drop_type;
	}
	void set_drop_type(unsigned char drop) {
		if (drop_type != drop) {
			set_modified(true);
			drop_type = drop;
		}
	}
	bool autohits() const {
		return m_autohit;
	}
	void set_autohits(bool tf) {
		if (m_autohit != tf) {
			set_modified(true);
			m_autohit = tf;
		}
	}
	bool lucky() const {
		return m_lucky;
	}
	void set_lucky(bool tf) {
		if (m_lucky != tf) {
			set_modified(true);
			m_lucky = tf;
		}
	}
	bool returns() const {
		return m_returns;
	}
	void set_returns(bool tf) {
		if (m_returns != tf) {
			set_modified(true);
			m_returns = tf;
		}
	}
	bool is_homing() const {
		return homing;
	}
	void set_homing(bool sb) {
		if (homing != sb) {
			set_modified(true);
			homing = sb;
		}
	}
	bool explodes() const {
		return m_explodes;
	}
	void set_explodes(bool b) {
		if (m_explodes != b) {
			set_modified(true);
			m_explodes = b;
		}
	}
	static int get_info_flag() {
		return 2;
	}
	int get_base_strength() const;
};

#endif
