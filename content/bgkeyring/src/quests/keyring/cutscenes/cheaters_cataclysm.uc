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
 *	This source file contains some functions specific to a cutscene
 *	from the Keyring Quest. This happens only if the player hackmoves
 *	Laurianna (with roots) near her father. Maybe I will change it
 *	in the future to trigger when the player takes too long to
 *	bring Laurianna to her father, but not yet.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

enum Cataclism_levels {
	LAURIANNA_DIES						= 1,
	LAURIANNA_INSANE					= 2,
	CATACLISM_START						= 3,
	ARMAGEDDON							= 4,
	END_GAME							= 5
};

void beginCataclysm object#() () {
	if ((event == LAURIANNA_DIES) || (event == LAURIANNA_INSANE)) {
		//Begin a magic storm:
		gflags[MAGIC_STORM_SPELL] = true;
		UI_set_weather(3);

		//Get party list and append Zauriel to it:
		var party = UI_get_party_list();
		party &= ZAURIEL;

		//Make everyone SHY away:
		for (member in party) {
			member->set_schedule_type(SHY);
		}

		//Long, long earthquake:
		UI_earthquake(3600);

		if (event == LAURIANNA_DIES) {
			//Halt the above scripts:
			ZAURIEL->halt_scheduled();
			AVATAR->halt_scheduled();

			//Zauriel's message:
			ZAURIEL.say("@NO! Thou hast forgotten the necklace! NOOOOO!@");
			ZAURIEL.hide();

			//Unfreeze Zauriel and Avatar:
			ZAURIEL->unfreeze();
			AVATAR->unfreeze();

			struct<Position> pos = LAURIANNA->get_object_position();
			//Laurianna will lie down unconscious and trigger the cataclysm:
			UI_sprite_effect(ANIMATION_BIG_BLAST, pos.x, pos.y, 0, 0, 0, -1);
			script LAURIANNA {
				call trueFreeze;
				nohalt;
				sfx SOUND_BIG_BLAST;
				wait 2;
				say "@Aaahhhh...@";
				wait 3;
				actor frame bowing;
				wait 3;
				actor frame kneeling;
				wait 3;
				actor frame sleeping;
				wait 3;
				call beginCataclysm, CATACLISM_START;
			}
		} else {
			LAURIANNA->set_schedule_type(DANCE);
			//Laurianna has gone insane. Not used yet.
			script LAURIANNA {
				repeat 20 {
					wait 6;
					say "@Tra-la-la...@";
					wait 6;
					say "@Let the world burn!@";
					wait 6;
					say "@Fire is so pretty...@";
					wait 6;
					say "@Look! It burns!@";
					wait 6;
					say "@Wilt thou die? For me?@";
					wait 6;
					say "@Can I burn those flowers?@";
					wait 6;
					say "@Pretty birds. Dead birds.@";
				};
			}

			script AVATAR after 18 ticks call beginCataclysm, CATACLISM_START;
		}
	} else if (event == CATACLISM_START) {
		//Get party list and append Zauriel:
		var party = UI_get_party_list();
		party &= ZAURIEL;

		//Select one of them randomly:
		var rand = UI_get_random(UI_get_array_size(party));
		var member = party[rand];

		//Get position for the above NPC and for the Avatar:
		struct<Position> npc_pos = member->get_object_position();
		struct<Position> avatar_pos = AVATAR->get_object_position();
		//Get a random position around the Avatar:
		struct<Position> rand_pos = [
			avatar_pos.x + UI_get_random(41) - 21,
			avatar_pos.y + UI_get_random(41) - 21,
			0
		];

		//Random barks:
		var barks = ["@Argh!@", "@Ouch!@", "@That hurts!@", "@Please stop!@"];
		rand = UI_get_random(UI_get_array_size(barks));

		//Exploding lightning bolts:
		//One for the randomly selected party member:
		UI_sprite_effect(ANIMATION_BIG_BLAST, npc_pos.x, npc_pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_LIGHTNING, npc_pos.x, npc_pos.y, 0, 0, 0, -1);
		UI_play_sound_effect2(SOUND_BIG_BLAST, member);
		member->reduce_health(HEALTH, UI_die_roll(3, 9), MAGIC_DAMAGE);
		script member {
			say barks[rand];
			wait 2;
			actor frame bowing;
			wait 2;
			actor frame kneeling;
			wait 2;
			actor frame bowing;
			wait 2;
			actor frame standing;
		}

		//One for the random position:
		UI_sprite_effect(ANIMATION_BIG_BLAST, rand_pos.x, rand_pos.y, 0, 0, 0, -1);
		UI_sprite_effect(ANIMATION_LIGHTNING, rand_pos.x, rand_pos.y, 0, 0, 0, -1);
		UI_play_sound_effect2(SOUND_BIG_BLAST, AVATAR);

		//Queue the next lightning bolts:
		script AVATAR after 12 ticks call beginCataclysm, CATACLISM_START;

		if (AVATAR->get_npc_prop(HEALTH) <= 5) {
			//The Avatar is dying
			//Halt cataclysm:
			AVATAR->halt_scheduled();
			//make Avatar lie down to die:
			script AVATAR {
				call trueFreeze;
				say "@Too hurt...@";
				wait 6;
				actor frame bowing;
				wait 2;
				actor frame kneeling;
				wait 2;
				actor frame sleeping;
				wait 6;
				call beginCataclysm, END_GAME;
			}
		}
	} else if (event == END_GAME) {
		//Thus we reach the end of Britannia:
		AVATAR.say("With your dying breath, you see the world of Britannia being sundered appart by violent earthquakes... then, darkness surrounds you.");
		UI_restart_game();
	}
}
