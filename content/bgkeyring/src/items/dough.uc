/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-03-19
 */

/*	Reimplemented to offload doubleclick behaviour to BakeBread, since the
 *	silly sods who programmed it to start with duplicated the behaviour in
 *	two locations.
 */
void Dough shape#(0x292) () {
	//this behaviour allows you to place bread upon a hearth without drag-dropping
	if (event == DOUBLECLICK) {
		if (get_item_frame() > 0) {
			//dough is not flour
			struct<ObjPos> target = UI_click_on_item();
			if (target->get_item_shape() == SHAPE_HEARTH) {
				if (set_last_created()) {
					struct<Position> target_pos = target->get_object_position();
					//choose a random position somewhere along the hearth
					//tweaked to allow one more space for it...
					//...and then untweaked again, as it makes the dough
					//too hard to see
					target_pos.x -= UI_die_roll(1, 2);
					target_pos.z += 1;

					//now place the dough there
					if (UI_update_last_created(target_pos)) {
						//Changed to call BakeBread instead of Dough
						script item after 60 ticks call bakeBread;
						if (UI_get_random(2) == 1) {
							randomPartyBark("@Do not overcook it!@");
						}
					}
				}
			}
		}
	}
}
