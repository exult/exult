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
 *	This source file contains the code for the keyring item.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Constants
enum keyring_constants {
   KEY_ALAGNER				= 254,	// Keys for inn doors.
   KEY_CHRISTOPHERS_CHEST	= 253	// Key for Christopher's chest.
};

void UseKeyOnChest (var chest) {
	if (chest->get_item_shape() == SHAPE_LOCKED_CHEST) {
		//key was used in a locked chest:
		chest->set_item_shape(SHAPE_CHEST);
		chest->item_say("Unlocked");

		if (chest->get_item_quality() == KEY_CHRISTOPHERS_CHEST) {
			gflags[UNLOCKED_CHRISTOPHERS_CHEST] = true;
		}
	} else {
		//key was used in an unlocked chest:
		//Check to see if the key is inside the chest:
		if (containedBy(item, chest)) {
			//To differentiate between a key and a keyring:
			var item_shape = get_item_shape();

			//Complain that the key is inside the chest:
			var msg;
			if (item_shape == SHAPE_KEYRING) {
				msg = "The keyring";
			} else if (item_shape == SHAPE_KEY) {
				msg = "The key";
			}
			AVATAR->item_say(msg + " is inside the chest!");
		} else {
			//Lock the chest.
			//Closes the chest's gumps, if open, and those of all contained containers:
			chest->close_gump();
			var gump_objs = [761, 799, 801, 802, 803];
			for (gump in gump_objs) {
				var cont_coll = chest->get_cont_items(gump, QUALITY_ANY, FRAME_ANY);
				for (cont in cont_coll) {
					cont->close_gump();
				}
			}
			chest->set_item_shape(SHAPE_LOCKED_CHEST);
			chest->item_say("Locked");
		}
	}
}

void KeyInternal (var target, var keyfits, var barks) {
	var lockables = [
		SHAPE_LOCKED_CHEST,
		SHAPE_CHEST,
		SHAPE_DOOR_HORIZONTAL,
		SHAPE_DOOR_VERTICAL,
		SHAPE_DOOR2_HORIZONTAL,
		SHAPE_DOOR2_VERTICAL
	];

	var target_shape = target->get_item_shape();

	//The target cannot be unlocked:
	if (!(target_shape in lockables)) {
		return;
	}

	//Key fits whatever it is being used on:
	if (keyfits) {
		//Play the key sound:
		UI_play_sound_effect2(SOUND_KEY, item);

		if (target_shape in [SHAPE_DOOR_HORIZONTAL, SHAPE_DOOR_VERTICAL, SHAPE_DOOR2_HORIZONTAL, SHAPE_DOOR2_VERTICAL]) {
			//Key was used in a door:
			UseKeyOnDoor(target);
		} else {
			//Key was used in a chest:
			UseKeyOnChest(target);
		}
	} else {
		//key does not fit target:
		flashBlocked(0);
		randomPartyBark(barks[UI_get_random(UI_get_array_size(barks))]);
	}
}

class Keyring_data {
	var keys;
	void set_keyring_frame (var keyring) {
		var count = UI_get_array_size(keys);
		UI_play_sound_effect2(SOUND_KEY, keyring);
		// Set the keyring's frame:
		if (count >= 4) {
			keyring->set_item_frame(4);
		} else {
			keyring->set_item_frame(count);
		}
	}
	var is_on_keyring (var target) {
		return (target in keys);
	}
	var add_to_keyring (var keyring, var key) {
		var qual = key->get_item_quality();
		switch(qual) {
			case KEY_INN:
				//Refuse Inn keys:
				avatarSay("@I don't think that the innkeeper would like that...@");
				return false;

			case KEY_ALAGNER:
				//Refuse Alagner's storeroom key:
				avatarSay("@I am supposed to return this key to Alagner.@");
				return false;

			default:
				if (!is_on_keyring(qual)) {
					// Add the key to the keyring:
					keys << qual;
				}
				set_keyring_frame(keyring);
				// Delete the key from the world:
				key->remove_item();
				return true;
		}
	}
	void dump_keys () {
		// Create a bag for the keys:
		var bag = UI_create_new_object(SHAPE_BAG);
		for (key in keys) {
			// Create the keys:
			var keyobj = UI_create_new_object(SHAPE_KEY);
			keyobj->set_item_frame(key % 32);
			// Set their qualities:
			keyobj->set_item_quality(key);
			// Place the key in the bag:
			bag->give_last_created();
		}
		// Place the bag at the avatar's feet:
		UI_update_last_created(AVATAR->get_object_position());
		// Clear the keyring collection:
		keys = [];
	}
	var add_party_keys (var keyring) {
		//Count party keys:
		var party = UI_get_party_list();
		var added = 0;
		for (npc in party) {
			//For each party member, get contained keys
			var key_coll = npc->get_cont_items(SHAPE_KEY, QUALITY_ANY, FRAME_ANY);
			for (key in key_coll) {
				//For each key found, get key quality:
				var quality = key->get_item_quality();
				//Do not add inn keys or Alagner's key!
				if ((quality != KEY_INN) && (quality != KEY_ALAGNER)) {
					//Add key to keyring:
					if (!is_on_keyring(quality)) {
						keys << quality;
					}
					//Remove key:
					key->remove_item();
					added += 1;
				}
			}
		}

		// Set the keyring frame and play the add-key sound.
		set_keyring_frame(keyring);

		//Have someone say how many keys were added:
		randomPartyBark("@Added " + added + " keys@");
		return UI_get_array_size(keys);
	}
}

class<Keyring_data> get_keyring () {
	//This stores the keys in the keyring:
	static class<Keyring_data> keyring;
	static var initialized;
	if (!initialized) {
		keyring = new Keyring_data([]);
		initialized = true;
	}
	return keyring;
}

void Keyring shape#(0x44C) () {
	//If we did not arive here due to a double-click, leave
	if (!(event == DOUBLECLICK)) {
		return;
	}

	struct<ObjPos> target = UI_click_on_item();

	//Since BG has no native support for adding keys to the keyring when you
	//add the key to the container, we include this here: using the keyring
	//on the avatar adds all keys to the keyring.
	if (target->is_npc()) {
		if (isAvatar(target)) {
			// Add all party keys to the keyring:
			get_keyring()->add_party_keys(item);
		} else if (target->get_npc_number() == IOLO) {
			// Just for fun: dumping all keys off the keyring:
			IOLO.say("@Should I remove ALL the keys from the keyring?@");
			if (askYesNo()) {
				get_keyring()->dump_keys();
			}
		}
		return;
	}

	KeyInternal(target,
				get_keyring()->is_on_keyring(target->get_item_quality()),
				["@We don't have that key.@", "@It's not on the keyring.@"]);
}
