/*
Copyright (C) 2001-2022 The Exult Team

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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "CombatStats_gump.h"

#include "Gump_manager.h"
#include "Paperdoll_gump.h"
#include "actors.h"
#include "game.h"
#include "gamewin.h"
#include "misc_buttons.h"

/*
 *  Statics:
 */

static const int colx    = 110;
static const int coldx   = 29;
static const int rowy[7] = {15, 29, 42, 73, 87, 93, 106};

/*
 *  Create stats display.
 */
CombatStats_gump::CombatStats_gump(int initx, int inity)
		: Gump(nullptr, initx, inity, game->get_shape("gumps/cstats/1")) {
	set_object_area(TileRect(0, 0, 0, 0), 23, 83);

	party_size = gwin->get_party(party, 1);

	const int shnum = game->get_shape("gumps/cstats/1") + party_size - 1;
	ShapeID::set_shape(shnum, 0);

	int i;    // Blame MSVC
	for (i = 0; i < party_size; i++) {
		add_elem(new Halo_button(this, colx + i * coldx, rowy[4], party[i]));
		add_elem(new Combat_mode_button(
				this, colx + i * coldx + 1, rowy[3], party[i]));
		add_elem(new Face_button(
				this, colx + i * coldx - 13, rowy[0], party[i]));
	}
}

/*
 *  Paint on screen.
 */

void CombatStats_gump::paint() {
	Gump_manager* gman = gumpman;

	Gump::paint();

	if (gwin->failed_copy_protection()) {
		const int oinkx = 91;
		for (int i = 0; i < party_size; i++) {
			sman->paint_text(2, "Oink", x + oinkx + i * coldx, y + rowy[1]);
			sman->paint_text(2, "Oink", x + oinkx + i * coldx, y + rowy[2]);
		}
		sman->paint_text(2, "Oink", x + oinkx, y + rowy[5]);
		sman->paint_text(2, "Oink", x + oinkx, y + rowy[6]);
	} else {
		// stats for all party members
		for (int i = 0; i < party_size; i++) {
			gman->paint_num(
					party[i]->get_effective_prop(Actor::combat),
					x + colx + i * coldx, y + rowy[1]);
			gman->paint_num(
					party[i]->get_property(Actor::health), x + colx + i * coldx,
					y + rowy[2]);
		}
		// magic stats only for Avatar
		gman->paint_num(
				party[0]->get_property(Actor::magic), x + colx, y + rowy[5]);
		gman->paint_num(
				party[0]->get_property(Actor::mana), x + colx, y + rowy[6]);
	}
}
