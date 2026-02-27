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
 *	This source file contains modified code for Mariah to allow her
 *	to cast spells and join after the tetrahedron generator is destroyed.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

extern void mariahSellSpells 0x8BB (var var0000);
extern void mariahSellReagents 0x8BC (var var0000);

void Mariah object#(0x499) () {
	static var did_post_tetrahedron;
	if (event == DOUBLECLICK) {
		var avatartitle = getPoliteTitle();
		var avatarname = getAvatarName();
		var party = UI_get_party_list();

		if (!gflags[MET_MARIAH]) {
			MARIAH.say("You see your old friend Mariah.");
			gflags[MET_MARIAH] = true;
		} else {
			if (gflags[BROKE_TETRAHEDRON]) {
				MARIAH.say("@Yes, ", avatarname,
					"? How may I help thee?@ Mariah greets you.");
			} else {
				MARIAH.say("@Yes, ", avatartitle,
					"?@ Mariah smiles, a trifle too sweetly.");
			}
		}

		if (!did_post_tetrahedron) {
			did_post_tetrahedron = true;
			//Give an exp boost to Mariah, so she won't be so far outclassed by other companions
			var totalexp;

			//Calculate the average exp of all party members
			for (npc in party) {
				totalexp += npc->get_npc_prop(EXPERIENCE);
			}

			totalexp /= UI_get_array_size(party);
			set_npc_prop(EXPERIENCE, totalexp - get_npc_prop(EXPERIENCE));
		}

		add(["name", "job", "bye"]);
		if (gflags[BROKE_TETRAHEDRON]) {
			if (item in party) {
				add("leave");
				add("Cast spell");
			} else {
				add("join");
			}
			converse (0) {
				case "name" (remove):
					if (isNearby(IOLO)) {
						IOLO.say("@Surely thou dost recognize thine old companion, Mariah?@*");
						IOLO.hide();
						MARIAH->show_npc_face(0);
					} else {
						say("@Hast thou already forgotten me, ", avatarname,
							"? I am Mariah.@");
					}


				case "job":
					add("spells");
					if (item in party) {
						say("@I am one of thy travelling conpanions, ",
							avatarname, ". I am also a mage, as thou shouldst remember.@");
					} else if (get_schedule_type() == WAIT) {
						say("@I am waiting to join thy company again.@");
					} else {
						say("@I sell spells, reagents, and sometimes a few potions here at the Lycaeum. Dost thou wish to buy any of these, ",
							avatarname, "?@");
						add(["reagents", "potions", "Lycaeum"]);
					}

				case "Cast spell":
					var spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, -get_npc_number(), FRAME_ANY);
					if (!spellbook) {
						// Extra safeguard:
						spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, QUALITY_ANY, 1);
						if (spellbook) {
							spellbook->set_item_quality(-get_npc_number());
						}
					}
					if (spellbook) {
						event = DOUBLECLICK;
						spellbook->spellitem_Spellbook();
					} else {
						say("@I would be happy to, but thou hast taken away my spellbook.@");
					}

				case "join":
					var partysize = UI_get_array_size(party);
					if (partysize < 8) {
						say("@I would be honored to join thee, ",
							avatartitle, "!@");
						add_to_party();
						add("leave");
						remove("join");
					} else {
						say("@I do believe thou dost have enough members in thy group. I shall await until someone leaves and thou dost ask me again.@");
					}

				case "leave":
					say("@Dost thou want me to wait here or should I go home?@");
					UI_clear_answers();
					var choice = askForResponse(["wait here", "go home"]);
					if (choice == "wait here") {
						say("@Very well. I shall wait until thou dost return.@*");
						remove_from_party();
						set_schedule_type(WAIT);
						abort;
					} else {
						say("@I shall obey thy wish. I would be happy to re-join if thou shouldst ask. Fair days ahead, friend ",
							avatarname, ".@*");
						remove_from_party();
						set_schedule_type(LOITER);
						abort;
					}

				case "spells":
					mariahSellSpells(MARIAH);

				case "reagents":
					mariahSellReagents("Reagents");

				case "potions":
					say("@I am afraid, ", avatarname,
						", that I have a very meager selection.@");
					mariahSellReagents("Potions");

				case "Lycaeum" (remove):
					say("She shakes her head sadly. @I have not been `myself' for so long that I no longer recognize this town.@ Her eyes widen.",
						"~~ @There are so many buildings around the Lycaeum now, hast thou seen them?@",
						"~~She pauses, looking at you.",
						"~~@By the way, old friend. I assume thou art responsible for returning the ether to its normal state. I thank thee.@");

				case "bye":
					say("@Fair days ahead, friend ", avatarname, ".@*");
					abort;

			}
		} else {
			converse (0) {
				case "name" (remove):
					if (isNearby(IOLO)) {
						IOLO.say("@Surely thou dost recognize thine old companion, Mariah?@*");
						IOLO.hide();
						MARIAH.say("@Yes, dost thou not recognize me?@ She pauses, glaring at you. @But who art thou, and where are my pastries?@");
					} else {
						say("@Yes, thou mayest tell me thy name,@ she says, glancing around the building. @Are not the many books beautiful?@");
					}

				case "job":
					say("She smiles. @I have a very important job, I do. My, are not those shelves lovely? So neat and orderly.@ She looks back at you.",
						"~~@Be careful! The ink wells are full, and the quills so sharp.@ She giggles.");
					add(["shelves", "ink wells", "quills"]);

				case "shelves" (remove):
					say("@Are not they the neatest, most orderly, and well-kept shelves thou hast seen? They do an excellent job of maintaining them!@");
					add("they");

				case "ink wells" (remove):
					say("@They are always so full and ready for use. They are so good about keeping them filled and clean!@");
					add("they");

				case "quills" (remove):
					say("@Oh, yes, they are quite sharp! Always there when one needs to scribe a missive. They do an excellent job of having many ready at a moment's notice!@");
					add("they");

				case "they" (remove):
					say("@Yes, they do!@ Her face turns sad. @But I only sell.@");
					add("sell");

				case "sell" (remove):
					say("@Yes,@ she agrees, @I do indeed sell. I even spell. In fact, I even sell spells! But, if thou desirest reagents, thou art out of luck, for I only sell those during one of the seven weekdays. Wouldst thou like to know which day?",
						"~~@What a lovely set of books thou must have! I have just the item for thee to match thy shelves -- a potion. If thou wilt buy a spell or reagent from me, I will sell thee a potion for only its normal price!@");
					add(["which day", "reagents", "potions"]);

				case "which day" (remove):
					say("@Why, today. Thou art in luck. Buy a spell.@");
					mariahSellSpells(MARIAH);

				case "reagents":
					mariahSellReagents("Reagents");

				case "potions":
					mariahSellReagents("Potions");

				case "bye":
					say("@Certainly, come back anytime and buy.@*");
					abort;
			}
		}
	} else if (event == PROXIMITY) {
		if ((MARIAH->get_schedule_type() == LOITER) && !gflags[BROKE_TETRAHEDRON]) {
			var barks = [
				"@Where -are- those pastries!@",
				"@Lovely, lovely shelves!@",
				"@Lovely, lovely ink wells!@",
				"@Magic is in the air...@"
			];
			MARIAH->item_say(barks[UI_get_random(UI_get_array_size(barks))]);
		} else {
			scheduleBarks(MARIAH);
		}
	}
}
