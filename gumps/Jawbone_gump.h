/*
Copyright (C) 2001-2013 The Exult Team

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

#ifndef JAWBONE_GUMP_H
#define JAWBONE_GUMP_H

#include "Gump.h"

class Game_object;
class Container_game_object;
class Game_window;
class Jawbone_object;

class Jawbone_gump : public Gump {
public:
	Jawbone_gump(Jawbone_object *cont, int initx, int inity);

	// Find the object the mouse is over
	Game_object *find_object(int mx, int my) override;

	// Add object.
	bool add(Game_object *obj, int mx = -1, int my = -1,
	        int sx = -1, int sy = -1, bool dont_check = false,
	        bool combine = false) override;

	// Paint it and its contents.
	void paint() override;

private:
	void set_to_spot(Game_object *obj, int sx, int sy);
	void paint_tooth(int index);

	bool on_tooth(int sx, int sy, int index); // is spot on tooth?

	Jawbone_object *jawbone;

};

#endif
