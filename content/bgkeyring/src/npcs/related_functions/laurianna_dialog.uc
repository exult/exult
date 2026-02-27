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
 *	This source file contains usecode for the Keyring Quest.
 *	Specifically, it is has Laurianna's dialog functions.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void switchFace (var facenum, var slot) {
	if (!slot) {
		UI_change_npc_face0(facenum);
	} else {
		UI_change_npc_face1(facenum);
	}
}

var lauriannaAskJoin () {
	var party = UI_get_party_list();
	var party_size = UI_get_array_size(party);

	if (party_size < 8) {
		say("@Wilt thou bring me to my father? Wilt thou let me join thee?@");
		if (askYesNo()) {
			add_to_party();
			add("leave");
			return true;
		} else {
			say("@I shall wait here until thou dost change thy mind, then.@");
		}
	} else {
		say("@Alas, thy party has no room for me... I shall await thee here shouldst thou make room for me.@");
	}

	return false;
}

void lauriannaAskJournal () {
	var journal;

	if (PARTY->count_objects(SHAPE_JOURNAL, QUALITY_ANY, FRAME_ANY)) {
		say("@Thou hast the journal in thy possession; wilt thou give it to me?@");
		if (askYesNo()) {
			UI_remove_party_items(1, SHAPE_JOURNAL, QUALITY_ANY, FRAME_ANY);
		} else {
			say("@Shouldst thou change thy mind, talk to me again.@");
			add("Give journal");
			return;
		}
	} else {
		say("@There is the journal; I shall take it with me.@");
		journal = find_nearest(SHAPE_JOURNAL, 50);
		journal->remove_item();
	}
	say("@There, I have it now.@");
	gflags[LAURIANNA_HAS_JOURNAL] = true;
}

void lauriannaGiveKeyring () {
	if (gflags[LAURIANNA_HAS_JOURNAL]) {
		say("@Thank thee for all thy help, ", getPoliteTitle(),
			". As a token of my appreciation, I shall give thee this keyring.");
		say("@I had crafted it for father, since he always lost his keys -- but alas, I never had the opportunity to give it to him.");
		say("@It is magical -- any keys added to it will disappear. Whenever thou hast need of a key, it will magically reappear, ready for use.");
		say("@I hope that thou dost find it useful...@");

		giveExperience(300);

		var keyring = UI_create_new_object(SHAPE_KEYRING);
		if (!AVATAR->give_last_created()) {
			say("@Since thou art so overburdened, I shall place it on the ground.@");
			UI_update_last_created(AVATAR->get_object_position());
		}
	}
}

void lauriannaSellReagents () {
	sellItems(
				//Names
				["nothing",
				 "Black pearl", "Blood moss", "Nightshade",
				 "Mandrake root", "Garlic", "Ginseng",
				 "Spider silk", "Sulphurous ash"],
				//Shapes
				[SHAPE_REAGENT,
				 SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT,
				 SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT,
				 SHAPE_REAGENT, SHAPE_REAGENT],
				//Frames
				[0,
				 0, 1, 2,
				 3, 4, 5,
				 6, 7],
				//Price per unit
				[0,
				 5, 2, 3,
				 5, 1, 1,
				 3, 3],
				 //Quantities
				 [0,
				  1, 1, 1,
				  1, 1, 1,
				  1, 1],
				  //Articles
				 ["",
				  "", "", "",
				  "", "", "",
				  "", ""],
				  //Quantity strings
				 ["",
				  " each", " for one portion", " for one button",
				  " each", " for one clove", " for one portion",
				  " for one portion", " for one portion"],
				  //Quantity tokens
				 ["",
				  "", "", "",
				  "", "", "",
				  "", ""],
				 //Dialog strings
				 [" Dost thou still want it?@",
				  "@How many ", " dost thou want?@",
				  "@Here thou art. Dost thou wish anything else?@",
				  "@Alas, thou canst not carry that much. Wouldst thou be interested in anything else?@",
				  "@Thou dost not have enough gold to buy all that... May I interest thee in anything else?@",
				  "@If thou art sure of that... Dost thou want anything else?@",
				  "@I can understand thy decision. Wilt thou buy anything else?@",
				  "@Let me now if thou dost change thy mind, Avatar. What were we talking about?@"]);
}

void lauriannaHeal (var spellbook) {
	var party = UI_get_party_list();
	var in_party = (item in party);
	if (spellbook) {
		if (!in_party) {
			say("@Let me see what I can do.@");
			/*
			LAURIANNA.hide();
			script LAURIANNA call aiMain;
			abort;
			*/
			serviceHeal();
		} else {
			if (get_schedule_type() == TEND_SHOP) {
				say("@Normally, I would have to charge thee -- I am on duty right now.");
				say("@However, given all that thou hast done for me, I shall heal thee for free.");
			} else {
				say("@Given all that thou hast done for me, the least I can do is heal thee for free.");
			}
			serviceHeal();
		}
	} else {
		say("@I'd love to, but thou hast taken my spellbook.@");
	}
}

