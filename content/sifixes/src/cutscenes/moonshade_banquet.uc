/*
 *
 *  Copyright (C) 2024  The Exult Team
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
 */

void MoonshadeBanquet shape#(0x32E)() {
	if (FILBERCIO->get_npc_id() == 13) {
		try {
			MoonshadeBanquet.original();
		} catch() {
			if (gflags[POTHOS_RETURNED]) {
				var joinables = getJoinableNPCNumbers();
				for (npc in joinables) {
					if (npc->get_npc_id() == 11
						&& !npc->get_item_flag(IN_PARTY)) {
						npc->add_to_party();
					}
				}
			}
			abort;
		}
	}
	MoonshadeBanquet.original();
}
