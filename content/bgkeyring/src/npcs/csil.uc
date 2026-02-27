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
//extern void csilHeal 0x870 (var price_heal, var price_cure, var price_resurrect);

void Csil object#(0x423) () {
	if (event == DOUBLECLICK) {
		add(["name", "job", "services", "bye"]);
		if (!gflags[MET_CSIL]) {
			item.say("You see a healer who looks wise and honest.");
			say("@I have been looking forward to thine arrival, Avatar. Word spreads quickly. I am pleased to meet thee!@");
			gflags[MET_CSIL] = true;
		} else {
			item.say("@Hello again, Avatar!@ Csil says with a smile.");
		}

		converse (0) {
			case "name" (remove):
				say("@I am Csil the Healer, although in a past life I was known as Abrams. I became Csil when I was advanced.@");
				add("advanced");

			case "job":
				say("@I am Britain's healer, and have been for many years. If thou wishest to employ my services, please say so. I shall be only too happy to help.@");

			case "advanced" (remove):
				say("@When my name was Abrams, I lived on the island of New Magincia and did mine apprentice work there. My practice grew, and soon I was travelling by ship to Moonglow to see patients there.@");
				add(["patients", "practice"]);

			case "patients" (remove):
				say("@Soon I had patients on three islands. It was then that Lord British received word of my practice.@");
				add("Lord British");

			case "practice" (remove):
				say("@My practice grew swiftly. I am a modest man, but I do not mind saying that I was a popular healer.@");
				if (isNearby(SHAMINO)) {
					SHAMINO.say("@He is probably the best healer in all Britannia. Why, he cured a, er, particular problem I had in no time at all.@*");
					if (isNearby(IOLO)) {
						SHAMINO.hide();
						IOLO.say("@Oh? What problem was that?@*");
						IOLO.hide();
						SHAMINO.say("@Never mind. The whole world does not need to know about it.@*");
					}
					SHAMINO.hide();
					UI_show_npc_face(CSIL, 0);
				}

			case "Lord British" (remove):
				say("@Well, Lord British himself was struck down with some sort of malady. He sent for me. I arrived at the castle as soon as I could leave my patients, and I examined the king. It appeared to me that something had infested his blood. I have a theory about it, which I am convinced is correct. Others, however, do not share my view.@");
				add(["theory", "others"]);

			case "theory" (remove):
				say("@I believe that most sicknesses are caused by tiny living things. We cannot see these things with the naked eye. However, I am working on developing an instrument which -can- see these creatures. I believe that someday, healing will not depend on magic at all, but on some form of treatment which makes one less vulnerable to these living creatures. Since these animals are biological, I call this theorized treatment 'antibiotics'. What dost thou think, Avatar? Am I on the right track?@");
				if (askYesNo()) {
					say("@Good. I thought so.@");
				} else {
					say("@No? Hmmm.@ Csil looks concerned. @Well, I cannot believe in the archaic tradition of bleeding a person until the sickness has left his body. There must be another way...@",
						"~~Csil looks at his notes, worried that his theory is invalid.");
				}

			case "others" (remove):
				say("@There is a group of people who do not encourage my studies. We do not get along at all. I think they have something against healers which goes beyond simple distrust. Dost thou know whom I mean?@");
				if (askYesNo()) {
					say("Csil nods. @I thought so. The Fellowship is not... quite what they seem.@");
				} else {
					say("@No?@ Csil lowers his voice. @The Fellowship.@");
				}
				add("Fellowship");

			case "Fellowship" (remove):
				say("@They have a doctrine which outlines their beliefs. They believe if one is faced with pain, then he has no choice but to go through it in order to be a 'better person'. I do not agree with this. No one should ever go through needless pain. But... they are entitled to their own opinions.@");

			case "services":
				//csilHeal(40, 30, 450);
				serviceHeal();

			case "bye":
				break;
		}
		say("@Goodbye, Avatar.@*");
	} else if (event == PROXIMITY) {
		scheduleBarks(CSIL);
	}
}
