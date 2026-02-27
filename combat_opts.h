/*
 *  Combat_opts.h - Combat options.
 *
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef COMBAT_OPTS_H
#define COMBAT_OPTS_H 1

#include "singles.h"

/*
 *  Combat options:
 */
class Combat : public Game_singletons {
	static bool paused;    // For suspending.
public:
	static int difficulty;    // 0=normal, >0 harder, <0 easier.

	enum Mode {
		original = 0,    // All automatic,
		keypause = 1     // Kbd (space) suspends/resumes.
	};

	static Mode mode;
	static bool show_hits;    // Display #'s.
	// In game:
	static void toggle_pause();    // Pause/resume.
	static void resume();          // Always resume.

	static bool is_paused() {
		return paused;
	}

	static bool charmed_more_difficult;
};

#endif
