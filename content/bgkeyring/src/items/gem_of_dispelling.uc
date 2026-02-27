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
 *	This source file contains the usecode for Gems of Dispelling.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Gem_Of_Dispelling shape#(0x451) () {
	if (event != DOUBLECLICK) {
		return;
	}

	//Get gem's quality:
	var gem_quality = get_item_quality();
	//Get quest state:
	var quest_state = getQuestState();
	if ((quest_state == PLAYER_HAS_GEM) && (gem_quality == 0)) {
		//If the gem is good, and Zauriel has fixed/made/told so to the player,
		if (AVATAR->get_distance(LAURIANNA) <= 25) {
			//and if the Avatar is close enough to Laurianna,
			//close all gumps:
			UI_close_gumps();
			//Description of action:
			AVATAR.say("You pick up the gem with your hand and firmly crush it -- although, to your surprise, the gem is far easier to crush than you had anticipated.");
			say("You wait for the spell to take effect as the magical energies course through the Ether.");
			AVATAR.hide();

			//Give the amulet to Laurianna:
			var amulet = UI_create_new_object(SHAPE_AMULET);
			LAURIANNA->give_last_created();
			//Make Laurianna have roots:
			struct<Position> pos = LAURIANNA->get_object_position();
			LAURIANNA->set_item_shape(SHAPE_LAURIANNA_ROOTED);
			LAURIANNA->set_last_created();
			UI_update_last_created(pos);

			//Create Laundo and his goons:
			createMageAndGoons();

			//Done here since the mage & goons must be invisible
			//at the start of the cutscene:
			gflags[PLAYER_USED_GEM] = true;

			//Remove the gem:
			remove_item();
			//Crushing animation and start of cutscene:
			script AVATAR {
				nohalt;
				finish;
				wait 1;
				actor frame standing;
				wait 2;
				actor frame raise_2h;
				sfx SOUND_GLASS_SHATTER;
				wait 2;
				actor frame strike_2h;
				wait 2;
				actor frame raise_2h;
				wait 1;
				actor frame standing;
				call beginCutsceneMageAndGoons, BEGIN_CUTSCENE;
			}
		} else {
			//Reproach Avatar for trying to waste the gem:
			randomPartySay("@Gems like this are -very- rare; we should not waste it without being sure!@");
		}
	} else if (quest_state == TOLD_ABOUT_GEM) {
		//Otherwise, warn player to seek Zauriel:
		randomPartySay("@These should be the gems mentioned by Zauriel. We should take them to him quickly!@");
	}
}
