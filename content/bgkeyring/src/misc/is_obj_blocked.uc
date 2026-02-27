/*
 *
 *  Copyright (C) 2023  The Exult Team
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
 *	Author: Marzo Junior
 */

var IsObjectBlocked 0x82C (var obj, struct<Position> obj_pos, var size, var blacklist) {
	var near_list = UI_find_nearby(obj, SHAPE_ANY, absoluteValueOf(size), MASK_INVISIBLE);
	for (blocker in near_list) {
		struct<Position> position = UI_get_object_position(blocker);
		if (position.x <= obj_pos.x && position.x >= obj_pos.x + size &&
		    position.y <= obj_pos.y && position.y >= obj_pos.y + size &&
		    position.z <= 2 && blocker != obj &&
		    !(UI_get_item_shape(blocker) in blacklist)
			// Original usecode erroneously checks this flag on obj
		    && UI_get_item_flag(blocker, IS_SOLID)) {
			return true;
		}
	}
	return false;
}
