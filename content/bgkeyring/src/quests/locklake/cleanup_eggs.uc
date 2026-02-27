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
 *	This source file contains the code for the cleaning of Lock Lake.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

const int LOCK_LAKE_TIMER				= 0xC;

void eggCleanLockLake object#() () {
	if (event != EGG) {
		return;
	}
	struct<Position> pos;

	//Get the eggs' quality:
	var quality = get_item_quality();
	//Base number of objects to remove:
	var items_to_remove	= 10;

	//Independent control of time for each of the eggs while
	//using but a single timer:
	static var last_elapsed_time_1;
	static var last_elapsed_time_2;
	static var last_elapsed_time_3;
	static var last_elapsed_time_4;
	static var last_elapsed_time_5;

	if (gflags[LOCK_LAKE_BILL_SIGNED]) {
		//Miranda's Bill has been signed, so the cleaning can start
		//Check the elapsed time:
		var elapsed_time = UI_get_timer(LOCK_LAKE_TIMER);

		if (!elapsed_time) {
			//No elapsed time means the timer does not exist yet;
			//create timer and abort:
			UI_set_timer(LOCK_LAKE_TIMER);
			abort;
		}

		//Store the current elapsed time, but only
		//after storing it in a local variable:
		var time;
		if (quality == 0) {
			time = last_elapsed_time_1;
			last_elapsed_time_1 = elapsed_time;
		} else if (quality == 1) {
			time = last_elapsed_time_2;
			last_elapsed_time_2 = elapsed_time;
		} else if (quality == 5) {
			time = last_elapsed_time_3;
			last_elapsed_time_3 = elapsed_time;
		} else if (quality == 9) {
			time = last_elapsed_time_4;
			last_elapsed_time_4 = elapsed_time;
		} else if (quality == 11) {
			time = last_elapsed_time_5;
			last_elapsed_time_5 = elapsed_time;
		}

		//Get the amount of time that elapsed since the last visit
		//to this egg:
		elapsed_time -= time;

		if (elapsed_time <= 0) {
			//No time elapsed or invalid time; abort:
			abort;
		}

		//Get the center of the current cleaning session:
		if (!(quality in [5, 7])) {
			pos = get_object_position();
		} else {
			pos = [0x5D3, 0x44A, 0x0];
		}

		//Get the shapes of all nearby 'garbage' items:
		var garbage = pos->find_nearby(SHAPE_BROKEN_DISH, 50, MASK_NONE);
		garbage = [garbage, pos->find_nearby(SHAPE_GARBAGE, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_STAIN, 50, MASK_TRANSLUCENT)];
		garbage = [garbage, pos->find_nearby(SHAPE_DIAPER, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_BROKEN_ROOF, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_ANCHOR, 50, MASK_NONE)];

		garbage = [garbage, pos->find_nearby(SHAPE_CHAIR, 50, MASK_NONE)];

		garbage = [garbage, pos->find_nearby(SHAPE_FOOD, 50, MASK_NONE)];

		garbage = [garbage, pos->find_nearby(SHAPE_TOP, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_HOOD, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_PANTS, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_CLOTH, 50, MASK_NONE)];

		garbage = [garbage, pos->find_nearby(SHAPE_BODIES_3, 50, MASK_NONE)];
		garbage = [garbage, pos->find_nearby(SHAPE_BODIES_4, 50, MASK_NONE)];

		if (garbage) {
			//Only do this if there is at least one garbage item
			var obj;
			//See how many items are available to be cleaned:
			var total_items = UI_get_array_size(garbage);
			var rand;
			//Split garbage in three categories according to condition checking:
			var simple_garbage = [SHAPE_BROKEN_DISH, SHAPE_GARBAGE, SHAPE_STAIN, SHAPE_DIAPER, SHAPE_BROKEN_ROOF, SHAPE_ANCHOR];
			var clothing = [SHAPE_TOP, SHAPE_HOOD, SHAPE_PANTS, SHAPE_CLOTH];
			var bodies = [SHAPE_BODIES_3, SHAPE_BODIES_4];
			var itemshape;
			var unremovable_objects;

			//How many items will be cleaned this time:
			items_to_remove = (elapsed_time + UI_die_roll(5, 12)) * 3;

			for (obj in garbage) {
				//For each garbage object in the list,
				//Get a random number:
				rand = UI_get_random(total_items);

				//Get the garbage's shape and position:
				itemshape = obj->get_item_shape();
				pos = obj->get_object_position();

				if ((itemshape in simple_garbage)
						//There are some clothing and food items which are
						//inside houses and should not be removed:
						|| (((itemshape in clothing) || (itemshape == SHAPE_FOOD)) && (pos.y < 1230))
						//Only chairs of frame 21 (which are outside houses)
						//should be removed:
						|| ((itemshape == SHAPE_CHAIR) && (obj->get_item_frame() == 21))) {
					//If we are here, the item can (at least in principle) be removed;
					//use the random number above to see if we should remove the
					//item or not:
					if (rand <= items_to_remove) {
						//Remove it and reduce likelyhood of next item being removed:
						obj->remove_item();
						items_to_remove -= 1;
					}
				} else if (itemshape in bodies) {
					//We have a body in our hands
					//If we are here, the item can (at least in principle) be removed;
					//use the random number above to see if we should remove the
					//item or not:
					if (rand <= items_to_remove) {
						//If this is the fish with Mack's key,
						if (obj->get_cont_items(SHAPE_KEY, 55, FRAME_ANY)) {
							//set flag so that Cove's mayor now has it
							gflags[MACKS_KEY_WITH_COVE_MAYOR] = true;
						}
						//Get all objects contained in the body:
						var cont_items = obj->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY);
						if (cont_items) {
							for (obj2 in cont_items) {
								//Remove them all:
								obj2->remove_item();
							}
						}
						//Remove body and reduce likelyhood of next item being removed:
						obj->remove_item();
						items_to_remove -= 1;
					}
				} else {
					//Object cannot be removed (e.g., clothes inside houses)
					unremovable_objects += 1;
				}

				//Increase likelyhood that next objects will be removed:
				total_items -= 1;
			}

			if (unremovable_objects && (unremovable_objects == UI_get_array_size(garbage))) {
				//If there are only unremovable objects left,
				if (quality == 0) {
					//Remove the egg with quality zero:
					remove_item();
					return;
				}
			} else if (items_to_remove < 10) {
				//If there are less than 10 items left to remove in this
				//session, there is nothing else that can be done now:
				return;
			}
		}

		//The egg with quality zero has nothing to do from now on;
		if (quality == 0) {
			//Remove it:
			remove_item();
			return;
		}

		var posx = [
			0x59F, 0x5AF, 0x5BF, 0x5CF, 0x5DF, 0x5EF, 0x5CF, 0x5DF,
			0x60F, 0x5FF, 0x5EF, 0x5DF
		];
		var posy;

		items_to_remove -= 9;
		while (items_to_remove > 0) {
			//Go in steps of 9
			items_to_remove -= 9;
			//Determine y coordinate by the quality:
			if ((quality >= 1) && (quality <= 6)) {
				posy = 0x45F;
			} else if ((quality == 7) || (quality == 8)) {
				posy = 0x44F;
			} else {
				posy = 0x4BF;
			}
			//Create the fake water cover:
			UI_create_new_object(SHAPE_FAKE_WATER);
			//Put it in the correct place:
			UI_update_last_created([posx[quality], posy, 0]);
			//Create another fake water cover:
			UI_create_new_object(SHAPE_FAKE_WATER);
			//Put it in the correct place:
			UI_update_last_created([posx[quality + 1], posy, 0]);
			if (quality in [3, 7, 9, 11]) {
				//These eggs are basically done
				if (quality >= 9) {
					//Some eggs have fly-generating eggs nearby;
					//find fly eggs:
					var fly_eggs = [UI_find_nearby([0x5CA, 0x4C6, 0x0], SHAPE_EGG, 5, MASK_EGG),
									UI_find_nearby([0x5DB, 0x4D3, 0x0], SHAPE_EGG, 5, MASK_EGG),
									UI_find_nearby([0x5E4, 0x4BE, 0x0], SHAPE_EGG, 5, MASK_EGG),
									UI_find_nearby([0x600, 0x4C9, 0x0], SHAPE_EGG, 5, MASK_EGG)];
					for (egg in fly_eggs) {
						//Remove them all:
						egg->remove_item();
					}
				}
				//Egg is done, so remove it:
				remove_item();
				break;
			} else {
				//Prepare egg for next cleaning session:
				quality += 2;
				set_item_quality(quality);
			}
		}
	}
}
