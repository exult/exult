/*
 *  Schedule.cc - Schedules for characters.
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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Audio.h"
#include "Zombie.h"
#include "actions.h"
#include "actors.h"
#include "combat.h"
#include "dir.h"
#include "frameseq.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "monstinf.h"
#include "paths.h"
#include "schedule.h"
#include "span.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "ucscriptop.h"
#include "ucsymtbl.h"
#include "usefuns.h"
#include "useval.h"

#include <array>
#include <string>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::rand;
using std::vector;

vector<std::string> Schedule_change::script_names;
int                 Patrol_schedule::num_path_eggs    = -1;
bool                Sleep_schedule::sleep_interrupted = false;

/*
 *  Create.
 */
Schedule::Schedule(Actor* n)
		: npc(n), blocked(-1, -1, -1), start_pos(n->get_tile()),
		  street_maintenance_failures(0), street_maintenance_time(0) {
	prev_type = npc ? npc->get_schedule_type() : -1;
}

/*
 *  Set up an action to get an actor to a location (via pathfinding), and
 *  then execute another action when he gets there.
 */

void Schedule::set_action_sequence(
		Actor*            actor,         // Whom to activate.
		const Tile_coord& dest,          // Where to walk to.
		Actor_action*     when_there,    // What to do when he gets there.
		bool from_off_screen,            // Have actor walk from off-screen.
		int  delay                       // Msecs. to delay.
) {
	const bool persistant
			= actor->get_npc_num() == 0 || actor->get_party_id() >= 0;
	actor->set_action(Actor_action::create_action_sequence(
			actor, dest, when_there, from_off_screen, persistant));
	actor->start(250, delay);    // Get into time queue.
}

/*
 *  Try to procure an item. First, scans inventory. Then scans for a nearby
 *  match. Finally, it creates the item.
 *  Expects you to manually add the hammer as a client.
 */

Game_object* Schedule::set_procure_item_action(
		Actor* actor, Game_object* obj, int dist, int shnum, int frnum) {
	Game_window* gwin  = Game_window::get_instance();
	const int    delay = gwin->get_std_delay();
	if (obj) {
		// On inventory? Use it.
		if (obj->get_outermost() == actor) {
			return obj;
		}

		// If on map, pick it up.
		if (!obj->is_pos_invalid()
			&& set_pickup_item_action(actor, obj, delay)) {
			return obj;
		}

		// Not on inventory and not on map; someone took it. So fall through
		// and procure a new one.
	}
	// First, look in inventory.
	Game_object_vector vec;
	actor->get_objects(vec, shnum, c_any_qual, frnum);
	if (!vec.empty()) {
		return vec[0];    // If there, just use it.
	}

	// Now find one around and get the nearest one.
	actor->find_nearby(vec, shnum, dist, 0, c_any_qual, frnum);
	Game_object* found = find_nearest(actor, vec);

	// If there, go pick it up.
	if (found && !set_pickup_item_action(actor, found, delay)) {
		found = nullptr;
	}

	if (!found) {
		// Not there, so create it.
		const Game_object_shared newobj
				= std::make_shared<Ireg_game_object>(shnum, frnum, 0, 0);
		found = newobj.get();
		actor->add(found, true);
	}
	return found;
}

bool Schedule::set_pickup_item_action(
		Actor* actor, Game_object* obj, int delay) {
	Approach_object_pathfinder_client cost(actor, obj, 1);
	Actor_action* pact = Path_walking_actor_action::create_path(
			actor->get_tile(), obj->get_tile(), cost);
	if (pact) {
		actor->set_action(new Sequence_actor_action(
				pact, new Pickup_actor_action(obj, delay)));
		return true;
	} else {
		return false;
	}
}

Game_object* Schedule::find_nearest(Actor* actor, Game_object_vector& nearby) {
	Game_object* nearest   = nullptr;
	int          best_dist = 1000;    // Find closest.
	for (auto* obj : nearby) {
		int dist;
		if ((dist = obj->distance(actor)) < best_dist) {
			// Found it.
			nearest   = obj;
			best_dist = dist;
		}
	}
	return nearest;
}

/*
 *  Look for nearby enemies.
 *
 *  Output: true if any are found, which means npc's schedule has changed!
 */

bool Schedule::seek_foes() {
	Actor_vector vec;    // Look within 20 tiles, but check LOF.
	npc->find_nearby_actors(vec, c_any_shapenum, 20, 0x28);
	const int  npc_align     = npc->get_effective_alignment();
	Actor*     foe           = nullptr;
	const bool see_invisible = npc->can_see_invisible();
	for (auto* actor : vec) {
		if (actor->is_dead() || actor->get_flag(Obj_flags::asleep)
			|| (!see_invisible && actor->get_flag(Obj_flags::invisible))
			|| !Fast_pathfinder_client::is_straight_path(npc, actor)) {
			continue;    // Dead, asleep or invisible and can't see invisible.
		}
		if (Combat_schedule::is_enemy(
					npc_align, actor->get_effective_alignment())) {
			foe = actor;
			break;
		}
	}
	if (foe) {
		Actor* safenpc = npc;
		// May delete us!
		safenpc->set_schedule_type(Schedule::combat, nullptr);
		safenpc->set_target(foe);
		return true;
	}
	return false;
}

/*
 *  Look for lamps to light/unlight and shutters to open/close.
 *
 *  Output: 1 if successful, which means npc's schedule has changed!
 */

int Schedule::try_street_maintenance() {
	// What to look for: shutters, light sources,
	constexpr static const std::array night{322, 372, 336, 997, 889};
	constexpr static const std::array sinight{290, 291, 336, 997, 889};
	constexpr static const std::array day{290, 291, 338, 997, 526};

	const long curtime = Game::get_ticks();
	if (curtime < street_maintenance_time) {
		return 0;    // Not time yet.
	}
	if (npc->get_npc_num() < 0 || !npc->is_sentient()) {
		return 0;    // Only want normal NPC's.
	}
	// At least 30secs. before next one.
	street_maintenance_time
			= curtime + 30000 + street_maintenance_failures * 5000;
	const int            hour = gclock->get_hour();
	const bool           bg   = GAME_BG;
	tcb::span<const int> shapes;
	if (hour >= 9 && hour < 18) {
		shapes = day;
	} else if (hour >= 18 || hour < 6) {
		shapes = bg ? night : sinight;
	} else {
		return 0;    // Dusk or dawn.
	}
	const Tile_coord npcpos = npc->get_tile();
	// Look at screen + 1/2.
	TileRect winrect = gwin->get_win_tile_rect();
	winrect.enlarge(winrect.w / 4);
	if (!winrect.has_world_point(npcpos.tx, npcpos.ty)) {
		return 0;
	}
	// Get to within 1 tile.
	Game_object* found = nullptr;    // Find one we can get to.
	std::unique_ptr<Path_walking_actor_action>
			pact;    // Gets ->action to walk there.
	for (size_t i = 0; !found && i < shapes.size(); i++) {
		Game_object_vector objs;    // Find nearby.
		const int          cnt = npc->find_nearby(objs, shapes[i], 20, 0);
		int                j;
		for (j = 0; j < cnt; j++) {
			Game_object* obj   = objs[j];
			const int    shnum = obj->get_shapenum();
			if (!bg) {    // Serpent isle?
				const int frnum = obj->get_framenum();
				if ((shnum == 290 || shnum == 291)
					&&    // Shutters?
						  // Want closed during day.
					(shapes.data() == day.data()) != (frnum <= 3)) {
					continue;
				}
				// Serpent lamp posts can't toggle.
				if ((shnum == 526 || shnum == 889)
					&& (frnum == 0 || frnum == 1 || frnum == 4 || frnum == 5)) {
					continue;
				}
			}
			Approach_object_pathfinder_client cost(npc, obj, 1);
			pact.reset(Path_walking_actor_action::create_path(
					npcpos, obj->get_tile(), cost));
			if (pact) {
				found = obj;
				break;
			}
			street_maintenance_failures++;
		}
	}
	if (!found || !pact) {
		return 0;    // Failed.
	}
	// Set actor to walk there.
	npc->set_schedule_type(
			Schedule::street_maintenance,
			std::make_unique<Street_maintenance_schedule>(
					npc, pact.release(), found));
	// Warning: we are deleted here
	return 1;
}

void Schedule::im_dormant() {
	npc->set_action(nullptr);
}

/*
 *  Get actual schedule (for Usecode intrinsic).
 */

int Schedule::get_actual_type(Actor* npc) const {
	return npc->get_schedule_type();
}

bool Schedule::try_proximity_usecode(int odds) {
	if ((rand() % odds) == 0) {
		// Note: since we can get here from within usecode, lets not
		// call the function directly but queue it instead.
		auto* scr = new Usecode_script(npc);
		(*scr) << Ucscript::dont_halt << Ucscript::usecode2
			   << npc->get_usecode() << Usecode_machine::npc_proximity;
		scr->start();
		npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		return true;
	}
	return false;
}

/*
 *  Remove desk items NPC is holding (that we created).
 */

void Schedule_with_objects::cleanup() {
	for (const Game_object_weak& elem : created) {
		const Game_object_shared item = elem.lock();
		if (item && item->get_owner() == npc) {
			item->remove_this();
		}
	}
	created.clear();
}

void Schedule_with_objects::set_current_item(Game_object* obj) {
	current_item = weak_from_obj(obj);
}

void Schedule_with_objects::add_object(Game_object* obj) {
	created.push_back(obj->weak_from_this());
}

Schedule_with_objects::~Schedule_with_objects() {
	cleanup();
}

/*
 *  Walk to an item.
 *  Output: false if failed.
 */
bool Schedule_with_objects::walk_to_random_item(int dist) {
	Game_object_vector vec;
	current_item     = Game_object_weak();
	const int nitems = find_items(vec, dist);
	if (nitems) {
		Game_object* item    = vec[rand() % nitems];
		current_item         = weak_from_obj(item);
		Tile_coord spot      = item->get_tile();
		const int  floor     = npc->get_lift() / 5;
		spot.tz              = floor * 5;
		const Tile_coord pos = Map_chunk::find_spot(spot, 1, npc);
		if (pos.tx != -1
			&& npc->walk_path_to_tile(
					pos, gwin->get_std_delay(), 1000 + rand() % 1000)) {
			return true;
		}
	}
	return false;
}

/*
 *  Drop an item on a table.
 */
bool Schedule_with_objects::drop_item(
		Game_object* to_drop, Game_object* table) {
	Tile_coord     spot = npc->get_tile();
	const TileRect foot = table->get_footprint();
	// East/West of table?
	if (spot.ty >= foot.y && spot.ty < foot.y + foot.h) {
		spot.tx = spot.tx <= foot.x ? foot.x : foot.x + foot.w - 1;
	} else {    // North/south.
		spot.ty = spot.ty <= foot.y ? foot.y : foot.y + foot.h - 1;
	}
	const Shape_info& info = table->get_info();
	spot.tz                = table->get_lift() + info.get_3d_height();
	const Tile_coord pos   = Map_chunk::find_spot(
            spot, 1, to_drop->get_shapenum(), to_drop->get_framenum());
	if (pos.tx != -1 && pos.tz == spot.tz
		&& foot.has_world_point(pos.tx, pos.ty)) {
		// Passes test.
		npc->set_action(new Pickup_actor_action(to_drop, pos, 250, false));
		--items_in_hand;
		return true;
	} else {
		return false;
	}
}

/*
 *  Run usecode function.
 */

void Scripted_schedule::run(int id    // A Usecode function #.
) {
	if (id) {
		ucmachine->call_method(inst, id, npc);
	}
}

/*
 *  Lookup Usecode 'method'.
 */

static int find_method(
		Usecode_class_symbol* cls, const char* meth, bool noerr) {
	Usecode_symbol* ucsym = (*cls)[meth];
	if (!ucsym) {
		if (!noerr) {
			cerr << "Failed to find method '" << meth << "' in class '"
				 << cls->get_name() << "'." << endl;
		}
		return 0;
	}
	return ucsym->get_val();
}

/*
 *  Create schedule that uses Usecode methods for the actions.
 */

Scripted_schedule::Scripted_schedule(Actor* n, int type) : Schedule(n) {
	const char*           nm  = Schedule_change::get_script_name(type);
	Usecode_class_symbol* cls = ucmachine->get_class(nm);
	if (!cls) {
		cerr << "Could not find scripted schedule '" << nm
			 << "'. Switching to 'Loiter' schedule instead." << endl;
		npc->set_schedule_type(loiter);
		return;
	} else {
		inst = new Usecode_value(0);
		inst->class_new(cls, cls->get_num_vars());
		now_what_id   = find_method(cls, "now_what", false);
		im_dormant_id = find_method(cls, "im_dormant", true);
		ending_id     = find_method(cls, "ending", true);
		set_weapon_id = find_method(cls, "set_weapon", true);
		set_bed_id    = find_method(cls, "set_bed", true);
	}
}

/*
 *  Cleanup.
 */

Scripted_schedule::~Scripted_schedule() {
	ucmachine->call_method(inst, -1, nullptr);    // Free 'inst'.
}

/*
 *  Create schedule to turn lamps on/off, open/close shutters.
 */

Street_maintenance_schedule::Street_maintenance_schedule(
		Actor* n, Actor_action* p, Game_object* o)
		: Schedule(n), obj(weak_from_obj(o)), shapenum(o->get_shapenum()),
		  framenum(o->get_framenum()), paction(p),
		  oldloc(n->get_schedule()->get_start_pos()) {}

/*
 *  Street-maintenance schedule.
 */

void Street_maintenance_schedule::now_what() {
	if (paction) {    // First time?
		// Set to follow given path.
		cout << npc->get_name() << " walking for street maintenance" << endl;
		npc->set_action(paction);
		npc->start(gwin->get_std_delay());
		paction = nullptr;
		return;
	}
	const Game_object_shared objptr = obj.lock();
	if (objptr && npc->distance(objptr.get()) <= 2 &&    // We're there.
		objptr->get_shapenum() == shapenum
		&& objptr->get_framenum() == framenum) {
		cout << npc->get_name() << " is about to perform street maintenance"
			 << endl;
		const int        dir = npc->get_direction(objptr.get());
		const std::array frames{
				static_cast<signed char>(
						npc->get_dir_framenum(dir, Actor::standing)),
				static_cast<signed char>(npc->get_dir_framenum(dir, 3)),
		};
		signed char   standframe = frames[0];
		Actor_action* pact       = nullptr;
		if (shapenum == 997) {    // Spent light source
			const int hour = gclock->get_hour();
			int       newshp;
			if (hour >= 18 || hour < 6) {    // Night
				newshp = 338;                // Lit light source
			} else {
				newshp = 336;    // Unlit light source
			}
			// Makes it last some 5-9 hours; quality seems to be within this
			// range in the original.
			const int qual = 30 + rand() % 27;
			pact           = new Change_actor_action(
                    objptr.get(), newshp, framenum, qual);
		} else {    // Just call the correct usecode.
			pact = new Activate_actor_action(objptr.get());
		}

		npc->set_action(new Sequence_actor_action(
				new Frames_actor_action(frames.data(), frames.size()),
				new Face_pos_actor_action(
						objptr->get_tile(), gwin->get_std_delay()),
				pact, new Frames_actor_action(&standframe, 1)));
		npc->start(gwin->get_std_delay());
		if (npc->can_speak()) {
			switch (shapenum) {
			case 322:    // Closing shutters.
			case 372:
				npc->say(first_close_shutters, last_close_shutters);
				break;
			case 290:    // Open shutters (or both for SI).
			case 291:
				if (Game::get_game_type() == BLACK_GATE) {
					npc->say(first_open_shutters, last_open_shutters);
				} else    // SI.
					if (framenum <= 3) {
						npc->say(first_open_shutters, last_open_shutters);
					} else {
						npc->say(first_close_shutters, last_close_shutters);
					}
				break;
			case 336:    // Turn on light source.
			case 889:    // Turn on lamp.
				npc->say(first_lamp_on, last_lamp_on);
				break;
			case 338:    // Turn off light source.
			case 526:    // Turn off lamp.
				npc->say(lamp_off);
				break;
			case 997:    // Replacing candle.
				npc->say(new_candle);
				break;
			}
		}
		shapenum = 0;    // Don't want to repeat.
		return;
	}
	cout << npc->get_name() << " is done with street maintenance" << endl;
	// Set back to old schedule.
	const int  period  = gclock->get_hour() / 3;
	Actor*     safenpc = npc;
	Tile_coord safeloc = oldloc;
	// This sometimes fails after a restore.
	safenpc->update_schedule(period, 0, &safeloc);
	if (safenpc->get_schedule_type() == static_cast<int>(street_maintenance)) {
		safenpc->update_schedule(period, 0, &safeloc);
	}
}

void Street_maintenance_schedule::ending(int newtype) {
	ignore_unused_variable_warning(newtype);
}

/*
 *  Get actual schedule (for Usecode intrinsic).
 */

int Street_maintenance_schedule::get_actual_type(Actor* /* npc */
) const {
	return prev_type;
}

/*
 *  What do we do now?
 */

void Follow_avatar_schedule::now_what() {
	const bool is_blocked = blocked.tx != -1;
	blocked               = Tile_coord(-1, -1, -1);
	if (npc->get_flag(Obj_flags::asleep) || npc->is_dead()
		|| npc->get_flag(Obj_flags::paralyzed)
		|| gwin->main_actor_dont_move()) {    // Under Usecode control.
		return;                               // Disabled.
	}
	Actor*           av        = gwin->get_main_actor();
	const Tile_coord leaderpos = av->get_tile();
	const Tile_coord pos       = npc->get_tile();
	const int        dist2lead = av->distance(pos);
	if (!av->is_moving() &&    // Avatar stopped.
		dist2lead <= 12) {     // And we're already close enough.
		return;
	}
	const uint32 curtime = SDL_GetTicks();    // Want the REAL time here.
	if (!is_blocked) {                        // Not blocked?
		npc->follow(av);                      // Then continue following.
		return;
	}
	if (curtime < next_path_time) {    // Failed pathfinding recently?
		// Wait a bit.
		npc->start(gwin->get_std_delay(), next_path_time - curtime);
		return;
	}
	// Find a free spot within 3 tiles.
	Map_chunk::Find_spot_where where = Map_chunk::anywhere;
	// And try to be inside/outside.
	where                 = gwin->is_main_actor_inside() ? Map_chunk::inside
														 : Map_chunk::outside;
	const Tile_coord goal = Map_chunk::find_spot(leaderpos, 3, npc, 0, where);
	if (goal.tx == -1) {    // No free spot?  Give up.
		cout << npc->get_name() << " can't find free spot" << endl;
		next_path_time = SDL_GetTicks() + 1000;
		return;
	}
	// Get his speed.
	int speed = av->get_frame_time();
	if (!speed) {    // Avatar stopped?
		speed = gwin->get_std_delay();
	}
	if (pos.distance(goal) <= 3) {
		return;    // Already close enough!
	}
	// Succeed if within 3 tiles of goal.
	if (npc->walk_path_to_tile(goal, speed - speed / 4, 0, 3, 1)) {
		return;    // Success.
	}
	cout << "... but failed to find path." << endl;
	// On screen (roughly)?
	int ok;
	// Get window rect. in tiles.
	const TileRect wrect = gwin->get_win_tile_rect();
	if (wrect.has_world_point(pos.tx - pos.tz / 2, pos.ty - pos.tz / 2)) {
		// Try walking off-screen.
		ok = npc->walk_path_to_tile(
				Tile_coord(-1, -1, -1), speed - speed / 4, 0);
	} else {    // Off screen already?
		ok = npc->approach_another(av);
	}
	if (!ok) {    // Failed? Don't try again for a bit.
		next_path_time = SDL_GetTicks() + 1000;
	}
}

/*
 *  Waiting...
 */

void Wait_schedule::now_what() {}

/*
 *  Create a horizontal pace schedule.
 */

std::unique_ptr<Pace_schedule> Pace_schedule::create_horiz(Actor* n) {
	const Tile_coord t = n->get_tile();    // Get his position.
	return std::make_unique<Pace_schedule>(n, 1, t);
}

/*
 *  Create a vertical pace schedule.
 */

std::unique_ptr<Pace_schedule> Pace_schedule::create_vert(Actor* n) {
	const Tile_coord t = n->get_tile();    // Get his position.
	return std::make_unique<Pace_schedule>(n, 0, t);
}

void Pace_schedule::pace(
		Actor* npc, char& which, int& phase, Tile_coord& blocked, int delay) {
	const int dir
			= npc->get_dir_facing();    // Use NPC facing for starting direction
	switch (phase) {
	case 1: {
		bool changedir = false;
		switch (dir) {
		case north:
		case south:
			if (which) {
				changedir = true;
			}
			break;
		case east:
		case west:
			if (!which) {
				changedir = true;
			}
			break;
		}
		if (changedir) {
			phase = 4;
			npc->start(delay, delay);
			return;
		}
		if (blocked.tx != -1) {    // Blocked?
			Game_object* obj = npc->find_blocking(blocked, dir);
			if (obj) {
				if (obj->as_actor()) {
					if (npc->can_speak()) {
						npc->say(first_move_aside, last_move_aside);
						// Ask NPC to move aside.
						if (obj->move_aside(npc, dir)) {
							// Wait longer.
							npc->start(3 * delay, 3 * delay);
						} else {
							// Wait longer.
							npc->start(delay, delay);
						}
						return;
					}
				}
			}
			blocked.tx = -1;
			changedir  = true;
		}

		if (changedir) {
			phase++;
		} else {
			const Tile_coord p0         = npc->get_tile().get_neighbor(dir);
			Frames_sequence* frames     = npc->get_frames(dir);
			int&             step_index = npc->get_step_index();
			if (!step_index) {    // First time?  Init.
				step_index = frames->find_unrotated(npc->get_framenum());
			}
			// Get next (updates step_index).
			const int frame = frames->get_next(step_index);
			// One step at a time.
			npc->step(p0, frame);
		}
		npc->start(delay, delay);
		break;
	}
	case 2:
		phase++;
		npc->change_frame(
				npc->get_dir_framenum(npc->get_dir_facing(), Actor::standing));
		npc->start(2 * delay, 2 * delay);
		break;
	case 3:
	case 4: {
		phase++;
		npc->change_frame(
				npc->get_dir_framenum((dir + 6) % 8, Actor::standing));
		npc->start(2 * delay, 2 * delay);
		break;
	}
	default:
		phase                 = 1;
		npc->get_step_index() = 0;
		npc->start(2 * delay, 2 * delay);
		break;
	}
}

