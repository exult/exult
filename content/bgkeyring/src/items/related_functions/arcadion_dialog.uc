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
 *	This source file contains several functions used by the blacksword,
 *	including dialog with Arcadion in all forms.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//For better coding of the blacksword creation sequence:
const int BLACKSWORD_SWING_EVENT					= 1;
const int BLACKSWORD_LIGHTNING_EVENT				= 2;

void killLordBritish (var lordbritish) {
	//Lord British is close enough to kill:
	if (AVATAR->get_distance(lordbritish) < 5) {
		//The dialog with Lord British:
		say("@Yes! I have long sought the end of Lord British, my traitorous master.@");
		LORD_BRITISH.say("@", getAvatarName(), ", for what reason art thou brandishing that black sword in my presence?@");
		AVATAR->show_npc_face(UI_is_pc_female());
		say("The daemon responds, using your mouth. @This blade is thy doom,...@ You spit the words, @Lord British!@");
		LORD_BRITISH.say("Lord British looks truly taken aback, his eyes narrow calculatingly. @What foul treachery is this?@");
		AVATAR->show_npc_face(UI_is_pc_female());
		say("You find yourself unable to respond, and your muscles are clenching as if to lash out with the wicked blade in your hand.");
		LORD_BRITISH.say("@Perhaps when thou art sitting in a dungeon, thy tongue will loosen.");
		LORD_BRITISH.say("@Guards!@*");

		//The direction the avatar must face:
		var dir = directionFromAvatar(lordbritish);

		//Avatar strikes his liege:
		script AVATAR {
			face dir;
			actor frame ready;
			wait 2;
			actor frame raise_2h;
			wait 2;
			actor frame reach_2h;
			wait 1;
			actor frame strike_2h;
			wait 2;
			actor frame standing;
		}

		//Lord British is awake:
		if (!lordbritish->get_item_flag(ASLEEP)) {
			//Direction Lord British must face:
			dir = ((dir + 4) % 8);

			//Lord British tries to defend himself, but dies anyway:
			script lordbritish {
				face dir;
				wait 3;
				actor frame ready;
				wait 7;
				call killTarget;
			}
		} else {
			//You cod, striking down your liege while he is ASLEEP???
			//Kill Lord British:
			script lordbritish {
				wait 12;
				call killTarget;
			}
		}
	} else {
		//Lord British is too far away to kill:
		say("The Shade Blade lets out a harsh whisper. @Move a little closer to him, and I'll perform this task for thee, master.@");
	}
}

var isCloseEnoughtToKill (var target, var answer) {
	if (AVATAR->get_distance(target) < 5) {
		return true;
	} else {
		say(answer);
		return false;
	}
}

void killAnimation (var target) {
	//The direction the avatar must face:
	var dir = directionFromAvatar(target);

	//Avatar strikes his foe:
	script AVATAR {
		face dir;
		actor frame ready;
		wait 1;
		actor frame raise_2h;
		wait 1;
		actor frame reach_2h;
		wait 1;
		actor frame strike_2h;
		wait 2;
		actor frame standing;
	}

	//Target is awake:
	if (!target->get_item_flag(ASLEEP)) {
		//Direction target must face:
		dir = ((dir + 4) % 8);

		//Target tries to defend itself and dies anyway:
		script target {
			face dir;
			wait 2;
			actor frame ready;
			wait 4;
			actor frame bowing;
			wait 1;
			actor frame kneeling;
			wait 1;
			call killTarget;
		}
	} else {
		//In a great display of valor and honor, the avatar kills a sleeping foe:
		script target {
			wait 12;
			call killTarget;
		}
	}
}

