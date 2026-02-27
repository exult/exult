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
 *	This source file contains usecode for the Lock Lake Cleanup mod.
 *	Specifically, the Cove mayor now returns Mack's key to the Avatar
 *	if the Avatar wants it.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void LordHeather object#(0x44D) () {
	var player_female;
	var msg;

	if (event == DOUBLECLICK) {
		LORD_HEATHER->show_npc_face(0);
		player_female = UI_is_pc_female();

		add(["name", "job", "bye"]);

		if (gflags[HEARD_ABOUT_NATASSIA]) {
			add("Nastassia");
		}
		if (gflags[HAS_MIRANDAS_BILL]) {
			add(["bill", "Lock Lake"]);
		}
		if (gflags[KNOWS_MACKS_KEY_WITH_COVE_MAYOR] && gflags[MACKS_KEY_WITH_COVE_MAYOR]) {
			add("key");
		}
		if (!gflags[MET_LORD_HEATHER]) {
			say("This regal gentleman epitomizes a well-liked politician.");
			say("@Hello! Lord British sent word that thou might come to visit us. Welcome to Cove, Avatar!@");
			gflags[MET_LORD_HEATHER] = true;
		} else {
			say("@Hello again, Avatar!@ Lord Heather proclaims.");
		}
		converse (0) {
			case "name" (remove):
				say("@I am Lord Heather. And I recognize thee, Avatar!@");

			case "job":
				say("@I am the Town Mayor of Cove, home of the Shrine of Compassion.@");
				add(["Cove", "Shrine"]);

			case "Cove" (remove):
				say("@It's a small place, I know. Many of our residents have moved away to the larger towns, especially Britain. But we have maintained a small core of loyal Covites.@");

			case "Shrine" (remove):
				say("@We are proud of our Shrine. One of our residents takes good care of it. Thou must try and visit the Shrine if thou hast not already. It is a monument to all the lovers in town.@");
				add("lovers");

			case "lovers" (remove):
				say("@Britain may be the city of Compassion, but Cove has become the city of Passion. Everyone here seems to fall in love rather easily. Thou wilt find that everyone loves someone. Almost everyone, that is.@");
				add(["everyone", "almost everyone"]);

			case "everyone" (remove):
				say("@Well, let's see... I am in love with Jaana, our healer. And she is in love with me, of course. Then there is Zinaida, who runs the Emerald. She has an interest in De Maria, our local bard. And vice versa. Rayburt, our trainer, is courting Pamela, the innkeeper.@");

				if (isNearby(IOLO)) {
					IOLO.say("@Sounds like bad theatre to me!@");
					IOLO.hide();
				}
				if (isNearby(SPARK)) {
					SPARK.say("@Any wenches mine own age around here?@*");
					SPARK.hide();
				}
				gflags[KNOWS_COVE_GOSSIP] = true;

				if (isNearby(JAANA)) {
					say("@I see that thou art leaving Cove for a while, my dear?@*");

					JAANA.say("@Yes, milord. But I shall return. I promise thee.@*");

					LORD_HEATHER.say("@I shall try not to worry about thee, but it will be difficult.@*");

					JAANA.say("@Do not worry. I shall be safe with the Avatar.@*");

					LORD_HEATHER.say("@I do hope so.@ The Mayor embraces Jaana.*");
					JAANA.hide();
				}

			case "almost everyone" (remove):
				say("@Except for Nastassia.@");
				add("Nastassia");

			case "Nastassia" (remove):
				if (!gflags[HEARD_ABOUT_NATASSIA]) {
					say("@She is a lovely young woman who is always melancholy. De Maria can tell thee more about her. I suggest thou seekest him at the Emerald. 'Tis a sad but compelling tale.@");
					gflags[KNOWS_DEMARIA_KNOWS] = true;
				} else {
					if (player_female) {
						msg = "someone";
					} else {
						msg = "a man like thee";
					}

					say("@I do hope thou canst help her. She needs ", msg,
						" to bring her out of her depression.@");
				}

			case "bill" (remove):
				if (!gflags[LOCK_LAKE_BILL_SIGNED]) {
					if (contHasItemCount(PARTY, 1, SHAPE_SCROLL, 4, FRAME_ANY)) {
						say("@'Tis about time that the government did something about the awful stench coming from that lake! I shall be happy to sign thy bill of law! Take it back to the Great Council post haste!@ Lord Heather signs the bill and hands it back to you.");
						gflags[LOCK_LAKE_BILL_SIGNED] = true;
						UI_set_timer(LOCK_LAKE_TIMER);
					} else {
						say("@But thou dost not have a bill of law!@");
					}
				} else {
					say("@I thought I already signed that bill!@");
				}

			case "key" (remove):
				say("@Hast thou changed thy mind about trying to find the key's owner?@");
				if (askYesNo()) {
					var key = UI_create_new_object(SHAPE_KEY);
					key->set_item_quality(55);
					key->set_item_frame(19);

					gflags[MACKS_KEY_WITH_COVE_MAYOR] = false;
					if (AVATAR->give_last_created()) {
						say("@Here is the key, then. Farewell, Avatar!@");
					} else {
						UI_update_last_created(AVATAR->get_object_position());
						say("@Since thou art too encumbered to take the key, I shall place it upon the ground at thy feet. Farewell, Avatar!@");
					}
				} else {
					say("@If thou dost change thy mind, come and talk to me again. Farewell, Avatar!@");
				}

			case "Lock Lake" (remove):
				say("@It has gotten so putrid that on hot summer days the stink is suffocating. I believe that the Britannian Mining Company in Minoc is the source of the problem. Mining waste is being deposited in the Lake. Thou shouldst be glad it is nearly winter!@");

			case "bye":
				break;
		}
		if (gflags[MACKS_KEY_WITH_COVE_MAYOR] && !gflags[KNOWS_MACKS_KEY_WITH_COVE_MAYOR]) {
			//Mack's key was found by the cleaners and is with Lord Heather
			//Make Avatar aware of that:
			say("@Oh, I almost forgot! While cleaning Lock Lake, one of the residents found this key. It belongs to no one in town, and nobody really wants to keep it.");
			say("@Maybe in thy travels thou wilt find its rightful owner, if thou art willing to try. What dost thou say? Wilt thou take it?@");

			gflags[KNOWS_MACKS_KEY_WITH_COVE_MAYOR] = true;
			if (askYesNo()) {
				//Avatar wants the key; give it to him:
				var key = UI_create_new_object(SHAPE_KEY);
				key->set_item_quality(55);
				key->set_item_frame(19);

				gflags[MACKS_KEY_WITH_COVE_MAYOR] = false;
				if (AVATAR->give_last_created()) {
					say("@Here is the key, then. Farewell, Avatar!@");
				} else {
					UI_update_last_created(AVATAR->get_object_position());
					say("@Since thou art too encumbered to take the key, I shall place it upon the ground at thy feet. Farewell, Avatar!@");
				}
			} else {
				//Avatar doesn't want it, so leave the option of
				//coming for it later:
				say("@If thou dost change thy mind, come and talk to me again. Farewell, Avatar!@");
			}
		} else {
			say("@Do come and visit again, Avatar!@*");
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(LORD_HEATHER);
	}
}
