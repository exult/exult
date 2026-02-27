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
 *	Specifically, it contains the last cutscene of the quest.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

enum Ritual_levels {
	BEGIN_RITUAL						= 1,
	SPRITE_EFFECTS_LAURIANNA			= 2,
	ERECT_BARRIER						= 3,
	SPRITE_EFFECTS_ZAURIEL				= 4,
	DRAGON_SHAPESHIFT					= 5,
	PARTING_REMARKS						= 6,
	BEGIN_ZAURIEL_COMBAT				= 7,
	ZAURIEL_DIED						= 8,
	RITUAL_END							= 9
};

void zaurielTeleportPartyAround object#() () {
	//Get party list and append Laurianna:
	var party = [UI_get_party_list(), LAURIANNA->get_npc_object()];
	var dist = 20;
	//Get Zauriel's position:
	struct<Position> pos = get_object_position();
	//Find nearby eggs:
	var eggs = 	pos->find_nearby(SHAPE_EGG, dist, MASK_EGG);
	//Find nearby barriers:
	var barriers = pos->find_nearby(SHAPE_BARRIER, 3 * dist, MASK_TRANSLUCENT);
	var count = 0;
	struct<Position> new_pos;
	//The only four which are allowed to fight:
	var companions = [
		UI_get_avatar_ref(),
		SHAMINO->get_npc_object(),
		IOLO->get_npc_object(),
		DUPRE->get_npc_object()
	];

	for (egg in eggs) {
		//Each egg has a quality equal to the number
		//of a joinable NPC
		var qual = egg->get_item_quality();
		var npc;
		if (qual == 0) {
			//Quality 0 means it is for the avatar:
			npc = UI_get_avatar_ref();
		} else {
			//Get a reference to the NPC
			npc = qual->get_npc_object();
		}
		if (npc in party) {
			//The NPC is in the party
			new_pos = egg->get_object_position();
			if (!(npc in companions)) {
				//And he is not one of the 'companions'
				if (npc != LAURIANNA->get_npc_object()) {
					//Remove npc from party
					npc->remove_from_party();
				}
				//Place them in WAIT mode facing south:
				npc->set_schedule_type(WAIT);
				npc->set_item_frame_rot(STAND_SOUTH);

				//Move the appropriate barrier:
				count += 1;
				pos = [new_pos.x + 1, new_pos.y + 1, new_pos.z];
				barriers[count]->move_object(pos);
			}
			//Move the NPC to the egg:
			UI_sprite_effect(ANIMATION_TELEPORT, new_pos.x, new_pos.y, 0, 0, 0, -1);
			npc->move_object(new_pos);
		} else {
			//Remove the egg if the NPC is not here.
			egg->remove_item();
		}
	}
}

void zaurielUnfreezeFormerParty () {
	struct<Position> pos = get_object_position();
	//Find nearby eggs:
	var eggs = 	pos->find_nearby(SHAPE_EGG, 20, MASK_EGG);

	for (egg in eggs) {
		//Each egg has a quality equal to the number
		//of a joinable NPC
		var qual = egg->get_item_quality();
		var npc = qual->get_npc_object();
		npc->trueUnfreeze();
		egg->remove_item();
	}
}

