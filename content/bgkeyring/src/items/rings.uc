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
 *	This source file contains a 'fix' for rings which prevents them
 *	from taking effect if you are merely *holding* them.
 *	keyring item.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Invisibility_Ring shape#(0x128) () {
	var cont;
	if (event == READIED) {
		//Get ring container:
		cont = get_container();
		if (cont->is_npc() && (item == cont->get_readied(BG_LEFT_RING) || item == cont->get_readied(BG_RIGHT_RING))) {
			//If the container is an NPC, and if the item is equipped in a finger,
			//make user invisible:
			cont->set_item_flag(INVISIBLE);
		}
	} else if (event == UNREADIED) {
		//Stop invisibility:
		cont = getOuterContainer(item);
		cont->clear_item_flag(INVISIBLE);
	}
}

void Regeneration_Ring shape#(0x12A) () {
	var cont;
	if (event == READIED) {
		//Get ring container:
		cont = get_container();
		if (cont->is_npc() && (item == cont->get_readied(BG_LEFT_RING) || item == cont->get_readied(BG_RIGHT_RING))) {
			//If the container is an NPC, and if the item is equipped in a finger,
			//make user regenerate:
			halt_scheduled();
			script item after 100 ticks
				call Regeneration_Ring;
		}
	} else if (event == UNREADIED) {
		//Stop regeneration:
		halt_scheduled();
	} else if (event == SCRIPTED) {
		//Halt ring scripts:
		halt_scheduled();
		//Get ring container:
		cont = get_container();
		if (cont) {
			//Get Health and Strength:
			var max = cont->get_npc_prop(STRENGTH);
			var current = cont->get_npc_prop(HEALTH);
			if (current < max) {
				//NPC is hurt; regenerate 1 HP:
				cont->set_npc_prop(HEALTH, 1);
				if (UI_die_roll(1, 100) == 1) {
					//1% change for the ring to vanish:
					remove_item();
					return;
				}
			}
			//Call this function again:
			script item after 100 ticks
				call Regeneration_Ring;
		}
	}
}
