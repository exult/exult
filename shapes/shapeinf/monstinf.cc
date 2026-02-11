/**
 ** Monstinf.cc - Information (about NPC's, really) from 'monster.dat'.
 **
 ** Written: 8/13/01 - JSF
 **/

/*
Copyright (C) 2000-2022 The Exult Team

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

#include "monstinf.h"

#include "common_types.h"
#include "endianio.h"
#include "exult_constants.h"
#include "ignore_unused_variable_warning.h"

#include <istream>

using std::istream;

std::vector<Equip_record> Monster_info::equip;
Monster_info              Monster_info::default_info;

/*
 *  Read in monster info. from 'monsters.dat'.
 *
 *  Output: Shape # this entry describes.
 */

bool Monster_info::read(
		std::istream& in,         // Input stream.
		int           version,    // Data file version.
		Exult_Game    game        // Loading BG file.
) {
	ignore_unused_variable_warning(version);
	uint8 buf[Monster_info::entry_size - 2];    // Entry length.
	in.read(reinterpret_cast<char*>(buf), sizeof(buf));
	uint8* ptr = buf;
	if (buf[Monster_info::entry_size - 3] == 0xff) {    // means delete entry.
		set_invalid(true);
		return true;
	}
	uint8 value  = Read1(ptr);    // Byte 2.
	m_sleep_safe = (value & 1) != 0;
	m_charm_safe = (value & 2) != 0;
	strength     = (value >> 2) & 63;

	value            = Read1(ptr);    // Byte 3.
	m_curse_safe     = (value & 1) != 0;
	m_paralysis_safe = (value & 2) != 0;
	dexterity        = (value >> 2) & 63;

	value         = Read1(ptr);          // Byte 4.
	m_poison_safe = (value & 1) != 0;    // verified
	m_int_b1      = (value & 2) != 0;    // What does this do?
	intelligence  = (value >> 2) & 63;

	value     = Read1(ptr);    // Byte 5.
	alignment = value & 3;
	combat    = (value >> 2) & 63;

	value        = Read1(ptr);    // Byte 6 (slimes).
	m_splits     = (value & 1) != 0;
	m_cant_die   = (value & 2) != 0;
	m_power_safe = (value & 4) != 0;
	m_death_safe = (value & 8) != 0;
	armor        = (value >> 4) & 15;

	Read1(ptr);    // Byte 7: Unknown.

	value      = Read1(ptr);    // Byte 8 - weapon reach.
	reach      = value & 15;
	weapon     = (value >> 4) & 15;
	flags      = Read1(ptr);    // Byte 9.
	vulnerable = Read1(ptr);    // Byte 10.
	immune     = Read1(ptr);    // Byte 11.

	value        = Read1(ptr);    // Byte 12.
	m_cant_yell  = (value & (1 << 5)) != 0;
	m_cant_bleed = (value & (1 << 6)) != 0;

	value        = Read1(ptr);    // Byte 13: partly unknown.
	m_attackmode = (value & 7);
	if (m_attackmode == 0) {    // Fixes invalid data saved by older
		m_attackmode = 2;       // versions of ES.
	} else {
		m_attackmode--;
	}
	m_byte13 = value & ~7;

	equip_offset = Read1(ptr);    // Byte 14.

	value              = Read1(ptr);    // Exult's extra flags: byte 15.
	m_can_teleport     = (value & 1) != 0;
	m_can_summon       = (value & 2) != 0;
	m_can_be_invisible = (value & 4) != 0;

	Read1(ptr);    // Byte 16: Unknown (0).
	const int sfx_delta = game == BLACK_GATE ? -1 : 0;
	sfx                 = Read1s(ptr) + sfx_delta;    // Byte 17.
	return true;
}

/*
 *  Get a default block for generic NPC's.
 */

const Monster_info* Monster_info::get_default() {
	if (default_info.strength == 0u) {    // First time?
		default_info.strength = default_info.dexterity = default_info.intelligence = default_info.combat = 10;
		default_info.alignment                                                                           = 0;    // Neutral.
		default_info.armor = default_info.weapon = 0;
		default_info.reach                       = 3;
		default_info.flags                       = (1 << static_cast<int>(walk));
		default_info.equip_offset                = 0;
		default_info.immune = default_info.vulnerable = default_info.m_byte13 = 0;
		default_info.m_can_be_invisible = default_info.m_can_summon = default_info.m_can_teleport = default_info.m_cant_bleed
				= default_info.m_cant_die = default_info.m_cant_yell = default_info.m_poison_safe = default_info.m_charm_safe
				= default_info.m_sleep_safe = default_info.m_paralysis_safe = default_info.m_curse_safe = default_info.m_power_safe
				= default_info.m_death_safe = default_info.m_int_b1 = default_info.m_splits = false;
		default_info.m_attackmode                                                           = 2;
	}
	return &default_info;
}

/*
 *  Set all the stats.
 */

void Monster_info::set_stats(int str, int dex, int intel, int cmb, int armour, int wpn, int rch) {
	if (strength != str || dexterity != dex || intelligence != intel || combat != cmb || armor != armour || weapon != wpn
		|| reach != rch) {
		set_modified(true);
	}
	strength     = str;
	dexterity    = dex;
	intelligence = intel;
	combat       = cmb;
	armor        = armour;
	weapon       = wpn;
	reach        = rch;
}

/*
 *  How much experience the monster is worth. Ignores stats,
 *  immunities and vulnerabilities (check Actor::reduce_health
 *  for why).
 */

int Monster_info::get_base_xp_value() const {
	// This formula is exact.
	int expval = armor + weapon;
	expval += m_sleep_safe ? 1 : 0;
	expval += m_charm_safe ? 1 : 0;
	expval += m_curse_safe ? 1 : 0;
	expval += m_paralysis_safe ? 1 : 0;
	expval += m_poison_safe ? 1 : 0;
	// Don't know what this does, but it *does* add to XP.
	expval += m_int_b1 ? 1 : 0;
	// This prevents death from Death Bolt.
	expval += m_death_safe ? 1 : 0;
	expval += m_power_safe ? 8 : 0;
	expval += (flags & (1 << fly)) != 0 ? 1 : 0;
	expval += (flags & (1 << swim)) != 0 ? 1 : 0;
	expval += (flags & (1 << ethereal)) != 0 ? 2 : 0;
	expval += (flags & (1 << 5)) != 0 ? 2 : 0;    // No idea what this does.
	expval += (flags & (1 << see_invisible)) != 0 ? 2 : 0;
	expval += (flags & (1 << start_invisible)) != 0 ? 8 : 0;
	expval += m_splits ? 2 : 0;
	expval += reach > 5 ? 2 : (reach > 3 ? 1 : 0);
	return expval;
}
