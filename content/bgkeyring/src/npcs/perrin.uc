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
 *	Specifically, Perryn will now talk about Laurianna once she has
 *	settled in Yew.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Externs:
extern void perrinTrain 0x8CA (var props, var cost);

void Perrin object#(0x4EE) () {
	var avatar_name;
	var avatar_title;

	if (event == DOUBLECLICK) {
		//Avatar's name and title:
		avatar_name = getAvatarName();
		avatar_title = getPoliteTitle();

		add(["name", "job", "bye"]);

		if (!gflags[MET_PERRIN]) {
			item.say("The man before you stretches and inhales deeply.");
			gflags[MET_PERRIN] = true;
		} else {
			item.say("@Glorious day, ", avatar_name, ".@ Perrin grins.");
		}

		converse (0) {
			case "name" (remove):
				say("@Please, ", avatar_title,
					", call me Perrin. I reside here in Empath Abbey.@");
				add("Empath Abbey");

			case "job" (remove):
				say("@I am a scholar, ", avatar_title,
					". Dost thou want training in the realm of books?@");

				if (askYesNo()) {
					//Avatar wants training:
					say("@My price is 45 gold for each training session, but I will also teach thee what little I know about magic. Is this acceptable?@");

					if(askYesNo()) {
						//If the price is OK, then train:
						perrinTrain([INTELLIGENCE, MAX_MANA], 45);
					} else {
						say("@Very well, ", avatar_title, ".@");
					}
				} else {
					say("@Forgive me, I am a bit overzealous in my search for students. I hope thou wilt return in the future.@");
				}

			case "Empath Abbey" (remove):
				say("@This is a pleasant location. I like the privacy, which gives me a chance to study when I need to. The Brotherhood is across the road, and I am near a healer. Also, I have begun a study on the effects of dealing with death for undertakers. I am using Tiery as a case study.@");
				add(["Brotherhood", "healer", "Tiery"]);
				if (gflags[LAURIANNA_IN_YEW]) {
					//Lauriana is now in Yew, so mention her too:
					say("@Recently, a new woman named Laurianna also came to live nearby.@");
					add("Laurianna");
				}

			case "Brotherhood" (remove):
				say("@That is the abbey. The monks who reside there are famous for their ability to produce exquisite wine. Nearby is the Highcourt and a prison.@");
				add(["wine", "highcourt", "prison"]);

			case "wine" (remove):
				say("@Thou shouldst try some. The monks have been making it for more than three hundred years!@");

			case "highcourt" (remove):
				say("@The official there is named Sir Jeff. From what I hear, he runs his ship very tight. I do not envy the jailer that works with him. It must be extremely difficult to be near such a strict disciplinarian all day long.@");

			case "prison" (remove):
				say("@It is located just behind the court. And,@ he grins, @I am proud to say that is at least one thing about which I know nothing.@");

			case "Tiery" (remove):
				say("@He is the undertaker who lives just north of the Brotherhood.@");

			case "Laurianna" (remove):
				say("@She is very friendly, that one. She is living in the healer's house, as well as taking daily classes with me.");
				say("@I must confess that she is not the only one learning, however -- I've learned a great deal about magic from her.@");

			case "healer" (remove):
				say("@I have yet to meet her, but I know she loves animals. I have seen her playing with the deer and squirrels that inhabit this region.@");
				gflags[KNOWS_REYNA_LOVES_ANIMALS] = true;

			case "bye" (remove):
				say("@Goodbye, ", avatar_name,
					". Best of luck in thy journeys.@*");
				break;
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(item);
	}
}
