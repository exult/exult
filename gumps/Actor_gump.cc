/*
Copyright (C) 2000-2022 The Exult Team

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

#include "Actor_gump.h"

#include "actors.h"
#include "gamewin.h"
#include "misc_buttons.h"

#include <array>
#include <cstdio>

using std::size_t;

#define TWO_HANDED_BROWN_SHAPE 48
#define TWO_HANDED_BROWN_FRAME 0
#define TWO_FINGER_BROWN_SHAPE 48
#define TWO_FINGER_BROWN_FRAME 1

/*
 *  Statics:
 */
namespace {
	struct Position {
		short x;
		short y;
	};

	Position disk{124, 115};     // Where to show 'diskette' button.
	Position heart{124, 132};    // Where to show 'stats' button.
	Position combat{52, 100};    // Combat button.
	Position halo{47, 110};      // "Protected" halo.
	Position cmode{48, 132};     // Combat mode.
	// Coords. of where to draw things,
	std::array coords{
			Position{114, 10}, /* head */
			Position{115, 24}, /* back */
			Position{115, 37}, /* belt */
			Position{115, 55}, /* lhand */
			Position{115, 71}, /* lfinger */
			Position{114, 85}, /* legs */
			Position{ 76, 98}, /* feet */
			Position{ 35, 70}, /* rfinger */
			Position{ 37, 56}, /* rhand */
			Position{ 37, 37}, /* torso */
			Position{ 37, 24}, /* neck */
			Position{ 37, 11}  /* ammo */
	};
}    // namespace

/*
 *  Find the index of the closest 'spot' to a mouse point.
 *
 *  Output: Index, or -1 if unsuccessful.
 */

int Actor_gump::find_closest(
		int mx, int my,    // Mouse point in window.
		int only_empty     // Only allow empty spots.
) {
	mx -= x;
	my -= y;                           // Get point rel. to us.
	long closest_squared = 1000000;    // Best distance squared.
	int  closest         = -1;         // Best index.
	for (size_t i = 0; i < coords.size(); i++) {
		const int  dx       = mx - coords[i].x;
		const int  dy       = my - coords[i].y;
		const long dsquared = dx * dx + dy * dy;
		// Better than prev.?
		if (dsquared < closest_squared
			&& (!only_empty || !container->get_readied(i))) {
			closest_squared = dsquared;
			closest         = i;
		}
	}
	return closest;
}

/*
 *  Create the gump display for an actor.
 */

Actor_gump::Actor_gump(
		Container_game_object* cont,    // Container it represents.  MUST
		//   be an Actor.
		int initx, int inity,    // Coords. on screen.
		int shnum                // Shape #.
		)
		: Gump(cont, initx, inity, shnum) {
	set_object_area(TileRect(26, 0, 104, 132), 22, 124);
	Actor* npc = cont->as_actor();
	add_elem(new Heart_button(this, heart.x, heart.y));
	if (npc->get_npc_num() == 0) {
		add_elem(new Disk_button(this, disk.x, disk.y));
		add_elem(new Combat_button(this, combat.x, combat.y));
	}
	add_elem(new Halo_button(this, halo.x, halo.y, npc));
	add_elem(new Combat_mode_button(this, cmode.x, cmode.y, npc));

	for (size_t i = 0; i < coords.size(); i++) {
		// Set object coords.
		Game_object* obj = container->get_readied(i);
		if (obj) {
			set_to_spot(obj, i);
		}
	}
}

/*
 *  Add an object.
 *
 *  Output: false if cannot add it.
 */

bool Actor_gump::add(
		Game_object* obj, int mx, int my,    // Screen location of mouse.
		int sx, int sy,     // Screen location of obj's hotspot.
		bool dont_check,    // Skip volume check.
		bool combine        // True to try to combine obj.  MAY
							//   cause obj to be deleted.
) {
	ignore_unused_variable_warning(sx, sy);
	Game_object* cont = find_object(mx, my);

	if (cont && cont->add(obj, false, combine)) {
		return true;
	}

	const int index = find_closest(mx, my, 1);

	if (index != -1 && container->add_readied(obj, index)) {
		return true;
	}

	if (container->add(obj, dont_check, combine)) {
		return true;
	}

	return false;
}

/*
 *  Set object's coords. to given spot.
 */

void Actor_gump::set_to_spot(
		Game_object* obj,
		int          index    // Spot index.
) {
	// Get shape info.
	Shape_frame* shape = obj->get_shape();
	if (!shape) {
		return;    // Not much we can do.
	}
	const int w = shape->get_width();
	const int h = shape->get_height();
	// Set object's position.
	obj->set_shape_pos(
			coords[index].x + shape->get_xleft() - w / 2 - object_area.x,
			coords[index].y + shape->get_yabove() - h / 2 - object_area.y);
	// Shift if necessary.
	const int x0    = obj->get_tx() - shape->get_xleft();
	const int y0    = obj->get_ty() - shape->get_yabove();
	int       newcx = obj->get_tx();
	int       newcy = obj->get_ty();
	if (x0 < 0) {
		newcx -= x0;
	}
	if (y0 < 0) {
		newcy -= y0;
	}
	const int x1 = x0 + w;
	const int y1 = y0 + h;
	if (x1 > object_area.w) {
		newcx -= x1 - object_area.w;
	}
	if (y1 > object_area.h) {
		newcy -= y1 - object_area.h;
	}
	obj->set_shape_pos(newcx, newcy);
}

/*
 *  Paint on screen.
 */

void Actor_gump::paint() {
	// Watch for any newly added objs.
	for (size_t i = 0; i < coords.size(); i++) {
		// Set object coords.
		Game_object* obj = container->get_readied(i);
		if (obj) {    //&& !obj->get_tx() && !obj->get_ty())
			set_to_spot(obj, i);
		}
	}

	Gump::paint();    // Paint gump & objects.

	// Paint over blue lines for 2 handed
	Actor* actor = container->as_actor();
	if (actor) {
		if (actor->is_two_fingered()) {
			const int sx = x + 36;
			// Note this is the right finger slot shifted slightly
			const int sy = y + 70;
			ShapeID   sid(
                    TWO_FINGER_BROWN_SHAPE, TWO_FINGER_BROWN_FRAME,
                    SF_GUMPS_VGA);
			sid.paint_shape(sx, sy);
		}
		if (actor->is_two_handed()) {
			const int sx = x + 36;
			// Note this is the right hand slot shifted slightly
			const int sy = y + 55;
			ShapeID   sid(
                    TWO_HANDED_BROWN_SHAPE, TWO_HANDED_BROWN_FRAME,
                    SF_GUMPS_VGA);
			sid.paint_shape(sx, sy);
		}
	}
	// Show weight.
	const int max_weight = container->get_max_weight();
	const int weight     = container->get_weight() / 10;
	char      text[20];
	snprintf(text, sizeof(text), "%d/%d", weight, max_weight);
	const int twidth = sman->get_text_width(2, text);
	const int boxw   = 102;
	sman->paint_text(2, text, x + 28 + (boxw - twidth) / 2, y + 120);
}

Container_game_object* Actor_gump::find_actor(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	return container;
}
