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
 *	This source file contains the code for the Improved Orb of the Moons.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

/* The old Moongate destinations:
enum moongate_destinations {
	BRITAIN				= 1,
	JHELOM				= 2,
	YEW					= 3,
	MINOC				= 4,
	TRINSIC				= 5,
	NEW_MAGINCIA		= 6,
	SKARA_BRAE			= 7,
	SPIRITUALITY		= 8
	MOONGLOW			= 9,
};
*/

enum moongate_destinations_new {
	BRITAIN				= 1,
	JHELOM				= 2,
	YEW					= 3,
	MINOC				= 4,
	TRINSIC				= 5,
	SKARA_BRAE			= 6,
	NEW_MAGINCIA		= 7,
	MOONGLOW			= 8,
	COMPASSION			= 9,
	VALOR				= 10,
	JUSTICE				= 11,
	SACRIFICE			= 12,
	HONOR				= 13,
	SPIRITUALITY		= 14,
	HUMILITY			= 15,
	HONESTY				= 16

	/*
	//Maybe I will add these locations in the future:
	CASTLE_BRITISH		= 17,
	SERPENTS_HOLD		= 18,
	STONEGATE			= 19, //Some other location, maybe?
	ISLE_OF_AVATAR		= 20,
	EMPATH_ABBEY		= 21,
	TERFIN_SHRINE		= 22,
	LYCAEUM				= 23,
	?????				= 24
	*/
};

var isOrbMoongate 0x826 (var itemref) {
	var orbmoongates;
	orbmoongates = [
		SHAPE_ORB_MOONGATE_HORIZONTAL, SHAPE_ORB_MOONGATE_NW_SE,
		SHAPE_ORB_MOONGATE_VERTICAL, SHAPE_ORB_MOONGATE_NE_SW
	];
	if ((itemref->get_item_shape()) in orbmoongates) {
		return true;
	} else {
		return false;
	}
}

