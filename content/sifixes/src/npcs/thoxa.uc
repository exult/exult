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
 *	This was rewritten to prevent Thoxa from "resurrecting" Iolo, Shamino and
 *	Dupre after the banes have been released. Given the way this was done in the
 *	original, I was *forced* to reimplement the entire function. Sad...
 */

void Thoxa object#(0x4D3) () {
	var avatarlocation;
	var title;
	var met_thoxa;
	var companions;
	var index;
	var max;
	var npc;
	struct<Position> pos;
	var var000E;

	avatarlocation = getAvatarLocationID();
	met_thoxa = get_item_flag(MET);

	title = "son";
	if (UI_is_pc_female()) {
		title = "daughter";
	}

	if (event == DOUBLECLICK) {
		if (KARNAX->get_npc_id() != 0) {
			item_say("@Not now...@");
			abort;
		}
		AVATAR->item_say("@Excuse me...@");
		item->makePartyFaceNPC();
		delayedBark(item, "@Yes, my " + title + "?@", 2);
		set_schedule_type(TALK);
	} else if (event == STARTED_TALKING) {
		run_schedule();
		clear_item_say();
		show_npc_face0(0);
		if ((avatarlocation == MONITOR) && gflags[AVATAR_GOT_SHORT_STICK] &&
		    !gflags[DUPRE_IS_TOAST]) {
			say("@It is written that 'The Hero from Another World shall face the end as the beginning'! Thou didst enter our land with thy three companions, they must be present at the final moment to forestall disaster!@");
			delayedBark(item, "@I shall pray for thee!@", 0);

			// Yet to be tested:
			resurrectCompanions();
			move_object([0x5FA, 0x78F, 0x0]);
			// How it is done in the original:
			/*
			companions = [IOLO, SHAMINO, DUPRE];
			for (npc in companions with index to max) {
				if (!npc->npc_nearby()) {
					npc->approach_avatar(80, 40);
					if (!(npc->get_schedule_type() == FOLLOW_AVATAR)) {
						npc->add_to_party();
					}
				}
			}
			*/
			abort;
		}

		UI_set_conversation_slot(0);
		pos = get_object_position();

		// This is the bit Thoxa says in the Spinebreaker Mountains. I added a
		// check to see if the banes have been released, so the companions can
		// no longer be brought back this way:
		if (pointInsideRect(pos, [0x914, 0x1BB], [0x939, 0x1D9]) &&
		    !gflags[BANES_RELEASED]) {
			say("@Thou must not enter this door without thy three stalwart companions, Hero from Another World!@");
			say("@It matters not if any others venture with thee, but through this portal the four must pass!@");
			say("@I shall act as a channel for the power to restore thy group as it should be.@");
			resurrectCompanions();
			script item {
				nohalt;
				// See the note in "headers/si_externals.uc"
				call gwaniChild;
			}
			abort;
		}

		say("@We meet again. Thy destiny draws near quickly... How may I be of service to thee?@");

		if (areThereBodiesNearby()) {
			say("@Oh! One of thy friends hath met an untimely end. This should not be... [@If thou art ready to see thy friend again, merely ask and I will return them to thee.@");
			add("resurrection");
		} else {
			if ((avatarlocation == MONK_ISLE) && (!met_thoxa)) {
				say("@Welcome to Monk Isle, my ", title, ".@");
				say("@Thou mayest tarry with us for as long as thou dost wish, but remember... The Sands of Time wait for no one, not even thee.@");
				say("@Whilst thou art here, thou wilt learn that the life of a monk is governed by the bells. Not all of our membership may speak to thee, the novices are bound by a vow of silence to help them better contemplate the mysteries.@");
				add(["Sands of Time", "bells", "mysteries"]);
			}
		}

		if (gflags[TALKED_TO_GREAT_HIEROPHANT] &&
		    !gflags[TALKED_TO_CHAOS_HIEROPHANT]) {
			say("@I see... Thou hast made great progress in thy quest, my ",
				title, ". But before thou canst continue, thou must seek the wisdom of the last child of Chaos. He alone holds the key to the location of the Chaos Hierophant.@");

			if (SETHYS->get_item_flag(MET)) {
				say("@Thou didst meet him in his imprisonment, my ", title,
					". Yet he remains a prisoner out of time. Seek him within the Shrine that is his home.@");
			} else {
				say("@He remains a prisoner out of time, imprisoned within the Shrine that is his home.@");
			}
		}
		add("bye");
		converse (0) {
			case "return" (remove):
				say("@Xenka disappeared several centuries ago. No one knows where she went.@");
				say("@However, in her writings, Xenka hath promised to return to us when the end is near. She will then guide us once more.@");

			case "resurrection" (remove):
				Resurrect();

			case "Sands of Time" (remove):
				say("@Within the Hourglass of Fate lie the Sands of Time. As each grain doth fall, so doth another moment hasten us to our doom.@");
				add("doom");

			case "doom" (remove):
				if (gflags[BANES_RELEASED]) {
					say("@If thou dost fail in thy quest, our world will end... ripped apart by the storms and earthquakes that doth now plague it.@");
				} else {
					say("@If thou dost fail in thy quest, our world will end... ripped apart by the earthquakes that will soon plague it.@");
				}

			case "bells" (remove):
				say("@They toll, though no one rings them. And it is said that when all the bells toll it will signal Xenka's return. Until then, they signal the order of our daily life. From Chapel to field, from field to the library, and so on.@");
				add(["return", "Chapel", "field", "library"]);

			case "Chapel" (remove):
				say("@The Chapel is where we go to meditate. It is also where all the monks gather to perform resurrections. The book of Xenka's prophecies rests on the altar for all to see.@");

			case "field" (remove):
				say("@Since we are so secluded from the rest of the land, we grow our own food. And as food concerns us all, we all share the tasks of tending to the garden and orchard.@");
				add("garden and orchard");

			case "garden and orchard" (remove):
				say("@Thou art welcome to take what thou dost need from our fields. We would gladly share with thee the bounty of the land that thou art striving to save.@");

			case "library" (remove):
				say("@Over the years we have gathered many books and scrolls that we thought would help us give light to the meaning within Xenka's prophecies. Only recently hath Miggim undertaken the task of trying to set the library to rights. It is he that helps any visitors that come to us seeking information.@");
				add(["books and scrolls", "visitors", "information"]);

			case "books and scrolls" (remove):
				say("@I fear that I do not know all we possess. Thou art free to look, of course. But if thou hast specific questions, thou shouldst direct them to Miggim.@");

			case "visitors" (remove):
				if (npcNearbyAndVisible(GWENNO)) {
					if (!GWENNO->get_item_flag(SI_ZOMBIE)) {
						say("@Such visitors are rare, for the journey here is not an easy one. As I remember it, Gwenno was our last visitor.@");
					} else {
						say("@Such visitors are rare, for the journey here is not an easy one. As I remember it, our last visitor was poor Gwenno.@");
					}
				} else {
					say("@Such visitors are rare, for the journey here is not an easy one. As I remember it, our last visitor was that woman, Gwenno.@");
				}

				add("Gwenno");

			case "Gwenno" (remove):
				if (npcNearbyAndVisible(GWENNO)) {
					if (!GWENNO->get_item_flag(SI_ZOMBIE)) {
						message("@It is a pleasure having her here, my ");
						message(title);
						message(", now that her proper state of mind hath been restored.@");
						say();
						say("@I hope to speak with Gwenno about her time with the Ice People. It would be most instructive, I am sure.@");
					} else {
						say("@'Tis a pity to see her so undone. I hope that thou canst find a way to restore her to her proper state of mind soon.@");
						say("@I had hoped to be able to speak with her about her time with the Ice People. It would be most instructive, I am sure.@");
					}
				} else {
					say("@She stayed with us for a short period of time. I think that she was seeking information concerning the Ice People of the north. But thou shouldst speak with Miggim... Gwenno spent most of her time within the library.@");
				}

				add("Ice People");

			case "Ice People" (remove):
				message("@I know very little about the Ice People, my ");
				message(title);
				message(". I know only that they are not like me and thee, but are covered in a thick hide -- like that of a bear, only white.@");
				say();

			case "information" (remove):
				say("@They used to come to us for everything from healing arts to weather predictions. Of course, that was before the storms began.@");

			case "mysteries" (remove):
				message("@Xenka's prophecies are very unclear, my ");
				message(title);
				message(". Lacking Xenka's heavenly insight, we must struggle to wrest the meaning from each passage. Some devote their entire lives to finding the meaning of only a handful of passages.@");
				say();

			case "bye":
				if (avatarlocation == MONK_ISLE) {
					UI_remove_npc_face0();
					delayedBark(AVATAR, "@Goodbye!@", 0);
					delayedBark(item, "@Fortune!@", 2);
					break;
				}

				say("@I must return to Monk Isle.@");
				UI_remove_npc_face0();
				pos = get_object_position();
				pos.x -= (pos.z / 2);
				pos.y -= (pos.z / 2);
				UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
				UI_play_sound_effect(0x51);
				remove_npc();
				abort;
		}
	} else if (event == DEATH) {
		xenkanMonkDies(item);
	}
}