void arcadionSwordFormDialog object#() () {
	BLACK_SWORD_FACE->show_npc_face(0);

	if (gflags[MET_ARCADION]) {
		say("@Yes, master. What dost thou seek of thy servant?@ Arcadion asks you in a deep, harmonic voice.");
	} else {
		say("The sword glimmers darkly as you speak to it. @Greetings, my master. And how can thy humble servant aid thee?@ The daemon's voice has regained much of its oddly disturbing humor.");
		gflags[MET_ARCADION] = true;
	}

	add(["name", "job", "bye", "powers"]);
	if (gflags[SCROLL_OF_INFINITY] && (!gflags[BANISHED_EXODUS])) {
		add("help");
	}

	converse(0) {
		case "name" (remove):
			say("The daemon sword's tone is rather ominous as he says, @I am, and ever shall be, thy servant Arcadion.@");

		case "job" (remove):
			say("@I am the Shade Blade. My destiny is to serve thee until we are...@ The sword pauses, @parted.@");

		case "powers":
			if (!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_BLACK_SWORD, FRAME_ANY)) {
				say("@I needs must be in thy hand, master, if thou wishest to use my powers.@");
			} else {
				say("@Which of my powers dost thou seek to use?@");
				converse(["Magic", "Fire", "Death", "Return", "none"]) {
					case "none":
						say("@As thou wish, master. I but seek to serve thee.@");
						break;

					case "Magic":
						var part_of_day = UI_part_of_day();
						if ((part_of_day == NIGHT) || ((part_of_day == MIDNIGHT) || (part_of_day == EARLY))) {
							replenishMana(true);
						} else {
							say("The blade croons quietly, @Alas, master. My energies seem a trifle low. Perhaps if thou were to find some creature to slay, my power would be sufficient. After all, I have needs just as thou dost.@");
						}

					case "Death":
						say("@Where is the corpse of which thou dost speak?@ The dark sword begins to vibrate in your hand.*");
						BLACK_SWORD_FACE.hide();

						struct<ObjPos> target = UI_click_on_item();
						var target_shape = target->get_item_shape();
						struct<Position> target_position = target->get_object_position();

						BLACK_SWORD_FACE->show_npc_face(0);

						if (target->is_npc()) {
							if ((target_shape == SHAPE_MALE_AVATAR) || (target_shape == SHAPE_FEMALE_AVATAR)) {
								say("The daemon speaks with a sanctimonious tone. @I could not in honor take the life of my most wondrous master.@");
							} else if (target_shape == SHAPE_LORD_BRITISH) {
								killLordBritish(target);
								return;
							} else if ((target_shape == SHAPE_BATLIN) || (target_shape == SHAPE_BATLIN2)) {
								say("@Alas master, this one is protected by a power greater than mine. His destiny lies elsewhere.@");
							} else if (isUndead(target_shape)) {
								say("The sword recoils in something akin to horror. @That creature is beyond even my power. I suggest that thou hackest it to bits, if possible, then burn the pieces.@ Arcadion offers helpfully.");
							} else if (target->get_npc_number() == ZAURIEL) {
								say("The sword recoils in something akin to horror. @This one is beyond even my power. I suggest that thou hackest him to bits, if possible, then burn the pieces.@ Arcadion offers helpfully.");
							} else if (target->get_npc_number() == LAURIANNA) {
								say("The sword recoils in something akin to horror. @Alas, master, I dare not. Her power is so great it would destroy us both were I to kill her.@");
							} else if (target_shape == SHAPE_DRAGON) {
								if (isCloseEnoughtToKill(target, "The Shade Blade croons sofltly. @Move a little closer to the dragon, and I'll end its life for thee, master.@")) {
									if (target->get_cont_items(SHAPE_SCROLL, 241, 4)) {
										say("@Ah, Dracothraxus. We meet once again. 'Tis a pity thou shan't survive our meeting this time. Perhaps if thou hadst given the gem to me when first I asked, none of this unpleasantness would be necessary.@");

										DRACOTHRAXUS_FACE->show_npc_face(0);
										say("The dragon responds with great resignation. @My will is not mine own in this matter, Arcadion. Mayhap thou art finding too, that thy will is not thine own.@");
										DRACOTHRAXUS_FACE.hide();

										say("The daemon, possibly stung by the dragon's repartee, falls silent and goes to its bloody work.*");
									}
									killAnimation(target);
									return;
								}
							} else if (target_shape == SHAPE_MONSTER_MAGE) {
								if (isCloseEnoughtToKill(target, "@Move closer to him, and I'll see that his life plagues thee no more.@ The dark sword sounds almost gleeful at this prospect.")) {
									if (target->get_cont_items(SHAPE_SCROLL, 240, 4)) {
										say("@I owe thee quite a favor for this, master. I thank thee for allowing me this, my revenge!@*");
									}
									killAnimation(target);
									return;
								}
							} else if (target_shape == SHAPE_GOLEM) {
								say("@This creature is not strictly speaking,... living. Thy best course of action would be to smash it to pieces@ You hear a smile in Arcadion's voice.");
							} else if (isWorthyToKill(target_shape)) {
								if (isCloseEnoughtToKill(target, "@I must get closer to this one in order to enjoy its essence.@ The blade hums eagerly as it tugs in the direction of your selected target.")) {
									say("@Very well, master. If thou cannot dispatch this foe thyself, I shall do it for thee.@");
									killAnimation(target);
									return;
								}
							} else {
								say("The daemon sword abruptly ceases its vibration. @This being is hardly worth a death the likes of which I would visit upon it. Call upon me again when thou art faced with a more worthy opponent.@");
							}
						} else {
							if (isCorpseShape(target_shape)) {
								say("@Perhaps thou misunderstands my meaning. I do not raise the dead... I slay the living.@ The last is spoken in a sibilant whisper.");
							} else if (!target_shape) {
								say("@Thou wouldst have me destroy the very world around thee. Not a very bright idea for such a virtuous one as thou art thought to be.@ A strangely metallic chuckle escapes from the sword.");
							} else if (target_shape == SHAPE_FERRYMAN) {
								say("The sword recoils in something akin to horror. @That one is beyond even my power.@");
							} else if ((target_shape == SHAPE_DRAFT_HORSE) || (target_shape == SHAPE_WOUNDED_MAN)) {
								say("The daemon sword abruptly ceases its vibration. @This being is hardly worth a death the likes of which I would visit upon it. Call upon me again when thou art faced with a more worthy opponent.@");
							} else if (target_shape == SHAPE_BLACK_SWORD) {
								say("@Thou shall not be rid of me quite so easily, my master. However, I do not begrudge thine attempt. Quite to the contrary. I respect thy resourcefulness.@");
							} else if (target_shape == SHAPE_DARK_CORE) {
								say("@Would that I had such power. That artifact would allow me to return to my home plane if only I could unlock its secrets.@");
							} else {
								say("@Hast thou such a grudge against this inanimate object that thou wouldst see it perish forever?@ His voice is laden with undisguised sarcasm. @I cannot take life from that which is already lifeless.@");
							}
						}

					case "Return":
						if (!inIsleOfFire()) {
							say("@Ah... home again. I never tire of rocky little islands. Dost thou truly wish to go to the forsaken Isle of Fire?@");
							if (askYesNo()) {
								say("@I see. Very well, master. But let us not forget this little favor...@ The gem in the hilt of the sword glows brightly then everything dims.*");
								script item {
									wait 1;
									call teleportIsleOfFire;
								}
								return;
							} else {
								say("@It is good. Sense returns to the Virtuous Wonder. Thou art truly without peer in the arena of thought, master.@");
							}
						} else {
							say("@Forgive me, master, but are we not already on or near the Isle of Fire? Though, why one would wish to remain here on this forsaken piece of rock, I have no idea.@");
						}

					case "Fire":
						say("@And what, pray tell, is the intended target of thy immense and most puissant wrath, O' Master of Infinite Destruction?@");
						BLACK_SWORD_FACE.hide();
						item->createFire();
						return;
				}
			}

		case "help" (remove):
			say("Arcadion's voice is smug as he replies to your request for assistance. @Yes, I can help thee if thou wishest to exile what remains of Exodus to the Void. Firstly, thou shalt have need of the lenses of which the doddering, old fool spoke. Next thou needs must have the three Talismans of Principle. And finally, make sure that there are lit torches upon the walls to either side of the pedestal upon which the Dark Core rests.");
			add(["lenses", "talismans"]);

		case "lenses" (remove):
			say("@The concave and convex lenses which thou used to place the Codex of Infinite Wisdom within the Void, I believe now sit forgotten in the Museum of Britannia. They must be placed between the Dark Core and the torches on either side of the pedestal@");

		case "talismans" (remove):
			say("@The Talismans of Principle must be placed upon the Dark Core like wedges in a pie.@");

		case "bye":
			say("@Forgive me master, but I shan't be leaving. However, thou mayest cease thy speaking... if thou dost wish it.@*");
			return;
	}
}

