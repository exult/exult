/*
Copyright (C) 2001-2024 The Exult Team

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

#ifndef GAMEDISPLAYOPTIONS_GUMP_H
#define GAMEDISPLAYOPTIONS_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class GameDisplayOptions_gump : public Modal_gump {
private:
	int                      facestats;
	int                      sc_enabled;
	int                      sc_outline;
	bool                     sb_hide_missing;
	std::vector<std::string> sc_outline_txt;
	int                      text_bg;
	int                      smooth_scrolling;
	bool                     usecode_intro;
	bool                     extended_intro;
	bool                     menu_intro;
	int                      paperdolls;

	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_cancel,
		id_help,
		id_facestats,
		id_sc_enabled,
		id_sc_outline,
		id_sb_hide_missing,
		id_text_bg,
		id_smooth_scrolling,
		id_menu_intro,
		id_usecode_intro,
		id_extended_intro,
		id_paperdolls,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	GameDisplayOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;

	void build_buttons();

	void load_settings();
	void save_settings();
	void cancel();
	void help();

	void toggle_facestats(int state) {
		facestats = state;
	}

	void toggle_sc_enabled(int state) {
		sc_enabled = state;
	}

	void toggle_sc_outline(int state) {
		sc_outline = state;
	}

	void toggle_sb_hide_missing(int state) {
		sb_hide_missing = state;
	}

	void toggle_text_bg(int state) {
		text_bg = state;
	}

	void toggle_smooth_scrolling(int state) {
		smooth_scrolling = state;
	}

	void toggle_menu_intro(int state) {
		menu_intro = state;
	}

	void toggle_usecode_intro(int state) {
		usecode_intro = state;
	}

	void toggle_extended_intro(int state) {
		extended_intro = state;
	}

	void toggle_paperdolls(int state) {
		paperdolls = state;
	}
};

#endif
