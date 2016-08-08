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
 
/*	The five automatons in Skullcrusher are stuck in Waiting until they are
 *	attacked, despite them having code for reactions and some conversations.
 *	The same egg near each of them does not to seem to trigger any activity.
 *	This code reuses the existing broken eggs to bring them to life.
 *
 *	This file works in tandem with:
 *	"npcs\skullcrusher_automatons.uc" and "misc\egg_gwani_attack.uc"
 *	
 *	2016-08-08 Submitted by Knight Captain
 */	
 
 void SkullcrusherAutomatons object#(0x6B9) ()
{
	if (event == EGG)
	{	
		// First, give them schedules.
		if (!gflags[SKULLCRUSHER_AUTOMATONS])
		{
			{	// Entrance Guard
				var npc = GUARD17;
				npc->set_npc_name("West Guard");
				npc->set_schedule_type(STANDTHERE);
				// Replaces Waiting schedule at 0962, 1045.
				npc->set_new_schedules(DAWN, STANDTHERE, [0962, 1045]);
			}
			
			{	// Exit Guard
				var npc = GUARD21;
				npc->set_npc_name("Exit Guard");
				npc->set_schedule_type(PACE_VERTICAL);
				// Replaces Waiting schedule at 1297, 0888.
				npc->set_new_schedules(DAWN, PACE_VERTICAL, [1297, 0888]);
			}

			{	// Serpent Gate Guard
				var npc = GUARD20;
				npc->set_npc_name("North Door Guard");
				// Move him to the new spot, since run_schedule won't place him because he's close enough?
				npc->move_object([1217, 0887, 00], 1);
				npc->set_schedule_type(STANDTHERE);
				// Replaces Waiting schedule at 1225, 0884 with standing to the west now 1217, 0887.
				npc->set_new_schedules(DAWN, STANDTHERE, [1217, 0887]);
			}
			
			{	// East Big Doors Guard, northern one
				var npc = GUARD19;
				npc->set_npc_name("East Door Guard N");
				// Move him just a step West.
				npc->move_object([1418, 1045, 00], 1);
				npc->set_schedule_type(STANDTHERE);
				// Replaces Waiting schedule at 1419, 1045.
				npc->set_new_schedules(DAWN, STANDTHERE, [1418, 1045]);
				// Make him face West.
				script npc {nohalt; face WEST;}
			}
			
			{	// East Big Doors Guard, southern one
				var npc = GUARD18;
				npc->set_npc_name("East Door Guard S");
				// Move him just a step West.
				npc->move_object([1418, 1048, 00], 1);
				npc->set_schedule_type(STANDTHERE);
				// Replaces Waiting schedule at 1419, 1048.
				npc->set_new_schedules(DAWN, STANDTHERE, [1418, 1048]);
				// Make him face West.
				script npc {nohalt; face WEST;}
			}
			
			// Set the flag so this does not run again. At least one of the eggs is an
			// auto-reset egg, meaning it would keep retrying these changes when hatched.
			gflags[SKULLCRUSHER_AUTOMATONS] = true;
		}

		// Then make them respond to your presence.
		
		{	// Entrance Guard
			var npc = GUARD17;
			// If he is on-screen and has not been broken and re-created via spell.
			if ((npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Bark per existing Origin Usecode.
				delayedBark(npc, "@Hold!@", 1);
				// Set his next activity to Talk because he has conversation.
				npc->set_schedule_type(TALK);
			}
		}
		
		{	// Exit Guard
			var npc = GUARD21;
			if ((npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Bark per existing Origin Usecode.
				delayedBark(npc, "@Die, intruder!@", 1);
				// Using Chaotic so he'll also attack the Evil Gwani inside.
				// This only happens if he knocks out the whole party.
				npc->set_alignment(CHAOTIC);
				// Set his target to the Avatar.
				npc->set_opponent(AVATAR);
				npc->set_schedule_type(IN_COMBAT);
			}
		}
			
		{	// Serpent Gate Guard
			var npc = GUARD20;
			if ((npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Bark per existing Origin Usecode.
				delayedBark(npc, "@Keep Away!@", 1);
				// Use his conversation.
				npc->set_schedule_type(TALK);
			}
			if ((!npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Set so if he returns from his HOUND he doesn't stand in the wrong direction.
				script npc {nohalt; face SOUTH;}
			}
		}

		{	// East Big Doors Guard, Northern one.
			var npc = GUARD19;
			if ((npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Make him step towards you, he does not have conversation.
				script npc {nohalt; step WEST; wait 1; step WEST;}
			}
		}

		{	// East Big Doors Guard, Southern one.
			var npc = GUARD18;
			if ((npcNearbyAndVisible(npc)) && (!npc->get_item_flag(SI_ZOMBIE)))
			{
				// Make him step towards you.
				script npc {nohalt; step WEST; wait 1; step WEST;}
				// Bark per existing Origin Usecode.
				delayedBark(npc, "@Intruder!@", 1);
				// Set his next activity to Talk. His conversation triggers GUARD19 to attack too.
				npc->set_schedule_type(TALK);
			}
		}
	SkullcrusherAutomatons.original();
	}
}
