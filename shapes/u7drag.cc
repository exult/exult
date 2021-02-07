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

#include <cassert>

#define FILE_URI "file://"
static const size_t FileUriLen = strlen(FILE_URI);

/*
 *  Check and skip the URI introducer.
 */
static bool verify_is_uri(
    const unsigned char *&data,
    const char *searched_uri
) {
	const size_t urilen = strlen(searched_uri);
	if (memcmp(data,
	           searched_uri,
	           urilen      ) == 0) {
		data += urilen;
		return true;
	}
	if (memcmp(data,
	           searched_uri + FileUriLen,
	           urilen       - FileUriLen) == 0) {
		data += urilen  - FileUriLen;
		return true;
	}
	return false;
}

/*
 *  Store a Shape : file, shape, frame.
 *
 *  Output: Length of data.
 */

int Store_u7_shapeid(
    unsigned char *data,
    int file,             // 0-0xff.
    int shape,            // 0-0xffff.
    int frame             // 0-0xff.
) {
	return 1 + snprintf(reinterpret_cast<char *>(data), U7DND_DATA_LENGTH(3),
	                    FILE_URI "/%s.%d.%d.%d", U7_TARGET_SHAPEID_NAME, file, shape, frame);
}

/*
 *  Retrieve a Shape : file, shape, frame.
 */

void Get_u7_shapeid(
    const unsigned char *data,
    int &file,            // 0-0xff.
    int &shape,           // 0-0xffff.
    int &frame            // 0-0xff.
) {
	const char *ptr = strchr(reinterpret_cast<const char *>(data), '.');
	sscanf(ptr, ".%d.%d.%d", &file, &shape, &frame);
}

/*
 *  Check a Shape.
 */

bool Is_u7_shapeid(
    const unsigned char *data
) {
	return verify_is_uri(data, FILE_URI "/" U7_TARGET_SHAPEID_NAME ".");
}

/*
 *  Store a Chunk : chunk.
 *
 *  Output: Length of data.
 */

int Store_u7_chunkid(
    unsigned char *data,
    int cnum              // 0-0xffff.
) {
	return 1 + snprintf(reinterpret_cast<char *>(data), U7DND_DATA_LENGTH(1),
	                    FILE_URI "/%s.%d", U7_TARGET_CHUNKID_NAME, cnum);
}

/*
 *  Retrieve a Chunk : chunk.
 */

void Get_u7_chunkid(
    const unsigned char *data,
    int &cnum             // 0-0xffff.
) {
	const char *ptr = strchr(reinterpret_cast<const char *>(data), '.');
	sscanf(ptr, ".%d", &cnum);
}

/*
 *  Check a Chunk.
 */

bool Is_u7_chunkid(
    const unsigned char *data
) {
	return verify_is_uri(data, FILE_URI "/" U7_TARGET_CHUNKID_NAME ".");
}

/*
 *  Store an NPC : npc.
 *
 *  Output: Length of data.
 */

int Store_u7_npcid(
    unsigned char *data,
    int npcnum            // 0-0xffff.
) {
	return 1 + snprintf(reinterpret_cast<char *>(data), U7DND_DATA_LENGTH(1),
	                    FILE_URI "/%s.%d", U7_TARGET_NPCID_NAME, npcnum);
}

/*
 *  Retrieve an NPC.
 */

void Get_u7_npcid(
    const unsigned char *data,
    int &npcnum           // 0-0xffff.
) {
	const char *ptr = strchr(reinterpret_cast<const char *>(data), '.');
	sscanf(ptr, ".%d", &npcnum);
}

/*
 *  Check an NPC.
 */

bool Is_u7_npcid(
    const unsigned char *data
) {
	return verify_is_uri(data, FILE_URI "/" U7_TARGET_NPCID_NAME ".");
}

/*
 *  Store a Combo : x, y, right, below tiles, count of shapes -> each shape : x, y, z, shape, frame.
 *
 *  Output: Length of data.
 */

int Store_u7_comboid(
    unsigned char *data,
    int xtiles,           // 0-0xffff : X - Footprint in tiles.
    int ytiles,           // 0-0xffff : Y - Footprint in tiles.
    int tiles_right,      // 0-0xffff : Tiles to the right of the hot-spot.
    int tiles_below,      // 0-0xffff : Tiles below the hot-spot.
    int cnt,              // 0-0xffff : Number of members.
    U7_combo_data *ents   // The members, with locations relative - can be negative - to hot-spot.
) {
	unsigned char *ptr = data + snprintf(reinterpret_cast<char *>(data), U7DND_DATA_LENGTH(5),
	                                     FILE_URI "/%s.%d.%d.%d.%d.%d", U7_TARGET_COMBOID_NAME,
	                                     xtiles, ytiles, tiles_right, tiles_below, cnt);
	for (int i = 0; i < cnt; i++) {
		ptr = ptr + snprintf(reinterpret_cast<char *>(ptr),
		                     U7DND_DATA_LENGTH(5+(5*cnt)) - (ptr - data),
		                     ".%d.%d.%d.%d.%d",
		                     ents[i].tx, ents[i].ty, ents[i].tz, ents[i].shape, ents[i].frame);
	}
	return ptr + 1 - data;
}

/*
 *  Retrieve a Combo : x, y, right, below tiles, count of shapes -> each shape : x, y, z, shape, frame.
 *
 *  Output: cnt  = count of shapes.
 *          ents = Allocated array of shapes with offsets relative to hot-spot.
 */

void Get_u7_comboid(
    const unsigned char *data,
    int &xtiles,          // 0-0xffff : X - Footprint in tiles.
    int &ytiles,          // 0-0xffff : Y - Footprint in tiles.
    int &tiles_right,     // 0-0xffff : Tiles to the right of the hot-spot.
    int &tiles_below,     // 0-0xffff : Tiles below the hot-spot.
    int &cnt,             // 0-0xffff : Number of members.
    U7_combo_data *&ents  // The members, with locations relative - can be negative - to hot-spot.
) {
	const char *ptr = strchr(reinterpret_cast<const char *>(data), '.');
	int n;
	sscanf(ptr, ".%d.%d.%d.%d.%d%n", &xtiles, &ytiles, &tiles_right, &tiles_below, &cnt, &n);
	ptr += n;
	ents = new U7_combo_data[cnt];
	for (int i = 0; i < cnt; i++) {
		sscanf(ptr, ".%d.%d.%d.%d.%d%n",
		       &ents[i].tx, &ents[i].ty, &ents[i].tz, &ents[i].shape, &ents[i].frame, &n);
		ptr += n;
	}
}

/*
 *  Check a Combo.
 */

bool Is_u7_comboid(
    const unsigned char *data
) {
	return verify_is_uri(data, FILE_URI "/" U7_TARGET_COMBOID_NAME ".");
}
