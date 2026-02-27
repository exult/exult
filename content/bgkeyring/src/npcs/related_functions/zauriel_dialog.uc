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
 *	Specifically, it contains dialog functions for Zauriel.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void zaurielSellPotions () {
	sellItems(
				//Names
				["nothing",
				 "sleep potion", "healing potion", "curative potion",
				 "vial of poison", "awakening potion", "protection potion",
				 "illumination potion", "invisibility potion"],
				//Shapes
				[0,
				 SHAPE_POTION, SHAPE_POTION, SHAPE_POTION,
				 SHAPE_POTION, SHAPE_POTION, SHAPE_POTION,
				 SHAPE_POTION, SHAPE_POTION],
				//Frames
				[0,
				 0, 1, 2,
				 3, 4, 5,
				 6, 7],
				//Price per unit
				[0,
				 20, 160, 160,
				 20, 10, 10,
				 40, 80],
				 //Quantities
				 [0,
				  1, 1, 1,
				  1, 1, 1,
				  1, 1],
				  //Articles
				 ["",
				  "A ", "A ", "A ",
				  "A ", "An ", "A ",
				  "An ", "An "],
				  //Quantity strings
				 ["",
				  " per vial", " per vial", " per vial",
				  " each", " per vial", " per vial",
				  " per vial", " per vial"],
				  //Quantity tokens
				 ["",
				  "", "", "",
				  "", "", "",
				  "", ""],
				 //Dialog strings
				 [" Dost thou still want it?@",
				  "@How many ", "dost thou want?@",
				  "@Done! What else dost thou want?@",
				  "@If thou couldst carry that much, I would sell it to thee. Anything else?@",
				  "@Thou dost not have sufficient funds. Earn some gold and then return. Anything else?@",
				  "@Fine by me. Dost thou wish anything else?@",
				  "@Fair enough. Anything else?@",
				  "@I have no problems with that. Where were we?@"]);
}

void zaurielSellReagents () {
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
				 6, 1, 5,
				 7, 1, 1,
				 3, 2],
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
				  "@Done! What else dost thou want?@",
				  "@If thou couldst carry that much, I would sell it to thee. Anything else?@",
				  "@Thou dost not have sufficient funds. Earn some gold and then return. Anything else?@",
				  "@Fine by me. Dost thou wish anything else?@",
				  "@Fair enough. Anything else?@",
				  "@I have no problems with that. Where were we?@"]);
}

void zaurielGiveAdvance () {
	var advance_reagent = [SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT,SHAPE_REAGENT];
	var advance_potions = [SHAPE_POTION,SHAPE_POTION,SHAPE_POTION,SHAPE_POTION,SHAPE_POTION,SHAPE_POTION,SHAPE_POTION,SHAPE_POTION];
	var advance_item_frames =	[0, 1, 2, 3, 4, 5, 6, 7];
	var advance_reagent_quantity = [20, 20, 20, 20, 20, 20, 20, 20];
	var advance_potions_quantity = [1, 4, 4, 0, 2, 2, 1, 2];
	var advance_quality = [0, 0, 0, 0, 0, 0, 0, 0];

	var pouch1 = createContainerWithObjects(SHAPE_BAG, advance_reagent, advance_item_frames, advance_reagent_quantity, advance_quality);
	var gavereags = forceGiveObjToParty(pouch1);
	var pouch2 = createContainerWithObjects(SHAPE_BAG, advance_potions, advance_item_frames, advance_potions_quantity, advance_quality);
	var gavepotns = forceGiveObjToParty(pouch2);

	say("@In any case, since thou wilt help me, I shall now give thee a selection of reagents and potions to assist thee.");
	say("@This is an advance payment, but it shall not be deducted from thy upcoming reward. Consider it as a bonus...@");


	if (gavereags && gavepotns) {
		say("@There is the advance. I have placed it in a couple of bags for thy convenience.@");
	} else {
		if (!gavereags) {
			pouch1->set_last_created();
		}
		UI_update_last_created(AVATAR->get_object_position());

		if (!gavepotns) {
			pouch2->set_last_created();
		}
		UI_update_last_created(AVATAR->get_object_position());

		var msg = "it";
		if ((!gavereags) && (!gavepotns)) {
			msg = "them";
		}

		say("@I placed the advance in these bags. Since thee and thy party are too encumbered, I shall place ",
			msg, " on the ground.@");
	}
}

void zaurielCreateComponents (var quantities) {
	var cont = createContainerWithObjects(SHAPE_CHEST,
										  [SHAPE_SPIDER_EGG, SHAPE_BEE_STINGER, SHAPE_INVISIBILITY_DUST],
										  [0,				0,					0],
										  quantities,
										  [0,				0,					0]);
	cont->set_last_created();
	ZAURIEL->give_last_created();
}

void zaurielDestroyComponents () {
	var shapes = [SHAPE_SPIDER_EGG, SHAPE_BEE_STINGER, SHAPE_INVISIBILITY_DUST];
	var pouch = ZAURIEL->get_cont_items(SHAPE_CHEST, QUALITY_ANY, FRAME_ANY);

	for (shp in shapes) {
		var cnt = ZAURIEL->count_objects(shp, QUALITY_ANY, FRAME_ANY);
		pouch->remove_cont_items(cnt, shp, QUALITY_ANY, FRAME_ANY, true);
	}
	pouch->remove_item();
}

