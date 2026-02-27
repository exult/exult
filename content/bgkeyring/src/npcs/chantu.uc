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
//extern void chantuHeal 0x860 (var price_cure, var price_heal, var price_resurrect);

void Chantu object#(0x411) () {
	if (event == DOUBLECLICK) {
		if (!gflags[MET_CHANTU]) {
			item.say("You see a solemn fellow in healer's robes.");
			gflags[MET_CHANTU] = true;
		} else {
			item.say("@Hello, again,@ Chantu says. @How may I help thee?@");
		}

		add(["name", "job", "murder", "services", "bye"]);
		if (gflags[KNOWS_ABOUT_CHRISTOPERS_ARGUMENT]) {
			add(["Fellowship", "Klog"]);
		}

		converse (0) {
			case "name" (remove):
				say("@My name is Chantu,@ he says with a slight bow.");
				say();

			case "job":
				say("@I am the Trinsic healer. I can perform a heal, a poison cure, or a resurrection on any of thy friends. Or on thee, of course.@");

			case "murder" (remove):
				say("@'Tis a sad state for Britannia when events such as these happen. Christopher was a good man. I hope that the villain is caught.@");

			case "services":
				//chantuHeal(30, 50, 400);
				serviceHeal();

			case "Fellowship" (remove):
				say("The healer frowns. @The Fellowship does not appreciate the efforts of healers in Britannia. Although they do admirable things, The Fellowship is short-sighted when evaluating the need for healers. They believe that our work can be done through their so-called 'Triad of Inner Strength'.@");
				if (UI_wearing_fellowship()) {
					say("Chantu notices your medallion and his eyes widen.");
					say("@Excuse me, ", getPoliteTitle(),
					    ", I did not mean to offend thee.@");
				}

			case "Klog" (remove):
				say("The healer shrugs. @He does his duty as he sees fit. And I do mine.@");

			case "bye":
				say("@Goodbye.@*");
				break;
		}
	} else if (event == PROXIMITY) {
		if (CHANTU->get_schedule_type() == PATROL) {
			var barks = [
				"@Feeling better?@",
				"@How are we today?@",
				"@Thy fever has lessened.@",
				"@Try to sleep...@"
			];
			CHANTU->item_say(barks[UI_get_random(UI_get_array_size(barks))]);
		} else {
			scheduleBarks(CHANTU);
		}
	}
}
