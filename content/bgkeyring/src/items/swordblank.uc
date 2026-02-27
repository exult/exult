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
 *	Last modified: 2006-02-27
 */

//The swordblank object itself. This can be used on several different
//objects to automatically place it on those objects (e.g. on a firepit)
//or perform other forging-related behaviours (e.g. quenching it in a
//trough of water).
void SwordBlank shape#(0x29C) () {
	var current_frame;

	//Hot enough to hammer
	var hot_frames;
	//Heated, but not hot enough
	var warm_frames;
	//Cooled down completely
	var cool_frames;


	current_frame = get_item_frame();

	//The swordblank has different behaviours when clicked, depending
	//on what heated state it is in. e.g. if the sword is on an anvil
	//and is hot, doubleclicking it will hit it with a hammer; if it
	//is too cool for hammering, it will be picked up instead.
	if (isBlackSword(item)) {
		hot_frames = [10, 11, 12];
		warm_frames = [8, 9];
		cool_frames = [13, 14, 15];
	} else {
		hot_frames = [2, 3, 4, 5];
		warm_frames = [1];
		cool_frames = [0, 6, 7];
	}

	if (event == DOUBLECLICK) {
		//Avatar is wielding a hammer - now check if the swordblank
		//is sitting on an anvil and is hot enough. If so, we will
		//automatically hammer the sword; if not, just pick it up
		//normally
		if (AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_HAMMER, FRAME_ANY) ||
			//Also allow Julia's hammer:
			AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_JULIAS_HAMMER, FRAME_ANY)) {
			var anvil = find_nearest(SHAPE_ANVIL, 3);
			if (anvil && onAnvil(item, anvil) && current_frame in hot_frames) {
				gotoObject(anvil, 0, 2, 0, useHammerOnSwordblank, item, SCRIPTED);
				return;
			}
		}
		//The swordblank is just sitting there, demanding to be
		//picked up. Go and get it, then call this function again
		//as event = SCRIPTED
		gotoAndGet(item);
	} else if (event == SCRIPTED) {
		//Swordblank is now carried and is ready to be used on something
		UI_close_gumps();
		struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();

		//Now, decide what to do based on the shape of the target item

		//Used on anvil: go to anvil and call this function again with
		//event level 8
		if (target_shape == SHAPE_ANVIL) {
			gotoObject(target, 0, 2, 0, SwordBlank, item, 8);
		} else if (target_shape == SHAPE_FIREPIT) {
			//Used on firepit: go to firepit and call this function again
			//with event level 9
			gotoObject(target, 1, 0, 0, SwordBlank, item, 9);
		} else if (target_shape in [SHAPE_TROUGH_HORIZONTAL, SHAPE_TROUGH_VERTICAL]) {
			//Used on water trough: if sword is hot enough and trough isn't
			//empty, go to trough and call this function again with event level 10.
			//make sure the trough is full
			if (target->get_item_frame() in [3, 7]) {
				//now check if the swordblank is hot
				if (current_frame in hot_frames || current_frame in warm_frames) {
					//stolen from Bucket()
					var target_offsetx;
					var target_offsety;
					if (target_shape == SHAPE_TROUGH_HORIZONTAL) {
						target_offsetx = [-1, -2, -1, -2, 1, 1, -4, -4];
						target_offsety = [1, 1, -2, -2, 0, -1, 0, -1];
					} else {
						target_offsetx = [1, 1, -2, -2, 0, -1, 0, -1];
						target_offsety = [-1, -2, -1, -2, 1, 1, -4, -4];
					}

					gotoObject(target, target_offsetx, target_offsety, 0, SwordBlank, item, 10);
				} else {
					avatarBark("@The sword's not hot.@");
				}
			} else {
				avatarBark("@There's not enough water.@");
			}
		}
	} else if (event == 8) {
		//Swordblank was used on anvil
		//Animate the avatar
		script AVATAR {
			face north;
			actor frame bowing;
			wait 3;
			actor frame standing;
		}

		//Do the swordblank's scripting
		script item {
			nohalt;
			wait 3;
			//Place the blank atop the anvil
			call useSwordOnAnvil;
			//Since this script will have overridden the
			//original cooling behaviour
			call startCooling;
		}
	} else if (event == 9) {
		//Swordblank was used on firepit
		//Animate the avatar
		script AVATAR {
			//This is hardcoded because I can't be bothered finding
			//the nearest firepit just to measure the direction
			face north;
			actor frame bowing;
			wait 3;
			actor frame standing;
		}

		//Do the swordblank's scripting
		script item {
			nohalt;
			wait 3;
			//Place the blank atop the firepit
			call useSwordOnFirepit;
			//Since this script will have overridden the original
			//cooling behaviour
			call startCooling;
		}
	} else if (event == 10) {
		//Swordblank was used on trough
		//Animate the avatar
		script AVATAR {
			face west;
			actor frame bowing;
			wait 10;
			actor frame standing;
		}

		//Do the swordblank's scripting
		script item {
			nohalt;
			wait 5;
			call useSwordOnTrough;
		}
		//No startCooling, since the blade is now cold
	}
}