void zaurielMakeGem(var make) {
	remove(["Make gem", "Fix gem"]);
	say("@Yes, thou hast everything I asked thee. Here, give me the components.@ Zauriel takes the components from you.");

	// What we need to take from party.
	var spider_eggs = 2;
	var bee_stringer = 6;
	var invis_dust = 1;
	var gems = 1;

	// Check which frame of gem the party has, but make sure
	// not to include FoV gems here.
	var framenum = 0;
	do {
		if (PARTY->count_objects(SHAPE_GEM, QUALITY_ANY, framenum)) {
			break;
		}
		framenum += 1;
	} while (framenum < 12);

	if (!make) {
		// Since we are fixing a gem, we don't need to take everything.
		spider_eggs -= count_objects(SHAPE_SPIDER_EGG, QUALITY_ANY, FRAME_ANY);
		bee_stringer -= count_objects(SHAPE_BEE_STINGER, QUALITY_ANY, FRAME_ANY);
		invis_dust -= count_objects(SHAPE_INVISIBILITY_DUST, QUALITY_ANY, FRAME_ANY);
		gems -= count_objects(SHAPE_GEM, QUALITY_ANY, FRAME_ANY);
	}

	// Remove what is needed to fix the gem
	if (spider_eggs > 0) {
		UI_remove_party_items(spider_eggs, SHAPE_SPIDER_EGG, QUALITY_ANY, FRAME_ANY, true);
	}
	if (bee_stringer > 0) {
		UI_remove_party_items(bee_stringer, SHAPE_BEE_STINGER, QUALITY_ANY, FRAME_ANY, true);
	}
	if (invis_dust > 0) {
		UI_remove_party_items(invis_dust, SHAPE_INVISIBILITY_DUST, QUALITY_ANY, FRAME_ANY, true);
	}
	if (gems > 0) {
		UI_remove_party_items(gems, SHAPE_GEM, QUALITY_ANY, framenum, true);
	}

	// Destroy any components Zauriel may still have:
	zaurielDestroyComponents();
	giveExperience(100);

	var incantation;
	var gemdone;
	if (make) {
		incantation = "@In Lor Wis Ort Rel Por!@~@Vas An Ort Sanct!@";
		gemdone = "settles into a glowing stone.";
	} else {
		incantation = "@In Bet Ort Rel Vas Ort!@";
		gemdone = "has a subtler, softer glow than before.";
	}
	say("He then quickly intones a magical incantation.~", incantation);
	say("After a flash of light, the gathered components vanish and the gem ",
		gemdone);
	say("@Here is the gem. Make good use of it.@");

	// Give gem to avatar:
	UI_set_last_created(get_cont_items(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY));
	if (!AVATAR->give_last_created()) {
		say("@Since thou art so overburdened, I shall place the gem on the ground.@");
		UI_update_last_created(AVATAR->get_object_position());
	}
}

var zaurielExplainQuest(var doing_quest) {
	var msg;

	UI_push_answers();
	add(["events", "nothing else"]);

	converse(0) {
		case "events" (remove):
			say("@There are three events that, when combined, yield the present threat: My daughter's enormous innate power, her insanity and her kidnapping.",
				"~@Of which of these wishest thou to hear first?@");
			add(["daughter?", "power", "insanity", "kidnap"]);

		case "daughter?" (remove):
			say("@Yes, I do have a daughter, and her name is Laurianna. Why is it so strange?@");

		case "power" (remove):
			if (gflags[BROKE_TETRAHEDRON]) {
				msg = "had been plaguing";
			} else {
				msg = "plague";
			}
			say("@Laurianna's power is enormous -- being the result of having two powerful mages as parents,");
			say("@as well as being born under the influence of the damaged Ether waves that ",
				msg, " the land.");
			say("@If that were not enough, her powers have grown overmuch as she aged, and now they have far outstripped her ability to control them.");
			say("@She is trying to control her powers for the moment, but she will ultimately fail.");
			say("@And when she does, the result will most likely be massive destruction in a widespread area.");
			say("@It has happened before, and I was able to contain the destruction only through specially designed arrangements of Blackrock.");
			say("@Fortunately, I was able to create a potion that reduces her powers to a manageable level.@");
			add(["Ether", "potion", "blackrock", "Massive destruction"]);

		case "insanity" (remove):
			say("The look on Zauriel's face makes it obvious to you that it is a sore subject, of which he only grudgingly speaks.");
			say("@My daughter was born under the influence of the damaged Ether waves that caused many a mage to go insane,");
			say("@as well as hampering the use of magic throughout Britannia.");
			say("@Given that she was mageborn, the damaged Ether hath affected her throughout her life.");
			if (gflags[BROKE_TETRAHEDRON]) {
				msg = "@It is sad that she has never been sane, otherwise she would be cured of her insanity now that thou hast repaired the Ether.";
			} else {
				msg = "@And since she was never sane, I doubt that she would be cured even if someone like thee wert to repair the Ether.";
			}

			say(msg);
			say("@But one of the most dangerous aspects of her insanity is that she simply does not -want- to control her powers... she simply wants to let go.");
			say("@In the past, she required constant supervision to keep her powers in check. Lately, I developed a potion that suppresses her insanity temporarily.");
			say("@In any event, I was able to develop a cure for her insanity. I would have cured her already,");
			say("@but her kidnap happened at the precise moment I finished all preparations.@");
			add(["Ether", "potion", "cure", "Other aspects?"]);

		case "kidnap" (remove):
			say("@A band of thugs, led by a powerful mage, attacked me while I was distracted.");
			say("@Under normal conditions, they would be nothing but smoldering corpses lying upon the ground.");
			say("@Unfortunately, I was weakened from the preparations I had made to cure my daughter's insanity, and fell prey to them, much to my shame.");
			say("@I was able to divine the mage's purpose through magic: he wants to steal her power for himself.");
			say("@It is imperative for this to be prevented at all costs.@");
			add(["mage", "thugs", "cure", "steal", "prevented", "Why not do it thyself?"]);


		case "Ether" (remove):
			say("@Thou hast seen the most obvious effects of the damaged Ether: insane mages and difficult spellcasting.");
			say("@None are so dramatic as my daughter's plight, however.");
			say("@Due to a strange chance, I seem to be immune to the effects of the damaged ethereal waves, which is good. I have been unable to determine why,");
			say("@otherwise I would have used such knowledge to help Laurianna. And maybe the other mages too.@");
			if (gflags[BROKE_TETRAHEDRON]) {
				say("@But in any case, thou hast done mages everywhere a great favor by destroying that tetrahedron generator.@");
			}

		case "potion" (remove):
			say("@It is a specially designed potion I crafted through complicated alchemical processes thou knowest nothing about.");
			say("@It suffices to say that it has two effects: it suppresses my daughter's insanity and reduces her powers to a manageable level.");
			say("@The potion was tailor made for her. It has such potency that it is deadly poison to anyone else who imbibes it.");
			say("@I fear that not even thy liege, Lord British, would survive the effects of the potion, but it will not be me the one who attempts such a foul deed.");
			say("@When thou hast rescued my daughter, it is imperative that she drinks a vial of the potion lest the effects of the previous dose fade.");
			say("@But alas, all the vials I had where destroyed when I was attacked...@");
			add(["poison", "destroyed"]);

		case "blackrock" (remove):
			say("@Blackrock is a most wonderful substance that is malleable only by powerful magics.");
			say("@It has also the interesting property of blocking Ether waves.");
			if (gflags[0xE7]) {
				say("@Thou hast already talked to Rudyom, so I tend to think that thou knowest what Blackrock is.");
			}
			say("@Using a specially designed arrangement of Blackrock, I was able to create a zone of anti-resonant Ether waves.");
			say("@These waves had just the right properties so that they can nullify the destruction caused by my daughter's powers.@");

		case "Massive destruction" (remove):
			say("@It suffices to say that -everything- within a certain distance will be turned to dust. I know not how wide the region is nowadays,");
			say("@but it was already very wide when Laurianna was still a baby...@");

		case "cure" (remove):
			say("@The details are too technical to explain, but I am confident it will work.");
			say("@The only problem is that I must be -near- her when I cast the final spells...");
			say("@I suppose it could be adjusted to cure other mages as well, but they do not pose such a threat as Laurianna does.");
			say("@Besides, it is my belief that all other mages shall be cured shouldst thou manage to repair the Ether.@");

		case "Other aspects?" (remove):
			say("Zauriel glares at you with anger and speaks with an icy cold tone. @None of which I feel like sharing, 'Avatar'@.");
			if (UI_is_pc_female()) {
				msg = "she";
			} else {
				msg = "he";
			}
			say("Zauriel's face softens, and he speaks with great sadness. @I apologize, Avatar. Here I am, lashing out at ",
				msg, " whose aid I am trying to enlist...");
			say("@Dost thou not realize how sore of subject this is? I would prefer to avoid speaking about it if I can help it...@");

		case "mage" (remove):
			say("@I know not who he is. He is smart and devious, I give thee that...@");

		case "thugs" (remove):
			say("@Like all evil mages, he had a set of servants. His were a cyclops, a troll, a winged gargoyle and a human fighter.");
			say("@I estimate that the gargoyle and the human are the most dangerous of his goons... but I would not underestimate the other two.@");

		case "steal" (remove):
			say("@The mage has been seduced by the promise of quick power. He has divined the power my daughter wields, and lusts for it.");
			say("@I suppose that he is capable of stealing her powers, but I don't think any good would come out of it. Far from it...");
			say("@This particular mage has been hit particularly hard by the damaged Ether waves.");
			say("@I have no doubt that he would try to kill Lord British should he manage to steal my daughter's powers.");
			say("@And I have no doubt that he would succeed, should he manage to -control- the powers for long enough...@");

		case "prevented" (remove):
			say("@The mage did not have to deal with my daughter's powers throughout his entire life, as she did. The power is simply too much for him.");
			say("@My divinations indicate that he would lose control in a matter of minutes, and the uncontrolled energies would create a terrible cataclysm in Britannia.");
			say("@Think of the Armageddon spell for a quick reference... although I fear even the planet itself would not survive...@");

		case "Why not do it thyself?" (remove):
			say("@While it is true that I am a powerful mage, the situation is stacked against my interference; the mage I told you about is no fool.");
			say("@Through magic, I have learned that there is a magical trap set which will be triggered should I come too close to my daughter.");
			say("@And while I could probably survive any traps that the mage could make, this particular trap is an explosive necklace that will kill my -daughter-, not me.@");
			add("necklace");

		case "necklace" (remove):
			say("@The necklace was enchanted by the kidnapper himself, and cannot be removed by ordinary means.");
			say("@Fortunately, Blackrock can be used to disable the necklace if thou hast enough of it. All that is needed is to place the necklace inside the Blackrock.@");

		case "poison" (remove):
			say("@I understand thy concern, but it is unfounded. The potion was tailor made for my daughter,");
			say("@which makes her immune to the deleterious effects of the concoction.@");

		case "destroyed" (remove):
			say("@It is no real reason for worry, as I can make more. But alas, I don't have the two main ingredients.");
			say("@And since I have been waiting for thee here, I haven't been able to gather them.");
			say("@Therefore, thy first mission, shouldst thou decide to help me, will be to gather these ingredients.@");
			add("ingredients");

		case "ingredients" (remove):
			say("@When thou hast decided to help me, I shall tell thee what is needed. But not before.@");

		case "nothing else" (remove):
			if (!doing_quest) {
				say("@So, wilt thou perform this task for me? Wilt thou rescue my daughter?@");
				if (askYesNo()) {
					doing_quest = true;
					say("@Splendid! I shall tell thee what I need to make the potion thou wilt need.@");
					add("Tell about potion");
				} else {
					break;
				}
			} else {
				say("@Anything else you want to know?@");
				break;
			}

		case "Tell about potion" (remove):
			say("@Yes, yes, if thou couldst have but waited for a while longer, thou wouldst realize that I was just about to say that!");
			break;
	}

	UI_pop_answers();
	return doing_quest;
}

