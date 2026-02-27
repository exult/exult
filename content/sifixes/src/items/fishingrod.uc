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

const int FRAME_FISH = 12;

void FishingRod shape#(SHAPE_FISHING_ROD) () {
	if ((event == DOUBLECLICK) || (event == WEAPON)) {
		UI_close_gumps();
		struct<ObjPos> target = UI_click_on_item();
		script AVATAR {
			actor frame strike_1h;
			actor frame standing;
		}
		if (!UI_is_water(target)) {
			UI_flash_mouse(0);
			return;
		}
		struct<Position> pos = AVATAR->get_object_position();
		pos.x += 1;
		if (UI_die_roll(1, 20) > 17) {
			var fish = UI_create_new_object(SHAPE_FOOD);
			if (fish) {
				fish->set_item_frame(FRAME_FISH);
				fish->set_item_flag(OKAY_TO_TAKE);
				UI_update_last_created(pos);
				var barks = [
						"@Indeed, a whopper!@",
						"@What a meal!@",
						"@That doth not look right.@"
					];
				var random = UI_get_random(UI_get_array_size(barks));
				randomPartyBark(barks[random]);
				if (random == 1 && SHAMINO->npc_nearby()) {
					delayedBark(SHAMINO, "@I have seen bigger.@", 16);
				}
			}
		} else {
			var barks = [
					"@Not even a bite!@",
					"@It did escape!@",
					"@I've lost my bait!@",
					"@I felt a nibble...@"
				];
			var random = UI_get_random(UI_get_array_size(barks));
			delayedBark(AVATAR, barks[random], 0);
			if (random == 2 && IOLO->npc_nearby()) {
				delayedBark(IOLO, "@I'm certain thou hadst one!@", 16);
			} else if (random == 4 && DUPRE->npc_nearby()) {
				delayedBark(DUPRE, "@We should hunt instead, Avatar!@", 16);
			}
		}
	}
}
