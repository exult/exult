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

void mightyGoblinDies 0x9B7 () {
	mightyGoblinDies.original();
	if (gflags[CLEARED_FAWN_TOWER]) {
		var objpos = [0x3E3, 0x821, 0x0, QUALITY_ANY, 0];
		var goblineggs = objpos->find_nearby(SHAPE_EGG, 50, 16);
		var index;
		var max;
		var egg;
		for (egg in goblineggs with index to max) {
			script egg setegg EXTERNAL_CRITERIA, 1;
		}
	}
}

void eggFawnTowerCleanup object#(0x72D) () {
	if ((event == EGG) && gflags[CLEARED_FAWN_TOWER]) {
		struct<Position> objpos = [get_object_position(), QUALITY_ANY, 0];
		var goblineggs = objpos->find_nearby(SHAPE_EGG, 50, 16);
		var index;
		var max;

		if (!gflags[BANES_RELEASED]) {
			var dishes = find_nearby(SHAPE_BROKEN_DISH, 50, 0);
			var dish;
			for (dish in dishes with index to max) {
				dish->remove_item();
			}

			var egg;
			for (egg in goblineggs with index to max) {
				script egg setegg EXTERNAL_CRITERIA, 1;
			}
		} else {
			for (egg in goblineggs with index to max) {
				script egg {
					setegg CACHED_IN, 1;
					hatch;
				}
			}
		}
		eggFawnTowerCleanup.original();
	}
}
