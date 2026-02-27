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
 *	This source file contains usecode for ALL the eggs used in the Keyring Quest.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

void eggNoOneThere object#() () {
	if (event != EGG) {
		return;
	}
	//Since the party does NOT have the gem, we have a different cutscene.
	//Just make sure that the quest is running properly.
	if (getQuestState() == QUEST_ACCEPTED) {
		script AVATAR {
			nohalt;
			say "@How odd...@";
			wait 10;
			say "@There is no one here.@";
		}

		script randomPartyMember() after 25 ticks {
			nohalt;
			say "@Maybe we should talk...@";
			wait 12;
			say "@...to Zauriel again, Avatar.@";
		}

		//Register the event:
		gflags[ISLAND_NO_ONE_THERE] = true;

		//Delete the egg:
		remove_item();
	}
}

void eggDetectGem object#() () {
	if (event != EGG) {
		return;
	}
	if (getQuestState() == PLAYER_HAS_GEM) {
		//The player has the gem, so warn him it is time to use it:
		randomPartySay("@Look, at how brightly the gem is glowing, Avatar! Maybe it is time to use it...@");

		//Delete the egg:
		remove_item();
	}
}

void eggCreateMageAndGoonsGoons object#() () {
	if (event != EGG) {
		return;
	}
	if (getQuestState() >= GEM_USED) {
		//The player has already used the gem.
		if (!gflags[MAGE_KILLED] && !LAURIANNA->find_nearest(SHAPE_MAGE_MALE, 50)) {
			//Since the mage hasn't been killed, and is nowhere to be found, we
			//create him again (but with no cutscene this time):
			createMageAndGoons();

			//Reset the counter:
			LAURIANNA->set_npc_id(0);

			//Fireworks sprite, indicating that Laurianna is... absent:
			script LAURIANNA after 10 ticks call beginCutsceneMageAndGoons, FIREWORKS_LAURIANNA;

			//Make mage & goons hostile:
			script AVATAR after 10 ticks  call beginCutsceneMageAndGoons, BEGIN_COMBAT;
		} else {
			//The mage has died, even if his goons are still alive; maybe the
			//player died, or otherwise teleported away. In any case, the mage's
			//goons will have scattered in terror now that he is no longer
			//there to resurrect them. So, have Laurianna begin conversation
			//instead:
			script item after 20 ticks call Laurianna, CLEAR_FLAGS;
		}
	}
}

void eggCreateLiche object#() () {
	if (event != EGG) {
		return;
	}
	//Find Joneleth the liche:
	var liche = find_nearest(SHAPE_LICHE, 10);
	//If he is not there, and if the player knows about the Gems of Dispelling,
	//create him:
	if ((!liche) && (getQuestState() == TOLD_ABOUT_GEM)) {
		createLichAndGems();
	}
}

void eggLicheDialog object#() () {
	if (event != EGG) {
		return;
	}
	//Find Joneleth the liche:
	var liche = find_nearest(SHAPE_LICHE, 10);

	if (UI_is_pc_inside()) {
		//The Avatar is inside the hut; see if Joneleth exists:
		if (!liche) {
			//He does not; create him:
			liche = createLichAndGems();
		}

		//Delete the egg:
		remove_item();

		//Display Joneleth's face, and have him taunt the Avatar:
		KEYRING_ENEMY->show_npc_face(JONELETH_FACE);
		say("Before you stands a liche. @A living -thing- dares to intrude upon my domain!@ he hisses. @Brave, but foolish...");
		say("@Ah, I can sense thou hast come to steal my gems! Thou hast but one thing to expect, mortal: death. DIE!@");

		//Start fight:
		liche->set_schedule_type(IN_COMBAT);
	} else if (get_distance(AVATAR) <= 15) {
		//The Avatar is outside, so we keep on checking until s/he is inside:
		script item after 2 ticks call eggLicheDialog;
	}
}

void eggCreateSpiderEggs object#() () {
	if (event != EGG) {
		return;
	}
	//This function is needed only because monster eggs do not (yet)
	//support shape >= 1024.

	var egg;

	//See how many eggs we will create:
	var num_eggs = UI_die_roll(2, 4);

	while (num_eggs > 0) {
		//Create egg:
		egg = UI_create_new_object(SHAPE_SPIDER_EGG);
		//Set random frame:
		egg->set_item_frame(UI_die_roll(0, 1));
		//Mark as temporary:
		egg->set_item_flag(TEMPORARY);
		//The vertical offsets for each sucessive egg:
		var yoff = [0, 1, 0, 1];
		//Place egg in correct position:
		UI_update_last_created([0x0CE - num_eggs, 0x277 + yoff[num_eggs], 0x0]);
		//Decrement counter:
		num_eggs -= 1;
	}
}

void deleteLicheEggs object#() () {
	//Count the number of gems of dispelling have been picked up:
	var party_gems = PARTY->count_objects(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY);

	if (!party_gems) {
		//None; try again after a few ticks:
		script item after 10 ticks call deleteLicheEggs;
	} else {
		//One or more; delete the relevant eggs:
		deleteNearbyEggs([0x717, 0x825,0x0], 5);
	}
}

void deathOfJoneleth object#() () {
	if ((get_npc_id() == ID_JONELETH) && (event == DEATH)) {
		//Joneleth the liche has died
		//Remove him from tournament mode:
		clear_item_flag(SI_TOURNAMENT);
		//Kill him:
		script item hit 50, MAGIC_DAMAGE;
		//Start loop which will end when the Avatar picks
		//up at least one Gem of Dispelling:
		script AVATAR after 10 ticks {
			nohalt;
			call deleteLicheEggs;
		}
		abort;
	}
}
