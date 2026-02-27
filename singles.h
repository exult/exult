/*
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

#ifndef SINGLES_H
#define SINGLES_H 1

class Game_window;
class Game_map;
class Effects_manager;
class Shape_manager;
class Usecode_machine;
class Game_clock;
class Palette;
class Gump_manager;
class Party_manager;

/*
 *  'Singletons' used throughout the code.
 *  NOTE:  For now, the implementation is in shapeid.cc.
 */
class Game_singletons {
protected:
	static Game_window*     gwin;
	static Game_map*        gmap;
	static Effects_manager* eman;
	static Shape_manager*   sman;
	static Usecode_machine* ucmachine;
	static Game_clock*      gclock;
	static Palette*         pal;
	static Gump_manager*    gumpman;
	static Party_manager*   partyman;
	friend Game_window;

public:
	static void init(Game_window* g);
};

#endif
