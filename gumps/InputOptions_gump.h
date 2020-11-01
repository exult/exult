/*
Copyright (C) 2001-2015 The Exult Team

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

#ifndef INPUTOPTIONS_GUMP_H
#define INPUTOPTIONS_GUMP_H

#include "Modal_gump.h"
#include <string>
#include <array>
#include <memory>

class Gump_button;

class InputOptions_gump : public Modal_gump {
public:
	InputOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	// Handle events:
	bool mouse_down(int mx, int my, int button) override;
	bool mouse_up(int mx, int my, int button) override;

	void build_buttons();

	void load_settings();
	void save_settings();
	void cancel();

	void toggle_item_menu(int state) {
		item_menu = state;
	}

	void toggle_dpad_location(int state) {
		dpad_location = state;
	}

	void toggle_touch_pathfind(int state) {
		touch_pathfind = state;
	}

private:
	enum button_ids {
	    id_first = 0,
	    id_ok = id_first,
	    id_cancel,
	    id_item_menu,
	    id_dpad_location,
	    id_touch_pathfind,
	    id_count
	};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	bool item_menu;
	int dpad_location;
	bool touch_pathfind;
};

#endif
