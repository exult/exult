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
//extern void eladHeal 0x879 (var price_heal, var price_cure, var price_resurrect);

void Elad object#(0x4A2) () {
	if (event == DOUBLECLICK) {
		add(["name", "job", "bye"]);
		if (!gflags[MET_ELAD]) {
			ELAD.say("The man looks at you through smiling eyes.");
			gflags[MET_ELAD] = true;
		} else {
			ELAD.say("Elad bows in your direction.",
			         "~@My pleasure to see thee again.@");
		}
		converse (0) {
			case "name" (remove):
				say("@Elad is my name, ", getPoliteTitle(), ".@");

			case "job":
				say("@I am the healer of the residents of this community.@");
				add(["residents", "heal", "community"]);

			case "community" (remove):
				say("@Moonglow is mine home. I have lived in this town for mine entire life. But I am weary of my life here. 'Tis time, I think, to move on. If only I did not have such strong ties here.@",
				    "~He sighs sadly.");
				if (!gflags[0x01ED]) {
					say("@There is a traveller visiting from Yew. He has seen many exciting things in Britannia. I enjoy listening to his many tales of adventure.@");
					add("traveller");
				}
				add("ties");

			case "traveller" (remove):
				say("@His name is Addom. While he is in town, I am letting him stay in one of my beds.@");

			case "heal" (remove):
				say("@Yes, I sell mine healing services to those who need them.@");
				add("services");

			case "services" (remove):
				var time = UI_part_of_day();
				if ((time == DAWN) || ((time == MORNING) || ((time == NOON) || (time == EVENING)))) {
					//eladHeal(25, 10, 425);
					serviceHeal();
				} else {
					say("@Perhaps thou couldst come for healing when I am working in my shop.@");
				}

			case "ties" (remove):
				say("@My patients here in Moonglow. Who will help them if not I?@");

			case "residents" (remove):
				say("@There are many people in Moonglow. My father once told me that the town was much smaller during his time. In fact, he said that Moonglow used to be separate from the Lycaeum!",
				    "~~@But, I digress. Thou didst ask about the people. I know most of the residents here. Dost thou want to know about the Lycaeum, the observatory, The Fellowship, the farmers, the trainer, or the tavern?@");
				add(["Lycaeum", "observatory", "Fellowship", "farmers", "trainer", "tavern"]);

			case "Lycaeum" (remove):
				say("@The Lycaeum is run by a kind man named Nelson. His advisor is Zelda. Do not break any rules in her presence or thou wilt receive a sharp reprimand!",
				    "~@Jillian also studies there. She can teach thee many things. And do not worry about Mariah. She is harmless if thou wilt but leave her be.@");

			case "observatory" (remove):
				say("@The head there is Brion. He is the twin of the head of the Lycaeum. I like him, although he and his brother are both a little eccentric.@");

			case "Fellowship" (remove):
				say("@I know these people least of all. The branch opened here about five years ago under the direction of a man named Rankin. A few months ago, a clerk joined him. Her name is Balayna.@");

			case "farmers" (remove):
				say("@Cubolt owns the farm. He manages it with his younger brother Tolemac and their friend Morz. I am not positive, but I believe Tolemac recently joined The Fellowship.@");

			case "tavern" (remove):
				say("@Phearcy tends the bar there. He is another good person with whom thou shouldst speak about the townspeople. However, he enjoys gossip, and may be a bit single-minded.@");

			case "trainer" (remove):
				say("@The trainer is named Chad. I believe he specializes in swift, skillful fighting, with knives and swords and such. See him if thou wishest to improve thy skills.@");

			case "bye":
				say("@Leaving so soon, ", getPoliteTitle(), "? Very well, may thy journeys be filled with prosperity.@",
				    "~He sighs. Suddenly, his face brightens.",
					"~@Wait! Perhaps I could join thee?@",
					"~He quickly stands up, smiling. Then, just as suddenly, his smile fades.",
					"~@No. I cannot. I have far too many things to do, too many people to care for. Perhaps in the future?@",
					"~He forces a smile.",
					"~@I hope when we meet next time, ",
					getPoliteTitle(),
					", I will have the opportunity to join thee. Pleasant journey, my friend.@*");
				break;
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(ELAD);
	}
}
