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

//Called when user clicks on food
//Reimplemented to add in apple->dough behaviour
void Food shape#(0x179) () {
	//Todo: organise this better, by using explicit frames? e.g. nutrition[FRAME_BREAD] = 4 etc.
	var nutritional_values = [
		4,	//Small bread
		6,	//Long bread (baguette)
		2,	//Rolls
		5,	//Fruitcake
		3,	//Cake
		3,	//Pie
		1,	//Pastry
		12,	//Sausages
		24,	//Mutton
		16,	//Beef
		24,	//Chicken
		24,	//Ham
		4,	//Trout
		8,	//Flounder
		16,	//Venison
		6,	//Dried meat
		2,	//Apple
		3,	//Banana
		2,	//Carrot
		1,	//Grapes
		4,	//Pumpkin (large)
		3,	//Pumpkin (small)
		1,	//Leek
		24,	//Ribs
		3,	//Egg
		1,	//Butter
		9,	//Cheese (wheel)
		2,	//Cheese (wedge)
		32,	//Green cheese (WTF? 32?)
		8,	//Potato
		6,	//Fish and chips
		0	//Silverleaf (I could eat Silverleaf for hours)
	];

	var nutrition = nutritional_values[get_item_frame() + 1];
	var food_frame = get_item_frame();

	//Apples can be used on pastry dough to make pie
	if (food_frame == FRAME_APPLE || food_frame == FRAME_EGG) {
		struct<ObjPos> target = UI_click_on_item();

		//apple was used on dough
		if (target->get_item_shape() == SHAPE_DOUGH) {
			var target_frame = target->get_item_frame();

			if (food_frame == FRAME_APPLE) {
				//remove the apple and change the dough to the raw-pie appearance
				if (target_frame == FRAME_DOUGH_FLAT) {
					remove_item();
					target->set_item_frame(FRAME_DOUGH_PIE);
				} else if (target_frame == FRAME_DOUGH_BALL) {
					randomPartySay("@Thou shouldst first roll out the dough to make a base!@");
				}
			} else if (food_frame == FRAME_EGG && target_frame == FRAME_FLOUR) {
				//eggs + flour = cake batter
				remove_item();
				target->set_item_frame(FRAME_DOUGH_CAKE);
			}
		} else {
			//otherwise, just use the food normally
			consumeEdible(item, target, nutrition, SOUND_EAT);
		}
	} else {
		//Use regular food normally
		useEdible(item, nutrition, SOUND_EAT);
	}
}
