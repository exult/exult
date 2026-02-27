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
 *	Specifically, it contains a cutscene where Zauiel makes
 *	the Blackrock potion.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

enum MakePotion_levels {
	CAST_TELEPORT						= 1,
	TELEPORT_WIDGETS					= 2,
	POTION_FADE_SCREEN					= 3,
	MOVE_TO_LAB							= 4,
	POTION_UNFADE_SCREEN				= 5,
	MAKE_POTION							= 6,
	PLACE_BLACKROCK						= 7,
	UPDATE_CAULDRON						= 8,
	DELETE_POTIONS						= 9,
	BEGIN_DIALOG_1						= 10,
	MOVE_TO_LBCASTLE					= 11,
	BEGIN_DIALOG_2						= 12,
	TELEPORT_TO_SKARABRAE				= 13,
	WALK_TO_CAULDRON_1					= 14,
	WALK_TO_CAULDRON_2					= 15,
	WALK_TO_POTION_1					= 16,
	WALK_TO_POTION_2					= 17,
	IS_AT_CAULDRON_1					= 18,
	IS_AT_CAULDRON_2					= 19,
	IS_AT_POTION_1						= 20,
	IS_AT_POTION_2						= 21
};

void zaurielMakePotion object#() () {
	struct<Position> pos;
	var party = UI_get_party_list();
	var casting;

	if (event == CAST_TELEPORT) {
		//item = ZAURIEL
		ZAURIEL->show_npc_face(0);
		say("@I shall teleport us all into my lab so that we have some privacy while I work on the potion.@");
		//Teleportation spell:
		script item {
			nohalt;
			finish;
			face south;
			call showCastingFrames;
			actor frame cast_up;
			wait 2;
			say "@Vas In Por@";
			next frame;
			wait 2;
			previous frame;
			wait 2;
			next frame;
			wait 2;
			previous frame;
			sfx SOUND_TELEPORT;
			call zaurielMakePotion, TELEPORT_WIDGETS;
			wait 8;
			call zaurielMakePotion, POTION_FADE_SCREEN;
			actor frame standing;
			wait 5;
			call zaurielMakePotion, MOVE_TO_LAB;
		}
		abort;
	} else if (event == TELEPORT_WIDGETS) {
		//item = ZAURIEL
		//The spell effect animation:
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
		for (npc in party) {
			pos = npc->get_object_position();
			UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
		}
	} else if (event == POTION_FADE_SCREEN) {
		//Fade to black:
		UI_fade_palette(12, 1, 0);
	} else if (event == MOVE_TO_LAB) {
		//item = ZAURIEL
		//Move Zauriel
		move_object([0x535, 0xA47, 0x0]);
		set_item_frame_rot(STAND_EAST);

		//Use the Avatar to unfade the screen:
		script AVATAR {
			face west;
			wait 2;
			call zaurielMakePotion, POTION_UNFADE_SCREEN;
			wait 5;
			call zaurielMakePotion, MAKE_POTION;
		}
		//Move the party:
		pos = [0x53D, 0xA46, 0x0];
		PARTY->move_object(pos);
	} else if (event == POTION_UNFADE_SCREEN) {
		//Unfade from black:
		UI_fade_palette(12, 1, 1);
	} else if (event == MAKE_POTION) {
		script AVATAR after 2 ticks call trueFreeze;

		ZAURIEL->show_npc_face(0);
		say("@There, I can work unfettered by other concerns now. Here, let me have the ingredients.@");
		//Take the ingredients away:
		UI_remove_party_items(1, SHAPE_BLACKROCK, QUALITY_ANY, FRAME_ANY, true);
		UI_remove_party_items(1, SHAPE_VENOM, QUALITY_ANY, FRAME_ANY, true);
		say("Zauriel takes the Blackrock and the Silver Serpent venom from you. @Thank thee, Avatar.@");
		//The cutscene:
		script ZAURIEL after 3 ticks {
			nohalt;
			finish;
			call zaurielMakePotion, WALK_TO_CAULDRON_1;
		}

	} else if (event >= WALK_TO_CAULDRON_1 && event <= WALK_TO_POTION_2) {
		var pos = [0x537, 0xa46, 0x0];
		var newevent;
		switch (event) {
			case WALK_TO_CAULDRON_1:
				newevent = IS_AT_CAULDRON_1;
				break;
			case WALK_TO_CAULDRON_2:
				newevent = IS_AT_CAULDRON_2;
				break;
			case WALK_TO_POTION_1:
				pos = [0x534, 0xa47, 0x0];
				newevent = IS_AT_POTION_1;
				break;
			case WALK_TO_POTION_2:
				pos = [0x534, 0xa49, 0x0];
				newevent = IS_AT_POTION_2;
				break;
		}
		ZAURIEL->si_path_run_usecode(pos, newevent, ZAURIEL, zaurielMakePotion, true);
	} else if (event >= IS_AT_CAULDRON_1 && event <= IS_AT_POTION_2) {
		if (event <= IS_AT_CAULDRON_2) {
			if (event == IS_AT_CAULDRON_1) {
				script ZAURIEL {
					nohalt;
					finish;
					actor frame standing;
					face east;
					wait 2;
					actor frame bowing;
					wait 2;
					call zaurielMakePotion, PLACE_BLACKROCK;
					actor frame standing;
					wait 2;
					call zaurielMakePotion, WALK_TO_POTION_1;
				};
			} else {
				script ZAURIEL {
					nohalt;
					finish;
					actor frame standing;
					face east;
					repeat 3 {
						actor frame standing;
						wait 2;
						actor frame ready;
						wait 2;
						call zaurielMakePotion, UPDATE_CAULDRON;
					};
					actor frame standing;
					wait 2;
					call zaurielMakePotion, BEGIN_DIALOG_1;
				}
			}
		} else {
			var newevent;
			if (event == IS_AT_POTION_1) {
				newevent = WALK_TO_POTION_2;
			} else {
				newevent = WALK_TO_CAULDRON_2;
			}
			script ZAURIEL {
				nohalt;
				finish;
				actor frame standing;
				face west;
				wait 2;
				actor frame ready;
				wait 2;
				call zaurielMakePotion, DELETE_POTIONS;
				actor frame standing;
				wait 2;
				call zaurielMakePotion, newevent;
			};
		}
	} else if (event == PLACE_BLACKROCK) {
		var blackrock = UI_create_new_object(SHAPE_BLACKROCK);
		UI_update_last_created([1337, 2630, 2]);
	} else if (event == DELETE_POTIONS) {
		var potions = ZAURIEL->find_nearest(SHAPE_BLACKROCK_POTION, 3);
		potions->remove_item();
		UI_play_sound_effect(SOUND_KEY);
	} else if (event == UPDATE_CAULDRON) {
		var cauldron = ZAURIEL->find_nearest(SHAPE_CAULDRON, 10);
		var blackrock = ZAURIEL->find_nearest(SHAPE_BLACKROCK, 10);
		var framemap = [ 4, -1,  3, 		0,  2];
		var sfxmap	 = [67, 67, 67, SOUND_KEY, 67];
		var frameindex = cauldron->get_item_frame() + 1;

		if (blackrock) {
			blackrock->UI_remove_item();
		}
		if (framemap[frameindex] != 0) {
			UI_sprite_effect(ANIMATION_POOF, 1336, 2629, 0, 0, 0, -1);
		}
		UI_play_sound_effect(sfxmap[frameindex]);
		cauldron->set_item_frame(framemap[frameindex]);
	} else if (event == BEGIN_DIALOG_1) {
		var potion;

		ZAURIEL->show_npc_face(0);
		say("Zauriel finally turns to you and hands you the potion. @Here, the potion is ready.");
		say("@Make good use of it, for I shall -not- give thee any more!@ In the vial is a glowing black potion.");

		//Try to create the potion in the Avatar's inventory:
		UI_set_last_created(get_cont_items(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY));
		if (!AVATAR->give_last_created()) {
			say("@Since thou art so overburdened, I shall place the potion on the ground.@");
			UI_update_last_created(AVATAR->get_object_position());
		}

		//Remind the player that a piece of Blackrock is still needed:
		var blackrock_count = PARTY->count_objects(SHAPE_BLACKROCK, QUALITY_ANY, FRAME_ANY);
		if (!blackrock_count) {
			say("@Remember that thou dost still need to gather a piece of Blackrock ore in order to neutralize the necklace!@");
		}
		say("@I shall now tell thee where my daughter is being held.@");
		add("Where are they?");

		converse (0) {
			case "Where are they?" (remove):
				say("@Yes, I was just about to say that! A little more patience, Avatar, if thou wilt!");
				say("@There is a small island to the north of Skara Brae; that is where the mage and his thugs are located.");
				say("@My divinations indicate that they are in the northwestern portion of the island.");
				say("@There -is- the issue that the island is disconnected from the mainland, being reachable only by boat or through flying...");
				say("@But thou art resourceful enough to bypass this minor obstacle.@");
				gflags[ZAURIEL_TOLD_LOCATION] = true;
		}

		say("@Now that everything is said, I shall take us back to where we were.@");
		//Teleportation animation:
		script item {
			nohalt;
			finish;
			face south;
			call showCastingFrames;
			actor frame cast_up;
			wait 2;
			say "@Vas In Por@";
			next frame;
			wait 2;
			previous frame;
			wait 2;
			next frame;
			wait 2;
			previous frame;
			sfx SOUND_TELEPORT;
			call zaurielMakePotion, TELEPORT_WIDGETS;
			wait 8;
			call zaurielMakePotion, POTION_FADE_SCREEN;
			actor frame standing;
			wait 5;
			call zaurielMakePotion, MOVE_TO_LBCASTLE;
		}
		abort;
	} else if (event == MOVE_TO_LBCASTLE) {
		//item = ZAURIEL
		AVATAR->trueUnfreeze();
		//Move Zauriel:
		move_object([0x3A2, 0x4E0, 0x0]);
		set_item_frame_rot(STAND_SOUTH);
		//Use Avatar to unfade screen:
		script AVATAR {
			face north;
			wait 2;
			call zaurielMakePotion, POTION_UNFADE_SCREEN;
			wait 5;
			call zaurielMakePotion, BEGIN_DIALOG_2;
		}
		//Move party:
		pos = [0x3A2, 0x4E6, 0x0];
		PARTY->move_object(pos);
	} else if (event == BEGIN_DIALOG_2) {
		ZAURIEL->show_npc_face(0);
		say("@Here we are again. Thou knowest now what needs to be done, so I ask thee to do so quickly.");
		say("@But before thou dost go, I must confess that I find this place far too crowded for me.");
		say("@When thou hast rescued my daughter, come meet me near the Skara Brae Moongate.");
		if (PARTY->count_objects(SHAPE_SEXTANT, QUALITY_ANY, FRAME_ANY)) {
			say("@Thy sextant shall guide thee: I shall be at 45 degrees south, 38 degrees west.@");
		} else {
			say("@Shouldst thou acquire a sextant, take this as a reference: I shall be at 45 degrees south, 38 degrees west.@");
		}

		giveExperience(100);
		//Fire the teleport animation and move Zauriel:
		gflags[ZAURIEL_TELEPORTED] = true;
		struct<Position> pos = ZAURIEL->get_object_position();
		UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
		script ZAURIEL {
			nohalt;
			say "@Meet me near Skara Brae!@";
			sfx SOUND_TELEPORT;
			wait 6;
			call zaurielMakePotion, TELEPORT_TO_SKARABRAE;
		}
	} else if (event == TELEPORT_TO_SKARABRAE) {
		//Move him to the destination:
		move_object([0x224, 0x630, 0x0]);
		set_item_frame_rot(STAND_SOUTH);
		set_new_schedules([DAWN,			MORNING,		AFTERNOON,		EVENING,		NIGHT],
						  [WAIT,			WAIT,			WAIT,			WAIT,			WAIT],
						  [0x224, 0x630,	0x224, 0x630,	0x224, 0x630,	0x224, 0x630,	0x224, 0x630]);
	}
}
