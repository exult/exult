/*
 *  monsters.cc - Monsters.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "monsters.h"

#include "Audio.h"
#include "actors.h"
#include "animate.h"
#include "chunks.h"
#include "effects.h"
#include "game.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "monstinf.h"
#include "objiter.h"
#include "objs/contain.h"
#include "schedule.h"
#include "ucmachine.h"
#include "weaponinf.h"

using std::rand;
using std::vector;

/*
 *  Slimes move differently.
 */
class Slime_actor : public Monster_actor {
	void update_frames(Tile_coord src, Tile_coord dest);

public:
	Slime_actor(const std::string& nm, int shapenum, int num = -1, int uc = -1)
			: Monster_actor(nm, shapenum, num, uc) {}

	// Step onto an (adjacent) tile.
	bool step(Tile_coord t, int frame, bool force = false) override;
	// Remove/delete this object.
	void remove_this(Game_object_shared* keep = nullptr) override;
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;

	void lay_down(bool die) override {
		ignore_unused_variable_warning(die);
		Game_object_shared keep;
		remove_this(&keep);    // Remove (but don't delete this).
		set_invalid();
	}

	bool is_slime() const override {
		return true;
	}
};

Game_object_shared Monster_actor::in_world;

/*
 *  Add to global list (if not already there).
 */

void Monster_actor::link_in() {
	if (prev_monster || in_world.get() == this) {
		return;    // Already in list.
	}
	if (in_world) {    // Add to head.
		(static_cast<Monster_actor*>(in_world.get()))->prev_monster = this;
	}
	next_monster = in_world;
	in_world     = shared_from_this();
}

/*
 *  Remove from global list (if in list).
 */

void Monster_actor::link_out() {
	if (next_monster) {
		(static_cast<Monster_actor*>(next_monster.get()))->prev_monster
				= prev_monster;
	}
	if (prev_monster) {
		prev_monster->next_monster = next_monster;
	} else    // We're at start of list.
		if (in_world.get() == this) {
			in_world = next_monster;
		}
	next_monster = nullptr;
	prev_monster = nullptr;
}

/*
 *  Create monster.
 */

Monster_actor::Monster_actor(
		const std::string& nm, int shapenum,
		int num,    // Generally -1.
		int uc)
		: Npc_actor(nm, shapenum, num, uc), next_monster(nullptr),
		  prev_monster(nullptr), animator(nullptr) {
	// Check for animated shape.
	const Shape_info& info = get_info();
	if (info.is_animated() || info.has_sfx()) {
		animator = Animator::create(this);
	}
}

/*
 *  Delete.
 */

Monster_actor::~Monster_actor() {
	delete animator;
}

/*
 *  Equip.
 */

