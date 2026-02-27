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
 *	This source file contains usecode to make Lord British use the
 *	Generic Healing Service.
 *
 *	Author: Marzo Junior, based on Alun Bestor's code for LB
 *
 *	Last Modified: 2006-06-18
 */

//LB's healing conversation thread
//extern void performLBHealing 0x8B4(var price_cure, var price_heal, var price_resurrect);

//Conversation thread for the crystal and ship deed for the Isle of Fire
extern void giveFVDeedAndCrystal 0x8B5();

void giveFoVReward object#() () {
	if (!gflags[LB_REWARDED_FOR_FV]) {
		gflags[LB_REWARDED_FOR_FV] = true;

		struct<Position> pos = AVATAR->get_object_position();
		UI_sprite_effect(ANIMATION_TELEPORT, pos.x - pos.z/2, pos.y - pos.z/2, 0, 0, 0, -1);
		UI_play_sound_effect(67);

		var str = AVATAR->get_npc_prop(STRENGTH);
		var hps = AVATAR->get_npc_prop(HEALTH);
		if (str < 60) {
			AVATAR->set_npc_prop(STRENGTH, 60 - str);
		}
		if (hps < 60) {
			AVATAR->set_npc_prop(HEALTH, 60 - hps);
		}
	} else {
		item.say("@I congratulate and thank thee, ", getAvatarName(),
			". Thy deeds continue to speak well of thee.@");
	}
}

