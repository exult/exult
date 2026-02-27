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
 *	This source file contains usecode for the Codex of Ultimate Wisdom.
 *	Things would be much easier if gflags accepted variables as well as
 *	constants...
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

const int CODEX					= -295;

void displayCodex object#() () {
	if (event == SCRIPTED) {
		//item is one of the lenses
		//Halt usecode for Avatar:
		halt_scheduled();
		//Sprite effects and sound associated with the Codex's appearance:
		UI_sprite_effect(ANIMATION_TELEPORT, 0xA9D, 0xAE4, 0, 0, 0, -1);
		UI_play_sound_effect(67);
		//After a little while, call this function again:
		script item after 10 ticks call displayCodex, EGG;
	} else if (event == EGG) {
		//The main part of the function.
		var codex_opens = "The pages of the Codex start to turn, and they stop in the page containing a message that you need.";
		var page_turning = "Once again the pages of the Codex start to turn, and they stop in the page containing a message that you need.";
		var ominous_turning = "Suddenly, a great wind starts blowing from nowhere. Amidst the terrible roar, the pages of the Codex turn wildly.~When the pages stop turning, ";
		var single_line = "there is but a single line in the page, containing exactly the message you need to see the most.";
		//Create the "Codex" and place it atop the pedestal:
		var codex = UI_create_new_object(SHAPE_JOURNAL);
		codex->set_item_frame(1);
		UI_update_last_created([0xAA1, 0xAE7, 7]);
		if (!gflags[SEEN_CODEX_ONCE]) {
			//First time the Codex appears in the game; extended intro:
			CODEX.say("Before you stands an image of the Codex of Ultimate Wisdom, brought forth from the Void by the combined actions of the Britannian and Gargoyle lenses.");
			//Mark as done:
			gflags[SEEN_CODEX_ONCE] = true;
			giveExperience(100);
		} else {
			CODEX.say("Once again, the Codex appears before you.");
		}
		//Check to see if the avatar cheated his way
		//inside the shrine of the Codex
		if (gflags[IN_CODEX_QUEST]) {
			//Nope, honest Avatar.
			var shrine_quests = 0;
			var displayed_messages = 0;
			//Display true codex message(s)
			//The basic idea is this:
			//	-	Count how many messages have been displayed by the Codex
			//		in a previous call to this function
			//	-	Display the messages that haven't been displayed yet, and count
			//		how many messages are being displayed
			var msgs = [
				"@Sacrifice is a virtue only if it is voluntary; to force others to Sacrifice of themselves is a grave injustice.~@Each man must be his own judge when determining how far he is willing to go to help others.@",
				"@The laws of the land are imperfect and do not always serve Justice.~@Keep always in mind that obeying or enforcing an unjust law is itself unjust.@",
				"@Pride is the bane of Virtue. When one deems himself to be better or to know more than others,~@he is opening the gates to unvirtuous behaviour by disregarding Truth, Love and Courage.@",
				"@The path to Spirituality is paved by wisdom and knowledge. Thou hast the need to know the right questions to ask,~@and hast to have learned enough of the world to be able to understand those questions.~@Spirituality is the never-ending search for the answers.@",
				"@There is a fine line between Valor and recklessness; wisdom lies in being able to tell one from another.~@Know that discretion is the better part of Valor.@",
				"@A life without enlightenment is a form of suffering about which the unenlightened is unaware.~@The enlightened can show their Compassion by fighting against such ignorance.@",
				"@Honor is more than being true to his word and being willing to defend one's convictions.~@Honor is also the courage to -revise- one's convictions when they are shown to be flawed or based on false assumptions.@",
				"@Honesty starts with one's self. Know thyself well, and maintain not a false assessment of thy knowledge or martial skills.~@Only when this is done can one achieve true enlightenment.@"
			];
			for (msg in msgs with index) {
				var codex_flag = VIEWED_CODEX_BASE + index;
				var shrine_flag = MEDITATED_AT_SHRINE_BASE + index;
				if (gflags[codex_flag]) {
					shrine_quests += 1;
				} else if (gflags[shrine_flag]) {
					gflags[codex_flag] = true;
					gflags[shrine_flag] = false;
					if (displayed_messages > 0) {
						say(page_turning);
					} else {
						say(codex_opens);
					}
					say(msg);
					displayed_messages += 1;
				}
			}

			var msg;
			var single_message = "The Codex of Ultimate Wisdom closes after displaying its message and then returns to the Void, until such a time when it is needed again.";
			var multi_message = "The Codex of Ultimate Wisdom closes after displaying its messages and then returns to the Void, until such a time when it is needed again.";
			var no_messages = "You await expectantly for it to display a message, but the Codex remains closed.~Eventually, it returns to the Void until such a time as when it is needed.";

			giveExperience(displayed_messages * 25);

			//Now see what else happens:
			if (gflags[RELOCATE_CODEX_QUEST]) {
				//Player has relocaed the Codex, finishing the Codex Quest;
				//Display congratulatory message:
				say(codex_opens);
				say("@This Codex is now safe again, seeker. Thou hast done well!@");
				if (displayed_messages == 0) {
					msg = single_message;
				}
			} else if (gflags[CODEX_ALL_EIGHT_SHRINES]) {
				//Player has been to all eight Shrines, and received the Codex Quest.
				if (gflags[CODEX_ALL_ITEMS_IN_PLACE]) {
					//The Vortex Cube and the three Items of Principle are
					//in their respective positions
					say(ominous_turning, single_line);
					say("@Speak ye the three-part Word of Passage!@");
					var choices = [
						"Infinity", "Singularity", "British",
						"Avidus", "Malum", "Ignavus", "Veramocor",
						"Inopia", "Vilis", "Infama", "Fallax"
					];
					var choice = askForResponse(choices);
					if (choice == "Veramocor") {
						//Player knows what he is talking about!
						CODEX.hide();
						avatarSay("In an ominous tone, you speak the three-part Word of Passage, which you learned so long ago while you where still on the Quest of the Avatar: @Veramocor!@");
						UI_earthquake(24);
						say("The image of the Codex disappears suddenly, and something begins to happen.");

						//Mark quest as completed and give XP:
						gflags[RELOCATE_CODEX_QUEST] = true;
						giveExperience(300);

						//Freeze Avatar, bark WoP, wait and unfreeze:
						script AVATAR {
							call trueFreeze;
							say "Veramocor!";
							wait 15;
							call trueUnfreeze;
						}

						//Animations & sounds:
						struct<Position> pos = codex->get_object_position();
						UI_sprite_effect(ANIMATION_CIRCLE_BARRIER, pos.x - pos.z/2, pos.y - pos.z/2 + 12, 0, -9, 0, -1);
						UI_sprite_effect(ANIMATION_TELEPORT, pos.x - pos.z/2, pos.y - pos.z/2, 0, 0, 0, -1);
						UI_play_sound_effect(64);
						//Delayed removal of the Codex:
						script codex after 13 ticks	remove;

						pos = [2719, 2801, 4];
						var itemsofprinc = pos->find_nearby(SHAPE_ITEM_OF_PRINCIPLE, 3, MASK_NONE);
						//Animations in the items of principle:
						for (obj in itemsofprinc) {
							pos = obj->get_object_position();
							UI_sprite_effect(ANIMATION_PURPLE_BUBBLES, pos.x - pos.z/2, pos.y - pos.z/2, 0, 0, 0, -1);
						}

						abort;
					} else if (choice in ["Infinity", "Singularity", "British"]) {
						say("In an ominous tone, you speak the word of your choice: @",
							choice, "!@");
						say("After a while, you realize that nothing happened at all.");
					} else {
						say("In an ominous tone, you speak the Word of Power: @",
							choice,
							"!@ The ground suddenly trembles.");
						UI_earthquake(12);
						say("After a while, you realize that nothing else happened.");
					}
					if (displayed_messages == 0) {
						msg = single_message;
					}
				} else {
					say(ominous_turning,
						"you can see the instructions to relocating the Codex in the Void.");
					say("@Look ye at the Codex symbol in the middle of this shrine. There shall ye place the Items of Principle.",
						"~@Each item must sit atop the circle which represents the same Principle.");
					say("@Place then the Vortex Cube midway between the Items of Principle and the two Lenses, and read again from this Codex.@");
					if (displayed_messages == 0) {
						msg = single_message;
					}
				}
			} else if (shrine_quests + displayed_messages == 8) {
				say(ominous_turning,
					"the displayed message is none other than the one you need the most.");
				say("@Know ye, seeker, that this Codex is in peril!",
					"~@A malevolent entity called 'The Guardian' seeks to pervert this Codex and turn it into a weapon of conquest and oppression.");
				say("@In order to prevent this, this Codex -must- be relocated unto another position in the Void.",
					"~@Bring thou the Vortex Cube unto this shrine, together with the Book of Truth, the Candle of Love and the Bell of Courage, and read once more from this Codex!@");
				//The player visited all eight shrines, viewed all eight messages:
				gflags[CODEX_ALL_EIGHT_SHRINES] = true;
				giveExperience(50);
				msg = multi_message;
			} else if ((shrine_quests + displayed_messages >= 7) &&
				(!gflags[VIEWED_CODEX_FOR_SPIRITUALITY] || !gflags[MEDITATED_AT_SPIRITUALITY]) &&
				(gflags[BROKE_SPHERE] || !gflags[ORB_FIXED_TIMELORD]) &&
				!gflags[ATTUNED_SPIRITUALITY_STONE]) {
				//The player has visited all shrines but the Shrine of Spirituality
				//Also, player does not have access to that shrine
				//This also happens if the player meditated at that shrine but
				//hasn't returned to it yet.
				if (displayed_messages > 0) {
					say(page_turning);
				} else {
					say(codex_opens);
				}
				if (PARTY->count_objects(SHAPE_VIRTUE_STONE, QUALITY_ANY, 7)) {
					//The party has the white virtue stone
					say("@Hold the Virtue Stone of Spirituality in thy hand and speak ye the Word of Power of Spirituality!@");
					var choices = [
						"Avidus", "Malum", "Ignavus", "Veramocor",
						"Inopia", "Vilis", "Infama", "Fallax"
					];
					var choice = askForResponse(choices);

					CODEX.hide();
					avatarSay("In an ominous tone, you speak the Word of Power: @" + choice + "!@ The ground trembles for a while afterwards.");
					UI_earthquake(12);
					if (choice == "Ignavus") {
						//Player knows his Ultima Lore well!
						var stone = PARTY->find_object(SHAPE_VIRTUE_STONE, QUALITY_ANY, 7);
						if (stone) {
							//Move stone to the correct place, set it and
							//return it to where it was:
							var cont = stone->get_container();
							stone->move_object([2919, 2435, 1, 1]);
							stone->mark_virtue_stone();
							stone->set_last_created();
							cont->give_last_created();
							gflags[SPIRITUALITY_STONE_QUEST] = false;
							gflags[ATTUNED_SPIRITUALITY_STONE] = true;
						}
						say("The Virtue Stone glows brightly for a moment, and you sense the flux of magic.");
					} else {
						say("After a while, you realize that nothing else happened.");
					}
					script codex remove;
					abort;
				} else {
					say("@Bring ye the Virtue Stone of Spirituality unto this shrine. Once that is done, read again from this Codex",
						"~@and learn how to attune the stone to the Shrine of Spirituality.@");
					gflags[SPIRITUALITY_STONE_QUEST] = true;
				}
				if (displayed_messages > 1) {
					msg = multi_message;
				} else {
					msg = single_message;
				}
			} else if (displayed_messages == 0) {
				msg = no_messages;
			} else {
				if (displayed_messages > 1) {
					msg = multi_message;
				} else {
					msg = single_message;
				}
			}
			say(msg);
		} else {
			//Cheater, cheater, compulsive cheater :-)
			say(ominous_turning, single_line);
			say("@CHEATERS NEVER WIN!@");
		}
		//Remove Codex:
		script codex remove;
	}
}
