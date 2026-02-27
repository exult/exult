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
 *	Specifically, Reyna will now talk about Laurianna once she has
 *	settled in Yew.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Externs:
//extern void reynaHeal 0x8D2 (var price_heal, var price_cure, var price_resurrect);

void Reyna object#(0x46C) () {
	var avatar_title;
	var part_of_day;
	var dont_add_aimi;
	var dont_add_heal;
	var has_flowers;
	var tomb;
	var used_half_price;

	if (!(event == DOUBLECLICK)) {
		return;
	}

	//Get title for avatar:
	avatar_title = getPoliteTitle();
	//Get the part of day (used to see if Reyna will heal
	//while not at the shop):
	part_of_day = UI_part_of_day();

	//To prevent adding options again and again:
	dont_add_aimi = false;
	dont_add_heal = false;
	//If party has brought flowers:
	has_flowers = contHasItemCount(PARTY, 1, SHAPE_PLANT, QUALITY_ANY, 4);

	add(["name", "job", "bye"]);

	if (!gflags[MET_REYNA]) {
		item.say("The woman greets you with shining eyes.");
		gflags[MET_REYNA] = true;
	} else {
		item.say("@Hello, ", avatar_title, ",@ says Reyna.");
	}

	tomb = AVATAR->find_nearest(SHAPE_TOMBSTONE, -1);

	if (tomb) {
		//Reyna is at the cemetery:
		if (!gflags[ASKED_REYNA_ABOUT_MOTHER]) {
			add("cemetery");
		}
	}
	if (has_flowers) {
		//Avatar has flowers:
		if (gflags[ASKED_REYNA_ABOUT_MOTHER]) {
			add("brought flowers");
		}
	}
	if (gflags[KNOWS_REYNA_JOB]) {
		//Avatar knows that Reyna is a healer, so add the option:
		add("heal");
		dont_add_heal = true;
	}

	converse(0) {
		case "name" (remove):
			say("@I am Reyna,@ she says, brushing the hair out of her face.");

		case "job" (remove):
			say("@I am a healer. I have chosen to set up shop here near the forest.@");
			gflags[KNOWS_REYNA_JOB] = true;

			if (!dont_add_heal) {
				add("heal");
			}

			add("forest");
			if (gflags[KNOWS_REYNA_LOVES_ANIMALS]) {
				add("animals");
			}

		case "forest" (remove):
			say("@I wanted to live and work here because the land is very beautiful. I have found many things to do and see. Unfortunately, the forest is so spread out that I have yet to meet many of the others who live in this area. I do know that the Abbey is just across the way from mine house.",
				"~~@And somewhere nearby is a scholar.@ She appears thoughtful for a moment. @Also, I believe there is a prison just east of the Abbey.");
			add(["Abbey", "scholar", "prison"]);

		case "prison" (remove):
			say("@I've never actually seen it,@ she laughs, @but rumor has it that the cells are located right next to the court, for quick, easy imprisonment after the trial.@");

		case "scholar" (remove):
			if (gflags[LAURIANNA_IN_YEW]) {
				say("@From Aimi and Laurianna I have heard that he is brilliant, and... also a bit overzealous to instruct those interested in increasing their knowledge. I believe his name is Perrin.@");
				add("Laurianna");
			} else {
				say("@From Aimi I have heard that he is brilliant, and... also a bit overzealous to instruct those interested in increasing their knowledge.@");
			}

			if (!dont_add_aimi) {
				add("Aimi");
			}

		case "Laurianna" (remove):
			say("@She moved recently to Yew. She has lost her father recently, and is trying to find her place in the world.");
			say("@Laurianna is helping me run my shop. She is also spending a lot of her time studying with Perrin the scholar.@");

		case "Aimi" (remove):
			dont_add_aimi = true;
			if (gflags[0x015A]) {
				say("@She is the monk who tends the garden at the Abbey.@");
			} else {
				say("@She is one of the monks who lives at the Abbey. At this time, she is the only other person I have actually met here in the forest.@");
			}

		case "Abbey" (remove):
			say("@That is how this area -- Empath Abbey -- got its name, from the monks who live at the abbey of the Brotherhood of the Rose. They are said to make delicious wine. One of the monks cares for a beautiful garden in her spare time. In fact, I often buy flowers from her.",
				"~~ But,@ she grins, @as for the other monks, all that I ever see them do is make wine and wander the countryside.@");
			gflags[0x015A] = true;
			add(["flowers", "others"]);

		case "others" (remove):
			say("@Aimi is the only one I have met, but I know there are one or two others who make wine there.@");

			if (!dont_add_aimi) {
				add("Aimi");
			}

		case "flowers" (remove):
			say("@Yes, I get them for my mother.@");

			if (!gflags[ASKED_REYNA_ABOUT_MOTHER]) {
				add("mother");
			}

		case "mother", "cemetery" (remove):
			var msg = "";
			gflags[ASKED_REYNA_ABOUT_MOTHER] = true;
			if (tomb) {
				//Check to see if there are flowers at Reyna's mother's grave:
				var plants = find_nearby(SHAPE_PLANT, 10, MASK_NONE);
				for (plant in plants) {
					if (plant->get_item_frame() == 4) {
						//There are:
						msg = "I realize there are already very beautiful flowers here, but there can never be enough to demonstrate how much she is missed. ";
					}
				}
			}
			say("She looks down at her feet, and then back up at you. It is obvious she is fighting an urge to cry.");
			say("@Several months ago, my mother passed away in her home town. She was born here in the forests, and had asked to be buried here, near me. Every morning I come out here to visit her and set flowers by her grave.");
			say("@But,@ a lone tear escapes and trickles down her cheek, @I am the only member of our family who lives nearby. No one else is able to visit or leave flowers very often.");
			say("@Her grave looks so bare sometimes.@ She looks off into the horizon and sighs. @",
				msg,
				"It would be nice if there were some way to have more flowers brought to her.@");
			say("She quickly turns and looks at you.");
			say("@I am terribly sorry for rambling on like that. Please excuse me, ",
				getPoliteTitle(), ".@");

			if (has_flowers) {
				add("have flowers");
			}

		case "brought flowers", "have flowers" (remove):
			say("Her eyes light up as she sees the bouquet of flowers.");
			say("@They are lovely! Thou art too kind, ", getPoliteTitle(),
				", to bring flowers for my mother! I cannot wait to set them by her grave.@");
			UI_remove_party_items(1, SHAPE_PLANT, QUALITY_ANY, 4, true);

			//Give a random amount of experience for the nice Avatar:
			var rand = UI_die_roll(1, 6);
			var exp;
			if ((rand == 1) || (rand == 2)) {
				exp = 9;
			} else if ((rand == 3) || ((rand == 4) || (rand == 5))) {
				exp = 19;
			} else if (rand == 6) {
				exp = 90;
			}
			giveExperience(exp);

			//Mark the (one-shot) 50% discount:
			gflags[GAVE_REYNA_FLOWERS] = true;

		case "heal":
			if ((part_of_day == MORNING) || ((part_of_day == NOON) || (part_of_day == AFTERNOON))) {
				//If Reyna is working, so she will heal:
				gflags[REYNA_EMERGENCY] = true;
			}
			if (gflags[REYNA_EMERGENCY]) {
				//She will also heal in emergencies (see below)
				if (gflags[GAVE_REYNA_FLOWERS]) {
					//50% discount if the Avatar gave her flowers:
					used_half_price = true;
					say("@For thy kindly gift of flowers, I will aid thee for half price.@ She smiles at you.");
					//reynaHeal(15, 5, 200);
				} /*else {
					reynaHeal(30, 10, 400);
				}
				*/
				serviceHeal();
			} else {
				//Reyna won't heal if she is not at the shop unless it is
				//an emergency:
				say("@I am sorry, ", avatar_title,
					", but, unless this is an emergency, I would prefer to wait until my shop is open.@");
				add("emergency");
			}

		case "emergency" (remove):
			//Avatar claims it is an emergency
			//Get party list:
			var party = UI_get_party_list();
			var is_emergency = false;
			for (npc in party) {
				//For every party member,
				if (npc->get_item_flag(POISONED) || (npc->get_npc_prop(HEALTH) < 10)) {
					//If he is badly hurt or poisoned, it *is* an
					//emergency, so break:
					is_emergency = true;
					break;
				}
			}
			//Feedback:
			var msg;
			if (UI_get_array_size(party) > 1) {
				msg = " and your companions";
			} else {
				msg = "";
			}
			say("She quickly examines you", msg, ".");

			if (is_emergency == true) {
				//It is an emercency, so set flag:
				gflags[REYNA_EMERGENCY] = true;
				say("@Thou art correct, ", avatar_title,
					". This is a true emergency!@");
				add("heal");
			} else {
				//Not an emergency; send Avatar away:
				say("@I am sorry, but thy wounds are not mortal. Perhaps thou canst visit me when my shop is open.@");
			}

		case "animals" (remove):
			say("She smiles shyly.",
				"~~@I very much love animals. When I was very young, I found an ailing dove that I was unable to nurse back to health. Since that time, I began to study the healing arts, so that I would be able to help other animals who might need healing.",
				"~~@Of course,@ she laughs, @now that I have the skills, I use them to help people, too.@");

		case "bye":
			say("@Goodbye, ", avatar_title, ".");
			if (gflags[GAVE_REYNA_FLOWERS]) {
				say("@I thank thee for the bouquet!");
				if (used_half_price) {
					gflags[GAVE_REYNA_FLOWERS] = false;
				}
			}

			say("@May health always follow thee!@*");
			gflags[REYNA_EMERGENCY] = false;
			break;
	}
}
