/**
 ** Party.cc - Manage the party.
 **
 ** Written: 4/8/02 - JSF
 **/
/*  Copyright (C) 2002-2022  The Exult Team
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

#include "party.h"

#include "Face_stats.h"
#include "ShortcutBar_gump.h"
#include "actors.h"
#include "dir.h"
#include "exult.h"
#include "frameseq.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "schedule.h"

#include <cstring>
#include <iostream>
#include <string>

using std::cout;
using std::endl;

/*
 *  Create.
 */

Party_manager::Party_manager()
		: party{}, party_count(0), dead_party{}, dead_party_count(0), valid{},
		  validcnt(0) {}

/*
 *  Add NPC to party.
 *
 *  Output: false if no room or already a member.
 */

bool Party_manager::add_to_party(Actor* npc    // (Should not be the Avatar.)
) {
	const size_t maxparty = party.size();
	if (!npc || party_count == maxparty || npc->is_in_party()) {
		return false;
	}
	remove_from_dead_party(npc);    // Just to be sure.
	npc->set_alignment(Actor::good);
	npc->set_party_id(party_count);
	npc->set_flag(Obj_flags::in_party);
	// We can take items.
	npc->set_flag_recursively(Obj_flags::okay_to_take);
	party[party_count++] = npc->get_npc_num();
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
	return true;
}

/*
 *  Remove party member.
 *
 *  Output: false if not found.
 */

bool Party_manager::remove_from_party(Actor* npc) {
	if (!npc) {
		return false;
	}
	const int id = npc->get_party_id();
	if (id == -1) {    // Not in party?
		return false;
	}
	const int npc_num = npc->get_npc_num();
	if (party[id] != npc_num) {
		cout << "Party mismatch!!" << endl;
		return false;
	}
	// Shift the rest down.
	for (size_t i = id + 1; i < party_count; i++) {
		Actor* npc2 = gwin->get_npc(party[i]);
		if (npc2) {
			npc2->set_party_id(i - 1);
		}
		party[i - 1] = party[i];
	}
	npc->clear_flag(Obj_flags::in_party);
	party_count--;
	party[party_count] = 0;
	npc->set_party_id(-1);
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
	return true;
}

/*
 *  Find index of NPC in dead party list.
 *
 *  Output: Index, or -1 if not found.
 */

int Party_manager::in_dead_party(Actor* npc) const {
	const int num = npc->get_npc_num();
	for (size_t i = 0; i < dead_party_count; i++) {
		if (dead_party[i] == num) {
			return i;
		}
	}
	return -1;
}

/*
 *  Add NPC to dead party list.
 *
 *  Output: false if no room or already a member.
 */

bool Party_manager::add_to_dead_party(
		Actor* npc    // (Should not be the Avatar.)
) {
	const size_t maxparty = dead_party.size();
	if (!npc || dead_party_count == maxparty || in_dead_party(npc) >= 0) {
		return false;
	}
	dead_party[dead_party_count++] = npc->get_npc_num();
	return true;
}

/*
 *  Remove NPC from dead party list.
 *
 *  Output: false if not found.
 */

bool Party_manager::remove_from_dead_party(Actor* npc) {
	if (!npc) {
		return false;
	}
	const int id = in_dead_party(npc);    // Get index.
	if (id == -1) {                       // Not in list?
		return false;
	}
	// Shift the rest down.
	for (size_t i = id + 1; i < dead_party_count; i++) {
		dead_party[i - 1] = dead_party[i];
	}
	dead_party_count--;
	dead_party[dead_party_count] = 0;
	return true;
}

/*
 *  Update party status of an NPC that has died or been resurrected.
 */

void Party_manager::update_party_status(Actor* npc) {
	if (npc->is_dead()) {    // Dead?
		// Move party members to dead list.
		if (remove_from_party(npc)) {
			add_to_dead_party(npc);
		}
	} else {    // Alive.
		if (remove_from_dead_party(npc)) {
			add_to_party(npc);
		}
	}
}

/*
 *  In case NPC's were read after usecode, set party members' id's, and
 *  move dead members into separate list.
 */