void Monster_actor::equip(const Monster_info* inf, bool temporary) {
	// Get equipment.
	const int equip_offset = inf->equip_offset;
	if (equip_offset == 0
		|| equip_offset - 1 >= Monster_info::get_equip_cnt()) {
		return;
	}
	vector<Equip_record>& equip = Monster_info::equip;
	const Equip_record&   rec   = equip[equip_offset - 1];
	for (const Equip_element& elem : rec.elements) {
		// Give equipment.
		if (elem.shapenum <= 0 || (1 + (rand() % 100)) > elem.probability) {
			continue;    // You lose.
		}
		int frnum = (elem.shapenum == 377) ? get_info().get_monster_food() : 0;
		if (frnum < 0) {
			// Food. Want top randomize frame for each object.
			const int num_frames = ShapeID(elem.shapenum, 0).get_num_frames();
			for (int i = 0; i < elem.quantity; i++) {
				frnum = rand() % num_frames;
				create_quantity(1, elem.shapenum, c_any_qual, frnum, temporary);
			}
			continue;
		}
		const Shape_info&  einfo = ShapeID::get_info(elem.shapenum);
		const Weapon_info* winfo = einfo.get_weapon_info();
		if (einfo.has_quality() && winfo != nullptr && winfo->uses_charges()) {
			create_quantity(1, elem.shapenum, elem.quantity, frnum, temporary);
		} else if (einfo.has_quantity()) {
			// Randomize quantity for shapes that have a quantity value. This
			// matches behavior of the original games, where some items (like
			// gold coins) can be any amount in this range.
			int amount = 1 + (rand() % elem.quantity);
			create_quantity(
					amount, elem.shapenum, c_any_qual, frnum, temporary);
		} else {
			create_quantity(
					elem.quantity, elem.shapenum, c_any_qual, frnum, temporary);
		}
		const int ammo = winfo != nullptr ? winfo->get_ammo_consumed() : -1;
		if (ammo >= 0) {
			// Weapon requires ammo.
			create_quantity(
					(1 + (rand() % 10)) + (1 + (rand() % 10)), ammo, c_any_qual,
					0, temporary);
		}
	}
	if (inf->cant_yell()) {
		// TODO: This seems to match originals, but it is a bit of a kludge.
		return;
	}
	// This is a presumably sentient, so we will put all the equipment that is
	// not equipped inside a container.
	std::vector<Game_object*> contents;
	size_t                    num_items = 0;
	{
		Game_object*    obj;
		Object_iterator next(this->get_objects());
		while ((obj = next.get_next()) != nullptr) {
			// The originals never created a container only for food.
			if (find_readied(obj) == -1 && !obj->get_info().is_spell()) {
				contents.push_back(obj);
				if (obj->get_shapenum() != 377) {
					num_items++;
				}
			}
		}
	}
	if (num_items > 0) {
		struct container_info {
			int shape;
			int qual;
		};

		const std::array<container_info, 5> containers = {
				container_info{522,   0}, // Locked chest, pickable
				container_info{522, 255}, // Locked chest, pickable, trapped
				container_info{800,   0}, // Unlocked chest
				container_info{801,   0}, // Backpack
				container_info{802,   0}, // Bag
		};
		const auto [shape, qual]     = containers[rand() % containers.size()];
		Game_object_shared container = gmap->create_ireg_object(
				ShapeID::get_info(shape), shape, 0, 0, 0, 0);

		// Set temporary
		if (temporary) {
			container->set_flag(Obj_flags::is_temporary);
		}
		for (auto* obj : contents) {
			Game_object_shared keep;
			obj->remove_this(&keep);
			if (!container->add(obj, true)) {
				add(obj, true);
			}
		}

		if (!add(container.get(), true)) {
			container.reset();
			return;
		}
	}
}

/*
 *  Create an instance of a monster.
 */

Game_object_shared Monster_actor::create(int shnum    // Shape to use.
) {
	// Get usecode for shape.
	int ucnum = ucmachine->get_shape_fun(shnum);
	if (shnum == 529) {    // Slime?
		return std::make_shared<Slime_actor>("", shnum, -1, ucnum);
	} else {
		return std::make_shared<Monster_actor>("", shnum, -1, ucnum);
	}
}

static inline int Randomize_initial_stat(int val) {
	if (val > 7) {
		return val + rand() % 5 + rand() % 5 - 4;
	} else if (val > 0) {
		return rand() % val + rand() % val + 1;
	} else {
		return 1;
	}
}

/*
 *  Create an instance of a monster and initialize from monstinf.dat.
 */

