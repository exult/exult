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
 *	This source file contains usecode for the Shrines of the Virtues.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

const int SHRINE_FACES					= -296;
//Each shrine with its own 'face':
enum shrines {
	SHRINE_SACRIFICE					= 1,
	SHRINE_JUSTICE						= 2,
	SHRINE_HUMILITY						= 3,
	SHRINE_SPIRITUALITY					= 4,
	SHRINE_VALOR						= 5,
	SHRINE_COMPASSION					= 6,
	SHRINE_HONOR						= 7,
	SHRINE_HONESTY						= 8,
	SHRINE_SACRIFICE_DEFILED			= 9
};

enum shrine_codex_quest_levels {
	CODEX_NOT_STARTED					= 0,
	GOT_FROM_SHRINE						= 1,
	WENT_TO_CODEX						= 2,
	FINISHED_QUEST						= 3
};
//Easter egg:
const int BOOK_OF_FORGOTTEN_MANTRAS		= 29;

void Shrine shape#(0x463) () {
	var shrines = [
		"Sacrifice", "Justice", "Humility", "Spirituality",
		"Valor", "Compassion", "Honor", "Honesty"
	];
	var mantras = [
		"Cah", "Beh", "Lum", "Om",
		"Ra", "Mu", "Summ", "Ahm"
	];
	if (event == DOUBLECLICK) {
		//Determine shrine's face and position:
		var shrine_frame = get_item_frame() + 1;
		struct<Position> pos = get_object_position();
		var runes;
		//Determine if the party has the correct rune:
		if (shrine_frame == SHRINE_SACRIFICE_DEFILED) {
		 	runes = PARTY->count_objects(SHAPE_RUNE, QUALITY_ANY, 0);
		} else {
		 	runes = PARTY->count_objects(SHAPE_RUNE, QUALITY_ANY, shrine_frame - 1);
		}
		if (!runes) {
			//Nope, doesn't have it:
			randomPartySay("@Thou dost need the appropriate rune...@");
			return;
		} else {
			//Player has the correct rune; close gumps and start
			//playing 'Stones':
			UI_close_gumps();
			UI_play_music(22, 0);

			if (shrine_frame == SHRINE_SACRIFICE_DEFILED) {
				//The shrine of sacrifice starts defiled, and covered in blood
				AVATAR.say("You stand before the Shrine of Sacrifice. The Shrine is broken and defiled, with blood stains all over the altar.",
					"~Whatever peace there once was here is now long gone.");
				say("Do you wish to say anything?");
				//Not saying anything leaves it defiled
				if (askYesNo()) {
					//Avatar will try to fix the shrine:
					say("What do you want to say?");
					var choice = chooseFromMenu2(["nothing", "mantra", "Word of Power"]);
					if (choice == 1) {
						//Or maybe not...
						say("You walk away from the shrine, leaving it in its present state.");
					} else if (choice == 2) {
						//Poor, poor Avatar...
						say("Which mantra do you speak?");
						if (PARTY->count_objects(SHAPE_BOOK, BOOK_OF_FORGOTTEN_MANTRAS, FRAME_ANY)) {
							//Easter egg: the Book of Forgotten Mantras adds more options
							var all_mantras;
							all_mantras[1] = mantras;
							all_mantras[2] = [
									"Akk", "Hor", "Kra", "Maow", "Detra", "Sa",
									"Nok", "Spank"
							];
							all_mantras[3] = [
									"A", "Mi", "Ah", "Xiop", "Yof", "Ow", "Ta",
									"Goo"
							];
							all_mantras[4] = [
									"Si", "Yam", "Vil", "Wez", "Forat", "Asg",
									"Sem", "Tex"
							];
							all_mantras[5] = [
									"As", "Hiy", "Eyac", "Hodis", "Ni", "Baw",
									"Fes", "Upa"
							];
							all_mantras[6] = [
									"Yuit", "Swer", "Xes", "Led", "Zep", "Bok",
									"Mar", "Sak"
							];
							all_mantras[7] = [
									"Ces", "Blah", "Swu"
							];
							var page = 1;
							var numpages = 7;
							while (true) {
								var choices;
								var next;
								var prev;
								var shift;
								if (page == 1) {
									choices = [
											"nothing", "next",
											all_mantras[page]
									];
									next  =  2;
									prev  = -1;
									shift =  1;
								} else if (page == numpages) {
									choices = [
											"nothing", "previous",
											all_mantras[page]
									];
									next  = -1;
									prev  =  2;
									shift =  1;
								} else {
									choices = [
											"nothing", "next", "previous",
											all_mantras[page]
									];
									next  = 2;
									prev  = 3;
									shift = 2;
								}
								choice = chooseFromMenu2(choices);
								if (choice == 1) {
									break;
								} else if (choice == prev) {
									page -= 1;
								} else if (choice == next) {
									page += 1;
								} else {
									choice -= 2;
									break;
								}
							}
							if (choice != 1) {
								var mantras = all_mantras[page];
								choice = mantras[choice - 1];
							}
						}
						else {
							choice = chooseFromMenu2(["nothing", mantras]);
							if (choice != 1) {
								choice = mantras[choice - 1];
							}
						}
						if (choice == 1) {
							//Or maybe not...
							say("You walk away from the shrine, leaving it in its present state.");
						} else {
							say("In an ominous tone, you speak the mantra: @",
								choice, "!@",
								"~After waiting for a while, you realize nothing has happened.");
							if (isNearby(DUPRE) && choice == "Ni") {
								DUPRE.say("@We are the knights who say... Ni!@");
							}
						}
					} else {
						//This is a wise Avatar!
						say("Which Word of Power do you speak?");
						var words_of_power = [
							"Avidus", "Malum", "Ignavus", "Veramocor",
							"Inopia", "Vilis", "Infama", "Fallax"
						];
						choice = chooseFromMenu2(["nothing", words_of_power]);
						if (choice == 1) {
							//Or maybe not...
							say("You walk away from the shrine, leaving it in its present state.");
						} else {
							choice = words_of_power[choice - 1];
							say("In an ominous tone, you speak the Word of Power: @",
								choice, "!@ The ground suddenly trembles.");
							UI_earthquake(12);

							if (choice == "Avidus") {
								//The right choice, of course.
								giveExperience(50);
								AVATAR.hide();
								//Cleanse the shrine:
								obj_sprite_effect(ANIMATION_TELEPORT, 0, 0, 0, 0, 0, -1);
								set_item_frame(SHRINE_SACRIFICE - 1);
								UI_play_sound_effect2(64, item);

								var bloodstains = pos->find_nearby(SHAPE_BLOOD, 20, MASK_TRANSLUCENT);
								for (blood in bloodstains) {
									script blood after (3*get_distance(blood))/2 ticks remove;
								}
							} else {
								say("After a while, you realize that nothing else happened.");
							}
						}
					}
				}
				//LEAVE
				abort;
			}
			//Normal shrines:
			SHRINE_FACES->show_npc_face(shrine_frame - 1);
			say("You stand before the Shrine of ", shrines[shrine_frame],
				". The Shrine is a quiet and peaceful place, amidst the turmoil that is Britannia nowadays.");
			say("A mystical voice sounds inside your head as you approach the altar. @Welcome, seeker. Dost thou wish to meditate at this altar?@");
			if (!askYesNo()) {
				//Avatar doesn't wish to meditate, so leave:
				abort;
			}
			//Ask for mantra...
			say("@Upon which mantra wilt thou meditate, seeker?@");
			var chosen_mantra = chooseFromMenu2(mantras);
			//... and for number of cycles:
			say("@For how many cycles wilt thou meditate, seeker?@");
			var cycles = chooseFromMenu2(["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]) - 1;
			if (!cycles) {
				//0 cycles...
				SHRINE_FACES.hide();
				AVATAR.say("@I think I will meditate at another time.@");
				//Avatar doesn't wish to meditate, so leave:
				abort;
			}

			//Determine the place where the Avatar will meditate:
			var dir = direction_from(AVATAR);
			pos.z -= 4;
			if (dir in [NORTHWEST, NORTH, NORTHEAST]) {
				pos.y -= 2;
			} else if (dir in [SOUTHWEST, SOUTH, SOUTHEAST]) {
				pos.y += 2;
			} else if (dir == EAST) {
				pos.x += 4;
			} else {
				pos.x -= 4;
			}
			//Make Avatar go there:
			AVATAR->si_path_run_usecode(pos, PATH_SUCCESS, AVATAR, Shrine, true);
			UI_set_path_failure(Shrine, AVATAR, PATH_FAILURE);
			//Create a path egg containing the info about mantra
			//in the frame and cycles in the quality:
			var pathegg = UI_create_new_object(SHAPE_PATH_EGG);
			pathegg->set_item_frame(chosen_mantra);
			pathegg->set_item_flag(TEMPORARY);
			pathegg->set_item_quality(cycles);
			pos = get_object_position();
			UI_update_last_created(pos);

			//NPC banthers:
			var noun = "he";
			if (UI_is_pc_female()) {
				noun = "she";
			}

			const int BARK_COUNT = 3;
			var rand = UI_get_random(BARK_COUNT);
			var delay = UI_die_roll(5, 11);
			if (inParty(DUPRE) && (rand == 1)) {
				script DUPRE after delay ticks say "@Is there a pub nearby?@";
			} else if (inParty(SHAMINO) && (rand == 2)) {
				script SHAMINO after delay ticks say "@There " + noun + " goes again...@";
			} else if (inParty(IOLO) && (rand == 3)) {
				script IOLO after delay ticks say "@Where is my lute?@";
			}
			abort;
		}
	} else if (event == PATH_SUCCESS) {
		//Avatar reached destination
		//Find the path egg:
		var pathegg = find_nearest(SHAPE_PATH_EGG, 5);
		if (!pathegg) {
			//Not found means nothing to do; shouldn't happen...
			abort;
		}
		var dir = direction_from(pathegg);
		//Retrieve mantra and # of cycles:
		var chosen_mantra = pathegg->get_item_frame();
		var mantra = "@" + mantras[chosen_mantra] + "@";
		var cycles = pathegg->get_item_quality();

		//Meditation with a frozen Avatar:
		script item {
			nohalt;
			finish;
			call trueFreeze;
			face dir;
			actor frame standing;
			wait 2;
			actor frame bowing;
			wait 2;
			repeat cycles - 1 {
				actor frame kneeling;
				wait 3;
				say mantra;
				wait 3;
				actor frame kneeling;
				wait 3;
				actor frame kneeling;
				wait 3;
				//actor frame kneeling;
				//wait 3;
			};
			actor frame bowing;
			wait 2;
			actor frame standing;
			wait 2;
			call Shrine, SCRIPTED;
			call trueUnfreeze;
		}
	} else if (event == PATH_FAILURE) {
		//Avatar failed to get there
		item_say("@I can't get there@");
	} else if (event == SCRIPTED) {
		//Once again, find path egg:
		var pathegg = find_nearest(SHAPE_PATH_EGG, 5);
		if (!pathegg) {
			//Once again, should never happen...
			abort;
		}

		//Retrieve mantra and # of cycles
		var chosen_mantra = pathegg->get_item_frame();
		var cycles = pathegg->get_item_quality();
		//destroy path egg (no longer needed):
		pathegg->remove_item();
		//Find shrine and determine which shrine it is:
		var shrine = find_nearest(SHAPE_SHRINE, 5);
		var shrine_frame = shrine->get_item_frame() + 1;

		if (chosen_mantra != shrine_frame) {
			//Wrong mantra for shrine
			AVATAR.say("You had difficulty focusing your thoughts, and could not meditate.");
			abort;
		} else if (cycles < 3) {
			//Too few cycles
			AVATAR.say("After a while, you feel a sense of calm and inner peace.");
			abort;
		} else if (cycles > 3) {
			//Too many cycles
			AVATAR.say("You meditated for too long and eventually lost your focus.");
			abort;
		}

		//Basically, there are two flags per shrine which are used in binary
		var codex_quest_level = CODEX_NOT_STARTED;
		var codex_flag = VIEWED_CODEX_BASE + shrine_frame;
		var shrine_flag = MEDITATED_AT_SHRINE_BASE + shrine_frame;
		if (gflags[codex_flag] && gflags[shrine_flag]) {
			codex_quest_level = FINISHED_QUEST;
		} else if (gflags[codex_flag]) {
			codex_quest_level = WENT_TO_CODEX;
		} else if (gflags[shrine_flag]) {
			codex_quest_level = GOT_FROM_SHRINE;
		}
		if ((codex_quest_level == WENT_TO_CODEX) || (codex_quest_level == CODEX_NOT_STARTED)) {
			gflags[shrine_flag] = true;
		}
		var xpbonus = 0;
		//The experience bonus from meditating:
		if (codex_quest_level == CODEX_NOT_STARTED) {
			xpbonus = 25;
		} else if (codex_quest_level == WENT_TO_CODEX) {
			xpbonus = 75;
		}
		giveExperience(xpbonus);

		SHRINE_FACES->show_npc_face(shrine_frame - 1);
		say("After a while, you feel a sense of calm and inner peace.");
		if (codex_quest_level == CODEX_NOT_STARTED) {
			//Got a sacred quest to see the Codex:
			say("A mystical voice sounds inside your head, and a sacred quest is ordained. @Go thou to the Codex, seeker, to learn about ",
			    shrines[shrine_frame] + "!@");
		} else if (codex_quest_level == WENT_TO_CODEX) {
			//Returning from Codex:
			say("Once again, the mystical voice sounds inside your head. @Well done, seeker! Use well thy newfound enlightenment!@");
		}

		//NPC banther:
		if (UI_get_array_size(UI_get_party_list()) > 1) {
			var barks = [
				"@It's about time!@",
				"@We should be going...@"
			];
			var rand = UI_get_random(2 * UI_get_array_size(barks));
			if (rand > UI_get_array_size(barks)) {
				abort;
			}
			var npc = randomPartyMember();
			while (npc->get_npc_number() == AVATAR) {
				npc = randomPartyMember();
			}
			script npc after 8 ticks say barks[rand];
		}
		abort;
	}
}
