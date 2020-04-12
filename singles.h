/*
 *  Copyright (C) 2000-2013  The Exult Team
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
#define SINGLES_H   1

/*
 *  'Singletons' used throughout the code.
 *  NOTE:  For now, the implementation is in shapeid.cc.
 */
class Game_singletons {
protected:
	static class Game_window *gwin;
	static class Game_map *gmap;
	static class Effects_manager *eman;
	static class Shape_manager *sman;
	static class Usecode_machine *ucmachine;
	static class Game_clock *gclock;
	static class Palette *pal;
	static class Gump_manager *gumpman;
	static class Party_manager *partyman;
	friend class Game_window;

public:
	static void init(Game_window *g);
};

#endif
