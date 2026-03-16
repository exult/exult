/*
 *
 *  Copyright (C) 2026  The Exult Team
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
 */

void shapeBatlin shape#(0x13E)() {
	var is_female;
	var him_or_her;
	var var000A;
	var var000B;
	var var000C;
	var var000D;
	var action;

	is_female  = UI_is_pc_female();
	him_or_her = "him";
	if (is_female) {
		him_or_her = "her";
	}

	// The entire Spinebreaker/Eye of the moon vision sequence,
	// to show Batlin without the Fellowship medallion.
	if (event == PROXIMITY
		&& (gflags[DOING_SPINEBREAKER_VISION] && gflags[USING_FAR_SIGHT_MAGIC] && (!gflags[DID_SPINEBREAKER_VISION]))) {
		UI_set_npc_id(item, (UI_get_npc_id(item) + 1));

		if (UI_get_npc_id(item) == 1) {
			UI_fade_palette(12, 1, 1);
			delayedBark(item, "@So little time...@", 0);
			UI_play_music(52, getPathEgg(5, 1));
		}

		if (UI_get_npc_id(item) == 2) {
			delayedBark(item, "@...before the Avatar cometh.@", 0);
		}

		if (UI_get_npc_id(item) == 3) {
			UI_init_conversation();
			// Fixed: use frame 1 here, without the medallion.
			UI_show_npc_face0(FACE_BATLIN, 1);
			say("@I must prepare an appropriate welcome for mine old enemy!@");
			UI_end_conversation();
			var000D = [2346, 409, 0];
			UI_si_path_run_usecode(DEADEYE, var000D, 13, DEADEYE, 1152, false);
		}

		if (UI_get_npc_id(item) == 4) {
			delayedBark(item, "@Deadeye! Come here!@", 0);
		}

		if (UI_get_npc_id(item) == 5) {
			UI_init_conversation();
			// Fixed: use frame 1 here, without the medallion.
			UI_show_npc_face0(FACE_BATLIN, 1);
			say("@To thee I give the anchor's job... The Avatar must not enter the Shrine of Balance before the Wall of Lights "
				"is fully open. Thou must stop the Avatar, should the others fail.@");
			UI_show_npc_face1(FACE_DEADEYE, 0);
			say("@I'll split ", him_or_her, " from stem ta stern afore I let ", him_or_her, " pass, Batlin!@");
			say("@An' o' course there be another yearnin' for a chance ta meet up with ", him_or_her, " again, too...@");
			UI_remove_npc_face1();
			UI_set_conversation_slot(0);
			say("@Ah, yes... I had nearly forgotten.@");
			UI_set_schedule_type(DEADEYE, LOITER);
			UI_end_conversation();
		}

		if (UI_get_npc_id(item) == 6) {
			var000C = [2342, 391, 0];
			UI_si_path_run_usecode(BRUNT, var000C, 13, BRUNT, 1151, false);
			delayedBark(item, "@I must be certain...@", 0);
		}

		if (UI_get_npc_id(item) == 7) {
			delayedBark(item, "@Brunt!@", 0);
		}

		if (UI_get_npc_id(item) == 8) {
			UI_init_conversation();
			// Fixed: use frame 1 here, without the medallion.
			UI_show_npc_face0(FACE_BATLIN, 1);
			say("@Thou shalt be my door warden. I can count on thy strength to delay the Avatar...@");
			UI_show_npc_face1(FACE_BRUNT, 0);
			say("@Dost thou wish ", him_or_her, " dead or mangled, Batlin?@");
			UI_remove_npc_face1();
			UI_set_conversation_slot(0);
			say("@Mangled will suffice, mighty Brunt. But do as thou wishest...@");
			UI_show_npc_face1(FACE_BRUNT, 0);
			say("@Good... Dead it is!@");
			UI_remove_npc_face1();
			UI_set_conversation_slot(0);
			UI_end_conversation();
			UI_set_schedule_type(BRUNT, LOITER);
			var000B = [2340, 382, 0];
			var000A = UI_find_nearby(item, 883, 20, 0);
			if (var000A) {
				UI_si_path_run_usecode(var000A, var000B, 13, var000A, 883, false);
			}
		}

		if (UI_get_npc_id(item) == 9) {
			delayedBark(item, "@Ah, Palos!@", 0);
			action = UI_delayed_execute_usecode_array(item, [no_halt, call_usecode, SHAPE_BATLIN], 20);
		}

		if (UI_get_npc_id(item) == 10) {
			UI_init_conversation();
			// Fixed: use frame 1 here, without the medallion.
			UI_show_npc_face0(FACE_BATLIN, 1);
			say("@Thy fire shall warm the heart of my trap, Palos. I shall rely on thee to make sure the Avatar feels the "
				"warmth of our welcome.@");
			UI_show_npc_face1(FACE_PALOS, 0);
			say("@To be my great pleasure. To look forward to the meeting...@");
			UI_remove_npc_face1();
			UI_set_conversation_slot(0);
			var000A = UI_find_nearby(item, 883, 20, 0);
			if (!var000A) {
				UI_set_schedule_type(var000A, LOITER);
			}
			UI_end_conversation();
		}
		abort;
	}
	// For all other events, let the original handle them.
	shapeBatlin.original();
}
