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
 *	This source file contains modified code for Jaana to allow her
 *	to cast spells.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Jaana object#(0x405) () {
	var avatartitle;
	var avatarname;
	var party = UI_get_party_list();
	var choice;

	if (event == DOUBLECLICK) {
		avatartitle = getPoliteTitle();
		avatarname = getAvatarName();

		add(["name", "job", "bye"]);
		if (item in party) {
			add("leave");
			if (gflags[BROKE_TETRAHEDRON]) {
				add("Cast spell");
			}
		} else if (gflags[MET_JAANA]) {
			add("join");
		}

		if (gflags[KNOWS_COVE_GOSSIP]) {
			if (gflags[ASKED_JAANA_NAME]) {
				add("Lord Heather");
			}
		}

		if (gflags[KNOWS_JAANA_IS_HEALER]) {
			add("heal");
		}

		if (!gflags[MET_JAANA]) {
			item.say("You are surprised to see your old companion Jaana, looking only slightly aged since your last visit.");
			gflags[MET_JAANA] = true;
		} else {
			item.say("@Yes, ", avatarname, "?@ Jaana asks.");
		}

		var spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, -get_npc_number(), FRAME_ANY);
		if (!spellbook) {
			// Extra safeguard:
			spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, QUALITY_ANY, 0);
			if (spellbook) {
				spellbook->set_item_quality(-get_npc_number());
			}
		}

		converse (0) {
			case "name" (remove):
				say("@Why, I am Jaana. Thou shouldst remember me!@");

				if (gflags[KNOWS_COVE_GOSSIP]) {
					add("Lord Heather");
				}
				gflags[ASKED_JAANA_NAME] = true;

			case "job":
				add(["heal", "friends", "magic"]);
				gflags[KNOWS_JAANA_IS_HEALER] = true;

				if (item in party) {
					say("@Right now, I am travelling with thee. It is good to be travelling with mine old friends again. I can heal thee or any of our companions if thou dost wish it.@");
				} else {
					say("@I have been the Cove Healer for some time now, and can provide thee with mine healing services. Since magic is not reliable, I have been yearning to join a party of adventurers, such as mine old friends. I miss the old life!@");
					add("join");
				}

			case "heal":
				if (spellbook) {
					if (item in party) {
						say("@Let me see what I can do.@");
						/*
						JAANA.hide();
						script JAANA call aiMain;
						abort;
						*/
						serviceHeal();
					} else {
						serviceHeal();
					}
				} else {
					say("@I would be happy to, but thou hast taken away my spellbook.@");
				}

			case "Cast spell":
				if (spellbook) {
					event = DOUBLECLICK;
					spellbook->spellitem_Spellbook();
				} else {
					say("@I would be happy to, but thou hast taken away my spellbook.@");
				}

			case "friends" (remove):
				say("@Our old friends -- Iolo, Shamino, and Dupre. The men who conquer evil in the name of Lord British!@");
				add(["Iolo", "Shamino", "Dupre", "Lord British"]);

			case "join":
				var partysize = UI_get_array_size(party);
				if (partysize < 8) {
					say("@I would be honored to join thee, ", avatartitle, "!@");
					add_to_party();
					add("leave");
					remove("join");
				} else {
					say("@I do believe thou dost have too many members travelling in thy group. I shall wait until someone leaves and thou dost ask me again.@");
				}

			case "leave":
				say("@Dost thou want me to wait here or should I go home?@");
				UI_clear_answers();
				choice = askForResponse(["wait here", "go home"]);
				if (choice == "wait here") {
					say("@Very well. I shall wait until thou dost return.@*");
					remove_from_party();
					set_schedule_type(WAIT);
					abort;
				} else {
					say("@I shall obey thy wish. I would be happy to re-join if thou shouldst ask. Goodbye.@*");
					remove_from_party();
					set_schedule_type(LOITER);
					abort;
				}

			case "magic" (remove):
				if (!gflags[BROKE_TETRAHEDRON]) {
					say("@My magic has been affected by something in the air, but I have found that my senses are still with me. Hast thou noticed that the mages in the land are afflicted in the head? It is most disconcerting. Nevertheless, I can manage to cast a spell or two most of the time.@");
				} else {
					say("@I feel that the ether is flowing smoothly now. Magic is alive again!@");
				}

			case "Lord Heather" (remove):
				say("Jaana blushes. @Yes, I have been seeing our Town Mayor for some time now.@");

				if (isNearby(LORD_HEATHER)) {
					LORD_HEATHER.say("@I see that thou art leaving Cove for a while, my dear?@*");
					item.say("@Yes, milord. But I shall return. I promise thee.@*");
					LORD_HEATHER.say("@I shall try not to worry about thee, but it will be difficult.@*");
					item.say("@Do not worry. I shall be safe with the Avatar.@*");
					LORD_HEATHER.say("@I do hope so.@ The Mayor embraces Jaana.*");
					LORD_HEATHER.hide();

					if (isNearby(DUPRE)) {
						DUPRE.say("Dupre whispers to you, @And she doth say -I- am the scoundrel!@");

						if (isNearby(SHAMINO)) {
							SHAMINO.say("@Thou -art- the scoundrel!@");
						}
						DUPRE.say("@At least the only thing I've cozied up to is a mug of ale!@");
						DUPRE.say("@Ha!@, Dupre calls out. When he notices everyone is looking at him, he blushes slightly.");
						DUPRE.say("@Nothing to see here, carry on.@");
						DUPRE.hide();
						item.say("Jaana just laughs.");
					}
				}

			case "Iolo" (remove):
				if (!isNearby(IOLO)) {
					say("@Where is he? 'Twould be good to see him!@");
				} else {
					say("@He looks the same to me! Perhaps he has a little more waistline than before... but that is to be expected if one stays away from adventuring for too long!@*");

					IOLO.say("@What dost thou mean? 'Little more waistline' indeed!@*");
					IOLO.hide();

					say("@No offense intended, Iolo!@");
				}

			case "Shamino" (remove):
				if (!isNearby(SHAMINO)) {
					say("@Oh, I would love to see him. I wonder where he might be.@");
				} else {
					say("@Shamino, thou dost not look like a 'kid' anymore! What didst happen? Didst thou reach the venerable age of thirty?@*");

					SHAMINO.say("@Hmph. I am still a kid at heart.@*");
					SHAMINO.hide();

					say("@That is a relief.@ She grins cheekily.");
				}

			case "Dupre" (remove):
				if (!isNearby(DUPRE)) {
					say("@I miss having a drink or two with that rogue! Let's go find that knight!@");
				} else {
					say("@For someone recently knighted, he has retained his good looks and boyish charm, hasn't he?@*");

					DUPRE.say("@Thou dost mean 'mannish' charm, dost thou not?@*");

					item.say("@Oh, pardon -me-, sir. Thine immaturity confused me for a moment.@*");

					DUPRE.say("@Art thou going to let her get away with that, ",
						avatarname, "?@");

					if (askYesNo()) {
						say("Dupre is speechless and turns away in a huff.*");
					} else {
						say("@Good!@ Jaana winks at you from behind his back.*");
					}
					DUPRE.hide();
				}

			case "Lord British" (remove):
				say("@I have not seen our liege in many years.@");

			case "bye":
				break;

		}
		say("@Goodbye, ", avatartitle, ".@*");
	} else if (event == PROXIMITY) {
		scheduleBarks(item);
	}
}