void zaurielRitualCutscene object#() () {
	struct<Position> pos;
	var barrier;
	var body;
	var zauriel_journal;

	if (event == BEGIN_RITUAL) {
		//item = ZAURIEL

		//Prevents all party movement:
		freezeParty();

		//Make Laurianna invulnerable:
		pos = LAURIANNA->get_object_position();
		LAURIANNA->set_item_shape(SHAPE_LAURIANNA_MONSTER);
		LAURIANNA->set_last_created();
		UI_update_last_created(pos);

		pos = get_object_position();
		UI_sprite_effect(ANIMATION_FIREWORKS, pos.x, pos.y, 0, 0, 0, -1);

		item->begin_casting_mode();
		//Zauriel script:
		script item {
			//cast spells: freeze party (Vas An Por), create magical barrier around
			//Laurianna (Vas Ort Sanct Grav) and move party around (Vas In Por):
			nohalt;
			continue;
			sfx 110;
			face north;
			say "@Vas An Por@";
			actor frame standing;
			wait 1;
			actor frame cast_up;
			wait 1;
			next frame;
			wait 1;
			previous frame;
			sfx 108;
			say "@Vas Ort Sanct Grav@";
			wait 2;
			next frame;
			wait 2;
			previous frame;
			wait 2;
			next frame;
			wait 2;
			previous frame;
			sfx SOUND_TELEPORT;
			say "@Vas In Por@";

			//Actually teleport the party:
			call zaurielTeleportPartyAround;

			//Long casting ritual:
			wait 2;
			actor frame raise_2h;
			wait 2;
			next frame;
			wait 2;
			next frame;
			wait 3;

			//Make more spell effects:
			call zaurielRitualCutscene, SPRITE_EFFECTS_ZAURIEL;

			//Cast the dragon form spell (Rel An-Quas Ailem In BAL-ZEN)
			actor frame standing;
			say "@Rel An-Quas...@";
			actor frame cast_up;
			wait 2;
			sfx 66;
			wait 2;
			next frame;
			wait 2;
			previous frame;
			wait 2;
			say "@...Ailem In BAL-ZEN@";wait 2;

			//Actually shapeshift Zauriel:
			call zaurielRitualCutscene, DRAGON_SHAPESHIFT;

			//Finish the ritual:
			next frame;
			sfx 64;
			wait 2;
			previous frame;
			wait 2;
			next frame;
			wait 2;
			face west;
			wait 2;

			//Say last words and wait a bit:
			call zaurielRitualCutscene, PARTING_REMARKS;
			wait 2;

			//Begin combat:
			call zaurielRitualCutscene, BEGIN_ZAURIEL_COMBAT;
		}

		var party = [UI_get_party_list(), LAURIANNA];
		var companions = [
			UI_get_avatar_ref(),
			SHAMINO->get_npc_object(),
			IOLO->get_npc_object(),
			DUPRE->get_npc_object()
		];
		for (npc in party) {
			if (!(npc in companions)) {
				script npc {
					nohalt;
					continue;
					wait 15;
					//Make the spell effects:
					call zaurielRitualCutscene, SPRITE_EFFECTS_LAURIANNA;
					wait 12;
					//Create the magical barrier:
					call zaurielRitualCutscene, ERECT_BARRIER;
				}
			}
		}

		//Avatar script:
		script AVATAR {
			nohalt;
			continue;
			wait 2;
			say "@I-I... I can't move!@";
		}
	} else if (event == SPRITE_EFFECTS_LAURIANNA) {
		//item = LAURIANNA
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_CIRCLE_BARRIER, pos.x, pos.y, 0, 0, 0, -1);
	} else if (event == ERECT_BARRIER) {
		//item varies
		//Create the protective barrier:
		pos = get_object_position();
		barrier = UI_create_new_object(SHAPE_BARRIER);

		//Offset the position so that the barrier is in the right place:
		pos.x += 1;
		pos.y += 1;

		//Move the barrier to the target position:
		UI_update_last_created(pos);

		//Make the barrier temporary:
		barrier->set_item_flag(TEMPORARY);
	} else if (event == SPRITE_EFFECTS_ZAURIEL) {
		//item = ZAURIEL
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_FIREWORKS, pos.x, pos.y, 0, 0, 0, -1);
	} else if (event == DRAGON_SHAPESHIFT) {
		//item = ZAURIEL
		//Turn Zauriel into a dragon:
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, pos.x, pos.y, 0, 0, 0, -1);
		UI_play_sound_effect2(SOUND_BIG_BLAST, item);
		set_item_shape(SHAPE_DRAGON);
		set_item_frame(0);
		set_last_created();
		UI_update_last_created(pos);
	} else if (event == PARTING_REMARKS) {
		//item = ZAURIEL
		//Unfreeze the companions:
		AVATAR->trueUnfreeze();
		if (isNearby(IOLO)) {
			IOLO->trueUnfreeze();
		}
		if (isNearby(DUPRE)) {
			DUPRE->trueUnfreeze();
		}
		if (isNearby(SHAMINO)) {
			SHAMINO->trueUnfreeze();
		}

		//Speak using Dracothraxus' face:
		DRACOTHRAXUS_FACE.say("@I am truly sorry, Avatar, but I cannot think of a more worthy foe to bring about my demise!");
		say("@Let the world know that I fell in glorious battle for the fate of my daughter!@");
	} else if (event == BEGIN_ZAURIEL_COMBAT) {
		//item = ZAURIEL
		set_attack_mode(STRONGEST);
		//Have Zauriel start combat:
		item->trueUnfreeze();
		set_alignment(2);
		pos = get_object_position();
		set_schedule_type(IN_COMBAT);
	} else if (event == ZAURIEL_DIED) {
		//item = LAURIANNA
		//Unshapeshift Zauriel:
		body = find_nearest(SHAPE_LARGE_BODIES, 50);
		pos = body->get_object_position();
		body->set_item_shape(SHAPE_BODIES_1);
		body->set_item_frame(FRAME_MONK_BODY_1);
		//The final effects of the ritual:
		UI_play_sound_effect2(SOUND_BIG_BLAST, body);
		UI_sprite_effect(ANIMATION_CIRCLE_BARRIER, pos.x, pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_FIREWORKS, pos.x, pos.y, 0, 0, 0, -1);
		//Create the journal and place it on the body:
		zauriel_journal = UI_create_new_object(SHAPE_JOURNAL);
		zauriel_journal->set_item_quality(1);
		body->give_last_created();

		//parting comment and body disposal:
		script body {
			wait 3;
			say "@*gasp* T-t-thank thee *gasp* *cough* Avatar... *urgh*@";
			wait 12;
			remove;
		}

		unfreezeParty();

		script item after 16 ticks call zaurielRitualCutscene, RITUAL_END;
	} else if (event == RITUAL_END) {
		//item = LAURIANNA
		pos = get_object_position();
		//Final effects of the ritual:
		UI_play_sound_effect2(SOUND_BIG_BLAST, item);
		UI_sprite_effect(ANIMATION_CIRCLE_BARRIER, pos.x, pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_FIREWORKS, pos.x, pos.y, 0, 0, 0, -1);
		//delete all magical barriers:
		var dist = 20;
		var barriers = pos->find_nearby(SHAPE_BARRIER, dist, MASK_TRANSLUCENT);
		for (obj in barriers) {
			script obj remove;
		}
		zaurielUnfreezeFormerParty();

		item->trueUnfreeze();

		//Mark Laurianna's cure:
		gflags[LAURIANNA_CURED] = true;

		//Make Laurianna vulnerable again:
		pos = LAURIANNA->get_object_position();
		LAURIANNA->set_item_shape(SHAPE_LAURIANNA);
		LAURIANNA->set_last_created();
		UI_update_last_created(pos);

		//Start dialog:
		script item after 10 ticks {
			nohalt;
			finish;
			say "@I-I am... cured?@";
			wait 30;
			call Laurianna;
		}

		//More feedback:
		randomPartyBark(["@Look!@", "@A journal!@"]);
	}
}
