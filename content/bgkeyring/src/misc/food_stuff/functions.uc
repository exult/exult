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

/* Butter-churning behaviours */

//Create the new butter object, beside the churn
void createButter object#() () {
	//create the new butter object
	var butter = UI_create_new_object(SHAPE_FOOD);
	if (!butter) {
		return;
	}
	butter->set_item_frame(FRAME_BUTTER);

	butter->set_item_flag(TEMPORARY);
	butter->set_item_flag(OKAY_TO_TAKE);

	struct<Position> target_pos = get_object_position();
	//place it on the middle right-hand side of the churn
	target_pos.x += 1;
	//target_pos.y -= 3;

	UI_update_last_created(target_pos);

	//Get a random party member to say something nice about butter
	if (UI_get_random(10) > 5) {
		randomPartyBark("@Mmmm, rich creamery butter.@");
	}
}

void churnButter () {
	//the source of the milk (either milk bottle or bucket of milk)
	var source;

	if (event == CHURN_WITH_BOTTLE) {
		//churned from bottle of milk
		//Remove milk from the party's stash
		source = AVATAR->get_cont_items(SHAPE_BOTTLE, QUALITY_ANY, FRAME_MILK);
		source->remove_item();
	} else if (event == CHURN_WITH_BUCKET) {
		//churned from bucket of milk
		source = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, FRAME_BUCKET_MILK);
		//empty the bucket
		source->set_item_frame(FRAME_BUCKET_EMPTY);
	} else if (event == CHURN_WITH_PITCHER) {
		//churned from pitcher of milk
		source = AVATAR->get_cont_items(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_PITCHER_MILK);
		//empty the pitcher
		source->set_item_frame(FRAME_PITCHER);
	}

	//Play the avatar's animation
	script AVATAR {
		nohalt;
		call freeze;
		face directionFromAvatar(item);

		//Lean down to put the milk into the churn
		actor frame standing;
		actor frame bowing;
		wait 3;
		actor frame standing;
		wait 1;

		//pump and grind
		repeat 3 {
			wait 1;
			actor frame strike_1h;
			wait 1;
			actor frame ready;
			wait 1;
			actor frame reach_1h;
		};
		actor frame standing;
		call unfreeze;
	}

	//Play the churn animation
	script item {
		//wait for the avatar to get up from putting the milk in
		wait 7;

		frame FRAME_CHURN;
		repeat 3 {
			sfx CHURN_SOUND;
			frame FRAME_CHURN_3;
			wait 1;
			frame FRAME_CHURN_2;
			wait 1;
			frame FRAME_CHURN;
		};

		//now, create the butter next to the churn
		call createButter;
	}
}

//go to the churn (called by Bottle and Bucket)
void gotoChurn (var churn, var milktype) {
	//User tried to use a churn that was inside something
	if (churn->get_container()) {
		randomPartySay("@Thou must first place the butter churn on the ground.@");
		return;
	}

	UI_close_gumps();

	var offsetx = [-1, -1, -1, 0, 0];
	var offsety = [0, 1, -1, 1, -1];

	//go to the churn and call churnButter() once you get there
	gotoObject(churn, offsetx, offsety, -3, churnButter, churn, milktype);
}

