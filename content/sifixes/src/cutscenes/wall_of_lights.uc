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
 *	I partially reimplemented the Wall of Lights for two reasons:
 * (1) absolutelly force the companions to be there and
 * (2) so that when they leave as the banes, their equipment is
 * 	  not dumped item by item in the ground, but remains in its
 * 	  respective containers.
 * I haven't succeeded yet in preventing their presence there after
 * the changes I made. As for (2), you can now leave the equipment
 * with them as it will not be so annoying to equip the automatons
 * that will carry the equipment back to the companions.
 */

// externs
extern void partySurroundBatlin object#(0x61D) ();
extern void killEveryone object#(0x7D8) ();

const int BATLIN_FACE = -287;

void BatlinAtWallOfLights object#(0x73B) () {
	struct<Position> pos;
	var companions = [SHAMINO, DUPRE, IOLO];
	var baneshapes = [SHAPE_ANTISHAMINO, SHAPE_ANTIDUPRE, SHAPE_MAD_IOLO];
	var npc;
	var newbaneshape;

	if ((get_item_quality() == 255) && (event == EGG)) {
		var index;
		var max;

		pos = AVATAR->get_object_position();
		pos.x -= 4;
		pos.y += 10;
		for (npc in companions with index to max) {
			// Absolutely ensure that the companions will be there for the scene:
			npc->clear_item_flag(DEAD);
			npc->clear_item_flag(ASLEEP);
			npc->clear_item_flag(CHARMED);
			npc->clear_item_flag(CURSED);
			npc->clear_item_flag(PARALYZED);
			npc->clear_item_flag(POISONED);
			npc->clear_item_flag(IN_ACTION);
			npc->clear_item_flag(DONT_MOVE);
			npc->clear_item_flag(CONFUSED);

			// Do not let even the Avatar prevent them from being there:
			npc->set_item_flag(SI_TOURNAMENT);

			if (npc->get_distance(AVATAR) > 15) {
				pos.x += index * 2;
				npc->move_object(pos);
			}
		}
		BatlinAtWallOfLights.original();
		abort;
	} else if (event == EGG) {
		pos = AVATAR->get_object_position();
		pos.x -= 4;
		pos.y += 5;
		for (npc in companions with index to max) {
			if (!npc->npc_nearby()) {
				npc->set_last_created();
				pos.x += index * 2;
				UI_update_last_created(pos);
			}
		}
		BatlinAtWallOfLights.original();
		abort;
	} else if (event == SCRIPTED) {
		var pathegg = getPathEgg(2, 1);
		var pathegg_quality = pathegg->get_item_quality();

		if ((pathegg_quality > 3) && (pathegg_quality < 7)) {
			npc = companions[(pathegg_quality - 3)];
			UI_play_sound_effect(0x77);

			if (pathegg_quality > 4) {
				pos = npc->get_object_position();
				UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
			}

			var body = UI_find_object(-359, SHAPE_BODIES_2, QUALITY_ANY, 16);
			var dir = npc->find_direction(body);
			struct<Position> offsets = [0, 0];

			if (dir in [NORTHWEST, NORTH, NORTHEAST]) {
				offsets.y = -1;
			} else if (dir in [SOUTHWEST, SOUTH, SOUTHEAST]) {
				offsets.y = 1;
			}

			if (dir in [NORTHEAST, EAST, SOUTHEAST]) {
				offsets.x = 1;
			} else if (dir in [NORTHWEST, WEST, SOUTHWEST]) {
				offsets.x = -1;
			}

			var dist = npc->get_distance(body);
			offsets.x *= dist;
			offsets.y *= dist;
			pos = npc->get_object_position();
			pos.x += offsets.x;
			pos.y += offsets.y;
			offsets.x = -1 * offsets.x;
			offsets.y = -1 * offsets.y;
			UI_sprite_effect(ANIMATION_RED_SWIRL, pos.x, pos.y, offsets.x, offsets.y, 0, dist);
			pathegg->set_item_quality(pathegg_quality + 1);

			script pathegg {
				nohalt;
				wait (dist + 10);
				call BatlinAtWallOfLights;
			}

			newbaneshape = baneshapes[(pathegg_quality - 3)];

			npc->remove_from_party();
			pos = npc->get_object_position();
			struct<Position> objpos = [0x6B + 3 * npc, 0x36, 0];

			npc->move_object(objpos);
			setNewSchedules(npc, objpos.x, objpos.y, WAIT);

			var baneframe = npc->get_item_frame();
			var baneobj = newbaneshape->create_new_object2(pos);
			baneobj->set_item_frame(baneframe);
			baneobj->set_schedule_type(WAIT);
			UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
			abort;
		} else if (pathegg_quality == 7) {
			const int ANARCHY = -290;
			const int WANTONNESS = -291;
			const int INSANITY = -292;
			UI_show_npc_face0(ANARCHY, 0);
			say("@I am Anarchy! Hahaha!@");
			say("@The world shall quail before me! Wrong shall become right! And right shall become lost! Nothing shall escape my touch!@");
			UI_remove_npc_face0();

			UI_show_npc_face0(WANTONNESS, 0);
			say("@Hahaha! I am the Wantonness Bane!@");
			say("@Wherever I pass, people shall frolic and sate their wild desires! I shall drive thee to feed thy darkest hungers!@");
			UI_remove_npc_face0();

			UI_show_npc_face0(INSANITY, 0);
			say("@I -- hahaha! -- am the Insanity Bane! Hahaha!@");
			say("@All those who fall within my shadow shall have their reason clouded and their wits addled! Their greatest love shall lie in the rubbish they once loathed! Hahaha!@");
			UI_remove_npc_face0();

			pos = AVATAR->get_object_position();
			UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);

			var banenpc;

			for (newbaneshape in baneshapes with index to max) {
				banenpc = AVATAR->find_nearby(newbaneshape, 40, 0);
				if (banenpc) {
					pos = banenpc->get_object_position();
					banenpc->remove_item();
					dropAllItems(companions[index], pos);
					npc->clear_item_flag(SI_TOURNAMENT);
					npc->kill_npc();
				}
			}

			if (BOYDON->npc_nearby()) {
				pos = BOYDON->get_object_position();
				script BOYDON {
					nohalt;
					wait 7;
					hit 55, NORMAL_DAMAGE;
				}
			}
			UI_play_sound_effect(0x2A);
			gflags[SERPENT_GWANI_HORN_SPEECH] = true;
			script getPathEgg(5, 4) after 40 ticks {
				nohalt;
				call startSerpentSpeechViaRing;
			}
			pathegg->set_item_quality(0);
			AVATAR->clear_item_flag(DONT_MOVE);
			UI_set_weather(0);
			abort;
		} else {
			BatlinAtWallOfLights.original();
		}
	}
}
