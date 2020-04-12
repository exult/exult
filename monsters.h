/*
 *  monsters.h - Monsters.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef INCL_MONSTERS
#define INCL_MONSTERS   1

#include "actors.h"
#include "ignore_unused_variable_warning.h"

class Monster_actor;

/*
 *  Monsters get their own class because they have a bigger footprint
 *  than humans.
 */
class Monster_actor : public Npc_actor {
	static Game_object_shared in_world; // All monsters in the world.
	// Links for 'in_world' list.
    Game_object_shared next_monster;
	Monster_actor *prev_monster;
	Animator *animator;     // For wounded men.
	void link_in();         // Add to in_world list.
	void link_out();        // Remove from list.
	void equip(const Monster_info *inf, bool temporary);
public:
	Monster_actor(const std::string &nm, int shapenum, int num = -1,
	              int uc = -1);
	~Monster_actor() override;
	// Create an instance.
	static Game_object_shared create(int shnum);
	static Game_object_shared create(int shnum, Tile_coord pos,
	                             int sched = -1, int align = static_cast<int>(Actor::neutral),
	                             bool temporary = true, bool equipment = true);
	// Methods to retrieve them all:
	static Monster_actor *get_first_in_world() {
		return in_world ? static_cast<Monster_actor *>(in_world.get())
			   			: nullptr;
	}
	Monster_actor *get_next_in_world() {
		return next_monster ? static_cast<Monster_actor *>(next_monster.get())
			   				: nullptr;
	}
	static void delete_all();   // Delete all monsters.
	static void give_up() {     // For file errors only!
		in_world = nullptr;
	}
	bool move_aside(Actor *for_actor, int dir) override {
		ignore_unused_variable_warning(for_actor, dir);
		return false;    // Monsters don't move aside.
	}
	// Render.
	void paint() override;
	// Step onto an (adjacent) tile.
	bool step(Tile_coord t, int frame, bool force = false) override;
	// Remove/delete this object.
	void remove_this(Game_object_shared *keep = nullptr) override;
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;
	// Add an object.
	bool add(Game_object *obj, bool dont_check = false,
	                 bool combine = false, bool noset = false) override;
	int get_armor_points() override; // Get total armor value.
	// Get total weapon value.
	const Weapon_info *get_weapon(int &points, int &shape,
	                                Game_object  *&obj) override;
	int is_monster() override {
		return 1;
	}
	void die(Game_object *attacker) override;        // We're dead.
	void write(ODataSource *nfile);// Write out (to 'monsnpc.dat').
};

#endif
