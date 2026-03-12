/*
 *
 *  Copyright (C) 2023  The Exult Team
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
 *	Author: Marzo Junior
 */

void ListFieldReturnGear 0x931 (var obj) {
	// Getting the path egg quality early because it is reset to zero in the
	// original function.
	var pathEgg = getPathEgg(1, 2);
	var quality = pathEgg->get_item_quality();
	// Just in case.
	try {
		ListFieldReturnGear.original(obj);
	} catch (error) {
	}
	if (UI_get_npc_object(BOYDON) == obj) {
		UI_set_item_flag(obj, SI_TOURNAMENT);
	}
	if (quality) {
		var joinables = AVATAR->find_nearby(SHAPE_ANY, 45, MASK_NPC + MASK_INVISIBLE);
		for (npc in joinables) {
			if (npc->get_npc_id() == 13 && !npc->get_item_flag(IN_PARTY)) {
				npc->add_to_party();
				npc->set_npc_id(0);
			}
		}
	}
}


