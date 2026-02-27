/*  Copyright (C) 2016  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Neyobi is a Gwani child. She is asleep with an illness that can only
 *  be cured with blood from an ice dragon. The original code for her is
 *  incomplete, so while she can be cured, she always goes back to her bed.
 *  This file gives her a schedule so she moves around as her existing
 *  dialog suggests.
 *
 *  2016-07-09 Written by Knight Captain
 */

void Neyobi object#(0x493) () {
	if (event == DOUBLECLICK) {
		if (NEYOBI->get_item_flag(SI_ZOMBIE)) {
			// If she hasn't been cured yet.
			AVATAR->item_say("@Wake up, little one!@");
			partyUtters(1, "@She cannot wake, Avatar. She is very sick.@", "@Poor little one! She is very sick.@", false);
		}
		if (!NEYOBI->get_item_flag(SI_ZOMBIE)) {
			// If she is no longer sick.
			AVATAR->item_say("@Good morning, little one!@");
			delayedBark(NEYOBI, "@Tee hee hee hee!@", 3);
			if (!NEYOBI->get_item_flag(MET)) {
				// If we haven't met her yet, give her a schedule.
				NEYOBI->set_new_schedules([MIDNIGHT   , MORNING    , AFTERNOON  , EVENING    , NIGHT      ],
				                          [WANDER     , SLEEP      , LOITER     , WANDER     , LOITER     ],
				                          [0x408,0x3A4, 0x3F5,0x36A, 0x3EC,0x359, 0x3B6,0x35E, 0x473,0x350]);
			}
			// She will get up and approach the Avatar.
			NEYOBI->set_schedule_type(TALK);
		}
	}
	if (event == STARTED_TALKING) {
		// Return to regularly scheduled activity.
		NEYOBI->run_schedule();
		NEYOBI->clear_item_say();
		NEYOBI->show_npc_face0(0);

		// First words with face shown.
		say("@Thou be Avatar! Thou nice!@");
		// First questions available.
		add(["name", "What art thou doing?", "bye"]);
		// Moved this up so the schedule setting does not repeat.
		NEYOBI->set_item_flag(MET);

		converse (0) {
			case "name" (remove):
				say("@Mother say thou found medicine make me better. Thank thou! My name Neyobi! Thou know what that mean?@");
				if (askYesNo()) {
					// Yes or No prompt.
					// Yes
					say("@Then thou very smart!@");
				} else {
					// No
					say("@It mean 'little dew drops' and also mean my name. Neyobi! Me!@");
				}

			case "What art thou doing?" (remove):
				say("@Early today I play! I like look at clouds! Other day I saw one that look like penguin! Later, mother and Baiyanda teach lessons.@");
				add(["mother", "Baiyanda", "lessons"]);

			case "mother" (remove):
				say("@Yenani, silly! She chieftain of our tribe. She tell me one day, after I grow, that what I be.@");

			case "Baiyanda" (remove):
				if (gflags[BANES_RELEASED]) {
					// Only says this if the Banes are loose.
					say("@I not see her in long time! I miss her lot.@");
				} else {
					// Says this otherwise.
					say("@She healer, and smartest person in whole world!@");
				}

			case "lessons" (remove):
				say("@Mother tells me the stories of the old days. She also teach me language of Men. She say Gwani very good at learn languages. Especially young ones.@");
				say("@Baiyanda teach me about plants and things.@");

			case "bye":
				UI_remove_npc_face0();
				UI_remove_npc_face1();
				delayedBark(AVATAR, "@Have fun, little one.@", 0);
				delayedBark(NEYOBI, "@Bye!@", 3);
				break;
		}
	}
}

