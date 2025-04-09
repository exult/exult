/*
 *  Copyright (C) 2000-2024  The Exult Team
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

#ifndef FILE_GUMP_H
#define FILE_GUMP_H

#include "Modal_gump.h"

#include <array>

class Gump_text;

/*
 *  The file save/load box:
 */
class File_gump : public Modal_gump {
protected:
	static short textx, texty;    // Where to draw first text field.
	static short texth;           // Distance down to next text field.
	static std::array<short, 2> btn_rows;    // y-coord of each button row.
	static std::array<short, 3> btn_cols;    // x-coord of each button column.
	std::array<Gump_text*, 10>  names;       // 10 filename slots.
	std::array<Gump_button*, 6> buttons;     // 2 rows, 3 cols of buttons.
	Gump_text*    pushed_text = nullptr;     // Text mouse is down on.
	Gump_text*    focus       = nullptr;     // Text line that has focus.
	unsigned char restored    = 0;           // Set to 1 if we restored a game.

public:
	File_gump();
	~File_gump() override;
	// Find savegame index of text field.
	int  get_save_index(Gump_text* txt);
	void remove_focus();    // Unfocus text.
	void load();            // 'Load' was clicked.
	void save();            // 'Save' was clicked.
	void quit();            // 'Quit' was clicked.
	// Handle one of the toggles.
	int toggle_option(Gump_button* btn);

	int restored_game() {    // 1 if user restored.
		return restored;
	}

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode)
			override;    // Character typed.
};

#endif
