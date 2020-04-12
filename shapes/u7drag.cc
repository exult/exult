/**
 ** U7drag.cc - Common defines for drag-and-drop of U7 shapes.
 **
 ** Written: 12/13/00 - JSF
 **/

/*
Copyright (C) 2000 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include "u7drag.h"
#include "utils.h"

/*
 *  Store in char array.
 *
 *  Output: Length of what's stored.
 */

int Store_u7_shapeid(
    unsigned char *data,        // At least 4 bytes.
    int file,           // 0-255.
    int shape,          // 0-0xffff.
    int frame           // 0-255.
) {
	unsigned char *ptr = data;
	*ptr++ = file;
	Write2(ptr, shape);
	*ptr++ = frame;
	return ptr - data;
}

/*
 *  Retrieve shapeid.
 */

void Get_u7_shapeid(
    const unsigned char *data,        // At least 4 bytes.
    int &file,          // 0-255.
    int &shape,         // 0-0xffff.
    int &frame          // 0-255.
) {
	const unsigned char *ptr = data;
	file = *ptr++;
	shape = Read2(ptr);
	frame = *ptr;
}

/*
 *  Store/retrieve chunk #.
 *
 *  Output: Length of what's stored.
 */

int Store_u7_chunkid(
    unsigned char *data,        // At least 4 bytes.
    int cnum            // Chunk #.
) {
	unsigned char *ptr = data;
	Write2(ptr, cnum);
	return ptr - data;
}

void Get_u7_chunkid(
    const unsigned char *data,        // At least 4 bytes.
    int &cnum           // 0-0xffff returned.
) {
	const unsigned char *ptr = data;
	cnum = Read2(ptr);
}

/*
 *  Store/retrieve npc #.
 *
 *  Output: Length of what's stored.
 */

int Store_u7_npcid(
    unsigned char *data,        // At least 4 bytes.
    int npcnum          // Npc #.
) {
	unsigned char *ptr = data;
	Write2(ptr, npcnum);
	return ptr - data;
}

void Get_u7_npcid(
    const unsigned char *data,        // At least 4 bytes.
    int &npcnum         // 0-0xffff returned.
) {
	const unsigned char *ptr = data;
	npcnum = Read2(ptr);
}

/*
 *  Store combo.
 *
 *  Output: Length of data stored.
 */

int Store_u7_comboid(
    unsigned char *data,
    int xtiles, int ytiles,     // Footprint in tiles.
    int tiles_right,        // Tiles to right of hot-spot.
    int tiles_below,        // Tiles below hot-spot.
    int cnt,            // # members.
    U7_combo_data *ents     // The members, with locs. relative to
    //   hot-spot.
) {
	unsigned char *ptr = data;
	Write2(ptr, xtiles);
	Write2(ptr, ytiles);
	Write2(ptr, tiles_right);
	Write2(ptr, tiles_below);
	Write2(ptr, cnt);
	for (int i = 0; i < cnt; i++) {
		Write2(ptr, ents[i].tx);
		Write2(ptr, ents[i].ty);
		Write2(ptr, ents[i].tz);
		Write2(ptr, ents[i].shape);
		*ptr++ = static_cast<unsigned char>(ents[i].frame);
	}
	return ptr - data;
}

/*
 *  Retrieve a combo.
 *
 *  Output: cnt = #elements.
 *      ents = ALLOCATED array of shapes with offsets rel. to hot-spot.
 */

void Get_u7_comboid(
    const unsigned char *data,
    int &xtiles, int &ytiles,   // Footprint in tiles.
    int &tiles_right,       // Tiles to right of hot-spot.
    int &tiles_below,       // Tiles below hot-spot.
    int &cnt,
    U7_combo_data  *&ents
) {
	const unsigned char *ptr = data;
	xtiles = Read2(ptr);
	ytiles = Read2(ptr);
	tiles_right = Read2(ptr);
	tiles_below = Read2(ptr);
	cnt = Read2(ptr);
	ents = new U7_combo_data[cnt];
	for (int i = 0; i < cnt; i++) {
		// Tiles can be negative!
		ents[i].tx = static_cast<short>(Read2(ptr));
		ents[i].ty = static_cast<short>(Read2(ptr));
		ents[i].tz = static_cast<short>(Read2(ptr));
		ents[i].shape = Read2(ptr);
		ents[i].frame = *ptr++;
	}
}