void zaurielTellAboutPotion () {
	var blackrock_count = PARTY->count_objects(SHAPE_BLACKROCK, QUALITY_ANY, FRAME_ANY);
	var venom_count = PARTY->count_objects(SHAPE_VENOM, QUALITY_ANY, FRAME_ANY);
	var gave_potion = (!count_objects(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY));

	say("@Thou wilt need my special potion for Laurianna and a piece of Blackrock ore to neutralize the necklace.");
	if (!gave_potion) {
		say("@I will have to make the potion; hence I beseech thee to help me gather the ingredients.");
		say("@What shall be needed that I don't already have is one vial of Silver Serpent venom and a second piece of Blackrock ore.");
		if (venom_count) {
			if (blackrock_count) {
				say("@Since thou hast already all that is required for the potion, I can make it right now.");
				item->zaurielMakePotion();
			} else {
				say("@While thou hast the Silver Serpent venom, thou hast not any Blackrock.");
				say("@I beseech thee to gather two pieces of Blackrock and then come talk to me again.@");
			}
		} else {
			if (blackrock_count >= 2) {
				say("@Thou hast already all necessary Blackrock, but thou hast not any Silver Serpent venom.");
				say("@I beseech thee to gather a vial of Silver Serpent Venom.@");
			} else {
				var msg;
				say("@Thou hast neither enough Blackrock nor the required Silver Serpent venom.");
				if (blackrock_count == 0) {
					msg = "two pieces";
				} else {
					msg = "one piece";
				}
				say("@Thou wilt need to gather one vial of Silver Serpent Venom and ",
					msg, " of Blackrock ore.@");
			}

			add("Silver Serpent Venom");
		}
	} else {
		say("@I have already given thee the potion, to that much is off the list.");
		say("@What thou needst now is a piece of Blackrock ore to neutralize the necklace.@");
	}

}

