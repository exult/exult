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
 *
 *	This header file defines generic helper functions added to the original.
 *
 *	Author: Alun Bestor (exult@washboardabs.net)
 *	Modified by Exult Team
 *	Last modified: 2006-03-19
 */

//Generic functions
//-----------------

//returns a random entry from an array
var randomIndex(var array) {
	var size = UI_get_array_size(array);
	var rand = UI_get_random(size);
	return array[rand];
}

//NPC-related functions
//---------------------

//Returns true if the specified object is the avatar, false otherwise.
var isAvatar(var object) {
	return (object->get_npc_object() == UI_get_avatar_ref());
}

//Check if the player has met the specified person (i.e. whether their Met flag has been set).
//Can take either an NPC constant or an object reference.
var hasMet(var npc) {
	return npc->get_item_flag(MET);
}

//Returns true if the specified NPC is in the party, false otherwise
//There should be an intrinsic for this, but it doesn't appear to be defined
//Can take an NPC constant or an object reference.
var inParty(var npc) {
	npc = npc->get_npc_object();
	return (npc in UI_get_party_list());
}

//quick bark/say functions, useful for testing. See also randomPartySay() and randomPartyBark() (original functions)
void avatarBark(var line) {
	if (canTalk(AVATAR)) {
		AVATAR->item_say(line);
	}
}
void avatarSay(var line) {
	if (canTalk(AVATAR)) {
		AVATAR.say(line);
	}
}


//Moving-stuff-around functions
//-----------------------------

//Shift an object into a container (can be from another container or the world)
var moveToContainer(var object, var container, var dont_check_ownership) {
	struct<Position> orig_pos;
	var orig_container;

	//record the previous container
	orig_container = object->get_container();
	//object was in the world - record its last position
	if (!orig_container) {
		orig_pos = object->get_object_position();
	}

	//could not remove the item (protected?)
	if (!object->set_last_created()) {
		return false;
	}

	//try to put the item into the new container
	if (container->give_last_created()) {
		//check if the item was stolen, unless overridden
		if (!dont_check_ownership) {
			stealItem(object);
		}
		return true;
	} else {
		//if it couldn't be put into the new container, just put it back in the original container along with a mouse warning
		if (orig_container) {
			orig_container->give_last_created();
		} else {
			UI_update_last_created(orig_pos);
		}

		//UI_flash_mouse(4);
		return false;
	}
}

//Opposite of moveToContainer: tries to place the item at the specified location, or returns it to its previous location/container if that couldn't be done.
var moveToLocation(var object, var pos) {
	struct<Position> orig_pos;
	var orig_container;

	//record the previous container
	orig_container = object->get_container();
	//object was in the world - record its last position
	if (!orig_container) {
		orig_pos = object->get_object_position();
	}

	//could not remove the item (protected?)
	if (!object->set_last_created()) {
		return false;
	}

	//try to shift the item
	if (UI_update_last_created(pos)) {
		return true;
	} else {
		//if it couldn't be moved, just put it back in the original container/location along with a mouse warning
		if (orig_container) {
			orig_container->give_last_created();
		} else {
			UI_update_last_created(orig_pos);
		}

		UI_flash_mouse(CURSOR_WONT_FIT);
		return false;
	}
}


//generic get-item-from-container/world script, used by item interactions. It is used at the end of a march-to-container script, to play an appropriate animation and transfer the item into the player's inventory.
//once the item has been picked up, it will call the item's function with event = SCRIPTED.
void pickUpItem object#() () {
	var container = getOuterContainer(item);
	var direction = directionFromAvatar(container);

	//The avatar is already carrying the item, call the function immediately and leave it at that
	//This shouldn't happen, since gotoAndGet() will have picked up this case already - however, it could be the player drags the item into their inventory while walking, so...
	if (isAvatar(container)) {
		//The brief delay is present in the original scripts, and is purely cosmetic.
		script item {
			nohalt;
			wait 1;
			call get_usecode_fun();
		}
	} else {
		if (container->is_npc()) {
			//container is an NPC, animate the avatar frobbing the target
			script AVATAR {
				face direction;
				actor frame ready;
				wait 3;
				actor frame standing;
			}
		} else {
			//container is a regular object or just the world, animate the avatar leaning down
			script AVATAR {
				face direction;
				actor frame bowing;
				wait 3;
				actor frame standing;
			}
		}

		//Puts the item in the player's inventory and calls the intended function
		script item {
			nohalt;
			wait 3;
			call giveToAvatar;
			wait 2;
			call get_usecode_fun();
		}
	}
}


