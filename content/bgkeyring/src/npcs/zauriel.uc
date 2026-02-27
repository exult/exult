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
 *	Specifically, this is the (modular) function for Zauriel.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

#include "npcs/related_functions/zauriel_dialog.uc"			//Zauriel's dialog functions

void Zauriel object#(0x48B) () {
	var met_zauriel;
	var bark;

	//For the main quest itself:
	//See if the daughter is near the father:
	var laurianna_near = (get_distance(LAURIANNA) <= 20);

	//For the "destroy Britannia" sequence:
	var has_amulet;

	//The current quest's state:
	var quest_state = getQuestState();

	//If she is near, check to see if she has already teleported; for fun, also see if she
	//has the amulet:
	if ((event == DEATH) && (get_item_shape() == SHAPE_DRAGON)) {
		clear_item_flag(SI_TOURNAMENT);
		script item hit 50, MAGIC_DAMAGE;
		script LAURIANNA after 6 ticks call zaurielRitualCutscene, ZAURIEL_DIED;
		abort;
	} else if (event == DEATH) {
		/*	DISABLED
		if (!in_usecode()) {
			item->begin_casting_mode();
			event = DOUBLECLICK;
			item->spellGreatHeal(item);
		}
		*/
		abort;
	} else if (laurianna_near) {
		//This should always return false unless the player hack moved Laurianna
		//near to Zauriel; this triggers the end of Birtannia:
		has_amulet = LAURIANNA->count_objects(SHAPE_AMULET, QUALITY_ANY, FRAME_ANY);
	} else if ((event == SCRIPTED) && (quest_state >= GEM_USED)) {
		//Player used the gem
		var barks;
		if (quest_state == GEM_USED) {
			//But not killed Laundo:
			barks = ["@Thou hast used the gem!@", "@Find and kill the mage!@", "@Rescue my daughter!@"];
		} else {
			//But is not with Laurianna:
			barks = ["@Thou hast killed the mage!@", "@Why art thou not with my daughter?@", "@Go and rescue her!@"];
		}

		script item {
			face find_direction(AVATAR);
			say barks[1];
			wait 12;
			say barks[2];
			wait 12;
			say barks[3];
		}
		AVATAR->trueUnfreeze();
		item->trueUnfreeze();
		return;
	}

	var in_gem_subquest;

	if (quest_state == NO_ONE_THERE) {
		in_gem_subquest = true;
	} else {
		in_gem_subquest = false;
	}

	if (event == DOUBLECLICK) {
		//Close all gumps so the animation starts at once:
		UI_close_gumps();

		//Sees if the player has met Zauriel:
		met_zauriel = get_item_flag(MET);

		//Player message according to flags:
		if (!met_zauriel) {
			bark = "@Might we speak, sir?@";
		} else if (in_gem_subquest) {
			bark = "@There is a problem...@";
		} else if (!laurianna_near) {
			bark = "@Hello again, Zauriel.@";
		} else {
			bark = "@Here is thy daughter, Zauriel.@";
		}

		//SI-style start of dialog, although the NPC does not move to the Avatar:
		script AVATAR {
			call trueFreeze;
			face AVATAR->find_direction(item);
			actor frame standing;
			say bark;
			wait 10;
		}

		//Zauriel's response according to flags:
		if (!met_zauriel) {
			bark = "@The Avatar at last!@";
		} else if (in_gem_subquest) {
			bark = "@Please elaborate.@";
		} else if (!laurianna_near) {
			bark = "@How may I help?@";
		} else if (has_amulet) {
			bark = "@NO! THE AMULET! NOOOOO!@";
		} else {
			bark = "@Bless thee, Avatar!@";
		}

		//SI-style start of dialog, although the NPC does not move to the Avatar:
		script item {
			call trueFreeze;
			wait 5;
			face find_direction(AVATAR);
			actor frame standing;
			say bark;
			wait 8;
			call Zauriel;
		}

		if (laurianna_near) {
			if (has_amulet) {
				//This should only happen if the player cheated (hack moved) Laurianna
				//near to her father, as she will be unable to move for as long as she
				//is wearing the necklace. Let's have some fun destroying the world:

				//Start end of world:
				event = LAURIANNA_DIES;
				AVATAR->beginCataclysm();
			} else {
				//Laurianna has yet to teleport to her father; do so.
				LAURIANNA->remove_from_party();
				LAURIANNA->set_schedule_type(WAIT);

				var eggs = find_nearby(SHAPE_EGG, 20, MASK_EGG);
				struct<Position> pos;
				for (egg in eggs) {
					if (egg->get_item_quality() ==  -1 * LAURIANNA->get_npc_number()) {
						pos = egg->get_object_position();
						break;
					}
				}
				LAURIANNA->move_object(pos);
				UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
				//LAURIANNA->move_object([0x224, 0x628, 0x0]);
				//UI_sprite_effect(ANIMATION_TELEPORT, 0x224, 0x628, 0, 0, 0, -1);

				script LAURIANNA {
					sfx SOUND_TELEPORT;
					wait 4;
					face south;
					actor frame standing;
					say "@Father!@";
					wait 4;
				}
			}
		}
		return;
	} else if (event == SCRIPTED) {
		//Unfreeze Avatar and Zauriel:
		AVATAR->trueUnfreeze();
		item->trueUnfreeze();

		//Select which dialog to use:
		if (laurianna_near) {
			//The quest is finished.
			//Begin post-quest talk:
			zaurielLastTalk();
		} else if (quest_state>= NO_ONE_THERE) {
			zaurielTalkGemSubquest();
		} else {
			zaurielTalkPreQuest();
		}
	}
}
