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

#ifndef GAMEMENU_GUMP_H
#define GAMEMENU_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>

class Gump_button;

class Gamemenu_gump : public Modal_gump {
private:
	enum button_ids {
		id_first     = 0,
		id_load_save = id_first,
		id_video_options,
		id_audio_options,
		id_game_display_options,
		id_game_engine_options,
		id_input,
		id_quit,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	Gamemenu_gump();

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;

	void quit(bool return_to_menu = false);

	void quit_exult() {
		quit(false);
	}

	void quit_to_menu() {
		quit(true);
	}

	void loadsave();
	void video_options();
	void audio_options();
	void game_display_options();
	void game_engine_options();
	void input_options();

	static void do_exult_menu();
};

#endif