void Party_manager::link_party() {
	// avatar is a party member too
	gwin->get_main_actor()->set_flag(Obj_flags::in_party);
	// You own your own stuff.
	gwin->get_main_actor()->set_flag_recursively(Obj_flags::okay_to_take);
	const size_t tmp_party_count = party_count;
	party_count = dead_party_count = 0;
	// Now process them.
	for (size_t i = 0; i < tmp_party_count; i++) {
		if (party[i] <= 0) {    // Fix corruption.
			continue;
		}
		Actor* npc = gwin->get_npc(party[i]);
		// Shouldn't happen!
		if (!npc) {
			continue;    // Skip bad entry.
		}
		int oldid = npc->get_party_id();
		// But this has happened:
		if (oldid >= 0 && static_cast<unsigned int>(oldid) < party_count) {
			continue;    // Skip bad entry.
		}
		const int npc_num = npc->get_npc_num();
		if (npc->is_dead()) {    // Put dead in special list.
			npc->set_party_id(-1);
			if (dead_party_count >= dead_party.size()) {
				continue;
			}
			dead_party[dead_party_count++] = npc_num;
			continue;
		}
		npc->set_party_id(party_count);
		party[party_count++] = npc_num;
		// ++++This messes up places where they should wait, and should be
		// unnecessary.
		//		npc->set_schedule_type(Schedule::follow_avatar);
		// We can use all his/her items.
		npc->set_flag_recursively(Obj_flags::okay_to_take);
		npc->set_flag(Obj_flags::in_party);
	}
}

/*
 *  For each party member, this array has the party ID's (or -1) of the
 *  two member's followers, arrayed as follows:
 *          A
 *             0 1
 *            2 3 4
 *           7 5 6 8
 */
static const int followers[EXULT_PARTY_MAX + 1][2] = {
		{ 0,  1}, // These follow Avatar (ID = -1).
		{ 2,  3}, // Follow 0.
		{-1,  4}, // Follow 1.
		{ 7, -1}, // Follow 2.
		{ 5,  6}, // Follow 3.
		{-1,  8}, // Follow 4.
		{-1, -1},
        {-1, -1},
        {-1, -1}
};

/*
 *  Offsets for the follower, depending on direction (0-3, with
 *  0 = North, 1 = East, 2 = South, 3 = West).
 */
static const int left_offsets[4][2] = {
		// Follower is behind and to left.
		{-2,  2}, // North.
		{-2, -2}, // East.
		{ 2, -2}, // South.
		{ 2,  2}
};     // West.
static const int right_offsets[4][2] = {
		// Follower is behind and to right.
		{ 2,  2}, // North.
		{-2,  2}, // East.
		{-2, -2}, // South.
		{ 2, -2}
};    // West.

/*
 *  This should be called each time the Avatar takes a step while under
 *  control of the user.
 */

void Party_manager::get_followers(
		int dir    // Direction (0-7) Avatar just stepped.
) {
	validcnt = 0;    // Get party members to control.
	for (size_t i = 0; i < party_count; i++) {
		Actor* npc = gwin->get_npc(party[i]);
		if (!npc || npc->get_flag(Obj_flags::asleep)
			|| npc->get_flag(Obj_flags::paralyzed) || npc->is_dead()) {
			continue;    // Not available.
		}
		const int sched = npc->get_schedule_type();
		// Skip if in combat or set to 'wait'.
		if (sched != Schedule::combat && sched != Schedule::wait &&
			// Loiter added for SI.
			sched != Schedule::loiter
			&& !npc->in_queue()) {    // Already walking?
			valid[validcnt++] = npc;
		}
	}
	if (validcnt) {
		move_followers(gwin->get_main_actor(), -1, dir);
	}
}

/*
 *  To walk in formation, each party member will have one or two other
 *  party members who will follow him on each step.
 */

void Party_manager::move_followers(
		Actor* npc,       // Party member who just stepped.
		int    vindex,    // Index within 'valid'.
		int    dir        // Direction (0-7) he stepped in.
) const {
	ignore_unused_variable_warning(vindex);
	const int        id   = npc->get_party_id();    // (-1 if Avatar).
	const Tile_coord pos  = npc->get_tile();
	const int        lnum = followers[1 + id][0];
	const int        rnum = followers[1 + id][1];
	if (lnum == -1 && rnum == -1) {
		return;    // Nothing to do.
	}
	const int dir4 = dir / 2;    // 0-3 now.
	Actor*    lnpc = (lnum == -1 || static_cast<unsigned>(lnum) >= validcnt)
							 ? nullptr
							 : valid[lnum];
	Actor*    rnpc = (rnum == -1 || static_cast<unsigned>(rnum) >= validcnt)
							 ? nullptr
							 : valid[rnum];
	int       ldir = -1;
	int       rdir = -1;
	// Have each take a step.
	if (lnpc) {
		ldir
				= step(lnpc, npc, dir,
					   pos
							   + Tile_coord(
									   left_offsets[dir4][0],
									   left_offsets[dir4][1], 0));
	}
	if (rnpc) {
		rdir
				= step(rnpc, npc, dir,
					   pos
							   + Tile_coord(
									   right_offsets[dir4][0],
									   right_offsets[dir4][1], 0));
	}
	if (ldir >= 0 && !lnpc->is_dead()) {
		move_followers(lnpc, lnum, ldir);
	}
	if (rdir >= 0 && !rnpc->is_dead()) {
		move_followers(rnpc, rnum, rdir);
	}
}

