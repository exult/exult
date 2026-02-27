/*
 *
 *  Copyright (C) 2023  Marzo Sette Torres Junior
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
 *	This header file defines general constants used throughout usecode, for
 *	Black Gate and Serpent Isle. Constants particular to a function can be
 *	found in the relevant codefile; constants particular to a game can be
 *	found in that game's header directory.
 *
 */

/*
 *	Some structures for manipulation of the returns/parameters of some
 *	intrinsic functions.
 */

// Used in a few functions
struct Position2D {
	var x;
	var y;
}

// Return of get_object_position, and input of several other functions.
struct Position {
	var x;
	var y;
	var z;
}

/*
 *	Coordinate axes - use when referencing X,Y,Z coordinate arrays.
 *	Note that the coordinates returned by UI_click_on_item are 1 array-index
 *	higher, because index 1 of the returned array is the actual item clicked on.
 *	You can resolve this to a regular X,Y,Z coordinates array by using
 *	array = removeFromArray(array, array[1]); (see also bg_externals.uc)
 */
enum axes {
	X = &struct<Position>::x,	//horizontal axis (numbered from west to east)
	Y = &struct<Position>::y,	//vertical axis (numbered from north to south)
	Z = &struct<Position>::z	//lift axis (numbered from ground to sky)
};

// Used by find_nearby
struct FindSpec {
	struct<Position>;
	var quality;
	var framenum;
}

// Defined as return of one BG function and input of another.
struct PosObj {
	struct<Position>;
	var obj;
}

// Return of UI_click_on_item, and possible input of some other functions.
struct ObjPos {
	var obj;
	struct<Position>;
}
