/*
 *
 *  Copyright (C) 2006-2012  The Exult Team
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
 *	This source file contains usecode to prevent the Magic Carpet from
 *	landing in the area of the Shrine of the Codex.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2012-04-15
 */

// Basic takeoff logic from original carpet.
extern void CarpetTakeOff 0x812 (var barge);

enum carpet_events {
	AVATAR_CARPET = 20,
	PARTY_CARPET = 21
};

void Magic_Carpet shape#(SHAPE_MAGIC_CARPET) () {
	// Want only double-clicks and code executions from (si_)?path_run_usecode.
	if (event == DOUBLECLICK || event == AVATAR_CARPET || event == PARTY_CARPET) {
		// If showing gumps, close them.
		if (UI_in_gump_mode()) {
			UI_close_gumps();
		}

		var barge = get_barge();

		// Flag 10 = 'on moving barge'
		if (!barge->get_item_flag(ON_MOVING_BARGE)) {
			// Is all party in this barge?
			if (UI_on_barge() && AVATAR->get_barge() == barge) {
				// If the avatar is still walking, do NOT do this: it may cause
				// the avatar to be left on the ground while the carpet takes
				// off, and the player won't be able to land it again unless
				// he uses hack mover.
				if (event == PARTY_CARPET && AVATAR->in_usecode_path()) {
					return;
				}

				// If so, take off.
				CarpetTakeOff(barge);

				// Should always work:
				var pieces = barge->find_nearby(SHAPE_CARPET_ROLLER, 20, 0);
				var piece = 0;
				for (obj in pieces) {
					if (obj->get_barge() == barge) {
						piece = obj;
						break;
					}
				}
				// But, in any case...
				if (!piece) {
					randomPartySay("@There is something wrong with this flying carpet!@");
				} else {
					// Convert frame number into direction.
					var frnum  = piece->get_item_frame_rot();
					var nsrefl   = (frnum / 2) % 2;
					var diagrefl = frnum / 32;
					var dir = ((diagrefl * 6) + (nsrefl * 4)) % 8;

					// Make all party face the barge's direction and change
					// to their standing frames.
					var party = UI_get_party_list();
					for (npc in party with index) {
						script npc {
							face dir;
							actor frame standing;
						};
					}
				}
			} else {
				// Should always work:
				var pieces = barge->find_nearby(SHAPE_CARPET_ROLLER, 20, 0);
				var piece = 0;
				for (obj in pieces) {
					if (obj->get_barge() == barge) {
						piece = obj;
						break;
					}
				}
				// But, in any case...
				if (!piece) {
					randomPartySay("@There is something wrong with this flying carpet!@");
				} else if (event == DOUBLECLICK) {
					// Convert frame number into direction.
					var frnum  = piece->get_item_frame_rot();
					var nsrefl   = (frnum / 2) % 2;
					var diagrefl = frnum / 32;
					var dir = ((diagrefl * 6) + (nsrefl * 4)) % 8;

					// Get barge's position.
					struct<Position> pos = barge->get_object_position();
					pos.z += 1;

					// Get array of position offsets.
					var offsets;
					switch (dir) {
						case 0:	// north
							offsets = [
								-2, -9, -3, -7, -1, -7, -3, -5,
								-1, -5, -3, -3, -1, -3, -2, -1
							];
							break;
						case 2:	// east
							offsets = [
								-1, -2, -3, -3, -3, -1, -5, -3,
								-5, -1, -7, -3, -7, -1, -9, -2
							];
							break;
						case 4:	// south
							offsets = [
								-2, -2, -3, -4, -1, -4, -3, -6,
								-1, -6, -3, -8, -1, -8, -2, -10
							];
							break;
						case 6:	// west
							offsets = [
								-9, -1, -7, -2, -7,  0, -5, -2,
								-5,  0, -3, -2, -3,  0, -1, -1
							];
							break;
					}

					// Go through party.
					var party = UI_get_party_list();
					for (npc in party with index) {
						// Make party members walk to various points on the
						// carpet and try to make it rise.
						if (npc->get_npc_number() == AVATAR) {
							UI_path_run_usecode([pos.x + offsets[2*index-1],
								                 pos.y + offsets[2*index],
								                 pos.z], Magic_Carpet, barge, AVATAR_CARPET, true);
						} else {
							npc->si_path_run_usecode([pos.x + offsets[2*index-1],
								                      pos.y + offsets[2*index],
								                      pos.z], PARTY_CARPET, barge,
								                     Magic_Carpet, false);
						}
					}
				}
			}
		} else if (event != DOUBLECLICK) {
			// From here on, we do NOT want the result of (si_)?path_run_usecode.
			return;
		} else if (barge->get_item_flag(OKAY_TO_LAND)) {
			// Can we land?
			// There aren't debris preventing landing. Check if we are not over
			// the Codex Shrine.
			struct<Position> pos = AVATAR->get_object_position();
			if (AVATAR->get_map_num() == 0 &&
				(pos.x >= 0xA50) && (pos.y >= 0xABC) && (pos.x <= 0xAE0) && (pos.y <= 0xB2D)) {
				// Party is over the area of the Shrine of the Codex; prevent landing
				AVATAR.say("There is a strange force preventing you from landing in this area.");
			} else {
				// Everything is A-OK for landing, so do it.
				barge->clear_item_flag(ON_MOVING_BARGE);
				barge->clear_item_flag(ACTIVE_BARGE);
				// Make carpet descent until the ground.
				script barge {
					repeat 10 {
						descent;
						nop;
					};
				}
				// Resume normal music.
				UI_play_music(255, 0);
			}
		} else {
			// Can't land, so inform the player.
			randomPartySay("@I do not believe that we can land here safely.@");
		}
	}
}