void zaurielTalkPreQuest () {
	//Temporary variables:
	var rand;
	var asked_about_quest;
	var reward_counter;
	var msg;
	var potion;

	//Hold the met flag:
	var met_zauriel = get_item_flag(MET);

	//Get Avatar's name:
	var player_name = getAvatarName();

	var party = UI_get_party_list();

	//See which companions are here:
	var iolo_here = inParty(IOLO);
	var dupre_here = inParty(DUPRE);
	var shamino_here = inParty(SHAMINO);

	ZAURIEL->show_npc_face(0);

	//Player has not met Zauriel:
	if (!met_zauriel) {
		//Create the blackrock potion:
		potion = UI_create_new_object(SHAPE_BLACKROCK_POTION);
		give_last_created();

		//present his intro, which is rather long and involves Iolo, Shamino and Dupre:
		say("This strange man seems to be trying very hard not to show his face. For a while, he just seems to stare at you and your companions.");
		say("With a nod of approval, he finally speaks. @Welcome, ", player_name,
			", and well met.");
		say("@Let me first congratulate thee on thy timing, for I have been expecting thee for barely a couple of hours when thou didst arrive.");
		say("@I knew that if I stood in front of Lord British's castle for long enough, I was bound to run into thee,");
		say("@but it is fortunate that is has been sooner rather than later.@");

		if (iolo_here) {
			say("@I presume this is the famous Iolo, bard and crossbowman extraordinaire. Glad to make thine acquaintance.@");
			IOLO.say("@Thank thee for thy kind words, sir.@");
			IOLO.hide();
		}
		if (dupre_here) {
			say("The strange man turns to Dupre and gazes at him. @Sir Dupre, I presume. Thy fame precedes thee, valiant knight.@");
			DUPRE.say("@Well met indeed, noble sir!@");
			DUPRE.hide();
		}
		if (shamino_here) {
			say("@The legendary ranger Shamino, the deadliest bow in all Britannia. It honors me to meet thee.@ the man says with a bow.");
			SHAMINO.say("Shamino stares at the man for a while and then politely says @It is my pleasure to meet thee.@");
			SHAMINO.hide();
		}

		if (iolo_here && dupre_here && shamino_here) {
			//Iolo, Shamino and Dupre in party:
			say("@I am honored to meet the Avatar and his faithful companions! Such a mighty troupe reunited after 200 years can only result in great and virtuous deeds to be performed!");
		} else if (!(iolo_here || dupre_here || shamino_here)) {
			//Iolo, Shamino and Dupre not in party:
			say("@I must say that I am disappointed that thou dost not travel with thy friends Iolo, Shamino and Dupre anymore. Having such capable allies in thy party would ensure the success of thy quest.");
		} else {
			if (dupre_here) {
				if (iolo_here) {
					//Iolo and Dupre in party mean Shamino is not:
					msg = "friend Shamino, the ranger -- his bow";
				} else if (shamino_here) {
					//Shamino and Dupre in party mean Iolo is not:
					msg = "friend Iolo, the bard -- his crossbow";
				} else {
					//Neither Shamino nor Iolo in party:
					msg = "companions Iolo and Shamino -- their aid";
				}
			} else if (shamino_here) {
				if (iolo_here) {
					//Iolo and Shamino in party mean Dupre is not:
					msg = "friend Sir Dupre -- his sword";
				} else {
					//Neither Iolo nor Dupre in party:
					msg = "companions Iolo and Dupre -- their help";
				}
			} else {
				//Neither Shamino nor Dupre in party:
				msg = "companions Shamino and Dupre -- their assistance";
			}

			say("@If I were thee, I would be quick to find thine old ", msg,
				" shall be an invaluable asset in thy quest.");
		}

		say("@And yes, I do realize how unsettling it must be to meet someone who knows so much about thee and thy friends.");
		say("@It is just that I have an active interest in thee and thy career.",
			"~@Thine exploits and adventures make a formidable tale that I hope to preserve for posterity.@");

		//Player has now met Zauriel:
		set_item_flag(MET);
	} else {
		//Player has already met Zauriel, so use the shorter intro:
		say("Zauriel grins as you approach. @What can I do for thee now, Avatar?@");
	}

	//Standard options:
	add(["name", "job", "bye", "fellowship"]);

	if (!met_zauriel) {
		add("Waiting for me?");
	}
	//Player can always ask these again until Zauriel moves to Skara Brae:
	if (met_zauriel) {
		add(["task", "reward"]);
	}
	//Store the flags in variables for ease of use:
	var doing_quest = gflags[ACCEPTED_ZAURIEL_QUEST];

	//To store the player's supply of blackrock and silver serpent venom:
	var blackrock_count;
	var venom_count;
	var gave_potion;

	//Options added only if the player has accepted the quest:
	if (doing_quest) {
		//See if the potion has been given:
		gave_potion = (!count_objects(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY));

		//If it hasn't, see if the player has the necessary ingredients:
		if (!gave_potion) {
			//See if player has enough ingredients to make the potion:
			blackrock_count = PARTY->count_objects(SHAPE_BLACKROCK, QUALITY_ANY, FRAME_ANY);
			venom_count = PARTY->count_objects(SHAPE_VENOM, QUALITY_ANY, FRAME_ANY);

			//If this is the case, add the option:
			if ((blackrock_count >= 1) && venom_count) {
				add("Make potion");
			} else {
				add("What do I need again?");
			}
		} else {
			add("Where are they?");
		}
	}

	converse(0) {
		case "What do I need again?" (remove):
			zaurielTellAboutPotion();

		case "Waiting for me?" (remove):
			say("@Indeed. I have need of a hero such as thee to perform great deeds of courage and gallantry.");
			say("@It is a task of paramount importance, which shall bring thee a great reward when thou hast succeeded at it.@");
			add(["task", "reward"]);

		case "name" (remove):
			say("@Oh, forgive my manners. Thou mayest call me Zauriel. It is not my real name, but it is close enough that thy kind is able to pronounce it.@");
			add(["Zauriel", "kind"]);

		case "Zauriel" (remove):
			say("@Yes, that is how I told thee to call me. A pretty unusual name, thinkest thou not?@");

		case "kind" (remove):
			say("@It is not important. Let it go.@ The way in which he said this makes you consider twice before asking again.");

		case "job":
			say("@I suppose that that I am what thou wouldst call a 'mage'. I can sell thee potions and reagents and I can teach thee spells -- for a fee, of course.@");
			add(["spells", "reagents", "potions"]);

		case "spells" (remove):
			//Give lame excuse not to teach spells:
			if (PARTY->count_objects(SHAPE_SPELLBOOK, QUALITY_ANY, FRAME_ANY)) {
				say("@To be honest, I am a terrible teacher; thou wouldst do well to seek another instructor.");
				say("@Besides, until thou hast performed the task I need thee to, I will not be able to concentrate on such a menial duty.@");
			} else {
				//Berate the player for not having a spellbook and asking about spells:
				say("@Hadst thou a spellbook, I might teach thee some spells. But thou hast not. So I won't.@");
			}

		case "reagents":
			//The player has a spellbook:
			if (PARTY->count_objects(SHAPE_SPELLBOOK, QUALITY_ANY, FRAME_ANY)) {
				//Advertise free reagents:
				if (!doing_quest) {
					say("@If thou agreest to help me with my task, I shall give thee an ample supply of reagents as an advance payment.");
				}

				say("@But in the meantime, wishest thou to buy some reagents?@");
				if (askYesNo()) {
					//Sell reagents to player:
					zaurielSellReagents();
				} else {
					say("@No, of course thou dost not.@ You can swear that he is having a private laugh at your expense.");
				}
			} else {
				//The player does not have a spellbook:
				say("@Why wouldst thou want reagents when thou hast not a spellbook? Thinkest thou that they are edible?@");
			}

		case "potions":
			//Advertise free potions:
			if (!doing_quest) {
				say("@If thou agreest to help me with my task, I will stock thee with many potions as an advance payment. ");
			}

			say("@But in the meantime, wishest thou to buy some potions?@");
			if (askYesNo()) {
				//Sell potions to player:
				zaurielSellPotions();
			} else {
				say("@It would be wise to have potions; shouldst thou reconsider, let me know.@");
			}

		case "bye":
			if (!doing_quest) {
				say("@Shoulsdt thou change thy mind about doing my task, come talk to me.@");
			} else if (!gave_potion) {
				say("@Come talk to me when thou hast all ingredients for the potion.@");
			} else {
				say("@Come talk to me when thou hast rescued my daughter.@");
			}
			break;

		case "fellowship" (remove):
			say("@They claim to be a society of enlightened individuals who mutually help each other.");

			//The player has (or had) Alagner's notebook:
			if (gflags[DELIVERED_NOTEBOOK_TO_WISPS]) {
				say("@But thou knowest already that they are merely pawns of the Guardian, so why dost thou ask?@");
				add("the Guardian");
			} else {
				say("@To be honest, I have not paid any attention to them beyond that.@");
			}

		case "the Guardian" (remove):
			say("@Thou hast already talked to the wisps about him, so why dost thou ask me?@");

		case "task" (remove):
			say("@It is good that thou didst ask. There is a conjunction of events set in motion,");
			say("@events that will ultimately result in the deaths of many innocent people unless a brave hero intervenes.");
			say("@This particular task is exceedingly dangerous, and there is a very good chance of failure, resulting in death. Thou knowest, the usual.@");
			asked_about_quest = true;

			if (!doing_quest) {
				doing_quest = zaurielExplainQuest(false);
				if (doing_quest) {
					//Give the advance payment:
					zaurielTellAboutPotion();
					zaurielGiveAdvance();
					gave_potion = (!count_objects(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY));
				}
			} else {
				doing_quest = zaurielExplainQuest(true);
			}

		case "Silver Serpent Venom" (remove):
			say("@Silver Serpent venom is a poisonous substance extracted from silver serpents.");
			say("@There are people more capable than me to explain what it does, such as the apothecary here in Britain.");
			say("@It has been used by more than one mining company to make their employees work... harder.");
			say("@Thou might be able to find some with the Britannian Mining Company in Minoc.");
			say("@I can already feel thy concerns about giving such a substance to my daughter.");
			say("@While I would be hesitant to do this under normal conditions, the alchemical process I use to craft the final potion");
			say("@turns it into a mostly harmless substance, devoid of ill effects -- for my daughter, at any rate.@");

		case "Make potion" (remove):
			say("@Ah, thou hast all the needed ingredients to make the potion.");
			event = CAST_TELEPORT;
			item->zaurielMakePotion();

		case "Where are they?" (remove):
			say("@There is a small island to the north of Skara Brae; that is where the thugs are located.");
			say("@My divinations indicate that they are in the northwestern portion of the island.");
			say("@There -is- the issue that the island is disconnected from the mainland, being reachable only by boat or through flying.@");

		case "reward" (remove):
			if (!asked_about_quest && !doing_quest) {
				say("@Thou shouldst ask me about the task ahead first.@");
				add("reward");
			} else {
				say("@For starters, there is the good feeling thou wilt have knowing that thou hast saved the lives of many innocents and prevented widespread destruction.");
				say("@That alone is priceless stuff, as thou knowest very well.");
				say("@On a more practical note, there is an unspecified assortment of magical items and miscellaneous treasure");
				say("@which I shall give to thee should thou accomplish the task and save my daughter.@");
				add("What is the reward?");

				if (!doing_quest) {
					say("@So, wilt thou perform this task for me?@");
					if (askYesNo()) {
						doing_quest = true;
						say("@Splendid! I shall tell thee now what thou wilt need to rescue my daughter.@");
						zaurielTellAboutPotion();
						zaurielGiveAdvance();
						gave_potion = (!count_objects(SHAPE_BLACKROCK_POTION, QUALITY_ANY, FRAME_ANY));
					}
				}
			}

		case "What is the reward?":
			var random_answers = ["@The reward I shall give thee is very ample, and more than compensates the trouble thou wilt have. ",
					"@As I said, the reward is an unspecified assortment of magical items and miscellaneous treasure. ",
					"@Part of the reward is the warm and fuzzy feeling thou wilt have knowing that thou didst do the right thing. ",
					"@Magical gear. Gold. Gems. Knowing thou didst do the right thing. What more coudst thou wish for? ",
					"@One would think that merely knowing thou hast saved the lives of many innocents would be enough for thee...@"
					];

			//Player can try to badger Zauriel about what the reward is:
			reward_counter += 1;

			if (reward_counter > 20) {
				say("@Thy dogged perseverance will not aid thee in extracting any more information about the reward.");
				say("@As I said ", reward_counter,
					" times now, the reward is very ample and will be well worth thy time.@");
			} else {
				say(random_answers[UI_get_random(UI_get_array_size(random_answers))]);
				say("@But why art thou so worried about the reward?");
				if (reward_counter > 19) {
					msg = "@I assure thee that it exists, and that I will even give it to thee before I betray you at the end.@";
				} else {
					msg = "@I assure thee that it exists, and will be thine once thou hast completed the task.@";
				}
				say(msg);
				if (reward_counter == 20) {
					add("betray");
				}
			}

		case "betray" (remove): {
			//A very patient Avatar indeed... give him something for it
			giveExperience(50);
			say("@It was but a joke, of course. Thou hast set out to rescue my daughter, why would I betray thee?@");
		}
	}

	//Save the flags:
	if (doing_quest) {
		gflags[ACCEPTED_ZAURIEL_QUEST] = true;
	}
}

