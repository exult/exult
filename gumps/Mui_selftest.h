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

#ifndef MUI_SELFTEST_H
#define MUI_SELFTEST_H

#include "common_types.h"

#include <string>

class Modal_gump;
class Mui_window;
class Paintable;

/*
 *  Headless test driver for the themed menus. Inert unless the environment
 *  variable EXULT_MUI_TEST names a script file. Commands run one per frame:
 *  from the main loop while no modal gump is open, and from the modal loop
 *  of whatever gump is open (themed or classic). Each expect prints
 *  "MUI-TEST PASS <what>" or "MUI-TEST FAIL <what> got <x>"; the end prints
 *  "MUI-TEST DONE <n> pass <m> fail". A watchdog exits with status 124
 *  after EXULT_MUI_TEST_TIMEOUT seconds (default 90).
 */
class Mui_selftest {
public:
	static bool active();
	// From the main loop (exult.cc Handle_events).
	static void tick(uint32 ticks);
	// From Mui_window::run().
	static void on_window_frame(Mui_window* win);
	// From Gump_manager::do_modal_gump (classic modal gumps).
	static void on_modal_frame(Modal_gump* gump);
	// A themed window is going away.
	static void forget(Mui_window* win);
	// A text-mode cheat menu is about to run its own loop (what names it).
	// Logs it and, when the script asked for it, queues Escape so it
	// returns straight away.
	static void on_classic_screen(const char* what);
	// From the Get_click loop (Get_click_frame_hook): steps the script only
	// while a conversation waits for a click or key.
	static void on_conversation_wait(Paintable* paint);
	// From the themed conversation's wait loop.
	static void on_dialogue_frame();
	// Helpers for the command files (Mui_dialogue_test).
	// Print "MUI-TEST PASS what" or "MUI-TEST FAIL what got got".
	static void report(bool ok, const std::string& what, const std::string& got);
	// Queue a key press (SDL_Keycode, SDL_Keymod).
	static void press_key(int key, int mod);
	// Queue a click / a wheel turn at SDL window coordinates.
	static void click_window(float wx, float wy, int button);
	static void wheel_window(float wx, float wy, int dy);
	// Run the next command after ms milliseconds.
	static void set_wait(uint32 ms);
};

#endif
