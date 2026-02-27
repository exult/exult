/*
 *  npcnear.h - At random times, run proximity usecode functions on nearby
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

#ifndef NPCNEAR_H
#define NPCNEAR_H 1

#include "tqueue.h"

#include <vector>

class Game_window;
class Npc_actor;
class Actor;

/*
 *  This class keeps track of NPC's nearby, and randomly runs the Usecode
 *  proximity functions for them.
 */
class Npc_proximity_handler : public Time_sensitive {
	Game_window*  gwin;
	unsigned long wait_until;    // Skip running usecodes until past.
public:
	Npc_proximity_handler(Game_window* gw) : gwin(gw) {
		wait_until = 0;
	}

	// Add npc to queue.
	void add(unsigned long curtime, Npc_actor* npc, int additional_ticks = 0);
	void remove(Npc_actor* npc);    // Remove.
	// Run usecode function.
	void handle_event(unsigned long curtime, uintptr udata) override;
	// Wait before running more funs.
	void wait(int secs);
	void get_all(std::vector<Actor*>& list);    // Fill list with nearby NPC's.
};

#endif /* INCL_NPCNEAR */
