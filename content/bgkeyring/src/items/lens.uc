/*
 *
 *  Copyright (C) 2006  The Exult Team
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
 *
 *	This source file contains usecode for the Britannian and Gargoyle lenses.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Lens shape#(0x2D6) () {
	if (event == DOUBLECLICK) {
		//Get rotated frame #:
		var framenum = get_item_frame_rot();
		//Rotate by 90 degrees:
		set_item_frame_rot((framenum + 32) % 64);
		struct<Position> pos = get_object_position();
		framenum %= 2;
		if ((pos.x == 0xA9C + (framenum * 7)) && (pos.y == 0xAE7) && (pos.z == 4)) {
			//This was done in the correct place (in the Codex Shrine),
			//so find the right egg and activate it:
			var egg = pos->find_nearby(SHAPE_EGG, 1, MASK_EGG);
			event = EGG;
			egg->eggCodexLenses();
		}
	}
}
