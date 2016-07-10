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
 
void Neyobi object#(0x493) () 
{
	if (event == DOUBLECLICK)
	{
		if (NEYOBI->get_item_flag(SI_ZOMBIE)) // If she hasn't been cured yet.
            {
            AVATAR->item_say("@Wake up, little one!@"); // Only works if partyUtters is commented out or placed before this line. Bug 1957.
            partyUtters(1, "@She cannot wake, Avatar. She is very sick.@", "@Poor little one! She is very sick.@", false);
            }
        if (!NEYOBI->get_item_flag(SI_ZOMBIE)) // If she is no longer sick.
            {
            AVATAR->item_say("@Good morning, little one!@");
		        delayedBark(NEYOBI, "@Tee hee hee hee!@", 3); // Per original usecode. Does not appear, Bug 1957.
            // This next if section is a hack to get around having to edit the NPC on the map.
            // Otherwise we would only need to take her out of the WAIT activity.
            if (!NEYOBI->get_item_flag(MET)) // If we haven't met her yet, give her a schedule.
                {        
                NEYOBI->set_new_schedules(MIDNIGHT, WANDER, [0x408, 0x3A4]); // Outside of family cave.
                NEYOBI->modify_schedule(MORNING, SLEEP, [0x3F5, 0x36A]); // Similar to parents, Yenani and Myauri.
                NEYOBI->modify_schedule(AFTERNOON, LOITER, [0x3EC, 0x359]); // No place to sit and eat. 
                NEYOBI->modify_schedule(EVENING, WANDER, [0x3B6, 0x35E]); // With Yenani to learn.
                NEYOBI->modify_schedule(NIGHT, LOITER, [0x473, 0x350]); // With Baiyanda, should not step in fire.
                // set_new_schedules can only have a single one in its clause, as it overwrites everything else.
                // In this case we use it first to clear Neyobi's existing WAIT at DAWN, then fill in the rest with
                // modify schedule, which only updates that daypart. Not sure how to clear a single entry otherwise.
                }
        NEYOBI->set_schedule_type(TALK); // She will get up and approach the Avatar.
            }
    }
	if (event == STARTED_TALKING)
	{
		NEYOBI->run_schedule(); // Return to regularly scheduled activity.
		NEYOBI->clear_item_say();
		NEYOBI->show_npc_face0(0);

        say("@Thou be Avatar! Thou nice!@"); // First words with face shown.
        add(["name", "What art thou doing?", "bye"]); // First questions available.
        NEYOBI->set_item_flag(MET); // Moved this up so the schedule setting does not repeat.
        
        converse (0)
		{
			case "name" (remove):
                say("@Mother say thou found medicine make me better. Thank thou! My name Neyobi! Thou know what that mean?@");
                if (askYesNo()) // Yes or No prompt.
				{
					say("@Then thou very smart!@"); // Yes
				}
				else
					say("@It mean 'little dew drops' and also mean my name. Neyobi! Me!@"); // No
                
            case "What art thou doing?" (remove): 
                say("@Early today I play! I like look at clouds! Other day I saw one that look like penguin! Later, mother and Baiyanda teach lessons.@");    
                add(["mother", "Baiyanda", "lessons"]);
                
            case "mother" (remove):
                say("@Yenani, silly! She chieftain of our tribe. She tell me one day, after I grow, that what I be.@");
                
            case "Baiyanda" (remove):
                if (gflags[BANES_RELEASED])
                say("@I not see her in long time! I miss her lot.@"); // Only says this if the Banes are loose.
                else
                say("@She healer, and smartest person in whole world!@"); // Only says this otherwise.
            
            case "lessons" (remove):
                say("@Mother tells me the stories of the old days. She also teach me language of Men. She say Gwani very good at learn languages. Especially young ones.@"); 
                say("@Baiyanda teach me about plants and things.@"); 
        
			case "bye":
				UI_remove_npc_face0();
				UI_remove_npc_face1();
				delayedBark(AVATAR, "@Have fun, little one.@", 0);
				delayedBark(NEYOBI, "@Bye!@", 3); // Does not appear, Bug 1957.
				break;
        }
    }
}