void lauriannaPrePotionDialog object#() () {
	// Notice: Laurianna's dialog makes extensive use of the UI_change_npc_face0
	// intrinsic due to her madness. She has FOUR faces all told, which are
	// switched constantly.
	// I prefer to have this single comment here, at the top, than to write
	// comments all over the code saying basically the same thing...
	if (!get_item_flag(MET)) {
		giveExperience(100);

		set_item_flag(MET);

		LAURIANNA->show_npc_face(1);

		say("The woman before your eyes has a rather curled posture, and looks around quickly as if many invisible beings are conspiring to distract her attention.");
		say("Despite the wizard's death, her feet appear to be buried slightly in the ground, and you can swear that there are roots sprouting.");
		say("You presume that this is Laurianna, Zauriel's daughter. As if reading her mind, she speaks just as these thoughts flash across your head.");
		say("@Y-yes, m-my name is Laurianna. Thou hast met my f-father, hast thou not?@");
		say("She gives a sideways glance to the emptiness at her side and quickly brings her arm up, as if trying to defend from an unseen attack.");
		say("Without waiting for a reply, she continues. @Thou seemest to be very familiar to me, ",
			getPoliteTitle(), ".");
		say("@I believe it is of thee that father oft spoke to me -- thou art ",
			getAvatarName(), ", the Avatar, art thou not?@");
		if (askYesNo()) {
			//Honest avatar
			UI_change_npc_face0(0);
			say("A large and welcoming smile appears in her face. @Then it is all about to end.");
			say("@Father says he has a way of banishing these daemons that torment me, and that thou wouldst have to rescue me before he could use it!@");
			giveExperience(50);
		} else {
			//Chide lying avatar
			UI_change_npc_face0(2);
			say("She seems annoyed at you. @Oh, Avatar, I thought Honesty was one of the virtues thou didst have to master!");
			UI_change_npc_face0(0);
			say("@But in any case, I am glad that thou art here. Because now that thou hast rescued me --");
			say("@as father said thou wouldst -- the daemons that torment me can be finally dealt with!@");
		}

		say("When speaking this last sentence, Laurianna whispered the word 'daemons', as if she was trying to avoid drawing their attention.");
		say("@But alas, I cannot go anywhere with thee for I am rooted to the ground!@");

		add(["name", "job", "bye", "daemons", "rooted", "Rescued as father said?", "potion", "necklace"]);
	} else {
		LAURIANNA->show_npc_face(0);
		say("@Hi! What can I do for thee? Anything I can kill? Please?@");
		add(["name", "job", "bye", "potion", "necklace"]);
	}

	var msg;
	var rand_npc;

	converse(0) {
		case "name" (remove):
			UI_change_npc_face0(2);
			say("@I already told thee that! I am still Laurianna, daughter of Zauriel.@");
			UI_change_npc_face0(1);
			say("She looks nervously around as if something had drawn her attention. @No, I must remain calm! I am sorry!@");

		case "job" (remove):
			UI_change_npc_face0(0);
			say("@Right now is to be a little plant in this lovely island!@ she replies with an unusual dose of cheerfulness for someone rooted to the ground.");
			add("plant");

		case "bye" (remove):
			UI_change_npc_face0(3);
			say("She speaks with terrible wrath, in a tone that makes you quiver. @Thou shalt -not- abandon me here! I shall -kill- thee before thou dost!@",
				"~The air crackles full of magical energy as she says that, and you feel terribly cold for a moment.");
			UI_change_npc_face0(2);
			if (UI_is_pc_female()) {
				msg = "her";
			} else {
				msg = "his";
			}

			say("@No!@ she screams, seemingly trying to get a hold of herself. @Bad daemons, thou shalt not force me to take ",
				msg, " life!@");
			UI_change_npc_face0(0);
			say("@I am sorry, ", getPoliteTitle(),
				"! Sometimes these daemons almost get the best of me!@ she ends with a smile.");
			add("daemons");

		case "daemons" (remove):
			UI_change_npc_face0(2);
			say("@Do not speak of them so openly! They are mean!@ she speaks urgently to you, in a half-whispered voice.");
			UI_change_npc_face0(1);
			say("@Father says that they are not real, that they are 'figments of my imagination'.@ she says in even tone.");
			UI_change_npc_face0(0);
			say("@Little does he know...@ she adds in a whisper, giving a sideways glance to empty space.");
			add(["mean", "imagination"]);

		case "rooted" (remove):
			UI_change_npc_face0(0);
			say("@Seest thou not my lovely roots? Look at them! They are so pretty...@ she gives an adoring look to the roots sprouting from her feet.");
			UI_change_npc_face0(2);
			say("@That mean mage made these so I could not escape. It is good that he is dead!@ She glances at the mage's corpse and it immediately explodes in flames.");
			UI_change_npc_face0(1);
			say("Realizing what she did, she curls up in a ball, screaming. @Bad daemons! Look what thou didst make me do!@ You can't help but feel pity.");
			UI_change_npc_face0(0);
			say("After a moment, she rises with a smile and the mage's body stops burning immediately -- and to your astonishment, the flames turn into doves that fly away.");
			rand_npc = randomPartyMember();

			rand_npc.say("@She seems to be as dangerous and insane as Zauriel told us!@, ",
				UI_get_npc_name(rand_npc), " whispers to you.");
			rand_npc.hide();

			add(["mage", "daemons", "pretty roots"]);

		case "Rescued as father said?" (remove):
			UI_change_npc_face0(0);
			say("@Indeed! Thou hast rescued me! My father sees past, present and future, and he told me thou wouldst rescue me from this mage!");
			UI_change_npc_face0(1);
			say("@But alas, I cannot go anywhere. Not since that mage turned me into a plant...");
			add(["plant", "Past, present and future"]);

		case "plant" (remove):
			UI_change_npc_face0(0);
			say("Laurianna beams with uncontained joy. @Yes, I am a plant! A lovely rose! A lovely -red- rose!@");
			UI_change_npc_face0(1);
			say("She shakes her head sadly. @But alas, roses can't talk or walk. Or see. So I won't see my father again...@");
			UI_change_npc_face0(0);
			say("Smiling once again, she adds @Maybe I can change that! I can make all roses walk, talk and see! I then could be a rose and still see my father!@");
			add(["rose", "Change all roses?"]);

		case "mean" (remove):
			UI_change_npc_face0(3);
			say("She screams with uncontrolled rage. @Don't insult them! They can get very angry with thee!@ You can feel the surge in magical energies as she speaks.");
			UI_change_npc_face0(2);
			say("She calms down a little. @Knowest thou nothing about daemons? They can get angry very quickly.@ she adds in a whisper.");

		case "imagination" (remove):
			UI_change_npc_face0(0);
			say("@I have a very good imagination! I used to paint pretty portraits of dead rabbits when I was little!");
			UI_change_npc_face0(1);
			say("@Alas, no -- those were the daemons. I love rabbits!@ she adds quickly.");
			UI_change_npc_face0(2);
			say("In a deep tone, with what can only be termed an 'evil' smile, she adds @Dead rabbits...@");
			add("rabbits");

		case "mage" (remove):
			UI_change_npc_face0(1);
			say("@He was nice. He wanted to take my powers so I would not have to have them anymore.");
			UI_change_npc_face0(2);
			say("With a smirk, she adds @I hope he burns in the Abyss for it...");
			UI_change_npc_face0(0);
			say("@I think he wanted to kill a king with my powers!@ she adds with a most paradoxical smile.");
			add("king", "powers");

		case "pretty roots" (remove):
			UI_change_npc_face0(1);
			say("@Art they not pretty?@ She looks adoringly at the roots. @I think that they will go away if the necklace is destroyed...@");

		case "Past, present and future" (remove):
			UI_change_npc_face0(1);
			say("@He knows everything!@ she intones in a solemn tone, and with an unreadable expression. You can't help but feel that this is not entirely as she claims.");

		case "rose" (remove):
			UI_change_npc_face0(2);
			say("@Beware, for this rose has thorns!@ she speaks menacingly, and then cackles.");
			UI_change_npc_face0(0);
			say("@But still is a very pretty rose!@ she adds smiling.");

		case "Change all roses?" (remove):
			UI_change_npc_face0(1);
			say("@I suppose I could. But then they would not be roses anymore, would they?@");

		case "rabbits" (remove):
			UI_change_npc_face0(0);
			say("@Thou knowest, those cute little furballs that eat carrots.");
			UI_change_npc_face0(2);
			say("@They taste very good too. And are fun to burn!@ she adds smirking.");

		case "king" (remove):
			UI_change_npc_face0(1);
			say("@Oh, thou knowest which king: thy liege, Lord British.@");
			UI_change_npc_face0(0);
			say("She motions you to come closer and whispers something to you. @Between thou and me, I think that he is more than a little mad...@");

		case "powers" (remove):
			UI_change_npc_face0(0);
			say("She gives a broad smile. @I am very powerful! I could kill thee with but a thought if I so wanted!");
			say("@In fact, I am so powerful I can barely control my own powers!@ She speaks with such pride that you could almost believe that this is a good thing...");
			UI_change_npc_face0(1);
			say("@But thou knowest that already, since thou hast spoken to father...@");

		case "necklace" (remove):
			UI_change_npc_face0(3);
			say("@It is mine, -mine- I tell thee! Do not try to touch it!@ As she speaks the sentence, the weather takes a quick turn for the worse. A storm forms almost immediately, and lightning starts to strike nearby.");
			UI_change_npc_face0(0);
			say("Then, almost as soon as it began, the storm dissipates. @Thou mayest admire it, but please leave it alone!@");
			randomPartySay("@Maybe we should give her the potion first, Avatar...@");

		case "potion":
			UI_change_npc_face0(1);
			say("When you mention Zauriel's potion, her expression becomes unreadable. She gives you a serious look, and stays silent for a while.");
			say("She seems to be struggling with some inner daemons, and her face starts to show it. Apparently, she is struggling with the idea of taking the potion.");
			if (!PARTY->count_objects(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY)) {
				//Player doesn't have the potion
				UI_change_npc_face0(0);
				say("With a look of relief, she talks again. @Ah, fortunately thou hast not the potion with thee! Farewell!@");
				script item after 4 ticks say "@Leave me be!@";
				break;
			} else {
				//Player has potion...
				UI_change_npc_face0(1);
				say("Laurianna's face becomes resigned after a while. @Alas, I know that my father wants me to drink it... here, let me have it...@");
				randomPartySay("@Thou dost remember that this potion has Silver Serpent venom in it, dost thou not, Avatar? Art thou sure thou wishest to take the risk?@");
				if (askYesNo()) {
					//And decided to use it
					giveExperience(200);
					//Delete potion
					UI_remove_party_items(1, SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY, true);
					say("She quaffs the potion at once. Her expression becomes unreadable for a while.");
					//Register cure:
					gflags[LAURIANNA_DRANK_POTION] = true;
					script item after 1 ticks call Laurianna;
					clear_item_flag(MET);
					set_schedule_type(WAIT);
					break;
				} else {
					//But is having second thoughts about using it
					AVATAR.say("You think for a moment and decide against giving the potion, as it might be dangerous.");
					AVATAR.hide();
					UI_change_npc_face0(0);
					say("Beaming with joy, Laurianna thanks you. @Thank thee, Avatar, for not making me drink the potion!");
					UI_change_npc_face0(2);
					say("@I don't kill rabbits when I am under its effects...@ she adds in a whispered tone, which makes you wonder if you did the right thing.");
					UI_change_npc_face0(0);
					say("@Farewell!@");
					script item after 4 ticks say "@Leave me be!@";
					break;
				}
			}
	}
}

