/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_GAME_MENU_H
#define MUI_GAME_MENU_H

#include "Mui_window.h"

/*
 *  The themed game menu: Return to game, Journey Log, Options, Cheats and
 *  Quit. Options opens the themed Options window; if it cannot be shown,
 *  a page with the classic option screens.
 */
class Mui_game_menu : public Mui_window {
public:
	Mui_game_menu();

protected:
	void build(Mui_box& body) override;

	void preferred_size(float& w, float& h) const override {
		w = 440;
		h = 0;
	}

private:
	bool options_page = false;
	void journey_log();
	void options();
	void cheats();
	void quit();
	void classic_options(int which);
};

#endif
