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

//Called when user clicks on a bottle
//(This has been rewired so that we can use milk bottles on butter churns:
//see churn.uc for more details)
void Bottle shape#(0x268) () {
	if (event == DOUBLECLICK) {
		//Bottle was clicked on
		//this is a milk bottle - go and pick it up before you use it, then
		//call this function again with event = SCRIPTED
		if (get_item_frame() == FRAME_MILK) {
			gotoAndGet(item);
		} else {
			useEdible(item, BOTTLE_NUTRITION, SOUND_DRINK);
			return;
		}
	} else if (event == SCRIPTED) {
		//Milk was picked up, and is ready to be used
		struct<ObjPos> target = UI_click_on_item();
		//a churn was selected - go to the churn and run churnButter()
		if (target->get_item_shape() == SHAPE_KITCHEN_ITEM &&
				target->get_item_frame() == FRAME_CHURN) {
			gotoChurn(target, CHURN_WITH_BOTTLE);
		} else {
			//otherwise, just drink the damn milk already
			consumeEdible(item, target, BOTTLE_NUTRITION, SOUND_DRINK);
		}
	}
}