void Lord_British object#(0x417) () {
	if (event == DOUBLECLICK) {
		var avatarname = getAvatarName();

		var asked_about_moongates = false;
		var asked_about_magic = false;

		//standard conversation options
		add(["name", "job", "bye", "Fellowship"]);

		//Oops...
		if (gflags[CAST_ARMAGEDDON]) {
			item.say("@Fool!! What possessed thee to cast that damned Armageddon Spell? I knew it was dangerous! Thou didst know it was dangerous!! Now look at us! We are all alone on the entire planet! Britannia is ruined! What kind of Avatar art thou!?! Now, with no Moongates working, we are both forced to spend eternity in this blasted wasteland!",
				"~~@Of course, it could be viewed as a clever solution to all of our problems. After all, not even this so-called Guardian would want Britannia now!@*");
			abort;
		} else if (gflags[BANISHED_EXODUS]) {
			//Completed the Forge of Virtue
			//hasn't been rewarded yet
			if (!gflags[LB_REWARDED_FOR_FV]) {
				item.say("@I felt the passing of the remains of Exodus from this realm. It has lifted a great weight from my shoulders. And so Avatar, I cannot let this accomplishment go unrewarded. Please kneel, my friend.@ Lord British holds out his hands as you obey his command.");
				item.hide();

				var dir = directionFromAvatar(item);
				var inv_dir = (dir + 4) % 8;

				//The actual reward has been offloaded into a helper function.
				//Added a little bit of animation to LB too.
				script item {
					call trueFreeze;
					face inv_dir;
					//wait 6;
					//call giveFoVReward;
					wait 1;
					actor frame ready;
					wait 1;
					actor frame strike_2h;
					wait 2;
					call giveFoVReward;
					wait 4;
					actor frame ready;
					wait 1;
					actor frame standing;
					//wait 16;
					//call giveFoVReward;
					wait 11;
					call giveFoVReward;
					call trueUnfreeze;
				}

				//Avatar kneels
				script AVATAR {
					call trueFreeze;
					face dir;
					wait 1;
					actor frame bowing;
					wait 1;
					actor frame kneeling;
					wait 6;
					actor frame bowing;
					wait 1;
					actor frame standing;
					call trueUnfreeze;
				}
				abort;
			}
		} else if (!gflags[TALKED_ABOUT_RUMBLE]) {
			//has not asked about earthquake yet
			add("rumble");
		}

		if (!gflags[GOT_ORB]) {
			//Hasn't received orb of the moons yet
			add("Orb of the Moons");
		}
		if (gflags[AGREED_TO_FREE_WESTON] && !gflags[WESTON_FREED]) {
			//Has met Weston but hasn't freed him yet
			add("Weston");
		}

		if (gflags[ASKED_LB_ABOUT_HEAL]) {
			add("heal");
		}
		if (gflags[LEARNED_ABOUT_ALIGNMENT] && !gflags[ASKED_LB_ABOUT_GUARDIAN]) {
			//Has Alagner's notebook?
			add("The Guardian");
		}

		if (!gflags[MET_LORD_BRITISH]) {
			item.say("You see your old friend Lord British, looking a bit older than when you last saw him. His eyes gleam at the sight of you.",
				"~~@Welcome, my friend,@ he says, embracing you. @Please. Tell me what brings thee to Britannia! Or, more importantly, what 'brought' thee here?@");
			gflags[MET_LORD_BRITISH] = true;
			add(["red Moongate", "Orb of the Moons"]);
		} else {
			item.say("@Yes, ", avatarname, "?@ Lord British asks.");
		}

		converse(0) {
			case "name" (remove):
				say("Lord British laughs. @What, art thou joking, Avatar? Dost thou not recognize thine old friend?@");

			case "job":
				say("Lord British rolls his eyes. @Must we go through this formality?@ He laughs, shaking his head.");
				say("@Very well. As thou well knowest, I am sovereign of Britannia and have been for some time now. Even though I come from thine homeland, I have chosen to live my life here.@");
				add(["Britannia", "homeland"]);

			case "homeland" (remove):
				say("@I know that it has been many a year since I visited our Earth, but surely thou dost remember that the two of us hail from the same time and place? And, as brothers in origin, thou shouldst also remember that thou canst ask me for aid at any time thou mightest require it.@");
				add("aid");

			case "aid" (remove):
				say("@Do not forget, Avatar, that I have the power to heal thee. That is one bit of magic that still seems to work for me. And I could probably provide thee with some equipment and a spellbook.@");
				add(["equipment", "spellbook"]);
				if (!gflags[ASKED_LB_ABOUT_HEAL]) {
					add("heal");
				}
				gflags[ASKED_LB_ABOUT_HEAL] = true;

			case "Britannia" (remove):
				say("@The state of the land could not be more prosperous. Dost thou realize that thou hast been away for 200 Britannian years?@ Lord British wags a finger at you.",
					"~~ @I am certain that thy friends have rued thine absence. 'Tis a shame thou didst stay away so long! But... I am so very happy to see thee. Britannia is prosperous and abundant. Look around thee. Explore the newly refurbished castle. Travel the land. Peace is prominent in all quarters.",
					"~~@Yes, Britannia has never been better. Well, almost never.@");
				add(["friends", "castle", "almost never"]);
				if (!gflags[ASKED_LB_ABOUT_MAGIC] && !asked_about_magic) {
					add("magic");
				}

			case "almost never" (remove):
				say("@Well, 'things' are indeed fine. It is the 'people' I am concerned about.",
					"~~@There is something wrong in Britannia, but I do not know what it is. Something is hanging over the heads of the Britannian people. They are unhappy. One can see it in their eyes. There is nothing that is unifying the population, since there has been peace for so long.",
					"~~@Perhaps thou couldst determine what is happening. I implore thee to go out amongst the people. Watch them in their daily tasks. Speak with them. Work with them. Break bread with them. Perhaps they need someone like the Avatar to take an interest in their lives.@");

			case "red Moongate" (remove):
				say("You relate the story of how a red Moongate appeared behind your house and mysteriously took you to Trinsic.",
					"~~Lord British's brow creases as you speak. Finally he says, @I did not send the red Moongate to fetch thee. Someone or something must have activated that Moongate. And that is strange indeed, because we have been having a bit of trouble with Moongates as of late. In fact, we have been having trouble with magic in general!@");
				if (!asked_about_moongates) {
					add("Moongates");
				}
				if (!asked_about_magic) {
					add("magic");
				}

			case "Orb of the Moons" (remove):
				say("@Mine has not worked since the troubles with magic began. In fact, none of the Moongates have been working reliably for quite a while!");
				say("@Didst thou bring thine Orb of the Moons?@");
				if(askYesNo()) {
					say("@Really? Where is it? Thou dost not have it on thee!");
				} else {
					say("@I see.");
				}
				say("@Hmmm. Thou might be stranded in Britannia. Here. Why not try mine? I shall let thee borrow it. Perhaps it will work for thee. Be careful, though. The Moongates have become dangerous.@");
				if (UI_add_party_items(1, SHAPE_ORB, QUALITY_ANY, FRAME_ANY, false)) {
					say("Lord British hands you his Orb of the Moons.");
					gflags[GOT_ORB] = true;
				} else {
					say("@Thine hands are too full to take the Orb!@");
				}
				if (!asked_about_moongates) {
					add("Moongates");
				}
				if (!asked_about_magic) {
					add("magic");
				}

			case "castle" (remove):
				say("@Yes, it has been redecorated since thy last visit. The architects and workers did a splendid job.@",
					"~~The ruler leans toward you with a sour look on his face.",
					"~~ @The only mar in the entire complex is that damn nursery!@");
				add("nursery");

			case "nursery" (remove):
				say("@I will not go near the place! Kings and dirty diapers do not mix. The Great Council talked me into implementing the nursery after several of my staff started having families. Although it was probably a necessity, I shall pretend it does not exist!@");

			case "Trinsic" (remove):
				say("@I have not been down there in many years. Has something happened there?@");
				if (chooseFromMenu2(["a murder", "nothing much"]) != 1) {
					say("@Indeed. Then it seems that Trinsic has not changed much since I saw it last.@ His eyes twinkle.");
				} else {
					say("@Murder? In Trinsic?@ The ruler looks concerned.",
						"~~@I have heard nothing about it. Art thou investigating it?@");
					if (askYesNo()) {
						say("@Very good. It pleases me that thou art concerned about my people.@");
					} else {
						say("@Ah, but perhaps thou shouldst!@");
					}
					say("The king pauses a moment. @Now that thou dost mention it, I have had reports of other similar murders in the past few months. In fact, there was one here in Britain three or four years ago. The body was mutilated in a ritualistic fashion. Apparently there is a maddened killer on the loose. But I have no doubt that someone such as thee, Avatar, can find him!@");
					add(["ritualistic", "killer"]);
				}

			case "ritualistic" (remove):
				say("@I do not recall many details. Thou shouldst ask Patterson, the town mayor, about it. He may remember more.@");
				gflags[LEARNED_ABOUT_BRITAIN_MURDER] = true;

			case "killer" (remove):
				say("@That is, of course, only an assumption on my part. But that is all we have had to work with. Unless thou hast already uncovered some useful information?@");
				if (gflags[LEARNED_ABOUT_HOOK]) {
					add("Hook");
				}
				if (gflags[LEARNED_ABOUT_CROWN_JEWEL]) {
					add("Crown Jewel");
				}

			case "Fellowship" (remove):
				say("@They are an extremely useful and productive group of citizens. Thou shouldst most certainly visit the Fellowship Headquarters here in Britain and speak with Batlin. The Fellowship has done many good deeds throughout Britannia, including feeding the poor, educating and helping those in need, and promoting general good will and peace.@");
				add(["Batlin", "Headquarters"]);

			case "Headquarters" (remove):
				say("@Yes, it is not far from the castle, to the southwest. It is just south of the theatre.@");

			case "Batlin" (remove):
				say("@He is a druid who began The Fellowship about twenty years ago. He is highly intelligent, and is a warm and gentle human being.@");

			case "Hook" (remove):
				say("@A man with a hook?@ The king rubs his chin.",
					"~~@No, I do not recall ever meeting a man with a hook.@");

			case "Crown Jewel" (remove):
				say("@I am afraid I cannot possibly know of every ship that comes through our ports. Thou shouldst check with Clint the Shipwright if thou hast not done so.@");

			case "friends" (remove):
				say("@Thou must mean Iolo, Shamino, and Dupre, of course.@");
				add(["Iolo", "Shamino", "Dupre"]);

			case "Iolo" (remove):
				say("@I have seen our friend rarely over the years. I understand he has been spending most of his time in Trinsic.@");
				if (isNearby(IOLO)) {
					say("@Hello, Iolo! How art thou?@*");
					IOLO.say("@I am well, my liege! 'Tis good to see thee!@*");
					IOLO.hide();
				}
				add("Trinsic");

			case "Shamino" (remove):
				say("@That rascal does not come around very often, though I understand he spends most of his time in Britain these days!@");

				if (isNearby(SHAMINO)) {
					say("@What dost thou have to say for thyself, Shamino?@*");
					SHAMINO.say("@Mine apologies, milord,@ Shamino says.*");
					item.say("@What's this I hear of a woman? An actress? Hmmmm?@*");
					SHAMINO.say("Shamino blushes and shuffles his feet.*");
					item.say("@I suspected as much!@ the ruler says, laughing.");
					SHAMINO.hide();
				}

			case "Dupre" (remove):
				say("@I have not seen that one since I knighted him. Typical -- I do the man a favor and he disappears! I heard he might be in Jhelom.@");

				if (isNearby(DUPRE)) {
					say("@Where hast thou been, Sir Dupre?@*");
					DUPRE.say("@Oh, here and there, milord,@ the fighter replies.*");
					item.say("@I have very few friends from our homeland here in Britannia. Thou must make a point to visit more often! Especially since thou art a knight!@*");
					DUPRE.say("@If thou dost wish it, milord,@ Dupre says, bowing.*");
					DUPRE.hide();
				}
				add("Jhelom");

			case "Jhelom" (remove):
				say("@A rather violent place, by all accounts. I have not had the pleasure of a visit in quite a while.@");

			case "magic" (remove):
				say("@Something is awry. Magic has not been working for the longest time. I even have trouble creating food with magic! It must be something to do with the magical ether.",
					"~~@There are those who say that magic is dying, what with the trouble with the Moongates and the situation with Nystul. I am beginning to suspect that they might be right!@");
				say("Lord British studies you a moment.");
				say("@Perhaps magic will work much better for thee. Thou hast not been in Britannia long. It is possible that whatever has affected magic has not made its mark upon thee yet. Please try it. A spellbook is stored with the rest of thine equipment.@");
				gflags[ASKED_LB_ABOUT_MAGIC] = true;
				add(["Nystul", "spellbook", "equipment"]);
				asked_about_magic = true;
				if (!asked_about_moongates) {
					add("Moongates");
				}

			case "Nystul" (remove):
				if (!gflags[BROKE_TETRAHEDRON]) {
					if(!gflags[MET_NYSTUL]) {
						say("@Er... try talking to him.@");
					} else {
						say("The king lowers his voice.",
							"~~@He is acting oddly, isn't he? Something has happened to his mind. He doesn't seem to be able to concentrate on magic anymore.@");
					}
				} else {
					say("@He is beginning to act much more normally.@");
				}

			case "Moongates" (remove):
				say("@The Moongates are not functioning! We cannot use them as we have in the past. Not only are they dysfunctional, they are, in fact, dangerous! One of my trusted sages used mine own Orb of the Moons to travel to the Shrine of Humility, and his body did shatter upon entering the gate! If only that mage in Cove hadn't gone mad!@");
				add(["mad mage", "Cove"]);
				asked_about_moongates = true;

			case "mad mage" (remove):
				say("The ruler leans forward and speaks quietly.",
					"~~@There is a mad mage in Cove by the name of Rudyom. Dost thou remember him? Rudyom was working with a magical substance called 'blackrock'. Before he went mad, he claimed that this mineral could solve the problems of the Moongates. I suggest that thou shouldst go to Cove and find him. Try to learn what it was he was doing with this blackrock material. It could be our only hope.@");
				gflags[LEARNED_ABOUT_BLACKROCK] = true;
				giveExperience(20);
				add("Rudyom");

			case "Rudyom" (remove):
				say("@He was a brilliant and respected mage. But something happened to him in recent years. He seemed to go completely senile.@");
				if (gflags[MET_NYSTUL]) {
					say("Suddenly, something jars Lord British's memory. @I wonder if there is a connection with what happened to Rudyom and what has befallen Nystul!@");
				}

			case "Cove" (remove):
				say("@Surely thou dost remember Cove. It is a very pleasant town to the east of Britain. Quite relaxing.@");

			case "The Guardian" (remove):
				//what a bloody cop-out
				say("@I do not know of a 'Guardian'. Art thou sure he really exists? Thou shouldst investigate further.@");
				gflags[ASKED_LB_ABOUT_GUARDIAN] = true;

			case "spellbook" (remove):
				say("@Yes, I have a spellbook stored away with the rest of the equipment.@");

			case "equipment" (remove):
				say("@Thou art welcome to any of mine equipment. I keep it in a locked storeroom here in the castle. Thou wilt find the key in my study.@");
				add(["storeroom", "study"]);

			case "storeroom" (remove):
				say("@I am sure thou canst find it.@",
					"~~The ruler smiles slyly. @Consider it something of a game!@");

			case "study" (remove):
				say("@'Tis in the western end of the castle.@");

			case "heal" (remove):
				//performLBHealing(0, 0, 0);
				serviceHeal();

			case "Weston":
				say("Lord British listens to your story about Weston. He looks concerned.",
					"~~@I do not recall this case. Let me check... Hmmm...@ He quickly scans a large scroll.",
					"~~@Imprisoned for the theft of one apple from the Royal Orchards... Ludicrous! Someone must have usurped mine authority. Thou mayest consider this man pardoned. An investigation will commence immediately into the circumstances surrounding his arrest, and into this fellow, Figg. My thanks to thee, Avatar.@");
				gflags[WESTON_FREED] = true;
				giveExperience(20);
				//deletes him from the universe (poor man)
				WESTON->remove_npc();

			//FV stuff
			case "rumble" (remove):
				say("Lord British looks at you gravely, @The foundation of Britannia was shaken with the rising of an island. This event was no random disaster, it was one of sorcerous intent.@");
				add("island");

			case "island" (remove):
				say("@Yes, ", avatarname,
					". I felt a great disturbance in the ether when this island arose from the sea. The island is none other than the Isle of Fire where thou defeated the Hellspawn Exodus.@");
				add(["Isle of Fire", "Exodus"]);

			case "Isle of Fire" (remove):
				say("@", avatarname,
					", thou shouldst know that when I created the shrines of the Virtues, I also set upon this island three great shrines, dedicated to the Priciples of Truth, Love, and Courage.");
				say("They reside within the walls of the Castle of Fire. I never revealed this to thee before as I thought them forever lost when the Isle of Fire mysteriously sank beneath the waves.");
				say("The shrines are meant for the use of an Avatar only, and therefore a talisman will be necessary to use one.");
				say("The talismans are guarded by tests that thou shouldst have no problem passing if thou wishest to seek their counsel.@");
				giveFVDeedAndCrystal();

			case "Exodus" (remove):
				say("@Thy battle with that strange mixture of machine and spirit is now legendary. Do be careful if thou art going to the isle, for the remains of that being now reside in one of the chambers of the Castle of Fire.@");

			case "bye":
				say("@Goodbye, ", avatarname, ". Do come back soon.@*");
				return;
		}
	} else if (event == PROXIMITY) {
		scheduleBarks(LORD_BRITISH);
	} else if (event == SCRIPTED) {
		//dunno why this would be getting called using SCRIPTED: if LB's schedule changes to TALK, that calls the NPC's function using DOUBLECLICK anyway
		if (gflags[CAST_ARMAGEDDON]) {
			event = DOUBLECLICK;
			item.Lord_British();
			return;
		}
	}
}
