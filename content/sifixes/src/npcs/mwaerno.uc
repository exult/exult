/*	Copyright (C) 2016  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*	This uses the correct Gwani cloak check to replace the incorrect ones
 *	used by Mwaerno, and also allows for setting his MET flag. Mwaerno's
 *	original code looks for Shape 277, a crossbeam, frame 8. That shape
 *	does not have a frame 8.
 *
 *	2016-07-17 Written by Knight Captain with code from Malignant Manor.
 */

void Mwaerno object#(0x491) () {
	if (event == STARTED_TALKING) {
		if (gwaniCloakCheck(true)) {
			UI_run_schedule(MWAERNO);
			UI_clear_item_say(MWAERNO);
			UI_show_npc_face0(MWAERNO, 0);
			say("@Rrrow! Meteka Gwani adu laseka!@ *This creature looks at you with angry eyes.");
			// Baiyanda is Mwaerno's wife.
			// If she is nearby, can talk, and not sleeping.
			// The npcCanTalk external does not check the sleep activity, only the sleeping flag.
			// That sleeping flag does not always get set by scheduled sleeping in SI.
			if (npcNearbyAndVisible(BAIYANDA) && npcCanTalk(BAIYANDA) && BAIYANDA->get_schedule_type(!SLEEP)) {
				UI_show_npc_face1(BAIYANDA, 0);
				UI_set_conversation_slot(1);
				say("@Soko terama dok!@ *This one gives you a mean look and spits.");
				UI_remove_npc_face1();
				partyUtters(1, "@Avatar, 'tis the pelts! They can smell that we have Gwani pelts on us! I suggest we throw them away!@", "@I have Gwani pelts!@", false);
				return;
			} else {
				partyUtters(1, "@Avatar, 'tis the pelts! He can smell that we have Gwani pelts on us! I suggest we throw them away!@", "@I have Gwani pelts!@", false);
				return;
			}
		}
		// Conversation included in this code to set the Met flag and avoid the bad checks.
		MWAERNO->run_schedule();
		MWAERNO->clear_item_say();
		MWAERNO->show_npc_face0();

		// First words with face shown.
		say("@Thou look like Avatar,@ *the Gwani creature says with broken speech.");
		// First questions available.
		add(["name", "Gwani", "bye"]);

		converse (0) {
			case "name" (remove):
				say("@Mwaerno. Means 'rain clouds'.@");
				MWAERNO->set_item_flag(MET);

			case "Gwani" (remove):
			say("@Our village near. Speak to Yenani.@");

			case "bye":
				UI_remove_npc_face0();
				UI_remove_npc_face1();
				delayedBark(AVATAR, "@Goodbye!@", 1);
				delayedBark(MWAERNO, "@Atala dak!@", 5);
				break;
		}
	} else {
		Mwaerno.original();
	}
}

