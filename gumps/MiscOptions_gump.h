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

#ifndef MISCOPTIONS_GUMP_H
#define MISCOPTIONS_GUMP_H

#include "Modal_gump.h"
#include <array>
#include <memory>
#include <string>

class Gump_button;

class MiscOptions_gump : public Modal_gump {
private:
	int difficulty;         // Setting for the buttons.
	int show_hits;
	int mode;
	bool charmDiff;
	bool scroll_mouse;
	bool usecode_intro;
	bool menu_intro;
	bool alternate_drop;
	bool allow_autonotes;
	void build_buttons();
	int sc_enabled;
	int sc_outline;
	bool sb_hide_missing;
	std::vector<std::string> sc_outline_txt;

	enum button_ids {
	    id_first = 0,
	    id_ok = id_first,
	    id_cancel,
#ifndef __IPHONEOS__
	    id_scroll_mouse,
#endif
	    id_menu_intro,
	    id_usecode_intro,
	    id_alternate_drop,
	    id_allow_autonotes,
	    id_sc_enabled,
	    id_sc_outline,
	    id_sb_hide_missing,
	    id_difficulty,
	    id_show_hits,
	    id_mode,
	    id_charmDiff,
	    id_count
	};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	MiscOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	// Handle events:
	bool mouse_down(int mx, int my, int button) override;
	bool mouse_up(int mx, int my, int button) override;

	void load_settings();
	void save_settings();
	void cancel();

	void toggle_scroll_mouse(int state) {
		scroll_mouse = state;
	}

	void toggle_menu_intro(int state) {
		menu_intro = state;
	}

	void toggle_usecode_intro(int state) {
		usecode_intro = state;
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

	void toggle_difficulty(int state) {
		difficulty = state;
	}

	void toggle_show_hits(int state) {
		show_hits = state;
	}

	void toggle_mode(int state) {
		mode = state;
	}

	void toggle_charmDiff(int state) {
		charmDiff = state;
	}

	void toggle_alternate_drop(int state) {
		alternate_drop = state;
	}

	void toggle_allow_autonotes(int state) {
		allow_autonotes = state;
	}
};

#endif
