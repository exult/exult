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
 *	I decided to reimplement Gwenno's usecode entirely. I could have left many
 *	things for the original to handle...
 *	The goal was to make her accept the White Diamond Necklace from Iolo, who
 *	always said he lost it.
 */

// externs
extern void askGwennoBelongings 0x835 ();

void Gwenno object#(0x495) () {
	var avatarname;
	var gavenecklace;

	avatarname = getAvatarName();

	if ((event == DOUBLECLICK) || (event == SCRIPTED)) {
		AVATAR->item_say("@A pleasure to see thee...@");
		GWENNO->makePartyFaceNPC();
		if (!GWENNO->get_item_flag(SI_ZOMBIE)) {
			if (!gflags[SERPENT_GWENNO_BANE_SPEECH]) {
				gflags[SERPENT_GWENNO_BANE_SPEECH] = true;
				GWENNO->set_new_schedules(MIDNIGHT, DESK_WORK, [0x977, 0x48C]);
				GWENNO->run_schedule();
			}
			delayedBark(GWENNO, "@'Tis good to see thee!@", 2);
			GWENNO->set_schedule_type(TALK);
		} else {
			GWENNO->set_new_schedules(MIDNIGHT, DESK_WORK, [0x977, 0x48C]);
			GWENNO->run_schedule();
			if (!gflags[SERPENT_GWENNO_BANE_SPEECH]) {
				gflags[SERPENT_GWENNO_BANE_SPEECH] = true;
				script getPathEgg(5, 4) after 30 ticks {
					nohalt;
					call startSerpentSpeechViaRing;
				}
			}
			var barks = [
				"@I must sate mine hunger!@",
				"@Come, allow me to feed upon thee!@",
				"@Blood! Blood everywhere!@",
				"@Let me feel thy naked flesh!@"
			];
			var rand = UI_get_random(UI_get_array_size(barks));
			delayedBark(GWENNO, barks[rand], 2);
		}
	}

	if (event == STARTED_TALKING) {
		if (GWENNO->get_item_flag(IN_PARTY)) {
			GWENNO->set_schedule_type(FOLLOW_AVATAR);
			add("leave");
		} else {
			GWENNO->run_schedule();
			add("join");
		}

		GWENNO->clear_item_say();
		GWENNO->show_npc_face0(0);

		if (!IOLO->get_item_flag(SI_ZOMBIE) && gflags[INSANITY_BANE_DEAD] &&
		    !gflags[IOLO_GWENNO_REUNITED]) {
			say("@Iolo! My beloved Iolo!@");
			say("@Oh, I had feared that I would never see thee again!@");
			IOLO.say("@My lovely Gwenno!@");
			say("@Now mine heart can sing, at the sight of thee!@");

			UI_set_conversation_slot(0);
			say("@Thou dost look pale, dear Iolo.@");
			say("@Thou didst never take care of thyself when I was away...@");

			UI_set_conversation_slot(1);
			// The original checked for the wrong frame:
			if (hasItemCount(PARTY, 1, SHAPE_AMULET, QUALITY_ANY, 8)) {
				say("@The Lady of Fawn gave me this necklace to give to thee, when at last I found thee, my love.@");
				say("@Take it as a token of my love.@");
				// The original tried to remove from Iolo only -- this could
				// lead to a problem, as it checked in the entire party for its
				// presence...
				gavenecklace = giveItem(PARTY, GWENNO, 1, SHAPE_AMULET, QUALITY_ANY, 8, true);
			} else {
				say("@The Lady of Fawn gave me a necklace to give to thee, my love. But in all my trials to find thee, I seem to have lost it.@");
				say("@I had thought to give it to thee as a token of my love.@");
			}
			UI_remove_npc_face1();
			UI_set_conversation_slot(0);

			if (gavenecklace) {
				say("@I shall cherish it always, Iolo.@");
				say("@But thou didst not have to give me anything to prove thy love. That thou art here now is proof enough.@");
			} else {
				say("@It doth not matter, Iolo.@");
				say("@Thou dost not have to give me anything to prove thy love. That thou art here now is proof enough.@");
			}
			gflags[IOLO_GWENNO_REUNITED] = true;
		}

		if (IOLO->get_item_flag(SI_ZOMBIE) && gflags[INSANITY_BANE_DEAD]) {
			say("@A thousand thanks for bringing back mine husband, ",
				avatarname, ".@");
			say("@I grieve that his wits seem to have left him. But at least he is safe with me.@");
			say("@I shall do all that I can to aid thee in restoring him. Or gladly care for him all the remainder of my days... even as he is.@");
		}

		if (!GWENNO->get_item_flag(MET)) {
			GWENNO->set_item_flag(MET);
			say("@What a relief to see thee again, ", avatarname, ".@");
			say("@I fear that thy work hath only begun. Batlin's deeds have only worsened the storms.@");
			say("@As the sand dwindles within the Hourglass of Fate, the danger will only increase. Thou must find the answer quickly, Avatar!@");
		} else {
			say("@I have been doing much study, ", avatarname, ".@");
			say("@There is much I must tell thee before the sand runs down in the Hourglass of Fate.@");
		}

		if (!gflags[KNOWS_OF_SOUL_GEMS]) {
			add(["Batlin's deeds", "danger", "answer"]);
		}

		if (gflags[KNOWS_OF_SOUL_GEMS] && (!gflags[ANARCHY_BANE_DEAD] ||
		    !gflags[INSANITY_BANE_DEAD] || !gflags[WANTONNESS_BANE_DEAD])) {
			say("@Avatar! I have found a most intriguing scroll that tells of some sort of mystic connection between the Banes and the ancient temples!@");
			add("mystic connection");
		}

		if (gflags[TALKED_TO_GREAT_HIEROPHANT] &&
		    !gflags[TALKED_TO_CHAOS_HIEROPHANT]) {
			say("@Now I understand what Xenka meant by being aided by specters. But, unless thou canst find the grave of the last Chaos Hierophant, I know not how thou wilt restore Balance.@");
			say("@Perhaps if thou wouldst ask one of the monks -- perhaps Thoxa -- she could try to divine where the Chaos Hierophant lies.@");
		}

		if (gflags[GWENNO_HAS_BELONGINGS] && !GWENNO->get_item_flag(IN_PARTY) &&
		    GWENNO->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY)) {
			add("belongings");
		}
		add(["bye"]);

		converse (0) {
			case "belongings" (remove):
				askGwennoBelongings();

			case "join" (remove):
				if (gflags[DUPRE_IS_TOAST]) {
					if (UI_get_array_size(UI_get_party_list2()) < 5) {
						add("leave");
						say("@If thou dost think I can be of use...@");
						GWENNO->add_to_party();
						gflags[GWENNO_HAS_BELONGINGS] = true;
					} else {
						say("@I hardly think that thou hast need of me, Avatar. Look at all thy fine companions!@");
						say("@Instead, I shall remain here. After all, what can one old woman do for thee?@");
					}
				} else if (IOLO->get_item_flag(SI_ZOMBIE)) {
					say("@But I must remain here and study! I must help thee find a way to restore my dear Iolo!@");
				} else {
					say("@I will be of more use to thee if I remain here. I seem to have a talent for finding the information thou dost need.@");
				}

			case "leave" (remove):
				add("join");
				say("@I understand, ", avatarname, "...@");
				GWENNO->remove_from_party();
				npcAskWhereToWait(GWENNO);

			case "Batlin's deeds" (remove):
				say("@Until Batlin interfered, this land had no Balance, but complete chaos was held in check... Until he loosed the Banes.@");
				say("@Unless Balance is restored, this land shall die... And take every world that touches it into the dust of oblivion as well.@");
				add(["Banes", "every world"]);

			case "Banes" (remove):
				UI_push_answers();
				say("@The Banes are the splintered force of Chaos.@");
				say("@Thou wilt have to cage them again, as they were before Batlin came. 'Tis only a temporary solution at best.@");
				say("@Perhaps I can consult the materials here on Monk Isle and discover a way to rid this land of the threat entirely.@");
				add(["cage them", "materials", "change subject"]);

			case "cage them" (remove):
				say("@Thou shouldst go to Moonshade.@");
				say("@If thou canst learn who gave Batlin the secret of trapping souls, I will try to learn what can make them able to withstand the forces that we wish to contain.@");
				add(["Moonshade", "secret"]);
				gflags[SEEKING_SOUL_TRAP_SECRET] = true;

			case "Moonshade" (remove):
				say("@I do not believe that caging souls was possible within Britannia. Therefore, the spell must have come from this land.@");
				say("@And where else but Moonshade wilt thou find a spell in this land? But they are a closed-mouthed lot -- I wish thee luck.@");

			case "secret" (remove):
				say("@Although I spent quite a bit of time within Moonshade, I heard nothing about such a spell.@");
				say("@Since the Mages are all basically vain, that must mean that the spell is a secret... or at least not common knowledge.@");

			case "materials" (remove):
				say("@The Monks have said that they have an extensive library here. Perhaps, with all I have learned from my travels throughout the land, I can use their books and scrolls to find the answer.@");

			case "every world" (remove):
				UI_push_answers();
				say("@Britannia -- even thine home, Avatar -- is in deadly peril!@");
				say("@No world exists without influencing another. So all may be destroyed if New Sosaria dies.@");
				add(["New Sosaria", "change subject"]);

			case "New Sosaria" (remove):
				say("@According to the Mages of Moonshade, New Sosaria is the name of this land, ",
					avatarname, ", though 'tis often called Serpent Isle.@");
				say("@The forefathers of all the people here were dissenters of Lord British's rule.@");
				add(["Serpent Isle", "Lord British's rule"]);

			case "Serpent Isle" (remove):
				say("@I came upon the ruins of a civilization in many places during my travels.@");
				say("@They all bore the mark of some serpent design or another. I noted that there appeared to be a different serpent motif in different areas.@");
				add("serpent motif");

			case "serpent motif" (remove):
				say("@One serpent always seems to crawl toward the left. Another serpent always crawls toward the right, and the remaining serpent always crawls straight.@");
				say("@I have no idea if it means anything, but there was a difference between the different ruins.@");

			case "Lord British's rule" (remove):
				say("@It doth seem difficult to believe that they could be talking about the Lord British we know.@");
				say("@But perhaps time runs differently here than in Britannia.@");

			case "danger" (remove):
				UI_push_answers();
				say("@Thou hast already experienced the power of the Storms of Imbalance. As time passes, these will increase in frequency and strength.@");
				say("@After a further period of time, earthquakes will begin to tear the land apart... Until it at last destroys itself.@");
				add(["Storms of Imbalance", "period of time", "change subject"]);

			case "Storms of Imbalance" (remove):
				say("@The Prophecies of Xenka speak of the Storms of Imbalance.@");
				say("@Storms unlike any living man hath seen. Storms whose results no man can predict...@");
				say("@I am sorry, the prophecies are cryptic at best.@");
				add(["prophecies", "cryptic"]);

			case "prophecies" (remove):
				say("@The monks have recorded all of Xenka's visions. They keep this tome of wisdom within their Chapel, on the altar.@");
				say("@It is very confusing to read.@");

			case "cryptic" (remove):
				say("@The language is vague and hard to understand.@");
				say("@Much of it sounds like bad poetry... In fact, if Iolo's songs were as bad, I would break his lute rather than allow him to inflict them on others!@");

			case "period of time" (remove):
				say("@I do not know! These forsaken prophecies are so poorly written that I cannot make any headway in finding any reference to any other!@");
				say("@I do not know who is responsible, the monks or Xenka herself, but I sometimes feel as if I could strike someone!@");

			case "answer" (remove):
				UI_push_answers();
				say("@According to Xenka's visions, 'The Hero from Another World will succeed through the use of powerful magic, the aid of specters, and the implements of the Hierophant.'@");
				say("@Before the sands run down, we must find the meaning of these accursed riddles!@");
				add(["powerful magic", "specters", "implements", "change subject"]);

			case "powerful magic" (remove):
				say("@I know not what a farm wife would consider great magic!@");
				say("@For all I know it could be as simple as lighting a candle! Or perhaps she is referring to the power to trap souls.@");
				say("@It would be so much more simple if Xenka would return and explain these infernal riddles herself! She is worse than Chuckles!@");

			case "specters" (remove):
				say("@For all I know, Xenka is talking about herself!@");
				say("@All of these monks are convinced that she will return to help thee finish thy quest... While I wade through all these insane ramblings!@");
				say("@What I need is to talk to her specter and see if I can wring any sense out of her rather than these writings!@");

			case "implements" (remove):
				say("@From what Karnax hath told me, a Hierophant was some sort of priest or holy man for these Ophidians.@");
				say("@What tools would a holy man use? Something for blessing people, perhaps... I know not. I must search further for more clues.@");

			case "mystic connection" (remove):
				UI_push_answers();
				say("@Before thou canst redeem thy faithful companions, thou must take the soul cages and bathe them in the water of the temple that is connected to that Bane.@");
				say("@And whilst thou art at each temple, according to the scroll, there is a device of some sort through which thou canst divine the location of each Bane.@");
				add(["bathe", "temple", "device", "change subject"]);

			case "bathe" (remove):
				say("@The soul cages... er, prisms... that thou dost have cannot withstand the powers of beings such as the Banes.@");
				say("@Only a prism purified by the water of the appropriate temple can hold each Bane. Otherwise, the prism will be ineffective.@");

			case "temple" (remove):
				say("@Karnax did tell me that Miggim doth have maps to the various temples...@");
				say("@Apparently each temple was dedicated to one of the six virtues of the Ophidians. Thou shouldst seek out the correct temple before thou canst defeat the Banes.@");
				add(["Karnax", "Miggim", "Ophidians"]);

			case "Karnax" (remove):
				say("@Karnax is quite pompous. He is more inclined to lecture me than to help.@");
				say("@But I will gladly suffer his speeches if they can help me find the way to restore mine husband.@");
				say("@His knowledge of the history of these islands hath been invaluable!@");

			case "Miggim" (remove):
				say("@Miggim is very quiet and helpful.@");
				say("@He hath been most helpful reading books and scrolls that might hold some clue or another. I am thankful for his knowledge of this library... without him I would never find anything!@");

			case "Ophidians" (remove):
				say("@'Ophidians' is the word that Karnax uses for the people that once dwelled in the serpent ruins throughout the land.@");
				say("@I do not know if it is their name for themselves, or if it is a name that he concocted for them.@");

			case "device" (remove):
				say("@Once again, Avatar, I have nothing further that I can tell thee. I have been unable to find any more references to devices within the temples.@");
				say("@I can find tallies of grain given to the temples, or lists of stonework hired by each temple caretaker. But I can find nothing else to help thee... I will keep searching.@");

			case "change subject":
				say("@Certainly...@");
				UI_pop_answers();

			case "bye":
				UI_remove_npc_face0();
				delayedBark(AVATAR, "@I shall return...@", 0);
				delayedBark(GWENNO, "@Hurry back!@", 2);
				break;

		}
	}
}