/*
 *  Schedule change for pacing:
 */

void Pace_schedule::now_what() {
	const int delay = gwin->get_std_delay();
	if (!phase) {
		phase++;
		if (loc != npc->get_tile()) {
			npc->walk_to_tile(loc, delay, delay);
		} else {
			npc->start(delay, delay);
		}
	} else {
		Pace_schedule::pace(npc, which, phase, blocked, delay);
	}
}

/*
 *  Just went dormant.
 */
void Eat_at_inn_schedule::im_dormant() {
	// Force NPCs to sit down again after cache-out/cache-in.
	// npc->change_frame(npc->get_dir_framenum(Actor::standing));
	npc->set_action(nullptr);
}

/*
 *  Eat at inn.
 */

void Eat_at_inn_schedule::now_what() {
	const int frnum = npc->get_framenum() & 0xf;
	if (!sitting_at_chair || frnum != Actor::sit_frame) {
		// First have to sit down.
		constexpr static const std::array chairs{873, 292};
		// Already good?
		if (frnum == Actor::sit_frame && npc->find_closest(chairs, 1)) {
			sitting_at_chair = true;
		} else {
			if (!Sit_schedule::set_action(npc)) {
				// Try again in a while.
				npc->start(250, 5000);
			}
			return;
		}
	}
	Game_object_vector foods;    // Food nearby?
	const int          cnt = npc->find_nearby(foods, 377, 2, 0);
	if (cnt) {    // Found?
		// Find closest.
		Game_object* food = nullptr;
		int          dist = 500;
		for (auto* obj : foods) {
			const int odist = obj->distance(npc);
			if (odist < dist) {
				dist = odist;
				food = obj;
			}
		}
		if (rand() % 5 == 0 && food) {
			gwin->add_dirty(food);
			food->remove_this();
		}
		if (npc->can_speak() && rand() % 4) {
			npc->say(first_munch, last_munch);
		}
	} else if (npc->can_speak() && rand() % 4) {
		npc->say(first_more_food, last_more_food);
	}
	// Wake up in a little while.
	npc->start(250, 5000 + rand() % 12000);
}

// TODO: This should be in a loop to remove food one at a time with a delay
void Eat_at_inn_schedule::ending(int new_type) {    // new schedule type
	ignore_unused_variable_warning(new_type);
	Game_object_vector foods;    // Food nearby?
	const int          cnt = npc->find_nearby(foods, 377, 2, 0);
	if (cnt) {    // Found?
		for (auto* food : foods) {
			if (food) {
				gwin->add_dirty(food);
				food->remove_this();
				if (npc->can_speak()) {
					npc->say(first_munch, last_munch);
				}
			}
		}
	}
}

/*
 *  Find someone listening to the preacher.
 */

Actor* Find_congregant(Actor* npc) {
	Actor_vector vec;
	Actor_vector vec2;
	if (!npc->find_nearby_actors(vec, c_any_shapenum, 16)) {
		return nullptr;
	}
	vec2.reserve(vec.size());    // Get list of ones to consider.
	for (auto* act : vec) {
		if (act->get_schedule_type() == Schedule::sit && !act->is_in_party()) {
			vec2.push_back(act);
		}
	}
	return !vec2.empty() ? vec2[rand() % vec2.size()] : nullptr;
}

/*
 *  Preach:
 */

void Preach_schedule::now_what() {
	switch (state) {
	case find_podium: {
		Game_object_vector vec;
		if (!npc->find_nearby(vec, 697, 17, 0)) {
			npc->set_schedule_type(loiter);
			return;
		}
		Game_object*     podium       = vec[0];
		Tile_coord       pos          = podium->get_tile();
		static const int deltas[4][2] = {
				{-1,  0},
                { 1,  0},
                { 0, -2},
                { 0,  1}
        };
		const int frnum = podium->get_framenum() % 4;
		pos.tx += deltas[frnum][0];
		pos.ty += deltas[frnum][1];
		Actor_pathfinder_client cost(npc, 0);
		Actor_action*           pact = Path_walking_actor_action::create_path(
                npc->get_tile(), pos, cost);
		if (pact) {
			state = at_podium;
			npc->set_action(new Sequence_actor_action(
					pact, new Face_pos_actor_action(podium, 200)));
			npc->start(gwin->get_std_delay());
			return;
		}
		npc->start(250, 5000 + rand() % 5000);    // Try again later.
		return;
	}
	case at_podium:
		if (rand() % 2) {    // Just wait a little.
			npc->start(gwin->get_std_delay(), rand() % 3000);
		} else {
			if (rand() % 3) {
				state = exhort;
			} else if ((GAME_SI) || rand() % 3) {
				state = visit;
			} else {
				state = find_icon;
			}
			npc->start(gwin->get_std_delay(), 2000 + rand() % 2000);
		}
		return;
	case exhort: {
		signed char  frames[8];    // Frames.
		const size_t cnt = 1 + rand() % (sizeof(frames) - 1);
		// Frames to choose from:
		static const char choices[]{0, 8, 9};
		for (size_t i = 0; i < cnt - 1; i++) {
			frames[i] = npc->get_dir_framenum(
					choices[rand() % (sizeof(choices))]);
		}
		// Make last one standing.
		frames[cnt - 1] = npc->get_dir_framenum(Actor::standing);
		npc->set_action(new Frames_actor_action(frames, cnt, 250));
		npc->start(gwin->get_std_delay());
		npc->say(first_preach, last_preach);
		state         = at_podium;
		Actor* member = Find_congregant(npc);
		if (member) {
			auto* scr = new Usecode_script(member);
			scr->add(Ucscript::delay_ticks, 3);
			scr->add(Ucscript::face_dir, member->get_dir_facing());
			scr->add(Ucscript::npc_standing_frame);
			scr->add(
					Ucscript::say,
					get_text_msg(
							first_amen
							+ rand() % (last_amen - first_amen + 1)));
			scr->add(Ucscript::delay_ticks, 2);
			scr->add(Ucscript::npc_sit_frame);
			scr->start();    // Start next tick.
		}
		return;
	}
	case visit: {
		state = find_podium;
		npc->start(gwin->get_std_delay(), 1000 + rand() % 2000);
		Actor* member = Find_congregant(npc);
		if (!member) {
			return;
		}
		const Tile_coord        pos = member->get_tile();
		Actor_pathfinder_client cost(npc, 1);
		Actor_action*           pact = Path_walking_actor_action::create_path(
                npc->get_tile(), pos, cost);
		if (!pact) {
			return;
		}
		npc->set_action(new Sequence_actor_action(
				pact, new Face_pos_actor_action(member, 200)));
		state = talk_member;
		return;
	}
	case talk_member:
		state = find_podium;
		npc->say(first_preach2, last_preach2);
		npc->start(250, 2000);
		return;
	case find_icon: {
		state = find_podium;    // In case we fail.
		npc->start(2 * gwin->get_std_delay());
		Game_object* icon = npc->find_closest(724);
		if (!icon) {
			return;
		}
		Tile_coord pos = icon->get_tile();
		pos.tx += 2;
		pos.ty -= 1;
		Actor_pathfinder_client cost(npc, 0);
		Actor_action*           pact = Path_walking_actor_action::create_path(
                npc->get_tile(), pos, cost);
		if (pact) {
			npc->set_action(pact);
			state = pray;
		}
		return;
	}
	case pray: {
		auto* scr = new Usecode_script(npc);
		(*scr) << Ucscript::face_dir << 6    // Face west.
			   << Ucscript::npc_standing_frame << Ucscript::npc_bow_frame
			   << Ucscript::delay_ticks << 3 << Ucscript::npc_kneel_frame;
		scr->add(Ucscript::say, get_text_msg(first_amen + rand() % 2));
		(*scr) << Ucscript::delay_ticks << 5 << Ucscript::npc_bow_frame
			   << Ucscript::delay_ticks << 3 << Ucscript::npc_standing_frame;
		scr->start();    // Start next tick.
		state = find_podium;
		npc->start(2 * gwin->get_std_delay(), 4000);
		return;
	}
	default:
		state = find_podium;
		npc->start(250);
		return;
	}
}

Patrol_schedule::Patrol_schedule(Actor* n)
		: Schedule(n), pathnum(-1), dir(1), state(-1), center(0, 0, 0),
		  whichdir(0), phase(1), pace_count(0), seek_combat(false),
		  forever(false) {
	if (num_path_eggs < 0) {
		Shapes_vga_file& shapes = Shape_manager::get_instance()->get_shapes();
		num_path_eggs           = shapes.get_num_frames(PATH_SHAPE) - 1;
	}
}

/*
 *  Schedule change for patroling:
 */

void Patrol_schedule::now_what() {
	if (seek_combat && seek_foes()) {    // Check for nearby foes.
		return;
	}
	const int    speed = gwin->get_std_delay();
	Game_object* path;
	bool         gotpath = true;
	switch (state) {
	case -1: {    // Initialization
		Game_object_vector nearby;
		npc->find_nearby(
				nearby, PATH_SHAPE, 25, 0x10, c_any_qual, c_any_framenum);
		gotpath = !nearby.empty();
		[[fallthrough]];
	}
	case 0: {    // Find next path.
		if (!gotpath) {
			// SI seems to try switching to preprogrammed schedules
			// first if no path.
			if (GAME_SI) {
				Schedule_change* list;
				int              cnt;
				npc->get_schedules(list, cnt);
				if (cnt != 0
					&& list[npc->find_schedule_at_time(gclock->get_hour() / 3)]
									   .get_type()
							   != Schedule::patrol) {
					npc->update_schedule(gclock->get_hour() / 3);
				} else {
					// when the preset schedule is patrol or there is no preset
					// change schedule to loiter
					npc->set_schedule_type(loiter);
				}
				// The above will have deleted us, so return immediately.
				return;
			}
			seek_combat = true;
			state       = 4;
			npc->start(2 * speed + rand() % 500);
			break;
		} else if (forever) {
			// Flag 64 is 'Repeat Forever', and means 'just use the last
			// path egg again and again' (but see hack note below).
			state = 1;
			npc->start(speed, speed);
			break;
		}
		pathnum += dir;    // Find next path.
		if (pathnum == 0 && dir == -1) {
			dir = 1;    // Start over from zero.
		} else if (pathnum == num_path_eggs && dir == 1) {
			dir = -1;    // Start backwards from num_path_eggs.
		}
		if (pathnum >= static_cast<int>(paths.size())) {
			paths.resize(pathnum + 1);
		}
		// Already know its location?
		path = pathnum >= 0 ? paths[pathnum] : nullptr;
		if (!path) {    // No, so look around.
			Game_object_vector nearby;
			npc->find_nearby(nearby, PATH_SHAPE, 25, 0x10, c_any_qual, pathnum);
			path = find_nearest(npc, nearby);
			if (path) {    // Save it.
				paths[pathnum] = path;
			} else {    // Turn back if at end.
				dir = -dir;
				pathnum += dir;
				npc->start(speed, speed);
				break;
			}
		}
		Tile_coord d = path->get_tile();
		if (!npc->walk_path_to_tile(d, speed, 2 * speed + rand() % 500)) {
			// Look for free tile within 1 square.
			d = Map_chunk::find_spot(
					d, 1, npc->get_shapenum(), npc->get_framenum(), 1);
			if (d.tx == -1
				|| !npc->walk_path_to_tile(d, speed, rand() % 1000)) {
				// Failed.  Later.
				npc->start(speed, 2000);
				return;
			}
		}
		state = 1;    // Walking to path.
		break;
	}
	case 1:                    // Walk to next path.
		if (pathnum >= 0 &&    // Arrived at path?
			static_cast<unsigned int>(pathnum) < paths.size()
			&& (path = paths[pathnum]) != nullptr
			&& (forever || npc->distance(path) < 2)) {
			int delay = 2;
			// Scripts for all actions. At worst, display standing frame
			// once the path egg is reached.
			auto* scr = new Usecode_script(npc);
			(*scr) << Ucscript::npc_standing_frame;
			// Quality = type.  (I think high bits
			// are flags).
			const int qual = path->get_quality();
			seek_combat    = (qual & 32) != 0;
			forever        = (qual & 64) != 0;
			// Hack to fix pirate fort at the Isle of the Avatar (BG).
			// It works "incorrectly" in the original.
			if (forever && GAME_BG && (qual & 31) == 0) {
				forever = false;
			}
			// TODO: Find out what flag 128 means. It would seem that it is
			// 'Exc. Reserved.', but what does those mean?
			switch (qual & 31) {
			case 0:    // None.
				break;
			case 25:    // 50% wrap to 0.
				if (rand() % 2) {
					break;
				}
				[[fallthrough]];
			case 1:    // Wrap to 0.
				pathnum = -1;
				dir     = 1;
				break;
			case 2:    // Pause; guessing 3 ticks.
				(*scr) << Ucscript::delay_ticks << 3;
				delay = 5;
				break;
			case 24:    // Read
				// Find the book which will be read.
				book = weak_from_obj(npc->find_closest(642, 4));
				[[fallthrough]];
			case 3:    // Sit.
				if (Sit_schedule::set_action(npc)) {
					scr->start();    // Start next tick.
					state = 2;
					return;
				}
				break;
			case 4:    // Kneel at tombstone.
			case 5:    // Kneel
				// TODO: Both are identical in SI, which has no tombs.
				// In BG, placing this path egg near a tomb causes the
				// NPC to kneel in front of the tomb, drops flowers in
				// front of it (shape 999, frames 1, 2, 3, 6 or 7 at
				// random, then stands up again after a while.
				// If anyone complains enough I will add it.
				// -- Marzo Jan 22/2013
				(*scr) << Ucscript::delay_ticks << 2 << Ucscript::npc_bow_frame
					   << Ucscript::delay_ticks << 4
					   << Ucscript::npc_kneel_frame << Ucscript::delay_ticks
					   << 20 << Ucscript::npc_bow_frame << Ucscript::delay_ticks
					   << 4 << Ucscript::npc_standing_frame;
				delay = 36;
				break;
			case 6:              // Loiter.
				scr->start();    // Start next tick.
				center = path->get_tile();
				state  = 4;
				npc->start(speed, speed * delay);
				return;
			case 7:      // Left about-face.
			case 8: {    // Right about-face.
				const int  dirs_left[]  = {6, 4};
				const int  dirs_right[] = {2, 4};
				const int* face_dirs;
				if ((path->get_quality() & 31) == 7) {
					face_dirs = dirs_right;
				} else {
					face_dirs = dirs_left;
				}
				const int facing = npc->get_dir_facing();
				for (int i = 0; i < 2; i++) {
					(*scr) << Ucscript::delay_ticks << 2 << Ucscript::face_dir
						   << (facing + face_dirs[i]) % 8;
				}
				delay = 8;
				break;
			}
			case 9:     // Horiz. pace.
			case 10:    // Vert. pace.
				// Note: both do vertical pacing in the originals.
				// I am basing functionality on the names given in
				// text.flx; this causes Iolo in SI prison to be
				// correct, but causes Fawn guards to perform an
				// horizontal pace. This actually looks better, and
				// seems to be what the world builders intended.
				whichdir   = (qual & 1);
				pace_count = -1;
				phase      = 1;
				scr->start();    // Start next tick.
				center = path->get_tile();
				state  = 5;
				npc->start(speed, speed * delay);
				return;
			case 11:    // 50% reverse.
				if (rand() % 2) {
					dir *= -1;
				}
				break;
			case 12:    // 50% skip next.
				if (rand() % 2) {
					pathnum += dir;
					// This must be done to ensure valid ranges.
					if (pathnum == 0 && dir == -1) {
						dir = 1;    // Start over from zero.
					} else if (pathnum == num_path_eggs && dir == 1) {
						dir = -1;    // Start backwards from num_path_eggs.
					}
				}
				break;
			case 13: {    // Hammer.
						  // Try to use an existing hammer.
				const Game_object_shared existing = hammer.lock();
				hammer = weak_from_obj(set_procure_item_action(
						npc, existing.get(), 15, 623, 0));
				scr->start();    // Start next tick.
				state = 6;
				npc->start(speed, speed * delay);
				return;
			}
			case 14:    // Check area.
				// Force check for lamps, etc.
				street_maintenance_time = 0;
				if (try_street_maintenance()) {
					delete scr;
					return;    // We no longer exist.
				}
				delay += 2;
				(*scr) << Ucscript::delay_ticks << 2;
				break;
			case 15:    // Usecode.
				// Don't let this script halt others,
				//   as it messes up automaton in
				//   SI-Freedom.
				(*scr) << Ucscript::dont_halt;
				// HACK: This is needed for SS Maze: it makes the
				// party wait outside instead of endlessly looping
				// at the maze's door.
				// TODO: Need to check if both functions get called
				// on this case or if only one is, as is done here.
				if (GAME_SI && npc->get_ident() == 31) {
					(*scr) << Ucscript::usecode2 << SSMazePartyWait
						   << static_cast<int>(Usecode_machine::internal_exec);
				} else {
					(*scr) << Ucscript::usecode2 << npc->get_usecode()
						   << static_cast<int>(Usecode_machine::npc_proximity);
				}
				delay = 3;
				break;
			case 16:    // Bow to ground.
				// Trying to differentiate this from 17, below.
				(*scr) << Ucscript::delay_ticks << 2 << Ucscript::npc_bow_frame
					   << Ucscript::delay_ticks << 2;
				delay = 8;
				break;
			case 17:    // Bow from ground.
				// Trying to differentiate this from 16, above.
				(*scr) << Ucscript::npc_bow_frame << Ucscript::delay_ticks << 2
					   << Ucscript::npc_standing_frame;
				delay = 8;
				break;
			case 20:    // Ready weapon
				npc->ready_best_weapon();
				break;
			case 21:    // Unready weapon
				npc->empty_hands();
				break;
			case 22:      // One-handed swing.
			case 23: {    // Two-handed swing.
				const int    dir = npc->get_dir_facing();
				signed char  frames[14];    // Get frames to show.
				Game_object* weap = npc->get_readied(rhand);
				int          cnt  = npc->get_attack_frames(
                        weap ? weap->get_shapenum() : 0, false, dir, frames);
				frames[cnt++] = npc->get_dir_framenum(dir, Actor::ready_frame);
				frames[cnt++] = npc->get_dir_framenum(dir, Actor::standing);
				if (cnt) {
					npc->set_action(
							new Frames_actor_action(frames, cnt, speed));
				}
				delay = cnt;    // Get back into time queue.
				break;
			}
			// These two don't seem to be used at all in the originals.
			// Their text.flx names suggest that they are meant to do some
			// sort of synchronization: a working hypothesis based on multi-
			// thread programming concepts is that 18 (wait for semaphore)
			// puts NPCs in some sort of wait state until another NPC also
			// in patrol schedule reaches 19 (release semaphore).
			// TODO: Come up with a way to test hypothesis above.
			case 18:    // Wait for semaphore
			case 19:    // Release semaphore
			default:
#ifdef DEBUG
				cout << "Unhandled path egg quality in patrol schedule: "
					 << qual << endl;
#endif
				break;
			}
			scr->start();    // Start next tick.
			state = 0;       // THEN, find next path.
			npc->start(speed, speed * delay);
		} else {
			state = 0;    // Walking to path.
			npc->start(speed, speed);
		}
		break;
	case 2:    // Sitting/reading.
		// Stay 5-15 secs.
		if ((npc->get_framenum() & 0xf) == Actor::sit_frame) {
			const Game_object_shared book_obj = book.lock();
			if (book_obj && npc->distance(book_obj.get()) < 4) {
				// Open book if reading.
				const int frnum = book_obj->get_framenum();
				if (frnum % 3) {
					book_obj->change_frame(frnum - frnum % 3);
				}
			}
			npc->start(250, 5000 + rand() % 10000);
		} else {    // Not sitting.
			npc->start(250, rand() % 1000);
		}
		state = 3;    // Continue on afterward.
		break;
	case 3:    // Stand up.
		if ((npc->get_framenum() & 0xf) == Actor::sit_frame) {
			const Game_object_shared book_obj = book.lock();
			if (book_obj && npc->distance(book_obj.get()) < 4) {
				// Close book.
				const int frnum = book_obj->get_framenum();
				book_obj->change_frame(frnum - frnum % 3 + 1);
				book = Game_object_weak();
			}
			// Standing up animation.
			auto* scr = new Usecode_script(npc);
			(*scr) << Ucscript::delay_ticks << 2 << Ucscript::npc_bow_frame
				   << Ucscript::delay_ticks << 2
				   << Ucscript::npc_standing_frame;
			scr->start();    // Start next tick.
			npc->start(speed, speed * 7);
		}
		state = 0;
		break;
	case 4:    // Loiter.
		if (rand() % 5 == 0) {
			state = 0;
			npc->start(speed, speed);
		} else {
			const int dist = 12;
			const int newx = center.tx - dist + rand() % (2 * dist);
			const int newy = center.ty - dist + rand() % (2 * dist);
			// Wait a bit.
			npc->walk_to_tile(newx, newy, center.tz, speed, rand() % 2000);
		}
		break;
	case 5:    // Pacing.
		if (npc->get_tile().distance(center) < 1) {
			pace_count++;
			if (pace_count == 6) {
				npc->change_frame(npc->get_dir_framenum(
						npc->get_dir_facing(), Actor::standing));
				npc->start(3 * speed, 3 * speed);
				state = 0;
				break;
			}
			if (phase >= 2 && phase <= 4) {
				pace_count--;
			}
		}
		Pace_schedule::pace(npc, whichdir, phase, blocked, speed);
		break;
	case 6: {    // Ready hammer and swing it.
				 // Hammer should be in inventory by now.
		const Game_object_shared hammer_obj = hammer.lock();
		Game_object_shared       keep;
		if (!hammer_obj) {
			// uh-oh... this should be impossible.
			// Try finding hammer again
			state = 1;
			npc->start(speed, speed);
		}
		hammer_obj->remove_this(&keep);
		// For safety, unready weapon first.
		npc->empty_hands();
		// Ready the hammer in the weapon hand.
		npc->add_readied(hammer_obj.get(), lhand, false, true);
		npc->add_dirty();
		// Number of swings.
		const int repcnt = 1 + rand() % 3;

		auto* scr = new Usecode_script(npc);
		(*scr) << Ucscript::delay_ticks << 2 << Ucscript::npc_ready_frame
			   << Ucscript::delay_ticks << 2 << Ucscript::npc_raise1_frame
			   << Ucscript::delay_ticks << 2 << Ucscript::sfx
			   << Audio::game_sfx(45) << Ucscript::npc_out_frame
			   << Ucscript::delay_ticks << 2 << Ucscript::repeat << -13
			   << repcnt << Ucscript::delay_ticks << 2
			   << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 2
			   << Ucscript::npc_standing_frame;
		const int delay = 11 * (repcnt + 1) + 6;
		scr->start();    // Start next tick.
		state = 0;       // THEN, find next path.
		npc->start(speed, speed * delay);
		break;
	}
	default:
		// Just in case.
		break;
	}
}

