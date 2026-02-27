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
 *	This source file contains usecode for the Keyring Quest.
 *	Specifically, it is the (modular) function for Laurianna.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

#include "npcs/related_functions/laurianna_dialog.uc"		//Laurianna's dialog functions

void Laurianna object#(0x494) () {
	var queststate = getQuestState();
	//Leave if Laundo is still alive:
	if (queststate < PLAYER_KILLED_MAGE) {
		return;
	}
	var bark;
	if (event == CLEAR_FLAGS) {
		if (get_distance(AVATAR) > 15) {
			//Loop if avatar is too far away:
			script item after 5 ticks call Laurianna, CLEAR_FLAGS;
			abort;
		}
		//Globals take a different meaning when Laurianna is freed;
		//this is done to avoid wasting globals on an ending quest:
		gflags[ACCEPTED_ZAURIEL_QUEST] = false;
		gflags[ZAURIEL_TOLD_LOCATION] = false;
		gflags[ZAURIEL_TELEPORTED] = false;
		gflags[ISLAND_NO_ONE_THERE] = false;
		gflags[GAVE_GEM_SUBQUEST] = false;
		gflags[PLAYER_USED_GEM] = false;

		//Delete any remaining eggs:
		deleteNearbyEggs(get_object_position(), 6);

		//Register that Laundo and his goons are gonners:
		set_npc_id(0);

		//Ensure met flag is not set:
		clear_item_flag(MET);

		script item {
			nohalt;
			call trueFreeze;
			wait 2;
			face find_direction(AVATAR);
			wait 2;
			say "@Avatar...@";
			wait 6;
			call lauriannaPrePotionDialog;
		}

		script AVATAR {
			nohalt;
			call trueFreeze;
			wait 6;
			face AVATAR->find_direction(item);
			wait 2;
			say "@Yes?@";
			wait 1;
			call trueUnfreeze;
		}
	} else if (get_npc_id() != 0) {
		//Abort if ID is not zero:
		return;
	} else if (event == DEATH) {
		//Nope, she can't die:
		abort;
	} else if (event == EVENT_TELEPORT) {
		//Move her to the destination and unfreeze her:
		item->trueUnfreeze();
		/*	DISABLED
		//Remove Laurianna's invulnerability:
		clear_item_flag(SI_TOURNAMENT);
		*/
		set_last_created();
		UI_update_last_created([0x103, 0x195, 0x0]);
		run_schedule();
		gflags[LAURIANNA_IN_YEW] = true;
	} else if (event == DOUBLECLICK) {
		//Player message according to flags:
		if (queststate >= POTION_WAS_USED) {
			bark = "@Hello again, Laurianna.@";
		} else {
			bark = "@Might we speak?@";
		}

		//SI-style start of dialog, although the NPC does not move to the Avatar:
		script AVATAR {
			call trueFreeze;
			face AVATAR->find_direction(item);
			actor frame standing;
			say bark;
			wait 10;
		}

		//Laurianna's response according to flags:
		if (queststate >= POTION_WAS_USED) {
			bark = "@Hi, Avatar!@";
		} else {
			bark = "@Thou dost have no potions, right?@";
		}

		//SI-style start of dialog, although the NPC does not move to the Avatar:
		script item {
			call trueFreeze;
			wait 5;
			face find_direction(AVATAR);
			actor frame standing;
			say bark;
			wait 8;
			call Laurianna;
		}
	} else if (event == SCRIPTED) {
		item->trueUnfreeze();
		AVATAR->trueUnfreeze();
		//Call appropriate dialog:
		if (queststate == LAURIANNA_MOVED_TO_YEW) {
			lauriannaYewDialog();
		} else if (queststate == LAURIANNA_IS_CURED) {
			lauriannaPostQuestDialog();
		} else if (queststate == POTION_WAS_USED) {
			lauriannaPostPotionDialog();
		} else {
			item->lauriannaPrePotionDialog();
		}
	} else if (event == PROXIMITY) {
		//Scgedule barks:
		var schedule = get_schedule_type();
		var barks;
		var partofday = UI_part_of_day();

		if (queststate == LAURIANNA_MOVED_TO_YEW) {
			scheduleBarks(item);
			abort;
		} else if (queststate == POTION_WAS_USED) {
			if ((partofday < 2) || (partofday == 7)) {
				schedule = SLEEP;
			}

			if (schedule != SLEEP) {
				barks = [
					"@Looks like rain...@",
					"@Canst thou take me to my father?@",
					"@I am -so- hungry... -In Mani Ylem!-@",
					"@I am tired...@"
				];
				if (count_objects(SHAPE_AMULET, QUALITY_ANY, FRAME_ANY)) {
					barks &= "Canst thou not help me with this necklace?";
				}
			} else {
				barks = "Z-z-z-z...";
			}
		} else if (queststate == PLAYER_KILLED_MAGE) {
			if ((partofday < 2) || (partofday == 7)) {
				schedule = SLEEP;
			}

			if (schedule != SLEEP) {
				barks = [
					"@Tra-la-la...@",
					"@Fire is so pretty...@",
					"@Can I burn those flowers?@",
					"@When do I get some food?@",
					"@Pretty bird. Can I kill it?@"
				];
			} else {
				barks = "Z-z-z-z...";
			}
		}

		script item say barks[UI_get_random(UI_get_array_size(barks))];
	}
}
