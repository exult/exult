/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-03-19
 */

/*	Lady Tory, the empath of Serpent's Hold. Reimplemented to improve
 *	her dialogue, which was pretty awful.
 */

const int RIKY_QUEST_EXPERIENCE = 100;	//How much the party gets for returning Riky
void Lady_Tory object#(0x4C8) () {
	var player_name;
	var polite_title;
	var asked_about_john_paul;

	if(event == DOUBLECLICK) {
		player_name = getAvatarName();
		polite_title = getPoliteTitle();
		asked_about_john_paul = false;

		add(["name", "job", "bye"]);
		if (gflags[STARTED_HOLD_INVESTIGATION] && !gflags[FINISHED_HOLD_INVESTIGATION]) {
			add("statue");
		}
		if (gflags[HEARD_ABOUT_RIKY] && !gflags[RESCUED_RIKY]) {
			add("Riky");
		}

		if(!gflags[MET_TORY]) {
			item.say("The woman smiles at you compassionately.");
			gflags[MET_TORY] = true;
		} else {
			item.say("Tory smiles and reaches out to you. @Hello, ", player_name,
				". I sense thou art troubled.@");
		}

		converse(0) {
			case "name" (remove):
				say("@I am Lady Tory, ", polite_title, ".@");
				if (!gflags[HEARD_ABOUT_RIKY]) {
					say("@Mother of Riky,@ she says, sobbing.");
					add("Riky");
				}

			case "job":
				say("@My job is to provide counsel for Lord John-Paul and anyone else in need of guidance here at the Hold.@");
				add(["Lord John-Paul", "Hold"]);

			case "Riky" (remove):
				if (gflags[HEARD_ABOUT_RIKY]) {
					say("@Hast thou found my child?@");
					if (askYesNo()) {
						//Party is carrying Riky
						if (PARTY->count_objects(SHAPE_BABY, QUALITY_ANY, FRAME_RIKY)) {
							giveExperience(RIKY_QUEST_EXPERIENCE);
							say("@I cannot begin to express my gratitude, ",
								polite_title, ". Thank thee ever so much!@",
								"~She begins sobbing for joy. @Pl-please set him back gently in the cradle.@");
							gflags[RESCUED_RIKY] = true;
						} else if (PARTY->count_objects(SHAPE_BABY, QUALITY_ANY, FRAME_ANY)) {
							//Carrying some baby, but it ain't him
							say("@Why, that's not my little Riky, ", polite_title,
								". Thou hast someone else's child. Oh, where could my boy have been taken?@ she says, crying.");
						} else {
							say("@But, I see no child with thee. Thine humor is quite dark. Please return when thou art carrying my baby boy!@*");
							return;
						}
					} else {
						say("@Please, I beseech thee, continue thine hunt!@");
					}
				} else {
					gflags[HEARD_ABOUT_RIKY] = true;

					say("@My poor baby boy. He -- he was taken one night by cruel harpies who wanted a child for their own. I -- I know not where they have taken him, but I have heard some of the knights mention that a group of the vile women-birds cluster around the shrine of Honor. But, they have not yet been able to defeat them.@ She sniffs.");
					say("@But thou ", polite_title,
						", thou wilt help me get my child back. Oh, please, wilt thou?@");
					if (askYesNo()) {
						say("@I cannot thank thee enough for helping me!@ She appears to have cheered up greatly.");
					} else {
						say("@Thou art a no more than a coward. Get thee gone, coward!@");
						gflags[RESCUED_RIKY] = true;
					}
				}

			case "statue" (remove):
				say("@Hmm,@ she appears thoughtful, @when the incident was brought up to everyone here at the hold, I remember Sir Jordan becoming a bit nervous. Perhaps thou shouldst speak with him.@");

			case "Hold" (remove):
				say("@I sense that thou wishest to know about the residents here at Serpent's Hold. Is this correct?@");
				//Wow, that's deep, lady. I ask you about Serpent's Hold,
				//and you intuit by my VERY AURA that I want to know about
				//Serpent's Hold. What a marvel you are.

				if (askYesNo()) {
					say("@As counselor for the Hold, I can tell thee about many people. Hast thou met the healer or the provisioner? And, as a warrior thyself, thou mayest wish to visit the trainer and the armourer.@");

					if (!asked_about_john_paul) {
						add("Lord John-Paul");
					}
					add(["healer", "armourer", "trainer", "provisioner"]);
				} else {
					say("@Very well. Come to me if thou changest thy mind.@");
				}

			case "Lord John-Paul" (remove):
				say("@He is an extraordinary leader. Everyone looks up to him. Thou hast only to ask his captain.@");
				add("captain");
				asked_about_john_paul = true;

			case "healer" (remove):
				say("@Lady Leigh is very skilled as a healer. I have yet to see her lose a patient.@");

			case "armourer" (remove):
				say("@Hmmm. Well, Sir Richter has changed much recently -- ever since he joined The Fellowship. He seems a little less compassionate.@");
				add("Fellowship");

			case "tavernkeeper" (remove):
				say("@Sir Denton is the most astute man I have ever met. He is the only one I cannot sense. And I have never seen him remove his armour....@ She shrugs.");

			case "trainer" (remove):
				say("@I know Menion least of all. He is very quiet, spending most of his spare time weaponsmithing. The tavernkeeper may know more about him.@");
				add("tavernkeeper");

			case "provisioner" (remove):
				say("@Her name is Lady Jehanne. She is the Lady of Sir Pendaran,@ she says with a gleam in her eye.");
				add("Sir Pendaran");

			case "captain" (remove):
				say("@The Captain of the guard, Sir Horffe, is a gargoyle. He was found by two humans who raised him to be a valiant knight. He is a very dedicated warrior, and rarely leaves Lord John-Paul's side.@");
				if (gflags[MET_HORFFE]) {
					add("Gargish accent");
				}

			case "Gargish accent" (remove):
				say("@Despite his human upbringing, Horffe has struggled to maintain his Gargish identity. By speaking in the same manner as his brethren, he feels he can better hold on to his background.@");

			case "Sir Pendaran" (remove):
				say("@He is a brave and hearty fighter, and,@ she smiles, @he is also a bit on the attractive side.@");

			case "Fellowship" (remove):
				say("@The Fellowship does not have a branch here, but two of our knights are members: Sir Richter and Sir Pendaran. I know they are interested in having Sir Jordan join as well.@");
				add("Sir Jordan");

			case "Sir Jordan" (remove):
				say("@He is a wonder. Despite his blindness, he fights with amazing deftness. In fact, he also enjoys toying with mechanical items, and his loss of eyesight does not seem to affect that, either.",
					"~~@However, I sense in him a very recent change, remarkably like that in Sir Richter. He would be an interesting one to speak with. Thou mayest find him at Iolo's South.@*");
				if (isNearby(IOLO)) {
					IOLO.say("Iolo smiles proudly.",
						"~~@My shop has, er, grown a bit since thou wert here last, ",
						player_name, ".@");
					IOLO.hide();
				}

			case "bye":
				//Oh really? No shit? Maybe that's because I just said GOODBYE?
				say("@I sense thou hast pressing engagements elsewhere. I bid thee farewell.@*");
				return;
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(item);
	}
}
