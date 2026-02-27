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

/*	Bwundiai is the first Gwani you meet, and his only schedule is TALK
 *	until you talk to him. Then he gets a normal schedule. He had incorrect
 *	checks for Gwani cloaks and	pelts that were to give you an angry greeting.
 *	They were corrected by Malignant Manor and moved into a new function.
 	In order to avoid the incorrect checks, and resolve his endless TALK
 *	when you first meet him if you have pelts or cloaks, his entire
 *	STARTED_TALKING code has been re-written.
 *
 *	2016-07-05 Written by Malignant Manor
 *	2016-07-17 Knight Captain added full conversation so bad checks are skipped.
 */

void Bwundiai object#(0x490) () {
	if (event == STARTED_TALKING) {
		if (gwaniCloakCheck(true)) {
			BWUNDIAI->set_schedule_type(SHY);
			UI_clear_item_say(BWUNDIAI);
			UI_show_npc_face0(BWUNDIAI, 0);
			say("@Botoka na guta!@ *This creature looks at you with eyes so filled with hate that it is painful to look at them.");
			if (npcNearbyAndVisible(MWAERNO)) {
				UI_show_npc_face1(MWAERNO, 0);
				UI_set_conversation_slot(1);
				say("@Umgabar fotuba na Gwani!@ *This one looks at you with eyes that are cold and feral.");
				UI_remove_npc_face1();
				partyUtters(1, "@Avatar, they smell the Gwani pelts!@", "@They smell the Gwani pelts!@", false);
				return;
			} else {
				partyUtters(1, "@Avatar, he can smell the Gwani pelts!@", "@He can smell the Gwani pelts!@", false);
				return;
			}
		}
		// Since he is the first Gwani you meet, he is set to TALK all day outside their first cave.
		// After you talk to him he gets a schedule, and it looks to be repeated each time you talk.
		// 3 AM Loiter @ 0932, 0872
		// 9 AM MajorSit @ 1073, 0840
		// 6 PM Sleep @ 1063, 0845
		BWUNDIAI->set_new_schedules([EARLY		, MORNING	  , EVENING],
		                            [LOITER	   , MAJOR_SIT	, SLEEP],
		                            [0x03A4,0x0368, 0x0431,0x0348, 0x0427,0x034D]);
		// He does not return to his regular schedule.
		BWUNDIAI->set_schedule_type(SHY);
		BWUNDIAI->clear_item_say();
		BWUNDIAI->show_npc_face0();
		if (!BWUNDIAI->get_item_flag(MET)) {
			say("@Thou... the one... Gwenno spoke.@ ~You can tell the creature obviously has great trouble with your language.");
		} else {
			say("@Be good very much to meet again.@");
		}
		add(["name", "Gwani", "bye"]);

		converse (0) {
			case "name" (remove):
				// His name was misspelled in his original code!
				say("@Bwundiai my name is.@");
				BWUNDIAI->set_item_flag(MET);

			case "Gwani" (remove):
				say("@Our village near. But, thou seek Gwenno...@ ~He pauses thoughtfully before speaking to you again. ~@Gwani Death Temple on island north. West of Ice Dragon caves.@");
				if (npcNearbyAndVisible(IOLO) && npcCanTalk(IOLO)) {
					UI_show_npc_face1(IOLO, 0);
					UI_set_conversation_slot(1);
					say("Iolo glances over to you with a glint of sadness in his eyes.");
					UI_remove_npc_face1();
				}

			case "bye":
				UI_remove_npc_face0();
				UI_remove_npc_face1();
				delayedBark(AVATAR, "@Farewell, little fellow.@", 1);
				delayedBark(BWUNDIAI, "@Fare -- well@", 5);
				break;
		}
	} else {
		Bwundiai.original();
	}
}
