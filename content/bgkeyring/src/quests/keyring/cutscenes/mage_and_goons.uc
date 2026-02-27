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
 *	This source file contains some functions specific to a cutscene
 *	from the Keyring Quest.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Cutscene levels:
enum MageAndGoons_levels {
	BEGIN_CUTSCENE						= 1,
	FADE_SCREEN							= 2,
	CENTER_VIEW							= 3,
	UNFADE_SCREEN						= 4,
	DISPLAY_REGION						= 5,
	MAKE_VISIBLE						= 6,
	GENERATE_SCRIPTS					= 7,
	FIREWORKS_LAURIANNA					= 8,
	GARGOYLE_WARNS						= 9,
	LAUNDO_SPEAKS						= 10,
	BEGIN_COMBAT						= 11,
	PREPARE_NPC							= 12
};

void beginCutsceneMageAndGoons object#() () {
	var mage;
	var gargoyle;
	var cyclops;
	var troll;
	var fighter;
	struct<Position> pos;
	var dir;

	if (event == BEGIN_CUTSCENE) {
		//Cutscene begins HERE, with item == AVATAR

		//Play music:
		UI_play_music(21, 0);

		//Cutscene mode :-)
		AVATAR->trueFreeze();
		//Create magical effect:
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_BLUE_BEADS, pos.x, pos.y, 0, 0, 0, -1);

		//Screen fader:
		script item after 5 ticks call beginCutsceneMageAndGoons, FADE_SCREEN;

		//Find Laundo:
		mage = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_MAGE_MALE, 80, ID_MAGE_OR_GOON);
		//Use him to unfade screen and kick-start the rest of the scene:
		script mage after 10 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, CENTER_VIEW;
			wait 2;
			call beginCutsceneMageAndGoons, UNFADE_SCREEN;
			wait 5;
			call beginCutsceneMageAndGoons, DISPLAY_REGION;
		}
	} else if (event == FADE_SCREEN) {
		//Fade screen to black:
		UI_fade_palette(12, 1, 0);
	} else if (event == CENTER_VIEW) {
		//center around item:
		center_view();
	} else if (event == UNFADE_SCREEN) {
		//Unfade screen:
		UI_fade_palette(12, 1, 1);
	} else if (event == DISPLAY_REGION) {
		//The magical effect of the Gem of Dispelling

		//item == mage

		//Play a sound effect:
		UI_play_sound_effect2(SOUND_MOONGATE, item);

		//Get each of the mage's goons:
		gargoyle = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_GARGOYLE_WARRIOR, 80, ID_MAGE_OR_GOON);
		cyclops = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_CYCLOPS, 80, ID_MAGE_OR_GOON);
		troll = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_TROLL, 80, ID_MAGE_OR_GOON);
		fighter = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_FIGHTER_MALE, 80, ID_MAGE_OR_GOON);

		//Make Laurianna visible and generate the scripts for the mage and goons:
		script LAURIANNA after 6 ticks {
			nohalt;
			call trueFreeze;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
			wait 4;
			call beginCutsceneMageAndGoons, GENERATE_SCRIPTS;
		}

		//Make the mage and his goons visible:
		script item after 6 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
		};
		script gargoyle after 6 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
		};
		script cyclops after 6 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
		};
		script troll after 6 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
		};
		script fighter after 6 ticks {
			nohalt;
			call beginCutsceneMageAndGoons, MAKE_VISIBLE;
		};
	} else if (event == MAKE_VISIBLE) {
		//item varies
		//Clears DONT_RENDER flag:
		clear_item_flag(DONT_RENDER);
		//Play sound effect:
		UI_play_sound_effect2(110, item);
		//Show magic bubbles:
		pos = get_object_position();
		UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, pos.x, pos.y, 0, 0, 0, -1);
	} else if (event == GENERATE_SCRIPTS) {
		//item = LAURIANNA

		//Make Laurianna have fireworks around her:
		script LAURIANNA call beginCutsceneMageAndGoons, FIREWORKS_LAURIANNA;

		//Get references for the mage and his goons:
		mage = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_MAGE_MALE, 80, ID_MAGE_OR_GOON);
		gargoyle = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_GARGOYLE_WARRIOR, 80, ID_MAGE_OR_GOON);
		cyclops = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_CYCLOPS, 80, ID_MAGE_OR_GOON);
		troll = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_TROLL, 80, ID_MAGE_OR_GOON);
		fighter = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_FIGHTER_MALE, 80, ID_MAGE_OR_GOON);

		//Laundo's script; he casts a couple spells and wonders why they failed; then,
		//he turns to the Avatar and begins to talk:
		script mage {
			wait 2;
			actor frame cast_up;
			wait 2;
			actor frame cast_out;
			wait 2;
			actor frame cast_up;
			wait 2;
			actor frame cast_out;
			wait 2;
			say "@An Sanct Ort!@";
			wait 2;
			actor frame standing;
			wait 2;
			actor frame cast_up;
			wait 2;
			actor frame cast_out;
			wait 2;
			actor frame cast_up;
			wait 2;
			actor frame cast_out;
			wait 2;
			say "@An Ort Sanct!@";
			wait 2;
			actor frame standing;
			wait 2;
			face east;
			say "@Damn! Why doesn't it work?@";
			wait 18;
			say "@The spell is correct!@";
			wait 18;
			say "@It -should- work!@";
			wait 18;
			say "@Something -must- protect her!@";
			wait 34;
			say "@What?!@";
			wait 18;
			say "@Intruders?!?@";
			wait 20;
			say "@I'll talk to them!@";
			wait 12;
			call beginCutsceneMageAndGoons, LAUNDO_SPEAKS;
		}

		//Agra-Lem's script. He patrols around until he sees the Avatar and gang;
		//then he sounds the alarm:
		script gargoyle {
			repeat 4 {
				step west;
				wait 2;
			};
			actor frame standing;
			wait 1;
			face north;
			wait 1;
			face east;
			wait 1;
			repeat 5 {
				step east;
				wait 2;
			};
			actor frame standing;
			wait 1;
			face north;
			wait 1;
			face west;
			wait 1;
			repeat 2 {
				step west;
				wait 2;
			};
			actor frame standing;
			wait 1;
			face north;
			wait 1;
			actor frame standing;
			repeat 3 {
				step north;
				wait 2;
			};
			actor frame standing;
			wait 1;
			face east;
			wait 1;
			face south;
			wait 1;
			repeat 4 {
				step south;
				wait 2;
			};
			wait 2;
			say "@!!!@";
			wait 6;
			say "@To see intruders!@";
			repeat 2 {
				step west;
				wait 2;
			};
			call beginCutsceneMageAndGoons, GARGOYLE_WARNS;
		}

		//The cyclop's script. He just plays a game of Dragon, Sword, Gold
		//with the troll:
		script cyclops {
			actor frame raise_1h;
			wait 4;
			say "@Dragon...@";
			actor frame ready;
			wait 4;
			actor frame raise_1h;
			wait 4;
			say "@... sword...@";
			actor frame ready;
			wait 4;
			actor frame raise_1h;
			wait 4;
			say "@... gold!@";
			actor frame strike_1h;
			wait 4;
			actor frame standing;
			wait 8;
			say "@Damn!@";
			wait 56;
			say "@Yer cheatin'!@";
			wait 18;
			say "@Yer winnin' always!@";
			wait 22;
			say "@Huh?@";
		}

		//The troll's script. He just plays a game of Dragon, Sword, Gold
		//with the cyclops:
		script troll {
			wait 5;
			actor frame raise_1h;
			wait 4;
			say "@Dragon...@";
			actor frame ready;
			wait 4;
			actor frame raise_1h;
			wait 4;
			say "@... sword...@";
			actor frame ready;
			wait 4;
			actor frame raise_1h;
			wait 4;
			say "@... gold!@";
			actor frame strike_1h;
			wait 4;
			actor frame standing;
			wait 6;
			say "@Haha! My sword...@";
			wait 18;
			say "@... kills yer dragon!@";
			wait 18;
			say "@I win again!@";
			wait 22;
			say "@Am not!@";
			wait 18;
			say "@Yer too dumb!@";
			wait 14;
			face east;
			say "@Intruders? Kill!@";
		}

		//The human is having second doubts, but he is there for
		//the money anyway...
		script fighter {
			wait 4;
			say "@What am I doing here?@";
			wait 10;
			say "@I mean, look who is around...@";
			face north;
			wait 16;
			say "@One dumb troll...@";
			wait 16;
			say "@... a dumber cyclops...@";
			wait 16;
			say "@... one pissed-off gargoyle...@";
			face east;
			wait 16;
			say "@... and a mad mage.@";
			wait 20;
			say "@The only good thing...@";
			wait 16;
			say "@... is the gold I get.@";
			wait 24;
			say "@Intruders? Where?@";
		}
	} else if (event == FIREWORKS_LAURIANNA) {
		//item = LAURIANNA
		//This portion of the function is looped over and over
		//again until Laundo's death halts it.
		//Creates fireworks around Laurianna:
		pos = LAURIANNA->get_object_position();
		UI_sprite_effect(ANIMATION_FIREWORKS, pos.x, pos.y, 0, 0, 0, -1);
		//Loop:
		script item after 10 ticks call beginCutsceneMageAndGoons, FIREWORKS_LAURIANNA;
	} else if (event == GARGOYLE_WARNS) {
		//item = gargoyle
		//Gargoyle sounds the alarm:
		script item {
			say "@To see intruders!@";
			wait 12;
			say "@To believe intruders...@";
			wait 12;
			say "@... to see us too!@";
		}

		//A party member reacts to it:
		script randomPartyMember() after 20 ticks say "@Ooops...@";
	} else if (event == LAUNDO_SPEAKS) {
		//Laundo begins talking to Avatar:
		KEYRING_ENEMY->show_npc_face(LAUNDO_FACE);
		say("@Hey! Thou there! Who art thou?@");
		//Laundo demands Avatar's name...
		var choice = askForResponse([getAvatarName(), "Avatar", "None of thy business"]);
		if (choice == "Avatar") {
			say("The mage whispers to his companions. @I think we are safe. This one seems mad.@");
		} else if (choice == "None of thy business") {
			say("@A fiery spirit! I like that! No, wait -- I don't.@ He adds menacingly.");
		} else {
			say("@Bah, thy name is not really important and means nothing to me.@");
		}
		//...rapid-fires a lot of questions without giving time for answers...
		say("@-I- am the archmage Laundo. Now, who art thou?");
		say("@No, wait -- I have already asked that. What did I want to ask again?");
		say("@Ah, yes! How didst thou -- a mere mortal -- dispel my warding spells?");
		say("@And what art thou doing in my tower?");
		say("@Wait -- I am -not- in my tower! Regardless, what art thou doing here?");
		say("@Come now, speak quickly! Shouldst thou speak not, I shall kill thee where thou standest!");
		say("@Won't thou say anything? Hast thou a death wish?");
		say("@I am in the middle of an experiment! I don't have time for this! Speak or die!@");
		//...and is warned by Agra-Lem of his behaviour:
		const int GARGOYLE_FACE = -262;
		GARGOYLE_FACE.say("The gargoyle interrupts. @Master! To not be letting intruder speak!@");
		GARGOYLE_FACE.hide();
		say("@Ah, thou art correct, Agra-lem. Go on, peasant! Speak! I shall listen.");
		say("@Perhaps thou shalt amuse me before dying...@ he adds, sounding hopeful.");
		add(["name", "job", "bye", "dispel", "tower", "Doing here", "rescue", "experiment", "Laurianna", "Rooted woman"]);

		converse (0) {
			case "name" (remove):
				say("@-I- am the archmage Laundo. Now, who art thou?");
				say("@No, wait -- I have already asked that. Never mind.@");

			case "job" (remove):
				say("He looks at you with impatience. @I am a mage! An -arch-mage!@");
				add("archmage");

			case "archmage" (remove):
				say("He gives you an exasperated look. @A mage so powerful he transcends other mages!");
				say("@And I am one! Hahaha! Behold my powers and beware!@");

			case "bye" (remove):
				say("@Not so fast, peasant! Thou didst not tell me what thou art doing here!@");

			case "dispel" (remove):
				say("@Yes, I want to know how a nothing like thee dispelled my protection spells!");
				say("@Wilt thou tell me or wilt thou die?@");
				if (chooseFromMenu2(["tell", "die"]) == 2) {
					//Quickest way to end the talk:
					break;
				}
				AVATAR.say("You quickly explain about the Gem of Dispelling to Laundo.");
				AVATAR.hide();
				say("@Gems of Dispelling? Never heard of them. It doesn't matter anyway.@");

			case "tower" (remove):
				say("@Yes, I have a tower. All wizards have one. But I want a -castle- instead.@");
				add("castle");

			case "castle" (remove):
				say("@I mean castle British, of course. None others are worthy!");
				say("@But alas, I must first slay that tyrant British to have his castle...@");
				add("Tyrant British");

			case "Doing here" (remove):
				say("@Wilt thou tell me what art doing here?@");
				if (chooseFromMenu2(["rescue", "lie"]) == 1) {
					say("@I am sorry to say thou hast wasted thy time. That mad mage sent thee on a wild goose chase.");
				} else {
					say("@I am sorry to say thou art wasting my time. I can feel the touch of that mad mage upon thee!");
				}
				say("@In any case, thou wilt fail to rescue this mageling! She is mine!@");
				add("fail", "mad mage");
				remove("rescue");

			case "rescue" (remove):
				say("@I am sorry to say thou hast wasted thy time. That mad mage sent thee on a wild goose chase.");
				say("@In any case, thou wilt fail to rescue this mageling! She is mine!@");
				add("fail", "mad mage");
				remove("Doing here");

			case "fail" (remove):
				say("@Yes, thou wilt fail. By which I mean thou shalt die if thou dost try to rescue her.");
				say("@And even if thou survivest, she will not be going anywhere...");
				say("@Seest thou the necklace she wears? It is -my- handiwork.@");
				add("necklace");

			case "mad mage" (remove):
				say("@Yes, that mage that wants to keep this menace alive!@ he points towards Laurianna.");
				say("@He wants to keep that -beast- alive! And he helps people! He is -mad-!");
				say("@With all the power he has, he could -- I mean, -should- -- kill the tyrant British!@");
				add("Tyrant British");

			case "Laurianna" (remove):
				say("@Who? Never heard of her.@");
				AVATAR.say("You explain that Laurianna is the woman with roots just nearby.");
				AVATAR.hide();
				say("@Ah, so -that- is her name? Well, it doesn't matter. She won't be going anywhere.");
				say("@Seest thou the necklace she wears? It is -my- handiwork.@");
				add("necklace");

			case "necklace" (remove):
				say("@It is lovely, is it not? It is a mighty artifact!");
				say("@The person wearing it -- in this case, this dangerous lady here -- cannot move at all from where she is!");
				say("@Not even magic can move her! And the necklace also cannot be removed! Hah! She shall be here forever!");
				say("@And the best part is that the necklace will simply -explode- should someone powerful enough to dispel its magic");
				say("@come even -near- it. That keeps that mad mage from trying to rescue her! Isn't it brilliant?");

			case "experiment" (remove):
				say("@Since thou wilt die anyway, I don't see why not explain the greatness of my plans to thee!");
				say("@This woman is a menace. She wields enormous power, and has no control over them.");
				say("@So I intend to give her powers to a worthy receptacle -- -me-!");
				say("@I can then do what I want with them, and she won't be a menace anymore! Haha!");
				say("@Of course, then I can remove other menaces from the face of Britannia.");
				say("@One that comes to mind is that tyrant British. He -must- die!@");
				add("Tyrant British");

			case "Tyrant British" (remove):
				say("@Thou knowest, that bloke with a crown which calls himself 'king'");
				say("@That miserable excuse of a tyrant must perish for his misdeeds!@");

			case "Rooted woman" (remove):
				say("@Yes, that is an effect of the necklace I made for her. Isn't it lovely?@");
				add("necklace");
		}

		//When all options are exhausted (or the player chose the quick rout),
		//we get to this point:
		say("@Well, it was interesting. But thou art boring me. So now it is time to die!@");

		//Return view to avatar:
		script AVATAR {
			nohalt;
			call beginCutsceneMageAndGoons, FADE_SCREEN;
			wait 2;
			call beginCutsceneMageAndGoons, CENTER_VIEW;
			wait 3;
			call beginCutsceneMageAndGoons, UNFADE_SCREEN;
			wait 3;
			call beginCutsceneMageAndGoons, BEGIN_COMBAT;
			wait 3;
			call trueUnfreeze;
};
	} else if (event == BEGIN_COMBAT) {
		//item = AVATAR
		//Get references to the mage and his goons:
		mage = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_MAGE_MALE, 80, ID_MAGE_OR_GOON);
		gargoyle = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_GARGOYLE_WARRIOR, 80, ID_MAGE_OR_GOON);
		cyclops = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_CYCLOPS, 80, ID_MAGE_OR_GOON);
		troll = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_TROLL, 80, ID_MAGE_OR_GOON);
		fighter = findNearbyMonsterWithID(LAURIANNA->get_object_position(),
										SHAPE_FIGHTER_MALE, 80, ID_MAGE_OR_GOON);

		//Make them hostile and in combat:
		script mage {
			nohalt;
			call beginCutsceneMageAndGoons, PREPARE_NPC;
		};
		script gargoyle {
			nohalt;
			call beginCutsceneMageAndGoons, PREPARE_NPC;
		};
		script cyclops {
			nohalt;
			call beginCutsceneMageAndGoons, PREPARE_NPC;
		};
		script troll {
			nohalt;
			call beginCutsceneMageAndGoons, PREPARE_NPC;
		};
		script fighter {
			nohalt;
			call beginCutsceneMageAndGoons, PREPARE_NPC;
		};

		//Release the Avatar from cutscene mode:
	} else if (event == PREPARE_NPC) {
		//item varies
		//Make npc hostile:
		set_alignment(2);
		//Puts him in combat mode:
		set_schedule_type(IN_COMBAT);
		//Just to be sure:
		clear_item_flag(DONT_RENDER);
	}

}

void registerDeathOfMageOrGoon object#() () {
	if ((event == DEATH) &&  (get_npc_id() == ID_MAGE_OR_GOON)) {
		if (get_item_shape() == SHAPE_MAGE_MALE) {
			//Laundo has died
			//Set flag:
			gflags[MAGE_KILLED] = true;
			//Halt the fireworks around Laurianna:
			LAURIANNA->halt_scheduled();
		}
		//This is called whenever the mage or one of his goons die
		//Removes the mage/goon from tournament mode:
		clear_item_flag(SI_TOURNAMENT);
		//Kill them:
		reduce_health(50, MAGIC_DAMAGE);
		//Increase Laurianna's NPC ID:
		var new_id = 1 + LAURIANNA->get_npc_id();
		LAURIANNA->set_npc_id(new_id);

		if (new_id == 5) {
			LAURIANNA->set_npc_id(5);
			//Everyone is dead; halt scripts for Laurianna:
			LAURIANNA->halt_scheduled();
			//Prepare the next portion of the quest:
			script LAURIANNA after 20 ticks call Laurianna, CLEAR_FLAGS;
		}
	}
}
