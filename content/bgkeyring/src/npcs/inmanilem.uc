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
 *	This source file contains usecode to make the NPC use the
 *	Generic Healing Service.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-06-18
 */

//Externs:
//extern void inmanilemHeal 0x89D (var price_heal, var price_cure, var price_resurrect);
extern void scheduleGargishBarks 0x92F (var npc);

void Inmanilem object#(0x4B6) () {
	if (event == DOUBLECLICK) {
		add(["name", "job", "bye"]);
		if (!gflags[MET_INMANILEM]) {
			item.say("You are greeted by a friendly gargoyle.");
			gflags[MET_INMANILEM] = true;
		} else {
			item.say("@To see you are doing well, human,@ says Inmanilem.");
		}

		converse (0) {
			case "name" (remove):
				say("@To be called Inmanilem, human. To wish information about Terfin?@");
				add(["information", "Inmanilem"]);

			case "Inmanilem" (remove):
				say("@To be Gargoyle for `make healed one.'@");

			case "job":
				say("@To be the healer.@");
				add("heal");
				if (gflags[KNOWNS_ABOUT_CONFLICTS]) {
					add("conflicts");
				}

			case "heal" (remove):
				var time = UI_part_of_day();
				if ((time == DAWN) || ((time == MORNING) || ((time == NOON) || (time == AFTERNOON)))) {
					//inmanilemHeal(25, 10, 430);
					serviceHeal();
				} else {
					say("@To feel sorry, but to be busy with other things now. To ask you to come back when I have the time to heal you.@");
				}

			case "information" (remove):
				say("@To tell you to seek out Draxinusom, human, or Forbrak. To have much information about Terfin.@");
				add(["Draxinusom", "Forbrak", "Terfin"]);

			case "Forbrak" (remove):
				say("@To be the tavernkeeper. To be very strong of body, and of mind.@");

			case "Terfin" (remove):
				say("@To be the city of gargoyles. To be the one of two towns where many gargoyles live. To like it here,@ he adds, smiling.");
				add("one?");

			case "one?" (remove):
				say("@To tell you the other is called Vesper. To be in the desert in northeastern Britannia. To have also humans living there, unlike here.@");

			case "Draxinusom" (remove):
				say("@To be our leader. To live near the Hall of Knowledge.@");
				add("Hall");

			case "Hall" (remove):
				say("@To be where the three altars of singularity are kept.@");
				add("altars");

			case "altars" (remove):
				say("@To be Passion, Control, and Diligence. To be the values that most gargoyles hold as the key of our existence.@");
				add(["most gargoyles", "key"]);

			case "key" (remove):
				say("He nods his head emphatically. @To be quite similar to the human concept of virtues.@");

			case "most gargoyles" (remove):
				say("@There is a rival now -- The Fellowship. To know not if it is good or bad, but to know I do not follow it!@");

			case "conflicts" (remove):
				say("@To know only of one dissatisfied gargoyle. To have always been problem, but now acting hostile and aggressive. To be named Silamo, the gardener.",
					"~~@To recommend you talk to Silamo.@");
				gflags[KNOWS_SILAMO_UNHAPPY] = true;

			case "bye":
				say("@To wish you good health, human.@*");
				break;
		}
	} else if (event == PROXIMITY) {
		scheduleGargishBarks(INMANILEM);
	}
}
