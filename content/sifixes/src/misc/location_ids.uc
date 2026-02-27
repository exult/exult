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

enum location_ids {
	THE_WILDERNESS = -1,
	WHITE_DRAGON_CASTLE = 0x0,
	// Defined in "In Wis" usecode, but not in the function below:
	CLAW = 0x1,
	MONK_ISLE = 0x2,
	FAWN = 0x3,
	FURNACE = 0x4,
	GLACIER_MOUNTAINS = 0x5,
	// Defined in "In Wis" usecode, but not in the function below:
	GRAND_SHRINE = 0x6,
	GREAT_NORTHERN_FOREST = 0x7,
	ICE_PLAINS = 0x8,
	SLEEPING_BULL_INN = 0x9,
	ISLE_OF_CRYPTS = 0xA,
	MAD_MAGE_ISLAND = 0xB,
	KNIGHTS_TEST = 0xC,
	MONITOR = 0xD,
	MOONSHADE = 0xE,
	FREEDOM_MOUNTAINS = 0xF,
	SHAMINOS_CASTLE = 0x10,
	DISCIPLINE = 0x11,
	EMOTION = 0x12,
	ENTHUSIASM = 0x13,
	ETHICALITY = 0x14,
	LOGIC = 0x15,
	TOLERANCE = 0x16,
	SKULLCRUSHER_MOUNTAINS = 0x17,
	SPINEBREAKER_MOUNTAINS = 0x18,
	SUNRISE_ISLE = 0x19,
	GORLAB_SWAMP = 0x1A,
	STARTING_AREA = 0x1B,
	WESTERN_FOREST = 0x1C,
	GWANNI_VILLAGE = 0x1D,
	PROGRAMMERS_ISLAND = 0x1E,
	DREAM_WORLD = 0x1F,
	SILVER_SEED = 0x20,
	THE_MAZE = 0x21
};