/*
 *  End of patrol
 */

void Patrol_schedule::ending(int new_type    // New schedule.
) {
	ignore_unused_variable_warning(new_type);
	const Game_object_shared hammer_obj = hammer.lock();
	if (hammer_obj) {
		hammer_obj->remove_this();
	}
}

Talk_schedule::Talk_schedule(Actor* n, int fb, int lb, int eid)
		: Schedule(n), firstbark(fb), lastbark(lb), eventid(eid), phase(0) {}

Talk_schedule::Talk_schedule(Actor* n)
		: Schedule(n), firstbark(first_talk), lastbark(last_talk),
		  eventid(GAME_BG ? Usecode_machine::double_click
						  : Usecode_machine::chat),
		  phase(0) {}

/*
 *  Schedule change for 'talk':
 */

void Talk_schedule::now_what() {
	const int speed = gwin->get_std_delay();
	// Switch to phase 3 if we are reasonable close
	if (phase < 3 && npc->distance(gwin->get_main_actor()) < 6) {
		phase = 3;
		npc->start(speed, 250);
		return;
	}
	// Don't talk to invisible avatar unless NPC can see invisible.
	if (gwin->get_main_actor()->get_flag(Obj_flags::invisible)
		&& !npc->can_see_invisible()) {
		// Try a little later.
		phase = 0;
		npc->start(speed, 5000);
		return;
	}
	switch (phase) {
	case 0: {    // Start by approaching Avatar.
		if (npc->distance(gwin->get_main_actor()) > 50) {
			// Too far?
			// Try a little later.
			npc->start(speed, 5000);
			return;
		}
		// Aim for within 5 tiles.
		Actor_pathfinder_client cost(npc, 5);
		Actor_action*           pact = Approach_actor_action::create_path(
                npc->get_tile(), gwin->get_main_actor(), 5, cost);
		if (!pact) {
#ifdef DEBUG
			cout << "Talk: Failed to find path for " << npc->get_name() << endl;
#endif
			// No path found; try again a little later.
			npc->start(speed, 500);
			return;
		} else {
			if (Fast_pathfinder_client::is_grabable(
						npc, gwin->get_main_actor())) {
				if (rand() % 3 == 0) {
					npc->say(firstbark, lastbark);
				}
			}
			// Walk there, and retry if
			//   blocked.
			npc->set_action(pact);
			npc->start(speed);    // Start walking.
		}
		phase++;
		return;
	}
	case 1:    // Wait a second.
	case 2: {
		if (Fast_pathfinder_client::is_grabable(npc, gwin->get_main_actor())) {
			if (rand() % 3 == 0) {
				npc->say(firstbark, lastbark);
			}
		}
		// Step towards Avatar.
		const Tile_coord pos  = npc->get_tile();
		const Tile_coord dest = gwin->get_main_actor()->get_tile();
		const int dx = dest.tx > pos.tx ? 1 : (dest.tx < pos.tx ? -1 : 0);
		const int dy = dest.ty > pos.ty ? 1 : (dest.ty < pos.ty ? -1 : 0);
		npc->walk_to_tile(pos + Tile_coord(dx, dy, 0), speed, 500);
		phase = 3;
		return;
	}
	case 3: {    // Talk.
		const int dist = npc->distance(gwin->get_main_actor());
		// Got to be close & reachable.
		if (dist > 5
			|| !Fast_pathfinder_client::is_grabable(
					npc, gwin->get_main_actor())) {
			phase = 0;
			npc->start(speed, 500);
			return;
		}
		// But first face Avatar.
		npc->change_frame(npc->get_dir_framenum(
				npc->get_direction(gwin->get_main_actor()), Actor::standing));
		phase++;
		npc->start(speed, 250);    // Wait another 1/4 sec.
		break;
	}
	case 4:
		npc->stop();    // Stop moving.
		// NOTE:  This could DESTROY us!
		npc->activate(eventid);
		// SO don't refer to any instance
		//   variables from here on.
		gwin->paint();
		return;
	default:
		break;
	}
}

/*
 *  Schedule change for 'arrest':
 */

Arrest_avatar_schedule::Arrest_avatar_schedule(Actor* n)
		: Talk_schedule(
				  n, first_arrest, last_arrest, Usecode_machine::double_click) {
	npc->set_usecode(ArrestUsecode);
}

/*
 *  Schedule change for 'arrest avatar':
 */

void Arrest_avatar_schedule::ending(int newtype) {
	if (newtype != combat) {
		npc->set_alignment(Actor::neutral);
	}
}

// For Usecode intrinsic.
int Arrest_avatar_schedule::get_actual_type(Actor* npc) const {
	ignore_unused_variable_warning(npc);
	return combat;
}

/*
 *  Create a loiter schedule.
 */

Loiter_schedule::Loiter_schedule(
		Actor* n,
		int    d    // Distance in tiles to roam.
		)
		: Schedule(n), center(n->get_tile()), dist(d) {}

/*
 *  Schedule change for 'loiter':
 */

void Loiter_schedule::now_what() {
	if (rand() % 3 == 0) {    // Check for lamps, etc.
		if (try_street_maintenance()) {
			return;    // We no longer exist.
		}
	}
	// Pure guess. Since only some schedules seem to call proximity
	// usecode, I guess I should put it in here.
	// Seems rare for pure loiter, quite frequent for tend shop.
	if ((npc->get_schedule_type() == Schedule::loiter
		 && !(GAME_SI && npc->get_shapenum() == 0x355)
		 && try_proximity_usecode(12))
		|| (npc->get_schedule_type() == Schedule::tend_shop
			&& try_proximity_usecode(8))) {
		return;
	}
	const int newx = center.tx - dist + rand() % (2 * dist);
	const int newy = center.ty - dist + rand() % (2 * dist);
	// Wait a bit.
	npc->walk_to_tile(
			newx, newy, center.tz, 2 * gwin->get_std_delay(), rand() % 2000);
}

/*
 *  Schedule change for graze.
 */

void Graze_schedule::now_what() {
	const int delay = 2 * gwin->get_std_delay();
	switch (phase) {
	case 0: {
		const int newx = center.tx - dist + rand() % (2 * dist);
		const int newy = center.ty - dist + rand() % (2 * dist);
		// Wait a bit.
		npc->walk_to_tile(
				newx, newy, center.tz, 2 * gwin->get_std_delay(),
				rand() % 2000);
		phase++;
		break;
	}
	case 1:
	case 2:
		if (npc->get_shapenum() == 0x1fd) {    // Fish?
			if (rand() % 12 == 0) {
				const int        dir = npc->get_dir_facing();
				const std::array frames{
						static_cast<signed char>(
								npc->get_dir_framenum(dir, 0xe)),
						static_cast<signed char>(
								npc->get_dir_framenum(dir, 0xf)),
						static_cast<signed char>(
								npc->get_dir_framenum(dir, Actor::standing)),
				};
				npc->set_action(
						new Frames_actor_action(frames.data(), frames.size()));
			}
		} else {
			npc->add_dirty();
			npc->change_frame(npc->get_dir_framenum(
					npc->get_dir_facing(), Actor::standing));
			npc->add_dirty();
		}
		phase++;
		break;
	case 3:
		if (npc->get_shapenum() == 0x1fd) {    // Fish?
			phase = 0;
		} else {
			phase++;
		}
		break;
	default: {
		// Non-fish only.
		signed char frames[12];
		const int   dir = npc->get_dir_facing();
		int         cnt = 0;
		int         max = 2 + rand() % 4;
		for (; cnt < max; cnt++) {
			frames[cnt] = npc->get_dir_framenum(dir, 0xe);
		}
		max += 2 + rand() % 4;
		for (; cnt < max; cnt++) {
			frames[cnt] = npc->get_dir_framenum(dir, 0xf);
		}
		npc->set_action(new Frames_actor_action(frames, cnt));
		if (4 + rand() % 4 < phase) {
			phase = 0;
		} else {
			phase++;
		}
		break;
	}
	}
	npc->start(delay, delay);
}

/*
 *  Schedule change for kid games.
 */

void Kid_games_schedule::now_what() {
	const Tile_coord pos = npc->get_tile();
	Actor*           kid = nullptr;    // Get a kid to chase.
	// But don't run too far.
	while (!kids.empty()) {
		kid = kids.back();
		kids.pop_back();
		if (npc->distance(kid) < 16) {
			break;
		}
		kid = nullptr;
	}

	if (kid) {
		Fast_pathfinder_client cost(npc, kid->get_tile(), 1);
		Actor_action*          pact = Path_walking_actor_action::create_path(
                pos, kid->get_tile(), cost);
		if (pact) {
			npc->set_action(pact);
			npc->start(100, 250);    // Run.
			return;
		}
	} else {    // No more kids?  Search.
		Actor_vector vec;
		npc->find_nearby_actors(vec, c_any_shapenum, 16);
		for (auto* act : vec) {
			if (act->get_schedule_type() == kid_games) {
				kids.push_back(act);
			}
		}
	}
	Loiter_schedule::now_what();    // Wander around the start.
}

/*
 *  Schedule change for 'dance':
 */

void Dance_schedule::now_what() {
	Tile_coord dest = center;    // Pick new spot to walk to.
	dest.tx += -dist + rand() % (2 * dist);
	dest.ty += -dist + rand() % (2 * dist);
	const Tile_coord cur = npc->get_tile();
	const int        dir = static_cast<int>(
            Get_direction4(cur.ty - dest.ty, dest.tx - cur.tx));
	// Pure guess. Since only some schedules seem to call proximity
	// usecode, I guess I should put it in here.
	// Seems quite rare.
	if (try_proximity_usecode(8)) {
		return;
	}
	signed char* frames;
	size_t       nframes;
	const int    danceroutine = rand() % 5;
	const int    speed        = 2 * gwin->get_std_delay();
	switch (danceroutine) {
	default: {
		// Spin in place using one of several frames.
		constexpr static const std::array base_frames{
				Actor::standing, Actor::up_frame, Actor::out_frame};
		const signed char basefr = base_frames[danceroutine];
		nframes                  = 6;
		frames                   = new signed char[nframes];
		size_t i;
		for (i = 0; i < nframes - 2; i++) {
			frames[i] = npc->get_dir_framenum((dir + 2 * i) % 8, basefr);
		}
		frames[i] = npc->get_dir_framenum((dir + 2 * i) % 8, basefr);
		frames[i + 1]
				= npc->get_dir_framenum((dir + 2 * i) % 8, Actor::standing);
		break;
	}
	case 3: {    // Flap arms
		// Seems to repeat a random number of times.
		constexpr static const std::array flap_frames{
				Actor::up_frame, Actor::out_frame};
		nframes = 5 + 4 * (rand() % 4);
		frames  = new signed char[nframes];
		for (size_t i = 0; i < nframes - 1; i++) {
			frames[i] = npc->get_dir_framenum(dir, flap_frames[i % 2]);
		}
		frames[nframes - 1] = npc->get_dir_framenum(dir, Actor::standing);
		break;
	}
	case 4: {    // Punch
		constexpr static const std::array swing_frames{
				Actor::ready_frame, Actor::raise1_frame, Actor::reach1_frame,
				Actor::strike1_frame, Actor::ready_frame};
		nframes = swing_frames.size();
		frames  = new signed char[nframes];
		for (size_t i = 0; i < swing_frames.size(); i++) {
			frames[i] = npc->get_dir_framenum(dir, swing_frames[i]);
		}
		break;
	}
	}
	// Create action to walk.
	Actor_action* walk = new Path_walking_actor_action(new Zombie());
	walk->walk_to_tile(npc, cur, dest);
	// Walk, then spin.
	npc->set_action(new Sequence_actor_action(
			walk, new Frames_actor_action(frames, nframes, speed)));
	delete[] frames;
	npc->start(gwin->get_std_delay(), 500);    // Start in 1/2 sec.
}

void Tool_schedule::get_tool() {
	const Game_object_shared newobj
			= std::make_shared<Ireg_game_object>(toolshape, 0, 0, 0, 0);
	Game_object* tool_obj = newobj.get();
	tool                  = newobj;
	// Free up both hands.
	Game_object* obj = npc->get_readied(rhand);
	if (obj) {
		obj->remove_this();
	}
	if ((obj = npc->get_readied(lhand)) != nullptr) {
		obj->remove_this();
	}
	npc->add_readied(tool_obj, lhand);
}

/*
 *  End of mining/farming:
 */

void Tool_schedule::ending(int newtype) {
	ignore_unused_variable_warning(newtype);
	const Game_object_shared tool_obj = tool.lock();
	if (tool_obj) {
		tool_obj->remove_this();    // Should safely remove from NPC.
	}
}

constexpr static const std::array cropshapes{423};

static void Grow_crops(Actor* npc, int frame_group0) {
	Game_object_vector crops;
	npc->find_closest(crops, cropshapes);
	// Grow the farther ones.
	const int cnt = crops.size();
	for (auto it = crops.begin() + cnt / 2; it != crops.end(); ++it) {
		if (rand() % 4) {
			continue;
		}
		Game_object* c      = *it;
		const int    frnum  = c->get_framenum();
		const int    growth = ((frnum & 3) + 1) & 3;
		if ((frnum & ~3) == frame_group0 && growth != 3) {
			c->change_frame((frnum & ~3) | growth);
		}
	}
}

/*
 *  Schedule change for tool-user (currently only farming).
 */

void Farmer_schedule::now_what() {
	int                      delay    = 0;
	const Game_object_shared tool_obj = tool.lock();
	if (!tool_obj) {    // First time?
		get_tool();
	}
	switch (state) {
	case start:
		state = find_crop;
		[[fallthrough]];
	case find_crop: {
		Game_object_vector crops;
		crop                  = Game_object_weak();
		Game_object* crop_obj = nullptr;
		if (npc->can_speak() && rand() % 5 == 0) {
			npc->say(first_farmer2, last_farmer2);
		}
		npc->find_closest(crops, cropshapes);
		// Filter out frame #3 (already cut).
		for (auto* crop : crops) {
			if (crop->get_framenum() % 4 != 3) {
				crop_obj = crop;
				if (rand() % 3 == 0) {    // A little randomness.
					break;
				}
			}
		}
		crop = weak_from_obj(crop_obj);
		if (crop_obj) {
			Actor_pathfinder_client cost(npc, 2);
			Actor_action* pact = Path_walking_actor_action::create_path(
					npc->get_tile(), crop_obj->get_tile(), cost);
			if (pact) {
				state = attack_crop;
				npc->set_action(new Sequence_actor_action(
						pact, new Face_pos_actor_action(crop_obj, 200)));
				break;
			} else {
				crop = Game_object_weak();
			}
		}
		state = wander;
		delay = 1000 + rand() % 1000;    // Try again later.
		break;
	}
	case attack_crop: {
		const Game_object_shared crop_obj = crop.lock();
		if (!crop_obj || crop_obj->is_pos_invalid()
			|| npc->distance(crop_obj.get()) > 2) {
			state = find_crop;
			break;
		}
		signed char frames[20];    // Use tool.
		const int   dir = npc->get_direction(crop_obj.get());
		int         cnt = npc->get_attack_frames(toolshape, false, dir, frames);
		if (cnt) {
			frame_group0  = crop_obj.get()->get_framenum() & ~3;
			frames[cnt++] = npc->get_dir_framenum(dir, Actor::standing);
			npc->set_action(new Frames_actor_action(frames, cnt));
			state = crop_attacked;
		} else {
			state = wander;
		}
		break;
	}
	case crop_attacked: {
		const Game_object_shared crop_obj = crop.lock();
		if (!crop_obj || crop_obj->is_pos_invalid()) {
			state = find_crop;
			break;
		}
		if (npc->can_speak() && rand() % 8 == 0) {
			npc->say(first_farmer, last_farmer);
		}
		if (rand() % 3 == 0) {    // Cut down crop.
			const int frnum = crop_obj->get_framenum();
			// Frame 3 in each group of 4 is 'cut'.
			crop_obj->change_frame(frnum | 3);
			crop = Game_object_weak();
			// Wander now and then.
			state = (rand() % 4 == 0) ? wander : find_crop;
			if (frame_group0 >= 0 && grow_cnt < 4 && rand() % 2) {
				Grow_crops(npc, frame_group0);
				++grow_cnt;
			}
			delay = 500 + rand() % 2000;
		} else {
			state = attack_crop;
			delay = 250;
		}
		break;
	}
	case wander:
		Loiter_schedule::now_what();
		if (rand() % 2 == 0) {
			state = find_crop;
		}
		return;
	}
	npc->start(gwin->get_std_delay(), delay);
}

/*
 *  Miners attack ore.
 */

void Miner_schedule::now_what() {
	int                      delay    = 0;
	const Game_object_shared tool_obj = tool.lock();
	if (!tool_obj) {    // First time?
		get_tool();
	}
	switch (state) {
	case find_ore: {
		constexpr static const std::array oreshapes{915, 916};
		Game_object_vector                ores;
		npc->find_closest(ores, oreshapes);
		size_t to;
		size_t cnt = ores.size();
		// Filter out frame #3 (dust).
		for (size_t from = to = 0; from < cnt; ++from) {
			if (ores[from]->get_framenum() < 3) {
				ores[to++] = ores[from];
			}
		}
		cnt = to;
		if (cnt) {
			Game_object* ore_obj = ores[rand() % cnt];
			ore                  = weak_from_obj(ore_obj);
			Actor_pathfinder_client cost(npc, 2);
			Actor_action* pact = Path_walking_actor_action::create_path(
					npc->get_tile(), ore_obj->get_tile(), cost);
			if (pact) {
				state = attack_ore;
				npc->set_action(new Sequence_actor_action(
						pact, new Face_pos_actor_action(ore_obj, 200)));
				break;
			}
		}
		ore   = Game_object_weak();
		state = wander;
		delay = 1000 + rand() % 1000;    // Try again later.
		break;
	}
	case attack_ore: {
		const Game_object_shared ore_obj = ore.lock();
		if (!ore_obj || ore_obj->is_pos_invalid()
			|| npc->distance(ore_obj.get()) > 2) {
			state = find_ore;
			break;
		}
		signed char frames[20];    // Use pick.
		const int   dir = npc->get_direction(ore_obj.get());
		int         cnt = npc->get_attack_frames(toolshape, false, dir, frames);
		if (cnt) {
			frames[cnt++] = npc->get_dir_framenum(dir, Actor::standing);
			npc->set_action(new Frames_actor_action(frames, cnt));
			state = ore_attacked;
		} else {
			state = wander;
		}
		break;
	}
	case ore_attacked: {
		const Game_object_shared ore_obj = ore.lock();
		if (!ore_obj || ore_obj->is_pos_invalid()) {
			state = find_ore;
			break;
		}
		state = attack_ore;
		if (rand() % 6 == 0) {    // Break up piece.
			int frnum = ore_obj->get_framenum();
			if (frnum == 3) {
				state = find_ore;    // Dust.
			} else if (rand() % (4 + 2 * frnum) == 0) {
				if (npc->can_speak()) {
					npc->say(first_miner_gold, last_miner_gold);
				}
				const Tile_coord pos = ore_obj->get_tile();
				ore_obj->remove_this();
				int shnum;
				if (frnum == 0) {    // Gold.
					shnum = 645;
					frnum = rand() % 2;
				} else {    // Gem.
					shnum = 760;
					frnum = rand() % 10;
				}
				const Game_object_shared newobj
						= std::make_shared<Ireg_game_object>(
								shnum, frnum, 0, 0);
				newobj->move(pos);
				newobj->set_flag(Obj_flags::is_temporary);
				state = find_ore;
				break;
			} else {
				ore_obj->change_frame(frnum + 1);
				if (ore_obj->get_framenum() == 3) {
					state = find_ore;    // Dust.
				}
			}
		}
		if (npc->can_speak() && rand() % 4 == 0) {
			npc->say(first_miner, last_miner);
		}
		delay = 500 + rand() % 2000;
		break;
	}
	case wander:
		if (rand() % 2 == 0) {
			Loiter_schedule::now_what();
			return;
		} else {
			state = find_ore;
		}
		break;
	}
	npc->start(gwin->get_std_delay(), delay);
}

/*
 *  Schedule change for hounding the Avatar.
 */

