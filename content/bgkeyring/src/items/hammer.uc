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

void Hammer shape#(0x26F) () {
	//Hammer was doubleclicked, use it on something
	if (event == DOUBLECLICK) {
		//Hammer is not readied
		if (!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_HAMMER, FRAME_ANY)) {
			randomPartySay("@Thou must wield the hammer in thine hand to use it.@");
			return;
		}

		//clear the view
		UI_close_gumps();
		struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();

		var swordblank;
		var anvil;
		//used on swordblank, make sure the swordblank is on an anvil
		if (target_shape == SHAPE_SWORDBLANK) {
			swordblank = target;
			anvil = target->find_nearest(SHAPE_ANVIL, 3);
			if (anvil) {
				if (onAnvil(swordblank, anvil)) {
					//The sword is correctly positioned: go to the anvil,
					//and call useHammerOnSwordblank()
					gotoObject(anvil, 0, 2, 0, useHammerOnSwordblank, swordblank, SCRIPTED);
				} else {
					//Otherwise, advise the player to adjust its position
					randomPartySay("@Thou shouldst place the blade squarely upon the anvil.@");
				}
			} else {
				randomPartySay("Thou must place the blade upon an anvil before hammering it.");
			}
		} else if (target_shape == SHAPE_ANVIL) {
			//used on anvil, make sure there's a swordblank on top of it
			swordblank = target->find_nearest(SHAPE_SWORDBLANK, 3);
			anvil = target;
			if (swordblank) {
				if (onAnvil(swordblank, anvil)) {
					//The sword is correctly positioned: go to the anvil,
					//and call useHammerOnSwordblank()
					gotoObject(anvil, 0, 2, 0, useHammerOnSwordblank, swordblank, SCRIPTED);
				} else {
					//Otherwise, advise the player to adjust its position
					randomPartySay("@Thou shouldst place the blade squarely upon the anvil.@");
				}
			}
		}
	}
}