void FlyingCarpetRoller shape#(SHAPE_CARPET_ROLLER) () {
	// Check if we are on a barge.
	var barge  = get_barge();

	if (barge) {
		// We are. Is the barge in motion?
		if (barge->get_item_flag(ON_MOVING_BARGE)) {
			// Yes. Tell it to stop.
			barge->Magic_Carpet();
			return;
		}
		struct<Position> pos = barge->get_object_position();
		if (pos.z > 0) {
			return;
		}

		// Find nearby pieces of the carpet and delete them all.
		var pieces = find_nearby(SHAPE_MAGIC_CARPET, 20, 0);
		for (obj in pieces) {
			if (obj->get_barge() == barge) {
				obj->remove_item();
			}
		}

		// Delete the barge object too.
		barge->remove_item();
	}

	// Convert frame number into direction and rotation bits.
	var frnum  = get_item_frame_rot();
	var nsrefl   = (frnum / 2) % 2;
	var diagrefl = frnum / 32;
	var dir = ((diagrefl * 6) + (nsrefl * 4)) % 8;
	var diagbit  = diagrefl * 32;
	var nsbit    = nsrefl * 2;

	// Get piece position.
	struct<Position> pos    = get_object_position();

	// Create a rolled-up carpet and set its frame.
	var roll = UI_create_new_object(SHAPE_ROLLED_CARPET);
	roll->set_item_frame_rot(0 + nsbit + diagbit);

	// Move the rolled-up carpet into the right spot.
	switch (dir) {
		case 0:	// north
			UI_update_last_created([pos.x + 1, pos.y    , 0]);
			break;
		case 2:	// east
			UI_update_last_created([pos.x    , pos.y + 1, 0]);
			break;
		case 4:	// south
			UI_update_last_created([pos.x + 1, pos.y    , 0]);
			break;
		case 6:	// west
			UI_update_last_created([pos.x    , pos.y + 1, 0]);
			break;
	}

	// Finally, remove this piece.
	remove_item();
}

