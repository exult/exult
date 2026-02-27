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
 *	This source file contains some generic utility functions.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Finds a temporary NPC from a given shape and NPC id.
var findNearbyMonsterWithID (var loc, var shp, var dist, var id) {
	var npcs = UI_find_nearby(loc, shp, dist, MASK_NONE);
	for (npc in npcs) {
		if ((npc->get_npc_number() > 0) && (npc->get_npc_id() == id)) {
			return npc;
		}
	}
}

//Sets BG_DONT_MOVE flag:
void trueFreeze object#() () {
	set_item_flag(BG_DONT_MOVE);
}
//Clears BG_DONT_MOVE flag:
void trueUnfreeze object#() () {
	clear_item_flag(BG_DONT_MOVE);
}
//Puts an NPC in casting mode:
void showCastingFrames object#() () {
	begin_casting_mode();
}

void freezeParty () {
	//Get party list:
	var party = UI_get_party_list();
	//Random NPC for bark:
	var rand = UI_get_random(UI_get_array_size(party) - 1) + 1;
	for (member in party with index) {
		//For each NPC in party,
		//freeze NPC:
		member->trueFreeze();
		//This is the randomly chosen NPC which barks something:
		if (rand == index) {
			script member {
				wait 4;
				say "@I am frozen in place!@";
			}
		}
	}
}

void unfreezeParty () {
	//Get party list:
	var party = UI_get_party_list();
	for (member in party) {
		//Unfreeze all party members:
		member->trueUnfreeze();
	}
}

var createContainerWithObjects (var cont_shape, var item_shapes, var item_frames, var item_quantity, var item_quality) {
	var shape_count = UI_get_array_size(item_shapes);
	var counter;
	var obj;

	var num;
	var max;

	//Create a container from the desired shape:
	var cont = UI_create_new_object(cont_shape);
	//Allow Avatar to take it without problems:
	cont->set_item_flag(OKAY_TO_TAKE);
	//Make it temporary:
	cont->set_item_flag(TEMPORARY);

	while (counter < shape_count) {
		//Increment counter:
		counter += 1;

		if (item_quantity[counter]) {
			//Create the object:
			obj = UI_create_new_object(item_shapes[counter]);

			//Set its frame:
			obj->set_item_frame(item_frames[counter]);

			//Set its quality
			obj->set_item_quality(item_quality[counter]);

			//Make it okay to take and temporary:
			obj->set_item_flag(OKAY_TO_TAKE);
			obj->set_item_flag(TEMPORARY);

			//Try to set quantity; if the item does not have a quantity,
			//the intrinsic will return 0:
			if (!obj->set_item_quantity(item_quantity[counter])) {
				//Since the item does not have a quantity, we have to create
				//each item individually.

				//Place the last object in the container:
				cont->give_last_created();

				//Repeat the creation-setting-giving proccess:
				num = 1;
				while (num < item_quantity[counter]) {
					num += 1;
					obj = UI_create_new_object(item_shapes[counter]);
					obj->set_item_frame(item_frames[counter]);
					obj->set_item_quality(item_quality[counter]);
					obj->set_item_flag(OKAY_TO_TAKE);
					obj->set_item_flag(TEMPORARY);

					//Place the last object in the container:
					cont->give_last_created();
				}
			} else {
				//The item has a quantity, and it has been correctly set.

				//Place it in the container:
				cont->give_last_created();
			}
		}
	}

	return cont;
}

void interjectIfPossible (var npcnum, var msg) {
	if (npcnum->npc_nearby()) {
		var facenum = 0;
		/*
		 * Note: Not all NPCs have their NPC # equal to their face number.
		 * Here would be a good place to set the face number for these NPCs,
		 * along the lines of
		 *  if (npcnum->get_item_shape() == 0x02EB)
		 *  	npcnum = 0xFED6;
		 * or maybe
		 * 	if (npcnum == 0xFED6)
		 *  	npcnum = 0xFEDA;
		 * Also, specifying a different face here is a good idea if the NPC
		 * should use a different face.
		*/


		npcnum->show_npc_face(facenum);
		if (npcnum->get_item_flag(CONFUSED)) {
			say("@Slurp@");
		} else {
			for (lines in msg) {
				say(lines);
			 }
		}
		npcnum.hide();
	}
}

var forceGiveObjToParty (var obj) {
	//Get party list:
	var party = UI_get_party_list();
	//Set the desired object as being the last created:
	obj->set_last_created();
	for (npc in party) {
		//For each npc in the party,
		//try to give item to NPC:
		if (npc->give_last_created()) {
			//If successful, see if it is the Avatar:
			if (npc != UI_get_avatar_ref()) {
				//It isn't; have the NPC pop up and say it
				//will take the item:
				npc.say("@I'll take it!@");
				npc.hide();
			}
			//Success:
			return true;
		}
	}
	//Failure:
	return false;
}

void sellItems (var options, var shapes, var frames, var price, var quantity, var articles, var quantity_text, var quantity_tokens, var dialog) {
	var choice_index;
	var msg;
	var sell_result;
	var total_price;
	var isplural;

	//Present the options to the player:
	choice_index = chooseFromMenu2(options);

	//Keep doing this until the player choses not to buy anything else:
	while (!(choice_index==1)) {
		total_price = price[choice_index] * quantity[choice_index];

		if (quantity[choice_index] > 1) {
			isplural = true;
		} else {
			isplural = false;
		}

		//Inform price to player:
		say("@^",
			makeSellPriceString(articles[choice_index], options[choice_index],
						isplural, price[choice_index],
						quantity_text[choice_index]),
			dialog[1]);

		//See if player agrees with price:
		if (askYesNo()) {
			//Ask for quantity:
			say(dialog[2], quantity_tokens[choice_index], dialog[3]);

			//Sell everything to the party:
			sell_result = sellAmountToParty(shapes[choice_index], frames[choice_index], quantity[choice_index], price[choice_index], 32, 1, true);

			if (sell_result == 1) {
				msg = dialog[4];
			} else if (sell_result == 2) {
				msg = dialog[5];
			} else if (sell_result == 3) {
				msg = dialog[6];
			} else {
				msg = dialog[7];
			}

			//Inform the player if the buy was successful:
			say(msg);
		} else {
			say(dialog[8]);
		}

		//Present the menu again:
		choice_index = chooseFromMenu2(options);
	}

	say(dialog[9]);
}