/*
 *  Get tile to step to, given destination tile (possibly more than 1
 *  step away), and the party's direction.
 */

inline Tile_coord Get_step_tile(
		const Tile_coord& pos,     // Current pos.
		const Tile_coord& dest,    // Desired dest.
		int               dir      // Dir. party is moving (0-7).
) {
	ignore_unused_variable_warning(dir);
	int dx = dest.tx - pos.tx;
	int dy = dest.ty - pos.ty;
	if (dx < -1) {
		dx = -1;    // Limit to 1 tile.
	} else if (dx > 1) {
		dx = 1;
	}
	if (dy < -1) {
		dy = -1;
	} else if (dy > 1) {
		dy = 1;
	}
	return pos + Tile_coord(dx, dy, 0);
}

/*
 *  Find the party member occupying a given tile, starting with a given
 *  party #.
 *  Note:   Maybe it should check a rectangle of tiles someday if we want
 *      to have NPC's bigger than 1 tile.
 */

static Actor* Find_member_blocking(
		Tile_coord pos,     // Position to check.
		int        first    // Party ID to start with.
) {
	Game_window*   gwin  = Game_window::get_instance();
	Party_manager* pman  = gwin->get_party_man();
	const int      count = pman->get_count();

	for (int i = first; i < count; i++) {
		Actor* npc = gwin->get_npc(pman->get_member(i));
		assert(npc != nullptr);
		pos.tz = npc->get_lift();    // Use NPC's, since it might be up/dn
		//   by a step.
		if (npc->blocks(pos)) {
			return npc;    // Found.
		}
	}
	return nullptr;
}

/*
 *  Get the direction from a tile to NPC's position.
 */

inline int Get_dir_from(Actor* npc, Tile_coord& from) {
	const Tile_coord pos = npc->get_tile();
	return Get_direction(from.ty - pos.ty, pos.tx - from.tx);
}

/*
 *  Is the straight path to the leader clear, and less than 5 tiles?
 */

inline bool Clear_to_leader(
		Actor* npc, Actor* leader,
		Tile_coord from    // Start from here.
) {
	const Tile_coord dest = leader->get_tile();
	int              dist = leader->distance(from);
	if (dist > 4) {
		return false;    // Too far.
	}
	while (--dist) {    // Check tiles up to there.
		const int  dir  = Get_dir_from(leader, from);
		Tile_coord next = from.get_neighbor(dir);
		if (npc->is_blocked(next, &from)) {
			Actor* bnpc = Find_member_blocking(next, 0);
			if (!bnpc) {
				return false;    // Blocked by non-party-member.
			}
			next.tz = bnpc->get_lift();
		}
		from = next;
	}
	const int difftz = from.tz - dest.tz;    // Check diff. in z-coords.
	return difftz * difftz <= 1;             // Can't be more than 2.
}

/*
 *  Get a notion of 'cost' for stepping to a particular tile.
 *
 *  Output: Currently, 10000 if blocked, or (dist)**2.
 */

const int max_cost = 10000;

static int Get_cost(
		Actor*     npc,                       // NPC to take the step.
		Actor*     leader,                    // NPC he's following.
		Tile_coord to,                        // Tile to step to.
		Actor**    find_blocking = nullptr    // Returns blocking party member.
) {
	int cost = 0;
	if (find_blocking) {
		*find_blocking = nullptr;
	}
	if (npc->is_blocked(to)) {    // (To.tz is updated.)
		// Can't go there.
		if (find_blocking) {
			// Find member we can swap with.
			*find_blocking = Find_member_blocking(to, 1 + npc->get_party_id());
			if (!*find_blocking) {
				return max_cost;
			}
			to.tz = (*find_blocking)->get_lift();
			cost += 1;    // Assess one point to swap.
		} else {
			return max_cost;
		}
	}
	const Tile_coord lpos   = leader->get_tile();
	const int        difftz = to.tz - lpos.tz;
	const int    // Measure closeness.
			diffty
			= Tile_coord::delta(to.ty, lpos.ty);
	const int difftx = Tile_coord::delta(to.tx, lpos.tx);
	// Get dist**2 in x-y plane.
	const int xydist2 = diffty * diffty + difftx * difftx;
	cost += difftz * difftz + xydist2;
	if (xydist2 > 2) {    // More than 1 tile away?
		// Check 1 more tile towards leader.
		if (!Clear_to_leader(npc, leader, to)) {
			cost += 16;    // If blocked, try to avoid.
		}
	}
	return cost;
}

