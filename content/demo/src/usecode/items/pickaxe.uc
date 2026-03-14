/*
 *  Copyright (C) 2007-2011 Malignant Manor
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
 *  Author: Malignant Manor
 */

void Pickaxe shape#(624)()
{
	// This is also used as a weapon function for pickaxe shape
	var range;
	var shape;
	var OREA = 915;
	var OREB = 916;
	var GEM = 760;
	var GOLD_NUGGET = 645;
	var target = UI_click_on_item();
	var SHAPE = UI_get_item_shape(target);
	var FRAME = UI_get_item_frame(target);

/*	if (event != WEAPON)
	{
		var partymember = ;
		if (!UI_is_readied(0xFE9C, 11, 624, 0xFE99) && )
		{
			randomPartySay("@Thou must hold that in thine hand.@");
			return;
		}
	}*/
	if (SHAPE == OREA || SHAPE == OREB)
	{
		UI_set_item_flag(target, TEMPORARY); // needed for static ore
		if (FRAME == 0 && UI_get_random(4) == 4) // broken slightly
			UI_set_item_frame(target, 1);
		else if (FRAME == 1 && UI_get_random(4) == 4) // broken some more
			UI_set_item_frame(target, 2);
		else if (FRAME == 2 && UI_get_random(4) == 4) 
		{
			if (UI_get_random(100) <=4)
			{
				shape = GOLD_NUGGET;
				range = [0, 1];
			}
			else if (UI_get_random(100) >=94)
			{
				shape = GEM;
				range = [0, 11]; // any higher and you get the special gems
			}
			else	// ore dust until deleted by leaving proximity
			{
				UI_set_item_frame(target, 3);
				return;
			}
			UI_set_item_shape(target, shape);
			UI_set_item_frame(target, UI_die_roll(range[1], range[2]));
			UI_update_last_created([target]);
			UI_clear_item_flag(target, TEMPORARY);
		}
	}
}
