/*
 *  npcnear.cc - At random times, run proximity usecode functions on nearby
 * NPC's.
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

#include "npcnear.h"

#include "actors.h"
#include "cheat.h"
#include "chunks.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "paths.h"
#include "schedule.h"
#include "ucmachine.h"

#include <cstdlib>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::rand;

bool Bg_dont_wake(Game_window* gwin, Actor* npc);

/*
 *  Add an npc to the time queue.
 */

void Npc_proximity_handler::add(
		unsigned long curtime,    // Current time (msecs).
		Npc_actor*    npc,
		int           additional_ticks    // More secs. to wait.
) {
	int msecs;    // Hostile?  Wait 0-2 secs.
	if (npc->get_effective_alignment() >= Actor::evil) {
		msecs = rand() % 2000;
	} else {    // Wait between 2 & 6 secs.
		msecs = (rand() % 4000) + 2000;
	}
	unsigned long newtime = curtime + (msecs * gwin->get_std_delay() / 100);
	newtime += additional_ticks * gwin->get_std_delay();
	gwin->get_tqueue()->add(newtime, this, npc);
}

/*
 *  Remove entry for an npc.
 */

void Npc_proximity_handler::remove(Npc_actor* npc) {
	npc->clear_nearby();
	gwin->get_tqueue()->remove(this, npc);
}

/*
 *  Is this a Black Gate (Skara Brae) ghost, or Penumbra?
 */

bool Bg_dont_wake(Game_window* gwin, Actor* npc) {
	ignore_unused_variable_warning(gwin);
	int num;
	return Game::get_game_type() == BLACK_GATE
		   && (npc->get_info().has_translucency() ||
			   // Horace or Penumbra?
			   (num = npc->Actor::get_npc_num()) == 141 || num == 150);
}

/*
 *  Run proximity usecode function for the NPC now.
 */

void Npc_proximity_handler::handle_event(unsigned long curtime, uintptr udata) {
	auto* npc         = reinterpret_cast<Npc_actor*>(udata);
	int   extra_delay = 5;    // For next time.
							  // See if still on visible screen.
	const TileRect   tiles = gwin->get_win_tile_rect().enlarge(10);
	const Tile_coord t     = npc->get_tile();
	if (!tiles.has_world_point(t.tx, t.ty) ||    // No longer visible?
												 // Not on current map?
		npc->get_map() != gwin->get_map()
		|| npc->is_dead()) {    // Or no longer living?
		npc->clear_nearby();
		return;
	}
	auto sched
			= static_cast<Schedule::Schedule_types>(npc->get_schedule_type());
	// Sleep schedule?
	if (npc->get_schedule() && sched == Schedule::sleep && !npc->is_in_party()
		&& !Bg_dont_wake(gwin, npc) && npc->distance(gwin->get_main_actor()) < 6
		&& Fast_pathfinder_client::is_straight_path(npc, gwin->get_main_actor())
		&& (rand() % 3) == 0) {
		// Trick:  Stand, but stay in
		//   sleep_schedule.
		npc->get_schedule()->ending(Schedule::stand);
		Sleep_schedule::sleep_interrupted = true;
		if (npc->is_goblin()) {
			npc->say(goblin_awakened);
		} else if (npc->can_speak()) {
			npc->say(first_awakened, last_awakened);
		}
		// In 10 seconds, go back to sleep.
		npc->start(0, 10000);
		extra_delay = 11;    // And don't run Usecode while up.
	}
	add(curtime, npc, extra_delay);    // Add back for next time.
}

/*
 *  Set a time to wait for before running any usecodes.  This is to
 *  skip all the text events that would happen at the end of a conver-
 *  sation.
 */

void Npc_proximity_handler::wait(int secs    // # of seconds.
) {
	wait_until = Game::get_ticks() + 1000 * secs;
}

/*
 *  Fill a list with all the nearby NPC's.
 */

void Npc_proximity_handler::get_all(
		Actor_vector& alist    // They're appended to this.
) {
	Time_queue_iterator next(gwin->get_tqueue(), this);
	Time_sensitive*     obj;
	uintptr             data;    // NPC is the data.
	while (next(obj, data)) {
		alist.push_back(reinterpret_cast<Npc_actor*>(data));
	}
}
