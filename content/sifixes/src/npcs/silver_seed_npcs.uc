/*	Copyright (C) 2016  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/*	Each of the Silver Seed NPCs have been added to this mod as real NPCs,
 *	yet their original conversations remain in their shape's code.
 *	This file points the new NPC numbers back to that shape's conversation
 *	and allows for setting the MET flag and following new schedules.
 *
 *	Some of the shapes' code uses aborts, so try and catch are used to
 *	consistently return the NPCs to their newly scheduled activities.
 *	
 *	2016-08-13 Written by Knight Captain
 */

void Isstanar object#(0x4EA) ()
{
	if (event == DOUBLECLICK)

		item->funcCapedAutomaton();

	else if (event == STARTED_TALKING)
	{
		item->funcCapedAutomaton();
		ISSTANAR->set_item_flag(MET);
		// Isstanar orginally returns to LOITER after talking,
		// but has no aborts to try/catch.
		// He should follow his new schedule instead.
		ISSTANAR->run_schedule();
	}
}

void Elissa object#(0x4EB) ()
{
	if (event == DOUBLECLICK)

		item->funcMageWomanBlueGreen();
	
	else if (event == STARTED_TALKING)
	{
		try
		{
			item->funcMageWomanBlueGreen();
		}
		catch ()
		{
			ELISSA->set_item_flag(MET);
			// Elissa originally returns to LAB after saying Bye, unless there is an abort.
			// She should follow her new schedule instead.
			ELISSA->run_schedule();
		}
	}

	// Due to the different way Exult handles the death of newly-added NPCs,
	// Elissa needs DEATH code to remove her spells shapes.
	else if (event == DEATH)
	{
		removeSpellShapes(item);
		UI_clear_item_flag(item, 0x001D);
		UI_reduce_health(item, 0x0032, 0x0000);
	}
}

void Surok object#(0x4EC) ()
{
	if (event == DOUBLECLICK)
		
		item->funcHealer();	

	else if (event == STARTED_TALKING)
	{
		try
		{
			item->funcHealer();
		}
		catch ()
		{
			// Surok's original funcHealer code uses an abort on Bye.
			SUROK->set_item_flag(MET);
			SUROK->run_schedule();
		}
	}
}

void Tsandar object#(0x4ED) ()
{
	if (event == DOUBLECLICK)

		item->funcOphidianSoldier();
	
	else if (event == STARTED_TALKING)
	{
		try
		{
			item->funcOphidianSoldier();
		}
		catch ()
		{
			TSANDAR->set_item_flag(MET);
			// Tsandar originally returns to LOITER after saying Bye,
			// unless his training options return an abort.
			TSANDAR->run_schedule();
		}
	}
}

void Shal object#(0x4EE) ()
{
	if (event == DOUBLECLICK)

		item->funcMadMan();

	else if (event == STARTED_TALKING)
	{
		// Met must be set first for The Fiend.
		SHAL->set_item_flag(MET);
		item->funcMadMan();
	}
	
	else if (event == DEATH)
	{
		removeSpellShapes(item);
		UI_clear_item_flag(item, 0x001D);
		UI_reduce_health(item, 60, 0x0000);
	}
}

void Yurel object#(0x4EF) ()
{
	if (event == DOUBLECLICK)
	
		item->funcCatMan();

	else if (event == STARTED_TALKING)
	{
		item->funcCatMan();
		YUREL->set_item_flag(MET);
		// Yurel otherwise would just STANDTHERE.
		YUREL->run_schedule();
	}
	
	// Yurel was originally programmed with Barks when the Avatar is nearby,
	// but they did not work in the original game because the path number eggs
	// need to have some set to Quality 15 (or 47) for the Usecode to trigger.
	else if (event == PROXIMITY)
	{	
		if (YUREL->get_schedule_type() == PATROL)
		{
			var meows = ["@Leave Yurel alone!@", "@Stay away from Yurel!@", "@Yurel wants to go home!@", "@Oh, woe!@", "@Yurel hungry!@", "@Help me!@"];
			delayedBark(YUREL, meows[UI_get_random(UI_get_array_size(meows))], 0);
		}
	}
}

void Draxinar object#(0x4F0) ()
{
	if (event == DOUBLECLICK)
		
		item->funcGreenDragon();

	else if (event == STARTED_TALKING)
	{
		try
		{
			item->funcGreenDragon();
		}
		catch ()
		{
			// Draxinar's original code uses an abort on Bye.
			DRAXINAR->run_schedule();
			
			// Check if we know Draxinar's name. It's possible to end the conversation without it.
			if (DRAXINAR->get_npc_id() > 0)
			{
				DRAXINAR->set_item_flag(MET);
			}
		}
	}

	else if (event == DEATH)
	{
		removeSpellShapes(item);
		UI_clear_item_flag(item, 0x001D);
		UI_reduce_health(item, 0x0032, 0x0000);
	}
}
