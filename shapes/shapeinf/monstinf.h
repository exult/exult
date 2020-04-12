/**
 ** Monstinf.h - Information (about NPC's, really) from 'monster.dat'.
 **
 ** Written: 8/13/01 - JSF
 **/

#ifndef INCL_MONSTINF
#define INCL_MONSTINF   1

/*
Copyright (C) 2000-2013 The Exult Team

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

#include <iosfwd>
#include <cassert>
#include <vector>

#include "baseinf.h"
#include "exult_constants.h"

class Shape_info;
class Shapes_vga_file;

/*
 *  An element from 'equip.dat', describing a monster's equipment:
 */
class Equip_element {
	unsigned short shapenum = 0;    // What to create, or 0 for nothing.
	unsigned char probability = 0;  // 0-100:  probabilit of creation.
	unsigned char quantity = 0;     // # to create.
public:
	friend class Monster_info;
	friend class Monster_actor;
	void set(int shnum, int prob, int quant) {
		shapenum = shnum;
		probability = prob;
		quantity = quant;
	}
	int get_shapenum() const {
		return shapenum;
	}
	int get_probability() const {
		return probability;
	}
	int get_quantity() const {
		return quantity;
	}
};

/*
 *  A record from 'equip.dat' consists of 10 elements.
 */
class Equip_record {
	Equip_element elements[10];
public:
	friend class Monster_info;
	friend class Monster_actor;
	// Set i'th element.
	void set(int i, int shnum, int prob, int quant) {
		elements[i].set(shnum, prob, quant);
	}
	Equip_element &get(int i) {
		assert(i >= 0 && i < 10);
		return elements[i];
	}
};

/*
 *  Monster info. from 'monsters.dat':
 */
class Monster_info : public Base_info {
	static std::vector<Equip_record> equip; // ->equipment info.
	static Monster_info default_info;   // For shapes not found.
	unsigned char strength;     // Attributes.
	unsigned char dexterity;
	unsigned char intelligence;
	unsigned char alignment;    // Default alignment.
	unsigned char combat;
	unsigned char armor;        // These are unarmed stats.
	unsigned char weapon;
	unsigned char reach;
	unsigned char flags;        // Defined below.
	// The following are bits corresponding
	//   to Weapon_data::Damage_type.
	unsigned char vulnerable, immune;
	unsigned char equip_offset; // Offset in 'equip.dat' (1 based;
	//   if 0, there's none.)
	short sfx;      // Sound used when attacking. We *need* better sfx packs.
	bool m_splits;          // For slimes.
	bool m_cant_die;
	bool m_cant_yell;       // Can't yell during combat.
	bool m_cant_bleed;

	bool m_poison_safe;     // Can't be poisoned.
	bool m_charm_safe;      // Can't be charmed.
	bool m_sleep_safe;      // Can't be put to sleep.
	bool m_paralysis_safe;      // Can't be paralyzed.
	bool m_curse_safe;      // Can't be cursed.
	bool m_power_safe;      // As above 4 items, plus return of flag 13.
	bool m_death_safe;      // Return of flag 14. Immune to death spells?
	bool m_int_b1;          // May give XP; but what does it do???

	char m_attackmode;      // Sets initial attack mode.
	char m_byte13;          // Unknown; Bits 3 through 7 of byte 13.

