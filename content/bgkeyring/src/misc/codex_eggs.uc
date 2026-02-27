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
 *	This source file contains usecode for the Codex-ralated eggs.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

void eggCodexLenses object#() () {
	struct<Position> pos;
	var quality;
	var lens;
	var npc;
	var framenum;
	var bark;

	if ((event == EGG || event == SCRIPTED)) {
		//Get egg's position and quality:
		pos = get_object_position();
		quality = get_item_quality();
		//Find the nearest lens:
		lens = pos->find_nearby(SHAPE_LENS, 0, MASK_NONE);
		if (lens) {
			//there is a lens nearby;
			//see if it is the right one:
			framenum = lens->get_item_frame_rot();
			if (framenum == (quality + 32)) {
				//It is, and correctly rotated too
				//Indicate to player that he did something right:
				UI_sprite_effect(ANIMATION_GREEN_BUBBLES, pos.x - 2, pos.y - 2, 0, 0, 0, -1);
				UI_play_sound_effect(67);
				if (event == SCRIPTED) {
					//This was a scripted call, which means both
					//lenses are in place and properly rotated
					//Halt scheduled scripts:
					halt_scheduled();
					//Display Codex:
					script AVATAR after 16 ticks call displayCodex, SCRIPTED;
				} else {
					//Egg event; call the other egg to see if
					//the other lens is there and correctly
					//rotated as well:
					pos = [(0xAA3 - quality * 7), 0xAE7, 4];
					var egg = pos->find_nearby(SHAPE_EGG, 1, MASK_EGG);
					if (egg) {
						egg->halt_scheduled();
						event = SCRIPTED;
						egg->eggCodexLenses();
					}
				}
			} else if (framenum == quality) {
				//Right lens, needs to be rotated:
				if (event == SCRIPTED) {
					//This means that the *other* lens is rotated, and
					//this one isn't
					bark = "@Rotate the other lens too@";
				} else {
					//Neither lens are rotated
					bark = "@Rotate the lens@";
				}
				UI_item_say(randomPartyMember(), bark);
			} else {
				//Wrong lens in the right place:
				bark = "@It goes on the other side@";
				UI_item_say(randomPartyMember(), bark);
			}
		}
	}
}

void eggCodexShrineEntrance object#() () {
	if (event != EGG) {
		return;
	}
	var statues = UI_find_nearby(item, SHAPE_GUARDIAN_STATUE, 20, MASK_NONE);
	var statue;

	if (statues) {
		//If the Avatar is facing south, he is most likely leaving
		//the shrine of the Codex. Do not display the statues' message
		//in this case.
		var avframe = AVATAR->get_item_frame_rot();
		if (!((avframe >= 16) && (avframe <= 31))) {
			//Allow Avatar to see the Codex if there is any shrine quest which the Avatar
			//hasn't completed yet:
			var in_quest = false;
			var i = 0;
			while (i < 8) {
				i += 1;
				if (gflags[MEDITATED_AT_SHRINE_BASE + i] && !gflags[VIEWED_CODEX_BASE + i]) {
					in_quest = true;
					break;
				}
			}
			if (gflags[SPIRITUALITY_STONE_QUEST]) {
				//Allow for attunement of white virtue stone:
				in_quest = true;
			}
			if (gflags[CODEX_ALL_EIGHT_SHRINES]) {
				//Or if the player is in the Codex quest:
				in_quest = true;
			}
			if (gflags[RELOCATE_CODEX_QUEST]) {
				//But not if the Codex Quest is over:
				in_quest = false;
			}

			if (in_quest) {
				//The Avatar is in a sacred quest, so let him enter:
				statue = statues[2];
				script statue
					say "@Thou art welcome, seeker@";
				statue = statues[1];
				script statue after 15 ticks
					say("@Let Virtue bring thee wisdom@");

				//Anti-cheater flag:
				gflags[IN_CODEX_QUEST] = true;
			} else {
				//The Avatar is trying to enter without a quest;
				//boot him out:
				statue = statues[2];
				script statue
					say "@Thou art not on a sacred quest!@";
				statue = statues[1];
				script statue after 15 ticks
					say("@Passage denied!@");
				var pos = [2719, 2852, 0];
				AVATAR->halt_scheduled();
				AVATAR->trueFreeze();
				AVATAR->si_path_run_usecode(pos, PATH_SUCCESS, AVATAR, trueUnfreeze, true);
				UI_set_path_failure(trueUnfreeze, AVATAR, PATH_FAILURE);
				AVATAR->obj_sprite_effect(ANIMATION_FIREWORKS, 0, 0, 0, 0, 0, 6);
			}
		}
	}
}