void Hound_schedule::now_what() {
	Actor*           av     = gwin->get_main_actor();
	Tile_coord       avpos  = av->get_tile();
	const Tile_coord npcpos = npc->get_tile();
	// How far away is Avatar?
	const int dist = npc->distance(av);
	// Pure guess. Since only some schedules seem to call proximity
	// usecode, I guess I should put it in here.
	// Seems quite rare.
	if (try_proximity_usecode(12)) {
		return;
	}
	if (dist < 3) {    // Close enough?
		// Face avatar and check again soon.
		const int dir = npc->get_direction(av);
		npc->change_frame(npc->get_dir_framenum(dir, Actor::standing));
		npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		return;
	} else if (dist > 20) {    // Too far?
		// Check again in a few seconds.
		npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		return;
	}
	const int newdist = 1 + rand() % 2;    // Aim for about 3 tiles from Avatar.
	avpos.tx += rand() % 3 - 1;            // Vary a bit randomly.
	avpos.ty += rand() % 3 - 1;
	Fast_pathfinder_client cost(npc, avpos, newdist);
	Actor_action*          pact
			= Path_walking_actor_action::create_path(npcpos, avpos, cost);
	if (pact) {
		npc->set_action(pact);
		npc->start(gwin->get_std_delay(), 50);
	} else {    // Try again.
		npc->start(gwin->get_std_delay(), 2000 + rand() % 3000);
	}
}

/*
 *  Schedule change for 'wander':
 */

void Wander_schedule::now_what() {
	if (rand() % 2) {    // 1/2 time, check for lamps, etc.
		if (try_street_maintenance()) {
			return;    // We no longer exist.
		}
	}
	Tile_coord pos     = npc->get_tile();
	const int  legdist = 32;
	// Go a ways from current pos.
	pos.tx += -legdist + rand() % (2 * legdist);
	pos.ty += -legdist + rand() % (2 * legdist);
	// Don't go too far from center.
	if (pos.tx - center.tx > dist) {
		pos.tx = center.tx + dist;
	} else if (center.tx - pos.tx > dist) {
		pos.tx = center.tx - dist;
	}
	if (pos.ty - center.ty > dist) {
		pos.ty = center.ty + dist;
	} else if (center.ty - pos.ty > dist) {
		pos.ty = center.ty - dist;
	}
	// Find a free spot.
	const Tile_coord dest
			= Map_chunk::find_spot(pos, 4, npc->get_shapenum(), 0, 1);
	if (dest.tx == -1
		|| !npc->walk_path_to_tile(
				dest, gwin->get_std_delay(), rand() % 2000)) {
		// Failed?  Try again a little later.
		npc->start(250, rand() % 3000);
	}
}

/*
 *  Stand up if not already.
 */

static void Stand_up(Actor* npc) {
	if ((npc->get_framenum() & 0xf) != Actor::standing) {
		// Stand.
		npc->change_frame(Actor::standing);
	}
}

/*
 *  Create a sleep schedule.
 */

Sleep_schedule::Sleep_schedule(Actor* n)
		: Schedule(n), state(0), for_nap_time(false) {
	floorloc.tx = -1;    // Set to invalid loc.
	if (Game::get_game_type() == BLACK_GATE) {
		spread0 = 3;
		spread1 = 16;
	} else {    // Serpent Isle.
		spread0 = 7;
		spread1 = 20;
	}
}

/*
 * Checks if a bed is being used by a sleeping NPC. The second
 * parameter is a 'whitelist' NPC which will be ignored, and
 * should be used for preventing an NPC from detecting himself
 * as occupying the bed.
 */

bool Sleep_schedule::is_bed_occupied(Game_object* bed, Actor* npc) {
	// Check if someone is already in the bed.
	Actor_vector occ;    // Unless there's another occupant.
	bed->find_nearby_actors(occ, c_any_shapenum, 2);
	const TileRect foot = bed->get_footprint();
	const int      bedz = bed->get_tile().tz / 5;
	for (auto* it : occ) {
		if (npc == it) {
			continue;
		}
		const Tile_coord tn = it->get_tile();
		// Don't check frame -- prevents wounded man shape from being
		// considered as an occupant.
		if (tn.tz / 5 == bedz /*&& ((*it)->get_framenum()&0xf) ==
								 Actor::sleep_frame*/
			&& foot.has_world_point(tn.tx, tn.ty)) {
			return true;
		}
	}
	return false;
}

/*
 *  Schedule change for 'sleep':
 */

void Sleep_schedule::now_what() {
	Game_object_shared bed_obj = bed.lock();
	if (bed_obj && state <= 1 && is_bed_occupied(bed_obj.get(), npc)) {
		bed_obj = nullptr;
		bed     = Game_object_weak(bed_obj);
		state   = 0;
		npc->stop();
	}
	if (!bed_obj && state == 0 &&               // Always find bed.
		!npc->get_flag(Obj_flags::asleep)) {    // Unless flag already set
		// Find closest EW or NS bed.
		vector<int> bedshapes;
		bedshapes.push_back(696);
		bedshapes.push_back(1011);
		if (GAME_BG) {
			bedshapes.push_back(363);
			bedshapes.push_back(312);
		}
		Game_object_vector beds;    // Want to find top of bed.
		for (const int bedshape : bedshapes) {
			npc->find_nearby(beds, bedshape, 24, 0);
		}
		int best_dist = 100;
		for (auto* newbed : beds) {
			const int newdist = npc->distance(newbed);
			if (newdist < best_dist && !is_bed_occupied(newbed, npc)
				&& Fast_pathfinder_client::is_straight_path(npc, newbed)) {
				best_dist = newdist;
				bed_obj   = newbed->shared_from_this();
			}
		}
		bed = Game_object_weak(bed_obj);
	}
	if (!bed_obj && npc == gwin->get_main_actor()) {
		npc->start(gwin->get_std_delay());
		return;
	}
	const int frnum = npc->get_framenum();
	if ((frnum & 0xf) == Actor::sleep_frame) {
		return;    // Already sleeping.
	}

	switch (state) {
	case 0: {    // Find path to bed.
		if (!bed_obj) {
			static const int possible_dirs[] = { west, north };
			Tile_coord npc_pos = npc->get_tile();
			int sleep_dir_frame = npc->get_dir_framenum(west, Actor::sleep_frame);
			const int ztiles = npc->get_info().get_3d_height();

			for (int direction : possible_dirs) {
				int frame = npc->get_dir_framenum(direction, Actor::sleep_frame);
				bool blocked = false;
				// to check for enough visible room we have to use the height of
				// the frame but it is offset by 1 to the NPC position
				if (direction == west) {
					Tile_coord testTile1(npc_pos.tx + 1, npc_pos.ty, npc_pos.tz);
					Tile_coord testTile2(npc_pos.tx - (ztiles - 1), npc_pos.ty, npc_pos.tz);
					if (npc->is_blocked(testTile1, 0) || npc->is_blocked(testTile2, 0))
						blocked = true;
				} else if (direction == north) {
					Tile_coord testTile1(npc_pos.tx, npc_pos.ty + 1, npc_pos.tz);
					Tile_coord testTile2(npc_pos.tx, npc_pos.ty - ztiles, npc_pos.tz);
					if (npc->is_blocked(testTile1, 0) || npc->is_blocked(testTile2, 0))
						blocked = true;
				}
				if (!blocked) {
					sleep_dir_frame = frame;
					break;
				}
			}
			npc->change_frame(sleep_dir_frame);
			npc->force_sleep();
			return;
		}
		state = 1;
		Game_object_vector tops;    // Want to find top of bed.
		bed_obj->find_nearby(tops, bed_obj->get_shapenum(), 1, 0);
		const int floor = (bed_obj->get_tile().tz) / 5;
		for (auto* top : tops) {
			const int        frnum = top->get_framenum();
			const Tile_coord tpos  = top->get_tile();
			// Restrict to sheets on the same floor as the selected bed.
			if (frnum >= spread0 && frnum <= spread1
				&& floor == (tpos.tz / 5)) {
				bed_obj = top->shared_from_this();
				bed     = Game_object_weak(bed_obj);
				break;
			}
		}
		Tile_coord bloc = bed_obj->get_tile();
		bloc.tz -= bloc.tz % 5;    // Round down to floor level.
		const Shape_info& info = bed_obj->get_info();
		bloc.tx -= info.get_3d_xtiles(bed_obj->get_framenum()) / 2;
		bloc.ty -= info.get_3d_ytiles(bed_obj->get_framenum()) / 2;
		// Get within 3 tiles.
		Actor_pathfinder_client cost(npc, 3);
		Actor_action*           pact = Path_walking_actor_action::create_path(
                npc->get_tile(), bloc, cost);
		if (pact) {
			npc->set_action(pact);
		}
		npc->start(200);    // Start walking.
		break;
	}
	case 1: {           // Go to bed.
		npc->stop();    // Just to be sure.
		if (npc->distance(bed_obj.get()) > 3) {
			state = 0;
			npc->start(200);    // Try again.
		}
		const int bedshape = bed_obj->get_shapenum();
		const int dir = (bedshape == 696 || bedshape == 363) ? west : north;
		npc->change_frame(npc->get_dir_framenum(dir, Actor::sleep_frame));
		// Get bed info.
		const Shape_info& info   = bed_obj->get_info();
		const Tile_coord  bedloc = bed_obj->get_tile();
		floorloc                 = npc->get_tile();
		Usecode_script::terminate(bed_obj.get());
		int bedframe = bed_obj->get_framenum();    // Unmake bed.
		if (bedframe >= spread0 && bedframe < spread1 && (bedframe % 2)) {
			bedframe++;
			bed_obj->change_frame(bedframe);
		}
		const bool bedspread = (bedframe >= spread0 && !(bedframe % 2));
		// Put NPC on top of bed, making sure that children are
		// not completely covered by sheets.
		int delta = npc->get_info().get_3d_height();
		delta     = (delta < 4) ? (delta - 4) : 0;
		npc->move(
				bedloc.tx + delta, bedloc.ty + delta,
				bedloc.tz + (bedspread ? 0 : info.get_3d_height()));
		// Either call bed usecode for avatar or put NPC to sleep.
		npc->force_sleep();
		state = 2;
		if (for_nap_time) {    // Usecode 622 handles sleeping.
			// Calling it may delete us, though.
			ucmachine->call_usecode(
					SleepUsecode, bed_obj.get(), Usecode_machine::double_click);
			return;    // So leave nothing to chance.
		}
		break;
	}
	default:
		break;
	}
}

/*
 *  Just went dormant.
 */
void Sleep_schedule::im_dormant() {
	// Force NPCs to lay down again after cache-out/cache-in.
	// npc->change_frame(npc->get_dir_framenum(Actor::standing));
	npc->set_action(nullptr);
}

/*
 *  Wakeup time.
 */

void Sleep_schedule::ending(int new_type    // New schedule.
) {
	// Needed for Skara Brae.
	if (new_type == static_cast<int>(wait) ||
		// Not time to get up, Penumbra!
		new_type == static_cast<int>(sleep)) {
		return;    // ++++Does this leave NPC's stuck?++++
	}

	bool                     makebed = false;
	int                      dir     = 0;
	const Game_object_shared bed_obj = bed.lock();
	// Still in bed?
	if (bed_obj && (npc->get_framenum() & 0xf) == Actor::sleep_frame
		&& npc->distance(bed_obj.get()) < 8) {
		// Locate free spot.
		if (floorloc.tx == -1) {
			// Want spot on floor.
			floorloc = npc->get_tile();
			// Note: doing this in all cases may cause incorrect
			// placement when getting out of bed (e.g., top floor
			// bed in Spark's house.
			floorloc.tz -= floorloc.tz % 5;
		}
		Tile_coord pos = Map_chunk::find_spot(
				floorloc, 6, npc->get_shapenum(),
				static_cast<int>(Actor::standing), 0);
		if (pos.tx == -1) {    // Failed?  Allow change in lift.
			pos = Map_chunk::find_spot(
					floorloc, 6, npc->get_shapenum(),
					static_cast<int>(Actor::standing), 1);
		}
		floorloc = pos;
		// Make bed.
		const int frnum = bed_obj->get_framenum();
		// Not if going into combat, or was awakened
		if (!sleep_interrupted && new_type != static_cast<int>(combat)
			&& frnum >= spread0 && frnum <= spread1 && !(frnum % 2)
			&& !is_bed_occupied(
					bed_obj.get(),
					npc)) {    // And not if there is another occupant.
			// Make the bed set itself after a small delay.
			makebed   = true;
			auto* scr = new Usecode_script(bed_obj.get());
			(*scr) << Ucscript::finish << Ucscript::delay_ticks << 3
				   << Ucscript::frame << frnum - 1;
			scr->start();
			const Tile_coord bloc = bed_obj->get_center_tile();
			// Treat as cartesian coords.
			dir = static_cast<int>(Get_direction(
					floorloc.ty - bloc.ty, bloc.tx - floorloc.tx));
		}
	}
	if (floorloc.tx >= 0) {    // Get back on floor.
		npc->move(floorloc);
	}
	npc->clear_sleep();
	npc->change_frame(npc->get_dir_framenum(Actor::standing));
	if (makebed && !sleep_interrupted) {
		// Animation for making bed.
		auto* scr = new Usecode_script(npc);
		(*scr) << Ucscript::dont_halt << Ucscript::face_dir << dir
			   << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 1
			   << Ucscript::npc_raise1_frame << Ucscript::delay_ticks << 1
			   << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 1
			   << Ucscript::npc_standing_frame;
		scr->start();
	}
	gwin->set_all_dirty();    // Update all, since Av. stands up.
	if (sleep_interrupted) {
		sleep_interrupted = false;
	}
	state = 0;    // In case we go back to sleep.
}

void Sleep_schedule::set_bed(Game_object* b) {
	npc->set_action(nullptr);
	bed          = weak_from_obj(b);
	state        = 0;
	for_nap_time = true;
}

/*
 *  Create a 'sit' schedule.
 */

Sit_schedule::Sit_schedule(
		Actor*       n,
		Game_object* ch    // Chair, or null to find one.
		)
		: Schedule(n), chair(weak_from_obj(ch)), sat(false),
		  did_barge_usecode(false) {}

/*
 *  Schedule change for 'sit':
 */

void Sit_schedule::now_what() {
	const int                frnum     = npc->get_framenum();
	const Game_object_shared chair_obj = chair.lock();
	if (chair_obj && (frnum & 0xf) == Actor::sit_frame
		&& npc->distance(chair_obj.get()) <= 1) {
		// Already sitting.
		// Seat on barge?
		if (chair_obj->get_info().get_barge_type() != Shape_info::barge_seat) {
			return;
		}
		if (did_barge_usecode) {
			return;    // But NOT more than once for party.
		}
		did_barge_usecode = true;
		if (gwin->get_moving_barge()) {
			return;    // Already moving.
		}
		if (!npc->is_in_party()) {
			return;    // Not a party member.
		}
		Actor*    party[9];    // See if all sitting.
		const int cnt = gwin->get_party(&party[0], 1);
		for (int i = 0; i < cnt; i++) {
			if ((party[i]->get_framenum() & 0xf) != Actor::sit_frame) {
				return;    // Nope.
			}
		}
		// Find barge.
		Game_object* barge = chair_obj->find_closest(961);
		if (!barge) {
			return;
		}
		// Special usecode for barge pieces:
		// (Call with item=Avatar to avoid
		//   running nearby barges.)
		ucmachine->call_usecode(
				BargeUsecode, gwin->get_main_actor(),
				Usecode_machine::double_click);
		return;
	}
	// Wait a while if we got up.
	Game_object* new_chair = chair_obj.get();
	if (!set_action(
				npc, new_chair, sat ? (1000 + rand() % 1000) : 0, &new_chair)) {
		npc->start(200, 1000);    // Failed?  Try again later.
	} else {
		sat = true;
	}
	chair = weak_from_obj(new_chair);
}

/*
 *  Just went dormant.
 */
void Sit_schedule::im_dormant() {
	// Force NPCs to sit down again after cache-out/cache-in.
	// npc->change_frame(npc->get_dir_framenum(Actor::standing));
	npc->set_action(nullptr);
}

/*
 *  Action to sit in the chair NPC is in front of.
 */

class Sit_actor_action : public Frames_actor_action, public Game_singletons {
	Game_object_weak chair;       // Chair.
	Tile_coord       chairloc;    // Original chair location.
	Tile_coord       sitloc;      // Actually where NPC sits.
	signed char      frames[2];
	static short     offsets[8];    // Offsets where NPC should sit.

	signed char* init(Game_object* chairobj, Actor* actor) {
		// Frame 0 faces N, 1 E, etc.
		const int dir = 2 * (chairobj->get_framenum() % 4);
		frames[0]     = actor->get_dir_framenum(dir, Actor::bow_frame);
		frames[1]     = actor->get_dir_framenum(dir, Actor::sit_frame);
		return frames;
	}

	static bool is_occupied(const Tile_coord& sitloc, Actor* actor) {
		Game_object_vector occ;    // See if occupied.
		if (Game_object::find_nearby(occ, sitloc, c_any_shapenum, 0, 8)) {
			for (auto* npc : occ) {
				if (npc == actor) {
					continue;
				}
				const int frnum = npc->get_framenum() & 15;
				if (frnum == Actor::sit_frame || frnum == Actor::bow_frame) {
					return true;
				}
			}
		}
		if (actor->get_tile() == sitloc) {
			return false;    // We're standing there.
		}
		// See if spot is blocked.
		Tile_coord pos = sitloc;    // Careful, .tz gets updated.
		return Map_chunk::is_blocked(
				pos, actor->get_info().get_3d_height(), MOVE_WALK, 0);
	}

public:
	Sit_actor_action(Game_object* o, Actor* actor)
			: Frames_actor_action(init(o, actor), 2), chair(weak_from_obj(o)) {
		sitloc = chairloc = o->get_tile();
		// Frame 0 faces N, 1 E, etc.
		const int nsew = o->get_framenum() % 4;
		sitloc.tx += offsets[2 * nsew];
		sitloc.ty += offsets[2 * nsew + 1];
	}

	Tile_coord get_sitloc() const {
		return sitloc;
	}

	static bool is_occupied(Game_object* chair, Actor* actor) {
		const int dir = 2 * (chair->get_framenum() % 4);
		return is_occupied(
				chair->get_tile()
						+ Tile_coord(offsets[dir], offsets[dir + 1], 0),
				actor);
	}

	// Handle time event.
	int handle_event(Actor* actor) override;
};

// Offsets where NPC should sit:
short Sit_actor_action::offsets[8] = {0, -1, 1, 0, 0, 1, -1, 0};

/*
 *  Show frame for sitting down.
 */

int Sit_actor_action::handle_event(Actor* actor) {
	if (get_index() == 0) {    // First time?
		if (is_occupied(sitloc, actor)) {
			return 0;    // Abort.
		}
		const Game_object_shared chair_obj = chair.lock();
		if (!chair_obj || chair_obj->get_tile() != chairloc) {
			// Chair was moved!
			actor->say(first_chair_thief, last_chair_thief);
			return 0;
		}
	}
	return Frames_actor_action::handle_event(actor);
}

/*
 *  Is chair occupied, other than by 'actor'?
 */

bool Sit_schedule::is_occupied(Game_object* chairobj, Actor* actor) {
	return Sit_actor_action::is_occupied(chairobj, actor);
}

/*
 *  Set up action.
 *
 *  Output: false if couldn't find a free chair.
 */

bool Sit_schedule::set_action(
		Actor*        actor,
		Game_object*  chairobj,      // May be 0 to find it.
		int           delay,         // Msecs. to delay.
		Game_object** chair_found    // ->chair ret'd if not nullptr.
) {
	Game_object_vector chairs;
	if (chair_found) {
		*chair_found = nullptr;    // Init. in case we fail.
	}
	if (!chairobj) {    // Find chair if not given.
		constexpr static const std::array chairshapes{873, 292};
		actor->find_closest(chairs, chairshapes);
		for (auto& chair : chairs) {
			if (!Sit_actor_action::is_occupied(chair, actor)) {
				Path_walking_actor_action action(nullptr, 6);
				const bool                reachable = action.walk_to_tile(
                                               actor, actor->get_tile(),
                                               chair->get_tile(), 1)
									   != nullptr;
				// Found an unused one.
				if (reachable) {
					chairobj = chair;
					break;
				}
			}
		}
		if (!chairobj) {
			return false;
		}
	} else if (Sit_actor_action::is_occupied(chairobj, actor)) {
		return false;    // Given chair is occupied.
	}
	auto* act = new Sit_actor_action(chairobj, actor);
	// Walk there, then sit.
	set_action_sequence(actor, act->get_sitloc(), act, false, delay);
	if (chair_found) {
		*chair_found = chairobj;
	}
	return true;
}

/*
 *  Desk work.
 */

Desk_schedule::Desk_schedule(Actor* n)
		: Schedule_with_objects(n), state(desk_setup) {}

/*
 *  Find tables.
 */
void Desk_schedule::find_tables(int shapenum) {
	Game_object_vector vec;
	npc->find_nearby(vec, shapenum, 16, 0);
	const int floor = npc->get_lift() / 5;    // Make sure it's on same floor.
	for (auto* table : vec) {
		if (table->get_lift() / 5 != floor) {
			continue;
		}
		tables.push_back(table->weak_from_this());
	}
}

/*
 *  Walk to table.
 *  Output: false if failed.
 */
bool Desk_schedule::walk_to_table() {
	while (!tables.empty()) {
		const int ind                       = rand() % tables.size();
		table                               = tables[ind];
		const Game_object_shared table_keep = table.lock();
		if (!table_keep) {
			tables.erase(tables.begin() + ind);
			continue;
		}
		table = table_keep;
		// This will find a random spot around the table:
		const Tile_coord pos = Map_chunk::find_spot(
				table_keep->get_tile(), 1, npc->get_shapenum(),
				npc->get_framenum());

		if (pos.tx != -1
			&& npc->walk_path_to_tile(
					pos, gwin->get_std_delay(), 1000 + rand() % 1000)) {
			return true;
		}
	}
	table = Game_object_weak();
	return false;
}