void lauriannaPostPotionDialog () {
	LAURIANNA->show_npc_face(1);
	//Check for the amulet's presence:
	var has_amulet = count_objects(SHAPE_AMULET, QUALITY_ANY, FRAME_ANY);
	//See if player has blackrock:
	var have_blackrock = PARTY->count_objects(SHAPE_BLACKROCK, QUALITY_ANY, FRAME_ANY);

	//Check to see if Laurianna is in the party or not:
	var party = UI_get_party_list();
	var in_party = (item in party);
	var party_size = UI_get_array_size(party);

	add(["name", "job", "bye"]);
	if (has_amulet) {
		//Amulet is still there...
		add("necklace");
		//Add options if player has blackrock:
		if (have_blackrock) {
			add("Have blackrock");
		} else {
			add("blackrock");
		}
	} else if (!in_party) {
		//Amulet is gone; add options to join or leave:
		add("join");
	} else {
		add("leave");
	}

	if (!get_item_flag(MET)) {
		set_item_flag(MET);
		say("You are astonished by the sudden change in Laurianna's demeanor. While just a moment ago she was constantly looking around and had severe mood swings,");
		say("she now seems to be calm and focused, somewhat apathetic even.");
		say("When she speaks again, she does so in an even, boring tone. @Thank thee for bringing me the potion. I am feeling better now.");
		say("@I would really appreciate if thou couldst remove this necklace; I yearn to see my father, and the necklace prevents it in more than one way.");
		say("@I believe that blackrock could be helpful in this case -- father oft spoke of its Ether-blocking capabilities.");
		say("@But then again, I suppose he didst spoke of it to thee.@");
	} else {
		if (has_amulet) {
			say("@So, wilt thou help me get rid of this necklace?@");
		} else if (!in_party) {
			say("@Wouldst thou take me to my father, please?@");
		} else {
			say("@How can I be of assistance, ", getPoliteTitle(), "?@");
			add("Cast spell", "heal");
		}
	}
	var spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, -get_npc_number(), FRAME_ANY);
	if (!spellbook) {
		// Extra safeguard:
		spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, QUALITY_ANY, 2);
		if (spellbook) {
			spellbook->set_item_quality(-get_npc_number());
		}
	}

	converse(0) {
		case "name" (remove):
			say("@As I believe I told thee before, my name is Laurianna.@");

		case "job" (remove):
			say("@While I have no real job to speak of due to my... condition, I am a mage of no small amount of power -- as thou knowest well.");
			say("@Right now, all I want to do is meet my father again, but I can cast a spell or two if thou needst me to.@");
			add(["Cast spell", "heal", "Meet father"]);

		case "bye" (remove):
			if (!in_party && !has_amulet) {
				in_party = lauriannaAskJoin();
			} else if (!has_amulet) {
				say("@Thou knowest where to find me if thou needst my assistance.@");
			} else {
				say("@If thou findest a way to remove this necklace, please do come talk to me again.@");
			}
			break;

		case "necklace" (remove):
			say("@This necklace is literally rooting me to this spot. While I can talk normally, and even cast spells, I cannot move at all...");
			say("@I have tried in vain to remove it, as it simply reappears around my neck whenever I try it. But I am sure father told thee how to remove it...@");

		case "Have blackrock" (remove):
			giveExperience(50);
			say("@Yes, I can sense it. Here, let me have it for a moment.@ You hand the blackrock to Laurianna.");
			say("She mumbles a barely audible incantation, and to your astonishment, the blackrock immediately becomes malleable.");
			say("Laurianna surrounds the necklace with the blackrock, and easily removes it.");
			say("She then makes a blackrock coating around the necklace, and throws it into the nearby ocean.");
			say("Gathering what is left of the blackrock, she gives it back to you. @There, I am free now!@");
			//Delete the amulet:
			(get_cont_items(SHAPE_AMULET, QUALITY_ANY, FRAME_ANY))->remove_item();
			has_amulet = false;
			in_party = lauriannaAskJoin();
			//Remove Laurianna's roots:
			struct<Position> pos = get_object_position();
			set_item_shape(SHAPE_LAURIANNA);
			set_last_created();
			UI_update_last_created(pos);
			UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, pos.x, pos.y, 0, 0, 0, -1);
			UI_play_sound_effect(67);

		case "blackrock" (remove):
			say("@My father can tell thee more of it, if he hasn't yet.@");

		case "join" (remove):
			if (!has_amulet && (party_size < 8)) {
				say("@Of course, ", getPoliteTitle(), ", I would be honored");
				say("@Thou wilt take me to my father, wilt thou not?@");
				if (askYesNo()) {
					add_to_party();
					add("leave");
					in_party = true;
				} else {
					say("@Then what would be the point of joining thee?@");
					add("join");
				}
			} else if (!has_amulet) {
				say("@Alas, thy party has no room for me... I shall await thee here shouldst thou make room for me.@");
				add("join");
			} else {
				say("@I would gladly join thee, but this necklace prevents me from moving from this spot.@");
				add("join");
			}

		case "leave" (remove):
			if (get_distance(ZAURIEL) > 20) {
				//If she is too far from Zauriel, refuse to leave:
				say("@But my father is nowhere near this place! Thou didst promise to take me to him, and I shall remain in thy group until thou dost do so.@");
			} else {
				//Thank Avatar from taking her to her father:
				say("@Thou hast indeed brought me to my father. Thank thee, Avatar!@");

				//Teleport Laurianna to her father.
				remove_from_party();
				set_schedule_type(WAIT);
				var eggs = ZAURIEL->find_nearby(SHAPE_EGG, 20, MASK_EGG);
				struct<Position> pos;
				for (egg in eggs) {
					if (egg->get_item_quality() == -1 * get_npc_number()) {
						pos = egg->get_object_position();
						break;
					}
				}
				move_object(pos);
				UI_sprite_effect(ANIMATION_TELEPORT, pos.x, pos.y, 0, 0, 0, -1);
				AVATAR->trueFreeze();

				script item {
					call trueFreeze;
					sfx SOUND_TELEPORT;
					wait 4;
					face south;
					actor frame standing;
					say "@Father!@";
					wait 4;
				}

				script ZAURIEL {
					call trueFreeze;
					sfx SOUND_TELEPORT;
					wait 8;
					face north;
					say "@My beloved child!@";
					wait 8;
					call Zauriel;
				}
				break;
			}

		case "Cast spell" :
			if (spellbook) {
				event = DOUBLECLICK;
				spellbook->spellitem_Spellbook();
			} else {
				say("@I'd love to, but thou hast taken away my spellbook.@");
			}
		case "heal" :
			lauriannaHeal(spellbook);

		case "Meet father" (remove):
			say("@Not only do I miss him terribly, but also he found a cure to my condition. He was about to use it when I was kidnapped...");
			say("@Indeed, father even said that I would be kidnapped, and that it was thee the one who would rescue me.");
			say("@He also told me that this ordeal was necessary and unavoidable... from what I read -- against his permission, mind thee -- on his journal,");
			say("@I am afraid that he was indeed correct.@");
			add("Kidnap necessary", "Knew I would rescue thee?", "journal");

		case "Kidnap necessary" (remove):
			say("@According to my father, thy help is demanded by fate if he is to cure me. I know not exactly why, for he did not tell me...");
			say("@I do know that father had visions of the future once, and that he saw this particular future we are experiencing right now.");
			say("@While he -did- say that there were other possibilities, he also said that I would be cured only in this one...@");

		case "Knew I would rescue thee?" (remove):
			say("@Father had some visions of the future a few years ago. Among the several possibilities he saw, he decided that this would be the best course of action -- namely, letting me be kidnapped and enlisting thine aid to rescue me.");
			say("@My father is a very proud man, but he generally knows what he is talking about. I suppose that he is right about this too.@");

		case "journal" (remove):
			say("@My father keeps a detailed journal of his life. But his life is not all contained in his journal...");
			say("@He did have visions of what was then to be the future and wrote them down on the journal.");
			say("@They were too long and detailed, so I did not read them all... but many where about thee and thy companions.");
			say("@For example, it was written that thou wouldst discover that Christopher, the Trinsic blacksmith, had been murdered by a man with a hook.");
			say("@I realize that this would make father an accessory to murder, since he did nothing to prevent it -- but he knew also that this murder was for the greater good in the long run, for it would start thee in thy quest.@");
			add("My quest?");

		case "My quest?" (remove):
			if (gflags[DELIVERED_NOTEBOOK_TO_WISPS]) {
				say("@Alas, I know not what it is... I never had the chance to read that part myself...@");
			} else {
				say("@I did try to read about it, but my father never wrote what he saw about that...@");
			}
	}
}