/*
 *  Take best step to follow the leader.
 *
 *  Output: True if a step taken.
 */

static bool Take_best_step(
		Actor* npc, Actor* leader,
		Tile_coord& pos,      // Current pos.
		int         frame,    // Frame to show.
		int         dir       // Direction we want to go.
) {
	static const int deltadir[] = {0, 1, 7, 2, 6, 3, 5};

	int        best_cost = max_cost + 8;
	Tile_coord best(-1, -1, -1);
	Actor*     best_in_way = nullptr;
	for (const int i : deltadir) {
		const int        diri = (dir + i) % 8;
		const Tile_coord to   = pos.get_neighbor(diri);
		Actor*           in_way;    // Fudge cost with diff. in dir.
		const int        cost = Get_cost(npc, leader, to, &in_way);
		if (cost < best_cost) {
			best_cost   = cost;
			best_in_way = in_way;
			best        = to;
		}
	}
	if (best_cost >= max_cost) {
		return false;
	}
	if (!best_in_way) {    // Nobody in way?
		return npc->step(best, frame);
	}
	best = best_in_way->get_tile();    // Swap positions.
	Game_object_shared npc_keep;
	Game_object_shared best_keep;
	npc->remove_this(&npc_keep);
	best_in_way->remove_this(&best_keep);
	npc->change_frame(frame);    // Appear to take a step.
	npc->move(best);
	best_in_way->move(pos);
	return true;
}

/*
 *  See if a step is reasonable.  This is the first test made.
 */

inline bool Is_step_okay(
		Actor*     npc,       // NPC to take the step.
		Actor*     leader,    // NPC he's following.
		Tile_coord to         // Tile to step to.
) {
	if (npc->is_pos_invalid()
		|| npc->is_blocked(to)) {    // (To.tz is updated.)
		return false;
	}
	int difftz = to.tz - leader->get_lift();
	difftz *= difftz;    // Deltaz squared.
	if (difftz > 4) {    // More than 2?
		return false;    // We'll want to find best dir.
	}
	// How close in XY?
	const int dist = leader->distance(to);
	if (dist == 1) {
		return difftz <= 1;    // 1 tile away, so want dz <= 1.
	}
	return Clear_to_leader(npc, leader, to);    // Couldn't take a 2nd step.
}

/*
 *  Move one follower to its destination (if possible).
 *
 *  Output: Direction (0-7) moved (or given 'dir' if we don't move).
 */

int Party_manager::step(
		Actor*            npc,
		Actor*            leader,    // Who NPC is following.
		int               dir,       // Direction we're walking (0-7).
		const Tile_coord& dest       // Destination tile.
) const {
	Tile_coord       pos = npc->get_tile();    // Current position.
	const Tile_coord to  = Get_step_tile(pos, dest, dir);
	if (to.tx == pos.tx && to.ty == pos.ty) {
		return dir;    // Not moving.
	}
	// TEST:
	if (npc->in_queue() || npc->is_moving()) {
		cout << npc->get_name() << " shouldn't be stepping!" << endl;
	}
	Frames_sequence* frames     = npc->get_frames(dir);
	int&             step_index = npc->get_step_index();
	if (!step_index) {    // First time?  Init.
		step_index = frames->find_unrotated(npc->get_framenum());
	}
	// Get next (updates step_index).
	const int frame = frames->get_next(step_index);
	// Want dz<=1, dx<=2, dy<=2.
	if (Is_step_okay(npc, leader, to) && npc->step(to, frame))
		;
	// Could have died from stepping on
	//   something.
	else if (
			npc->is_dead()
			|| !Take_best_step(
					npc, leader, pos, frame, npc->get_direction(dest))) {
		// Failed to take a step.
		cout << npc->get_name() << " failed to take a step" << endl;
		frames->decrement(step_index);
		return dir;
	}
	return Get_dir_from(npc, pos);
}