void zaurielTalkGemSubquest () {
	var msg;

	var quest_state = getQuestState();

	//See if the gem has been given:
	var gave_gem;
	if (quest_state == PLAYER_HAS_GEM) {
		gave_gem = true;
	} else {
		gave_gem = false;
	}
	var have_gem;
	var have_liche_gems;

	show_npc_face(0);

	if (quest_state == NO_ONE_THERE) {
		//Create a gem in Zauriel's possession; it will be the gem given
		//to the player when he is finished with the subquest:
		var gem = UI_create_new_object(SHAPE_GEM_OF_DISPELLING);
		give_last_created();
		say("@If the look in thy face is any indicator, I can see that something is amiss. What seems to be the trouble?@");
		add("No one was there");
	} else {
		say("@What can I do for thee now, Avatar?@");

		if (!gave_gem) {
			//Don't do this if the player already has the gem
			var have_ingredients_make;
			//See if the party has gems or Gems of Dispelling:
			have_gem = PARTY->count_objects(SHAPE_GEM, QUALITY_ANY, FRAME_ANY);
			have_liche_gems = PARTY->count_objects(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY);

			var have_ingredients_fix;

			//Get how many of the ingredients the party has:
			var spider_eggs = PARTY->count_objects(SHAPE_SPIDER_EGG, QUALITY_ANY, FRAME_ANY);
			var invis_dust = PARTY->count_objects(SHAPE_INVISIBILITY_DUST, QUALITY_ANY, FRAME_ANY);
			var bee_stringer = PARTY->count_objects(SHAPE_BEE_STINGER, QUALITY_ANY, FRAME_ANY);
			// See how many gems the party has, but ignore the FoV gems.
			var gems = 0;
			var framenum = 0;
			do {
				gems += PARTY->count_objects(SHAPE_GEM, QUALITY_ANY, framenum);
				framenum += 1;
			} while (framenum < 12);

			if ((spider_eggs >= 2) && (invis_dust) && (bee_stringer >= 6) && (gems)) {
				//Enough to make a gem:
				have_ingredients_make = true;
			}

			//This is a little cheating; to avoid overcomplicating the verification of
			//necessary components for the gem, I create the other components when Zauriel
			//appraises the gems.
			spider_eggs += count_objects(SHAPE_SPIDER_EGG, QUALITY_ANY, FRAME_ANY);
			invis_dust += count_objects(SHAPE_INVISIBILITY_DUST, QUALITY_ANY, FRAME_ANY);
			bee_stringer += count_objects(SHAPE_BEE_STINGER, QUALITY_ANY, FRAME_ANY);

			if ((spider_eggs >= 2) && (invis_dust) && (bee_stringer >= 6)) {
				//Enough to fix the gem:
				have_ingredients_fix = true;
			}

			//Add Gem Subquest dialog options:
			if ((quest_state == TOLD_ABOUT_GEM) && have_ingredients_make && have_gem) {
				add("Make gem");
			}
			if ((quest_state == TOLD_ABOUT_GEM) && have_liche_gems) {
				add(["Have gem", "liche"]);
			}
			if ((quest_state == TOLD_ABOUT_GEM) && (!have_liche_gems)) {
				if (have_ingredients_fix) {
					add("Fix gem");
				} else {
					add("What do I need again?");
				}
			}
			add(["components", "scout"]);
		}
		add(["reagents", "potions", "bye"]);
	}

	converse(0) {
		case "What do I need again?" (remove):
			//See what the player needs:
			var spider_eggs = 2 - count_objects(SHAPE_SPIDER_EGG, QUALITY_ANY, FRAME_ANY);
			var invis_dust = 1 - count_objects(SHAPE_INVISIBILITY_DUST, QUALITY_ANY, FRAME_ANY);
			var bee_stringer = 6 - count_objects(SHAPE_BEE_STINGER, QUALITY_ANY, FRAME_ANY);

			//Create the long message:
			message("@To fix the gem, thou wilt need ");
			if (spider_eggs > 0) {
				message(spider_eggs + " giant spider egg");
				if (spider_eggs > 1) {
					message("s");
				}

				if ((invis_dust > 0) && bee_stringer > 0) {
					message(", ");
				} else if ((invis_dust > 0) || bee_stringer > 0) {
					message(" and ");
				}
			}
			if (invis_dust > 0) {
				message("1 invisibility dust");
				if (bee_stringer > 0) {
					message(" and ");
				}
			}
			if (bee_stringer > 0) {
				message(bee_stringer + " bee stinger");
				if (bee_stringer > 1) {
					message("s");
				}
			}
			message(".@");
			//Display it:
			say();

		case "reagents":
			//The player has a spellbook:
			if (PARTY->count_objects(SHAPE_SPELLBOOK, QUALITY_ANY, FRAME_ANY)) {
				say("@Wishest thou to buy some reagents, Avatar?@");
				if (askYesNo()) {
					//Sell reagents to player:
					zaurielSellReagents();
				} else {
					say("@No, of course thou dost not.@ You can swear that he is having a private laugh at your expense.");
				}
			} else {
				//The player does not have a spellbook:
				say("@Why wouldst thou want reagents when thou hast not a spellbook? Thinkest thou that they are edible?@");
			}

		case "potions":
			say("@Wishest thou to buy some potions, Avatar?@");
			if (askYesNo()) {
				//Sell potions to player:
				zaurielSellPotions();
			} else {
				say("@It would be wise to have potions; shouldst thou reconsider, let me know.@");
			}

		case "No one was there" (remove):
			//One wing of the Keyring Quest has been finished; kick start
			//the Gem of Dispelling subquest:
			giveExperience(100);

			AVATAR.say("You quickly relate to Zauriel that no one was there when you arrived at the island.");
			AVATAR.hide();

			say("At first, he has a look of disbelief in his face. @It cannot be! My divinations show that they are -in- the island.@");
			say("Then, his expression changes as if he was struck by lightning. @Unless...@");
			say("Zauriel quickly makes a few magical gestures and intones an incantation.",
				"~@In Wis Quas Hur!@");
			say("You are unsettled as Zauriel stares into the void, his gaze directed towards the far away island as if he could see it.");
			say("He finally turns towards you shaking his head. @Of course thou didst not see anything in the island. I should have expected that.@");
			add("What didst thou see?");

		case "What didst thou see?" (remove):
			say("@I saw a mighty spell of protection; they are actually halfway between Britannia and the Ethereal Void! -That- is why thou didst see nor bump into them.");
			say("@And that spell is very potent indeed... I have never seen anything like it before.");
			say("@Unfortunately, I cannot dispel it from here. I also cannot go there for that purpose due to the necklace... Let me think for a while.@");
			say("Zauriel pauses for a moment, lost in deep thoughts. @Maybe...@ He stops mid-sentence, and turns to you with a smile. @Yes, it might work!@");
			add("Might work");

		case "Might work" (remove):
			say("@Yes, it might indeed@ Zauriel answers eagerly.",
				"~Seeming to realize that you have no idea what he is talking about, Zauriel quickly elaborates.");
			say("@Gems of Dispelling! I haven't given them much thought for almost 20 years now! They might prove to be a way out of this conundrum!@");
			add("Gems of Dispelling");

		case "Gems of Dispelling" (remove):
			say("@Of course thou hast no clue still as to what I am talking about, for they are a rather recent invention --");
			say("@why, they were invented not long before the damaged Ether waves began!");
			say("@These gems have mighty dispelling spells cast upon them when they are made.");
			say("@Once the gem is shattered, the spells are triggered, dispelling all manner of spells in a wide area.");
			say("@Another feature of these gems is that it shines brightly whenever it detects powerful spells nearby,");
			say("@thus warning its owner that it might be a good idea to shatter it.");
			say("@With one such gem in thy possession, thou couldst dispel the protective spell thyself.@");
			add(["Who invented them?", "Where can we get one?"]);

		case "Who invented them?" (remove):
			say("@It was a mage I knew many years ago that went by the name of Joneleth. He has long since died, sadly...@");

		case "Where can we get one?" (remove):
			say("@That may very well prove to be an obstacle. I do not believe that any Gems are in existence today.");
			say("@Fortunately, I did learn the secret of making these gems myself. I haven't used it in almost 20 years, but I think I still remember it.");
			say("@This means that thou wilt have to go on another journey to gather some components so I can make one.");
			say("@Alternatively, thou couldst scout Joneleth's old house in the hopes of finding any remaining gems.@");
			add(["components", "scout"]);

		case "components" (remove):
			say("@There are four items that thou wilt have to acquire: the first is a gem, of course.");
			say("@Also, thou wilt need some invisibility dust. That will likely be hard to come by... and I afraid I don't have any with me.");
			say("@Third, there is the need for the stingers of giant bees -- six of them all told.");
			say("@There is a bee cave in northwestern Yew where thou mayest find them.");
			say("@Fourth and last, thou wilt need two giant spider eggs. Fortunately, there is a nest of such spiders a little to the southwest of the bee cave.@");

			if (!(quest_state == TOLD_ABOUT_GEM)) {
				say("@So, wilt thou go in search of the components?@");
				if (askYesNo()) {
					say("@Excellent! I shall await here preparing the incantations while thou gatherest the components I mention.@");
					add("bye");
					gflags[GAVE_GEM_SUBQUEST] = true;
					quest_state = TOLD_ABOUT_GEM;
				} else {
					add("components");
				}
			}

		case "scout" (remove):
			say("@From what I recall, Joneleth lived the remainder of his life in an island southeast of Buccaneers' Den.");
			say("@There is only one house there; if there are any Gems of Dispelling still in existence, they are likely there.");
			say("@Joneleth has been dead for many years, though, so there is no telling if his house has been looted before today.");
			say("@In any case, thou wilt do good to bring me any gems thou findest there, as there is no telling if their magic is still intact.");
			say("@Depending on thy luck, there will be a functioning gem there. Otherwise, I may need to reenchant any gems you might find.");
			say("@Luckily, reenchanting does not require as many ingredients as creating a new gem...");
			say("@but it may so happens that thou still hast to gather some of them anyway.@");
			if (!(quest_state == TOLD_ABOUT_GEM)) {
				say("@Wilt thou try scouting Joneleth's house?@");
				if (askYesNo()) {
					say("@Excellent! I shall be eagerly awaiting thy return, then.@");
					add("bye");
					gflags[GAVE_GEM_SUBQUEST] = true;
					quest_state = TOLD_ABOUT_GEM;
				} else {
					add("scout");
				}
			}

		case "Have gem" (remove):
			if (have_liche_gems > 1) {
				msg = "gems";
			} else {
				msg = "gem";
			}
			say("@Let me examine thy ", msg, ".@ Zauriel takes the ", msg,
				" from you.");
			//Here, Zauriel will appraise the gems the Avatar found
			//in Joneleth's body:
			var gems = [];
			var party = UI_get_party_list();
			for (npc in party) {
				gems &= npc->get_cont_items(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY);
			}

			var gem_quality;
			var low_qual = 255;
			var best_gem;
			var gem_appraise_better = ["@This gem is promising.@", "@This gem is the best so far...@", "@Now, that gem is better.@"];
			var gem_appraise_worse = ["@This gem isn't very promising...@", "@This gem should be thrown into the ocean!@", "@I hope thou hast better gems...@"];

			for (gem in gems) {
				gem_quality = gem->get_item_quality();
				if (gem_quality == 0) {
					if (have_liche_gems != 1) {
						say("@Excellent! Thou hast a perfect gem!@");
					} else {
						say("@Thou art lucky: thou hast but one gem, and it is perfect!@");
					}

					low_qual = 0;
					break;
				} else if (gem_quality < low_qual) {
					if (low_qual != 255) {
						say(gem_appraise_better[UI_get_random(UI_get_array_size(gem_appraise_better))]);
					} else if (have_liche_gems != 1) {
						say("@A decent gem, but let me see if thou hast better.@");
					}

					low_qual = gem_quality;
				} else {
					say(gem_appraise_worse[UI_get_random(UI_get_array_size(gem_appraise_worse))]);
				}

			}

			var quantities;
			var rand = UI_get_random(6);

			//Take away all gems ("For safekeeping"):
			UI_remove_party_items(have_liche_gems, SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY, true);

			if (low_qual != 0) {
				//None of the gems are ready for use
				if (have_liche_gems == 1) {
					msg = "@Unfortunately, thy gem is not";
				} else {
					msg = "@Alas, none of thy gems are";
				}
				say(msg, " ready for use yet.");
				//Determine (based on quality) what will be needed
				//to fix the best gem:
				if (low_qual < 10) {
					say("@Thou hast found a very good gem -- but thou wilt still have to gather a couple components.");
					if (rand < 4) {
						msg = "two bee stingers";
						quantities = [2, 4, 1];
					} else {
						msg = "one giant spider egg";
						quantities = [1, 6, 1];
					}
					say("@More accurately, thou shalt need to gather ", msg,
						" if I am to fix the gem.");
				} else if (low_qual < 20) {
					say("@Thou hast found a good gem -- however, thou wilt still have to gather a few components.");
					if (rand < 3) {
						msg = "three bee stingers";
						quantities = [2, 3, 1];
					} else if (rand > 4) {
						msg = "two giant spider eggs";
						quantities = [0, 6, 1];
					} else {
						msg = "one invisibility dust";
						quantities = [2, 6, 0];
					}

					say("@Specifically, I need thee to gather ", msg,
						" in order to repair the gem.");
				} else if (low_qual < 30) {
					if (have_liche_gems == 1) {
						msg = "gem";
					} else {
						msg = "best gem";
					}

					say("@Thy ", msg,
						" is badly damaged, but I can still work with it -- but thou wilt have to gather many components to make it work.");
					if (rand < 3) {
						msg = "six bee stingers";
						quantities = [2, 0, 1];
					} else if (rand > 4) {
						msg = "two giant spider eggs and two bee stingers";
						quantities = [0, 4, 1];
					} else {
						msg = "one invisibility dust, one giant spider egg and one bee stinger";
						quantities = [1, 5, 0];
					}

					say("@In other words, thou hast to gather ", msg,
						" before I can work on the gem.");
				} else {
					if (have_liche_gems == 1) {
						msg = "gem";
					} else {
						msg = "best gem";
					}

					say("@Thy ", msg,
						" is so damaged it is almost beyond recovery... after all thy hard work defeating the liche.");

					if (rand < 3) {
						msg = "six bee stingers and two spider eggs";
						quantities = [0, 0, 1];
					} else if (rand > 4) {
						msg = "two giant spider eggs, three bee stingers and one invisibility dust";
						quantities = [0, 3, 0];
					} else {
						msg = "one invisibility dust, one giant spider egg and five bee stinger";
						quantities = [1, 1, 0];
					}

					say("@In the end, thou hast to gather ", msg,
						" before I can work on the gem... which is almost as much as is needed for creating a new gem.");
				}
				zaurielCreateComponents(quantities);

				if (have_liche_gems == 1) {
					msg = "the gem";
				} else {
					msg = "all of the gems";
				}

				say("@In any case, I shall keep in my possession ", msg,
					" thou hast.");

				if (have_liche_gems == 1) {
					msg = "it";
				} else {
					msg = "them";
				}

				say("@After all, neither one of us wants anything to happen to ",
					msg, " in thy journey.@");
			} else {
				//Avatar has a perfect gem; give it back to him:
				UI_set_last_created(get_cont_items(SHAPE_GEM_OF_DISPELLING, QUALITY_ANY, FRAME_ANY));
				if (!AVATAR->give_last_created()) {
					say("@Since thou art so overburdened, I shall place the gem on the ground.@");
					UI_update_last_created(AVATAR->get_object_position());
				}
			}

		case "Make gem":
			zaurielMakeGem(true);

		case "Fix gem":
			zaurielMakeGem(false);

		case "liche" (remove):
			AVATAR.say("You tell Zauriel that you encountered a liche in Joneleth's house which claimed to own the gems.");
			AVATAR.hide();

			say("@A liche you said? Alas, I have little doubt that it was Joneleth himself. I thought he had died, but he apparently decided to dabble with the foul arts.");
			say("@I am afraid I am not surprised by his fate -- he was never that bright to begin with. The Gems of Dispelling notwithstanding, of course.@");

		case "bye":
			say("@Farewell, Avatar, and godspeed!@");
			break;
	}
}

