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

/*	The original Skullcrusher automatons' usecode does not have STARTED_TALKING
 *	sections, only PROXIMITY, DOUBLECLICK, and SCRIPTED. Combined with new egg
 *	code, this allows them to function as originally intended. Interestingly,
 *	in the leaked Beta they	do function, but rushed production and testing might
 *	be why they did not in the released version.
 *
 *	2016-08-08 Submitted by Knight Captain.
 */

// The first automaton in Skullcrusher is the one just inside the gate, NPC 190.
// To test this, set flag 613 and open the sliding door via the rune match puzzle.
void Guard17 object#(0x4be) () {
	// Entrance Guard
	if (event == STARTED_TALKING) {
		script GUARD17 after 1 ticks {
			nohalt;
			call Guard17;
		}
	}
	Guard17.original();
}

// Guard21 not included here because he has only Barks.

// Automaton by the Serpent Gate, NPC 188.
void Guard20 object#(0x4bc) () {
	// Serpent Gate Guard
	if (event == STARTED_TALKING) {
		script GUARD20 after 3 ticks {
			nohalt;
			call Guard20;
		}
	}
	Guard20.original();
}

// Guard19 not included because he follows Guard18's lead.
// Double-clicking him initiates Guard18's conversation.

// Second guard standing at the Eastern big brass doors that cannot be opened,
// NPC 186. This re-implements his conversation to refer to his hammer not his blade.
void Guard18 object#(0x4ba) () {
	// East Big Doors Guard, southern one
	if (event == DOUBLECLICK && !GUARD18->get_item_flag(SI_ZOMBIE)) {
		delayedBark(AVATAR, "@Hello!@", 5);
		delayedBark(GUARD18, "@Intruder!@", 5);
		GUARD18->set_schedule_type(TALK);
	}

	if ((event == SCRIPTED || event == STARTED_TALKING) && !GUARD18->get_item_flag(SI_ZOMBIE)) {
		GUARD18->run_schedule();
		GUARD18->clear_item_say();
		// Automaton face
		UI_show_npc_face0(298);

		say("@These doors have been sealed by the Serpent of Order, and none shall enter. By approaching thus far, thou hath shown thyself to be mine enemy.@");
		say("@Now I must kill thee, lest the Serpent judge me unworthy. Bless my hammer, master!@");
		delayedBark(GUARD18, "@I shall prevail!@", 3);
		delayedBark(GUARD19, "@Chaos dogs!@", 18);
		attackParty(GUARD18);
		attackParty(GUARD19);
	} else {
		Guard18.original();
	}
}