void lauriannaPostQuestDialog () {
	var msg;

	LAURIANNA->show_npc_face(0);
	say("Laurianna seems different than she was a few moments ago -- she acts as if a great burden has been lifted from her shoulders.");
	say("She seems happier than before, but even then, a great sorrow is evident -- for the loss of her father, you guess.");
	say("But despite all that, you have the feeling that she feels... lost. It is evident that this is all new to her, and she isn't entirely sure how to proceed.");
	say("It is then that she talks to you. @It is... strange. I am not entirely sure what happened, but I am certain that I no longer am who I was.");
	say("@I am terribly sorry for my father's death, but he gave me a gift I can scarcely understand yet. It is a pity that he had to die...");
	say("@But thou hast not to worry, Avatar, for I bear thee no ill will. It is true that thou didst slay my father, @but thou wouldst have been slain hadst thou hesitated.");
	say("@I shall not seek revenge. I will try to move on, and remember all that my father did for me. I believe that this is what he would have wanted...@");

	add(["name", "job", "No longer who thou wert?", "disagrees", "No ill will", "slain"]);

	if (PARTY->count_objects(SHAPE_JOURNAL, QUALITY_ANY, FRAME_ANY)) {
		add(["journal", "Give journal"]);
	} else if (gflags[READ_ZAURIEL_JOURNAL]) {
		add("journal");
	}
	converse(0) {
		case "name" (remove):
			say("@This, I am afraid, is one of the few things I yet know about myself... I am Laurianna.");
			say("@It comforts me to know that, but I yet have a long journey of self-discovery ahead...@");
			add("Self-discovery");

		case "job" (remove):
			say("@I suppose that -- for a while, at least -- my 'job' will be to discover who I am.@");
			add("Self-discovery");

		case "bye" (remove):
			say("@I think I shall move to Yew. I always loved trees. Who knows, maybe the monks in Empath Abbey can help me find myself...");
			say("@If thou hast ever the need of my services, or simply want to talk to me, come find me there.");
			lauriannaGiveKeyring();
			say("@Farewell, ", getAvatarName(), "!@");
			break;

		case "No longer who thou wert?" (remove):
			say("@There are many things I did in the past which I find... unsettling. I cannot imagine myself doing any of these again...");
			say("@I suppose that this means I am no longer insane. Or, at least, not as insane as I was...@");
			add("Not as insane?");

		case "Not as insane?" (remove):
			say("@Of course! Who in his right mind can say for sure whether she is sane or insane? I can say that I no longer am who I was,");
			say("@and that is a huge improvement; but I cannot make claims of being sane since I never knew sanity in my life.@");

		case "Self-discovery" (remove):
			say("@My whole life seems to be the memories of a total stranger. The things I did, the things I wanted to do...");
			say("@I simply don't know who I am any longer. I shall have to meditate on that...@");

		case "disagrees" (remove):
			if (!gflags[READ_ZAURIEL_JOURNAL]) {
				say("@My father had been consumed by the idea that he had to atone for his past misdeeds.");
				if (!gflags[BROKE_TETRAHEDRON]) {
					msg = "should";
				} else {
					msg = "now that";
				}
				say("@And he was terribly afraid of what he might do ", msg,
					" the Ether be restored...@");
				add(["Past misdeeds?", "Might do?"]);
			} else {
				say("@Thou hast read my father's journal already. I believe that thou knowest that already...@");
				add("journal");
			}

		case "No ill will" (remove):
			say("@I know that my father gave thee little choice in the matter -- hence, I shall not seek any form of revenge or compensation.");
			say("@Besides, given the spells that he had woven, he would have taken his own life had thou not killed him...");
			say("@Thou seest, he used his own life force to repair my mind. He had to die to free his life force, for otherwise the spell would not take effect.");
			if (gflags[READ_ZAURIEL_JOURNAL]) {
				say("@But thou hast already read about that in his journal...");
			}
			say("@But the foremost reason I bear thee no ill will is the single fact that thou hast saved Britannia from -me-.");
			say("@My father never truly understood my former madness. He -did- know that I did not want to control my powers.");
			say("@But he believed that my powers were too great for me to control, and that is where he was mistaken.");
			say("@Simply put, he underestimated the longing I had to let my powers run wild...");
			say("@When I was under the effects of the potion, the Blackrock flowing in my veins nullified my powers partially, but I still yearned to free them.");
			say("@And I did gave in to that desire, more than once -- but my powers were weakened by the Blackrock and caused no harm...@");

		case "slain" (remove):
			say("@Father wanted to cure me as much as he wanted to be remembered. Killing the Avatar would be one good way to be remembered...");
			say("@So yes, he would have slain thee...@");

		case "Past misdeeds?" (remove):
			say("@Before he was affected by the damaged Ether, he was quite insane.",
				"~@Worse, he was in an active quest to wrest Britannia from the hands of Lord British.");
			say("@There is no telling how many lives he destroyed in that period... especially since the first part of his journal has long been destroyed.@");
			add("journal");

		case "Might do?" (remove):
			say("@Father was insane before being affected by the damaged Ether. It is perhaps strange that he became sane because of it...");
			say("@He had a powerful fear of reverting to his old insanity -- he was afraid of all the destruction he might cause.");
			say("@Hence, he decided that the best thing for Britannia was to end his life somehow. And that is were you stepped in...@");

		case "journal" (remove):
			say("@Father kept a journal detailing his life... thou didst see it upon the ground after he fell in battle.");
			if (gflags[READ_ZAURIEL_JOURNAL]) {
				say("@Since thou hast read the journal already, I would like to keep it to remember him by.");
				lauriannaAskJournal();
			} else {
				say("@I would recommend thee to read his journal -- it might help thee understand father's deeds.");
				say("@Wishest thou to read father's journal?@");
				if (askYesNo()) {
					say("@Very well. I ask only that thou givest it to me when thou hast finished reading it.@");
				} else {
					lauriannaAskJournal();
				}
			}
			add("bye");
	}

	//Teleport Laurianna to Reyna's house:
	move_object([0x224, 0x628, 0x0]);
	UI_sprite_effect(ANIMATION_TELEPORT, 0x224, 0x628, 0, 0, 0, -1);

	set_new_schedules([DAWN,			MORNING,		AFTERNOON,		EVENING,		NIGHT],
					  [TEND_SHOP,		MAJOR_SIT,		EAT,			WANDER,			SLEEP],
					  [264, 406,		255, 308,		249, 405,		734, 127,		259, 405]);

	clear_item_flag(MET);

	//These flags acquire a new meaning after Laurianna moves to Yew:
	gflags[LAURIANNA_DRANK_POTION] = false;
	gflags[RECEIVED_ZAURIEL_REWARD] = false;
	gflags[LAURIANNA_CURED] = false;
	gflags[READ_ZAURIEL_JOURNAL] = false;

	script item {
		call trueFreeze;
		sfx SOUND_TELEPORT;
		wait 4;
		say "@Farewell!@";
		wait 4;
		call Laurianna, EVENT_TELEPORT;
	}
}