int Desk_schedule::find_items(Game_object_vector& vec, int dist) {
	constexpr static const std::array desk_frames{0, 1, 2, 3,  4,  5,  6,
												  7, 8, 9, 12, 13, 14, 15};
	npc->find_nearby(vec, 675, dist, 0);
	const int floor = npc->get_lift() / 5;    // Make sure it's on same floor.
	vec.erase(
			std::remove_if(
					vec.begin(), vec.end(),
					[floor](Game_object* item) {
						return item->get_lift() / 5 != floor
							   || std::find(
										  desk_frames.cbegin(),
										  desk_frames.cend(),
										  item->get_framenum())
										  == desk_frames.cend();
					}),
			vec.end());
	return vec.size();
}

// Pick a 'reasonable' desk item frame to create.
static int Desk_item_frame() {
	static const int quills[]    = {1, 14, 15};
	static const int documents[] = {6, 13};
	static const int wax[]       = {7, 12};
	static int       non_docs    = 0;    // Make sure to create docs.

	const int n = non_docs >= 2 ? 6 : rand() % 10;
	++non_docs;
	switch (n) {
	case 0:
		return 0;    // Gavel.
	case 1:
		return quills[rand() % 3];
	case 2:
		return 2;    // Inkwell
	case 3:
		return 3;    // Quill holder
	case 4:
		return 4;    // Book mark.
	case 5:
		return 5;    // Letter opener.
	case 6:
		non_docs = 0;
		return documents[rand() % 2];
	case 7:
		return wax[rand() % 2];
	case 8:
		return 8;    // Seal.
	case 9:
		return 9;    // Blotter.
	default:
		return 0;
	}
}

/*
 *  Schedule change for 'desk work':
 */

void Desk_schedule::now_what() {
	if (rand() % 3 == 0) {    // Check for lamps, etc.
		if (try_street_maintenance()) {
			return;    // We no longer exist.
		}
	}

	switch (state) {
	case desk_setup: {
		static int desks[2] = {283, 407};
		Stand_up(npc);
		if (tables.empty()) {
			find_tables(890);
			find_tables(633);
			find_tables(1000);
			find_tables(283);    // Desks too.
			find_tables(407);
		}
		// Create desk items if needed.
		items_in_hand = npc->count_objects(675);
		Game_object_vector vec;
		const int          nearby = Desk_schedule::find_items(vec, 16);
		const int          nitems = (7 + rand() % 5) - items_in_hand - nearby;
		if (nitems > 0) {
			items_in_hand += nitems;
			for (int i = 0; i < nitems; ++i) {
				int                      frame = Desk_item_frame();
				const Game_object_shared item
						= std::make_shared<Ireg_game_object>(
								675, frame, 0, 0, 0);
				npc->add(item.get(), true);
				add_object(item.get());
			}
		}
		Game_object* found = npc->find_closest(desks, 2);
		if (found) {
			constexpr static const std::array chairs{873, 292};
			desk  = found->weak_from_this();
			found = found->find_closest(chairs);
			if (found) {
				chair = found->weak_from_this();
			}
		}
		if (chair.expired()) {    // Failed.
			// Try again in a few seconds.
			npc->start(200, 5000);
			return;
		}
		state = sit_at_desk;
	}
		[[fallthrough]];
	case sit_at_desk: {
		const int frnum = npc->get_framenum();
		// Not sitting?
		if ((frnum & 0xf) != Actor::sit_frame) {
			const Game_object_shared chair_keep = chair.lock();
			if (!chair_keep
				|| !Sit_schedule::set_action(npc, chair_keep.get(), 0)) {
				chair = Game_object_weak();    // Look for any nearby chair.
				state = desk_setup;
				npc->start(200, 5000);    // Failed?  Try again later.
			} else {
				npc->start(250, 0);
			}
		} else if (rand() % (1 + items_in_hand) && walk_to_table()) {
			state = work_at_table;
		} else if (rand() % 2 && walk_to_random_item()) {
			state = get_desk_item;
		} else {    // Stand up a second.
			const std::array frames{
					static_cast<signed char>(
							npc->get_dir_framenum(Actor::standing)),
					static_cast<signed char>(
							npc->get_dir_framenum(Actor::reach1_frame)),
					static_cast<signed char>(
							npc->get_dir_framenum(Actor::standing)),
					static_cast<signed char>(
							npc->get_dir_framenum(Actor::bow_frame)),
					static_cast<signed char>(
							npc->get_dir_framenum(Actor::sit_frame)),
			};
			npc->set_action(
					new Frames_actor_action(frames.data(), frames.size()));
			npc->start(250, 3000 + rand() % 2000);
		}
		break;
	}
	case get_desk_item: {
		Game_object* item = get_current_item();
		if (item && npc->distance(item) <= 3) {
			// Turn to face it.
			npc->change_frame(npc->get_dir_framenum(
					npc->get_direction(item), Actor::standing));
			npc->set_action(new Pickup_actor_action(item, 250));
			state = picked_up_item;
		} else {
			state = sit_at_desk;
		}
		npc->start(250, 500 + rand() % 1000);
		break;
	}
	case picked_up_item:
		set_current_item(nullptr);
		if (rand() % 2 && walk_to_table()) {
			state = work_at_table;
			break;
		} else {
			state = sit_at_desk;
		}
		npc->start(250, 1000 + rand() % 500);
		break;
	case work_at_table:
		// Turn to face it.
		const Game_object_shared tbl = table.lock();
		if (tbl) {
			npc->change_frame(npc->get_dir_framenum(
					npc->get_facing_direction(tbl.get()), Actor::standing));
		}
		if (!tbl || rand() % 3 == 0) {
			state = sit_at_desk;                     // Back to desk.
		} else if (items_in_hand && rand() % 6) {    // Put down an item.
			Game_object_vector items;
			items_in_hand
					= npc->get_objects(items, 675, c_any_qual, c_any_framenum);
			if (items_in_hand) {
				Game_object* to_drop = items[rand() % items_in_hand];
				if (drop_item(to_drop, tbl.get())) {
					if (rand() % 2) {
						state = sit_at_desk;
					}
				}
			} else {
				state = sit_at_desk;
			}
		} else {
			const int        dir = npc->get_facing_direction(tbl.get());
			const std::array frames{
					static_cast<signed char>(
							npc->get_dir_framenum(dir, Actor::standing)),
					static_cast<signed char>(npc->get_dir_framenum(
							dir, (rand() % 2) ? Actor::reach1_frame
											  : Actor::reach2_frame)),
					static_cast<signed char>(
							npc->get_dir_framenum(dir, Actor::standing)),
			};
			Actor_action* face = new Face_pos_actor_action(tbl.get(), 200);
			npc->set_action(new Sequence_actor_action(
					face,
					new Frames_actor_action(frames.data(), frames.size())));
		}
		npc->start(250, 1000 + rand() % 500);
		break;
	}
}

/*
 *  Just went dormant.
 */
void Desk_schedule::im_dormant() {
	// Force NPCs to sit down again after cache-out/cache-in.
	// npc->change_frame(npc->get_dir_framenum(Actor::standing));
	npc->set_action(nullptr);
}

/*
 *	Done.
 */
void Desk_schedule::ending(int new_type    // New schedule.
) {
	ignore_unused_variable_warning(new_type);
	cleanup();
}

/*
 *  A class for indexing the perimeter of a rectangle.
 */
class Perimeter {
	TileRect perim;    // Outside given rect.
	int      sz;       // # squares.
public:
	Perimeter(TileRect& r) : sz(2 * r.w + 2 * r.h + 4) {
		perim = r;
		perim.enlarge(1);
	}

	int size() {
		return sz;
	}

	// Get i'th tile.
	void get(int i, Tile_coord& ptile, Tile_coord& atile);
};

/*
 *  Get the i'th perimeter tile and the tile in the original rect.
 *  that's adjacent.
 */

void Perimeter::get(
		int         i,
		Tile_coord& ptile,    // Perim. tile returned.
		Tile_coord& atile     // Adjacent tile returned.
) {
	if (i < perim.w - 1) {    // Spiral around from top-left.
		ptile = Tile_coord(perim.x + i, perim.y, 0);
		atile = ptile + Tile_coord(!i ? 1 : 0, 1, 0);
		return;
	}
	i -= perim.w - 1;
	if (i < perim.h - 1) {
		ptile = Tile_coord(perim.x + perim.w - 1, perim.y + i, 0);
		atile = ptile + Tile_coord(-1, !i ? 1 : 0, 0);
		return;
	}
	i -= perim.h - 1;
	if (i < perim.w - 1) {
		ptile = Tile_coord(perim.x + perim.w - 1 - i, perim.y + perim.h - 1, 0);
		atile = ptile + Tile_coord(!i ? -1 : 0, -1, 0);
		return;
	}
	i -= perim.w - 1;
	if (i < perim.h - 1) {
		ptile = Tile_coord(perim.x, perim.y + perim.h - 1 - i, 0);
		atile = ptile + Tile_coord(1, !i ? -1 : 0, 0);
		return;
	}
	// Bad index if here.
	get(i % sz, ptile, atile);
}

/*
 *  Initialize.
 */

void Lab_schedule::init() {
	Game_object* book_obj  = nullptr;
	Game_object* chair_obj = nullptr;
	cauldron               = weak_from_obj(npc->find_closest(995, 20));
	// Find 'lab' tables.
	vector<Game_object*> table_objs;
	npc->find_nearby(table_objs, 1003, 20, 0);
	npc->find_nearby(table_objs, 1018, 20, 0);
	const int cnt = table_objs.size();    // Look for book, chair.
	tables.resize(cnt);
	for (int i = 0; (!book_obj || !chair_obj) && i < cnt; i++) {
		Game_object* table  = table_objs[i];
		tables[i]           = weak_from_obj(table);
		const TileRect foot = table->get_footprint();
		// Book on table?
		if (!book_obj && (book_obj = table->find_closest(642, 4)) != nullptr) {
			const Tile_coord p = book_obj->get_tile();
			if (!foot.has_world_point(p.tx, p.ty)) {
				book_obj = nullptr;
			}
		}
		if (!chair_obj) {
			constexpr static const std::array chairs{873, 292};
			chair_obj = table->find_closest(chairs, 4);
		}
	}
	book  = weak_from_obj(book_obj);
	chair = weak_from_obj(chair_obj);
}

/*
 *  Create lab schedule.
 */

Lab_schedule::Lab_schedule(Actor* n) : Schedule(n), state(start) {
	init();
}

/*
 *  Lab work.
 */

void Lab_schedule::now_what() {
	const Tile_coord         npcpos       = npc->get_tile();
	int                      delay        = 100;    // 1/10 sec. to next action.
	const Game_object_shared cauldron_obj = cauldron.lock();
	const Game_object_shared book_obj     = book.lock();
	// Often want to get within 1 tile.
	Actor_pathfinder_client cost(npc, 1);
	switch (state) {
	case start:
	default: {
		if (!cauldron_obj) {
			// Try looking again.
			init();
			if (!cauldron_obj) {    // Again a little later.
				delay = 6000;
			}
			break;
		}
		const int r = rand() % 5;    // Pick a state.
		if (!r) {                    // Sit less often.
			state = sit_down;
		} else {
			state = r <= 2 ? walk_to_cauldron : walk_to_table;
		}
		break;
	}
	case walk_to_cauldron: {
		state = start;    // In case we fail.
		if (!cauldron_obj) {
			break;
		}
		Actor_action* pact = Path_walking_actor_action::create_path(
				npcpos, cauldron_obj->get_tile(), cost);
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Face_pos_actor_action(cauldron_obj.get(), 200)));
			state = use_cauldron;
		}
		break;
	}
	case use_cauldron: {
		const int dir = npc->get_direction(cauldron_obj.get());
		// Set random frame (skip last frame).
		cauldron_obj->change_frame(
				rand() % (cauldron_obj->get_num_frames() - 1));
		npc->change_frame(npc->get_dir_framenum(dir, Actor::bow_frame));
		const int r = rand() % 5;
		state       = !r ? use_cauldron : (r <= 2 ? sit_down : walk_to_table);
		break;
	}
	case sit_down: {
		const Game_object_shared chair_obj = chair.lock();
		if (!chair_obj
			|| !Sit_schedule::set_action(npc, chair_obj.get(), 200)) {
			state = start;
		} else {
			state = read_book;
		}
		break;
	}
	case read_book: {
		state = stand_up;
		if (!book_obj || npc->distance(book_obj.get()) > 4) {
			break;
		}
		// Read a little while.
		delay = 1000 + 1000 * (rand() % 5);
		// Open book.
		const int frnum = book_obj->get_framenum();
		book_obj->change_frame(frnum - frnum % 3);
		break;
	}
	case stand_up: {
		if (book_obj && npc->distance(book_obj.get()) < 4) {
			// Close book.
			const int frnum = book_obj->get_framenum();
			book_obj->change_frame(frnum - frnum % 3 + 1);
		}
		state = start;
		break;
	}
	case walk_to_table: {
		state             = start;    // In case we fail.
		const int ntables = tables.size();
		if (!ntables) {
			break;
		}
		const int                tableindex = rand() % ntables;
		const Game_object_shared table      = (tables[tableindex]).lock();
		if (!table) {
			break;
		}
		TileRect   r = table->get_footprint();
		Perimeter  p(r);    // Find spot adjacent to table.
		Tile_coord spot;    // Also get closest spot on table.
		p.get(rand() % p.size(), spot, spot_on_table);
		Actor_pathfinder_client cost0(npc, 0);
		Actor_action*           pact
				= Path_walking_actor_action::create_path(npcpos, spot, cost0);
		if (!pact) {
			break;    // Failed.
		}
		const Shape_info& info = table->get_info();
		spot_on_table.tz += info.get_3d_height();
		npc->set_action(new Sequence_actor_action(
				pact, new Face_pos_actor_action(spot_on_table, 200)));
		state = use_potion;
		break;
	}
	case use_potion: {
		state = start;
		vector<Game_object*> potions;
		Game_object::find_nearby(potions, spot_on_table, 340, 0, 0);
		if (!potions.empty()) {    // Found a potion.  Remove it.
			gwin->add_dirty(potions[0]);
			potions[0]->remove_this();
		} else {    // Create potion if spot is empty.
			const Tile_coord t
					= Map_chunk::find_spot(spot_on_table, 0, 340, 0, 0);
			if (t.tx != -1 && t.tz == spot_on_table.tz) {
				// create a potion randomly, but don't use the last frame
				const int nframes = ShapeID(340, 0).get_num_frames() - 1;
				const Game_object_shared p = gmap->create_ireg_object(
						ShapeID::get_info(340), 340, rand() % nframes, 0, 0, 0);
				p->move(t);
			}
		}
		signed char frames[2];
		const int   dir = npc->get_direction(spot_on_table);
		// Reach out hand:
		frames[0] = npc->get_dir_framenum(dir, 1);
		frames[1] = npc->get_dir_framenum(dir, Actor::standing);
		npc->set_action(new Frames_actor_action(frames, 2));
		break;
	}
	}
	npc->start(gwin->get_std_delay(), delay);    // Back in queue.
}

/*
 *  Schedule change for 'shy':
 */

void Shy_schedule::now_what() {
	Actor*           av     = gwin->get_main_actor();
	const Tile_coord avpos  = av->get_tile();
	const Tile_coord npcpos = npc->get_tile();
	// How far away is Avatar?
	const int dist = npc->distance(av);
	if (dist > 10) {    // Far enough?
		// Check again in a few seconds.
		if (rand() % 3) {    // Just wait.
			npc->start(250, 1000 + rand() % 1000);
		} else {    // Sometimes wander.
			npc->walk_to_tile(Tile_coord(
					npcpos.tx + rand() % 6 - 3, npcpos.ty + rand() % 6 - 3,
					npcpos.tz));
		}
		return;
	}
	// Get deltas.
	const int dx  = npcpos.tx - avpos.tx;
	const int dy  = npcpos.ty - avpos.ty;
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	// Which is farthest?
	const int farthest = adx < ady ? ady : adx;
	const int factor   = farthest < 2   ? 9
						 : farthest < 4 ? 4
						 : farthest < 7 ? 2
										: 1;
	// Walk away.
	Tile_coord       dest  = npcpos + Tile_coord(dx * factor, dy * factor, 0);
	const Tile_coord delta = Tile_coord(rand() % 3, rand() % 3, 0);
	dest                   = dest + delta;
	Monster_pathfinder_client cost(npc, dest, 4);
	Actor_action*             pact
			= Path_walking_actor_action::create_path(npcpos, dest, cost);
	if (pact) {    // Found path?
		npc->set_action(pact);
		npc->start(200, 100 + rand() % 200);    // Start walking.
	} else {                                    // Try again in a couple secs.
		npc->start(250, 500 + rand() % 1000);
	}
}

/*
 *  Steal.
 */

void Thief_schedule::steal(Actor* from) {
	if (npc->can_speak()) {
		npc->say(first_thief, last_thief);
	}
	int          shnum = rand() % 3;    // Gold coin, nugget, bar.
	Game_object* obj   = nullptr;
	for (int i = 0; !obj && i < 3; ++i) {
		obj   = from->find_item(644 + shnum, c_any_qual, c_any_framenum);
		shnum = (shnum + 1) % 3;
	}
	if (obj) {
		Game_object_shared keep;
		obj->remove_this(&keep);
		npc->add(obj, false);
	}
}

/*
 *  Next decision for thief.
 */

void Thief_schedule::now_what() {
	const unsigned long curtime = SDL_GetTicks();
	if (curtime < next_steal_time) {
		// Not time?  Wander.
		const Tile_coord center = gwin->get_main_actor()->get_tile();
		const int        dist   = 6;
		const int        newx   = center.tx - dist + rand() % (2 * dist);
		const int        newy   = center.ty - dist + rand() % (2 * dist);
		// Wait a bit.
		npc->walk_to_tile(
				newx, newy, center.tz, 2 * gwin->get_std_delay(),
				rand() % 4000);
		return;
	}
	if (npc->distance(gwin->get_main_actor()) <= 1) {
		// Next to Avatar.
		if (rand() % 3) {
			steal(gwin->get_main_actor());
		}

		next_steal_time = curtime + 8000 + rand() % 8000;
		npc->start(250, 1000 + rand() % 2000);
	} else {
		// Get within 1 tile of Avatar.
		const Tile_coord          dest = gwin->get_main_actor()->get_tile();
		Monster_pathfinder_client cost(npc, dest, 1);
		Actor_action*             pact = Path_walking_actor_action::create_path(
                npc->get_tile(), dest, cost);
		if (pact) {    // Found path?
			npc->set_action(pact);
			npc->start(gwin->get_std_delay(), 1000 + rand() % 1000);
		} else {    // Try again in a couple secs.
			npc->start(250, 2000 + rand() % 2000);
		}
	}
}

/*
 *  Create a 'waiter' schedule.
 */

Waiter_schedule::Waiter_schedule(Actor* n)
		: Schedule_with_objects(n), startpos(n->get_tile()), customer(nullptr),
		  cooking(false), state(waiter_setup) {}

static int waiter_shapes[] = {616, 628, 944};

int Waiter_schedule::find_items(Game_object_vector& vec, int dist) {
	if (cooking) {    // Find food, pot.
		npc->find_nearby(vec, 377, dist, 0);
		npc->find_nearby(vec, 944, dist, 0);
	}
	if (!cooking || vec.empty()) {
		// Bottle, cup, pot
		for (const int waiter_shape : waiter_shapes) {
			npc->find_nearby(vec, waiter_shape, dist, 0);
		}
	}
	const int floor = npc->get_lift() / 5;    // Make sure it's on same floor.
	vec.erase(
			std::remove_if(
					vec.begin(), vec.end(),
					[floor](Game_object* item) {
						return item->get_lift() / 5 != floor;
					}),
			vec.end());
	return vec.size();
}

static int Count_waiter_objects(Actor* npc) {
	int cnt = 0;
	for (const int waiter_shape : waiter_shapes) {
		cnt += npc->count_objects(waiter_shape);
	}
	return cnt;
}

static int Get_waiter_objects(
		Actor* npc, Game_object_vector& items, bool cooking = false) {
	int cnt = 0;
	if (cooking) {    // Find food, pot.
		cnt += npc->get_objects(items, 377, c_any_qual, c_any_framenum);
		cnt += npc->get_objects(items, 944, c_any_qual, c_any_framenum);
	}
	if (!cooking || !cnt) {
		for (const int waiter_shape : waiter_shapes) {
			cnt += npc->get_objects(
					items, waiter_shape, c_any_qual, c_any_framenum);
		}
	}
	return cnt;
}

/*
 *	Fill in 'plates' list with ones we may have created, ordered by closest
 *  first.  Return true if not empty.
 */
bool Waiter_schedule::find_unattended_plate() {
	if (unattended_plates.empty()) {
		constexpr static const std::array shapes{717};
		vector<Game_object*>              plates;
		npc->find_closest(plates, shapes, 32);
		const int floor
				= npc->get_lift() / 5;    // Make sure it's on same floor.

		for (auto* plate : plates) {
			Actor_vector custs;
			if (plate->get_lift() / 5 == floor
				&& plate->get_flag(Obj_flags::is_temporary)
				&& !plate->find_nearby_actors(custs, c_any_shapenum, 2)) {
				unattended_plates.push_back(weak_from_obj(plate));
			}
		}
	}
	return !unattended_plates.empty();
}

/*
 *  Find a new customer.
 */

bool Waiter_schedule::find_customer() {
	if (customers.empty()) {    // Got to search?
		Actor_vector vec;       // Look within 32 tiles;
		npc->find_nearby_actors(vec, c_any_shapenum, 32);
		for (auto* each : vec) {
			// Filter them.
			if (each->get_schedule_type() == Schedule::eat_at_inn) {
				customers.push_back(each);
			}
		}
	}

	if (!customers.empty()) {
		customer = customers.back();
		customers.pop_back();
	} else {
		customer = nullptr;
	}
	return customer != nullptr;
}

/*
 *  Walk to prep. table.
 *  Output: false if failed.
 */
