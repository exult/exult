/*
 *  Gump_manager.h - Object that manages all available gumps
 *
 *  Copyright (C) 2001-2024  The Exult Team
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

#ifndef GUMP_MANAGER_INCLUDED
#define GUMP_MANAGER_INCLUDED

#include "mouse.h"
#include "singles.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "imagebuf.h"
#include "font.h"
#include "shapeid.h"
class Gump;
class Game_object;
class Game_window;
class Modal_gump;
class Paintable;

class Gump_manager : public Game_singletons {
	struct Gump_list {
		Gump*      gump = nullptr;
		Gump_list* next = nullptr;

		Gump_list() = default;

		Gump_list(Gump* g) : gump(g) {}
	};

	Gump_list* open_gumps = nullptr;
	Gump*      kbd_focus  = nullptr;    // This gump gets kbd strokes.
	// So we can test for 'gump mode' quickly:
	int  non_persistent_count = 0;
	int  modal_gump_count     = 0;
	bool right_click_close    = true;
	bool dont_pause_game      = false;    // NEVER SET THIS MANUALLY! YOU MUST
										  // CALL set_gumps_dont_pause_game.
public:
	void add_gump(Gump* gump);    // Add a single gump to screen
	// Show gump for obj
	void add_gump(Game_object* obj, int shapenum, bool actorgump = false);

	bool remove_gump(Gump* gump);               // Detatch a gump from the list
	bool close_gump(Gump* gump);                // Close a gump
	void close_all_gumps(bool pers = false);    // Close all gumps
	void set_kbd_focus(Gump* gump);

	bool showing_gumps(bool no_pers = false) const;    // Are gumps showing?

	bool gump_mode() const {    // Fast check.
		return non_persistent_count > 0;
	}

	bool modal_gump_mode() const {    // displaying a modal gump?
		return modal_gump_count > 0;
	}

	Gump* find_gump(int x, int y, bool pers = true);    // Find gump x,y is in
	Gump* find_gump(const Game_object* obj);    // Find gump that object is in
	// Find gump for object obj:
	Gump* find_gump(const Game_object* owner, int shapenum);

	void update_gumps();
	void paint(bool modal);

	bool        double_clicked(int x, int y, Game_object*& obj);
	bool        handle_kbd_event(void* ev);
	static void translate_numpad(
			SDL_Keycode& code, uint16& unicode, uint16 mod);

	inline bool can_right_click_close() {
		return right_click_close;
	}

	inline void set_right_click_close(bool r) {
		right_click_close = r;
	}

	inline bool gumps_dont_pause_game() {
		return dont_pause_game;
	}

	void set_gumps_dont_pause_game(bool p);

	bool okay_to_quit(Paintable* paint = nullptr);
	int  prompt_for_number(
			 int minval, int maxval, int step, int def,
			 Paintable* paint = nullptr);
	bool do_modal_gump(
			Modal_gump*, Mouse::Mouse_shapes, Paintable* paint = nullptr);
	void paint_num(int num, int x, int y, std::shared_ptr<Font> font = nullptr);

	Gump_manager();

	~Gump_manager() {
		close_all_gumps(true);
	}

private:
	bool handle_modal_gump_event(Modal_gump* gump, SDL_Event& event);

	BackgroundPaintable* background = nullptr;
};

#endif    // GUMP_MANAGER_INCLUDED