Game_object_shared Monster_actor::create(
		int        shnum,    // Shape to use.
		Tile_coord pos,      // Where to place it.  If pos.tx < 0,
		//   it's not placed in the world.
		int  sched,    // Schedule type.
		int  align,    // Alignment.
		bool temporary, bool equipment) {
	// Get 'monsters.dat' info.
	const Monster_info* inf = ShapeID::get_info(shnum).get_monster_info();
	if (!inf) {
		inf = Monster_info::get_default();
	}
	Game_object_shared new_monster = create(shnum);
	auto*              monster = static_cast<Monster_actor*>(new_monster.get());
	monster->set_alignment(
			align == static_cast<int>(Actor::neutral) ? inf->alignment : align);
	// Movement flags
	if ((inf->flags >> Monster_info::fly) & 1) {
		monster->set_type_flag(Actor::tf_fly);
	}

	if ((inf->flags >> Monster_info::swim) & 1) {
		monster->set_type_flag(Actor::tf_swim);
	}

	if ((inf->flags >> Monster_info::walk) & 1) {
		monster->set_type_flag(Actor::tf_walk);
	}

	if ((inf->flags >> Monster_info::ethereal) & 1) {
		monster->set_type_flag(Actor::tf_ethereal);
	}

	if ((inf->flags >> Monster_info::start_invisible) & 1) {
		monster->set_flag(Obj_flags::invisible);
	}

	const int str = Randomize_initial_stat(inf->strength);
	monster->set_property(Actor::strength, str);
	// Max. health = strength.
	monster->set_property(Actor::health, str);
	monster->set_property(
			Actor::dexterity, Randomize_initial_stat(inf->dexterity));
	monster->set_property(
			Actor::intelligence, Randomize_initial_stat(inf->intelligence));
	monster->set_property(Actor::combat, Randomize_initial_stat(inf->combat));

	static const char monster_mode_odds[5][4] = {
			{20,  45,  70, 100}, // These are slightly off, but
			{50, 100,   0,   0}, // are good enough that no one
			{35,  70, 100,   0}, // will notice the difference
			{35,  55,  70, 100}, // without serious statistics.
			{50, 100,   0,   0}
    };
	static const Actor::Attack_mode monster_modes[5][4] = {
			{nearest,  random,    flee,   nearest}, // noncombatants
			{weakest, nearest, nearest,   nearest}, // opportunists
			{nearest,  random, nearest,   nearest}, // unpredictable
			{  flank,  defend, weakest, strongest}, // tacticians
			{berserk, nearest, nearest,   nearest}
    };    // berserkers

	const int prob = rand() % 100;
	int       i;
	for (i = 0; i < 3; i++) {
		if (prob < monster_mode_odds[static_cast<int>(inf->m_attackmode)][i]) {
			break;
		}
	}
	monster->set_attack_mode(
			monster_modes[static_cast<int>(inf->m_attackmode)][i]);

	// Set temporary
	if (temporary) {
		monster->set_flag(Obj_flags::is_temporary);
	}
	monster->set_invalid();    // Place in world.
	if (pos.tx >= 0) {
		monster->move(pos.tx, pos.ty, pos.tz);
	}
	if (equipment) {
		monster->equip(inf, temporary);    // Get equipment.
		if (sched == Schedule::combat) {
			monster->ready_best_weapon();
		}
	}
	if (sched < 0) {    // Set sched. AFTER equipping.
		sched = static_cast<int>(Schedule::loiter);
	}
	monster->set_schedule_type(sched);
	return new_monster;
}

/*
 *  Delete all monsters.  (Should only be called after deleting chunks.)
 */

void Monster_actor::delete_all() {
	in_world = nullptr;
}

/*
 *  Render.
 */

void Monster_actor::paint() {
	// Animate first
	if (animator) {    // Be sure animation is on.
		animator->want_animation();
	}
	Npc_actor::paint();    // Draw on screen.
}

/*
 *  Step onto an adjacent tile.
 *
 *  Output: false if blocked.
 *      Dormant is set if off screen.
 */

bool Monster_actor::step(
		Tile_coord t,        // Tile to step onto.
		int        frame,    // New frame #.
		bool       force) {
	// If move not allowed do I remove or change destination?
	// I'll do nothing for now
	if (!gwin->emulate_is_move_allowed(t.tx, t.ty)) {
		return false;
	}
	if (get_flag(Obj_flags::paralyzed) || get_map() != gmap) {
		return false;
	}
	// Get old chunk.
	Map_chunk* olist = get_chunk();
	// Get chunk.
	const int cx = t.tx / c_tiles_per_chunk;
	const int cy = t.ty / c_tiles_per_chunk;
	// Get ->new chunk.
	Map_chunk* nlist = gmap->get_chunk(cx, cy);
	nlist->setup_cache();    // Setup cache if necessary.
	// Blocked?
	if (is_blocked(t, nullptr, force ? MOVE_ALL : 0)) {
		if (schedule) {    // Tell scheduler.
			schedule->set_blocked(t);
		}
		stop();
		if (!gwin->add_dirty(this)) {
			dormant = true;    // Off-screen.
		}
		return false;    // Done.
	}
	// Check for scrolling.
	gwin->scroll_if_needed(this, t);
	add_dirty();    // Set to repaint old area.
	// Move it.
	// Get rel. tile coords.
	const int tx = t.tx % c_tiles_per_chunk;
	const int ty = t.ty % c_tiles_per_chunk;
	movef(olist, nlist, tx, ty, frame, t.tz);
	if (!add_dirty(true) &&
		// And > a screenful away?
		distance(gwin->get_camera_actor()) > 1 + c_screen_tile_size) {
		// No longer on screen.
		stop();
		dormant = true;
		return false;
	}
	quake_on_walk();
	return true;    // Add back to queue for next time.
}

