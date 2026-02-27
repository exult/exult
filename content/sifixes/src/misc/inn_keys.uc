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
 */

const int INN_SLEEPING_SOLDIER = 1;
const int INN_BROKEN_OAR_SOUTHEAST = 2;
const int INN_BROKEN_OAR_EAST = 3;
const int INN_BROKEN_OAR_WEST = 4;
const int INN_SLEEPING_BULL_MAIN = 5;
const int INN_SLEEPING_BULL_NORTH = 6;
const int INN_SLEEPING_BULL_EAST_S = 7;
const int INN_SLEEPING_BULL_EAST_N = 8;
const int INN_BLUE_BOAR = 9;

const int EVENT_WALKING = 20;

enum door_states {
	DOOR_UNLOCKED = 0,
	DOOR_OPEN = 1,
	DOOR_LOCKED = 2,
	DOOR_MAGIC_LOCK = 3
};
enum bed_states {
	BED_MADE = 0,
	BED_UNMADE = 1
};

extern void doorHorizontal object#(270) ();
extern void doorVertical object#(376) ();
extern void UseKeyOnDoor object#(0x281) ();

const int SHAPE_BED_HORIZONTAL = 696;
const int SHAPE_BED_VERTICAL = 1011;

void eggLockInnDoors object#(0xCB0) () {
	var egg_quality = UI_get_item_quality(item);
	if (gflags[BANES_RELEASED] || ((egg_quality == INN_SLEEPING_SOLDIER)
	    && gflags[GOBLIN_SIMON_DEAD])) {
		UI_remove_item(item);
		abort;
	}

	struct<Position> avpos = AVATAR->get_object_position();
	if (avpos.z >= 4) {
		abort;
	}

	var inn_keepers = [
		SIMON,
		JENDON, JENDON, JENDON,
		DEVRA, ARGUS, DEVRA, ARGUS,
		PETRA
	];
	var inn_names = [
		"Sleeping Soldier",
		"Broken Oar Inn", "Broken Oar Inn", "Broken Oar Inn",
		"Sleeping Bull Inn", "Sleeping Bull Inn",
		"Sleeping Bull Inn", "Sleeping Bull Inn",
		"Blue Boar Inn"
	];
	var step_directions = [
		NORTH,
		WEST, WEST, EAST,
		EAST, SOUTH, WEST, WEST,
		WEST
	];
	var qualities = [
		74,
		11, 11, 11,
		3, 3, 3, 3,
		184
	];

	var dist = 30;
	var inn_keeper = inn_keepers[egg_quality];
	var polite_title = getPoliteTitle();

	if (event == EVENT_WALKING) {
		if (inn_keeper->get_distance(AVATAR) < 10) {
			script item after 2 ticks {
				call eggLockInnDoors, STARTED_TALKING;
			}

			script inn_keeper {
				call freeze;
				face inn_keeper->direction_from(AVATAR);
				actor frame standing;
			}
		} else {
			// Just to ensure that the innkeeper *will* reach the Avatar:
			inn_keeper->approach_avatar(0, 0);
			script item after 2 ticks {
				call eggLockInnDoors, EVENT_WALKING;
			}
		}
		abort;
	}

	struct<Position> pos = get_object_position();

	if (egg_quality == INN_SLEEPING_SOLDIER) {
		pos.x -= 8;
		pos.y -= 16;
	} else if (egg_quality == INN_BROKEN_OAR_SOUTHEAST) {
		pos.x -= 16;
		pos.y += 7;
		pos.z = 6;
	} else if (egg_quality == INN_BROKEN_OAR_EAST) {
		pos.x -= 15;
		pos.y += 25;
		pos.z = 6;
	} else if (egg_quality == INN_BROKEN_OAR_WEST) {
		pos.x += 16;
		pos.y += 25;
		pos.z = 6;
	} else if (egg_quality == INN_SLEEPING_BULL_MAIN) {
		pos.x += 9;
		pos.y -= 19;
		pos.z = 6;
	} else if (egg_quality == INN_SLEEPING_BULL_NORTH) {
		pos.x += 16;
		pos.y += 20;
		pos.z = 6;
	} else if (egg_quality == INN_SLEEPING_BULL_EAST_S) {
		pos.x -= 38;
		pos.y -= 19;
		pos.z = 6;
	} else if (egg_quality == INN_SLEEPING_BULL_EAST_N) {
		pos.x -= 38;
		pos.y -= 3;
		pos.z = 6;
	} else if (egg_quality == INN_BLUE_BOAR) {
		pos.x -= 46;
		pos.y += 8;
	}

	var index;
	var max;
	var key;
	var inn_keys = PARTY->count_objects(SHAPE_KEY, qualities[egg_quality], FRAME_ANY);
	var ground_keys = pos->find_nearby(SHAPE_KEY, 50, 0);
	for (key in ground_keys with index to max) {
		if (key->get_item_quality() == qualities[egg_quality]) {
			inn_keys += 1;
		}
	}

	var dir;

	if (inn_keys || qualities[egg_quality]->is_on_keyring()) {
		var msg;
		if (event == EGG) {
			AVATAR->halt_scheduled();
			AVATAR->freeze();

			inn_keeper->halt_scheduled();
			inn_keeper->approach_avatar(0, 0);

			if (UI_is_pc_inside()) {
				msg = "@Yes?@";
			} else {
				msg = "@Oops...@";
			}

			script AVATAR after 4 ticks {
				face AVATAR->direction_from(inn_keeper);
				wait 2;
				say msg;
			}

			if (UI_is_pc_inside()) {
				msg = "@Hold on a bit...@";
			} else {
				msg = "@There thou art!@";
			}

			script inn_keeper after 2 ticks {
				nohalt;
				say msg;
			}

			script item after 2 ticks {
				call eggLockInnDoors, EVENT_WALKING;
			}

			abort;
		} else if (event == STARTED_TALKING) {
			if (UI_is_pc_inside()) {
				inn_keeper.say("@I take it that thou art checking out then, ",
					polite_title, "?@");
				if (askYesNo()) {
					say("@Here, let me have the room keys then. Worry not, I shall lock the doors myself.@");
					say("@I hope thou didst enjoy thy stay at the ",
					    inn_names[egg_quality], "!@");
					msg = "@Do come back!!@";
				} else {
					say("@Come back inside, then, and enjoy thy room, ",
						polite_title, "!@");

					inn_keeper->run_schedule();
					script inn_keeper after 2 ticks {
						nohalt;
						call unfreeze;
						actor frame standing;
						say "@Enjoy thy stay!@";
					}

					AVATAR->halt_scheduled();
					pos = get_object_position();
					dir = step_directions[egg_quality];
					if (dir == NORTH) {
						pos.y -= 8;
					} else if (dir == EAST) {
						pos.x += 8;
					} else if (dir == SOUTH) {
						pos.y += 8;
					} else if (dir == WEST) {
						pos.x -= 8;
					}

					AVATAR->si_path_run_usecode(pos, SI_PATH_SUCCESS, AVATAR, unfreeze, true);
					UI_set_path_failure(unfreeze, AVATAR, SI_PATH_FAILURE);
					abort;
				}
			} else {
				inn_keeper.say("@Where didst thou go carrying my keys? Here, let me have them at once!",
					"~@Thank thee. Please don't -ever- do this again!@");
				msg = "@Don't do this again!@";
			}

			AVATAR->unfreeze();
			AVATAR->halt_scheduled();

			inn_keeper->run_schedule();
			script inn_keeper after 2 ticks {
				nohalt;
				call unfreeze;
				actor frame standing;
				say msg;
			}
		}
	}

	UI_remove_party_items(inn_keys, SHAPE_KEY, qualities[egg_quality], FRAME_ANY, true);
	if (qualities[egg_quality]->is_on_keyring()) {
		qualities[egg_quality]->remove_from_keyring();
	}

	for (key in ground_keys with index to max) {
		if (key->get_item_quality() == qualities[egg_quality]) {
			key->remove_item();
		}
	}


	var inn_doors = [];
	var door_shapes = [SHAPE_DOOR_HORIZONTAL, SHAPE_DOOR_VERTICAL];

	var door;
	for (door in door_shapes with index to max) {
		inn_doors &= pos->find_nearby(door, dist, 0);
	}

	var door_state;
	var door_function;

	for (door in inn_doors with index to max) {
		event = DOUBLECLICK;
		if (door->get_item_quality() == qualities[egg_quality]) {
			door_state = (door->get_item_frame() % 4);
			if (door_state == DOOR_OPEN) {
				door_function = door->get_item_shape();
				if (door_function == SHAPE_DOOR_HORIZONTAL) {
					door->doorHorizontal();
				} else if (door_function == SHAPE_DOOR_VERTICAL) {
					door->doorVertical();
				}

				door->set_intercept_item();
				door->UseKeyOnDoor();
			} else if (door_state == DOOR_UNLOCKED) {
				door->set_intercept_item();
				door->UseKeyOnDoor();
			}
		}
	}

	var inn_beds = pos->find_nearby(SHAPE_BED_HORIZONTAL, dist, 0);
	inn_beds &= pos->find_nearby(SHAPE_BED_VERTICAL, dist, 0);
	var bed;
	var bed_state;
	for (bed in inn_beds with index to max) {
		bed_state = bed->get_item_frame();
		if (bed_state > 2) {
			bed_state = (bed_state - 3) % 2;
			if (bed_state == BED_UNMADE) {
				bed->set_item_frame(bed->get_item_frame() - 1);
			}
		}
	}
}