void zaurielLastTalk () {
	ZAURIEL->show_npc_face0(0);
	if (LAURIANNA->get_npc_object() in UI_get_party_list()) {
		//Laurianna is still in the party, so remove her:
		LAURIANNA->remove_from_party();
		LAURIANNA->set_schedule_type(WAIT);
	}
	giveExperience(300);

	//The reunion between father and daughter:
	say("Zauriel is clearly overjoyed to see his daughter returned.");

	LAURIANNA->show_npc_face1(0);
	say("Laurianna runs to her father and hugs him, relieved to be reunited with her father. @Father! I missed thee so much! I was so afraid...@");

	UI_set_conversation_slot(0);
	say("@Yes, my child, I know, I could feel thy fear. I am overjoyed that thou art safe once again! How dost thou fare, my child? Art thou well?@");

	UI_set_conversation_slot(1);
	say("@Aye, father. I was on the verge of losing control when the noble Avatar saved me!@");
	LAURIANNA.hide();

	UI_set_conversation_slot(0);
	say("@Avatar, I cannot thank thee enough for saving my beloved daughter from the hands of those fiends! Thank thee!");
	say("@As for thy reward, I do not have it in my possession; thou canst go to my house and retrieve it thyself.");
	say("@Thou canst find my house on an island to the northwest of Serpent's Hold; thou mayest take anything thou wishest from it.");
	say("@This key will unlock the front door and the door to my laboratory.@");

	//The reward turns out to be a key... for now, at least:
	var zauriel_key = UI_create_new_object(SHAPE_KEY);
	zauriel_key->set_item_quality(1);
	AVATAR->give_last_created();

	gflags[RECEIVED_ZAURIEL_REWARD] = true;

	say("@It is time now to begin the process of healing my daughter's mind. I sincerely hope it works...@");
	say("@Wilt thou help me, Avatar? I shall have need of your services for a little while longer if thou art willing.@");
	if (askYesNo()) {
		//A good avatar deserves something more:
		giveExperience(100);
		say("@Splendid! Thou art really worth of thy title, oh mighty champion of Virtue!@");
		say("@For what is worth, I must apologize for the actions I shall take now.@");
	} else {
		say("@I apologize then, for I shall have to force the issue.@");
	}
	say("Zauriel turns towards his daughter. @Thou knowest what is about to happen, knowest thou not?");
	say("@It is as we spoke before those buffoons interrupted.@");

	LAURIANNA->show_npc_face1(1);
	say("@Y-yes, father@, says Laurianna with a sad look in her eyes. @But father, is it really necessary? Can't something else be tried??@");

	UI_set_conversation_slot(0);
	say("@Thou knowest as well as I that it cannot be helped. It is meant to be this way, and it is the only hope thou hast of a normal life.",
		"~@Thou dost want that, dost thou not?@");

	UI_set_conversation_slot(1);
	say("@I do indeed. But the price is high...@");

	UI_set_conversation_slot(0);
	say("@The price is not thine to pay, so concern thee not with it.@");
	LAURIANNA.hide();

	UI_set_conversation_slot(0);
	if (chooseFromMenu2(["apologize", "What must be done?", "What is the price?", "Meant to be this way"])) {
		//Ignore the avatar's choice:
		say("@I sincerely wish that I had time to explain it to thee, but time is of the essence. I am sorry...@");
	}
	//Freeze everyone
	item->trueFreeze();
	LAURIANNA->trueFreeze();
	AVATAR->trueFreeze();

	//Start the ritual:
	event = BEGIN_RITUAL;
	item->zaurielRitualCutscene();
}
