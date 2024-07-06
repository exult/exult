/*
Copyright (C) 2000-2022 The Exult Team

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

#ifndef STATS_GUMP_H
#define STATS_GUMP_H

#include "Gump.h"
#include "misc_buttons.h"

class Actor;

/*
 *  A rectangular area showing a character's statistics:
 */
class Stats_gump : public Gump {
protected:
	Actor* get_actor() {
		return reinterpret_cast<Actor*>(container);
	}

	static short textx;        // X-coord. of where to write.
	static short texty[10];    // Y-coords.

public:
	Stats_gump(Container_game_object* cont, int initx, int inity);
	Stats_gump(
			Container_game_object* cont, int initx, int inity, int shnum,
			ShapeFile shfile);
	static Stats_gump* create(Game_object* npc_obj, int x, int y);

	// Add object.
	bool add(
			Game_object* obj, int mx = -1, int my = -1, int sx = -1,
			int sy = -1, bool dont_check = false,
			bool combine = false) override {
		ignore_unused_variable_warning(
				obj, mx, my, sx, sy, dont_check, combine);
		return false;    // Can't drop onto it.
	}

	// Paint it and its contents.
	void paint() override;

	Game_object* find_object(int mx, int my) override {
		ignore_unused_variable_warning(mx, my);
		return nullptr;
	}
};

#endif
