/*
 *  find_nearby.h - Header for defining static Game_object::find_nearby()
 *
 *  Copyright (C) 2001-2022 The Exult Team
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

#include "actors.h"
#include "chunks.h"
#include "citerate.h"
#include "flags.h"
#include "gamemap.h"
#include "gamewin.h"
#include "objiter.h"
#include "objs.h"

enum class FindMask {
	Normal         = 0b0000'0000,
	AnyNPC         = 0b0000'0100,
	LiveNPC        = 0b0000'1000,
	EggLike        = 0b0001'0000,
	Invisible      = 0b0010'0000,
	InvisibleParty = 0b0100'0000,
	Transparent    = 0b1000'0000,
};

inline FindMask operator&(FindMask left, FindMask right) {
	return static_cast<FindMask>(
			static_cast<int>(left) & static_cast<int>(right));
}

inline FindMask operator|(FindMask left, FindMask right) {
	return static_cast<FindMask>(
			static_cast<int>(left) | static_cast<int>(right));
}

inline bool operator!(FindMask val) {
	return static_cast<int>(val) == 0;
}

inline bool FlagNotSet(FindMask mask, FindMask value) {
	return !(mask & value);
}

inline bool FlagIsSet(FindMask mask, FindMask value) {
	return !FlagNotSet(mask, value);
}

template <typename VecType, typename Cast>
int Game_object::find_nearby(
		VecType&          vec,         // Objects appended to this.
		const Tile_coord& pos,         // Look near this point.
		int               shapenum,    // Shape to look for.
		//   -1=any (but always use mask?),
		//   c_any_shapenum=any.
		int         delta,        // # tiles to look in each direction.
		int         find_mask,    // See Check_mask() bellow.
		int         qual,         // Quality, or c_any_qual for any.
		int         framenum,     // Frame #, or c_any_framenum for any.
		const Cast& obj_cast,     // Cast functor.
		bool        exclude_okay_to_take) {
	auto mask_ = static_cast<FindMask>(find_mask);
	// Check an object in find_nearby() against the mask.
	auto Check_mask = [](Game_object* obj, FindMask mask) {
		const Shape_info& info = obj->get_info();
		if (FlagIsSet(mask, FindMask::AnyNPC) && !info.is_npc()) {
			return false;
		}
		if (FlagIsSet(mask, FindMask::LiveNPC)
			&& (!info.is_npc() || obj->get_flag(Obj_flags::dead))) {
			return false;
		}
		const Shape_info::Shape_class sclass = info.get_shape_class();
		// Egg/barge?
		if (FlagNotSet(mask, FindMask::EggLike)
			&& (sclass == Shape_info::hatchable
				|| sclass == Shape_info::barge)) {
			return false;
		}
		if (FlagNotSet(mask, FindMask::Transparent) && info.is_transparent()) {
			return false;
		}
		// Invisible object?
		if (obj->get_flag(Obj_flags::invisible)) {
			if (FlagNotSet(mask, FindMask::Invisible)) {
				if (FlagNotSet(mask, FindMask::InvisibleParty)) {
					return false;
				}
				if (!obj->get_flag(Obj_flags::in_party)) {
					return false;
				}
			}
		}
		return true;    // Passed all tests.
	};
	if (delta < 0) {    // +++++Until we check all old callers.
		delta = 24;
	}
	if (shapenum > 0
		&& mask_ == FindMask::AnyNPC) {    // Ignore mask=4 if shape given!
		mask_ = FindMask::Normal;
	}
	int            vecsize = vec.size();
	Game_window*   gwin    = Game_window::get_instance();
	Game_map*      gmap    = gwin->get_map();
	const TileRect bounds(
			(pos.tx - delta + c_num_tiles) % c_num_tiles,
			(pos.ty - delta + c_num_tiles) % c_num_tiles, 1 + 2 * delta,
			1 + 2 * delta);
	// Stay within world.
	Chunk_intersect_iterator next_chunk(bounds);
	TileRect                 tiles;
	int                      cx;
	int                      cy;
	while (next_chunk.get_next(tiles, cx, cy)) {
		// Go through objects.
		Map_chunk* chunk = gmap->get_chunk(cx, cy);
		tiles.x += cx * c_tiles_per_chunk;
		tiles.y += cy * c_tiles_per_chunk;
		Object_iterator next(chunk->get_objects());
		Game_object*    obj;
		while ((obj = next.get_next()) != nullptr) {
			// Check shape.
			if (shapenum >= 0) {
				const bool shape_match = [&]() {
					if (obj->get_shapenum() == shapenum) {
						return true;
					}
					Actor* npc = obj->as_actor();
					return npc != nullptr && npc->get_polymorph() == shapenum;
				}();
				if (!shape_match) {
					continue;
				}
			}
			if (qual != c_any_qual && obj->get_quality() != qual) {
				continue;
			}
			if (framenum != c_any_framenum && obj->get_framenum() != framenum) {
				continue;
			}
			if (!Check_mask(obj, mask_)) {
				continue;
			}
			if (exclude_okay_to_take
				&& obj->get_flag(Obj_flags::okay_to_take)) {
				continue;
			}
			const Tile_coord t = obj->get_tile();
			if (tiles.has_point(t.tx, t.ty)) {
				typename VecType::value_type castobj = obj_cast(obj);
				if (castobj) {
					vec.push_back(castobj);
				}
			}
		}
	}
	// Return # added.
	return vec.size() - vecsize;
}

#endif