/*
 *  Remove an object from its container, or from the world.
 *  The object is deleted.
 */

void Monster_actor::remove_this(
		Game_object_shared* keep    // Non-null to not delete.
) {
	if (!keep) {       // +++++Experiment
		link_out();    // Remove from list.
	}
	Npc_actor::remove_this(keep);
}

/*
 *  Move (teleport) to a new spot.
 */

void Monster_actor::move(int newtx, int newty, int newlift, int newmap) {
	Npc_actor::move(newtx, newty, newlift, newmap);
	link_in();    // Insure it's in global list.
}

/*
 *  Add an object.
 *
 *  Output: 1, meaning object is completely contained in this,
 *      0 if not enough space.
 */

bool Monster_actor::add(
		Game_object* obj,
		bool         dont_check,    // 1 to skip volume check.
		bool         combine,       // True to try to combine obj.  MAY
		//   cause obj to be deleted.
		bool noset    // True to prevent actors from setting sched. weapon.
) {
	ignore_unused_variable_warning(dont_check);
	// Try to add to 'readied' spot.
	if (Npc_actor::add(obj, true, combine, noset)) {
		return true;    // Successful.
	}
	// Just add anything.
	return Container_game_object::add(obj, true, combine);
}

/*
 *  Get total value of armor being worn.
 */

int Monster_actor::get_armor_points() const {
	const Monster_info* inf = get_info().get_monster_info();
	// Kind of guessing here.
	return Actor::get_armor_points() + (inf ? inf->armor : 0);
}

/*
 *  Get weapon value.
 */

const Weapon_info* Monster_actor::get_weapon(
		int& points, int& shape,
		Game_object*& obj    // ->weapon itself returned, or 0.
) const {
	// Kind of guessing here.
	const Weapon_info* winf = Actor::get_weapon(points, shape, obj);
	if (!winf) {    // No readied weapon?
		// Look up monster itself.
		shape = 0;
		winf  = get_info().get_weapon_info();
		if (winf) {
			shape  = get_shapenum();
			points = winf->get_damage();
		} else {    // Builtin (claws?):
			const Monster_info* inf = get_info().get_monster_info();
			if (inf) {
				points = inf->weapon;
			}
		}
	}
	return winf;
}

/*
 *  We're dead.  We're removed from the world, but not deleted.
 */

void Monster_actor::die(Game_object* attacker) {
	Actor::die(attacker);
	// Got to delete this somewhere, but
	//   doing it here crashes.
}

/*
 *  Get the tiles where slimes adjacent to one in a given position should
 *  be found.
 */

static void Get_slime_neighbors(
		const Tile_coord& pos,         // Position to look around.
		Tile_coord*       neighbors    // N,E,S,W tiles returned.
) {
	// Offsets to neighbors 2 tiles away.
	static const int offsets[8] = {0, -2, 2, 0, 0, 2, -2, 0};
	for (int dir = 0; dir < 4; dir++) {
		neighbors[dir]
				= pos + Tile_coord(offsets[2 * dir], offsets[2 * dir + 1], 0);
	}
}

/*
 *  Find whether a slime is a neighbor of a given spot.
 *
 *  Output: Direction (0-3 for N,E,S,W), or -1 if not found.
 */

int Find_neighbor(
		Game_object* slime,
		Tile_coord*  neighbors    // Neighboring spots to check.
) {
	const Tile_coord pos = slime->get_tile();
	for (int dir = 0; dir < 4; dir++) {
		if (pos == neighbors[dir]) {
			return dir;
		}
	}
	return -1;    // Not found.
}

