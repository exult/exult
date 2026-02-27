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

/* Baby: reimplemented so you can finally stick Lady Tory's baby in a cradle */
const int FRAME_RIKY			= 2;
void Baby shape#(0x2DA) () {
	if (event != DOUBLECLICK) {
		return;
	}

	//Added in gflags check - this one is set to true when Lady Tory
	//has thanked you for returning the child.
	if (get_item_frame() == FRAME_RIKY && !gflags[RESCUED_RIKY]) {
		if (gflags[HEARD_ABOUT_RIKY]) {
			//heard about Riky
			randomPartySay("@Praise All! The child is still alive. He must be returned to Lady Tory immediately!@");
		} else {
			randomPartySay("@Praise All! The child is still alive. We must find who his parents are, and return him to his home!@");
		}
	} else {
		struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		if (target_shape == SHAPE_FULL_CRADLE) {
			//used on full cradle
			randomPartySay("@Pardon me my friend, dost thou not think that would be a little crowded?@");
		} else if (target_shape == SHAPE_EMPTY_CRADLE) {
			//used on empty cradle
			target->set_item_shape(SHAPE_FULL_CRADLE);
			remove_item();
		} else {
			flashBlocked(60);
		}
	}
}