bool Waiter_schedule::walk_to_work_spot(bool counter) {
	vector<Game_object_weak>& tables = counter ? counters : prep_tables;
	while (!tables.empty()) {    // Walk to a 'prep' table.
		const int index                         = rand() % tables.size();
		prep_table                              = tables[index];
		const Game_object_shared prep_table_obj = prep_table.lock();
		if (prep_table_obj) {
			const Tile_coord pos
					= Map_chunk::find_spot(prep_table_obj->get_tile(), 1, npc);
			if (pos.tx != -1
				&& npc->walk_path_to_tile(
						pos, gwin->get_std_delay(), 1000 + rand() % 1000)) {
				return true;
			}
		}
		// Failed, so remove this table from the list.
		tables.erase(tables.begin() + index);
	}
	prep_table     = Game_object_weak();
	const int dist = 8;    // Bad luck?  Walk randomly.
	const int newx = startpos.tx - dist + rand() % (2 * dist);
	const int newy = startpos.ty - dist + rand() % (2 * dist);
	npc->walk_to_tile(
			newx, newy, startpos.tz, 2 * gwin->get_std_delay(), rand() % 2000);
	return false;
}

/*
 *  Walk to customer
 *  Output: false if failed.
 */
bool Waiter_schedule::walk_to_customer(int min_delay    // Min. delay in msecs.
) {
	if (customer) {
		if (customer->get_schedule_type() != Schedule::eat_at_inn) {
			// Customer schedule changed. Tell schedule to refresh the list
			// (this happens with Hawk & others in SI).
			customers.clear();
		} else {
			const Tile_coord dest
					= Map_chunk::find_spot(customer->get_tile(), 3, npc);
			if (dest.tx != -1
				&& npc->walk_path_to_tile(
						dest, gwin->get_std_delay(),
						min_delay + rand() % 1000)) {
				return true;    // Walking there.
			}
		}
	}
	npc->start(200, 2000 + rand() % 4000);    // Failed so try again later.
	return false;
}

/*
 *  Find tables and categorize them.
 */

void Waiter_schedule::find_tables(int shapenum, int dist, bool is_prep) {
	Game_object_vector vec;
	npc->find_nearby(vec, shapenum, dist, 0);
	const int floor = npc->get_lift() / 5;    // Make sure it's on same floor.
	for (auto* table_obj : vec) {
		if (table_obj->get_lift() / 5 != floor) {
			continue;
		}
		const Game_object_weak table = weak_from_obj(table_obj);
		Game_object_vector     chairs;    // No chairs by it?
										  // TODO: check for nearby stove.
		if (!table_obj->find_nearby(chairs, 873, 3, 0)
			&& !table_obj->find_nearby(chairs, 292, 3, 0)) {
			if (is_prep) {
				prep_tables.push_back(table);
			} else if (shapenum == 847) {
				counters.push_back(table);
			}
		} else {
			eating_tables.push_back(table);
		}
	}
}

void Waiter_schedule::find_prep_tables() {
	static const int shapes[] = {333, 1018, 1003,    // Tables.
								 664, 872,           // Stoves
								 995};               // Cauldron
	for (const int shape : shapes) {
		find_tables(shape, 26, true);
	}
	if (prep_tables.empty()) {    // Look farther.
		for (const int shape : shapes) {
			find_tables(shape, 36, true);
		}
	}
	if (prep_tables.empty()) {
		for (const int shape : shapes) {
			find_tables(shape, 50, true);
		}
	}
}

static Game_object* Find_customer_table(
		Actor* customer, vector<Game_object_weak>& tables) {
	for (auto& it : tables) {
		const Game_object_shared table = it.lock();
		if (table && customer->distance(table.get()) < 3) {
			return table.get();
		}
	}
	return nullptr;
}

static Game_object* Find_customer_plate(Actor* customer) {
	Game_object_vector plates;    // First look for a nearby plate.
	Game_object*       plate = nullptr;

	(void)customer->find_nearby(plates, 717, 1, 0);
	const int floor
			= customer->get_lift() / 5;    // Make sure it's on same floor.
	for (auto* it : plates) {
		plate            = it;
		const int platez = plate->get_lift();
		if (platez / 5 == floor && platez % 5 != 0) {
			return plate;
		}
	}
	return nullptr;
}

Game_object* Waiter_schedule::create_customer_plate() {
	const Tile_coord cpos = customer->get_tile();

	// First, erase all deleted tables.
	eating_tables.erase(
			std::remove_if(
					eating_tables.begin(), eating_tables.end(),
					[](Game_object_weak& elem) {
						const Game_object_shared table = elem.lock();
						return table != nullptr;
					}),
			eating_tables.end());

	// Go through tables.
	for (auto& eating_table : eating_tables) {
		const Game_object_shared table = eating_table.lock();
		if (!table) {
			// This should not be possible.
			// In any case, lets ignore these for now, and they
			// will be cleared next time through this function.
			continue;
		}
		const TileRect foot = table->get_footprint();
		if (foot.distance(cpos.tx, cpos.ty) > 2) {
			continue;
		}
		// Found it.
		Tile_coord spot = cpos;    // Start here.
		// East/West of table?
		if (cpos.ty >= foot.y && cpos.ty < foot.y + foot.h) {
			spot.tx = cpos.tx <= foot.x ? foot.x : foot.x + foot.w - 1;
		} else {    // North/south.
			spot.ty = cpos.ty <= foot.y ? foot.y : foot.y + foot.h - 1;
		}
		if (foot.has_world_point(spot.tx, spot.ty)) {
			// Passes test.
			const Shape_info& info = table->get_info();
			spot.tz                = table->get_lift() + info.get_3d_height();
			// Small plates:  frames 4, 5.  Seems random.
			const Game_object_shared plate
					= gmap->create_ireg_object(717, 4 + rand() % 2);
			plate->set_flag(Obj_flags::is_temporary);
			plate->move(spot);
			return plate.get();
		}
	}
	return nullptr;    // Failed.
}

static void Ready_food(Actor* npc) {
	Game_object*       food;
	Game_object*       obj = npc->get_readied(lhand);
	Game_object_shared food_keep;
	Game_object_shared obj_keep;
	if (obj) {    // Already something there?
		obj_keep = obj->shared_from_this();
		if (obj->get_shapenum() == 377) {
			return;    // Food, so done.
		}
		npc->remove(obj);    // Make space.
	}
	Game_object_vector items;
	if (npc->get_objects(items, 377, c_any_qual, c_any_framenum)) {
		// Already have one, so just move it.
		food      = items[0];
		food_keep = food->shared_from_this();
		npc->remove(food);
	} else {
		// Acquire some food.
		const int nfoods = ShapeID(377, 0).get_num_frames();
		int       frame  = rand() % nfoods;
		food_keep = std::make_shared<Ireg_game_object>(377, frame, 0, 0, 0);
		food      = food_keep.get();
		food->set_flag(Obj_flags::is_temporary);
		food->clear_flag(Obj_flags::okay_to_take);
	}
	npc->add_readied(food, lhand);
	if (obj) {
		npc->add(obj, true);    // Add back what was there before.
	}
}

/*
 *  Find serving spot for a customer.
 *
 *  Output: Plate if found, with spot set.  Plate is created if needed.
 */

Game_object* Waiter_schedule::find_serving_spot(Tile_coord& spot) {
	// First look for a nearby plate.
	Game_object* plate = Find_customer_plate(customer);
	if (!plate) {
		plate = create_customer_plate();
	}
	if (plate) {
		spot = plate->get_tile();
		spot.tz++;    // Just above plate.
	}
	return plate;
}

static void Prep_animation(Actor* npc, Game_object* table) {
	npc->change_frame(npc->get_dir_framenum(
			npc->get_facing_direction(table), Actor::standing));
	auto* scr = new Usecode_script(npc);
	(*scr) << Ucscript::face_dir << npc->get_dir_facing();
	for (int cnt = 1 + rand() % 3; cnt; --cnt) {
		(*scr) << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 1
			   << Ucscript::npc_raise1_frame << Ucscript::delay_ticks << 1;
	}
	(*scr) << Ucscript::npc_standing_frame;
	scr->start();    // Start next tick.
	const int shapenum = table->get_shapenum();
	// Cauldron or stove?  Animate a little.
	if (shapenum == 995) {
		// Only switch between frames 0 and 2
		if (table->get_framenum() == 0) {
			table->change_frame(2);
		} else {
			table->change_frame(0);
		}
	} else if (shapenum == 664 || shapenum == 872) {
		do {
			table->change_frame(rand() % table->get_num_frames());
		} while (table->is_frame_empty());
	}
}

/*
 *  Schedule change for 'waiter':
 */

void Waiter_schedule::now_what() {
	Game_object* food;

	if (state == wait_at_counter
		&& rand() % 4 == 0) {    // Check for lamps, etc.
		if (try_street_maintenance()) {
			return;    // We no longer exist.
		}
	}
	if (state == get_order || state == serve_food) {
		const int dist = customer ? npc->distance(customer) : 5000;
		if (dist > 32) {    // Need a new customer?
			state = get_customer;
			npc->start(200, 1000 + rand() % 1000);
			return;
		}
		// Not close enough, so try again.
		if (dist >= 3 && !walk_to_customer()) {
			state = get_customer;
			return;
		}
	}
	switch (state) {
	case waiter_setup:
		items_in_hand = Count_waiter_objects(npc);
		find_tables(971, 24);
		find_tables(633, 24);
		find_tables(847, 24);
		find_tables(890, 24);
		find_tables(964, 24);
		find_prep_tables();
		state = get_customer;
		[[fallthrough]];
	case get_customer:
		if (!find_customer()) {
			walk_to_prep();
			state = prep_food;
		} else if (walk_to_customer()) {
			state = get_order;
		}
		break;
	case get_order: {
		Game_object_vector foods;
		// Close enough to customer?
		if (customer->find_nearby(foods, 377, 1, 0) > 0) {
			if (rand() % 4) {
				npc->say(first_waiter_banter, last_waiter_banter);
			}
		} else {
			// Ask for order.
			npc->say(first_waiter_ask, last_waiter_ask);
			if (!Find_customer_plate(customer)) {
				state = give_plate;
				npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
				break;
			}
			customers_ordered.push_back(customer);
		}
		state = took_order;
		[[fallthrough]];
	}
	case took_order:
		// Get up to 4 orders before serving them.
		if (customers_ordered.size() >= 4 || customers.empty()) {
			walk_to_prep();
			state = prep_food;
		} else {
			state = get_customer;
			npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		}
		break;
	case give_plate: {
		create_customer_plate();
		auto* scr = new Usecode_script(npc);
		(*scr) << Ucscript::face_dir << npc->get_dir_facing()
			   << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 2
			   << Ucscript::npc_standing_frame;
		scr->start();    // Start next tick.
		state = took_order;
		customers_ordered.push_back(customer);
		npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		break;
	}
	case prep_food: {
		if (cooking && items_in_hand < 4 && rand() % 2 == 0
			&& walk_to_random_item(4)) {
			state = get_waiter_item;
			break;
		}
		const Game_object_shared prep_table_obj = prep_table.lock();
		Ready_food(npc);    // So we can drop it.
		if ((!cooking || rand() % 3) && prep_table_obj
			&& npc->distance(prep_table_obj.get()) <= 3) {
			Game_object* to_drop = nullptr;
			if (rand() % 3) {
				Game_object_vector items;
				items_in_hand = Get_waiter_objects(npc, items, true);
				to_drop       = items_in_hand ? items[rand() % items_in_hand]
											  : nullptr;
			}
			cooking = true;
			if (to_drop) {
				(void)drop_item(to_drop, prep_table_obj.get());
			} else {
				Prep_animation(npc, prep_table_obj.get());
			}
			npc->start(gwin->get_std_delay(), 500 + rand() % 500);
			break;
		}
		if (rand() % 3 == 1 && prep_tables.size() > 1) {
			// A little more cooking.
			walk_to_prep();
			break;
		}
		Ready_food(npc);
		cooking = false;
		state   = bring_food;
		[[fallthrough]];
	}
	case bring_food:
		if (customers_ordered.empty()) {    // All done serving them?
			if (rand() % 3 == 0) {
				gwin->get_usecode()->call_usecode(
						npc->get_usecode(), npc,
						Usecode_machine::npc_proximity);
			}
			if (walk_to_counter()) {
				state = wait_at_counter;
			} else {
				state = get_customer;
			}
		} else {
			customer = customers_ordered.back();
			customers_ordered.pop_back();
			Ready_food(npc);
			if (walk_to_customer(3000)) {
				state = serve_food;
			} else {
				state = get_customer;
			}
		}
		break;
	case wait_at_counter: {
		const Game_object_shared prep_table_obj = prep_table.lock();
		/* Check for customers who have left: */
		if (rand() % 2 && find_unattended_plate()) {
			state = walk_to_cleanup_food;
		} else if (items_in_hand < 3 && rand() % 2 && walk_to_random_item(12)) {
			state = get_waiter_item;
			break;
		} else if (prep_table_obj) {
			npc->change_frame(npc->get_dir_framenum(
					npc->get_facing_direction(prep_table_obj.get()),
					Actor::standing));
			if (rand() % 2) {
				Prep_animation(npc, prep_table_obj.get());
			}
			if (rand() % 3 == 1) {
				state = get_customer;
			}
		} else {
			state = get_customer;
		}
		npc->start(gwin->get_std_delay(), 2000 + rand() % 2000);
		break;
	}
	case get_waiter_item: {
		Game_object* item = get_current_item();
		if (item && npc->distance(item) <= 3) {
			npc->set_action(new Pickup_actor_action(item, 250));
			state = picked_up_item;
			npc->start(250, 500 + rand() % 1000);
			++items_in_hand;
			break;
		}
		[[fallthrough]];
	}
	case picked_up_item:
		set_current_item(nullptr);
		if (cooking) {
			walk_to_prep();
			state = prep_food;
		} else if (walk_to_counter()) {
			state = wait_at_counter;
		} else {
			state = get_customer;
		}
		break;
	case serve_food: {
		food = npc->get_readied(lhand);
		Tile_coord spot;
		if (food && food->get_shapenum() == 377 && find_serving_spot(spot)) {
			npc->change_frame(npc->get_dir_framenum(
					npc->get_direction(customer), Actor::standing));
			const Game_object_shared food_keep = food->shared_from_this();
			npc->remove(food);
			food->set_invalid();
			food->move(spot);
			if (rand() % 3) {
				npc->say(first_waiter_serve, last_waiter_serve);
			}
			auto* scr = new Usecode_script(npc);
			(*scr) << Ucscript::face_dir << npc->get_dir_facing()
				   << Ucscript::npc_ready_frame << Ucscript::delay_ticks << 2
				   << Ucscript::npc_standing_frame;
			scr->start();    // Start next tick.
		}
		state = served_food;
		npc->start(250, 1000 + rand() % 1000);
		break;
	}
	case served_food:
		// Randomly drop or pick up an item.
		if (rand() % 2) {
			if (customer && items_in_hand > 0) {
				Game_object_vector items;
				items_in_hand = Get_waiter_objects(npc, items);
				if (items_in_hand) {
					Game_object* table
							= Find_customer_table(customer, eating_tables);
					Game_object* to_drop = items[rand() % items_in_hand];
					if (table) {
						(void)drop_item(to_drop, table);
					}
				}
			}
		} else if (items_in_hand < 4) {
			Game_object_vector items;    // Look near by to pick up an item.
			const int          cnt = Waiter_schedule::find_items(items, 4);
			if (cnt) {
				npc->set_action(
						new Pickup_actor_action(items[rand() % cnt], 250));
				++items_in_hand;
			}
		}
		customer = nullptr;       // Done with this one.
		state    = bring_food;    // On to the next.
		npc->start(gwin->get_std_delay(), 1000 + rand() % 1000);
		break;
	case walk_to_cleanup_food:
		if (!unattended_plates.empty()) {
			// Closest.
			const Game_object_shared plate = unattended_plates.front().lock();
			if (plate
				&& npc->walk_path_to_tile(
						plate->get_tile(), gwin->get_std_delay(),
						500 + rand() % 1000, 2)) {
				state = cleanup_food;
				break;
			} else {
				unattended_plates.erase(unattended_plates.begin());
				npc->start(gwin->get_std_delay(), 0);
			}
		} else {
			state = walk_to_counter() ? wait_at_counter : get_customer;
		}
		break;
	case cleanup_food:
		if (!unattended_plates.empty()) {
			const Game_object_shared plate = unattended_plates.front().lock();
			unattended_plates.erase(unattended_plates.begin());
			// Delete after picking up.
			if (!plate) {
				state = walk_to_cleanup_food;
				npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
				break;
			}
			Actor_action* act = new Pickup_actor_action(plate.get(), 250, true);
			Game_object*  food = plate->find_closest(377, 2);
			if (food) {
				act = new Sequence_actor_action(
						act, new Pickup_actor_action(food, 250, true));
			}
			npc->set_action(act);
			state = walk_to_cleanup_food;
			npc->start(gwin->get_std_delay(), 500 + rand() % 1000);
		} else {
			state = walk_to_counter() ? wait_at_counter : get_customer;
		}
		break;
	}
}

/*
 *  Waiter schedule is done.
 */

void Waiter_schedule::ending(int new_type    // New schedule.
) {
	ignore_unused_variable_warning(new_type);
	// Remove what he/she is carrying.
	Game_object* obj = npc->get_readied(lhand);
	if (obj) {
		obj->remove_this();
	}
	obj = npc->get_readied(rhand);
	if (obj) {
		obj->remove_this();
	}
}

/*
 *  Sew/weave schedule.
 */

Sew_schedule::Sew_schedule(Actor* n)
		: Schedule(n), sew_clothes_cnt(0), state(get_wool) {}

/*
 *  Sew/weave.
 */

