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
//extern void leighHeal 0x8AC (var price_heal, var price_cure, var price_resurrect);

void Leigh object#(0x4C9) () {
	if (event == DOUBLECLICK) {
		var avatartitle = getPoliteTitle();
		if (!gflags[MET_LEIGH]) {
			item.say("This attractive woman gives you an approving look.");
			gflags[MET_LEIGH] = true;
		} else {
			item.say("@Greetings, ", avatartitle, ".@ Leigh smiles at you.");
		}

		add(["name", "job", "bye"]);
		if (gflags[CAN_EXAMINE_STONE_CHIPS] && gflags[KNOWS_LEIGH_IS_HEALER]) {
			add("examine chips");
		}

		converse (0) {
			case "name" (remove):
				say("She blushes. @I am Lady Leigh.@");

			case "job":
				say("@I am the Healer of Serpent's Hold.@");
				add(["Serpent's Hold", "heal"]);
				gflags[KNOWS_LEIGH_IS_HEALER] = true;
				if (gflags[CAN_EXAMINE_STONE_CHIPS]) {
					add("examine chips");
				}

			case "heal" (remove):
				if (get_schedule_type() == TEND_SHOP) {
					//leighHeal(25, 8, 385);
					serviceHeal();
				} else {
					say("@I am sorry, but I have too many other patients to help thee now. Perhaps when I next open my shop.@");
				}

			case "Serpent's Hold" (remove):
				say("@Lord Jean-Paul is in charge of keeping the order here, but Sir Denton would be an even better source of information about Serpent's Hold.@");
				add(["John-Paul", "Denton"]);

			case "John-Paul" (remove):
				say("@He is an easy man to find, for Sir Horffe hardly ever leaves his side. Watch for the tall, muscular gargoyle.@");
				if (!gflags[DOING_STATUE_QUEST]) {
					say("@In fact,@ she says, looking off in the distance, @I believe he may have a desire to speak with thee. Perhaps thou shouldst truly seek him out.@");
				}
				say("@If thou hast business about the town, and are not able to locate John-Paul, thou mightest wish to speak with Sir Richter.@");
				add(["Horffe", "Richter"]);

			case "Horffe" (remove):
				say("@He was found at a very young age, apparently abandoned by his father. Two people took and raised him as their own. As thou couldst see simply by meeting him, he is a very noble person and a stout warrior.@");

			case "Richter" (remove):
				say("@He is the armourer. His shop is in the back of the Hold.@");

			case "Denton" (remove):
				say("@He is the tavern keeper at the Hallowed Dock, just inside the Hold's doors. He is wonderful at remembering and discussing important facts.@");

			case "examine chips" (remove):
				if (contHasItemCount(PARTY, 1, SHAPE_STONE_CHIPS, QUALITY_ANY, 4)) {
					//Usecode bug: flag 0x268 is used exactly this once in the entire
					//BG usecode. I assume that they switched the flag number during
					//production, which made this into a bug.
					//if (gflags[0x268])
					//Instead, I check to see if the chips have been examined:
					if (gflags[EXAMINED_CHIPS]) {
						say("She looks at you, puzzled. @Did I not do that already?@");
					} else {
						say("She takes the stone chips from you and examines them. Using several vials of strange and unusual mixtures, she analyzes the blood. Finally, after a few silent minutes, she looks up, grinning.",
							"~~@I have determined the nature of the blood. It is definitely not human. In fact,@ she looks back down at the sample and raises one eyebrow, @it is gargoyle blood.@");
						add("gargoyle blood");
						gflags[EXAMINED_CHIPS] = true;
					}
				} else {
					say("@I am afraid that I must be able to see them to examine them.@");
				}

			case "gargoyle blood" (remove):
				say("She appears thoughtful.",
					"~~@What is odd, ", avatartitle,
					", is that there is only one gargoyle in Serpent's Hold. But I cannot imagine Sir Horffe would have had anything to do with this wanton destruction.@");

			case "bye":
				say("@Farewell, ", avatartitle, ".@*");
				break;
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(LEIGH);
	}
}