var getMoongateDestination 0x823 (var moongate) {
	/* Old destinations:
	//The coordinates of moongate destinations (changed to reflect the multi-map Britannia):
	dest_X =   [0x04CF, 0x0105, 0x0207, 0x073F, 0x03FF, 0x087F, 0x0237, 0x0B67, 0x0B3F];
	dest_Y =   [0x0522, 0x0B0A, 0x01EB, 0x0192, 0x0983, 0x0922, 0x0649, 0x0963, 0x0712];
	dest_Z =   [0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0001, 0x0000];
	dest_map = [0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000];
	*/

	//The coordinates of moongate destinations. These are also the destinations
	//of "standard" blue moongates. If you compare with the above, you'll see
	//that I switched Skara Brae with New Magincia. This is so the Orb works
	//closer to the one from U6. Whatever differences are left from U6 (and
	//there are many) I blame on the sphere generator :-).
	var dest_X = [
	//Cities:	Britain, 		Jhelom, 		Yew, 			Minoc, 			Trinsic,
				0x04CF, 		0x0105, 		0x0207, 		0x073F, 		0x03FF,
	//			Skara Brae, 	New Magincia	Moonglow
				0x0237, 		0x087F, 		0x0B3F,
	//Shrines:	Compassion, 	Valor,  		Justice, 		Sacrifice, 		Honor,
				0x05B8, 		0x01FB, 		0x02EE, 		0x08F1, 		0x0363,
	//			Spirituality,	Humility, 		Honesty
				0x0B67, 		0x0B22, 		0x0A99
	];

	var dest_Y = [
	//Cities:	Britain, 		Jhelom, 		Yew, 			Minoc, 			Trinsic,
				0x0522, 		0x0B0A, 		0x01EB, 		0x0192, 		0x0983,
	//			Skara Brae, 	New Magincia	Moonglow
				0x0649, 		0x0922, 		0x0712,
	//Shrines:	Compassion, 	Valor,  		Justice, 		Sacrifice, 		Honor,
				0x054C, 		0x0A70, 		0x008C, 		0x03C6, 		0x09DE,
	//			Spirituality,	Humility, 		Honesty
				0x0963, 		0x09C6, 		0x048C
	];

	var dest_Z = [
	//Cities:	Britain, 		Jhelom, 		Yew, 			Minoc, 			Trinsic,
				0x0000, 		0x0000, 		0x0000, 		0x0000, 		0x0001,
	//			Skara Brae, 	New Magincia	Moonglow
				0x0000, 		0x0000, 		0x0002,
	//Shrines:	Compassion, 	Valor,  		Justice, 		Sacrifice, 		Honor,
				0x0000, 		0x0000, 		0x0000, 		0x0000, 		0x0000,
	//			Spirituality,	Humility, 		Honesty
				0x0001, 		0x0000, 		0x0000
	];

	var dest_map = [
	//Cities:	Britain, 		Jhelom, 		Yew, 			Minoc, 			Trinsic,
				0, 				0, 				0, 				0, 				0,
	//			Skara Brae, 	New Magincia	Moonglow
				0, 				0, 				0,
	//Shrines:	Compassion, 	Valor,  		Justice, 		Sacrifice, 		Honor,
				0, 				0, 				0, 				0, 				0,
	//			Spirituality,	Humility, 		Honesty
				1, 				0, 				0
	];

	var dest_index;
	var dest_pos;

	//Get moongate quality = destination index:
	dest_index = (moongate->get_item_quality() + 1);

	//Blue moongates should only take the avatar to places with blue moongates;
	//this happens only in the exit from the Shrine of Spirituality, which should
	//take us to Skara Brae (Moonglow in the original):
	if (!isOrbMoongate(moongate) && (dest_index > 8)) {
		dest_index = SKARA_BRAE;
	}

	//If the orb hasn't been fixed, we can't visit the Timelord:
	if ((!gflags[ORB_FIXED_TIMELORD]) && (dest_index == SPIRITUALITY)) {
		dest_index = SKARA_BRAE;
	}
	//the original takes the avatar to Moonglow instead.

	//Return the exit coordinates of the moongate:
	dest_pos = [dest_X[dest_index], dest_Y[dest_index], dest_Z[dest_index], dest_map[dest_index]];

	//return the destination coordinates:
	return dest_pos;
}

void enterMoongate 0x824 (var moongate) {
	struct<Position> avatar_pos;
	struct<Position> moongate_pos;
	var moongate_shape;
	var var0004;

	//Check to see if the moongate hurts instead of teleports:
	if((UI_die_roll(1, 3) == 1) && (!(moongate->get_item_quality() == SPIRITUALITY))) {
		//Bad moongate, harm avatar
		AVATAR->reduce_health(HEALTH, 3, ETHEREAL_DAMAGE);

		//Sound of being hit?
		UI_play_sound_effect2(SOUND_HIT, item);

		//Get avatar and moongate properties:
		avatar_pos = AVATAR->get_object_position();
		moongate_pos = moongate->get_object_position();
		moongate_shape = moongate->get_item_shape();

		//Play the falling-down/kneeling-in-pain animation associated
		//with being wounded when trying to use a moongate:
		if ((moongate_shape == SHAPE_STANDING_RED_MOONGATE) ||
				(moongate_shape == SHAPE_STANDING_BLUE_MOONGATE) ||
				 (moongate_shape == SHAPE_ORB_MOONGATE_HORIZONTAL)) {
			avatar_pos = badMoongateAnim(avatar_pos, moongate_pos, &struct<Position>::y);
		} else {
			avatar_pos = badMoongateAnim(avatar_pos, moongate_pos, &struct<Position>::x);
		}

		//Puts the avatar in position determined by badMoongateAnim:
		AVATAR->move_object(avatar_pos);
	} else {
		//Fade to black:
		UI_fade_palette(12, 1, 0);

		//Moongate travel sound:
		UI_play_sound_effect2(SOUND_MOONGATE, item);

		//Teleport avatar to destination:
		var dest = getMoongateDestination(moongate);
		PARTY->move_object(dest);

		script AVATAR {
			nohalt;
			call exitMoongate;
		};
	}
}