void Sew_schedule::now_what() {
	const Tile_coord npcpos = npc->get_tile();
	// Often want to get within 1 tile.
	Actor_pathfinder_client cost(npc, 1);
	// Pure guess. Since only some schedules seem to call proximity
	// usecode, I guess I should put it in here.
	// Seems quite rare.
	if (try_proximity_usecode(12)) {
		return;
	}
	switch (state) {
	case get_wool: {
		Game_object_shared keep;
		// Clean up any remainders.
		if ((keep = spindle.lock())) {
			keep->remove_this();
		}
		if ((keep = cloth.lock())) {
			keep->remove_this();
		}
		cloth = spindle = Game_object_weak();
		npc->remove_quantity(2, 654, c_any_qual, c_any_framenum);
		npc->remove_quantity(2, 851, c_any_qual, c_any_framenum);

		Game_object* bale_obj = npc->find_closest(653);
		bale                  = weak_from_obj(bale_obj);
		if (!bale_obj) {    // Just skip this step.
			state = sit_at_wheel;
			break;
		}
		Actor_action* pact = Path_walking_actor_action::create_path(
				npcpos, bale_obj->get_tile(), cost);
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Pickup_actor_action(bale_obj, 250),
					new Pickup_actor_action(
							bale_obj, bale_obj->get_tile(), 250)));
		}
		state = sit_at_wheel;
		break;
	}
	case sit_at_wheel: {
		Game_object* chair_obj = npc->find_closest(873);
		chair                  = weak_from_obj(chair_obj);
		if (!chair_obj || !Sit_schedule::set_action(npc, chair_obj, 200)) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			return;
		}
		state = spin_wool;
		break;
	}
	case spin_wool: {    // Cycle spinning wheel 8 times.
		Game_object* spinwheel_obj = npc->find_closest(651);
		spinwheel                  = weak_from_obj(spinwheel_obj);
		if (!spinwheel_obj) {
			// uh-oh... try again in a few seconds?
			npc->start(250, 2500);
			return;
		}
		npc->set_action(new Object_animate_actor_action(spinwheel_obj, 8, 200));
		state = get_thread;
		break;
	}
	case get_thread: {
		const Game_object_shared spinwheel_obj = spinwheel.lock();
		if (!spinwheel_obj) {
			state = get_wool;
			break;
		}
		const Tile_coord t
				= Map_chunk::find_spot(spinwheel_obj->get_tile(), 1, 654, 0);
		if (t.tx != -1) {    // Space to create thread?
			const Game_object_shared newobj
					= std::make_shared<Ireg_game_object>(654, 0, 0, 0);
			spindle = Game_object_weak(newobj);
			newobj->move(t);
			gwin->add_dirty(newobj.get());
			npc->set_action(new Pickup_actor_action(newobj.get(), 250));
		}
		state = weave_cloth;
		break;
	}
	case weave_cloth: {
		const Game_object_shared spindle_obj = spindle.lock();
		if (spindle_obj) {    // Should be held by NPC.
			spindle_obj->remove_this();
		}
		spindle               = Game_object_weak();
		Game_object* loom_obj = npc->find_closest(261);
		loom                  = weak_from_obj(loom_obj);
		if (!loom_obj) {    // No loom found?
			state = get_wool;
			break;
		}
		const Tile_coord lpos = loom_obj->get_tile() + Tile_coord(-1, 0, 0);
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, lpos, cost);
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Face_pos_actor_action(loom_obj, 250),
					new Object_animate_actor_action(loom_obj, 4, 200)));
		}
		state = get_cloth;
		break;
	}
	case get_cloth: {
		const Game_object_shared loom_obj = loom.lock();
		const Tile_coord         t        = loom_obj ? Map_chunk::find_spot(
                                                loom_obj->get_tile(), 1, 851, 0)
													 : Tile_coord(-1, -1, -1);
		if (t.tx != -1) {    // Space to create it?
			const Game_object_shared newobj
					= std::make_shared<Ireg_game_object>(851, rand() % 2, 0, 0);
			cloth = Game_object_weak(newobj);
			newobj->move(t);
			gwin->add_dirty(newobj.get());
			npc->set_action(new Pickup_actor_action(newobj.get(), 250));
		}
		state = to_work_table;
		break;
	}
	case to_work_table: {
		Game_object* work_table_obj        = npc->find_closest(971);
		work_table                         = weak_from_obj(work_table_obj);
		const Game_object_shared cloth_obj = cloth.lock();
		if (!work_table_obj || !cloth_obj) {
			state = get_wool;
			break;
		}
		const Tile_coord tpos
				= work_table_obj->get_tile() + Tile_coord(1, -2, 0);
		Actor_action* pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);
		// Find where to put cloth.
		const TileRect    foot = work_table_obj->get_footprint();
		const Shape_info& info = work_table_obj->get_info();
		const Tile_coord  cpos(
                foot.x + foot.w / 2, foot.y + foot.h / 2,
                work_table_obj->get_lift() + info.get_3d_height());
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Face_pos_actor_action(work_table_obj, 250),
					new Pickup_actor_action(cloth_obj.get(), cpos, 250)));
		}
		state = set_to_sew;
		break;
	}
	case set_to_sew: {
		Game_object* shears = npc->get_readied(lhand);
		if (shears && shears->get_shapenum() != 698) {
			// Something's not right.
			shears->remove_this();
			shears = nullptr;
		}
		if (!shears) {
			// Shears on table?
			Game_object_vector vec;
			Game_object_shared keep;
			if (npc->find_nearby(vec, 698, 3, 0)) {
				shears = vec[0];
				gwin->add_dirty(shears);
				shears->remove_this(&keep);
			} else {
				keep   = std::make_shared<Ireg_game_object>(698, 0, 0, 0);
				shears = keep.get();
			}
			npc->add_readied(shears, lhand);
		}
		state           = sew_clothes;
		sew_clothes_cnt = 0;
		break;
	}
	case sew_clothes: {
		const Game_object_shared cloth_obj = cloth.lock();
		if (!cloth_obj) {
			state = get_wool;
			break;
		}
		const int   dir = npc->get_direction(cloth_obj.get());
		signed char frames[5];
		const int   nframes = npc->get_attack_frames(698, false, dir, frames);
		if (nframes) {
			npc->set_action(new Frames_actor_action(frames, nframes));
		}
		sew_clothes_cnt++;
		if (sew_clothes_cnt > 1 && sew_clothes_cnt < 5) {
			const int num_cloth_frames = ShapeID(851, 0).get_num_frames();
			cloth_obj->change_frame(rand() % num_cloth_frames);
		} else if (sew_clothes_cnt == 5) {
			gwin->add_dirty(cloth_obj.get());
			const Tile_coord   pos = cloth_obj->get_tile();
			Game_object_shared keep;
			cloth_obj->remove_this(&keep);
			// Top or pants.
			const int shnum = GAME_SI ? 403 : (rand() % 2 ? 738 : 249);
			cloth_obj->set_shape(shnum);
			const int nframes = ShapeID(shnum, 0).get_num_frames();
			cloth_obj->change_frame(rand() % nframes);
			cloth_obj->move(pos);
			state = get_clothes;
		}
		break;
	}
	case get_clothes: {
		Game_object*             shears    = npc->get_readied(lhand);
		const Game_object_shared cloth_obj = cloth.lock();
		if (shears && cloth_obj) {
			const Tile_coord pos = cloth_obj->get_tile();
			npc->set_action(new Sequence_actor_action(
					new Pickup_actor_action(cloth_obj.get(), 250),
					new Pickup_actor_action(shears, pos, 250)));
		} else {
			// ++++ maybe create shears? anyway, leaving this till after
			// possible/probable schedule system rewrite
			if (cloth_obj) {
				npc->set_action(new Pickup_actor_action(cloth_obj.get(), 250));
			} else {
				state = get_wool;
				break;
			}
		}
		state = display_clothes;
		break;
	}
	case display_clothes: {
		state                              = done;
		Game_object* wares_table_obj       = npc->find_closest(890);
		wares_table                        = weak_from_obj(wares_table_obj);
		const Game_object_shared cloth_obj = cloth.lock();
		if (!wares_table_obj || !cloth_obj) {
			if (cloth_obj) {
				cloth_obj->remove_this();
				cloth = Game_object_weak();
			}
			break;
		}
		const Tile_coord tpos
				= wares_table_obj->get_tile() + Tile_coord(1, -2, 0);
		Actor_action* pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);
		// Find where to put cloth.
		const TileRect    foot = wares_table_obj->get_footprint();
		const Shape_info& info = wares_table_obj->get_info();
		const Tile_coord  cpos(
                foot.x + rand() % foot.w, foot.y + rand() % foot.h,
                wares_table_obj->get_lift() + info.get_3d_height());
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact,
					new Pickup_actor_action(cloth_obj.get(), cpos, 250, true)));
		}
		cloth = Game_object_shared();    // Leave it be.
		break;
	}
	case done: {    // Just put down clothing.
		state = get_wool;
		Game_object_vector vec;    // Don't create too many.
		int                cnt = 0;
		if (GAME_SI) {
			cnt += npc->find_nearby(vec, 403, 5, 0);
		} else {    // BG shapes.
			cnt += npc->find_nearby(vec, 738, 5, 0);
			cnt += npc->find_nearby(vec, 249, 5, 0);
		}
		if (cnt >= 3) {
			Game_object* obj = vec[rand() % cnt];
			gwin->add_dirty(obj);
			obj->remove_this();
		}
		break;
	}
	default:    // Back to start.
		state = get_wool;
		break;
	}
	npc->start(250, 100);    // Back in queue.
}

/*
 *  Sewing schedule is done.
 */

void Sew_schedule::ending(int new_type    // New schedule.
) {
	ignore_unused_variable_warning(new_type);
	// Remove shears.
	Game_object* obj = npc->get_readied(lhand);
	if (obj) {
		obj->remove_this();
	}
	Game_object_shared cloth_keep;
	if ((cloth_keep = cloth.lock())) {
		// Don't leave cloth lying around.
		if (!cloth_keep->get_owner()) {
			gwin->add_dirty(cloth_keep.get());
		}
		cloth_keep->remove_this();
	}
}

/*
 *  Bake bread/pastries
 *
 *  Dough gets set to quality 50 and dough placed in oven is quality 51
 */

Bake_schedule::Bake_schedule(Actor* n)
		: Schedule(n), clearing(false), state(find_leftovers) {}

void Bake_schedule::now_what() {
	const Tile_coord              npcpos = npc->get_tile();
	const Actor_pathfinder_client cost1(npc, 1);
	const Actor_pathfinder_client cost2(npc, 2);
	int                           delay = 100;
	int                           dough_shp(GAME_SI ? 863 : 658);
	Game_object*                  stove = npc->find_closest(664);
	// Pure guess. Since only some schedules seem to call proximity
	// usecode, I guess I should put it in here.
	// Seems quite common.
	if (try_proximity_usecode(8)) {
		return;
	}

	switch (state) {
	case find_leftovers: {    // Look for misplaced dough already made by this
							  // schedule
		state                                = to_flour;
		Game_object_shared dough_in_oven_obj = dough_in_oven.lock();
		Game_object_shared dough_obj         = dough.lock();
		if (!dough_in_oven_obj) {
			// look for baking dough
			Game_object_vector baking_dough;
			const int          frnum(GAME_SI ? 18 : 2);
			Actor::find_nearby(
					baking_dough, npcpos, dough_shp, 20, 0, 51, frnum, false);
			if (!baking_dough.empty() && baking_dough[0] != dough_obj.get()) {
				// found dough
				dough_in_oven_obj = baking_dough[0]->shared_from_this();
				dough_in_oven_obj->clear_flag(Obj_flags::okay_to_take);
				// doesn't save okay_to_take
				dough_in_oven = Game_object_weak(dough_in_oven_obj);
				state         = remove_from_oven;
				break;
			}
			// looking for cooked food left in oven
			Game_object* oven_obj = npc->find_closest(831);
			if (!oven_obj) {
				oven_obj = stove;
			}
			oven = weak_from_obj(oven_obj);
			if (oven_obj) {
				Game_object_vector food;
				const Tile_coord   Opos = oven_obj->get_tile();
				Actor::find_nearby(
						food, Opos, 377, 2, 0, c_any_qual, c_any_framenum,
						true);
				if (!food.empty()) {    // found food
					dough_in_oven_obj = food[0]->shared_from_this();
					dough_in_oven     = Game_object_weak(dough_in_oven_obj);
					dough_in_oven_obj->clear_flag(
							Obj_flags::okay_to_take);    // doesn't save
														 // okay_to_take
					state = remove_from_oven;
					break;
				}
			}
		}
		if (!dough_obj) {    // Looking for unused dough on tables
			Game_object_vector leftovers;
			if (GAME_SI) {
				Actor::find_nearby(
						leftovers, npcpos, dough_shp, 20, 0, 50, 16, false);
				Actor::find_nearby(
						leftovers, npcpos, dough_shp, 20, 0, 50, 17, false);
				Actor::find_nearby(
						leftovers, npcpos, dough_shp, 20, 0, 50, 18, false);
			} else {
				Actor::find_nearby(
						leftovers, npcpos, dough_shp, 20, 0, 50, c_any_framenum,
						false);
			}
			if (!leftovers.empty()
				&& leftovers[0] != dough_in_oven_obj.get()) {    // found dough
				dough_obj = leftovers[0]->shared_from_this();
				dough     = Game_object_weak(dough_obj);
				dough_obj->clear_flag(
						Obj_flags::okay_to_take);    // doesn't save
													 // okay_to_take
				state              = make_dough;
				delay              = 0;
				Actor_action* pact = Path_walking_actor_action::create_path(
						npcpos, dough_obj->get_tile(), cost2);
				if (pact) {    // walk to dough if we can
					npc->set_action(pact);
				}
			}
		}
		break;
	}
	case to_flour: {    // Looks for flourbag and walks to it if found
		Game_object_vector items;
		Actor::find_nearby(items, npcpos, 863, -1, 0, c_any_qual, 0);
		Actor::find_nearby(items, npcpos, 863, -1, 0, c_any_qual, 13);
		Actor::find_nearby(items, npcpos, 863, -1, 0, c_any_qual, 14);

		if (items.empty()) {
			state = to_table;
			break;
		}

		const int    nr           = rand() % items.size();
		Game_object* flourbag_obj = items[nr];
		flourbag                  = weak_from_obj(flourbag_obj);

		const Tile_coord tpos = flourbag_obj->get_tile();
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost1);
		if (pact) {
			npc->set_action(pact);
		} else {
			// just ignore it
			state = to_table;
			break;
		}

		state = get_flour;
		break;
	}
	case get_flour: {    // Bend over flourbag and change the frame to zero if
						 // nonzero
		const Game_object_shared flourbag_obj = flourbag.lock();
		if (!flourbag_obj) {
			// what are we doing here then? back to start
			state = to_flour;
			break;
		}

		const int dir = npc->get_direction(flourbag_obj.get());
		npc->change_frame(npc->get_dir_framenum(dir, Actor::bow_frame));

		if (flourbag_obj->get_framenum() != 0) {
			flourbag_obj->change_frame(0);
		}

		delay = 750;
		state = to_table;
		break;
	}
	case to_table: {    // Walk over to worktable and create flour
		Game_object* table1        = npc->find_closest(1003);
		Game_object* table2        = npc->find_closest(1018);
		Game_object* worktable_obj = nullptr;
		if (stove) {
			Game_object_vector table;
			Actor::find_nearby(table, npcpos, 890, -1, 0, c_any_qual, 5);
			if (table.size() == 1) {
				worktable_obj = table[0];
			} else if (table.size() > 1) {
				if (rand() % 2) {
					worktable_obj = table[0];
				} else {
					worktable_obj = table[1];
				}
			}
		} else if (!table1) {
			worktable_obj = table2;
		} else if (!table2) {
			worktable_obj = table1;
		} else if (table1->distance(npc) < table2->distance(npc)) {
			worktable_obj = table1;
		} else {
			worktable_obj = table2;
		}

		if (!worktable_obj) {
			worktable_obj = npc->find_closest(1018);
		}
		worktable = weak_from_obj(worktable_obj);
		if (!worktable_obj) {
			// problem... try again in a few seconds
			delay = 2500;
			state = to_flour;
			break;
		}
		// Find where to put dough.
		const TileRect    foot = worktable_obj->get_footprint();
		const Shape_info& info = worktable_obj->get_info();
		Tile_coord        cpos(
                foot.x + rand() % foot.w, foot.y + rand() % foot.h,
                worktable_obj->get_lift() + info.get_3d_height());
		const Tile_coord tablepos = cpos;
		cpos.tz                   = 0;

		Actor_action* pact
				= Path_walking_actor_action::create_path(npcpos, cpos, cost1);
		if (pact) {
			Game_object_shared dough_obj = dough.lock();
			if (dough_obj) {
				dough_obj->remove_this();
			}
			if (Game::get_game_type() == SERPENT_ISLE) {
				dough_obj = std::make_shared<Ireg_game_object>(
						dough_shp, 16, 0, 0);
			} else {
				dough_obj = std::make_shared<Ireg_game_object>(
						dough_shp, 0, 0, 0);
			}
			dough = Game_object_weak(dough_obj);
			npc->add(dough_obj.get(), true);
			dough_obj->set_quality(50);    // doesn't save okay_to_take
			npc->set_action(new Sequence_actor_action(
					pact,
					new Pickup_actor_action(dough_obj.get(), tablepos, 250)));
		} else {
			// not good... try again
			delay = 2500;
			state = to_flour;
			break;
		}

		state = make_dough;
		break;
	}
	case make_dough: {    // Changes flour to flat dough then dough ball
		const Game_object_shared dough_obj = dough.lock();
		if (!dough_obj) {
			// better try again...
			delay = 2500;
			state = to_table;
			break;
		}
		dough_obj->clear_flag(
				Obj_flags::okay_to_take);    // doesn't save okay_to_take

		const int   dir = npc->get_direction(dough_obj.get());
		signed char fr[2];
		fr[0] = npc->get_dir_framenum(dir, 3);
		fr[1] = npc->get_dir_framenum(dir, 0);

		const int    frame = dough_obj->get_framenum();
		Game_object* dobj  = dough_obj.get();
		if (GAME_SI ? frame == 16 : frame == 0) {
			npc->set_action(new Sequence_actor_action(
					new Frames_actor_action(fr, 2, 500),
					((GAME_SI) ? new Frames_actor_action(0x11, 250, dobj)
							   : new Frames_actor_action(0x01, 250, dobj)),
					new Frames_actor_action(fr, 2, 500),
					((GAME_SI) ? new Frames_actor_action(0x12, 250, dobj)
							   : new Frames_actor_action(0x02, 250, dobj))));
		} else if (GAME_SI ? frame == 17 : frame == 1) {
			npc->set_action(new Sequence_actor_action(
					new Frames_actor_action(fr, 2, 500),
					((GAME_SI) ? new Frames_actor_action(0x12, 250, dobj)
							   : new Frames_actor_action(0x02, 250, dobj))));
		}

		state = remove_from_oven;
		break;
	}
	case remove_from_oven: {    // Changes dough in oven to food %7 and picks it
								// up
		const Game_object_shared dough_in_oven_obj = dough_in_oven.lock();
		if (!dough_in_oven_obj) {
			// nothing in oven yet
			state = get_dough;
			break;
		}
		Game_object* oven_obj;
		if (stove) {
			oven_obj = stove;
		} else {
			oven_obj = npc->find_closest(831);
		}
		oven = weak_from_obj(oven_obj);
		if (!oven_obj) {
			// this really shouldn't happen...
			dough_in_oven_obj->remove_this();
			delay = 2500;
			state = to_table;
			break;
		}
		if (dough_in_oven_obj->get_shapenum() != 377) {
			gwin->add_dirty(dough_in_oven_obj.get());
			dough_in_oven_obj->set_shape(377);
			dough_in_oven_obj->change_frame(rand() % 7);
			dough_in_oven_obj->clear_flag(
					Obj_flags::okay_to_take);    // doesn't save okay_to_take
			gwin->add_dirty(dough_in_oven_obj.get());
		}

		const Tile_coord tpos = oven_obj->get_tile() + Tile_coord(1, 1, 0);
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost1);
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact,
					new Pickup_actor_action(dough_in_oven_obj.get(), 250)));
		} else {
			// just pick it up
			npc->set_action(
					new Pickup_actor_action(dough_in_oven_obj.get(), 250));
		}

		state = display_wares;
		break;
	}
	case display_wares: {
		// Walk to displaytable. Put food on it. If table full, go to
		const Game_object_shared dough_in_oven_obj = dough_in_oven.lock();
		// clear_display which eventualy comes back here to place food
		if (!dough_in_oven_obj) {
			// try again
			delay = 2500;
			state = find_leftovers;
			break;
		}
		Game_object* displaytable_obj = npc->find_closest(633);    // Britain
		if (!displaytable_obj) {
			Game_object_vector table;
			int                table_shp = 890;    // Moonshade table
			int                table_frm = 1;
			if (stove) {
				table_shp = 1003;
				table_frm = 2;
			}
			Actor::find_nearby(
					table, npcpos, table_shp, -1, 0, c_any_qual, table_frm);
			if (table.size() == 1) {
				displaytable_obj = table[0];
			} else if (table.size() > 1) {
				if (rand() % 2) {
					displaytable_obj = table[0];
				} else {
					displaytable_obj = table[1];
				}
			}
		}
		displaytable = weak_from_obj(displaytable_obj);
		if (!displaytable_obj) {
			// uh-oh...
			dough_in_oven_obj->remove_this();
			delay = 2500;
			state = find_leftovers;
			break;
		}

		TileRect   r = displaytable_obj->get_footprint();
		Perimeter  p(r);    // Find spot adjacent to table.
		Tile_coord spot;    // Also get closest spot on table.
		Tile_coord spot_on_table;
		p.get(rand() % p.size(), spot, spot_on_table);
		const Actor_pathfinder_client& COST = (GAME_SI ? cost1 : cost2);
		Actor_action*                  pact
				= Path_walking_actor_action::create_path(npcpos, spot, COST);
		const Shape_info& info = displaytable_obj->get_info();
		spot_on_table.tz += info.get_3d_height();

		// Place baked goods if spot is empty.
		const Tile_coord t = Map_chunk::find_spot(spot_on_table, 0, 377, 0, 0);
		if (t.tx != -1 && t.tz == spot_on_table.tz) {
			npc->set_action(new Sequence_actor_action(
					pact,
					new Pickup_actor_action(
							dough_in_oven_obj.get(), spot_on_table, 250)));
			dough_in_oven = Game_object_weak();
			state         = get_dough;
		} else {
			delete pact;
			pact = Path_walking_actor_action::create_path(
					npcpos, displaytable_obj->get_tile(), cost1);
			npc->set_action(new Sequence_actor_action(
					pact, new Face_pos_actor_action(
								  displaytable_obj->get_tile(), 250)));
			delay = 250;
			state = clear_display;
		}
		clearing = false;
		break;
	}
	case clear_display: {    // Mark food for deletion by remove_food
		Game_object_vector food;
		Actor::find_nearby(
				food, npcpos, 377, 4, 0, c_any_qual, c_any_framenum, true);
		if (food.empty() && !clearing) {    // none of our food on the table
											// so we can't clear it
			const Game_object_shared dough_in_oven_obj = dough_in_oven.lock();
			if (dough_in_oven_obj) {
				dough_in_oven_obj->remove_this();
			}
			state = get_dough;
			break;
		}
		clearing = true;
		if (!food.empty()) {
			delay = 500;
			state = remove_food;
		} else {
			state = display_wares;
		}
		break;
	}
	case remove_food: {    // Delete food on display table one by one with a
						   // slight delay
		Game_object_vector food;
		Actor::find_nearby(
				food, npcpos, 377, 4, 0, c_any_qual, c_any_framenum, true);
		if (!food.empty()) {
			delay = 500;
			state = clear_display;
			gwin->add_dirty(food[0]);
			food[0]->remove_this();
		}
		break;
	}
	case get_dough: {    // Walk to work table and pick up dough
		const Game_object_shared dough_obj = dough.lock();
		if (!dough_obj) {
			// try again
			delay = 2500;
			state = find_leftovers;
			break;
		}
		Game_object* oven_obj;
		if (stove) {
			oven_obj = stove;
		} else {
			oven_obj = npc->find_closest(831);
		}
		oven = weak_from_obj(oven_obj);
		if (!oven_obj) {
			// wait a while
			delay = 2500;
			state = find_leftovers;
			break;
		}
		const Tile_coord tpos = dough_obj->get_tile();
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost2);
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Pickup_actor_action(dough_obj.get(), 250)));
		} else {
			// just pick it up
			npc->set_action(new Pickup_actor_action(dough_obj.get(), 250));
		}

		state = put_in_oven;
		break;
	}
	case put_in_oven: {    // Walk to oven and put dough on in.
		const Game_object_shared dough_obj = dough.lock();
		if (!dough_obj) {
			// try again
			delay = 2500;
			state = find_leftovers;
			break;
		}
		Game_object* oven_obj;
		if (stove) {
			oven_obj = stove;
		} else {
			oven_obj = npc->find_closest(831);
		}
		oven = weak_from_obj(oven_obj);
		if (!oven_obj) {
			// oops... retry
			dough_obj->remove_this();
			delay = 2500;
			state = to_table;
			break;
		}
		const Tile_coord tpos = oven_obj->get_tile() + Tile_coord(1, 1, 0);
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost1);

		// offsets for oven placement
		int offX = +1;
		int offY = 0;
		int offZ = 0;
		if (stove) {    // hide dough
			offX = -3;
			offY = 0;
			offZ = -2;
		}
		const TileRect    foot = oven_obj->get_footprint();
		const Shape_info& info = oven_obj->get_info();
		const Tile_coord  cpos(
                foot.x + offX, foot.y + offY,
                oven_obj->get_lift() + info.get_3d_height() + offZ);

		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Pickup_actor_action(dough_obj.get(), cpos, 250)));

			dough_obj->set_quality(51);    // doesn't save okay_to_take
			dough_in_oven = dough;
			dough         = Game_object_weak();
		} else {
			dough_obj->remove_this();
		}

		state = find_leftovers;
		break;
	}
	}
	npc->start(250, delay);    // Back in queue.
}

/*
 *  TODO: Eventually, this should be used to finish baking all
 *  leftovers which is basically duplicating the regular schedule
 *  minus to_flour, get_flour, and any future added kitchen items
 *  usage. Special checks for when to just give up should be added.
 */

