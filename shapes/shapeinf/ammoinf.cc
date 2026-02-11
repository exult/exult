/**
 ** ammoinf.h - Information from 'ammo.dat'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

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

#include "ammoinf.h"

#include "common_types.h"
#include "endianio.h"
#include "exult_constants.h"
#include "ignore_unused_variable_warning.h"

#include <istream>

using std::istream;

Ammo_info Ammo_info::default_info;

const Ammo_info* Ammo_info::get_default() {
	if (default_info.family_shape == 0) {
		default_info.family_shape = default_info.sprite = -1;
		default_info.damage = default_info.powers = default_info.damage_type = default_info.drop_type = 0;
		default_info.m_no_blocking = default_info.m_autohit = default_info.m_lucky = default_info.m_returns = default_info.homing
				= default_info.m_explodes                                                                   = false;
	}
	return &default_info;
}

int Ammo_info::get_base_strength() const {
	// ++++The strength values are utter guesses.
	int strength = damage;
	// These 4 get picked with about the same odds.
	strength += (powers & Weapon_data::no_damage) != 0 ? 10 : 0;
	strength += (powers & Weapon_data::sleep) != 0 ? 10 : 0;
	strength += (powers & Weapon_data::paralyze) != 0 ? 10 : 0;
	strength += (powers & Weapon_data::charm) != 0 ? 10 : 0;
	// These have slightly lower odds.
	strength += (powers & Weapon_data::poison) != 0 ? 5 : 0;
	strength += (powers & Weapon_data::curse) != 0 ? 5 : 0;
	strength += (powers & Weapon_data::magebane) != 0 ? 5 : 0;
	strength += m_lucky ? 5 : 0;
	strength += damage_type != Weapon_data::normal_damage ? 5 : 0;
	if (m_autohit) {
		strength *= 2;    // These are almost unfair...
	}
	if (m_no_blocking) {
		strength *= 2;    // ... and these get picked a lot more often.
	}
	return strength;
}

/*
 *  Read in an ammo-info entry from 'ammo.dat'.
 *
 *  Output: Shape # this entry describes.
 */

bool Ammo_info::read(
		std::istream& in,         // Input stream.
		int           version,    // Data file version.
		Exult_Game    game        // Loading BG file.
) {
	ignore_unused_variable_warning(version, game);
	uint8 buf[Ammo_info::entry_size - 2];    // Entry length.
	in.read(reinterpret_cast<char*>(buf), sizeof(buf));
	const uint8* ptr = buf;
	if (buf[Ammo_info::entry_size - 3] == 0xff) {    // means delete entry.
		set_invalid(true);
		return true;
	}
	family_shape               = little_endian::Read2(ptr);
	sprite                     = little_endian::Read2(ptr);    // How the missile looks like
	damage                     = Read1(ptr);
	const unsigned char flags0 = Read1(ptr);
	m_lucky                    = ((flags0) & 1) != 0;
	m_autohit                  = ((flags0 >> 1) & 1) != 0;
	m_returns                  = ((flags0 >> 2) & 1) != 0;
	m_no_blocking              = ((flags0 >> 3) & 1) != 0;
	homing                     = ((flags0 >> 4) & 3) == 3;
	drop_type                  = homing ? 0 : (flags0 >> 4) & 3;
	m_explodes                 = ((flags0 >> 6) & 1) != 0;
	Read1(ptr);    // 1 unknown.
	const unsigned char flags1 = Read1(ptr);
	damage_type                = (flags1 >> 4) & 15;
	powers                     = Read1(ptr);
	// Last 2 unknown.
	return true;
}
