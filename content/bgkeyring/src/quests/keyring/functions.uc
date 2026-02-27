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
 *	This source file contains some functions specific to the Keyring Quest.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void monsterStats (var monster, var stats) {
	var stat_value;
	//IDs of the properties to set:
	var stat_ids = [STRENGTH, DEXTERITY, INTELLIGENCE, HEALTH, COMBAT];
	for (statnum in stat_ids) {
		//For each stat in the array, set the monster's attribute:
		stat_value = monster->get_npc_prop(statnum + 1);
		monster->set_npc_prop(statnum + 1, stats[statnum + 1] - stat_value);
	}
}

void monsterEquipment (var monster, var contshape, var arms, var arms_quality, var pouch_content, var pouch_frames, var pouch_quantities, var pouch_qualities) {
	var bag;
	var new_obj;
	var index;

	for (itemnum in arms with /*counter*/index) {
		//For each item in arms,
		//increment counter:
		//index += 1;
		//Create the object:
		new_obj = UI_create_new_object(itemnum);
		//Set quality:
		new_obj->set_item_quality(arms_quality[index]);
		//Give to monster:
		monster->give_last_created();
	}

	//Create a backpack:
	bag = UI_create_new_object(contshape);
	//Give to monster:
	monster->give_last_created();
	//Reset counter:
	index = 0;
	while (index < UI_get_array_size(pouch_content)) {
		//For each object in the array,
		//increment counter:
		index += 1;
		//Create object:
		new_obj = UI_create_new_object(pouch_content[index]);
		//Set frame:
		new_obj->set_item_frame(pouch_frames[index]);
		if (pouch_quantities[index] > 1) {
			//Set quantity where available:
			new_obj->set_item_quantity(pouch_quantities[index]);
		}
		//Set quality:
		new_obj->set_item_quality(pouch_qualities[index]);
		//Put in the backpack:
		bag->give_last_created();
	}
}

