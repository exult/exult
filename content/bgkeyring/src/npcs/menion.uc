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

//Menion, the weaponsmith of Serpent's Hold - reimplemented to allow swordmaking

//Menion's training script (He trains strength and combat)
extern void trainWithMenion 0x8BE (var abilities, var price);
void Menion object#(0x4C0) () {
	var schedule;
	var part_of_day;

	if (event == DOUBLECLICK) {
		var polite_title = getPoliteTitle();
		var schedule = UI_get_schedule_type(item);

		add(["name", "job", "bye"]);

		//carrying a swordblank
		if (UI_count_objects(PARTY, SHAPE_SWORDBLANK, QUALITY_ANY, FRAME_ANY)) {
			add("forged sword");
		}
		if (gflags[MENION_HAS_SWORD]) {
			add("Could I have my sword now?");
		}

		if(!gflags[MET_MENION]) {
			item.say("The large, muscle-bound man smiles pleasantly at you.");
			gflags[MET_MENION] = true;
		} else {
			item.say("@Greetings, ", polite_title, ",@ says Menion.");
		}

		converse(0) {
			case "name" (remove):
				say("@I am Menion, ", polite_title, ".@ He shakes your hand.");

			case "job":
				say("@I am a trainer. I help warriors become bigger and stronger and better fighters. I also forge swords to match the strength in my students' arms.@");
				add(["train", "forge", "students"]);

			case "students" (remove):
				say("@I have taught many a warrior how to use his -- or her -- force against an opponent.@");
				add("force");

			case "force" (remove):
				say("@Yes, ", polite_title,
					". The key to effective fighting is striking hard and accurately at one's foe.@");
				add(["hard", "accurately"]);

			case "hard" (remove):
				say("@Physical strength permits the attacker a better chance of penetrating the other fighter's armour. Obviously, in a lethal combat, that is an important objective.@");

			case "accurately" (remove):
				say("@Needless to say, some targets on an individual are better than others. 'Tis always best to hit something that will either seriously incapacitate one's foe, or create enough pain to distract him.@");

			case "train" (remove):
				if (schedule == TEND_SHOP) {
					say("@I will train thee for ", MENION_TRAINING_PRICE,
						" gold. Wilt thou pay?@");
					if (askYesNo()) {
						trainWithMenion([STRENGTH, COMBAT], MENION_TRAINING_PRICE);
					} else {
						say("@Fine.@");
					}
				} else {
					say("@Perhaps this would be a more appropriate topic when I am at work.@");
				}

			case "forge" (remove):
				say("@Dost thou wish to make a sword?@");
				if (schedule in [BLACKSMITH, TEND_SHOP]) {
					if (askYesNo()) {
						say("He smiles. @I would be very happy to show thee the steps necessary to create a very fine blade.");
						say("@Firstly, thou shalt need a sword blank.@ He looks through a stack and picks one for you.");
						//Successfully gave the swordblank to the party
						if (UI_add_party_items(1, SHAPE_SWORDBLANK, 0, 0, true)) {
							say("@Now, when such a blank comes from the mould, it is blunt and dull. Thou must temper it: firstly by heating it upon the firepit until it is white-hot, and then beating an edge into it with thy hammer upon an anvil.");
							say("@Thou mayest need to heat it again several times, as thou canst only put a good edge upon the blade when it is malleable.");
							say("@Finally, once thy blade has the desired edge, you must quickly plunge it into cold water. This shall harden thy blade and prevent it from warping.");
							say("@Once this is done, it is ready to be given a proper pommel - bring the blade to me and I shall do this for thee.");
							say("@Good luck, and may thy blade be sturdy and sharp!@*");
						} else {
							//Carrying too much
							say("@Perhaps when thou hast fewer things to occupy thy pack, I can give this to thee and we might continue.@");
						}
					} else {
						say("@Perhaps sometime when thou hast more time.@");
					}
				} else {
					say("@I can help thee with that when I am at work.@");
				}

			case "bye":
				say("@May the strength in thine arms always match the strength of thy will.@*");
				break;

			//Everything that follows is added content
			case "forged sword" (remove):
				if (schedule in [BLACKSMITH, TEND_SHOP]) {
					say("@Thou hast finished thy swordblank? Please, let me see it.@");

					//now, check exactly what kind of swordblank we have

					//swordblank ready - take it off their hands
					if (UI_remove_party_items(1, SHAPE_SWORDBLANK, SWORDBLANK_READY, 7)) {
						say("He takes the blank from you, balancing it on his fingertips and then peering down the length of the blade, angling it in the light.");
						say("@Thou hast forged a fine blade indeed in this one! She is weighted well, and should keep her edge faithfully. All that she needs now is the pommel.@");
						say("@Menion slips an ornate pommel onto the tang of the blade, pinning it fast and binding it with straps of leather.",
							"~After a little work he presents to you the completed sword, and bows with a sheepish sense of ceremony.");

						//reward the player for their persistence
						if (!gflags[GOT_SWORDSMITHING_XP]) {
							gflags[GOT_SWORDSMITHING_XP] = true;
							giveExperience(SWORDSMITHING_XP);
						}

						if (UI_add_party_items(1, SHAPE_CUSTOM_SWORD, QUALITY_ANY, FRAME_ANY)) {
							say("@May thy blade stay true, ", polite_title, "!@");
						} else {
							//Bugger, they couldn't carry it. Now we need to do more work.
							gflags[MENION_HAS_SWORD] = true;
							say("@But thou art too burdened to take thy prize! Return when thou art carrying less and I shall give thee thy sword.@");
						}
					} else if (UI_count_objects(AVATAR, SHAPE_SWORDBLANK, QUALITY_ANY, 6)) {
						//swordblank still needs more work
						say("He examines the blank critically, turning it this way and that. @Thou hast the makings of a fine blade, but it needs more tempering before it is ready.",
							"~Remember, once thou art finally satisfied with the edge, thou must quickly quench the blade in water to harden it.@");
					} else if (UI_count_objects(AVATAR, SHAPE_SWORDBLANK, QUALITY_ANY, 0)) {
						//hasn't come along well at all
						say("@Why, I cannot see that thou hast made any impression on this blank at all! Put thy muscles to work, ",
							polite_title,
							", only with a strong arm and strong hammer canst thou craft a fine blade.@");
					} else {
						//ok, what the fuck kind of swordblank DO they have?
						var swordblank = UI_get_cont_items(UI_get_avatar_ref(), SHAPE_SWORDBLANK, QUALITY_ANY, FRAME_ANY);
						if (swordblank) {
							if (isBlackSword(swordblank)) {
								say("He looks the Black Sword over curiously. As he starts to run a finger along the blade, he suddenly shivers with cold -- or fear -- and hands sword back to you quickly.",
									"~@Such a blade is born of dark magics, and is beyond my ken!@");
							} else {
								//it must be too hot
								if (UI_get_item_frame(swordblank) > 0) {
									say("@Ahh, but thy blade is too hot for me to examine! Quench it first in cold water, and then I shall see thy handiwork.@");
								}
							}
						} else {
							say("But I do not see any swordblank upon thy person!");
						}
					}
				} else {
					say("@I can help thee with that when I am at work.@");
				}

			case "Could I have my sword now?" (remove):
				//Player returned for a sword they couldn't carry the last time
				if (UI_add_party_items(1, SHAPE_CUSTOM_SWORD, QUALITY_ANY, FRAME_ANY)) {
					say("@But of course!@ He fetches the sword and presents it to you.");
					say("@May thy blade stay true, ", polite_title, "!@");
					gflags[MENION_HAS_SWORD] = false;
				} else {
					say("@But again thou art too burdened! Could I interest you in some weights training, perchance?@");
					gflags[MENION_HAS_SWORD] = true;
				}
 		}
	} else if (event == PROXIMITY) {
		scheduleBarks(item);
	}
}
