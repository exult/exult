/*
 *  Copyright (C) 2001-2022 The Exult Team
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

#ifndef EXULTMENU_H
#define EXULTMENU_H

#include "game.h"
#include "palette.h"

#include <memory>

class BaseGameInfo;
class Font;
class Game_window;
class Image_buffer8;
class MenuList;
class ModManager;
class Mouse;
class Shape_frame;

class ExultMenu {
private:
	std::shared_ptr<Font> font;
	std::shared_ptr<Font> fonton;
	std::shared_ptr<Font> navfont;
	std::shared_ptr<Font> navfonton;
	Game_window*          gwin;
	Image_buffer8*        ibuf;
	Vga_file              exult_flx;
	//  Palette pal;
	int                       centerx, centery;
	int                       pagesize;
	void                      calc_win();
	std::unique_ptr<MenuList> create_main_menu(int first = 0);
	std::unique_ptr<MenuList> create_mods_menu(
			ModManager* selgame, int first = 0);
	BaseGameInfo* show_mods_menu(ModManager* selgame);

public:
	ExultMenu(Game_window* gw);
	BaseGameInfo* run();
	void          setup();
};

#endif    // EXULTMENU_H
