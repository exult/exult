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
 *	I decided to reimplement Shamino's usecode entirely. I could have left some things
 *	for the original to handle, but it wouldn't allow me to do what I wanted: which
 *	was (1) to make him refuse to leave while on Spinebreaker Mountains and
 *	(2) fixing the bug that he doesn't add his exchanged items to the list.
 */

extern void askShaminoBelongings 0x863 ();

const int BEATRIX_FACE = -286;

void Shamino object#(0x402) () {
	var shamino_id;
	var avatartitle;
	var avatarfemale;
	var avatarname;
	var trainer;
	var strength;
	var health;
	var delta;
	var frigmessage;
	var frigpresents;

	shamino_id = SHAMINO->get_npc_id();
	avatartitle = getPoliteTitle();
	avatarfemale = UI_is_pc_female();
	avatarname = getAvatarName();

	if ((event == DEATH) && SHAMINO->get_item_flag(SI_TOURNAMENT)) {
		if (gflags[MONITOR_TRAINING]) {
			trainer = SHAMINO->get_oppressor();
			trainer = (0 - trainer);
			if (!gflags[TEMP_FLAG_1]) {
				gflags[TEMP_FLAG_1] = true;
				trainingEndDialog(trainer->get_npc_object(), SHAMINO->get_npc_object());
				return;
			}
			endMonitorTraining(item);
			return;
		} else if (gflags[BEATRIX_PROTECTION]) {
			if ((getAvatarLocationID() != SHAMINOS_CASTLE) &&
			    (!gflags[BANES_RELEASED] &&
				    (getAvatarLocationID() != SPINEBREAKER_MOUNTAINS))) {
				BEATRIX_FACE->show_npc_face0(0);
				say("@Do not die, my sweet King...@");

				SHAMINO->show_npc_face1(0);
				say("@Beatrix, is that thee?@");
				UI_remove_npc_face1();

				UI_set_conversation_slot(0);
				say("@I forgive thee for deserting us, King Shamino. To prove that my love for thee is stronger than eternity, I shall heal thee of thy wounds.@");
				say("@Keep this book as a keepsake between us. Now I must go into the Void, but remember me always...@");

				strength = getPropValue(SHAMINO, STRENGTH);
				health = getPropValue(SHAMINO, HEALTH);
				delta = (strength - health);
				setPropValue(SHAMINO, HEALTH, delta);
				giveItemsToPartyMember(SHAMINO, 1, SHAPE_BOOK, 63, 0, 0, true);
				gflags[BEATRIX_PROTECTION] = false;
				gflags[BEATRIX_FORGAVE_SHAMINO] = true;
				SHAMINO->clear_item_flag(SI_TOURNAMENT);
			} else {
				SHAMINO->clear_item_flag(SI_TOURNAMENT);
				SHAMINO->reduce_health(50, NORMAL_DAMAGE);
			}
		}
	} else if (event == DOUBLECLICK) {
		AVATAR->item_say("Shamino...");
		SHAMINO->makePartyFaceNPC();
		if (!SHAMINO->get_item_flag(SI_ZOMBIE)) {
			delayedBark(SHAMINO, "@Yes, " + avatarname + "?@", 2);
			SHAMINO->set_schedule_type(TALK);
		} else {
			var barks = ["@Backward, not forward!@", "@Free thyselves!@", "@Chaos reigns!@", "@Take from the rich!@"];
			var rand = UI_get_random(UI_get_array_size(barks));
			delayedBark(SHAMINO, barks[rand], 2);
		}
	} else if (event == STARTED_TALKING) {
		SHAMINO->clear_item_say();
		if (SHAMINO->get_item_flag(IN_PARTY)) {
			SHAMINO->set_schedule_type(FOLLOW_AVATAR);
			add("leave");
		} else {
			SHAMINO->run_schedule();
			add("join");
		}

		SHAMINO->show_npc_face0(0);
		if (gflags[EQUIPMENT_EXCHANGED] && (!gflags[SHAMINO_MADE_EQUIPMENT_LIST])) {
			say("@Avatar! Art thou all right? I heard what sounded like the explosions of a volcano -- and look at all the fire here...@");
			say("@I have been searching all over for thee! That storm hath left me quite disconcerted. I can find neither Iolo nor Dupre.@");
			say("@And, to make matters worse, nothing remains of the equipment that Lord British gave us! It hath all been replaced by strange items that I do not recognize.@");
			say("@Perhaps thou shouldst inspect thine equipment as well! I shall make a list of what we have.@");

			giveItemsToPartyMember(AVATAR, 1, SHAPE_SCROLL, 189, 0, 0, true);
			gflags[SHAMINO_MADE_EQUIPMENT_LIST] = true;
			delayedBark(SHAMINO, "@Such strangeness!@", 0);
			unfreezeAvatar();
			SHAMINO->add_to_party();
			setExchangedItemFlags();
			gflags[SHAMINO_HAS_BELONGINGS] = true;
			SHAMINO->set_new_schedules(MIDNIGHT, EAT_AT_INN, [0x97C, 0x464]);
			abort;
		}

		if (shamino_id == BOOTED_FOR_FREEDOM) {
			say("@Thou hast escaped the foul mages' prison swiftly, indeed. Most wonderful!@");
			say("@Of course, I never doubted that thou wouldst return...@");
			say("@I also have news, ", avatartitle,
				". I bear a message, as well as two presents.@");
			frigmessage = false;
			frigpresents = false;
			add(["message", "presents"]);
			if (!gflags[AFTER_FREEDOM_NEWS]) {
				add("news");
			}
			if (gflags[SHAMINO_TELEPORTED_BY_MONKS]) {
				add("How didst thou arrive?");
			}
			if (gflags[SHAMINO_RESURRECTED_BY_MONKS]) {
				add("death");
			}
			remove("join");
		} else if (shamino_id == CURED_OF_INSANITY) {
			say("@Thank thee, Avatar, for restoring my mind! As always, I am ready to provide whatever aid I can, ",
				avatartitle, ".@");
			SHAMINO->set_npc_id(0);
			SHAMINO->add_to_party();
			script SHAMINO after 15 ticks call wraperXenkaReturns;
			remove("join");
			add(["leave", "bye"]);
		} else {
			say("@I am ready to provide whatever aid I can, ",
				avatartitle, ".@");

			if (DUPRE->get_npc_id() == BOOTED_FOR_FREEDOM) {
				add("Dupre's whereabouts");
			}
			if (IOLO->get_npc_id() == BOOTED_FOR_FREEDOM) {
				add("Iolo's whereabouts");
			}
			if (BOYDON->get_npc_id() == BOOTED_FOR_FREEDOM) {
				add("Boydon's whereabouts");
			}
			if (!SHAMINO->get_item_flag(IN_PARTY) &&
			    (SHAMINO->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY) &&
				    gflags[SHAMINO_HAS_BELONGINGS])) {
				add("belongings");
			}
			add(["bye"]);
		}

		add("time");

		converse (0) {
			case "time":
				say("@What time format dost thou prefer, Avatar?@");
				gflags[TIME_FORMAT_24_HOURS] = chooseFromMenu2(["12 hour", "24 hour"]) - 1;
				var msg = 12 * (gflags[TIME_FORMAT_24_HOURS] + 1);
				say("@Watches and sundials will now use ",
					msg, "-hour format.@");

			case "belongings" (remove):
				askShaminoBelongings();

			case "How didst thou arrive?" (remove):
				say("@The monks came and brought me here, saying that it was vitally important for me to be here.@");
				say("@The one called Karnax said that I must fulfill my purpose, or the world itself would be in jeopardy...@");
				if (frigmessage && frigpresents) {
					addShaminoToParty();
				}

			case "death" (remove):
				say("@That is right, I was dead when last we saw one another. However, the good monks found my body and returned life to me.@");
				if (frigmessage && frigpresents) {
					addShaminoToParty();
				}

			case "news" (remove):
				if (IOLO->get_item_flag(IN_PARTY)) {
					say("@To be honest, this is not my news -- it belongs to our good friend, the Bard. Thou shouldst ask Iolo.@");
				} else {
					if (IOLO->get_npc_id() == BOOTED_FOR_FREEDOM) {
						say("@Quickly, let us find Iolo. He can tell thee all about the discovery!@");
					} else {
						say("@The Mages of this place are not friendly toward outsiders, but thy survival shall surely impress them.@");
						say("@Particularly, I am sure that the one known as Gustacio will be inclined to help us now. Let us seek him out.@");
						gflags[AFTER_FREEDOM_NEWS] = true;
					}
				}

				if (frigmessage && frigpresents) {
					addShaminoToParty();
				}

			case "message" (remove):
				frigmessage = true;
				if (avatarfemale) {
					say("@I have had the chance to spend a few hours in the company of the Mage Frigidazzi, on whose account thou hast been imprisoned.@");
					say("@She asked me to convey her apologies to thee, and to present thee with three gifts.@");
				} else {
					say("@The Sorceress Frigidazzi met secretly with me, and expressed her sorrow at what had been done to thee.@");
					say("@I think she likes thee, ", avatartitle,
						". But with that MageLord around, she doth not dare to see thee again!@");
					say("@As a sign of her regret for what hath happened -- jail and all -- she sent three gifts to thee.@");
				}

				if (frigmessage && frigpresents) {
					addShaminoToParty();
				}

			case "presents" (remove):
				frigpresents = true;
				if (avatarfemale) {
					say("@These earrings are of ancient devising, and I was told that they would be most flattering to thee.@");
					say("@Note that they are engraved with serpentine runes, milady.@");
					say("@There is also a magic scroll, upon which is inscribed a spell to protect against heat. This we shall need ere we venture into the subterranean passageways.@");
				} else {
					say("@I bear three gifts from thine admirer -- a magic scroll, an earring, and a note.@");
					say("@The scroll bears a spell of protection against heat, and should be most useful.@");
					say("@As for the earring, it is inscribed with serpentine runes of ancient design.@");
				}

				say("@Lastly, there is a note, but its contents are not known to me.@");
				say("@Here they are...@");

				giveItemsToPartyMember(AVATAR, 1, SHAPE_SERPENT_EARRINGS, QUALITY_ANY, 0, 0, true);
				giveItemsToPartyMember(AVATAR, 1, SHAPE_SCROLL, 205, 6, 0, true);
				giveItemsToPartyMember(AVATAR, 1, SHAPE_MAGIC_SCROLL, 16, 0, 0, true);
				gflags[HAVE_CHILL_SPELL] = true;

				if (frigmessage && frigpresents) {
					addShaminoToParty();
				}

			case "join" (remove):
				if (gflags[BEGAN_KNIGHTS_TEST] && (!gflags[SLAIN_WOLF])) {
					say("@And disregard the rules of the Test of Knighthood? ",
						avatartitle, ", I am shocked.@");
					npcSpeakIfNearby(SHMED, "@Thou must go alone.@");
					delayedBark(SHMED, "@Alone!@", 0);
					delayedBark(SHAMINO, "@Shocked!@", 2);
					abort;
				}

				if (UI_get_array_size(UI_get_party_list2()) < 5) {
					add("leave");
					say("@Most gratefully!@");
					SHAMINO->add_to_party();
					gflags[SHAMINO_HAS_BELONGINGS] = true;
				} else {
					say("@I would join with thee, old friend, but I can see that thou already hast a large party.@");
					say("@A woodsman like myself would feel uncomfortable travelling with such a large group. I will remain here, instead.@");
				}

			case "leave" (remove):
				if (!gflags[EQUIPMENT_EXCHANGED]) {
					say("@But we have barely begun, Avatar! How can I stand idly here while the adventure awaits?@");
				} else if (getAvatarLocationID() == SPINEBREAKER_MOUNTAINS) {
					say("@But we are so close to Batlin, Avatar! How can I stand here idly while thou dost risk thy life?@");
				} else {
					add("join");
					say("@If that is what thou dost wish...@");
					SHAMINO->remove_from_party();
					askShaminoBelongings();
					npcAskWhereToWait(SHAMINO);
				}

			case "Dupre's whereabouts" (remove):
				say("@Thou knowest our good friend! He hath deposited himself at the Blue Boar Inn, and hath been trying out the local brews!@");

			case "Iolo's whereabouts" (remove):
				say("@Iolo hath struck up a friendship with the Mage Gustacio, from whom he is learning much about the storms which plague this land.@");
				say("@I expect we could find him at Gustacio's manor.@");

			case "Boydon's whereabouts" (remove):
				say("@Why, that magical creation doth have a budding romance with the shopgirl from the Canton!@");
				say("@If the shop is open when we arrive there, Boydon will almost certainly be there.@");

			case "bye":
				UI_remove_npc_face0();
				delayedBark(AVATAR, "@Thanks!@", 0);
				delayedBark(SHAMINO, "@Any time.@", 2);
				break;
		}
	} else if (event == EGG) {
		var location_id = getAvatarLocationID();

		// Does anyone know if this ever happens? Or where should it happen?
		if (location_id == TOLERANCE) {
			// In the original, it was Iolo's face which appeared... which is
			// likely an error, as this is in Shamino's usecode...
			SHAMINO->show_npc_face0(0);
			say("@We are beset by dragons, Avatar! We must slay them quickly!@");
			UI_remove_npc_face0();
			delayedBark(SHAMINO, "@I hate dragons!@", 2);
			abort;
		} else if (location_id == SHAMINOS_CASTLE) {
			SHAMINO->show_npc_face0(0);
			say("@It hath been so long I barely recognize this place, like somewhere from a dream. I do recall that there is an illusory door west of the castle gates near a big tree.@");
			UI_remove_npc_face0();
			delayedBark(SHAMINO, "@Somewhere around here...@", 3);
		}
	}
}