void blackswordCreationAnimation object#() () {
	//The original used global flags for this; they *could* have used quality instead...
	//I use event levels since they are available on Exult for UCC scripts.
	//Also, this was originally in the arcadionDialog function.
	if (event == BLACKSWORD_SWING_EVENT) {
		//(!gflags[BEGIN_BOND])
		script AVATAR {
			face south;
			actor frame strike_2h;
			wait 1;
			actor frame reach_2h;
			wait 1;
			actor frame raise_2h;
			wait 1;
		}

		script item after 7 ticks
			call blackswordCreationAnimation, BLACKSWORD_LIGHTNING_EVENT;

		//gflags[BEGIN_BOND] = true;
		return;
	} else if (event == BLACKSWORD_LIGHTNING_EVENT) {
		//(!gflags[FINISHED_BOND])
		struct<Position> pos = AVATAR->get_object_position();
		UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, 3);
		UI_sprite_effect(ANIMATION_LIGHTNING, pos.x, pos.y, 0, 0, 0, -1);
		UI_play_sound_effect(0x003E);

		script item after 3 ticks
			//call arcadionDialog;
			call arcadionSwordFormDialog;

		//gflags[FINISHED_BOND] = true;
		return;
	}
}

void arcadionGemFormDialog object#() () {
	ARCADION_GEM_FACE->show_npc_face(0);

	var dont_add_master = false;
	var msg;

	if (!gflags[MET_ARCADION]) {
		say("The little gem pulses with energy, @Now all Britannia shall feel my wrath. I'll make them all pay for every decade I spent within that accursed mirror!@ The gem glows brighter, and you expect the world to come apart at the seams... then, nothing. @NO!@ The daemon's primal scream sounds a bit crystalline through the medium of the gem. @This cannot be! That old fool was right. I'm still trapped!@ The daemon's anguished voice falls silent.");
		gflags[MET_ARCADION] = true;
		add(["name", "job", "wrath", "trapped", "bye"]);
	} else {
		say("The gem sparkles up at you, @Yes, master. How may I serve thee?@ Arcadion's voice is subdued.");
		add(["name", "job", "master", "bye"]);
	}

	if (gflags[TALKED_ABOUT_BINDING_GEM]) {
		add("black sword");
	}
	if (gflags[ARCADION_SLAVE]) {
		add("power");
	}

	converse (0) {
		case "black sword" (remove):
			say("@If thou dost wish me to bond the gem to the sword, thou hast but to command me, master.@");
			add("bond");

		case "bond" (remove):
			if (swordBlankAndGemInHands()) {
				var blacksword_blank = AVATAR->get_cont_items(SHAPE_SWORDBLANK, QUALITY_ANY, 15);
				var daemon_gem = AVATAR->get_cont_items(SHAPE_GEM, QUALITY_ANY, 13);

				blacksword_blank->remove_item();
				daemon_gem->remove_item();

				var blacksword = UI_create_new_object(SHAPE_BLACK_SWORD);
				blacksword->set_item_frame(0);

				say("@It will be done!@");
				say("As the gem touches the crosspiece of the sword, the sound of tearing metal screeches through the air. The blade shifts and shimmers almost as if alive.");

				if (AVATAR->give_last_created()) {
					say("Slowly, the sword settles into its original shape, except for the blue gem glowing in the hilt.");
				} else {
					say("There is a flash of what can only be described as black light and the sword is wrenched out of your grasp and falls to the ground.");
					struct<Position> pos = AVATAR->get_object_position();
					UI_update_last_created(pos);
				}

				gflags[COMMANDED_BOND] = true;
				gflags[MET_ARCADION] = false;

				script blacksword {
					finish;
					nohalt;
					//Maybe not needed?
					call forceGiveBlackSword;
					call blackswordCreationAnimation, BLACKSWORD_SWING_EVENT;
				}
				return;
			} else {
				say("@The sword and gem must be in thy hands for the bonding to be accomplished.@");
			}

		case "name" (remove):
			say("@My name is still Arcadion, although my prison has changed.@");

		case "job" (remove):
			say("@I am now thy servant. What is thy bidding, master?@");
			if(!dont_add_master) {
				add("master");
			}

		case "wrath" (remove):
			say("Arcadion sounds a bit pensive as he replies, @Forgive my momentary indiscretion, master. My bitter emotions overcame my reasoning for a brief time. I shall not let it happen again.@");
			if (!dont_add_master) {
				add("master");
			}

		case "trapped" (remove):
			if (!gflags[MET_ERETHIAN]) {
				msg = "the mage Erethian";
			} else {
				msg = "Erethian";
			}
			say("@It would seem that ", msg, " was correct in his assumption that should I enter this gem, my power would not be set free to use as I wish, instead it is at the beck and call of the one who possesses the gem.@");
			gflags[ARCADION_SLAVE] = true;
			add("power");

		case "power" (remove):
			if (!gflags[ACCEPTED_GEM_POWER]) {
				say("You hear a faint sigh, then, @Wouldst thou care to partake of my power?@");
				if (askYesNo()) {
					say("Arcadion sounds disappointed, @It is as I knew it would be. I am forever meant to be the slave of weak-willed mortals. Very well then, prepare thyself to recieve a portion of my vast eneregy.");
					gflags[ACCEPTED_GEM_POWER] = true;
					replenishMana(false);
				} else if (!gflags[REFUSED_GEM_POWER]) {
					say("@Perhaps I misjudged thee, master.@ He pauses for a thoughtful moment, @Mayhap in time thou canst call me friend as well as ally.@");
					gflags[REFUSED_GEM_POWER] = true;
				}
				if(!dont_add_master) {
					add("master");
				}
			} else {
				say("@Thou hast need of my energies again?@ Arcadion asks a little petulantly.");
				if(askYesNo()) {
					say("@Very well, prepare thyself.@ The gem glows.");
					replenishMana(false);
				} else if (!gflags[REFUSED_GEM_POWER]) {
					say("@What dost thou seek of me...@ A pause, @Master?@");
					gflags[REFUSED_GEM_POWER] = true;
				} else {
					say("@Dost thou seek to torment me with useless questions, or may I be of some service...@ A long pause, @Master.@");
				}

				if(!dont_add_master) {
					add("master");
				}
			}

		case "master" (remove):
			say("The daemon pauses for a moment, @Thou hast imprisoned my physical form, I am therefore bound to thy will by powers far older than thou or I wield. What wouldst thou have of me?@");
			add("bound");
			dont_add_master = true;

		case "bound" (remove):
			say("@Long ago, even by my accounting of time, my people were defeated by a powerful race of beings in an attempt to conquer this realm. This race lived here long before the coming of thy sovereign, Lord British. My poeple were defeated and they expected death, but these great and powerful beings were not destroyers. However, they also did not wish futher disruption by my kind. So they weaved enchantments beyond the ken of my race, binding us to the inhabitants of this realm. Thine own people merely use the existing enchantments to keep us enslaved, sometimes without an incling of how this was achieved.@");

		case "bye":
			say("@Farewell my master.@ The gem seems to dim a little.*");
			break;
	}
}

