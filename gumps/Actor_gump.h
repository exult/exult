/*
Copyright (C) 2000 The Exult Team

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

#ifndef ACTOR_GUMP_H
#define ACTOR_GUMP_H

#include "Gump.h"

class Heart_button;
class Disk_button;
class Combat_button;
class Halo_button;
class Combat_mode_button;

/*
 *  A rectangular area showing a character and his/her possessions:
 */
class Actor_gump : public Gump {
	UNREPLICATABLE_CLASS(Actor_gump)

protected:
	static short coords[24];    // Coords. of where to draw things,
	//   indexed by spot # (0-11).
	static int spotx(int i) {
		return coords[2 * i];
	}
	static int spoty(int i) {
		return coords[2 * i + 1];
	}
	// Find index of closest spot.
	int find_closest(int mx, int my, int only_empty = 0);
	void set_to_spot(Game_object *obj, int index);
	static short diskx, disky;  // Where to show 'diskette' button.
	static short heartx, hearty;    // Where to show 'stats' button.
	static short combatx, combaty;  // Combat button.
	static short halox, haloy;  // "Protected" halo.
	static short cmodex, cmodey;    // Combat mode.

public:
	Actor_gump(Container_game_object *cont, int initx, int inity,
	           int shnum);
	// Add object.
	bool add(Game_object *obj, int mx = -1, int my = -1,
	        int sx = -1, int sy = -1, bool dont_check = false,
	        bool combine = false) override;
	// Paint it and its contents.
	void paint() override;

	Container_game_object *find_actor(int mx, int my) override;
};

#endif