	bool m_can_teleport;
	bool m_can_summon;
	bool m_can_be_invisible;
public:
	friend class Shape_info;
	friend class Monster_actor;
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	enum { is_binary = 1, entry_size = 25 };
	static const Monster_info *get_default();
	static void reserve_equip(int cnt) {
		equip.reserve(cnt);
	}
	static void add_equip(Equip_record &eq) {
		equip.push_back(eq);
	}
	static int get_equip_cnt() {
		return equip.size();
	}
	static Equip_record &get_equip(int i) {
		assert(i >= 0 && static_cast<size_t>(i) < equip.size());
		return equip[i];
	}
	bool splits() const {
		return m_splits;
	}
	void set_splits(bool tf) {
		if (m_splits != tf) {
			set_modified(true);
			m_splits = tf;
		}
	}
	bool cant_die() const {
		return m_cant_die;
	}
	void set_cant_die(bool tf) {
		if (m_cant_die != tf) {
			set_modified(true);
			m_cant_die = tf;
		}
	}
	bool cant_yell() const {
		return m_cant_yell;
	}
	void set_cant_yell(bool tf) {
		if (m_cant_yell != tf) {
			set_modified(true);
			m_cant_yell = tf;
		}
	}
	bool cant_bleed() const {
		return m_cant_bleed;
	}
	void set_cant_bleed(bool tf) {
		if (m_cant_bleed != tf) {
			set_modified(true);
			m_cant_bleed = tf;
		}
	}
	bool poison_safe() const {
		return m_poison_safe;
	}
	void set_poison_safe(bool tf) {
		if (m_poison_safe != tf) {
			set_modified(true);
			m_poison_safe = tf;
		}
	}
	bool charm_safe() const {
		return m_charm_safe;
	}
	void set_charm_safe(bool tf) {
		if (m_charm_safe != tf) {
			set_modified(true);
			m_charm_safe = tf;
		}
	}
	bool sleep_safe() const {
		return m_sleep_safe;
	}
	void set_sleep_safe(bool tf) {
		if (m_sleep_safe != tf) {
			set_modified(true);
			m_sleep_safe = tf;
		}
	}
	bool paralysis_safe() const {
		return m_paralysis_safe;
	}
	void set_paralysis_safe(bool tf) {
		if (m_paralysis_safe != tf) {
			set_modified(true);
			m_paralysis_safe = tf;
		}
	}
	bool curse_safe() const {
		return m_curse_safe;
	}
	void set_curse_safe(bool tf) {
		if (m_curse_safe != tf) {
			set_modified(true);
			m_curse_safe = tf;
		}
	}
	bool power_safe() const {
		return m_power_safe;
	}
	void set_power_safe(bool tf) {
		if (m_power_safe != tf) {
			set_modified(true);
			m_power_safe = tf;
		}
	}
	bool death_safe() const {
		return m_death_safe;
	}
	void set_death_safe(bool tf) {
		if (m_death_safe != tf) {
			set_modified(true);
			m_death_safe = tf;
		}
	}
	bool get_int_b1() const {
		return m_int_b1;
	}
	void set_int_b1(bool tf) {
		if (m_int_b1 != tf) {
			set_modified(true);
			m_int_b1 = tf;
		}
	}
	char get_byte13() const {
		return m_byte13;
	}
	void set_byte13(char c) {
		if (m_byte13 != c) {
			set_modified(true);
			m_byte13 = c;
		}
	}
	char get_attackmode() const {
		return m_attackmode;
	}
	void set_attackmode(char c) {
		if (m_attackmode != c) {
			set_modified(true);
			m_attackmode = c;
		}
	}
	bool can_teleport() const {
		return m_can_teleport;
	}
	void set_can_teleport(bool tf) {
		if (m_can_teleport != tf) {
			set_modified(true);
			m_can_teleport = tf;
		}
	}
	bool can_summon() const {
		return m_can_summon;
	}
	void set_can_summon(bool tf) {
		if (m_can_summon != tf) {
			set_modified(true);
			m_can_summon = tf;
		}
	}
	bool can_be_invisible() const {
		return m_can_be_invisible;
	}
	void set_can_be_invisible(bool tf) {
		if (m_can_be_invisible != tf) {
			set_modified(true);
			m_can_be_invisible = tf;
		}
	}
	// Get bits indicating
	//   Weapon_data::damage_type:
	unsigned char get_vulnerable() const {
		return vulnerable;
	}
	void set_vulnerable(unsigned char v) {
		if (vulnerable != v) {
			set_modified(true);
			vulnerable = v;
		}
	}
	unsigned char get_immune() const {
		return immune;
	}
	void set_immune(unsigned char v) {
		if (immune != v) {
			set_modified(true);
			immune = v;
		}
	}
	enum Flags {
	    fly = 0,
	    swim = 1,
	    walk = 2,
	    ethereal = 3,       // Can walk through walls.
	    no_body = 4,        // Don't create body.
	    // 5:  gazer, hook only.
	    start_invisible = 6,
	    see_invisible = 7
	};
	unsigned char get_flags() const { // Get above set of flags.
		return flags;
	}
	void set_flags(unsigned char f) {
		if (flags != f) {
			set_modified(true);
			flags = f;
		}
	}
	bool has_no_body() const {  // No dead body?
		return (flags >> no_body) & 1;
	}
	int get_strength() const {
		return strength;
	}
	int get_dexterity() const {
		return dexterity;
	}
	int get_intelligence() const {
		return intelligence;
	}
	int get_alignment() const {
		return alignment;
	}
	void set_alignment(int a) {
		if (alignment != a) {
			set_modified(true);
			alignment = a;
		}
	}
	int get_combat() const {
		return combat;
	}
	int get_armor() const {
		return armor;
	}
	int get_weapon() const {
		return weapon;
	}
	int get_reach() const {
		return reach;
	}
	void set_stats(int str, int dex, int intel, int cmb, int armour,
	               int wpn, int rch);
	int get_equip_offset() const {
		return equip_offset;
	}
	void set_equip_offset(int o) {
		if (equip_offset != o) {
			set_modified(true);
			equip_offset = o;
		}
	}
	short get_hitsfx() const {
		return sfx;
	}
	void set_hitsfx(short s) {
		if (sfx != s) {
			set_modified(true);
			sfx = s;
		}
	}
	static int get_info_flag() {
		return 8;
	}
	int get_base_xp_value() const;
};



#endif
