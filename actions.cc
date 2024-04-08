/*
 *  actions.cc - Action controllers for actors.
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

#include "actions.h"

#include "Astar.h"
#include "Zombie.h"
#include "actors.h"
#include "cheat.h"
#include "dir.h"
#include "frameseq.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "monstinf.h"
#include "party.h"
#include "paths.h"
#include "ucmachine.h"

#include <cstdlib>
#include <cstring>

using std::cout;
using std::endl;

long Actor_action::seqcnt = 0;

/**
 *  Handle an event and check to see if we were deleted.
 *
 *  @return     Delay value from handle_event (0 if we've been deleted).
 */

int Actor_action::handle_event_safely(
		Actor* actor,
		bool&  deleted    // True returned if we're gone!
) {
	Actor_action* old_action = actor->get_action();
	const long    old_seq    = old_action->seq;
	// Do current action.
	const int delay = handle_event(actor);
	if (actor->get_action() != old_action || old_action->seq != old_seq) {
		deleted = true;    // We've been deleted.
		return 0;
	}
	deleted = false;
	return delay;
}

/**
 *  Set to walk from one point to another the dumb way.
 *
 *  @return     this, or nullptr if unsuccessful.
 */

Actor_action* Actor_action::walk_to_tile(
		Actor*            npc,    // Ignored.
		const Tile_coord& src, const Tile_coord& dest,
		int  dist,     // Ignored.
		bool ignnpc    // Ignored.
) {
	ignore_unused_variable_warning(npc, dist, ignnpc);
	auto* path = new Zombie();
	get_party  = false;
	// Set up new path.
	if (path->NewPath(src, dest, nullptr)) {
		return new Path_walking_actor_action(path);
	} else {
		delete path;
		return nullptr;
	}
}

/**
 *  Set up an action to get an actor to a location (via pathfinding), and
 *  then execute another action when he gets there.
 *
 *  @return     action.
 */

Actor_action* Actor_action::create_action_sequence(
		Actor*            actor,         // Whom to activate.
		const Tile_coord& dest,          // Where to walk to.
		Actor_action*     when_there,    // What to do when he gets there.
		bool from_off_screen,            // Have actor walk from off-screen.
		bool persistant    // Whether or not to keep retrying. Since NPCs
						   // move around, this causes them to be ignored
						   // as obstacles under many conditions.
) {
	Actor_action* act    = when_there;
	Tile_coord    actloc = actor->get_tile();
	if (from_off_screen) {
		actloc.tx = actloc.ty = -1;
	}
	if (dest != actloc) {    // Get to destination.
		// A persistence of 30 allows sitting on LB's ship from far away without
		// party members teleporting.
		const int     persistence = persistant ? 30 : 0;
		Actor_action* w
				= new Path_walking_actor_action(new Astar(), 3, persistence);
		Actor_action* w2 = w->walk_to_tile(actor, actloc, dest, 0, persistant);
		if (w2 != w) {
			delete w;
		}
		if (!w2) {    // Failed?  Teleport.
			w2 = new Move_actor_action(dest);
		}
		// And teleport if blocked walking.
		Actor_action* tel = new Move_actor_action(dest);
		// Walk there, then do whatever.
		Sequence_actor_action* seq;
		act = seq = new Sequence_actor_action(w2, tel, act);
		seq->set_speed(0);    // No delay between actions.
	}
	return act;
}

/**
 *  Null action.
 */

int Null_action::handle_event(Actor* actor) {
	ignore_unused_variable_warning(actor);
	return 0;
}

/**
 *  Create action to follow a path.
 */

Path_walking_actor_action::Path_walking_actor_action(
		PathFinder* p,         // Pathfinder, or 0 for Astar.
		int         maxblk,    // Max. retries when blocked.
		int         pers       // Keeps retrying this many times.
		)
		: path(p), max_blocked(maxblk), persistence(pers) {
	if (!path) {
		path = new Astar();
	}
	const Tile_coord src  = path->get_src();
	const Tile_coord dest = path->get_dest();
	original_dir          = static_cast<int>(
            Get_direction4(src.ty - dest.ty, dest.tx - src.tx));
}