void lauriannaYewDialog () {
	LAURIANNA->show_npc_face(0);

	var sched = get_schedule_type();

	if ((UI_get_timer(LAURIANNA_TIMER) > 72) && !gflags[LAURIANNA_READY]) {
		gflags[LAURIANNA_READY] = true;
		//Calculate the average exp of all party members
		var totalexp;
		var party = UI_get_party_list();
		for (npc in party) {
			totalexp += npc->get_npc_prop(EXPERIENCE);
		}
		totalexp /= UI_get_array_size(party);
		set_npc_prop(EXPERIENCE, totalexp - get_npc_prop(EXPERIENCE));

		var leather_armor = [
			SHAPE_LEATHER_HELM, SHAPE_LEATHER_ARMOR, SHAPE_LEATHER_COLLAR,
			SHAPE_LEATHER_GLOVES, SHAPE_LEATHER_LEGGINGS, SHAPE_LEATHER_BOOTS
		];
		var pouch_content = [
			SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT,
			SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT, SHAPE_REAGENT
		];
		var pouch_frames = [
			0, 1, 2, 3,
			4, 5, 6, 7
		];
		var pouch_quantities = [
			20, 20, 20, 20,
			20, 20, 20, 20
		];
		var pouch_qualities = [
			0, 0, 0, 0,
			0, 0, 0, 0
		];
		monsterEquipment(item,
						 SHAPE_PORTABLE_HOLE,
						 [SHAPE_LIGHTNING_WAND, leather_armor],
						 [100, 0, 0, 0, 0, 0, 0],
						 pouch_content, pouch_frames, pouch_quantities, pouch_qualities);
	}

	var in_party = (item in UI_get_party_list());

	if (!get_item_flag(MET)) {
		say("Laurianna seems a lot better than when you last saw her -- she seems to be happy now.");
		say("@Avatar! It is good to see thee again.@ she says.");

		if (sched == SLEEP) {
			say("@I am afraid that only thy timing is bad... I am trying to sleep now. Come again in the morning and I will gladly talk to thee!@");
			item_say("@Good night!@");
			run_schedule();
			return;
		}

		giveExperience(50);
		say("@Life here in Yew has been good for me; there are many nice people around here!");
		say("@I now have a job helping Reyna, the local healer. I am also being instructed by Perrin, the local scholar, about a great many subjects.");
		say("@I am also teaching -him- about magic in general. He is even considering writing a treatise on it...@");
		set_item_flag(MET);

		if (gflags[LAURIANNA_HAS_JOURNAL]) {
			UI_set_timer(LAURIANNA_TIMER);
		}
		add(["name", "job", "bye", "Reyna", "Perrin", "heal"]);
	} else {
		add(["name", "job", "bye", "heal"]);

		if (gflags[LAURIANNA_READY]) {
			if (gflags[LAURIANNA_WILL_JOIN]) {
				say("@Hello again, Avatar. What can I do for thee?@");
				if (!in_party) {
					add(["spells", "reagents", "join"]);
				} else {
					if (gflags[BROKE_TETRAHEDRON]) {
						add("Cast spell");
					}
					add("leave");
				}
			} else {
				say("@Avatar! As usual, thy sense of timing is impeccable! I was just thinking about thee!");
				say("@While I have learned a great deal during my time in Yew, I think there is much to learn elsewhere.");
				say("@Thou wouldst make a perfect traveling companion, since thou dost help people all over Britannia.");
				say("@I can be of great assistance to thee, if thou wilt have me in thy party.");
				if (!gflags[BROKE_TETRAHEDRON]) {
					say("@I am a mage of no small amount of power, and should the Ether be fixed, all that power will be a great help in thy quest!");
				} else {
					say("@I am a mage of no small amount of power, and all that power will be a great help in thy quest!");
				}
				say("@So, wilt thou have me in thy party?@");
				if (askYesNo()) {
					add_to_party();
					in_party = true;
					say("@I promise thee that thou shalt not be disappointed! Now, can I do anything for thee?@");
					if (gflags[BROKE_TETRAHEDRON]) {
						add("Cast spell");
					}
					add("leave");
				} else {
					say("@Shouldst thou change thy mind, I will be waiting for thee here. What can I do for thee?@");
					add("join");
					add(["reagents", "spells"]);
				}
				gflags[LAURIANNA_WILL_JOIN] = true;
			}
		} else {
			say("@Hello again, Avatar. What can I do for thee?@");
		}
	}

	if (!in_party && (sched == WANDER)) {
		add("Shrine of Justice");
	}

	if (PARTY->count_objects(SHAPE_JOURNAL, QUALITY_ANY, FRAME_ANY) &&
			!gflags[LAURIANNA_HAS_JOURNAL]) {
		add("Give journal");
	}

	var spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, -get_npc_number(), FRAME_ANY);
	if (!spellbook) {
		// Extra safeguard:
		spellbook = get_cont_items(SHAPE_SPELL_SPELLBOOK, QUALITY_ANY, 2);
		if (spellbook) {
			spellbook->set_item_quality(-get_npc_number());
		}
	}

	converse (0) {
		case "name" (remove):
			say("@Surely thou must be joking... My name is still Laurianna, as thou knowest well.");
			say("@Dost thou never tire of asking questions whose answers thou knowest already?@");

		case "job" (remove):
			if (item in UI_get_party_list()) {
				say("@One would think that travelling and fighting by thy side would be enough to divine my present occupation, Avatar.@");
			} else if (get_schedule_type() == WAIT) {
				say("@I am awaiting until thou asketh me to return to thy party.@");
			} else {
				say("@I am working as a healer in Reyna's shop. And I can sell thee some reagents.@");
				add("reagents");
			}

		case "Reyna" (remove):
			say("@She is the local healer, and is a very nice lady. She has given me lodging in her house in exchange for help running her shop.@");
			if (isNearby(REYNA)) {
				REYNA.say("@And she is doing a very fine job at that -- she is a natural healer!@");
				REYNA.hide();
			}

		case "Perrin" (remove):
			say("@He is a scholar, and a very wise one at that. I have been learning about a lot of things with him.@");
			if (isNearby(PERRIN)) {
				PERRIN.say("@Laurianna is an excellent student -- I fear that she will learn all I know in no time!");
				say("@But she is also not the only one learning here -- I've already learned a great deal about magic from her.@");
				PERRIN.hide();
			}

		case "heal":
			lauriannaHeal(spellbook);

		case "reagents":
			if (PARTY->count_objects(SHAPE_SPELLBOOK, QUALITY_ANY, FRAME_ANY)) {
				say("@Wishest thou to buy some reagents?@");
				if (askYesNo()) {
					lauriannaSellReagents();
				} else {
					say("@Oh. Never mind, then.@");
				}
			} else {
				say("@But thou hast not a spellbook, Avatar! What wouldst thou use reagents for?@");
			}

		case "Give journal" (remove):
			UI_remove_party_items(1, SHAPE_JOURNAL, QUALITY_ANY, FRAME_ANY);
			gflags[LAURIANNA_HAS_JOURNAL] = true;
			UI_set_timer(LAURIANNA_TIMER);
			lauriannaGiveKeyring();

		case "Shrine of Justice" (remove):
			say("@One of the things I am doing with my life is meditate at the shrines of the Virtues.");
			say("@I have started with the Shrine of Justice because it is nearest to where I live now.");
			say("@In the future, I hope to have visited them all, and learned as much as I can about the Eight Virtues.@");

		case "join" (remove):
			add_to_party();
			in_party = true;
			say("@I promise thee that thou shalt not be disappointed! Now, can I do anything for thee?@");
			if (gflags[BROKE_TETRAHEDRON]) {
				add("Cast spell");
			}
			add("leave");
			remove(["reagents", "spells"]);
			clear_item_flag(SI_TOURNAMENT);

		case "leave" (remove):
			say("@If that is thy wish... should I wait here or should I go home?@");
			remove_from_party();
			if (chooseFromMenu2(["wait here", "go home"]) == 1) {
				say("@Very well, I shall await your return.@");
				set_schedule_type(WAIT);
			} else {
				say("@If thou dost insist... I shall return to Yew, then.@");
				run_schedule();
			}
			set_item_flag(SI_TOURNAMENT);
			abort;

		case "Cast spell":
			if (spellbook) {
				event = DOUBLECLICK;
				spellbook->spellitem_Spellbook();
			} else {
				say("@I'd love to, but thou hast taken my spellbook.@");
			}

		case "bye":
			if (!in_party) {
				say("@Farewell, ", getPoliteTitle(),
					". I hope I'll see thee again soon!@");
			} else {
				say("@If thou hast need of anything, ", getPoliteTitle(),
					", let me know.@");
			}
			break;
	}
	item_say("@Goodbye!@");
}