/*
 *  Update the frame of a slime and its neighbors after it has been moved.
 *  The assumption is that slimes are 2x2 tiles, and that framenum/2 is
 *  based on whether there are adjoining slimes to the N, W, S, or E, with
 *  bit 0 being random.
 */

void Slime_actor::update_frames(
		Tile_coord src,    // May be invalid (tx = -1).
		Tile_coord dest    // May be invalid.  If src & dest are
						   //   both valid, we assume they're at
						   //   most 2 tiles apart.
) {
	Tile_coord         neighbors[4];    // Gets surrounding spots for slimes.
	int                dir;             // Get direction of neighbor.
	Game_object_vector nearby;          // Get nearby slimes.
	if (src.tx != -1) {
		if (dest.tx != -1) {    // Assume within 2 tiles.
			Game_object::find_nearby(nearby, dest, get_shapenum(), 4, 8);
		} else {
			Game_object::find_nearby(nearby, src, get_shapenum(), 2, 8);
		}
	} else {    // Assume they're both not invalid.
		Game_object::find_nearby(nearby, dest, get_shapenum(), 2, 8);
	}
	if (src.tx != -1) {    // Update neighbors we moved from.
		Get_slime_neighbors(src, neighbors);
		for (auto* slime : nearby) {
			if (slime != this && (dir = Find_neighbor(slime, neighbors)) >= 0) {
				const int ndir = (dir + 2) % 4;
				// Turn off bit (1<<ndir)*2, and set
				//   bit 0 randomly.
				slime->change_frame(
						(slime->get_framenum() & ~(((1 << ndir) * 2) | 1))
						| (rand() % 2));
			}
		}
	}
	if (dest.tx != -1) {    // Update neighbors we moved to.
		int frnum = 0;      // Figure our new frame too.
		Get_slime_neighbors(dest, neighbors);
		for (auto* slime : nearby) {
			if (slime != this && (dir = Find_neighbor(slime, neighbors)) >= 0) {
				// In a neighboring spot?
				frnum |= (1 << dir) * 2;
				const int ndir = (dir + 2) % 4;
				// Turn on bit (1<<ndir)*2, and set
				//   bit 0 randomly.
				slime->change_frame(
						(slime->get_framenum() & ~1) | ((1 << ndir) * 2)
						| (rand() % 2));
			}
		}
		change_frame(frnum | (rand() % 2));
	}
}

/*
 *  Step onto an adjacent tile.
 *
 *  Output: false if blocked.
 *      Dormant is set if off screen.
 */

bool Slime_actor::step(
		Tile_coord t,        // Tile to step onto.
		int        frame,    // New frame # (ignored).
		bool       force) {
	ignore_unused_variable_warning(frame);
	// Save old pos.
	const Tile_coord oldpos = get_tile();
	const bool       ret    = Monster_actor::step(t, -1, force);
	// Update surrounding frames (& this).
	const Tile_coord newpos = get_tile();
	update_frames(oldpos, newpos);
	const int first_slime_frame = 4;
	const int last_slime_frame  = 7;
	if (newpos != oldpos && rand() % 6 == 0) {
		bleed(first_slime_frame, last_slime_frame, oldpos);
	}
	return ret;
}

/*
 *  Remove an object from its container, or from the world.
 *  The object is deleted.
 */

void Slime_actor::remove_this(
		Game_object_shared* keep    // Non-null to not delete.
) {
	const Tile_coord   pos = get_tile();
	Game_object_shared keep_this;
	Monster_actor::remove_this(&keep_this);
	// Update surrounding slimes.
	update_frames(pos, Tile_coord(-1, -1, -1));
	if (keep) {
		*keep = std::move(keep_this);
	}
}

/*
 *  Move (teleport) to a new spot.  I assume slimes only use this when
 *  being added to the world.
 */

void Slime_actor::move(int newtx, int newty, int newlift, int newmap) {
	// Save old pos.
	Tile_coord pos;
	if (is_pos_invalid()) {    // Invalid?
		pos = Tile_coord(-1, -1, -1);
	} else {
		pos = get_tile();
	}
	Monster_actor::move(newtx, newty, newlift, newmap);
	// Update surrounding frames (& this).
	update_frames(pos, get_tile());
}