void Bake_schedule::ending(int new_type) {
	ignore_unused_variable_warning(new_type);
	const Game_object_shared dough_obj         = dough.lock();
	const Game_object_shared dough_in_oven_obj = dough_in_oven.lock();
	if (dough_obj) {
		dough_obj->remove_this();
	}
	if (dough_in_oven_obj) {
		dough_in_oven_obj->remove_this();
	}
}

/*
 *  Blacksmith.
 *
 * Note: the original kept the tongs & hammer, and put them on a nearby table
 */

Forge_schedule::Forge_schedule(Actor* n)
		: Schedule(n), state(put_sword_on_firepit) {}

void Forge_schedule::now_what() {
	const Tile_coord npcpos = npc->get_tile();
	// Often want to get within 1 tile.
	Actor_pathfinder_client cost(npc, 1);

	switch (state) {
	case put_sword_on_firepit: {
		Game_object_shared blank_obj = blank.lock();
		if (!blank_obj) {
			Game_object* found = npc->find_closest(668);
			if (found) {
				blank_obj = found->shared_from_this();
			}
			// TODO: go and get it...
		}
		if (!blank_obj) {
			blank_obj = std::make_shared<Ireg_game_object>(668, 0, 0, 0);
		}
		blank                    = Game_object_weak(blank_obj);
		Game_object* firepit_obj = npc->find_closest(739);
		firepit                  = weak_from_obj(firepit_obj);
		if (!firepit_obj) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			return;
		}
		const Tile_coord tpos = firepit_obj->get_tile();
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);

		const TileRect    foot = firepit_obj->get_footprint();
		const Shape_info& info = firepit_obj->get_info();
		const Tile_coord  bpos(
                foot.x + foot.w / 2 + 1, foot.y + foot.h / 2,
                firepit_obj->get_lift() + info.get_3d_height());
		if (pact) {
			npc->set_action(new Sequence_actor_action(
					pact, new Pickup_actor_action(blank_obj.get(), bpos, 250)));
		} else {
			npc->set_action(
					new Pickup_actor_action(blank_obj.get(), bpos, 250));
		}

		state = use_bellows;
		break;
	}
	case use_bellows: {
		Game_object*             bellows_obj = npc->find_closest(431);
		Game_object*             firepit_obj = npc->find_closest(739);
		const Game_object_shared blank_obj   = blank.lock();
		bellows                              = weak_from_obj(bellows_obj);
		firepit                              = weak_from_obj(firepit_obj);
		if (!bellows_obj || !firepit_obj || !blank_obj) {
			// uh-oh... try again in a few second
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}

		const Tile_coord tpos = bellows_obj->get_tile() + Tile_coord(3, 0, 0);
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);

		auto** a = new Actor_action*[35];
		a[0]     = pact;
		a[1]     = new Face_pos_actor_action(bellows_obj, 250);
		a[2]     = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[3]     = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[4]     = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[5]     = new Frames_actor_action(0x01, 0, firepit_obj);
		a[6]     = new Frames_actor_action(0x01, 0, blank_obj.get());
		a[7]     = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[8]     = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[9]     = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[10]    = new Frames_actor_action(0x02, 0, blank_obj.get());
		a[11]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[12]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[13]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[14]    = new Frames_actor_action(0x02, 0, firepit_obj);
		a[15]    = new Frames_actor_action(0x03, 0, blank_obj.get());
		a[16]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[17]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[18]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[19]    = new Frames_actor_action(0x03, 0, firepit_obj);
		a[20]    = new Frames_actor_action(0x04, 0, blank_obj.get());
		a[21]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[22]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[23]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[24]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[25]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[26]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[27]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[28]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[29]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[30]    = new Frames_actor_action(0x20 | Actor::bow_frame, 0);
		a[31]    = new Object_animate_actor_action(bellows_obj, 3, 1, 300);
		a[32]    = new Frames_actor_action(0x20 | Actor::standing, 0);
		a[33]    = new Frames_actor_action(0x00, 0, bellows_obj);
		a[34]    = nullptr;

		npc->set_action(new Sequence_actor_action(a));
		state = get_tongs;
		break;
	}
	case get_tongs: {
		Game_object_shared tongs_obj = tongs.lock();
		if (!tongs_obj) {
			tongs_obj = std::make_shared<Ireg_game_object>(994, 0, 0, 0);
			tongs     = Game_object_weak(tongs_obj);
		}
		npc->empty_hands();    // make sure the tongs can be equipped
		npc->add_readied(tongs_obj.get(), lhand);
		npc->add_dirty();

		state = sword_on_anvil;
		break;
	}
	case sword_on_anvil: {
		Game_object* anvil_obj             = npc->find_closest(991);
		Game_object* firepit_obj           = npc->find_closest(739);
		anvil                              = weak_from_obj(anvil_obj);
		firepit                            = weak_from_obj(firepit_obj);
		const Game_object_shared blank_obj = blank.lock();
		if (!anvil_obj || !firepit_obj || !blank_obj) {
			// uh-oh... try again in a few second
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}

		const Tile_coord tpos = firepit_obj->get_tile();
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);
		const Tile_coord tpos2 = anvil_obj->get_tile() + Tile_coord(0, 1, 0);
		Actor_action*    pact2
				= Path_walking_actor_action::create_path(tpos, tpos2, cost);

		const TileRect    foot = anvil_obj->get_footprint();
		const Shape_info& info = anvil_obj->get_info();
		const Tile_coord  bpos(
                foot.x + 2, foot.y,
                anvil_obj->get_lift() + info.get_3d_height());
		if (pact && pact2) {
			npc->set_action(new Sequence_actor_action(
					pact, new Pickup_actor_action(blank_obj.get(), 250), pact2,
					new Pickup_actor_action(blank_obj.get(), bpos, 250)));
		} else {
			// Don't leak the paths
			delete pact;
			delete pact2;
			npc->set_action(new Sequence_actor_action(
					new Pickup_actor_action(blank_obj.get(), 250),
					new Pickup_actor_action(blank_obj.get(), bpos, 250)));
		}
		state = get_hammer;
		break;
	}
	case get_hammer: {
		Game_object_shared hammer_obj = hammer.lock();
		if (!hammer_obj) {
			hammer_obj = std::make_shared<Ireg_game_object>(623, 0, 0, 0);
			hammer     = Game_object_weak(hammer_obj);
		}
		npc->add_dirty();
		const Game_object_shared tongs_obj = tongs.lock();
		if (tongs_obj) {
			tongs_obj->remove_this();
			tongs = Game_object_weak();
		}
		npc->empty_hands();    // make sure the hammer can be equipped
		npc->add_readied(hammer_obj.get(), lhand);
		npc->add_dirty();

		state = use_hammer;
		break;
	}
	case use_hammer: {
		Game_object* anvil_obj             = npc->find_closest(991);
		Game_object* firepit_obj           = npc->find_closest(739);
		anvil                              = weak_from_obj(anvil_obj);
		firepit                            = weak_from_obj(firepit_obj);
		const Game_object_shared blank_obj = blank.lock();
		if (!anvil_obj || !firepit_obj || !blank_obj) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}

		signed char frames[12];
		const int   cnt = npc->get_attack_frames(623, false, 0, frames);
		if (cnt) {
			npc->set_action(new Frames_actor_action(frames, cnt));
		}

		auto** a = new Actor_action*[10];
		a[0]     = new Frames_actor_action(frames, cnt);
		a[1]     = new Frames_actor_action(0x03, 0, blank_obj.get());
		a[2]     = new Frames_actor_action(0x02, 0, firepit_obj);
		a[3]     = new Frames_actor_action(frames, cnt);
		a[4]     = new Frames_actor_action(0x02, 0, blank_obj.get());
		a[5]     = new Frames_actor_action(0x01, 0, firepit_obj);
		a[6]     = new Frames_actor_action(frames, cnt);
		a[7]     = new Frames_actor_action(0x01, 0, blank_obj.get());
		a[8]     = new Frames_actor_action(0x00, 0, firepit_obj);
		a[9]     = nullptr;
		npc->set_action(new Sequence_actor_action(a));

		state = walk_to_trough;
		break;
	}
	case walk_to_trough: {
		npc->add_dirty();
		const Game_object_shared hammer_obj = hammer.lock();
		if (hammer_obj) {
			hammer_obj->remove_this();
		}
		npc->add_dirty();

		Game_object* trough_obj = npc->find_closest(719);
		trough                  = weak_from_obj(trough_obj);
		if (!trough_obj) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}

		if (trough_obj->get_framenum() == 0) {
			const Tile_coord tpos
					= trough_obj->get_tile() + Tile_coord(0, 2, 0);
			Actor_action* pact = Path_walking_actor_action::create_path(
					npcpos, tpos, cost);
			npc->set_action(pact);
			state = fill_trough;
			break;
		}

		state = get_tongs2;
		break;
	}
	case fill_trough: {
		Game_object* trough_obj = npc->find_closest(719);
		trough                  = weak_from_obj(trough_obj);
		if (!trough_obj) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}
		const int dir = npc->get_direction(trough_obj);
		trough_obj->change_frame(3);
		npc->change_frame(npc->get_dir_framenum(dir, Actor::bow_frame));

		state = get_tongs2;
		break;
	}
	case get_tongs2: {
		Game_object_shared tongs_obj = tongs.lock();
		if (!tongs_obj) {
			tongs_obj = std::make_shared<Ireg_game_object>(994, 0, 0, 0);
			tongs     = Game_object_weak(tongs_obj);
		}
		npc->empty_hands();    // make sure the tongs can be equipped
		npc->add_readied(tongs_obj.get(), lhand);
		npc->add_dirty();

		state = use_trough;
		break;
	}
	case use_trough: {
		Game_object* trough_obj            = npc->find_closest(719);
		Game_object* anvil_obj             = npc->find_closest(991);
		trough                             = weak_from_obj(trough_obj);
		anvil                              = weak_from_obj(anvil_obj);
		const Game_object_shared blank_obj = blank.lock();
		if (!trough_obj || !anvil_obj || !blank_obj) {
			// uh-oh... try again in a few seconds
			npc->start(250, 2500);
			state = put_sword_on_firepit;
			return;
		}

		const Tile_coord tpos = anvil_obj->get_tile() + Tile_coord(0, 1, 0);
		Actor_action*    pact
				= Path_walking_actor_action::create_path(npcpos, tpos, cost);

		const Tile_coord tpos2 = trough_obj->get_tile() + Tile_coord(0, 2, 0);
		Actor_action*    pact2
				= Path_walking_actor_action::create_path(tpos, tpos2, cost);

		if (pact && pact2) {
			signed char troughframe = trough_obj->get_framenum() - 1;
			if (troughframe < 0) {
				troughframe = 0;
			}

			const int   dir      = npc->get_direction(trough_obj);
			signed char npcframe = npc->get_dir_framenum(dir, Actor::bow_frame);

			auto** a = new Actor_action*[7];
			a[0]     = pact;
			a[1]     = new Pickup_actor_action(blank_obj.get(), 250);
			a[2]     = pact2;
			a[3]     = new Frames_actor_action(&npcframe, 1, 250);
			a[4]     = new Frames_actor_action(&troughframe, 1, 0, trough_obj);
			a[5]     = new Frames_actor_action(0x00, 0, blank_obj.get());
			a[6]     = nullptr;
			npc->set_action(new Sequence_actor_action(a));
		} else {
			// Don't leak the paths
			delete pact;
			delete pact2;
			// no path found, just pick up sword blank
			npc->set_action(new Sequence_actor_action(
					new Pickup_actor_action(blank_obj.get(), 250),
					new Frames_actor_action(0, 0, blank_obj.get())));
		}

		state = done;
		break;
	}
	case done: {
		npc->add_dirty();
		const Game_object_shared tongs_obj = tongs.lock();
		if (tongs_obj) {
			tongs_obj->remove_this();
		}
		npc->add_dirty();
		state = put_sword_on_firepit;
	}
	}
	npc->start(250, 100);    // Back in queue.
}

/*
 *  Forge schedule is done.
 */

void Forge_schedule::ending(int new_type    // New schedule.
) {
	ignore_unused_variable_warning(new_type);
	// Remove any tools.
	const Game_object_shared tongs_obj = tongs.lock();
	if (tongs_obj) {
		tongs_obj->remove_this();
	}
	const Game_object_shared hammer_obj = hammer.lock();
	if (hammer_obj) {
		hammer_obj->remove_this();
	}

	Game_object*             firepit_obj = npc->find_closest(739);
	Game_object*             bellows_obj = npc->find_closest(431);
	const Game_object_shared blank_obj   = blank.lock();

	if (firepit_obj && firepit_obj->get_framenum() != 0) {
		firepit_obj->change_frame(0);
	}
	if (bellows_obj && bellows_obj->get_framenum() != 0) {
		bellows_obj->change_frame(0);
	}
	if (blank_obj && blank_obj->get_framenum() != 0) {
		blank_obj->change_frame(0);
	}
}

/*
 *  Eat without a server
 *  The original seems to pathfind to a plate, then place the food on
 *  the plate, sit down, and then eat it if the food is close enough.
 *  They only seemed to eat one food item and then sit there doing nothing.
 *  If there is no plate, they will wander around in a loiter like
 *  schedule saying 0x410-0x413 randomly until a plate becomes available.
 *  When eating, there are no barks.
 *  TODO: Add plate pathfinding
 */
Eat_schedule::Eat_schedule(Actor* n) : Schedule(n), state(find_plate) {}

/*
 *  Just went dormant.
 */
void Eat_schedule::im_dormant() {
	// Force NPCs to sit down again after cache-out/cache-in.
	// npc->change_frame(npc->get_dir_framenum(Actor::standing));
	npc->set_action(nullptr);
}

void Eat_schedule::now_what() {
	int       delay = 5000 + rand() % 12000;
	const int frnum = npc->get_framenum();
	if ((frnum & 0xf) != Actor::sit_frame) {
		if (!Sit_schedule::set_action(npc)) {    // First have to sit down.
			npc->start(250, 5000);               // Try again in a while.
		}
		return;
	}
	switch (state) {
	case eat: {
		Game_object_vector foods;    // Food nearby?
		const int          cnt = npc->find_nearby(foods, 377, 2, 0);
		if (cnt) {                          // Found?
			Game_object* food = nullptr;    // Find closest.
			int          dist = 500;
			for (auto* obj : foods) {
				const int odist = obj->distance(npc);
				if (odist < dist) {
					dist = odist;
					food = obj;
				}
			}
			if (rand() % 5 == 0 && food) {
				gwin->add_dirty(food);
				food->remove_this();
			}
		}    // loops back to itself since npc can be pushed
		break;    // out of their chair and not eat right away
	}
	case find_plate: {
		state = serve_food;
		delay = 0;
		// make sure moved plate doesn't get food sent to it
		plate = Game_object_weak();
		Game_object_vector plates;    // First look for a nearby plate.
		Game_object*       plate_obj = nullptr;
		npc->find_nearby(plates, 717, 1, 0);
		const int floor
				= npc->get_lift() / 5;    // Make sure it's on same floor.
		for (auto* plate : plates) {
			plate_obj = plate;
			if (plate_obj->get_lift() / 5 == floor) {
				break;
			} else {
				plate_obj = nullptr;
			}
		}
		plate = weak_from_obj(plate_obj);
		break;
	}
	case serve_food: {
		state                              = eat;
		const Game_object_shared plate_obj = plate.lock();
		if (!plate_obj) {
			state = find_plate;
			break;
		}
		const Tile_coord         t = plate_obj->get_tile();
		const Game_object_shared food
				= gmap->create_ireg_object(377, rand() % 31);
		food->move(t.tx, t.ty, t.tz + 1);
		break;
	}
	}
	npc->start(250, delay);
}

// TODO: This should be in a loop to remove food one at a time with a delay
void Eat_schedule::ending(int new_type) {    // new schedule type
	ignore_unused_variable_warning(new_type);
	Game_object_vector foods;    // Food nearby?
	const int          cnt = npc->find_nearby(foods, 377, 2, 0);
	if (cnt) {    // Found?
		for (auto* food : foods) {
			if (food) {
				gwin->add_dirty(food);
				food->remove_this();
			}
		}
	}
}

/*
 *  Modify goal to walk off the screen.
 */

void Walk_to_schedule::walk_off_screen(
		TileRect& screen,    // In tiles, area slightly larger than
		//   actual screen.
		Tile_coord& goal    // Modified for path offscreen.
) {
	// Destination.
	if (goal.tx >= screen.x + screen.w) {
		goal.tx = screen.x + screen.w - 1;
		goal.ty = -1;
	} else if (goal.tx < screen.x) {
		goal.tx = screen.x;
		goal.ty = -1;
	} else if (goal.ty >= screen.y + screen.h) {
		goal.ty = screen.y + screen.h - 1;
		goal.tx = -1;
	} else if (goal.ty < screen.y) {
		goal.ty = screen.y;
		goal.tx = -1;
	}
}

/*
 *  Create schedule for walking to the next schedule's destination.
 */

Walk_to_schedule::Walk_to_schedule(
		Actor*            n,
		const Tile_coord& d,            // Destination.
		int               new_sched,    // Schedule when we get there.
		int               delay         // Msecs, or -1 for random delay.
		)
		: Schedule(n), dest(d), new_schedule(new_sched), retries(0), legs(0) {
	// Delay 0-5 secs.
	first_delay = delay >= 0 ? delay : (rand() % 5000);
}

/*
 *  Schedule change for walking to another schedule destination.
 */

void Walk_to_schedule::now_what() {
	if (npc->distance(dest) <= 3) {
		// Close enough!
		npc->set_schedule_type(new_schedule);
		return;
	}
	if (legs >= 40 || retries >= 2)    // Trying too hard?  (Following
									   //   Patterson takes about 30.)
	{
		// Going to jump there.
		npc->move(dest.tx, dest.ty, dest.tz);
		// Force actor to sit down/lie down again
		npc->change_frame(npc->get_dir_framenum(Actor::standing));
		npc->set_schedule_type(new_schedule);
		return;
	}
	// Get screen rect. in tiles.
	TileRect screen = gwin->get_win_tile_rect();
	screen.enlarge(6);    // Enlarge in all dirs.
	// Might do part of it first.
	Tile_coord from = npc->get_tile();
	Tile_coord to   = dest;
	// Destination off the screen?
	if (!screen.has_world_point(to.tx, to.ty)) {
		if (!screen.has_world_point(from.tx, from.ty)) {
			// Force teleport on next tick.
			retries = 100;
			npc->start(200, 100);
			return;
		}
		// Don't walk off screen if close, or
		//   if lots of legs, indicating that
		//   Avatar is following this NPC.
		if (from.distance(to) > 80 || legs < 10) {
			// Modify 'dest'. to walk off.
			walk_off_screen(screen, to);
		}
	} else if (!screen.has_world_point(from.tx, from.ty)) {
		// Modify src. to walk from off-screen.
		// Causes NPCs to teleport to on-screen far too often
		// (e.g., Blue Boar bartenders)
		// walk_off_screen(screen, from);
		from = Tile_coord(-1, -1, -1);
	}
	blocked = Tile_coord(-1, -1, -1);
	cout << "Finding path to schedule for " << npc->get_name() << endl;
	// Create path to dest., delaying
	//   0 to 1 seconds.
	if (!npc->walk_path_to_tile(
				from, to, gwin->get_std_delay(), first_delay + rand() % 1000)) {
		// Wait 1 sec., then try again.
#ifdef DEBUG
		cout << "Failed to find path for " << npc->get_name() << endl;
#endif
		npc->walk_to_tile(dest, gwin->get_std_delay(), 1000);
		retries++;    // Failed.  Try again next tick.
	} else {          // Okay.  He's walking there.
		legs++;
		retries = 0;
	}
	first_delay = 0;
}

/*
 *  Don't go dormant when walking to a schedule.
 */

void Walk_to_schedule::im_dormant() {
	npc->set_action(nullptr);    // Prevent segfault when opening/closing doors.
	Walk_to_schedule::now_what();    // Get there by any means.
}

/*
 *  Get actual schedule (for Usecode intrinsic).
 */

int Walk_to_schedule::get_actual_type(Actor* /* npc */
) const {
	return new_schedule;
}

/*
 *  Clear out names.
 */

void Schedule_change::clear() {
	script_names.clear();
}

/*
 *  Set a schedule from a U7 'schedule.dat' entry.
 */

void Schedule_change::set4(
		const unsigned char* entry    // 4 bytes read from schedule.dat.
) {
	time                       = entry[0] & 7;
	type                       = entry[0] >> 3;
	days                       = 0x7f;    // All days of the week.
	const unsigned char schunk = entry[3];
	const unsigned char x      = entry[1];
	const unsigned char y      = entry[2];
	const int           sx     = schunk % c_num_schunks;
	const int           sy     = schunk / c_num_schunks;
	pos                        = Tile_coord(
            sx * c_tiles_per_schunk + x, sy * c_tiles_per_schunk + y, 0);
}

/*
 *  Set a schedule from an Exult 'schedule.dat' entry (vers. -1).
 */

void Schedule_change::set8(
		const unsigned char* entry    // 8 bytes read from schedule.dat.
) {
	pos.tx = little_endian::Read2(entry);
	pos.ty = little_endian::Read2(entry);
	pos.tz = Read1(entry);
	time   = Read1(entry);
	type   = Read1(entry);
	days   = Read1(entry);
}

/*
 *  Write out schedule for Exult's 'schedule.dat'.
 */

void Schedule_change::write8(
		unsigned char* entry    // 8 bytes to write to schedule.dat.
) const {
	little_endian::Write2(entry, pos.tx);
	little_endian::Write2(entry, pos.ty);         // 4
	Write1(entry, static_cast<uint8>(pos.tz));    // 5
	Write1(entry, time);                          // 6
	Write1(entry, type);                          // 7
	Write1(entry, days);                          // 8
}

/*
 *  Set a schedule.
 */

void Schedule_change::set(
		int ax, int ay, int az, unsigned char stype, unsigned char stime) {
	time = stime % 8;    // Matches original behavior.
	type = stype;
	pos  = Tile_coord(ax, ay, az);
}
