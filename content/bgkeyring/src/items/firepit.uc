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

void Firepit shape#(0x2E3) () {
	//Firepit is being heated up by bellows
	//This behaviour was moved from Bellows() itself, and reorganised so
	//that regular firepits behave the same as the FoV one (along with
	//heating up regular swordblanks!)
	if (event == SCRIPTED) {
		var current_frame = get_item_frame();
		var is_fov = (current_frame > 3);

		var min_frame;
		var max_frame;
		//well lookee here, a Forge of Virtue forge
		if (is_fov) {
			min_frame = 4;
			max_frame = 7;
		} else {
			min_frame = 0;
			max_frame = 3;
		}

		var num_cooling_frames = current_frame - min_frame;

		var target_frame = current_frame + 1;
		if (target_frame > max_frame) {
			target_frame = max_frame;
		}
		num_cooling_frames = target_frame - min_frame - 1;

		//Heat the fire up another notch, then let it cool down
		script item {
			sfx SOUND_FIREPIT;
			wait 1;
			frame target_frame;
			repeat num_cooling_frames {
				wait FIREPIT_COOL_SPEED;
				previous frame;
			};
		}

		//Firepit is at its hottest - start heating any swordblanks up
		if (current_frame == max_frame) {
			//find all swordblanks nearby to heat them
			var nearby_swordblanks = find_nearby(SHAPE_SWORDBLANK, 3, MASK_ALL_UNSEEN);
			for (swordblank in nearby_swordblanks) {
				//Make sure the swordblank is actually on top of the firepit
				struct<Position> firepit_pos = get_object_position();
				struct<Position> swordblank_pos = swordblank->get_object_position();

				var x_offset = swordblank_pos.x - firepit_pos.x;
				var y_offset = swordblank_pos.y - firepit_pos.y;
				var z_offset = swordblank_pos.z - firepit_pos.z;

				//acceptable range
				var x_range = [0, -1, -2];
				var y_range = [0, -1, -2];
				var z_range = [2, 3];

				//Make sure swordblank is on top of the firepit and within
				//a certain zone (pretty lenient, as it's fucking hard to
				//position the blank right over the coals - try tweaking
				//the shape offsets)
				if (x_offset in x_range && y_offset in y_range && z_offset in z_range) {
					//regular forges won't heat up the Black Sword!
					if (!is_fov && swordblank->get_item_frame() >= 8) {
						randomPartySay("@Avatar, I do not think a regular forge is hot enough to temper such a blade. Thou shouldst take it to the Forge of Virtue.@");
					} else {
						script swordblank call heatSwordBlank;
					}
				}
			}
		}
	}
}