var isErethianHere() {
	var mage_list = find_nearby(SHAPE_MONSTER_MAGE, 10, MASK_NPC2);
	for (mage in mage_list) {
		if (mage->get_cont_items(SHAPE_SCROLL, 240, 4)) {
			return true;
		}
	}
	return false;
}

var isArcadionGemNearby () {
	var nearby_gems = find_nearby(SHAPE_GEM, 15, MASK_NONE);
	if (nearby_gems) {
		return arcadionGemInList(nearby_gems);
	} else {
		return false;
	}
}

void arcadionMirrorFormDialog object#() () {
	ARCADION_MIRROR_FACE->show_npc_face(0);

	var msg;

	if (isErethianHere()) {
		say("@Yes, Master. How may I serve thee?@ The dark form in the mirror bows deeply.");

		if (gflags[MET_ERETHIAN]) {
			msg = "Erethian";
		} else {
			msg = "the mage";
		}
		ERETHIAN_FACE->show_npc_face(1);
		say("Surprised, ", msg, " looks around and says, @I don't recall summoning thee. Nevermind, I have no need of thee at the current time. Begone!@ The old man waves his hand, negligently.");

		ARCADION_MIRROR_FACE->show_npc_face(0);
		say("Through a tightly clenched smile, the figure replies, @Very well...@ And after a significant pause, @Master.@*");
		arcadionMirrorHide();
		return;
	}

	if (gflags[HELPING_ARCADION]) {
		if (gflags[TALKED_ARCADION_WITH_GEM]) {
			say("Arcadion appears truly astonished, @For what dost thou wait?! I beg of thee! Release me!@");
			arcadionMirrorHide();
			return;
		} else {
			if (isArcadionGemNearby()) {
				say("@There is a gem nearby that can free me! It is a small blue stone. Take it, quickly, and use it to free me of this accursed mirror!@ The large daemon seethes with pent up frustration.*");
				gflags[TALKED_ARCADION_WITH_GEM] = true;
				arcadionMirrorHide();
				return;
			} else if (PARTY->count_objects(SHAPE_GEM, QUALITY_ANY, 12)) {
				say("@Thou hast within thy possessions a small blue gem. It can be used to free me! Crack this accursed mirror with it! I'll enter it as I am freed!@ Arcadion looks prepared to burst from the mirror.*");
				gflags[TALKED_ARCADION_WITH_GEM] = true;
				arcadionMirrorHide();
				return;
			}

			say("@Can I be of some small assistance in thy quest to release me. If so, thou hast but to ask.@ Arcadion's smile stretches from ear to ear.");
			add(["name", "job", "release", "bye"]);
		}
	} else if (gflags[REFUSED_HELP_ARCADION]) {
		say("@Run along now little mortal. Do not pester thy betters with the idle rantings of thy tongue.@ He appears at first nonchallant, then his expression becomes intense, @That is unless thou hast reconsidered my offer... Hast thou?@");
		if (askYesNo()) {
			say("A wicked look of triumph flickers across Arcadion's face to be quickly replaced by a ludicrous semblance of gratitude, @Thou art truly courageous to vow to release me. My eternal thanks are thine.@ An oily grin coats the daemon's face, @Thou hast made quite a powerful ally this day, mortal.@ His eyes blink in what is possibly meant to be a charming manner.");
			gflags[HELPING_ARCADION] = true;
			add(["name", "job", "daemon", "release", "bye"]);
		} else {
			say("@Ah, I see. Still content to run about with the other sheep.@ He waves you off and dims from sight.*");
			arcadionMirrorHide();
			return;
		}
	} else if (!gflags[MET_ARCADION]) {
		say("@Yes, Master. How may I serve thee...@ The wavering visage in the mirror hesitates for a moment, @Thou art not my master.@");
		say("He then continues with a small bow, @Greetings Britannian. What dost thou wish of the great daemon, Arcadion?@");

		gflags[MET_ARCADION] = true;
		add(["name", "job", "daemon", "bye"]);
	} else {
		say("@Greetings once again Britannian. What dost thou wish of me.@ The daemon is the soul of congeniality.");
		add(["name", "job", "daemon", "bye"]);
	}
	var dont_add_daemon = false;
	converse(0) {
		case "name" (remove):
			say("The big daemon smiles ingratiatingly, showing inch long pointed teeth. @As I have said, I am the daemon Arcadion.@");
			if (gflags[HELPING_ARCADION]) {
				say("His somewhat polished veneer seems to be unravelling at the edges in his anticipation of freedom.");
			} else if (!dont_add_daemon) {
				add("daemon");
			}

		case "job" (remove):
			if (!gflags[HELPING_ARCADION]) {
				say("Arcadion attempts to smile, but failing miserably, he gives you a grimace that could turn a dragon to stone. @I am currently in the service of one mage, Erethian by name.@ He states, rather formally. You get the distinct impression that Arcadion would just as soon rip Erethian limb from limb as serve him.");
				add(["Erethian", "serve"]);
			} else {
				say("@Well, if thou keepest thy promise to release me, I'll be free of that lice-ridden, flea-bitten, old mage.@");
				add("release");
			}

		case "Erethian" (remove):
			say("@He is my master...@ The daemon's smile contorts into a scarcely hidden scowl of hatred. @Until other... arrangements can be made.@ Arcadion's toothsome smile appears on his shadowy features.");
			if (!dont_add_daemon) {
				add("daemon");
			}

		case "daemon" (remove):
			say("@That is how thy people address those of my race.@ You can't tell from Arcadion's tone of voice whether or not he minds that fact.");
			dont_add_daemon = true;

		case "serve" (remove):
			say("The large daemon's eyes close as he appears to be restraining the force of horrific emotions,");
			say("@I have served that blind, old fool for over two hundred years!@ Arcadion pauses, regaining his composure. A thought visibly crosses his darkened face, @Perhaps thou mightest assist me to free myself of this unwanted bondage. I could prove an invaluable ally.@ The daemon pauses to let his offer sink in, then, @Well, mortal. Wilt thou help me?@");
			if(askYesNo()) {
				say("A wicked look of triumph flickers across Arcadion's face to be quickly replaced by a ludicrous semblance of gratitude, @Thou art truly courageous to vow to release me. My gratitude hath no bounds.@ An oily grin coats the daemon's face, @Thou hast made quite a powerful ally this day, mortal.@ His eyes blink in what is possibly meant to be a charming manner.");
				gflags[HELPING_ARCADION] = true;
				if (!dont_add_daemon) {
					add("daemon");
				}
				add("release");
			} else {
				gflags[REFUSED_HELP_ARCADION] = true;
				ARCADION_MIRROR_FACE.hide();
				ARCADION_MIRROR_FACE->show_npc_face(1);
				say("Arcadion looks as if he's about to force his way through the mirror, then once again masters his incredible rage.");
				ARCADION_MIRROR_FACE.hide();
				ARCADION_MIRROR_FACE->show_npc_face(0);
				say("He folds massive arms across a broad chest and slowly restores his gruesome smile, @I can respect thy cowardice in this situation. After all, Erethian is a powerful mage, not the sort that a sheep like thyself should be trifling with.@ His contemtuous sneer begins to fade as the daemon takes his leave.*");
				break;
			}

		case "release" (remove):
			say("@I'll need a special gem in which to house my essence when thou crackest this prison of a mirror.@ His eyes are alight with the possibility of his impending freedom.");
			add("gem");

		case "gem" (remove):
			if (isArcadionGemNearby()) {
				say("@I can sense that the gem is near! Take it! Take it, quickly and use it to free me of this accursed mirror!@ Arcadion is almost drooling in anticipation.*");
				gflags[TALKED_ARCADION_WITH_GEM] = true;
				break;
			} else if (PARTY->count_objects(SHAPE_GEM, QUALITY_ANY, 12)) {
				say("@Thou hast the gem! I feel it! Use it now to crack the mirror! I'll enter it as I'm freed!@ The daemon hardly restains his enthusiasm.*");
				gflags[TALKED_ARCADION_WITH_GEM] = true;
				break;
			} else {
				say("@There was one on this island, that much I know. Find it. Bring it to me and together, we shall break this mirror which binds me to that blasted mage.@*");
			}

		case "bye":
			if (gflags[HELPING_ARCADION]) {
				say("Arcadion winks in a very undaemonlike manner, @Farewell, brave mortal. Thy courage is unsurpassed among humans.@*");
			} else {
				say("The smiling daemon bows again, @Fare thee well, Britannian. Until we meet again.@ The daemon begins to fade even as his last words are spoken.*");
			}
			break;
	}
	arcadionMirrorHide();
}
