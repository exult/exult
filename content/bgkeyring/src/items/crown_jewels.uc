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
 *	This source file contains usecode for the Crown Jewels of Britannia.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void LB_Sceptre_Hit object#(0xB7E) () {
	if (event != WEAPON) {
		return;
	}
	var field_shapes = [SHAPE_ENERGY_FIELD, SHAPE_FIRE_FIELD, SHAPE_POISON_FIELD, SHAPE_SLEEP_FIELD];
	if (get_item_shape() in field_shapes) {
		struct<Position> pos = get_object_position();
		UI_sprite_effect(ANIMATION_BLUE_BEADS, pos.x - 1, pos.y - 1, 0, 0, 0, -1);
		UI_play_sound_effect(64);
		script item after 4 ticks remove;
	}
}

void LB_Sceptre shape#(0x466) () {
	//Add double-click support
}

void LB_Crown shape#(0x467) () {
	// TODO: Implement the magic protection side.
	if (event == READIED) {
		set_item_flag(MAGIC_PROTECTION);
	} else if (event == UNREADIED) {
		clear_item_flag(MAGIC_PROTECTION);
	}
}

void LB_Amulet shape#(0x468) () {
	if (event == READIED) {
		UI_infravision(get_container(), true);
	}
	if (event == UNREADIED) {
		UI_infravision(get_container(), false);
	}
}
