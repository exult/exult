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

const int QUALITY_FRESHFISH	= 1;	//the quality of freshly-caught fish
									//(this will expire to 0 at EXPIRY_RATE)
const int FISH_EXPIRY_RATE	= 50;	//the quality will be decremented by one
									//every EXPIRY_RATE ticks
//These factors are not currently used: sleeping will not decrement quality properly.

const int BASE_CATCH_CHANCE = 30;	//% base chance of catching a fish, if any are about
const int CHANCE_PER_FISH	= 10;	//% extra catch chance per nearby fish

const int SOUND_LANDEDFISH	= 40;	//played when you catch a fish
const int SOUND_FISHING		= 103;	//the plop of bait entering the water

//Reconstituted from original function, with some extra bits added
//(more advice to the player, tweaked catch chance). Also extended
//animation considerably, and increased catch chance to compensate
//for the delay
void FishingRod shape#(0x296) () {
	//Rod was doubleclicked on: start fishing
	if (event == DOUBLECLICK) {
		//make sure the fishing rod is readied first
		if (!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_FISHING_ROD, FRAME_ANY)) {
			randomPartySay("@Thou must have the rod in thy hand.@");
			return;
		}

		UI_close_gumps();
		struct<ObjPos> target = UI_click_on_item();

		if (!UI_is_water(target) && target->get_item_shape() != SHAPE_FISH) {
			UI_flash_mouse(0);

			//only make this 1 in 4, to stop irritating the player
			if (UI_get_random(4) == 1) {
				//Spark is the resident angling expert
				if (inParty(SPARK)) {
					SPARK.say("@Thou shouldst cast thy line into the water, Avatar!@ He grins cheekily.");
				} else {
					randomPartySay("@Last I heard, fish doth not live upon dry land. Thou shouldst try thy rod in the water!@");
				}
			}
			return;
		} else {
			//determine which way the avatar should be facing
			var direction = directionFromAvatar(target);
			//fishing animation
			script AVATAR {
				nohalt;
				call freeze;
				face direction;
				actor frame reach_2h;
				actor frame strike_2h;
				sfx SOUND_FISHING;

				//prevents the animation 'timing out' and reverting to standing
				repeat 2 {
					actor frame strike_2h;
					wait 5;
				};

				//call this function again, as event SCRIPTED, to determine
				//if fish were caught
				call FishingRod;
				actor frame ready;
				actor frame standing;
				call unfreeze;
			}
		}
	} else if (event == SCRIPTED) {
		var nearby_fish = AVATAR->find_nearby(SHAPE_FISH, 15, 0);
		var num_fish_nearby = UI_get_array_size(nearby_fish);
		var fish_caught = false;
		var rand;

		//Adjusted fish-catching chance: now it is a base chance +
		//an adjustable chance per fish
		if (num_fish_nearby > 0) {
			var catch_chance = BASE_CATCH_CHANCE + (num_fish_nearby * CHANCE_PER_FISH);
			if (UI_get_random(100) <= catch_chance) {
				fish_caught = true;
			}
		}

		//Hooray, we got one!
		if (fish_caught) {
			var fish = SHAPE_FOOD->create_new_object();
			if (!fish) {
				return;
			}

			fish->set_item_frame(FRAME_TROUT);
			//Added to stop fish sticking around indefinitely
			fish->set_item_flag(TEMPORARY);

			//put it near the avatar's feet
			struct<Position> avatar_pos = AVATAR->get_object_position();
			avatar_pos.x += 1;
			UI_update_last_created(avatar_pos);

			UI_play_sound_effect(SOUND_LANDEDFISH);

			//Added: indicates that the fish is fresh (for sale to Gordon)
			//Note: this required a new tfa.dat, to change the class of the
			//Food shape from Quality Flags to Quality.
			fish->set_item_quality(QUALITY_FRESHFISH);

			//Added: start the fish quality decrementing
			//Removed: disabled for now, as it doesn't count sleep at all
			//script fish after FISH_EXPIRY_RATE ticks { call 0xB40; }

			/*	Disabled as it is not correctly implemented
			//Added: remove a fish from the water
			var caught_fish;
			if (target->get_item_shape() == SHAPE_FISH) {
				//Player clicked on a particular fish - grab that one
				caught_fish = target;
			} else {
				//otherwise, grab one of the nearby fishies at random
				caught_fish = nearby_fish[UI_get_random(num_fish_nearby)];
			}
			caught_fish->remove_item();
			*/

			//Make one of the party members comment on the catch
			rand = UI_get_random(3);
			if (rand == 1) {
				randomPartyBark("@Indeed, a whopper!@");
				if (SPARK->npc_nearby()) {
					delayedBark(SPARK, "@I have seen bigger.@", 16);
				}
			} else if (rand == 2) {
				randomPartyBark("@What a meal!@");
			} else if (rand == 3) {
				randomPartyBark(["@That fish does not", "look right.@"]);
			}
		} else if (num_fish_nearby == 0) {
			//changed barks to reflect that there's no fish (better player guidance)
			rand = UI_get_random(3);
			if (rand == 1) {
				//Spark, butting his head in as usual
				if (inParty(SPARK) && canTalk(SPARK)) {
					SPARK.say("@The fish doth not seem to be biting, thou shouldst try somewhere else!",
						"~@My father took me fishing sometimes -- thou canst find good spots near bridges, where thou canst see the fishes beneath the water.@");
				} else {
					randomPartySay("@Methinks there are no fish in these waters. Let us try in a better spot!@");
				}
			} else if (rand == 2) {
				delayedBark(AVATAR, "@Not even a bite!@", 0);
			} else if (rand == 3) {
				delayedBark(AVATAR, "@I've lost my bait.@", 0);
			}
		} else {
			rand = UI_get_random(4);

			if (rand == 1) {
				delayedBark(AVATAR, "@Not even a bite!@", 0);
			} else if (rand == 2) {
				delayedBark(AVATAR, "@It got away!@", 1);
				if (IOLO->npc_nearby()) {
					delayedBark(IOLO, "@It was the Big One!@", 16);
				}
			} else if (rand == 3) {
				delayedBark(AVATAR, "@I've lost my bait.@", 0);
			} else if (rand == 4) {
				delayedBark(AVATAR, "@I felt a nibble.@", 0);
			}
		}
	}
}

/*
//This function is responsible for making fish go bad after a certain
//length of time.
//Not used: sleeping will not advance the script queue.
void expireFish () {
	var reduced_quality = get_item_quality() - 1;
	set_item_quality(reduced_quality);

	if (reduced_quality == 0) {
		//stop the countdown
		halt_scheduled();
		avatarBark("Fish now rotten!");
	} else {
		avatarBark("Fish now at " + reduced_quality + " quality");
		script item after FISH_EXPIRY_RATE ticks call 0x600;
	}
}
*/
