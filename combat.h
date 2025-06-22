/*
 *  combat.h - Combat scheduling.
 *
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef COMBAT_H
#define COMBAT_H 1

#include "schedule.h"

#include <list>

class Actor;
class Game_object;
class Spellbook_object;

/*
 *	Combat Music:
 */
enum Combat_song {
	CSBattle_Over,
	CSAttacked1,
	CSAttacked2,
	CSVictory,
	CSRun_Away,
	CSDanger,
	CSHidden_Danger,
	CSAvatar_died
};

/*
 *  Combat schedule:
 */
class Combat_schedule : public Schedule {
public:
	enum Phase {            // We'll be a finite-state-machine.
		initial     = 0,    // Just constructed.
		approach    = 1,    // Approaching a foe to attack.
		retreat     = 2,    // Avoiding a foe.
		flee        = 3,    // Run away!
		strike      = 4,    // In the process of striking.
		fire        = 5,    // In process of firing range weapon.
		parry       = 6,    // In the process of parrying a blow.
		stunned     = 7,    // Just been hit.
		wait_return = 8     // Wait for boomerang.
	};

protected:
	static unsigned long        battle_time;        // Time when battle started.
	static unsigned long        battle_end_time;    // And when it ended.
	Phase                       state;
	Schedule_types              prev_schedule;      // Before going into combat.
	std::list<Game_object_weak> opponents;          // Possible opponents.
	Game_object*                practice_target;    // Only for duel schedule.
	Game_object*                weapon;
	int               weapon_shape;    // Weapon's shape in shapes.vga.
	Spellbook_object* spellbook;       // If readied.
	// Ranges in tiles.
	//   0 means not applicable.
	bool          no_blocking;       // Weapon/ammo goes through walls.
	unsigned char yelled;            // Yell when first opponent targeted.
	bool          started_battle;    // 1st opponent targeted.
	unsigned char fleed;             // Incremented when fleeing.
	bool          can_yell;
	int           failures;         // # failures to find opponent.
	unsigned int  teleport_time;    // Next time we can teleport.
	unsigned int  summon_time;
	unsigned int  invisible_time;
	unsigned int  dex_points;       // Need these to attack.
	unsigned int strike_blocked;    // # failures to get a clear path to strike.
	int          alignment;         // So we can tell if it changed.

	void         start_battle();    // Play music at start of battle.
	bool         teleport();        // For monsters that can.
	bool         summon();
	bool         be_invisible();
	virtual void find_opponents();
	// Find attacker of protected member.
	std::list<Game_object_weak>::iterator find_weakest_opponent();
	std::list<Game_object_weak>::iterator find_strongest_opponent();
	std::list<Game_object_weak>::iterator find_nearest_opponent();
	std::list<Game_object_weak>::iterator find_random_opponent();
	std::list<Game_object_weak>::iterator find_attacked_opponent();
	std::list<Game_object_weak>::iterator find_protected_attacker();
	Game_object* find_foe(int mode);    // Find a new opponent.
	Game_object* find_foe();
	// Back off when being attacked.
	static void back_off(Actor* npc, Game_object* attacker);
	void        approach_foe(bool for_projectile = false);    // Approach foe.
	void        wander_for_attack();
	void        start_strike();
	void        run_away();
	Spellbook_object* readied_spellbook();

public:
	Combat_schedule(Actor* n, Schedule_types prev_sched);
	static void monster_died();    // Checks for victory.
	static void stop_attacking_npc(Game_object* npc);
	static void stop_attacking_invisible(Game_object* npc);
	void        now_what() override;    // Npc calls this when it's done
	void im_dormant() override;         // Npc calls this when it goes dormant.
	static void start_music_combat(Combat_song song, bool continuous);
	static void danger_music();
	void ending(int newtype) override;    // Switching to another schedule.
	void set_weapon(bool removed = false) override;    // Set weapon info.
	void set_hand_to_hand();

	bool has_started_battle() const {
		return started_battle;
	}

	void set_state(Phase s) {
		state = s;
	}

	static bool attack_target(
			Game_object* attacker, Game_object* target, const Tile_coord& tile,
			int weapon, bool combat = false);
	static bool is_enemy(int align, int other);
};

/*
 *  Dueling is like combat, but nobody gets hurt.
 */

class Duel_schedule : public Combat_schedule {
	Tile_coord start;      // Starting position.
	int        attacks;    // Count strikes.
	void       find_opponents() override;

public:
	Duel_schedule(Actor* n);
	void now_what() override;
};

bool In_ammo_family(int shnum, int family);    // Yow, a global function.

#endif
