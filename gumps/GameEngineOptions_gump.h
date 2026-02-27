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

#ifndef GAMEENGINEOPTIONS_GUMP_H
#define GAMEENGINEOPTIONS_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class GameEngineOptions_gump : public Modal_gump {
private:
	bool                     allow_autonotes;
	int                      gumps_pause;
	bool                     alternate_drop;
	int                      frames;
	std::vector<std::string> frametext;
	int                      difficulty;
	int                      show_hits;
	int                      mode;
	bool                     charmDiff;
	int                      cheats;
	int                      feeding;
	int                      enhancements;

	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_help,
		id_cancel,
		id_first_setting,
		id_allow_autonotes = id_first_setting,
		id_gumps_pause,
		id_alternate_drop,
		id_frames,
		id_show_hits,
		id_mode,
		id_charmDiff,
		id_difficulty,
		id_cheats,
		id_feeding,
		id_enhancements,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	int y_index_cheats_start = 0;

public:
	GameEngineOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	void build_buttons();
	void update_cheat_buttons();

	void load_settings();
	void save_settings();
	void cancel();
	void help();

	void toggle_allow_autonotes(int state) {
		allow_autonotes = state;
	}

	void toggle_gumps_pause(int state) {
		gumps_pause = state;
	}

	void toggle_alternate_drop(int state) {
		alternate_drop = state;
	}

	void toggle_frames(int state) {
		frames = state;
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

	void toggle_cheats(int state) {
		cheats = state;
		update_cheat_buttons();
	}

	void toggle_feeding(int state) {
		feeding = state;
	}

	void toggle_enhancements(int state) {
		enhancements = state;
	}

	Gump_button* on_button(int mx, int my) override;
};

#endif
