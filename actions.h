/*
 *  actions.h - Action controllers for actors.
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

#ifndef ACTIONS_H
#define ACTIONS_H 1

#include "ignore_unused_variable_warning.h"
#include "tiles.h"

#include <memory>
#include <vector>

class Actor;
class Game_object;
class Tile_coord;
class PathFinder;
class Pathfinder_client;
class Path_walking_actor_action;
class If_else_path_actor_action;
using Game_object_weak = std::weak_ptr<Game_object>;

/*
 *  This class controls the current actions of an actor:
 */
class Actor_action {
	static long seqcnt;    // Sequence # to check for deletion.
protected:
	bool get_party = false;    // At each step (of the Avatar), have
	//   the party follow.
	long seq;    // 'unique' sequence #.
public:
	Actor_action() {
		seq = ++seqcnt;
	}

	virtual ~Actor_action() = default;

	void set_get_party(bool tf = true) {
		ignore_unused_variable_warning(tf);
		get_party = true;
	}

	int handle_event_safely(Actor* actor, bool& deleted);
	// Handle time event.
	virtual int handle_event(Actor* actor) = 0;

	virtual void stop(Actor* actor) {    // Stop moving.
		ignore_unused_variable_warning(actor);
	}

	// Set simple path to destination.
	virtual Actor_action* walk_to_tile(
			Actor* npc, const Tile_coord& src, const Tile_coord& dest,
			int dist = 0, bool ignnpc = false);
	// Set action to walk to dest, then
	//   exec. another action when there.
	static Actor_action* create_action_sequence(
			Actor* actor, const Tile_coord& dest, Actor_action* when_there,
			bool from_off_screen = false, bool persistant = false);

	// Get destination, or ret. 0.
	virtual bool get_dest(Tile_coord& dest) const {
		ignore_unused_variable_warning(dest);
		return false;
	}

	// Check for Astar.
	virtual bool following_smart_path() const {
		return false;
	}

	virtual If_else_path_actor_action* as_usecode_path() {
		return nullptr;
	}

	virtual int get_speed() const {
		return 0;
	}

	virtual Actor_action* kill() {
		delete this;
		return nullptr;
	}
};

/*
 *  A null action just returns 0 the first time.
 */
class Null_action : public Actor_action {
public:
	Null_action() = default;
	int handle_event(Actor* actor) override;
};

/*
 *  Follow a path.
 */
class Path_walking_actor_action : public Actor_action {
protected:
	bool        reached_end = false;    // Reached end of path.
	PathFinder* path;                   // Allocated pathfinder.
	bool        deleted = false;        // True if the action has been killed.
private:
	int              original_dir;                // From src. to dest. (0-7).
	int              speed          = 0;          // Time between frames.
	bool             from_offscreen = false;      // Walking from offscreen.
	Actor_action*    subseq         = nullptr;    // For opening doors.
	Game_object_weak door_obj;                    // Store the door object
	bool             handling_door          = false;
	bool             door_sequence_complete = false;
	unsigned char    blocked                = 0;    // Blocked-tile retries.
	unsigned char    max_blocked;                   // Try this many times.
	unsigned char    blocked_frame = 0;             // Frame for blocked tile.
	unsigned char    persistence;
	Tile_coord       blocked_tile;    // Tile to retry.
	
	void set_subseq(Actor_action* sub) {
		delete subseq;
		subseq = sub;
	}

public:
	Path_walking_actor_action(
			PathFinder* p = nullptr, int maxblk = 3, int pers = 0);
	~Path_walking_actor_action() override;
	static Path_walking_actor_action* create_path(
			const Tile_coord& src, const Tile_coord& dest,
			const Pathfinder_client& cost);
	// Handle time event.
	int  handle_event(Actor* actor) override;
	bool open_door(Actor* actor, Game_object* door);
	void stop(Actor* actor) override;    // Stop moving.
	// Set simple path to destination.
	Actor_action* walk_to_tile(
			Actor* npc, const Tile_coord& src, const Tile_coord& dest,
			int dist = 0, bool ignnpc = false) override;
	// Get destination, or ret. 0.
	bool get_dest(Tile_coord& dest) const override;
	// Check for Astar.
	bool following_smart_path() const override;

	int get_speed() const override {
		return speed;
	}

	Actor_action* kill() override {
		deleted = true;
		return this;
	}
};

/*
 *  Follow a path to approach a given object, and stop half-way if it
 *  moved.
 */
class Approach_actor_action : public Path_walking_actor_action {
	Game_object_weak dest_obj;          // Destination object.
	int              goal_dist;         // Stop if within this distance.
	Tile_coord       orig_dest_pos;     // Dest_obj's pos. when we start.
	int              cur_step;          // Count steps.
	int              check_step;        // Check at this step.
	bool             for_projectile;    // Check for proj. path.
public:
	Approach_actor_action(
			PathFinder* p, Game_object* d, int gdist = -1,
			bool for_proj = false);
	static Approach_actor_action* create_path(
			const Tile_coord& src, Game_object* dest, int gdist,
			Pathfinder_client& cost);
	// Handle time event.
	int handle_event(Actor* actor) override;
};

/*
 *  Follow a path and execute one action if successful, another if
 *  failed.
 */