/**
 *  Delete.
 */

Path_walking_actor_action::~Path_walking_actor_action() {
	delete path;
	delete subseq;
	subseq       = nullptr;    // (Debugging).
	original_dir = -1;
}

/**
 *  Create action for walking to given destination using Astar.
 *  Note:  This is a static method.
 *
 *  @return     Action if successful, else nullptr.
 */

Path_walking_actor_action* Path_walking_actor_action::create_path(
		const Tile_coord&        src,     // Starting position.
		const Tile_coord&        dest,    // Destination.
		const Pathfinder_client& cost     // Cost for Astar.
) {
	auto* path = new Astar();
	// Get to within 1 tile.
	if (path->NewPath(src, dest, &cost)) {
		return new Path_walking_actor_action(path);
	} else {
		delete path;
		return nullptr;
	}
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Path_walking_actor_action::handle_event(Actor* actor) {
	if (subseq) {    // Going through a door?
		const int delay = subseq->handle_event(actor);
		if (delay) {
			return delay;    // Still going.
		}
		set_subseq(nullptr);
		// He was stopped, so restore speed.
		actor->set_frame_time(speed);
		return speed;    // Come back in a moment.
	}
	Tile_coord tile;
	if (blocked) {
		if (actor->step(blocked_tile, blocked_frame)) {
			// Successful?
			if (deleted) {
				return 0;
			}
			blocked = 0;
			// He was stopped, so restore speed.
			actor->set_frame_time(speed);
			return speed;
		}
		if (deleted) {    // step() deleted us.
			return 0;
		} else if (blocked++ > max_blocked) {
			// Persistant pathfinder?
			if (!persistence) {
				return 0;    // No.
			}
			// "Tire" a bit from retrying.
			persistence--;
			Game_object* block = Game_object::find_blocking(blocked_tile);
			// Being blocked by an NPC?
			if (block && block->as_actor() != nullptr) {
				// Try to create a new path -- the old one might be blocked
				// due to (say) the previously 'non-blocking' NPC now being
				// in a blocking state.
				if (walk_to_tile(
							actor, actor->get_tile(), path->get_dest(), 0,
							true)) {
					// Got new path.
					blocked = 0;
					return speed;
				}
			}
			return 0;
		} else {    // Wait up to 1.6 secs.
			return 100 + std::rand() % 500;
		}
	}
	const int newspeed = actor->get_frame_time();    // Get time between frames.
	if (!newspeed) {
		// This may mean bumping into another NPC, then having
		// another NPC call move_aside on you then (e.g., large
		// parties trying to sit on a barge).
		// If the pathfinding NPC is supposed to be persistant, then
		// try to create a new path -- the old one might be blocked
		// due to (say) the previously 'non-blocking' NPC now being
		// in a blocking state.
		if (!persistence
			|| !walk_to_tile(
					actor, actor->get_tile(), path->get_dest(), 0, true)) {
			speed = newspeed;
			return 0;    // Not moving.
		} else {
			actor->set_frame_time(speed);
			return speed;
		}
	}
	speed = newspeed;
	bool done;    // So we'll know if this is the last.
	if (!path->GetNextStep(tile, done)
		// This happens sometimes (bedroll cancel).
		|| (tile == actor->get_tile() && !path->GetNextStep(tile, done))) {
		reached_end = true;    // Did it.
		return 0;
	}
	if (done) {    // In case we're deleted.
		reached_end = true;
	}
	const Tile_coord cur    = actor->get_tile();
	const int        newdir = static_cast<int>(
            Get_direction4(cur.ty - tile.ty, tile.tx - cur.tx));
	Frames_sequence* frames     = actor->get_frames(newdir);
	int&             step_index = actor->get_step_index();
	if (!step_index) {    // First time?  Init.
		step_index = frames->find_unrotated(actor->get_framenum());
	}
	// Get next (updates step_index).
	const int frame     = frames->get_next(step_index);
	const int cur_speed = speed;    // Step() might delete us!
	if (from_offscreen) {           // Teleport to 1st spot.
		from_offscreen = false;
		actor->move(tile.tx, tile.ty, tile.tz);
		return cur_speed;
	} else if (actor->step(tile, frame)) {    // Successful.
		if (deleted) {
			return 0;
		}
		if (get_party) {    // MUST be the Avatar.
			Game_window* gwin = Game_window::get_instance();
			gwin->get_party_man()->get_followers(newdir);
			if (done) {
				gwin->get_main_actor()->get_followers();
			}
		}
		if (done) {    // Was this the last step?
			return 0;
		}
		return cur_speed;
	}
	if (deleted) {
		return 0;
	}
	reached_end = false;
	frames->decrement(step_index);    // We didn't take the step.
	// Blocked by a door?
	if (actor->distance(tile) <= 2 && !cheat.in_map_editor()
		&&    // And NOT map-editing?
		actor->is_sentient()) {
		Game_object* door = Game_object::find_door(tile);
		if (door != nullptr && door->is_closed_door() &&
			// Make sure it's not locked!
			door->get_framenum() % 4 < 2)

		// Try to open it.
		{
			if (open_door(actor, door)) {
				return speed;
			}
		}
	}
	if (!max_blocked ||           // No retries allowed?
		actor->is_dormant()) {    // Or actor off-screen?
		return 0;
	}
	blocked       = 1;
	blocked_tile  = tile;
	blocked_frame = frame;
	return 100 + std::rand() % 500;    // Wait .1 to .6 seconds.
}

/**
 *  Open door that's blocking the NPC, and set action to walk past and
 *  close it.
 *
 *  @return     true if successful.
 */

bool Path_walking_actor_action::open_door(Actor* actor, Game_object* door) {
	const Tile_coord cur = actor->get_tile();
	// Get door's footprint in tiles.
	const TileRect foot = door->get_footprint();
	// Open it, but kludge quality to
	//   avoid unwanted usecode.
	const int savequal = door->get_quality();
	door->set_quality(0);
	door->activate();
	door->set_quality(savequal);
	Tile_coord past;    // Tile on other side of door.
	past.tz = cur.tz;
	int dir;                  // Get dir to face door afterwards.
	if (foot.w > foot.h) {    // Horizontal?
		past.tx = foot.x + foot.w / 2;
		if (cur.ty <= foot.y) {    // N. of door?
			past.ty = foot.y + foot.h;
			dir     = 0;
		} else {    // S. of door?
			past.ty = foot.y - 1;
			dir     = 4;
		}
	} else {    // Vertical.
		past.ty = foot.y + foot.h / 2;
		if (cur.tx <= foot.x) {    // W. of door?
			past.tx = foot.x + foot.w;
			dir     = 6;
		} else {    // E. of door?
			past.tx = foot.x - 1;
			dir     = 2;
		}
	}
	Map_chunk::find_spot(past, 1, actor, 1);
	if (past.tx != -1) {    // Succeeded.  Walk past and close it.
#ifdef DEBUG
		cout << "Path_walking_actor_action::open_door()" << endl;
#endif
		const std::array frames{
				static_cast<signed char>(
						actor->get_dir_framenum(dir, Actor::standing)),
				static_cast<signed char>(
						actor->get_dir_framenum(dir, Actor::ready_frame)),
		};
		signed char standframe = frames[0];
		set_subseq(create_action_sequence(
				actor, past,
				new Sequence_actor_action(
						new Frames_actor_action(frames.data(), frames.size()),
						new Activate_actor_action(door),
						new Frames_actor_action(&standframe, 1))));
		return true;
	}
	return false;
}

/**
 *  Stopped moving.
 */

void Path_walking_actor_action::stop(Actor* actor) {
	// Don't set slimes.
	if (!actor->get_info().has_strange_movement() && actor->can_act()) {
		// ++++For now, just use original dir.
		Frames_sequence* frames = actor->get_frames(original_dir);
		actor->change_frame(frames->get_resting());
	}
}

/**
 *  Set to walk from one point to another, using the same pathfinder.
 *
 *  @return     this, or nullptr if unsuccessful.
 */

Actor_action* Path_walking_actor_action::walk_to_tile(
		Actor*            npc,
		const Tile_coord& src,     // tx=-1 or ty=-1 means don't care.
		const Tile_coord& dest,    // Same here.
		int               dist,    // Distance to get to within dest.
		bool ignnpc    // If pathfinder should ignore NPCs in many cases.
) {
	blocked        = 0;        // Clear 'blocked' count.
	reached_end    = false;    // Starting new path.
	get_party      = false;
	from_offscreen = false;
	//+++++Should dist be used below??:
	// Set up new path.
	// Don't care about 1 coord.?
	if (dest.tx == -1 || dest.ty == -1) {
		if (dest.tx == dest.ty) {    // Completely off-screen?
			Offscreen_pathfinder_client cost(npc, ignnpc);
			if (!path->NewPath(src, dest, &cost)) {
				return nullptr;
			}
		} else {
			Onecoord_pathfinder_client cost(npc, ignnpc);
			if (!path->NewPath(src, dest, &cost)) {
				return nullptr;
			}
		}
	}
	// How about from source?
	else if (src.tx == -1 || src.ty == -1) {
		// Figure path in opposite dir.
		if (src.tx == src.ty) {    // Both -1?
			// Aim from NPC's current pos.
			Offscreen_pathfinder_client cost(npc, npc->get_tile(), ignnpc);
			if (!path->NewPath(dest, src, &cost)) {
				return nullptr;
			}
		} else {
			Onecoord_pathfinder_client cost(npc, ignnpc);
			if (!path->NewPath(dest, src, &cost)) {
				return nullptr;
			}
		}
		from_offscreen = true;
		// Set to go backwards.
		if (!path->set_backwards()) {
			return nullptr;
		}
	} else {
		Actor_pathfinder_client cost(npc, dist, ignnpc);
		if (!path->NewPath(src, dest, &cost)) {
			return nullptr;
		}
	}
	// Reset direction (but not index).
	original_dir = static_cast<int>(
			Get_direction4(src.ty - dest.ty, dest.tx - src.tx));
	return this;
}

/**
 *  Return current destination.
 *
 *  @return     0 if none.
 */

bool Path_walking_actor_action::get_dest(Tile_coord& dest    // Returned here.
) const {
	dest = path->get_dest();
	return true;
}

/**
 *  Following an Astar path?
 */

bool Path_walking_actor_action::following_smart_path() const {
	return path != nullptr && path->following_smart_path();
}

/**
 *  Create action to follow a path towards another object.
 */

Approach_actor_action::Approach_actor_action(
		PathFinder*  p,          // Path to follow.
		Game_object* d,          // Destination object.
		int          gdist,      // Stop when this close to dest.
		bool         for_proj    // Check for projectile path.
		)
		: Path_walking_actor_action(p, 0),    // (Stop if blocked.)
		  dest_obj(weak_from_obj(d)), goal_dist(gdist),
		  orig_dest_pos(d->get_tile()), cur_step(0), for_projectile(for_proj) {
	// Get length of path.
	const int nsteps = path->get_num_steps();
	// cout << "Approach nsteps is " << nsteps << "." << endl;
	if (nsteps >= 6) {    // (May have to play with this).
		check_step = nsteps > 18 ? 9 : nsteps / 2;
	} else {
		check_step = 10000;
	}
}

/**
 *  Create action for walking towards a given (moving) object using Astar.
 *  Note:  This is a static method.
 *
 *  @return     Action if successful, else nullptr.
 */

Approach_actor_action* Approach_actor_action::create_path(
		const Tile_coord&  src,      // Starting position.
		Game_object*       dest,     // Destination.
		int                gdist,    // Stop when this close to dest.
		Pathfinder_client& cost      // Cost for Astar.
) {
	auto* path = new Astar();
	// Get to within 1 tile.
	if (path->NewPath(src, dest->get_tile(), &cost)) {
		return new Approach_actor_action(path, dest, gdist);
	} else {
		delete path;
		return nullptr;
	}
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Approach_actor_action::handle_event(Actor* actor) {
	const int delay = Path_walking_actor_action::handle_event(actor);
	const Game_object_shared dest_ptr = dest_obj.lock();
	if (!dest_ptr || !delay || deleted) {    // Done or blocked.
		return 0;
	}
	// Close enough?
	if (goal_dist >= 0 && actor->distance(dest_ptr.get()) <= goal_dist) {
		return 0;
	}
	if (++cur_step == check_step) {    // Time to check.
#ifdef DEBUG
		cout << actor->get_name() << " approach: Dist. to dest is "
			 << actor->distance(dest_ptr.get()) << endl;
#endif
		if (dest_ptr->distance(orig_dest_pos) > 2) {
			return 0;    // Moved too much, so stop.
		}
		if (for_projectile
			&& Fast_pathfinder_client::is_straight_path(
					actor, dest_ptr.get())) {
			return 0;    // Can fire projectile.
		}
		// Figure next check.
		const int nsteps = path->get_num_steps();
		if (nsteps >= 6) {
			// Try checking more often.
			check_step += 3;
		}
	}
	return delay;
}

/**
 *  Create if-then-else path.
 */

If_else_path_actor_action::If_else_path_actor_action(
		Actor* actor, const Tile_coord& dest, Actor_action* s, Actor_action* f)
		: Path_walking_actor_action(nullptr, 6),    // Maxblk = 6.
		  succeeded(false), failed(false), done(false), success(s), failure(f) {
	if (!walk_to_tile(actor, actor->get_tile(), dest)) {
		done = failed = true;
	}
}

/**
 *  Delete.
 */

If_else_path_actor_action::~If_else_path_actor_action() {
	delete success;
	delete failure;
}

/**
 *  Set failure action.
 */

void If_else_path_actor_action::set_failure(Actor_action* f) {
	delete failure;
	failure = f;
	done    = false;    // So it gets executed.
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int If_else_path_actor_action::handle_event(Actor* actor) {
	if (done) {
		return 0;    // Shouldn't really get here.
	}
	bool del;
	int  delay;
	if (succeeded) {    // Doing the success action?
		if ((delay = success->handle_event_safely(actor, del)) == 0 && !del) {
			done = true;
		}
		return delay;
	} else if (failed) {
		if ((delay = failure->handle_event_safely(actor, del)) == 0 && !del) {
			done = true;
		}
		return delay;
	}
	delay = Path_walking_actor_action::handle_event(actor);
	if (delay) {
		return delay;
	}
	if (deleted) {
		return 0;
	}
	if (!reached_end) {
		// Didn't get there.
		if (failure) {
			failed = true;
#ifdef DEBUG
			cout << "Executing 'failure' path usecode" << endl;
#endif
			delay = failure->handle_event_safely(actor, del);
			if (del) {    // Are we gone?
				return 0;
			}
		}
	} else {    // Success.
		if (success) {
			succeeded = true;
			delay     = success->handle_event_safely(actor, del);
			if (del) {    // Are we gone?
				return 0;
			}
		}
	}
	if (!delay) {
		done = true;    // All done now.
	}
	return delay;
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Move_actor_action::handle_event(Actor* actor) {
	if (dest.tx < 0 || actor->get_tile() == dest) {
		return 0;    // Done.
	}
	actor->move(dest);    // Zip right there.
	Game_window* gwin = Game_window::get_instance();
	if (actor == gwin->get_main_actor()) {
		// Teleported Avatar?
		gwin->center_view(dest);
	}
	dest.tx = -1;    // Set to stop.
	return 100;      // Wait 1/10 sec.
}

/**
 *  Activate an actor at a given time.
 */
Activate_actor_action::Activate_actor_action(Game_object* o)
		: obj(weak_from_obj(o)) {}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Activate_actor_action::handle_event(Actor* actor) {
	ignore_unused_variable_warning(actor);
	const Game_object_shared obj_ptr = obj.lock();
	if (obj_ptr) {
		obj_ptr->activate();
	}
	return 0;    // That's all.
}

/**
 *  Create usecode action.
 */
Usecode_actor_action::Usecode_actor_action(int f, Game_object* i, int ev)
		: fun(f), item(weak_from_obj(i)), eventid(ev) {}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Usecode_actor_action::handle_event(Actor* actor) {
	ignore_unused_variable_warning(actor);
	Game_window*             gwin     = Game_window::get_instance();
	const Game_object_shared item_ptr = item.lock();
	if (item_ptr) {
		gwin->get_usecode()->call_usecode(
				fun, item_ptr.get(),
				static_cast<Usecode_machine::Usecode_events>(eventid));
		gwin->set_all_dirty();    // Clean up screen.
	}
	return 0;    // That's all.
}

/**
 *  Create sequence of frames.
 */

Frames_actor_action::Frames_actor_action(
		const signed char* f,      // Frames.  -1 means don't change.
		int                c,      // Count.
		int                spd,    // Frame delay in 1/1000 secs.
		Game_object*       o)
		: index(0), speed(spd), obj(weak_from_obj(o)) {
	frames.resize(c);
	std::copy_n(f, c, frames.begin());
	use_actor = (o == nullptr);
}

/**
 *  Create sequence of frames.
 */

Frames_actor_action::Frames_actor_action(
		signed char  f,      // Frames.  -1 means don't change.
		int          spd,    // Frame delay in 1/1000 secs.
		Game_object* o)
		: frames{f}, index(0), speed(spd), obj(weak_from_obj(o)) {
	use_actor = (o == nullptr);
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Frames_actor_action::handle_event(Actor* actor) {
	const Game_object_shared o = obj.lock();
	if (index == frames.size() || (!o && !use_actor)) {
		return 0;    // Done.
	}
	const int frnum = frames[index++];    // Get frame.
	if (frnum >= 0) {
		if (o) {
			o->change_frame(frnum);
		} else {
			actor->change_frame(frnum);
		}
	}
	return speed;
}

/**
 *  Create a sequence with up to 4 actions.
 */

Sequence_actor_action::Sequence_actor_action(
		Actor_action* a0,    // (These will be deleted when done.)
		Actor_action* a1, Actor_action* a2, Actor_action* a3)
		: index(0), speed(100) {
	// Create 0-delimited list.
	actions = new Actor_action* [5] {
		a0, a1, a2, a3, nullptr,
	};
}

/**
 *  Delete.
 */

Sequence_actor_action::~Sequence_actor_action() {
	for (int i = 0; actions[i]; i++) {
		delete actions[i];
	}
	delete[] actions;
}

/**
 *  Handle a time event.
 *
 *  @return     0 if done with this action, else delay for next frame.
 */

int Sequence_actor_action::handle_event(Actor* actor) {
	if (!actions[index]) {    // Done?
		return 0;
	}
	// Do current action.
	bool deleted;
	int  delay = actions[index]->handle_event_safely(actor, deleted);
	if (deleted) {
		return 0;    // We've been deleted!
	}
	if (!delay) {
		index++;         // That one's done now.
		if (!speed) {    // Immediately?  Run with next.
			return handle_event(actor);
		}
		delay = speed;
	}
	return delay;
}

/**
 *  Create object animator.
 */
Object_animate_actor_action::Object_animate_actor_action(
		Game_object* o,
		int          cy,    // # of cycles.
		int          spd    // Time between frames.
		)
		: obj(weak_from_obj(o)), cycles(cy), speed(spd) {
	nframes = o->get_num_frames();
}

Object_animate_actor_action::Object_animate_actor_action(
		Game_object* o, int nfr, int cy, int spd)
		: obj(weak_from_obj(o)), nframes(nfr), cycles(cy), speed(spd) {}

/**
 *  Handle tick of the clock.
 */

int Object_animate_actor_action::handle_event(Actor* actor) {
	const Game_object_shared obj_ptr = obj.lock();
	ignore_unused_variable_warning(actor);
	if (!obj_ptr || !cycles) {
		return 0;
	}
	const int frnum = (obj_ptr->get_framenum() + 1) % nframes;
	if (!frnum) {    // New cycle?
		--cycles;
	}
	obj_ptr->change_frame(frnum);
	return cycles ? speed : 0;
}

/**
 *  Pick up/put down an object.
 */
Pickup_actor_action::Pickup_actor_action(Game_object* o, int spd, bool del)
		: obj(weak_from_obj(o)), pickup(1), speed(spd), cnt(0),
		  objpos(o->get_tile()), dir(0), temp(false), to_del(del) {}

// To put down an object:
Pickup_actor_action::Pickup_actor_action(
		Game_object* o, const Tile_coord& opos, int spd, bool t)
		: obj(weak_from_obj(o)), pickup(0), speed(spd), cnt(0), objpos(opos),
		  dir(0), temp(t), to_del(false) {}

/**
 *  Pick up an item (or put it down).
 */

int Pickup_actor_action::handle_event(Actor* actor) {
	Game_window*             gwin = Game_window::get_instance();
	Game_object_shared       keep;
	const Game_object_shared obj_ptr = obj.lock();
	int                      frnum   = -1;
	if (!obj_ptr) {
		return 0;    // It's gone!  So we're done.
	}
	switch (cnt) {
	case 0:    // Face object.
		dir   = actor->get_direction(objpos);
		frnum = actor->get_dir_framenum(dir, Actor::standing);
		cnt++;
		break;
	case 1: {    // Bend down.
		const int tz = pickup ? obj_ptr->get_lift() : objpos.tz;
		frnum        = (tz >= actor->get_lift() + 2)
							   ? ((rand() % 2) ? Actor::reach1_frame
											   : Actor::reach2_frame)
							   : Actor::bow_frame;
		frnum        = actor->get_dir_framenum(dir, frnum);
		cnt++;
		if (pickup) {
			if (actor->distance(obj_ptr.get()) > 8) {
				// No longer nearby.
				break;
			}
			gwin->add_dirty(obj_ptr.get());
			if (to_del) {
				obj_ptr->remove_this();    // Delete it.
			} else {
				obj_ptr->remove_this(&keep);
				actor->add(obj_ptr.get(), true);
			}
		} else {
			obj_ptr->remove_this(&keep);
			obj_ptr->move(objpos);
			if (temp) {
				obj_ptr->set_flag(Obj_flags::is_temporary);
			}
			gwin->add_dirty(obj_ptr.get());
		}
	} break;
	case 2:
		frnum = actor->get_dir_framenum(dir, Actor::standing);
		cnt++;
		break;
	default:
		return 0;    // Done.
	}
	actor->change_frame(frnum);
	return speed;
}

/**
 *  Action to turn towards a position or an object.
 */

Face_pos_actor_action::Face_pos_actor_action(const Tile_coord& p, int spd)
		: speed(spd), pos(p) {}

Face_pos_actor_action::Face_pos_actor_action(Game_object* o, int spd)
		: speed(spd), pos(o->get_tile()) {}

/**
 *  Just turn to face a tile.
 */

int Face_pos_actor_action::handle_event(Actor* actor) {
	const int dir   = actor->get_direction(pos);
	const int frnum = actor->get_dir_framenum(dir, Actor::standing);
	if (actor->get_framenum() == frnum) {
		return 0;    // There.
	}
	actor->change_frame(frnum);
	return speed;
}

/**
 *  Action to change the shape, frame and quality of an object.
 */

Change_actor_action::Change_actor_action(Game_object* o, int sh, int fr, int ql)
		: obj(weak_from_obj(o)), shnum(sh), frnum(fr), qual(ql) {}

/**
 *  Just change the object.
 */

int Change_actor_action::handle_event(Actor* actor) {
	ignore_unused_variable_warning(actor);
	Game_window*             gwin    = Game_window::get_instance();
	const Game_object_shared obj_ptr = obj.lock();
	if (obj_ptr) {
		gwin->add_dirty(obj_ptr.get());
		obj_ptr->set_shape(shnum, frnum);
		obj_ptr->set_quality(qual);
		gwin->add_dirty(obj_ptr.get());
	}
	return 0;
}
