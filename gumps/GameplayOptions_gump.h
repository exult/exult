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

#ifndef GAMEPLAYOPTIONS_GUMP_H
#define GAMEPLAYOPTIONS_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class GameplayOptions_gump : public Modal_gump {

private:
	int facestats;
	int fastmouse;
	int mouse3rd;
	int doubleclick;
	int rightclick_close;
	int cheats;
	int paperdolls;
	int text_bg;
	int frames;
	int right_pathfind;
	int gumps_pause;

	std::vector<std::string> frametext;
	int smooth_scrolling;

	enum button_ids {
	    id_first = 0,
	    id_ok = id_first,
	    id_cancel,
	    id_facestats,
	    id_text_bg,
	    id_fastmouse,
	    id_mouse3rd,
	    id_doubleclick,
	    id_rightclick_close,
	    id_right_pathfind,
	    id_gumps_pause,
	    id_cheats,
	    id_frames,
	    id_smooth_scrolling,
	    id_paperdolls,
	    id_count
	};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	GameplayOptions_gump();

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

	void toggle_facestats(int state) {
		facestats = state;
	}

	void toggle_fastmouse(int state) {
		fastmouse = state;
	}

	void toggle_mouse3rd(int state) {
		mouse3rd = state;
	}

	void toggle_doubleclick(int state) {
		doubleclick = state;
	}

	void toggle_cheats(int state) {
		cheats = state;
	}

	void toggle_paperdolls(int state) {
		paperdolls = state;
	}

	void toggle_text_bg(int state) {
		text_bg = state;
	}

	void toggle_frames(int state) {
		frames = state;
	}

	void toggle_rightclick_close(int state) {
		rightclick_close = state;
	}

	void toggle_right_pathfind(int state) {
		right_pathfind = state;
	}

	void toggle_gumps_pause(int state) {
		gumps_pause = state;
	}

	void toggle_smooth_scrolling(int state) {
		smooth_scrolling = state;
	}
};

#endif
