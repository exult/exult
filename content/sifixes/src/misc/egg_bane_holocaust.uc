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
 */

void eggBaneHolocaust object#(0x6B1) () {
	if (gflags[BANES_RELEASED]) {
		var npcnum = -1 * get_item_quality();
		var inn_keepers = [
			JENDON,
			DEVRA, ARGUS,
			ROCCO
		];
		var qualities = [
			11,
			3, 3,
			184
		];
		if (npcnum in inn_keepers) {
			var key = UI_create_new_object(SHAPE_KEY);
			key->set_item_quality(qualities[getIndexForElement(npcnum, inn_keepers)]);
			npcnum->give_last_created();
		}

		eggBaneHolocaust.original();
	}
}
