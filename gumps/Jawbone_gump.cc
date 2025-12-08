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

#include "Jawbone_gump.h"

#include "contain.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "jawbone.h"
#include "misc_buttons.h"
#include "objiter.h"

const int toothx[19] = {34, 32, 31, 31, 28, 31, 27, 31, 40, 50,
						57, 63, 72, 70, 75, 82, 83, 87, 0};

const int toothy[19] = {19, 30, 37, 44, 52, 57, 66, 77, 82, 84,
						80, 71, 69, 61, 50, 42, 36, 32, 0};

Jawbone_gump::Jawbone_gump(
		Jawbone_object* cont,    // Jawbone it represents.
		int initx, int inity     // Coords. on screen.
		)
		: Gump(cont, initx, inity, game->get_shape("gumps/jawbone")),
		  jawbone(cont) {
	set_object_area(TileRect(0, 0, 138, 116), 26, 97);
}

bool Jawbone_gump::add(
		Game_object* obj, int mx, int my, int sx, int sy, bool dont_check,
		bool combine) {
	ignore_unused_variable_warning(mx, my, sx, sy);
	// Jawbone_object handles all the checks required
	return jawbone->add(obj, dont_check, combine);
}

void Jawbone_gump::paint() {
	// Paint gump itself
	paint_shape(x, y);

	// Paint red "checkmark".
	paint_elems();

	jawbone->find_teeth();

	int i;    // Blame MSVC
	for (i = 0; i < 9; i++) {
		if (jawbone->teeth[i]) {
			paint_tooth(i);
		}
	}
	for (i = 17; i > 8; i--) {
		if (jawbone->teeth[i]) {
			paint_tooth(i);
		}
	}
}

void Jawbone_gump::paint_tooth(int index) {
	ShapeID shape(game->get_shape("gumps/tooth"), index, SF_GUMPS_VGA);

	const int objx = toothx[index];
	const int objy = toothy[index];

	shape.paint_shape(x + objx, y + objy);
}

Game_object* Jawbone_gump::find_object(int mx, int my) {
	jawbone->find_teeth();

	// get position relative to gump
	mx -= x;
	my -= y;

	int i;    // Blame MSVC

	// reverse of drawing order
	for (i = 9; i < 18; i++) {
		if (jawbone->teeth[i] && on_tooth(mx, my, i)) {
			// set correct position (otherwise tooth won't be on mouse cursor)
			set_to_spot(jawbone->teeth[i], mx, my);
			return jawbone->teeth[i];
		}
	}
	for (i = 8; i >= 0; i--) {
		if (jawbone->teeth[i] && on_tooth(mx, my, i)) {
			// set correct position (otherwise tooth won't be on mouse cursor)
			set_to_spot(jawbone->teeth[i], mx, my);
			return jawbone->teeth[i];
		}
	}

	return nullptr;
}

bool Jawbone_gump::on_tooth(int sx, int sy, int index) {
	const ShapeID sid(game->get_shape("gumps/tooth"), index, SF_GUMPS_VGA);
	Shape_frame*  shape = sid.get_shape();

	const int objx = toothx[index];
	const int objy = toothy[index];

	const TileRect r = gwin->get_shape_rect(shape, 0, 0);

	return r.has_point(sx - objx, sy - objy)
		   && shape->has_point(sx - objx, sy - objy);
}

void Jawbone_gump::set_to_spot(Game_object* obj, int sx, int sy) {
	// Get shape.
	Shape_frame* shape = obj->get_shape();

	// Height and width
	const int w = shape->get_width();
	const int h = shape->get_height();

	// Set object's position.
	obj->set_shape_pos(
			sx + shape->get_xleft() - w / 2, sy + shape->get_yabove() - h / 2);
}
