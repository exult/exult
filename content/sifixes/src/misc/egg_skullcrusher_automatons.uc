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

void SetSkullcrusherAutomaton (var npc, var name, var sched, var loc) {
	npc->set_npc_name(name);
	npc->set_schedule_type(sched);
	npc->set_new_schedules(DAWN, sched, loc);
	npc->move_object(loc, 1);
}

void SkullcrusherAutomatons object#(0x6B9) () {
	if (event == EGG) {
		// First, give them schedules.
		if (!gflags[SKULLCRUSHER_AUTOMATONS]) {
			// Entrance Guard
			SetSkullcrusherAutomaton(GUARD17, "West Guard", STANDTHERE, [0962, 1045]);
			// Exit Guard
			SetSkullcrusherAutomaton(GUARD21, "Exit Guard", PACE_VERTICAL, [1297, 0888]);
			// Serpent Gate Guard
			SetSkullcrusherAutomaton(GUARD20, "North Door Guard", STANDTHERE, [1217, 0887]);
			// East Big Doors Guard, northern one
			SetSkullcrusherAutomaton(GUARD19, "East Door Guard N", STANDTHERE, [1418, 1045]);
			script GUARD19 {
				nohalt;
				face WEST;
			}
			// East Big Doors Guard, southern one
			SetSkullcrusherAutomaton(GUARD18, "East Door Guard S", STANDTHERE, [1418, 1048]);
			script GUARD18 {
				nohalt;
				face WEST;
			}

			// Set the flag so this does not run again. At least one of the eggs is an
			// auto-reset egg, meaning it would keep retrying these changes when hatched.
			gflags[SKULLCRUSHER_AUTOMATONS] = true;
		}

		var npc;
		// Then make them respond to your presence.

		// Entrance Guard
		npc = GUARD17;
		// If he is on-screen and has not been broken and re-created via spell.
		if (npcNearbyAndVisible(npc) && !npc->get_item_flag(SI_ZOMBIE)) {
			// Bark per existing Origin Usecode.
			delayedBark(npc, "@Hold!@", 1);
			// Set his next activity to Talk because he has conversation.
			npc->set_schedule_type(TALK);
		}

		// Exit Guard
		npc = GUARD21;
		if (npcNearbyAndVisible(npc) && !npc->get_item_flag(SI_ZOMBIE)) {
			// Bark per existing Origin Usecode.
			delayedBark(npc, "@Die, intruder!@", 1);
			// Using Chaotic so he'll also attack the Evil Gwani inside.
			// This only happens if he knocks out the whole party.
			npc->set_alignment(CHAOTIC);
			// Set his target to the Avatar.
			npc->set_opponent(AVATAR);
			npc->set_schedule_type(IN_COMBAT);
		}

		// Serpent Gate Guard
		npc = GUARD20;
		if (!npc->get_item_flag(SI_ZOMBIE)) {
			if (npcNearbyAndVisible(npc)) {
				// Bark per existing Origin Usecode.
				delayedBark(npc, "@Keep Away!@", 1);
				// Use his conversation.
				npc->set_schedule_type(TALK);
			} else {
				// Set so if he returns from his HOUND he doesn't stand in the wrong direction.
				script npc {
					nohalt;
					face SOUTH;
				}
			}
		}

		// East Big Doors Guard, Northern one.
		npc = GUARD19;
		if (npcNearbyAndVisible(npc) && !npc->get_item_flag(SI_ZOMBIE)) {
			// Make him step towards you, he does not have a conversation.
			script npc {
				nohalt;
				step WEST;
				wait 1;
				step WEST;
			}
		}

		// East Big Doors Guard, Southern one.
		npc = GUARD18;
		if (npcNearbyAndVisible(npc) && !npc->get_item_flag(SI_ZOMBIE)) {
			// Make him step towards you.
			script npc {
				nohalt;
				step WEST;
				wait 1;
				step WEST;
			}
			// Bark per existing Origin Usecode.
			delayedBark(npc, "@Intruder!@", 1);
			// Set his next activity to Talk. His conversation triggers GUARD19 to attack too.
			npc->set_schedule_type(TALK);
		}

		SkullcrusherAutomatons.original();
	}
}
