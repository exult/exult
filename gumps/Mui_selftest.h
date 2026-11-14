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

class Modal_gump;
class Mui_window;

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
};

#endif
