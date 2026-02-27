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
 *	This source file contains the code for innkeepers reclaiming the Inn Keys.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

//Inn identifiers:
const int INN_WAYFARERS_NORTH			= 1;
const int INN_WAYFARERS_SOUTH			= 2;
const int INN_FALLEN_VIRGIN				= 3;
const int INN_OUT_N_INN					= 4;
const int INN_BUNK_AND_STOOL			= 5;
const int INN_MODEST_DAMSEL				= 6;
const int INN_SALTY_DOG					= 7;
const int INN_HONORABLE_HOUND			= 8;
const int INN_CHEQUERED_CORK_EAST		= 9;
const int INN_CHEQUERED_CORK_SOUTH		= 10;

//For locking doors:
enum door_states {
	DOOR_UNLOCKED						= 0,
	DOOR_OPEN							= 1,
	DOOR_LOCKED							= 2,
	DOOR_MAGIC_LOCK						= 3
};

//For making beds:
enum bed_states {
	BED_MADE							= 0,
	BED_UNMADE							= 1
};

//To get the proper offsets and NPC IDs:
const int EVENT_FIND_EGG				= 20;
const int EVENT_TALK					= 21;

void eggLockInnDoors object#() () {
	var polite_title = getPoliteTitle();
	var inn_keepers = [
		JAMES, JAMES, MANDY, PAMELA, OPHELIA, BORIS, POLLY,
		APOLLONIA, RUTHERFORD, RUTHERFORD
	];
	var inn_names = [
		"Wayfarer's Inn", "Wayfarer's Inn", "Fallen Virgin",
		"Out 'n' Inn", "Bunk and Stool", "Modest Damsel",
		"Salty Dog", "Honorable Hound", "Chequered Cork", "Chequered Cork"
	];
	var step_directions = [
		SOUTH, NORTH, NORTH, NORTHEAST, WEST, WEST,
		NORTH, WEST, WEST, NORTH
	];

	if (event == EVENT_TALK) {
		set_schedule_type(TALK);
		script item {
			call trueUnfreeze;
		}
		abort;
	} else if (event == EVENT_FIND_EGG) {
		struct<Position> objpos = AVATAR->get_object_position();
		objpos = [objpos, get_npc_id(), 7];
		var egg = objpos->find_nearby(SHAPE_EGG, 3, MASK_EGG);
		script egg call eggLockInnDoors, PATH_SUCCESS;
		abort;
	}

	//Get the innkeeper's index:
	var egg_quality = get_item_quality();
	var inn_keeper = inn_keepers[egg_quality];
	if (inn_keeper->is_dead()) {
		abort;
	}
	//Get the egg's position:
	struct<Position> pos = get_object_position();

	//Use the egg's position to determine the offsets to the doors
	// and beds
	if (egg_quality == INN_WAYFARERS_NORTH) {
		pos.y -= 15;
	} else if (egg_quality == INN_OUT_N_INN) {
		pos.x += 8;
		pos.y -= 16;
	} else if (egg_quality == INN_BUNK_AND_STOOL) {
		pos.x -= 48;
		pos.y -= 8;
	} else if (egg_quality == INN_MODEST_DAMSEL) {
		pos.x -= 24;
	} else if (egg_quality == INN_SALTY_DOG) {
		pos.x -= 18;
		pos.y -= 16;
	} else if (egg_quality == INN_HONORABLE_HOUND) {
		pos.y += 27;
	} else if (egg_quality == INN_CHEQUERED_CORK_EAST) {
		pos.x -= 47;
		pos.y += 9;
	} else if (egg_quality == INN_CHEQUERED_CORK_SOUTH) {
		pos.x -= 7;
		pos.y -= 15;
	}

	//Find all inn keys owner by the party:
	var inn_keys = PARTY->count_objects(SHAPE_KEY, KEY_INN, FRAME_ANY);
	//Find all keys in the ground:
	var ground_keys = pos->find_nearby(SHAPE_KEY, 50, MASK_NONE);
	for (key in ground_keys) {
		//See how many are inn keys:
		if (key->get_item_quality() == KEY_INN) {
			inn_keys += 1;
		}
	}

	if (inn_keys) {
		//If there are any inn keys at all,
		if (UI_is_pc_inside() || (event == PATH_SUCCESS)) {
			//And if the Avatar is inside OR the innkeeper reached the Avatar,
			if (event == EGG) {
				//If the Avatar is inside and innkeeper hasn't started
				//moving towards him yet
				//Halt scripts for Avatar and innkeeper:
				AVATAR->halt_scheduled();
				inn_keeper->halt_scheduled();
				//Freeze Avatar:
				AVATAR->trueFreeze();

				//Start to move the innkeeper towards the Avatar:
				inn_keeper->approach_avatar();

				inn_keeper->set_npc_id(egg_quality);

				var delay = inn_keeper->get_distance(AVATAR) / 5;
				//Innkeeper calls out to Avatar:
				script inn_keeper {
					call trueFreeze;
					wait delay;
					face inn_keeper->find_direction(AVATAR);
					say "@Hold on a bit...@";
					call eggLockInnDoors, EVENT_TALK;
				}

				//Avatar replies:
				script AVATAR after 2 * delay ticks {
					face AVATAR->direction_from(inn_keeper);
					wait 2;
					say "@Yes?@";
				}

				abort;
			} else if (event == PATH_SUCCESS) {
				inn_keeper->set_npc_id(0);
				inn_keeper->run_schedule();

				//Make innkeeper revert to normal schedule and unfreeze him:
				script inn_keeper after 2 ticks {
					nohalt;
					call trueUnfreeze;
					actor frame standing;
					say "@Do come back!!@";
				}

				//The innkeeper has reached destination (or gave up
				//trying and called the usecode anyway...)
				inn_keeper.say("@I take it that thou art checking out then, ",
				               polite_title + "?@");
				//Ask Avatar is he is checking out:
				if (askYesNo()) {
					//Yes he is;
					say("@Here, let me have the room keys then. Worry not, I shall lock the doors myself.@");
					say("@I hope thou didst enjoy thy stay at the ",
					    inn_names[egg_quality], "!@");

					//Unfreeze Avatar:
					AVATAR->trueUnfreeze();
				} else {
					//No, he is not;
					say("@Come back inside, then, and enjoy thy room, ",
					    polite_title, "!@");
					var step_to = step_directions[egg_quality];
					//Halt scripts:
					AVATAR->halt_scheduled();

					//Determine were the Avatar is going to:
					pos = get_object_position();
					var dir = step_directions[egg_quality];
					if (dir == NORTH) {
						pos.y -= 8;
					} else if (dir == EAST) {
						pos.x += 8;
					} else if (dir == SOUTH) {
						pos.y += 8;
					} else if (dir == WEST) {
						pos.x -= 8;
					}

					//Force Avatar back inside the inn:
					AVATAR->si_path_run_usecode(pos, PATH_SUCCESS, AVATAR, trueUnfreeze, true);
					UI_set_path_failure(trueUnfreeze, AVATAR, PATH_FAILURE);
					abort;
				}
			}
		}
	}

	//Remove inn keys from party:
	inn_keys->remove_party_items(SHAPE_KEY, KEY_INN, FRAME_ANY, true);
	//Delete innkeys from the ground:
	for (key in ground_keys) {
		if (key->get_item_quality() == KEY_INN) {
			key->remove_item();
		}
	}


	var inn_doors = [];
	var door_shapes = [
			SHAPE_DOOR_HORIZONTAL,
			SHAPE_DOOR_VERTICAL,
			SHAPE_DOOR2_HORIZONTAL,
			SHAPE_DOOR2_VERTICAL
			];
	//Find unlocked doors
	for (door in door_shapes) {
		inn_doors &= pos->find_nearby(door, 20, MASK_NONE);
	}

	var door_state;
	var door_function;

	for (door in inn_doors) {
		//For each nearby door,
		if (door->get_item_quality() == KEY_INN) {
			//If it is in inn door,
			door_state = (door->get_item_frame() % 4);
			if (door_state == DOOR_OPEN) {
				//and if it is open,
				//get usecode function #:
				door_function = door->get_usecode_fun();

				//Set event to DOUBLECLICK:
				event = DOUBLECLICK;
				//Close door; this is an indirect calle:
				door->(*door_function)();
				//Lock door
				UseKeyOnDoor(door);
			} else if (door_state == DOOR_UNLOCKED) {
				//If the door is closed, lock it:
				UseKeyOnDoor(door);
			}
		}
	}

	//Find all nearby beds:
	var inn_beds = pos->find_nearby(SHAPE_BED_HORIZONTAL, 20, MASK_NONE);
	inn_beds &= pos->find_nearby(SHAPE_BED_VERTICAL, 20, MASK_NONE);
	var bed;
	var bed_state;
	for (bed in inn_beds) {
		//For each bed found,
		bed_state = bed->get_item_frame();
		if (bed_state > 2) {
			//If the bed is actually a sheet, get its state:
			bed_state = (bed_state - 3) % 2;
			if (bed_state == BED_UNMADE) {
				//Bed not made, so make it:
				bed->set_item_frame(bed->get_item_frame() - 1);
			}
		}
	}
}


//New NPC usecode for better handling:
void James object#(0x42E) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		James.original();
	}
}

void Mandy object#(0x4E7) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Mandy.original();
	}
}

void Pamela object#(0x44E) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Pamela.original();
	}
}

void Ophelia object#(0x47A) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Ophelia.original();
	}
}

void Boris object#(0x482) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Boris.original();
	}
}

void Polly object#(0x4B3) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Polly.original();
	}
}

void Apollonia object#(0x413) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Apollonia.original();
	}
}

void Rutherford object#(0x45C) () {
	if ((event == DOUBLECLICK) && (get_npc_id() != 0)) {
		script item call eggLockInnDoors, EVENT_FIND_EGG;
	} else {
		Rutherford.original();
	}
}
