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

 /*	Egg used on baking hearths to cook dough that is placed upon it;
 *	Reimplemented to add in more dough recipes
 */
void bakeBread object#(0x635) () {
	if (event == EGG) {
		//egg called this function when dough was placed upon it
		var dough = find_nearby(SHAPE_DOUGH, 0, 0);
		//Added: also checks that the dough isn't just raw flour
		//(take that, you bastards)
		if (dough && dough->get_item_frame() > 0) {
			//in 60 ticks, the dough itself will BakeBread in order
			//to turn into bread
			script dough after 60 ticks {
				call bakeBread;
			}
			if (UI_get_random(2) == 1) {
				randomPartyBark("@Do not overcook it!@");
			}
		}
	} else if (event == SCRIPTED) {
		//dough called function to turn into bread
		struct<Position> dough_pos = get_object_position();
		var hearth = find_nearby(SHAPE_HEARTH, 2, MASK_NONE);

		//Who the hell wrote this?? Why not just check for the
		//existence of a hearth?
		if (UI_get_array_size(hearth) > 0) {
			var dough_frame = get_item_frame();

			remove_item();
			var bread = UI_create_new_object(SHAPE_FOOD);
			if (bread) {
				var bread_frame;
				var bread_name;

				bread->set_item_flag(TEMPORARY);
				//This whole bit has been pulled apart and rewritten to
				//support multiple kinds of bread

				if (dough_frame == FRAME_DOUGH_FLAT) {
					//Flat dough was used: this causes pastry to be born!
					bread_frame = FRAME_PASTRY;
					bread_name = "pastry";
				} else if (dough_frame == FRAME_DOUGH_PIE) {
					bread_frame = FRAME_PIE;
					bread_name = "pie";
				} else if (dough_frame == FRAME_DOUGH_CAKE) {
					//Randomly decides between fruitcake and regular cake
					if (UI_get_random(3) == 1) {
						bread_frame = FRAME_FRUITCAKE;
					} else {
						bread_frame = FRAME_CAKE;
					}
					bread_name = "cake";
				} else if (dough_frame == FRAME_DOUGH_BALL) {
					//Ball dough was used: this makes some kind of regular bread
					//(weighted so that the small round loaves occur twice as often)
					bread_name = "bread";
					var rand = UI_get_random(4);
					if (rand == 1) {
						bread_frame = FRAME_BAGUETTE;
					} else if	(rand == 2) {
						bread_frame = FRAME_ROLLS;
					} else {
						bread_frame	= FRAME_BREAD;
					}
				}

				bread->set_item_frame(bread_frame);

				if (UI_update_last_created(dough_pos)) {
					var rand = UI_get_random(3);
					if (rand == 1) {
						randomPartyBark("@I believe the " + bread_name + " is ready.@");
					} else if (rand == 2) {
						randomPartyBark("@Mmm... Smells good.@");
					}
				}
			}
		}
	}
}