void eggCodexQuest object#() () {
	if (event != EGG) {
		return;
	}
	if (!gflags[CODEX_ALL_EIGHT_SHRINES]) {
		return;
	}
	//Get the quality of the egg:
	var qual = get_item_quality();

	//To see if all items are where they should, I am using
	//bit flags in a pathegg's frame. These are the bit flags
	//for each different egg quality:
	var pathegg_frame_flags = [1, 2, 4, 8];

	//The bit flag corresponding to the current egg:
	var frflag = pathegg_frame_flags[qual + 1];

	//This will hold the pathegg's frame number:
	var frnum = 0;

	//This will store the set flags, if any:
	var setflags = [];

	//See if the path egg exists:
	var pathegg = find_nearest(SHAPE_PATH_EGG, 10);
	if (pathegg) {
		//It does; get the frame number:
		frnum = pathegg->get_item_frame();

		//Fill in the setflags array with the flags which are set:
		var currflag = frnum;
		var index = 4;
		while (index > 0) {
			if (currflag >= pathegg_frame_flags[index]) {
				setflags = [index - 1, setflags];
				currflag -= pathegg_frame_flags[index];
			}
			index -= 1;
		}
	}

	var objshape;
	//Check to see if we must detect items of principle or
	//the Vortex Cube:
	if (qual < 3) {
		objshape = SHAPE_ITEM_OF_PRINCIPLE;
	} else {
		objshape = SHAPE_VORTEX_CUBE;
	}

	//Find the object:
	var obj = find_nearest(objshape, 0);

	//No needed object nearby; check if we need to
	//delete the corresponding flag, as the item might
	//have been moved just now:
	if (!obj) {
		//No pathegg means nothing to do:
		if (!pathegg) {
			return;
		}

		//If the egg's quality is in the set flags, remove it:
		if (qual in setflags) {
			pathegg->set_item_frame(frnum - frflag);
			//Just to make sure:
			gflags[CODEX_ALL_ITEMS_IN_PLACE] = false;
		}

		//Leave now:
		return;
	}

	//Get the item's frame:
	var objframe = obj->get_item_frame();

	//Get object position:
	struct<Position> pos = obj->get_object_position();

	//Check to see if the egg's quality matches the frame
	//of the items of principle, if any:
	if ((objshape == SHAPE_ITEM_OF_PRINCIPLE) && (objframe != qual)) {
		//It is not; have some user feedback and leave:
		UI_sprite_effect(ANIMATION_POOF, pos.x - 2, pos.y - 2, 0, 0, 0, -1);
		return;
	}

	//If we got here, the object exists and is in the correct
	//place; have some user feedback:
	UI_sprite_effect(ANIMATION_GREEN_BUBBLES, pos.x - 2, pos.y - 2, 0, 0, 0, -1);
	UI_play_sound_effect(67);

	//If the pathegg does not exist, create it:
	if (!pathegg) {
		pathegg = UI_create_new_object(SHAPE_PATH_EGG);
		UI_update_last_created([2719, 2797, 4]);
	}

	//The pathegg always exists at this point; see if the flag is
	//already set (shouldn't happen):
	if (qual in setflags) {
		//Nothing to do; leave:
		return;
	} else {
		//It is not; set the flag:
		pathegg->set_item_frame(frnum + frflag);
		setflags = [setflags, qual];
	}

	//See if all items are in place:
	if (UI_get_array_size(setflags) == 4) {
		//Yes, they are:
		gflags[CODEX_ALL_ITEMS_IN_PLACE] = true;
	} else {
		//No, they are not:
		gflags[CODEX_ALL_ITEMS_IN_PLACE] = false;
	}
}