void RolledFlyingCarpet shape#(SHAPE_ROLLED_CARPET) () {
	// Is this sitting on a barge?
	if (get_barge()) {
		// Complain about it (for safety).
		randomPartySay("@There is not enough space to unroll the carpet.@");
		return;
	}

	// Convert frame number into direction and rotation bits.
	var frnum = get_item_frame_rot();
	var nsrefl   = (frnum / 2) % 2;
	var diagrefl = frnum / 32;
	var dir = ((diagrefl * 6) + (nsrefl * 4)) % 8;
	var diagbit  = diagrefl * 32;
	var nsbit    = nsrefl * 2;

	// Get carpet position.
	struct<Position> pos = get_object_position();

	// These will be positions for each of the carpet pieces.
	struct<Position> pospart1, pospart2, pospart3, posbarge;

	// Get positions adequate to the direction.
	switch (dir) {
		case 0:	// north
			pospart1 = [pos.x    , pos.y - 6, pos.z];
			pospart2 = [pos.x    , pos.y    , pos.z];
			pospart3 = [pos.x - 1, pos.y    , pos.z];
			posbarge = [pos.x    , pos.y    , pos.z];
			break;
		case 2:	// east
			pospart1 = [pos.x +11, pos.y    , pos.z];
			pospart2 = [pos.x + 5, pos.y    , pos.z];
			pospart3 = [pos.x    , pos.y - 1, pos.z];
			posbarge = [pos.x +11, pos.y    , pos.z];
			break;
		case 4:	// south
			pospart1 = [pos.x    , pos.y +11, pos.z];
			pospart2 = [pos.x    , pos.y + 5, pos.z];
			pospart3 = [pos.x - 1, pos.y    , pos.z];
			posbarge = [pos.x    , pos.y +11, pos.z];
			break;
		case 6:	// west
			pospart1 = [pos.x - 6, pos.y    , pos.z];
			pospart2 = [pos.x    , pos.y    , pos.z];
			pospart3 = [pos.x    , pos.y - 1, pos.z];
			posbarge = [pos.x    , pos.y    , pos.z];
			break;
	}

	// Remove the current piece from the world without deleting.
	set_last_created();

	// Create barge with correct size and direction.
	var barge;
	if (diagbit) {
		barge = UI_create_barge_object(12, 6, dir);
	} else {
		barge = UI_create_barge_object(6, 12, dir);
	}

	// Put this barge in the right position, but above the ground (yes, I am
	// being silly using 255; 10 or so would be enough).
	UI_update_last_created([posbarge.x, posbarge.y, 255]);

	// Check if the ground is clear (flag 21 == 'okay to land').
	if (barge->get_item_flag(OKAY_TO_LAND)) {
		// Create each of the carpet pieces, set their respective frames and
		// move them to the right position.
		var part1 = UI_create_new_object(SHAPE_MAGIC_CARPET);
		part1->set_item_frame_rot(nsbit + diagbit);
		UI_update_last_created(pospart1);

		var part2 = UI_create_new_object(SHAPE_MAGIC_CARPET);
		part2->set_item_frame_rot(2 - nsbit + diagbit);
		UI_update_last_created(pospart2);

		var part3 = UI_create_new_object(SHAPE_CARPET_ROLLER);
		part3->set_item_frame_rot(nsbit + diagbit);
		UI_update_last_created(pospart3);

		// Move barge to the ground. This will cause it to gather the barge
		// pieces into its list.
		barge->set_last_created();
		UI_update_last_created(posbarge);

		// Finally, delete the current object.
		remove_item();
	} else {
		// There is not enough space for the unrolled carpet.
		// Put the rolled-up carpet back.
		set_last_created();
		UI_update_last_created(pos);

		// Delete the barge object.
		barge->remove_item();

		// Complain about the lack of space.
		randomPartySay("@There is not enough space to unroll the carpet.@");
	}
}

