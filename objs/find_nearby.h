/*
 *  find_nearby.h - Header for defining static Game_object::find_nearby()
 *
 *  Copyright (C) 2001-2013 The Exult Team
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

#ifndef FIND_NEARBY_H
#define FIND_NEARBY_H

#include "citerate.h"
#include "chunks.h"
#include "gamemap.h"
#include "gamewin.h"
#include "objs.h"
#include "objiter.h"
#include "ignore_unused_variable_warning.h"

/*
 *  Check an object in find_nearby() against the mask.
 *
 *  Output: 1 if it passes.
 */
static int Check_mask(
    Game_window *gwin,
    Game_object *obj,
    int mask
) {
	ignore_unused_variable_warning(gwin);
	const Shape_info &info = obj->get_info();
	if ((mask & (4 | 8)) && // Both seem to be all NPC's.
	        !info.is_npc())
		return 0;
	Shape_info::Shape_class sclass = info.get_shape_class();
	// Egg/barge?
	if ((sclass == Shape_info::hatchable || sclass == Shape_info::barge) &&
	        !(mask & 0x10))     // Only accept if bit 16 set.
		return 0;
	if (info.is_transparent() &&    // Transparent?
	        !(mask & 0x80))
		return 0;
	// Invisible object?
	if (obj->get_flag(Obj_flags::invisible))
		if (!(mask & 0x20)) { // Guess:  0x20 == invisible.
			if (!(mask & 0x40)) // Guess:  Inv. party member.
				return 0;
			if (!obj->get_flag(Obj_flags::in_party))
				return 0;
		}

	return 1;           // Passed all tests.
}

template <typename VecType, typename Cast>
int Game_object::find_nearby(
    VecType &vec,           // Objects appended to this.
    Tile_coord const &pos,  // Look near this point.
    int shapenum,           // Shape to look for.
    //   -1=any (but always use mask?),
    //   c_any_shapenum=any.
    int delta,          // # tiles to look in each direction.
    int mask,           // See Check_mask() above.
    int qual,           // Quality, or c_any_qual for any.
    int framenum,           // Frame #, or c_any_framenum for any.
    Cast const &obj_cast,           // Cast functor.
    bool exclude_okay_to_take
) {
	if (delta < 0)          // +++++Until we check all old callers.
		delta = 24;
	if (shapenum > 0 && mask == 4)  // Ignore mask=4 if shape given!
		mask = 0;
	int vecsize = vec.size();
	Game_window *gwin = Game_window::get_instance();
	Game_map *gmap = gwin->get_map();
	Rectangle bounds((pos.tx - delta + c_num_tiles) % c_num_tiles,
	                 (pos.ty - delta + c_num_tiles) % c_num_tiles,
	                 1 + 2 * delta, 1 + 2 * delta);
	// Stay within world.
	Chunk_intersect_iterator next_chunk(bounds);
	Rectangle tiles;
	int cx;
	int cy;
	while (next_chunk.get_next(tiles, cx, cy)) {
		// Go through objects.
		Map_chunk *chunk = gmap->get_chunk(cx, cy);
		tiles.x += cx * c_tiles_per_chunk;
		tiles.y += cy * c_tiles_per_chunk;
		Object_iterator next(chunk->get_objects());
		Game_object *obj;
		while ((obj = next.get_next()) != nullptr) {
			// Check shape.
			if (shapenum >= 0) {
				if (obj->get_shapenum() != shapenum)
					continue;
			}
			if (qual != c_any_qual && obj->get_quality()
			        != qual)
				continue;
			if (framenum !=  c_any_framenum &&
			        obj->get_framenum() != framenum)
				continue;
			if (!Check_mask(gwin, obj, mask))
				continue;
			if (exclude_okay_to_take && obj->get_flag(Obj_flags::okay_to_take))
				continue;
			Tile_coord t = obj->get_tile();
			if (tiles.has_point(t.tx, t.ty)) {
				typename VecType::value_type castobj = obj_cast(obj);
				if (castobj) vec.push_back(castobj);
			}
		}
	}
	// Return # added.
	return vec.size() - vecsize;
}

#endif