class If_else_path_actor_action : public Path_walking_actor_action {
	bool          succeeded, failed, done;
	Actor_action *success, *failure;

public:
	If_else_path_actor_action(
			Actor* actor, const Tile_coord& dest, Actor_action* s,
			Actor_action* f = nullptr);
	~If_else_path_actor_action() override;
	void set_failure(Actor_action* f);

	bool done_and_failed() const {    // Happens if no path found in ctor.
		return done && failed;
	}

	bool is_done() const {
		return done;
	}

	// Handle time event.
	int handle_event(Actor* actor) override;

	If_else_path_actor_action* as_usecode_path() override {
		return this;
	}
};

/*
 *  Just move (i.e. teleport) to a desired location.
 */
class Move_actor_action : public Actor_action {
	Tile_coord dest;    // Where to go.
public:
	Move_actor_action(const Tile_coord& d) : dest(d) {}

	// Handle time event.
	int handle_event(Actor* actor) override;
};

/*
 *  Activate an object.
 */
class Activate_actor_action : public Actor_action {
	Game_object_weak obj;

public:
	Activate_actor_action(Game_object* o);
	// Handle time event.
	int handle_event(Actor* actor) override;
};

/*
 *  Go through a series of frames.
 */
class Frames_actor_action : public Actor_action {
	std::vector<signed char> frames;    // List to go through (a -1 means to
	//   leave frame alone.)
	size_t           index;    // Index for next.
	int              speed;    // Frame delay in 1/1000 secs.
	Game_object_weak obj;      // Object to animate
	bool             use_actor;

public:
	Frames_actor_action(
			const signed char* f, int c, int spd = 200,
			Game_object* o = nullptr);

	Frames_actor_action(signed char f, int spd = 200, Game_object* o = nullptr);

	// Handle time event.
	int handle_event(Actor* actor) override;

	size_t get_index() const {
		return index;
	}

	int get_speed() const override {
		return speed;
	}
};

/*
 *  Call a usecode function.
 */
class Usecode_actor_action : public Actor_action {
	int              fun;     // Fun. #.
	Game_object_weak item;    // Call it on this item.
	int              eventid;

public:
	Usecode_actor_action(int f, Game_object* i, int ev);
	// Handle time event.
	int handle_event(Actor* actor) override;
};

/*
 *  Do a sequence of actions.
 */
class Sequence_actor_action : public Actor_action {
	Actor_action** actions;    // List of actions, ending with null.
	int            index;      // Index into list.
	int            speed;      // Frame delay in 1/1000 secs. between
							   //   actions.
public:
	// Create with allocated list.
	Sequence_actor_action(Actor_action** act, int spd = 100)
			: actions(act), index(0), speed(spd) {}

	// Create with up to 4.
	Sequence_actor_action(
			Actor_action* a0, Actor_action* a1, Actor_action* a2 = nullptr,
			Actor_action* a3 = nullptr);

	int get_speed() const override {
		return speed;
	}

	void set_speed(int spd) {
		speed = spd;
	}

	~Sequence_actor_action() override;
	// Handle time event.
	int handle_event(Actor* actor) override;
};

//
//	The below could perhaps go into a highact.h file.
//

/*
 *  Rotate through an object's frames.
 */
class Object_animate_actor_action : public Actor_action {
	Game_object_weak obj;
	int              nframes;    // # of frames.
	int              cycles;     // # of cycles to do.
	int              speed;      // Time between frames.
public:
	Object_animate_actor_action(Game_object* o, int cy, int spd);
	Object_animate_actor_action(Game_object* o, int nframes, int cy, int spd);
	// Handle time event.
	int handle_event(Actor* actor) override;

	int get_speed() const override {
		return speed;
	}
};

/*
 *  Action to pick up an item or put it down.
 */

class Pickup_actor_action : public Actor_action {
	Game_object_weak obj;       // What to pick up/put down.
	int              pickup;    // 1 to pick up, 0 to put down.
	int              speed;     // Time between frames.
	int              cnt;       // 0, 1, 2.
	Tile_coord       objpos;    // Where to put it.
	int              dir;       // Direction to face.
	bool             temp;      // True to make object temporary on drop.
	bool             to_del;    // Delete after picking up object.
public:
	// To pick up an object:
	Pickup_actor_action(Game_object* o, int spd, bool del = false);
	// To put down an object:
	Pickup_actor_action(
			Game_object* o, const Tile_coord& opos, int spd, bool t = false);
	int handle_event(Actor* actor) override;

	int get_speed() const override {
		return speed;
	}
};

/*
 *  Action to turn towards an object or spot.
 */

class Face_pos_actor_action : public Actor_action {
	int        speed;    // Time between frames.
	Tile_coord pos;      // Where to put it.
public:
	Face_pos_actor_action(const Tile_coord& p, int spd);
	// To pick up an object:
	Face_pos_actor_action(Game_object* o, int spd);
	int handle_event(Actor* actor) override;

	int get_speed() const override {
		return speed;
	}
};

/*
 *  Action to modify another actor.
 */

class Change_actor_action : public Actor_action {
	Game_object_weak obj;                   // What to modify.
	int              shnum, frnum, qual;    // New shape, frame and quality.
public:
	Change_actor_action(Game_object* o, int sh, int fr, int ql);
	int handle_event(Actor* actor) override;
};

#endif /* INCL_ACTIONS */
