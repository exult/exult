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

void setExchangedItemFlags 0x92B () {
	var item_shapes = exchangedItemList(1);
	var item_qualities = exchangedItemList(2);
	var item_frames = exchangedItemList(3);
	var item_flags = exchangedItemList(4);
	var index = 1;
	var count = (UI_get_array_size(item_shapes) + 1);
	while (index < count) {
		// Iterate through the list of exchanged items and set the flag for
		// each item possessed by the party:
		if (hasItemCount(PARTY, 1, item_shapes[index], item_qualities[index], item_frames[index])) {
			if (item_flags[index] != -1) {
				gflags[item_flags[index]] = true;
			}
		}
		index += 1;
	}
}

var exchangedItemList 0x92C (var index) {
	var rr;
	if (index == 1) {
		return [
			SHAPE_PINECONE, SHAPE_NEST_EGG, SHAPE_REAGENT,
			SHAPE_ALCHEMY_APPARATUS, SHAPE_PUMICE, SHAPE_BRUSH,
			SHAPE_RING, SHAPE_STOCKINGS, SHAPE_FOOD, SHAPE_LARGE_SKULL,
			SHAPE_MONITOR_SHIELD, SHAPE_BOTTLE, SHAPE_URN, SHAPE_BOOTS,
			SHAPE_FILARI, SHAPE_SEVERED_LIMB, SHAPE_LEATHER_HELM,
			SHAPE_BREAST_PLATE
		];
	} else if (index == 2) {
		return [
			false, false, false, false, false, false, false, false, false,
			false, false, false, 255, false, QUALITY_ANY, false, false,
			false
		];
	} else if (index == 3) {
		return [0, 3, 15, 1, 0, 6, 0, 0, 21, 0, 0, 16, 0, 5, FRAME_ANY, 0, 4, 0];
	} else if (index == 4) {
		return [
			STORM_PINECONE, STORM_BLUE_EGG, STORM_RUDDY_ROCK,
			STORM_LAB_APPARATUS, STORM_PUMICE, STORM_GOBLIN_BRUSH,
			STORM_WEDDING_RING, STORM_STOCKINGS, -1, STORM_BEAR_SKULL,
			STORM_MONITOR_SHIELD, STORM_ICEWINE, STORM_URN, STORM_SLIPPERS,
			STORM_FILARI, STORM_SEVERED_HAND, STORM_FUR_CAP,
			STORM_BREAST_PLATE
		];
	}
}