void Orb_of_the_Moons shape#(0x311) () {
	struct<ObjPos> target;
	struct<Position> target_coords;
	var direction;
	var coordinate;
	var moongate_shape;
	var counter;
	var blocked;
	var moongate;
	var arrived_dest;
	var distance;
	struct<Position> avatar_pos;
	var coord_offset;

	//If the sphere generator has been destroyed, the
	//orb of the moons should not work:
	if (gflags[BROKE_SPHERE]) {
		randomPartyBark(["@How odd!@", "@It worked before.@"]);
		return;
	}

	if (event == DOUBLECLICK) {
		//If the avatar hasn't left Trinsic the proper way AND if he is
		//inside the greater Trinsic area, the orb won't work:
		if (!gflags[LEFT_TRINSIC] && (inGreaterTrinsicArea())) {
			return;
		}

		//I prefer this here to help the player choose a
		//target (no gumps in the way and all...):
		UI_close_gumps();

		//Prompt for target:
		target = UI_click_on_item();

		//Get the target's coordinates:
		target_coords = getClickPosition(target);

		//Find the direction of the throw:
		direction = AVATAR->find_direction(target_coords);

		var moongateshapes = [
			SHAPE_ORB_MOONGATE_HORIZONTAL, SHAPE_ORB_MOONGATE_NW_SE, SHAPE_ORB_MOONGATE_VERTICAL,
			SHAPE_ORB_MOONGATE_NE_SW, SHAPE_ORB_MOONGATE_HORIZONTAL, SHAPE_ORB_MOONGATE_NW_SE,
			SHAPE_ORB_MOONGATE_VERTICAL, SHAPE_ORB_MOONGATE_NE_SW
		];
		moongate_shape = moongateshapes[direction + 1];

		//Change the position to make moongate centered on target pos:
		//target_coords[coordinate] = (target_coords[coordinate] + 2);
		if ((direction == NORTH) || (direction == SOUTH)) {
			target_coords.x += 2;
		} else if ((direction == EAST) || (direction == WEST)) {
			target_coords.y += 2;
		} else {
			target_coords.x += 1;
			target_coords.y += 1;
		}

		//See if the target destination is blocked:
		blocked = (!UI_is_not_blocked(target_coords, moongate_shape, 0));

		if (blocked) {
			//It is blocked; attempt to circumvent it by incrementing
			//the Z coordinate and give up trying after 3 tries:
			counter = 3;
			while (counter) {
				target_coords.z += 1;
				blocked = (!UI_is_not_blocked(target_coords, moongate_shape, 0));
				if (!blocked) {
					counter = 0;
				} else {
					counter -= 1;
				}
			}

			//If it is still blocked, tell it to the player
			//and then leave:
			if (blocked) {
				UI_flash_mouse(0);
				return;
			}
		}

		//I prefer to have this done above to make it easier select a target:
		//UI_close_gumps();

		//Create the moongate:
		moongate = UI_create_new_object(moongate_shape);

		//Could not create moongate, so bail out:
		if (!moongate) {
			return;
		}

		//Why was it here AGAIN?
		//UI_close_gumps();

		//Move the moongate to the target position:
		UI_update_last_created(target_coords);

		//Undoes the change made above, in preparation
		//to move the avatar to the right spot:
		if ((direction == NORTH) || (direction == SOUTH)) {
			target_coords.x -= 2;
		} else if ((direction == EAST) || (direction == WEST)) {
			target_coords.y -= 2;
		} else {
			target_coords.x -= 1;
			target_coords.y -= 1;
		}

		//Moongate music:
		UI_play_music(0x0033, 0);

		//Mark moongate as temporary, to avoid filling Britannia
		//with nonfunctional moongates:
		moongate->set_item_flag(TEMPORARY);

		//Makes the moongate rise from the ground, animate for a while
		//then sink back to the ground and disappear:
		script moongate {
			frame 0;
			repeat 3 next frame;;
			repeat 6 {
				repeat 6 next frame;;
				frame 4;
			};
			repeat 3 previous frame;;
			remove;
		}

		//Determine the distance between Avatar and moongate:
		distance = AVATAR->get_distance(moongate);

		//Put destination-1 as the moongate's quality.
		//If the target is more than 7 units away, the
		//destination is a shrine instead, and we add
		//8 to the direction to get destination. Otherwise,
		//the direction is destination-1. In any case, the
		//blue moongates share some code with orb moongates,
		//so we cannot put the final destination here
		//without screwing them up.
		if (distance > 7) {
			moongate->set_item_quality(direction + 8);
		} else {
			moongate->set_item_quality(direction);
		}

		//Make avatar face moongate.
		script AVATAR {
			face direction;
		};

		//Give a small delay to avatar. Only needed for moongates
		//created very close to the avatar:
		distance = 5 - distance;
		if (distance > 0) {
			script AVATAR {
				wait distance;
			};
		}

		//Get avatar position:
		avatar_pos = AVATAR->get_object_position();

		//Determine coord offset for avatar destination:
		if (!(direction in [EAST, WEST])) {
			if ((target_coords.y < avatar_pos.y)) {
				coord_offset = 1;
			} else {
				coord_offset = -1;
			}
			target_coords.y += coord_offset;
		}
		if (!(direction in [NORTH, SOUTH])) {
			if ((target_coords.x < avatar_pos.x)) {
				coord_offset = 1;
			} else {
				coord_offset = -1;
			}
			target_coords.x += coord_offset;
		}

		//The final destination of the avatar before entering moongate:
		//target_coords[coordinate] = (target_coords[coordinate] + coord_offset);

		//UI_path_run_usecode(destination, function, itemref, eventid)
		//	Returns 1 if the avatar can reach destination, zero
		//	otherwise. Walks avatar to destination if he can, and
		//	calls function with event = eventid and item = itemref
		//	when he gets there.
		arrived_dest = UI_path_run_usecode(target_coords, Orb_of_the_Moons, moongate, PATH_SUCCESS);

		if (arrived_dest) {
			//UI_set_path_failure(function, itemref, eventid)
			//	Should the avatar not reach the destination, (say,
			//	because the player moved to somewhere else while
			//	the avatar was still walking), this will call
			//	function with event = eventid and item = itemref.
			UI_set_path_failure(Orb_of_the_Moons, moongate, PATH_FAILURE);

			//See if the orb is in party's possession:
			if (!(getOuterContainer(item) in UI_get_party_list())) {
				//... delete it from wherever it was...
				remove_item();

				//... and give it back to the avatar:
				UI_add_party_items(1, SHAPE_ORB, 0, 0, false);
			}
		}else {
			//Destination unreachable, close moongate:
			closeOrbMoongate(moongate);
		}
	}

	if (event == PATH_FAILURE) {
		//The player interrupted the avatar before reaching the moongate:
		//Sanity check: make sure we ARE talking about a moongate:
		if (isOrbMoongate(item)) {
			//Close the moongate:
			closeOrbMoongate(item);

			//Make fun of the player:
			randomPartyBark(["@No, Avatar.@", "@Let thyself enter.@"]);
		}
	} else if (event == PATH_SUCCESS) {
		//The avatar reached the moongate:
		//Make sure it IS a moongate...
		if (isOrbMoongate(item)) {
			//Close the moongate:
			closeOrbMoongate(item);

			//Teleport avatar:
			enterMoongate(item);
		}
	}
}
