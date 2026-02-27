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
 */

void FawnStorm object#(0x6BC) () {
	var pathegg = getPathEgg(2, 1);
	var lute;

	if (event == SCRIPTED) {
		var quality = pathegg->get_item_quality();
		if (quality == 6) {
			AVATAR->set_item_flag(DONT_MOVE);
			lute = UI_create_new_object(SHAPE_LUTE);
			if (lute) {
				lute->set_item_frame(0);
				struct<Position> pos = IOLO->get_object_position();
				pos.y += 1;
				if (UI_update_last_created(pos)) {
					pos.x += (pos.z / 2);
					pos.y += (pos.z / 2);
					UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, pos.x, pos.y, 0, 0, 0, -1);

					var dir = IOLO->find_direction(lute);
					script IOLO {
						nohalt;
						wait 1;
						face dir;
						say "@Look, Avatar!@";
						wait 5;
						actor frame bowing;
						wait 9;
						call FawnStorm;
					}
					pathegg->set_item_quality(quality + 1);
					UI_set_weather(0);
				} else {
					script pathegg after 10 ticks {
						nohalt;
						call FawnStorm;
					}
				}
			} else {
				script pathegg after 10 ticks {
					nohalt;
					call FawnStorm;
				}
			}
			abort;
		} else if (quality == 7) {
			lute = IOLO->find_nearby(SHAPE_LUTE, 25, 0);
			if (lute) {
				lute->set_last_created();
				IOLO->give_last_created();
			}

			script IOLO {
				nohalt;
				actor frame standing;
			}

			script pathegg after 3 ticks {
				nohalt;
				call FawnStorm;
			}

			pathegg->set_item_quality(quality + 1);
			AVATAR->clear_item_flag(DONT_MOVE);
			delayedBark(IOLO, "@A lute!@", 0);
			abort;
		}
	}
	FawnStorm.original();
}