//Pathfind to the target item and pick it up once you get there (using pickUpItem). After this has happened, the item's function will be called with event = SCRIPTED. This is commonly used for item interactions that require the Avatar to be carrying the item first.
void gotoAndGet (var target) {
	var offsetx;
	var offsety;
	var container;
	var func;

	UI_close_gumps();

	if (target->get_container()) {
		//item is contained by someone/thing - march the avatar over to the container and pick it up
		container = getOuterContainer(target);
		//Avatar is the container - call the target's function immediately.
		if (isAvatar(container)) {
			script target {
				nohalt;
				wait 1;
				call target->get_usecode_fun();
			}
			return;
		} else {
			offsetx = [0, 1, -1, 1];
			offsety = [2, 1, 2, 0];

			gotoObject(container, offsetx, offsety, -3, pickUpItem, target, 2);
		}
	} else {
		//item was lying on the ground
		offsetx = [0, 1, 1, 1, -1, -1, 0, -1];
		offsety = [1, 1, 0, -1, 1, 0, -1, -1];

		//go to item and call result function
		//(func = pickUpItem())...which means I need to go and define bloody numbers again
		gotoObject(target, offsetx, offsety, -3, pickUpItem, target, 2);
	}
}

//returns true if obj is contained by target, or false otherwise
var containedBy (var obj, var target) {
	var container;

	container = obj->get_container();
	while (container) {
		if (container == target) {
			return true;
		}
		container = container->get_container();
	}
	return false;
}


//Animation functions
//-------------------

//returns the direction (N/S/W/E) that the NPC is currently facing
var getFacing (var npc) {
	var direction;
	var framenum;

	framenum = npc->get_item_frame_rot();

	if (framenum >= EAST_FRAMESET) {
		direction = EAST;
	} else if (framenum >= WEST_FRAMESET) {
		direction = WEST;
	} else if (framenum >= SOUTH_FRAMESET) {
		direction = SOUTH;
	} else {
		direction = NORTH;
	}

	return direction;
}

//Simple function to reverse a direction (NORTH becomes SOUTH, etc.)
var invertDirection (var direction) {
	return (direction + 4) % 8;
}

//Gold-related functions (for streamlining shopping)
//--------------------------------------------------

//returns the total amount of gold the party has
var countGold (var amount) {
	return PARTY->count_objects(SHAPE_GOLD, QUALITY_ANY, FRAME_ANY);
}

//returns true if the party has <amount> gold, false otherwise
var hasGold (var amount) {
	var num_gold = PARTY->count_objects(SHAPE_GOLD, QUALITY_ANY, FRAME_ANY);
	return (num_gold >= amount);
}

//tries to deduct <amount> from the party's gold: returns true if they had the cash, or false if they can't afford it
var chargeGold (var amount) {
	if (hasGold(amount)) {
		return UI_remove_party_items(amount, SHAPE_GOLD, QUALITY_ANY, FRAME_ANY, true);
	} else {
		return false;
	}
}

//give <amount> gold to the party: returns true if successful, false otherwise
var giveGold (var amount) {
	return UI_add_party_items(amount, SHAPE_GOLD, QUALITY_ANY, FRAME_ANY, true);
}



//Script-related functions (used in script{} blocks)
//--------------------------------------------------
//Note that these functions cannot be passed any arguments, which is why they perform a specific action upon <item>.

//use during script sequences, to prevent the actor from moving according to schedule or player input
//IMPORTANT: Use nohalt; in these script sequences, otherwise the actor may remain frozen forever if the script is interrupted!
void freeze object#() () {
	set_item_flag(BG_DONT_MOVE);
}
void unfreeze object#() () {
	clear_item_flag(BG_DONT_MOVE);
}