var monsterCreate (var shapenum, var npcid, var pos, var facedir, var stats, var arms, var arms_quality, var pouch_content, var pouch_frames, var pouch_quantities, var pouch_qualities) {
	//Create the monster:
	var monster = UI_create_new_object(shapenum);
	//Initially set monster to be neutral:
	monster->set_alignment(1);
	//Have the monster be in WAIT mode:
	monster->set_schedule_type(WAIT);
	//Make him temporary:
	monster->set_item_flag(TEMPORARY);

	if (!gflags[PLAYER_USED_GEM]) {
		//Ensure monster is invisible before the Avatar
		//uses the gem:
		monster->set_item_flag(DONT_RENDER);
	}

	//Set the monster's attributes:
	monsterStats(monster, stats);
	//Give him some equipment:
	monsterEquipment(monster, SHAPE_BACKPACK, arms, arms_quality, pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	//Set NPC ID to requested value:
	monster->set_npc_id(npcid);
	if (npcid == ID_MAGE_OR_GOON) {
		//If the ID is that of the mage and his goons, put
		//the monster in tournament mode:
		monster->set_item_flag(SI_TOURNAMENT);
	}

	//Set monster as last created:
	monster->set_last_created();
	//Update monster to desired spot:
	UI_update_last_created(pos);
	//Have monster face desired direction:
	script monster face facedir;
	return monster;
}

void createMageAndGoons () {
	//This function is a rather boring long list of equipment. You can just look
	//at the mage to see how it is done, and skip all the rest...
	var monster_stats;
	var leather_armor = [
		SHAPE_LEATHER_HELM, SHAPE_LEATHER_ARMOR, SHAPE_LEATHER_COLLAR,
		SHAPE_LEATHER_GLOVES, SHAPE_LEATHER_LEGGINGS, SHAPE_LEATHER_BOOTS
	];

	var chain_armor   = [
		SHAPE_CHAIN_COIF, SHAPE_CHAIN_ARMOUR, SHAPE_GORGET,
		SHAPE_GAUNTLETS, SHAPE_CHAIN_LEGGINGS, SHAPE_LEATHER_BOOTS
	];

	var plate_armor   = [
		SHAPE_GREAT_HELM, SHAPE_PLATE_ARMOUR, SHAPE_GORGET,
		SHAPE_GAUNTLETS, SHAPE_PLATE_LEGGINGS, SHAPE_LEATHER_BOOTS
	];

	var pouch_content;
	var pouch_frames;
	var pouch_quantities;
	var pouch_qualities;
	var monster_equip;
	var monster_qualities;


	//Create the mage and his equipment:
	monster_stats = [12, 12, 26, 12, 18];
	pouch_content = [
		SHAPE_LIGHTNING_WAND,
		SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT,
		SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT
	];
	pouch_frames = [
		0,
		0, 1, 2, 3,
		4, 5, 6, 7
	];
	pouch_quantities = [
		1,
		21, 17, 16, 14,
		24, 22, 17, 19
	];
	pouch_qualities = [
		80,
		0, 0, 0, 0,
		0, 0, 0, 0
	];
	monster_equip = [SHAPE_FIREBOLT, SHAPE_LIGHTNING, leather_armor];
	monster_qualities = [40, 40, 0, 0, 0, 0, 0, 0];

	var mage = monsterCreate(SHAPE_MAGE_MALE, ID_MAGE_OR_GOON, [254, 1170, 0], WEST,
			monster_stats, monster_equip, monster_qualities,
			pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	mage->set_usecode_fun(registerDeathOfMageOrGoon);


	pouch_qualities = [
		0,
		0, 0, 0, 0,
		0, 0, 0, 0
	];

	//Create the gargoyle and his equipment:
	monster_stats = [24, 12, 12, 24, 20];
	pouch_content	 = [
		SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY, SHAPE_JEWELRY,
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD
	];
	pouch_frames = [
		0,
		0, 1, 2, 3,
		0, 1, 2, 3,
		0, 1, 2, 3,
		0, 1, 2, 3,
		4, 5, 6, 7,
		4, 5, 6, 7,
		FRAME_VENISON, FRAME_VENISON, FRAME_VENISON, FRAME_VENISON
	];
	pouch_quantities = [
		1,
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1
	];
	monster_equip = [SHAPE_2H_HAMMER, chain_armor];
	monster_qualities = [0, 0, 0, 0, 0, 0, 0];

	var garg = monsterCreate(SHAPE_GARGOYLE_WARRIOR, ID_MAGE_OR_GOON, [263, 1170, 0], EAST,
			monster_stats, monster_equip, monster_qualities,
			pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	garg->set_usecode_fun(registerDeathOfMageOrGoon);


	//Create the troll and his equipment:
	monster_stats = [16, 10, 6, 16, 17];
	pouch_content	 = [
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD,
		SHAPE_GEM, SHAPE_GEM, SHAPE_GEM, SHAPE_GEM,
		SHAPE_GEM, SHAPE_GEM, SHAPE_GEM, SHAPE_GEM
	];
	pouch_frames = [
		FRAME_CHICKEN, FRAME_CHICKEN, FRAME_CHICKEN, FRAME_CHICKEN,
		0, 1, 2, 3,
		4, 5, 6, 7
	];
	pouch_quantities = [
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1
	];
	monster_equip = [SHAPE_MORNINGSTAR, SHAPE_SPIKEDSHIELD, leather_armor];
	monster_qualities = [0, 0, 0, 0, 0, 0, 0, 0];

	var troll = monsterCreate(SHAPE_TROLL, ID_MAGE_OR_GOON, [249, 1164, 0], WEST,
			monster_stats, monster_equip, monster_qualities,
			pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	troll->set_usecode_fun(registerDeathOfMageOrGoon);


	//Create the cyclops and his equipment:
	monster_stats = [22, 17, 6, 22, 18];
	pouch_content	 = [
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD,
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_GOLD_NUGGET, SHAPE_GOLD_NUGGET,
		SHAPE_GOLD_NUGGET, SHAPE_GOLD_NUGGET, SHAPE_GOLD_NUGGET, SHAPE_GOLD_NUGGET
	];
	pouch_frames = [
		FRAME_FLOUNDER, FRAME_FLOUNDER, FRAME_FLOUNDER, FRAME_FLOUNDER,
		FRAME_FLOUNDER, FRAME_FLOUNDER, 0, 0,
		0, 1, 1, 1
	];
	pouch_quantities = [
		1, 1, 1, 1,
		1, 1, 1, 1,
		1, 1, 1, 1
	];
	monster_equip = [SHAPE_HALBERD, leather_armor];
	monster_qualities = [0, 0, 0, 0, 0, 0, 0];

	var cyclops = monsterCreate(SHAPE_CYCLOPS, ID_MAGE_OR_GOON, [241, 1164, 0], EAST,
			monster_stats, monster_equip, monster_qualities,
			pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	cyclops->set_usecode_fun(registerDeathOfMageOrGoon);


	//Create the fighter and his equipment:
	monster_stats = [20, 18, 12, 20, 22];
	pouch_content = [
		SHAPE_GOLD_BAR, SHAPE_GOLD_COIN,
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD, SHAPE_FOOD,
		SHAPE_FOOD, SHAPE_FOOD, SHAPE_BOTTLE, SHAPE_BOTTLE
	];
	pouch_frames = [
		0, 7,
		FRAME_APPLE, FRAME_APPLE, FRAME_CARROT, FRAME_CARROT,
		FRAME_SAUSAGES, FRAME_MUTTON, 13, 13
	];
	pouch_quantities = [
		1, 35,
		1, 1, 1, 1,
		1, 1, 1, 1
	];
	monster_equip = [SHAPE_2H_SWORD, plate_armor];
	monster_qualities = [0, 0, 0, 0, 0, 0, 0];

	var fighter = monsterCreate(SHAPE_FIGHTER_MALE, ID_MAGE_OR_GOON, [240, 1177, 0], WEST,
			monster_stats, monster_equip,  monster_qualities,
			pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	fighter->set_usecode_fun(registerDeathOfMageOrGoon);
}

extern void deathOfJoneleth object#() ();

var createLichAndGems () {
	var gem_count;
	var gem;
	var counter;

	var pouch;
	var liche;
	var egg;

	//Get random number of gems to create:
	gem_count = UI_get_random(4);
	//Create a pouch for the gems:
	pouch = UI_create_new_object(SHAPE_BAG);
	//Set pouch to temporary:
	pouch->set_item_flag(TEMPORARY);

	while (counter < gem_count) {
		//Increment counter:
		counter += 1;
		//Create a gem:
		gem = UI_create_new_object(SHAPE_GEM_OF_DISPELLING);
		//Make it temporary:
		gem->set_item_flag(TEMPORARY);
		//Get a random value for the gem's quality:
		var rand = UI_get_random(40) - 1;
		gem->set_item_quality(rand);
		//Place gem on pouch:
		pouch->give_last_created();
	}

	//Create Joneleth the liche:
	liche = UI_create_new_object(SHAPE_LICHE);

	//Place him in tournament mode:
	liche->set_item_flag(SI_TOURNAMENT);
	//Set his ID to the correct value:
	liche->set_npc_id(ID_JONELETH);
	//Make him hostile:
	liche->set_alignment(2);
	//Make him temporary:
	liche->set_item_flag(TEMPORARY);
	//Place him in WAIT mode:
	liche->set_schedule_type(WAIT);
	//Place him in WAIT mode:
	liche->set_usecode_fun(deathOfJoneleth);
	//Put him in the correct spot:
	UI_update_last_created([0x717, 0x825,0x0]);

	var spell;
	//Create firebolt spell:
	spell = UI_create_new_object(SHAPE_FIREBOLT);
	//Mark as temporary:
	spell->set_item_flag(TEMPORARY);
	//Give to Joneleth:
	liche->give_last_created();

	//Create lightning spell:
	spell = UI_create_new_object(SHAPE_LIGHTNING);
	//Mark as temporary:
	spell->set_item_flag(TEMPORARY);
	//Give to Joneleth:
	liche->give_last_created();

	//Set the pouch as being the last created item:
	pouch->set_last_created();
	//Give it to Joneleth:
	liche->give_last_created();

	//Make Joneleth face south:
	script liche face south;

	return liche;
}

void deleteNearbyEggs (var pos, var dist) {
	//Find all nearby eggs within dist of pos:
	var eggs = pos->find_nearby(SHAPE_EGG, dist, MASK_EGG);
	//Halt scripts for item:
	halt_scheduled();
	for (egg in eggs) {
		//For each egg found,
		if (egg->get_item_frame() == 7) {
			//Delete egg if it is an usecode egg:
			egg->remove_item();
		}
	}
}

var getQuestState () {
	//Boring function that returns a number indicating the
	//overall progress of the Keyring Quest:
	if (gflags[MAGE_KILLED]) {
		if (gflags[LAURIANNA_IN_YEW]) {
			return LAURIANNA_MOVED_TO_YEW;
		} else if (gflags[LAURIANNA_CURED]) {
			return LAURIANNA_IS_CURED;
		} else if (gflags[LAURIANNA_DRANK_POTION]) {
			return POTION_WAS_USED;
		} else {
			return PLAYER_KILLED_MAGE;
		}
	} else if (gflags[PLAYER_USED_GEM]) {
		return GEM_USED;
	} else if (gflags[GAVE_GEM_SUBQUEST]) {
		if (ZAURIEL->count_objects(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY)) {
			return TOLD_ABOUT_GEM;
		} else {
			return PLAYER_HAS_GEM;
		}
	} else if (gflags[ISLAND_NO_ONE_THERE]) {
		return NO_ONE_THERE;
	} else if (gflags[ZAURIEL_TELEPORTED]) {
		return QUEST_ACCEPTED;
	} else {
		return NOT_STARTED;
	}
}