void eggDeleteTimelord object#() () {
	if (event != EGG) {
		return;
	}
	if (gflags[BROKE_SPHERE]) {
		var objs = UI_find_nearby(item, SHAPE_STATUE, 10, MASK_NONE);
		objs = [objs, UI_find_nearby(item, SHAPE_TIME_BARRIER, 10, MASK_TRANSLUCENT)];
		for (obj in objs) {
			obj->remove_item();
		}
		remove_item();
	}
}

void eggReturnedItemsOfPrinciple object#() () {
	if (event != EGG) {
		return;
	}
	var obj = find_nearest(SHAPE_ITEM_OF_PRINCIPLE, 1);
	if (!obj) {
		return;
	}
	if (gflags[RELOCATE_CODEX_QUEST] && (get_item_quality() == obj->get_item_frame())) {
		//Give experience to Avatar if he returns the items of Principle
		//to where they belong:
		giveExperience(75);
		remove_item();
	}
}

void eggsPartyLocationBarks object#(0x621) () {
	var qual = get_item_quality();
	var msg;
	var npcnum;
	var istalk;
	if (event != EGG) {
		abort;
	}
	if (qual < 44) {
		eggsPartyLocationBarks.original();
		abort;
	} else if (qual == 44) {
		msg = "The Shrine of the Codex!";
		npcnum = SHAMINO;
		istalk = false;
	} else if (qual == 45) {
		msg = false;
		if (gflags[RELOCATE_CODEX_QUEST]) {
			var num = PARTY->count_objects(SHAPE_LENS, QUALITY_ANY, FRAME_ANY);
			if (num != 2) {
				if (num == 0) {
					msg = "You left the lenses behind!";
				} else {
					msg = "You left one of the lenses behind!";
				}
			}
			num = PARTY->count_objects(SHAPE_ITEM_OF_PRINCIPLE, QUALITY_ANY, FRAME_ANY);
			if (num != 3) {
				var has_book = contHasItemCount(PARTY, 1, SHAPE_ITEM_OF_PRINCIPLE, QUALITY_ANY, 0);
				var has_candle = contHasItemCount(PARTY, 1, SHAPE_ITEM_OF_PRINCIPLE, QUALITY_ANY, 1);
				var has_bell = contHasItemCount(PARTY, 1, SHAPE_ITEM_OF_PRINCIPLE, QUALITY_ANY, 2);
				var princitems = [" the Book of Truth", " the Candle of Love", " the Bell of Courage"];
				var index = 0;

				if (msg) {
					msg += " You also forgot";
				} else {
					msg = "You left";
				}
				if (num == 0) {
					msg += " the three Items of Principle!";
				} else if (num == 1) {
					if (has_book) {
						index = [2, 3];
					} else if (has_candle) {
						index = [1, 3];
					} else if (has_bell) {
						index = [1, 2];
					}
					msg += princitems[index[1]] + " and" + princitems[index[2]] + "!";
				} else if (num == 2) {
					if (!has_book) {
						index = 1;
					} else if (!has_candle) {
						index = 2;
					} else if (!has_bell) {
						index = 3;
					}
					msg += princitems[index] + "!";
				}
			}
			if (!contHasItemCount(PARTY, 1, SHAPE_VORTEX_CUBE, QUALITY_ANY, FRAME_ANY)) {
				if (msg) {
					msg += " And the Vortex Cube!";
				} else {
					msg = "You left the Vortex Cube behind!";
				}
			}
		} else {
			abort;
		}
		if (msg) {
			msg = "@" + msg + "@";
			npcnum = PARTY;
			istalk = true;
		}
	}

	if (npcnum == PARTY) {
		npcnum = randomPartyMember();
		if (npcnum == AVATAR) {
			abort;
		}
	}
	if (!isNearby(npcnum)) {
		abort;
	}

	if (!istalk) {
		UI_item_say(npcnum, "@" + msg + "@");
	} else if (istalk) {
		npcnum.say(msg);
	}
}