//This had to be overridden in order to separate the choosing of the target
//(now handled by UseEdible) from the act of eating the food
void consumeEdible (var food, var npc, var nutritional_value, var sound_effect) {
	if (!inParty(npc) || npc->get_item_flag(ASLEEP) || npc->get_item_flag(DEAD) ||
			npc->get_item_flag(PARALYZED)) {
		return;
	}

	//Added in custom behavior for some NPCs, when faced with the daunting prospect
	//of milk products
	//In future, lactose-intolerance could be handled by a custom flag
	var shapenum = food->get_item_shape();
	var framenum = food->get_item_frame();
	var cheesy_frames = [FRAME_BUTTER, FRAME_CHEESE1, FRAME_CHEESE2, FRAME_CHEESE3];

	if (npc == IOLO->get_npc_object() &&
		((shapenum == SHAPE_FOOD && framenum in cheesy_frames) ||
		(shapenum == SHAPE_BOTTLE && framenum == FRAME_MILK))) {
		//Poor Iolo is lactose intolerant.
		npc->item_say("@Nay, I am lactose-intolerant.@");
		return;
	} else if (npc == JAANA->get_npc_object() &&
		//Jaana's just health-conscious. Tch, these healers.
		//Tweaked 2005-03-17: Jaana has decided that cheese is ok,
		//so she only objects to butter now
		shapenum == SHAPE_FOOD && framenum == FRAME_BUTTER) {
		//sanctimonious cow
		npc.say("@No thank thee, I am watching my cholesterol. As shouldst thou!@");
		return;
	}

	//The current hunger level of the character
	var npc_food_level = npc->get_npc_prop(FOODLEVEL);
	//What hunger level the character will be at if fed this item
	var level_after_feeding = npc_food_level + nutritional_value;

	//Character is full already, do not pass go
	if (npc_food_level >= FOODLEVEL_FULL) {
		npc->item_say("@No, thank thee.@");
		return;
	}

	//Make sure it has been properly nicked
	stealItem(food);
	//Homph homph homph
	UI_play_sound_effect2(sound_effect, npc);
	if (shapenum == SHAPE_REAGENT) {
		// Garlic has quantity
		subtractQuantity(food);
	} else {
		food->remove_item();
	}

	//Used for randomising the barks
	var rand = UI_get_random(10);

	//What the character will say when fed
	var bark;

	//measure the NPC's current food level and their resulting food level to
	//determine barks

	//character was stuffed to start with
	if (npc_food_level >= FOODLEVEL_STUFFED) {
		if (framenum == FRAME_BUTTER && rand < 6) {
			bark = "@Urgh, my arteries...@";
		} else if (rand > 4 || (gflags[0x9B] && rand < 2)) {
			//no idea why it's checking a global flag - was this a debug
			//flag to test barks or something?
			bark = "@I'll soon be plump.@";
		}
	} else if (npc_food_level >= FOODLEVEL_PECKISH) {
		//character wasn't terribly hungry to start with
		//chowed down on garlic
		if (shapenum == SHAPE_REAGENT) {
			bark = "@Yum, garlic!@";
		} else if (level_after_feeding >= FOODLEVEL_FULL && rand < 3) {
			//fed with something really filling
			bark = "@Belch@";
		} else {
			bark = "@Ahh, very tasty.@";
		}
	} else if (level_after_feeding >= FOODLEVEL_STUFFED) {
		//for the following cases, the character was hungry to start with
		//character is now pretty full
		if (rand > 5) {
			bark = "@That hit the spot!@";
		} else {
			bark = "@Burp@";
		}
	} else if (level_after_feeding >= FOODLEVEL_WELLFED) {
		//character is now happily fed
		if (shapenum == SHAPE_REAGENT) {
			bark = "@Yum, garlic!@";
		} else {
			bark = "@Ah yes, much better.@";
		}
	} else if (level_after_feeding >= FOODLEVEL_PECKISH) {
		//character is still peckish
		if (rand > 5 || isAvatar(npc)) {
			bark = "@I am still hungry.@";
		} else {
			bark = "@May I have some more?@";
		}
	} else {
		//character is still starving
		if (rand > 5) {
			bark = "@More!@";
		} else {
			bark = "@I must have more!@";
		}
	}

	if (bark != "" && canTalk(npc)) {
		npc->item_say(bark);
	}
	npc->set_npc_prop(FOODLEVEL, nutritional_value);
	return;
}

//Called when food is clicked on - picks target and then feeds
//the food to the target. This has been rewritten so that the
//actual eating is handled by another function - this just
//controls the click behavior
void useEdible 0x813 (var food, var nutritional_value, var sound_effect) {
	struct<ObjPos> npc = UI_click_on_item();
	consumeEdible(food, npc, nutritional_value, sound_effect);
	return;
}