var getLocationID 0x993 (struct<Position> pos) {
	struct<Position2D> loc2d = [pos.x, pos.y];
	var location_id = THE_WILDERNESS;

	if (pointInsideRect(loc2d, [0x6AE, 0x53C], [0x73F, 0x5AF]) ||
	    pointInsideRect(loc2d, [0x98B, 0x01C], [0x9E3, 0x07E])) {
		location_id = WHITE_DRAGON_CASTLE;
	} else if (pointInsideRect(loc2d, [0x8C4, 0x3A1], [0x9FE, 0x4FE]) ||
	         pointInsideRect(loc2d, [0xA20, 0x400], [0xAFF, 0x4DF])) {
		location_id = MONK_ISLE;
	} else if (pointInsideRect(loc2d, [0xAFA, 0x400], [0xBFF, 0x600]) ||
	         pointInsideRect(loc2d, [0x000, 0x401], [0x0FF, 0x5FF])) {
		location_id = DREAM_WORLD;
	} else if (pointInsideRect(loc2d, [0x35A, 0x5FD], [0x4A3, 0x7D7]) ||
	         pointInsideRect(loc2d, [0xAC0, 0x0C0], [0xACF, 0x0DF])) {
		location_id = FAWN;
	} else if (pointInsideRect(loc2d, [0x480, 0xA00], [0x57F, 0xAAF]) ||
	         pointInsideRect(loc2d, [0x580, 0x970], [0x7CF, 0xABF]) ||
	         pointInsideRect(loc2d, [0x790, 0xAB0], [0x7EC, 0xAFC]) ||
	         pointInsideRect(loc2d, [0xB20, 0x710], [0xBDF, 0x7BC]) ||
	         pointInsideRect(loc2d, [0x00D, 0x61F], [0x0AB, 0x67B]) ||
	         pointInsideRect(loc2d, [0xA0D, 0x05E], [0xA3F, 0x07F]) ||
	         pointInsideRect(loc2d, [0xA33, 0x09F], [0xA5F, 0x0AF])) {
		location_id = FURNACE;
	} else if (pointInsideRect(loc2d, [0xA8F, 0x04F], [0xAE0, 0x0A0])) {
		     location_id = LOGIC;
	} else if (pointInsideRect(loc2d, [0x5C0, 0x1DC], [0x6F8, 0x236]) ||
		     pointInsideRect(loc2d, [0x550, 0x1D0], [0x590, 0x210])) {
		location_id = TOLERANCE;
	} else if (pointInsideRect(loc2d, [0x530, 0x211], [0x640, 0x370])) {
		location_id = GLACIER_MOUNTAINS;
	} else if (pointInsideRect(loc2d, [0x450, 0x4C0], [0x69F, 0x55F]) ||
		     pointInsideRect(loc2d, [0x230, 0x640], [0x320, 0x6B3]) ||
	         pointInsideRect(loc2d, [0x5FA, 0x3FC], [0x6F0, 0x4BF])) {
		location_id = GREAT_NORTHERN_FOREST;
	} else if (pointInsideRect(loc2d, [0x490, 0x860], [0x530, 0x8F0]) ||
		     pointInsideRect(loc2d, [0xAF0, 0x0C0], [0xAFF, 0x0DF])) {
		location_id = SLEEPING_BULL_INN;
	} else if (pointInsideRect(loc2d, [0x070, 0x320], [0x230, 0x410])) {
		location_id = ISLE_OF_CRYPTS;
	} else if (pointInsideRect(loc2d, [0x780, 0x4F0], [0x850, 0x571]) ||
		     pointInsideRect(loc2d, [0x7D0, 0x442], [0x880, 0x4F6])) {
		location_id = MAD_MAGE_ISLAND;
	} else if (pointInsideRect(loc2d, [0x310, 0x800], [0x400, 0x8E8])) {
		location_id = KNIGHTS_TEST;
	} else if (pointInsideRect(loc2d, [0x2A0, 0x980], [0x470, 0xAF0]) ||
		     pointInsideRect(loc2d, [0xB1E, 0x0AE], [0xB30, 0x0D1])) {
		location_id = MONITOR;
	} else if (pointInsideRect(loc2d, [0x834, 0x6BB], [0x9BE, 0x84E])) {
		location_id = MOONSHADE;
	} else if (pointInsideRect(loc2d, [0x7E0, 0x580], [0x8D0, 0x690]) ||
		     pointInsideRect(loc2d, [0x870, 0x520], [0x910, 0x5E0]) ||
		     pointInsideRect(loc2d, [0x8C0, 0x520], [0xA20, 0x591]) ||
	         pointInsideRect(loc2d, [0x960, 0x590], [0xA60, 0x650]) ||
	         pointInsideRect(loc2d, [0x9F0, 0x640], [0xA60, 0x6D0]) ||
	         pointInsideRect(loc2d, [0x9B0, 0x6A0], [0xA10, 0x710])) {
		location_id = FREEDOM_MOUNTAINS;
	} else if (pointInsideRect(loc2d, [0x708, 0x3B4], [0x7BA, 0x454]) ||
		     pointInsideRect(loc2d, [0xB93, 0x043], [0xBEC, 0x0EC])) {
		location_id = SHAMINOS_CASTLE;
	} else if (pointInsideRect(loc2d, [0x6E0, 0x250], [0x72F, 0x2AF]) ||
		     pointInsideRect(loc2d, [0xB00, 0x050], [0xB8F, 0x07F])) {
		location_id = DISCIPLINE;
	} else if (pointInsideRect(loc2d, [0x580, 0x390], [0x5BF, 0x3EF])) {
		location_id = EMOTION;
	} else if (pointInsideRect(loc2d, [0x8C0, 0x120], [0x93F, 0x14F])) {
		location_id = ENTHUSIASM;
	} else if (pointInsideRect(loc2d, [0x8B0, 0x310], [0x8FF, 0x35F]) ||
		     pointInsideRect(loc2d, [0x9A0, 0x300], [0xAFF, 0x38F])) {
		location_id = ETHICALITY;
	} else if (pointInsideRect(loc2d, [0x3A0, 0x3D0], [0x44F, 0x52F]) ||
		     pointInsideRect(loc2d, [0x450, 0x3F0], [0x56F, 0x49F]) ||
	         pointInsideRect(loc2d, [0x4A0, 0x270], [0x50F, 0x3FF]) ||
	         pointInsideRect(loc2d, [0x450, 0x260], [0x4CF, 0x2FF]) ||
	         pointInsideRect(loc2d, [0xB10, 0x800], [0x0F0, 0x8F0]) ||
	         pointInsideRect(loc2d, [0x000, 0x700], [0x0C0, 0x800])) {
		location_id = SKULLCRUSHER_MOUNTAINS;
	} else if (pointInsideRect(loc2d, [0x730, 0x208], [0x95E, 0x2F8]) ||
		     pointInsideRect(loc2d, [0x87E, 0x168], [0x95E, 0x208])) {
		location_id = SPINEBREAKER_MOUNTAINS;
	} else if (pointInsideRect(loc2d, [0x500, 0x000], [0x7EE, 0x190])) {
		location_id = SUNRISE_ISLE;
	} else if (pointInsideRect(loc2d, [0x540, 0x580], [0x5F0, 0x670]) ||
         pointInsideRect(loc2d, [0x5F0, 0x55A], [0x6EA, 0x640])) {
		location_id = GORLAB_SWAMP;
	} else if (pointInsideRect(loc2d, [0x210, 0x950], [0x2D0, 0xAB0])) {
		location_id = STARTING_AREA;
	} else if (pointInsideRect(loc2d, [0x258, 0x47E], [0x30C, 0x5C8])) {
		location_id = WESTERN_FOREST;
	} else if (pointInsideRect(loc2d, [0x37A, 0x33E], [0x488, 0x37A])) {
		location_id = GWANNI_VILLAGE;
	} else if (pointInsideRect(loc2d, [0x000, 0xAFF], [0x900, 0xC00]) ||
		     pointInsideRect(loc2d, [0x8FF, 0x9FF], [0xC00, 0xC00]) ||
		     pointInsideRect(loc2d, [0xB6F, 0x9BF], [0xC00, 0xA00])) {
		// This is in parts of the goblin caves; there was probably something else
		// there at another stage...
		// } else if (pointInsideRect(loc2d, [0x12F, 0x72F], [0x1DF, 0x7C0])) {
		// 	location_id = PROGRAMMERS_ISLAND;
		location_id = SILVER_SEED;
	} else if (pointInsideRect(loc2d, [0xA13, 0x014], [0xA3F, 0x03D]) ||
		     pointInsideRect(loc2d, [0x9D0, 0x0A0], [0x9EE, 0x0AE]) ||
	         pointInsideRect(loc2d, [0x250, 0x33F], [0x330, 0x481])) {
		// Doesn't ever seem to get used in the original (and in fact, it seems to
		// mess up dying in the Maze -- you can end up reviving near Fawn...).
		// I have commented it out, but given the way I placed it, it will never
		// get used anyway...
		// } else if (pointInsideRect(loc2d, [0x8FF, 0x9FF], [0xA00, 0xB00])) {
		// 	location_id = THE_MAZE;
		location_id = THE_WILDERNESS;
	} else if (pos.y < 0x400) {
		location_id = ICE_PLAINS;
	}
	return location_id;
}
