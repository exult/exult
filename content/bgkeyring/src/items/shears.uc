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

//Reimplemented to allow sheep-shearing
void Shears shape#(0x2BA) () {
	struct<ObjPos> target;
	var target_shape;

	if (event != DOUBLECLICK) {
		return;
	}
	target = UI_click_on_item();
	target_shape = target->get_item_shape();

	if (target_shape == SHAPE_CLOTH) {
		target->set_item_shape(SHAPE_BANDAGE);
		target->set_item_frame(UI_die_roll(0, 2));
	} else if (target_shape == SHAPE_SHEEP) {
		//located in sheep.uc
		startShearing(target);
	} else {
		randomPartySay("@Might not those come in handy for cutting cloth into bandages?@");
	}
}
