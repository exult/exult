/**
 ** Baseinf.h: General definitions for shape information.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_BASEINF_H
#define INCL_BASEINF_H 1

/*
Copyright (C) 2008  Exult Team

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

class Shape_info;

namespace Weapon_data {
	// Special weapon/ammo powers.
	enum Powers {
		sleep    = 1,
		charm    = 2,
		curse    = 4,
		poison   = 8,
		paralyze = 16,
		magebane = 32,    // Takes away mana.
		unknown  = 64,    // This enters XP formula, but can't
		// figure out *what* it does.
		no_damage = 128    // Weapon/missile causes no damage;
						   // also puts Draygan to sleep.
	};

	// Type of damage.  These also are the
	//   bit #'s in Monster_info's
	//   immune and vulerable fields.
	enum Damage_type {
		normal_damage     = 0,
		fire_damage       = 1,
		magic_damage      = 2,
		lightning_damage  = 3,
		poison_damage     = 3,
		starvation_damage = 3,
		freezing_damage   = 3,
		ethereal_damage   = 4,
		sonic_damage      = 5
	};
}    // namespace Weapon_data

/*
 *  Base class for all shape information classes.
 */

class Base_info {
protected:
	enum Info_bit_flags {
		All_clear     = 0,
		Info_modified = 1,
		From_patch    = 2,
		Have_static   = 4,
		Is_invalid    = 8
	};

	Info_bit_flags info_flags = All_clear;

	void set_flag(bool tf, int flag) {
		if (tf) {
			info_flags = static_cast<Info_bit_flags>(info_flags | flag);
		} else {
			info_flags = static_cast<Info_bit_flags>(info_flags & ~flag);
		}
	}

	bool get_flag(int flag) const {
		return (info_flags & flag) != 0;
	}

public:
	friend class Shape_info;
	Base_info() = default;

	Base_info(bool patch) : info_flags(patch ? From_patch : All_clear) {}

	Base_info(bool mod, bool patch, bool inv, bool st) {
		set_modified(mod);
		set_patch(patch);
		set_invalid(inv);
		set_static(st);
	}

	bool was_modified() const {
		return get_flag(Info_modified);
	}

	void set_modified(bool tf) {
		set_flag(tf, Info_modified);
	}

	bool from_patch() const {
		return get_flag(From_patch);
	}

	void set_patch(bool tf) {
		set_flag(tf, From_patch);
	}

	bool have_static() const {
		return get_flag(Have_static);
	}

	void set_static(bool tf) {
		set_flag(tf, Have_static);
	}

	bool is_invalid() const {
		return get_flag(Is_invalid);
	}

	void set_invalid(bool tf) {
		set_flag(tf, Is_invalid);
	}

	bool need_write() const {
		return get_flag(Info_modified) || get_flag(From_patch);
	}
};

#endif
