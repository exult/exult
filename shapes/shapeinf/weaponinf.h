/**
 ** weaponinf.h - Information from 'weapons.dat'.
 **
 ** Written: 06/01/2008 - Marzo
 **/

#ifndef INCL_WEAPONINF_H
#define INCL_WEAPONINF_H    1

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
 *  Specific information about weapons from 'weapons.dat'.
 */
class Weapon_info : public Base_info {
public:
	// Actor frames to show when attacking with weapon:
	enum Actor_frames {
	    reach = 0,
	    raise = 1,
	    fast_swing = 2,
	    slow_swing = 3
	};
	// The weapon kind:
	enum Weapon_uses {
	    melee = 0,
	    poor_thrown = 1,
	    good_thrown = 2,
	    ranged = 3
	};
	// Type of ammo used by weapon:
	enum Weapon_ammo {
	    self_ammo = -1,
	    quality_ammo = -2,
	    quantity_ammo = -3
	};
private:
	static Weapon_info default_info;    // For shapes not found.
	char damage;            // Damage points (positive).
	unsigned char powers;       // Poison, sleep, charm. flags.
	unsigned char damage_type;  // See Damage_type above.
	unsigned char actor_frames; // Frames for NPC when using (from
	//   Actor_frames above).  Low 2 bits
	//   are for 'strike', next 2 are for
	//   shooting/throwing.
	short ammo;         // Shape # of ammo. consumed, or
	//   -1 = weapon is ammo.
	//   -2 = consummes weapon quality.
	//   -3 = consumes weapon quantity if ranged.
	short projectile;       // Projectile shape, or
	//  -1 = no projectile shown.
	//  -3 = use weapon shape as sprite shape.
	bool m_autohit;         // Weapon always hits.
	bool m_lucky;           // Easier to hit with.
	bool m_explodes;        // Explodes on impact.
	bool m_no_blocking;     // Can move through walls.
	bool m_delete_depleted; // Delete ammo == -2 weapon when quality reaches 0.
	bool m_returns;         // Boomerang, magic axe.
	bool m_need_target;     // If false, can be used to attack a tile.
	short missile_speed;    // # of steps taken by the missile each cycle.
	short rotation_speed;   // Added to frame # each cycle (misslies only).
	int usecode;            // Usecode function, or 0.
	unsigned char uses;     // 0 = hand-hand, 1 = poor throwable,
	//   2 = good throwable, 3 = missile-firing.
	unsigned char range;        // Distance weapon can be used.
	short sfx, hitsfx;      // Sound when using/hit, or -1.
public:
	friend class Shape_info;
	// Read in from file.
	bool read(std::istream &in, int version, Exult_Game game);
	// Write out.
	void write(std::ostream &out, int shapenum, Exult_Game game);
	enum { is_binary = 1, entry_size = 21 };
	static const Weapon_info *get_default();
	int get_damage() const {
		return damage;
	}
	int get_damage_type() const {
		return damage_type;
	}
	void set_damage(int dmg, int dmgtype) {
		if (damage != dmg || damage_type != dmgtype) {
			set_modified(true);
			damage = dmg;
			damage_type = dmgtype;
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
	unsigned char get_actor_frames(bool projectile) const {
		return !projectile ? (actor_frames & 3) : (actor_frames >> 2) & 3;
	}
	void set_actor_frames(unsigned char f) {
		if (actor_frames != f) {
			set_modified(true);
			actor_frames = f;
		}
	}
	int get_ammo_consumed() const {
		return ammo;
	}
	int get_ammo() const {          // Raw value, for map-editor.
		return ammo;
	}
	void set_ammo(int a) {          // Raw value, for map-editor.
		if (ammo != a) {
			set_modified(true);
			ammo = a;
		}
	}
	bool uses_charges() const {
		return ammo == -2;
	}
	bool uses_ammo_on_ranged() const {
		return ammo != -1;
	}
	bool is_thrown() const
	// Figured this out from printing out values:
	{
		return ammo == -3 && uses != 0 && uses != 3;
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
	bool explodes() const {
		return m_explodes;
	}
	void set_explodes(bool tf) {
		if (m_explodes != tf) {
			set_modified(true);
			m_explodes = tf;
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
	bool delete_depleted() const {
		return m_delete_depleted;
	}
	void set_delete_depleted(bool tf) {
		if (m_delete_depleted != tf) {
			set_modified(true);
			m_delete_depleted = tf;
		}
	}
	bool needs_target() const {
		return m_need_target;
	}
	void set_needs_target(bool tf) {
		if (m_need_target != tf) {
			set_modified(true);
			m_need_target = tf;
		}
	}
	unsigned char get_uses() const {
		return uses;
	}
	void set_uses(unsigned char u) {
		if (uses != u) {
			set_modified(true);
			uses = u;
		}
	}
	int get_range() const {         // Raw # (for map-editor).
		return range;
	}
	void set_range(int r) {
		if (range != r) {
			set_modified(true);
			range = r;
		}
	}
	int get_striking_range() const {
		return uses < 3 ? range : 0;
	}
	int get_projectile_range() const {  // +++Guess for thrown weapons.
		return uses == 3 ? range : -1;
	}
	int get_projectile() const {
		return projectile;
	}
	void set_projectile(int p) {
		if (projectile != p) {
			set_modified(true);
			projectile = p;
		}
	}
	int get_missile_speed() const {
		return missile_speed;
	}
	void set_missile_speed(int s) {
		if (missile_speed != s) {
			set_modified(true);
			missile_speed = s;
		}
	}
	int get_rotation_speed() const {
		return rotation_speed;
	}
	void set_rotation_speed(int s) {
		if (rotation_speed != s) {
			set_modified(true);
			rotation_speed = s;
		}
	}
	int get_usecode() const {
		return usecode;
	}
	void set_usecode(int u) {
		if (usecode != u) {
			set_modified(true);
			usecode = u;
		}
	}
	int get_sfx() const {       // Return sound-effects #, or -1.
		return sfx;
	}
	int get_hitsfx() const {
		return hitsfx;
	}
	void set_sfxs(int s, int hits) {
		if (sfx != s || hitsfx != hits) {
			set_modified(true);
			sfx = s;
			hitsfx = hits;
		}
	}
	static int get_info_flag() {
		return 1;
	}
	int get_base_strength() const;
	int get_base_xp_value() const;
};

#endif
