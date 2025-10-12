/**
 ** Chunks.cc - Chunks (16x16 tiles) on the map.
 **
 ** Written: 10/1/98 - JSF
 **/

/*
Copyright (C) 2000-2025 The Exult Team

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
#	include <config.h>
#endif

#include "chunks.h"

#include "actors.h"
#include "animate.h"
#include "chunkter.h"
#include "citerate.h"
#include "databuf.h"
#include "dir.h"
#include "egg.h"
#include "game.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "miscinf.h"
#include "objiter.h"
#include "objs.h"
#include "ordinfo.h"
#include "shapeinf.h"

using std::rand;
using std::vector;

/*
 *  Create the cached data storage for a chunk.
 */

Chunk_cache::Chunk_cache() : obj_list(nullptr), egg_objects(4), eggs{} {}

/*
 *  This mask gives the low bits (b0) for a given # of ztiles.
 */
unsigned long tmasks[8]
		= {0x0L, 0x1L, 0x5L, 0x15L, 0x55L, 0x155L, 0x555L, 0x1555L};

/*
 *  Set (actually, increment count) for a given tile.
 *  Want:   00 => 01,   01 => 10,
 *      10 => 11,   11 => 11.
 *  So: newb0 = !b0 OR b1,
 *      newb1 =  b1 OR b0
 */
inline void Set_blocked_tile(
		Chunk_cache::blocked8z& blocked,    // 16x16 flags,
		int tx, int ty,                     // Tile #'s (0-15).
		int lift,                           // Starting lift to set.
		int ztiles                          // # tiles along z-axis.
) {
	const uint16 val = blocked[ty * c_tiles_per_chunk + tx];
	// Get mask for the bit0's:
	auto         mask0  = static_cast<uint16>(tmasks[ztiles] << 2 * lift);
	const uint16 mask1  = mask0 << 1;    // Mask for the bit1's.
	const uint16 val0s  = val & mask0;
	const uint16 Nval0s = (~val) & mask0;
	const uint16 val1s  = val & mask1;
	const uint16 newval = val1s | (val0s << 1) | Nval0s | (val1s >> 1);
	// Replace old values with new.
	blocked[ty * c_tiles_per_chunk + tx] = (val & ~(mask0 | mask1)) | newval;
}

/*
 *  Clear (actually, decrement count) for a given tile.
 *  Want:   00 => 00,   01 => 00,
 *      10 => 01,   11 => 10.
 *  So: newb0 =  b1 AND !b0
 *      newb1 =  b1 AND  b0
 */
inline void Clear_blocked_tile(
		Chunk_cache::blocked8z& blocked,    // 16x16 flags,
		int tx, int ty,                     // Tile #'s (0-15).
		int lift,                           // Starting lift to set.
		int ztiles                          // # tiles along z-axis.
) {
	const uint16 val = blocked[ty * c_tiles_per_chunk + tx];
	// Get mask for the bit0's:
	auto         mask0  = static_cast<uint16>(tmasks[ztiles] << 2 * lift);
	const uint16 mask1  = mask0 << 1;    // Mask for the bit1's.
	const uint16 val0s  = val & mask0;
	const uint16 Nval0s = (~val) & mask0;
	const uint16 val1s  = val & mask1;
	const uint16 newval = (val1s & (val0s << 1)) | ((val1s >> 1) & Nval0s);
	// Replace old values with new.
	blocked[ty * c_tiles_per_chunk + tx] = (val & ~(mask0 | mask1)) | newval;
}

/*
 *  Create new blocked flags for a given z-level, where each level
 *  covers 8 lifts.
 */

Chunk_cache::blocked8z& Chunk_cache::new_blocked_level(int zlevel) {
	if (static_cast<unsigned>(zlevel) >= blocked.size()) {
		blocked.resize(zlevel + 1);
	}
	blocked[zlevel] = std::make_unique<uint16[]>(256);
	return blocked[zlevel];
}

/*
 *  Set/unset the blocked flags in a region.
 */

void Chunk_cache::set_blocked(
		int startx, int starty,    // Starting tile #'s.
		int endx, int endy,        // Ending tile #'s.
		int lift, int ztiles       // Lift, height info.
) {
	int z = lift;
	while (ztiles) {
		const int zlevel = z / 8;
		const int thisz  = z % 8;
		int       zcnt   = 8 - thisz;
		if (ztiles < zcnt) {
			zcnt = ztiles;
		}
		auto& block = need_blocked_level(zlevel);
		for (int y = starty; y <= endy; y++) {
			for (int x = startx; x <= endx; x++) {
				Set_blocked_tile(block, x, y, thisz, zcnt);
			}
		}
		z += zcnt;
		ztiles -= zcnt;
	}
}

void Chunk_cache::clear_blocked(
		int startx, int starty,    // Starting tile #'s.
		int endx, int endy,        // Ending tile #'s.
		int lift, int ztiles       // Lift, height info.
) {
	int z = lift;
	while (ztiles) {
		const unsigned int zlevel = z / 8;
		const int          thisz  = z % 8;
		int                zcnt   = 8 - thisz;
		if (zlevel >= blocked.size()) {
			break;    // All done.
		}
		if (ztiles < zcnt) {
			zcnt = ztiles;
		}
		auto& block = blocked[zlevel];
		if (block) {
			for (int y = starty; y <= endy; y++) {
				for (int x = startx; x <= endx; x++) {
					Clear_blocked_tile(block, x, y, thisz, zcnt);
				}
			}
		}
		z += zcnt;
		ztiles -= zcnt;
	}
}

/*
 *  Add/remove an object to/from the cache.
 */

void Chunk_cache::update_object(
		Map_chunk* chunk, Game_object* obj,
		bool add    // 1 to add, 0 to remove.
) {
	ignore_unused_variable_warning(chunk);
	const Shape_info& info = obj->get_info();
	if (info.is_door()) {    // Special door list.
		if (add) {
			doors.insert(obj);
		} else {
			doors.erase(obj);
		}
	}
	const int ztiles = info.get_3d_height();
	if (!ztiles || !info.is_solid()) {
		return;    // Skip if not an obstacle.
	}
	// Get lower-right corner of obj.
	const int endx   = obj->get_tx();
	const int endy   = obj->get_ty();
	const int frame  = obj->get_framenum();    // Get footprint dimensions.
	const int xtiles = info.get_3d_xtiles(frame);
	const int ytiles = info.get_3d_ytiles(frame);
	const int lift   = obj->get_lift();
	// Simplest case?
	if (xtiles == 1 && ytiles == 1 && ztiles <= 8 - lift % 8) {
		if (add) {
			Set_blocked_tile(
					need_blocked_level(lift / 8), endx, endy, lift % 8, ztiles);
		} else if (blocked[lift / 8]) {
			Clear_blocked_tile(blocked[lift / 8], endx, endy, lift % 8, ztiles);
		}
		return;
	}
	const TileRect footprint = obj->get_footprint();
	// Go through interesected chunks.
	Chunk_intersect_iterator next_chunk(footprint);
	TileRect                 tiles;
	int                      cx;
	int                      cy;
	while (next_chunk.get_next(tiles, cx, cy)) {
		gmap->get_chunk(cx, cy)->set_blocked(
				tiles.x, tiles.y, tiles.x + tiles.w - 1, tiles.y + tiles.h - 1,
				lift, ztiles, add);
	}
}

/*
 *  Set a rectangle of tiles within this chunk to be under the influence
 *  of a given egg, or clear it.
 */

void Chunk_cache::set_egged(
		Egg_object* egg,
		TileRect&   tiles,    // Range of tiles within chunk.
		bool        add       // 1 to add, 0 to remove.
) {
	// Egg already there?
	int eggnum = -1;
	int spot   = -1;
	for (auto it = egg_objects.begin(); it != egg_objects.end(); ++it) {
		if (*it == egg) {
			eggnum = it - egg_objects.begin();
			break;
		} else if (*it == nullptr && spot == -1) {
			spot = it - egg_objects.begin();
		}
	}
	if (add) {
		if (eggnum < 0) {    // No, so add it.
			eggnum = spot >= 0 ? spot : egg_objects.size();
			if (spot >= 0) {
				egg_objects[spot] = egg;
			} else {
				egg_objects.push_back(egg);
			}
		}
		if (eggnum > 15) {    // We only have 16 bits.
			eggnum = 15;
		}
		const short mask  = (1 << eggnum);
		const int   stopx = tiles.x + tiles.w;
		const int   stopy = tiles.y + tiles.h;
		for (int ty = tiles.y; ty < stopy; ++ty) {
			for (int tx = tiles.x; tx < stopx; ++tx) {
				eggs[ty * c_tiles_per_chunk + tx] |= mask;
			}
		}
	} else {    // Remove.
		if (eggnum < 0) {
			return;    // Not there.
		}
		egg_objects[eggnum] = nullptr;
		if (eggnum >= 15) {    // We only have 16 bits.
			// Last one at 15 or above?
			for (auto it = egg_objects.begin() + 15; it != egg_objects.end();
				 ++it) {
				if (*it != nullptr) {
					// No, so leave bits alone.
					return;
				}
			}
			eggnum = 15;
		}
		const short mask  = ~(1 << eggnum);
		const int   stopx = tiles.x + tiles.w;
		const int   stopy = tiles.y + tiles.h;
		for (int ty = tiles.y; ty < stopy; ty++) {
			for (int tx = tiles.x; tx < stopx; tx++) {
				eggs[ty * c_tiles_per_chunk + tx] &= mask;
			}
		}
	}
}

/*
 *  Add/remove an egg to the cache.
 */

void Chunk_cache::update_egg(
		Map_chunk* chunk, Egg_object* egg,
		bool add    // 1 to add, 0 to remove.
) {
	ignore_unused_variable_warning(chunk);
	// Get footprint with abs. tiles.
	const TileRect foot = egg->get_area();
	if (!foot.w) {
		return;    // Empty (probability = 0).
	}
	TileRect crect;    // Gets tiles within each chunk.
	int      cx;
	int      cy;
	if (egg->is_solid_area()) {
		// Do solid rectangle.
		Chunk_intersect_iterator all(foot);
		while (all.get_next(crect, cx, cy)) {
			gmap->get_chunk(cx, cy)->set_egged(egg, crect, add);
		}
		return;
	}
	// Just do the perimeter.
	const TileRect top(foot.x, foot.y, foot.w, 1);
	const TileRect bottom(foot.x, foot.y + foot.h - 1, foot.w, 1);
	const TileRect left(foot.x, foot.y + 1, 1, foot.h - 2);
	const TileRect right(foot.x + foot.w - 1, foot.y + 1, 1, foot.h - 2);
	// Go through intersected chunks.
	Chunk_intersect_iterator tops(top);
	while (tops.get_next(crect, cx, cy)) {
		gmap->get_chunk(cx, cy)->set_egged(egg, crect, add);
	}
	Chunk_intersect_iterator bottoms(bottom);
	while (bottoms.get_next(crect, cx, cy)) {
		gmap->get_chunk(cx, cy)->set_egged(egg, crect, add);
	}
	Chunk_intersect_iterator lefts(left);
	while (lefts.get_next(crect, cx, cy)) {
		gmap->get_chunk(cx, cy)->set_egged(egg, crect, add);
	}
	Chunk_intersect_iterator rights(right);
	while (rights.get_next(crect, cx, cy)) {
		gmap->get_chunk(cx, cy)->set_egged(egg, crect, add);
	}
}

/*
 *  Create the cached data for a chunk.
 */

void Chunk_cache::setup(Map_chunk* chunk) {
	Game_object*    obj;    // Set 'blocked' tiles.
	Object_iterator next(chunk->get_objects());
	while ((obj = next.get_next()) != nullptr) {
		if (obj->is_egg()) {
			update_egg(chunk, obj->as_egg(), true);
		} else {
			update_object(chunk, obj, true);
		}
	}

	obj_list = chunk;
}

//	Temp. storage for 'blocked' flags for a single tile.
static uint16 tflags[256 / 8];
static int    tflags_maxz;
//	Test for given z-coord. (lift)
#define TEST_TFLAGS(i) (tflags[(i) / 8] & (3 << (2 * ((i) % 8))))

inline void Chunk_cache::set_tflags(int tx, int ty, int maxz) {
	int       zlevel = maxz / 8;
	const int bsize  = blocked.size();
	if (zlevel >= bsize) {
		memset(tflags + bsize, 0, (zlevel - bsize + 1) * sizeof(uint16));
		zlevel = bsize - 1;
	}
	while (zlevel >= 0) {
		auto& block      = blocked[zlevel];
		tflags[zlevel--] = block ? block[ty * c_tiles_per_chunk + tx] : 0;
	}
	tflags_maxz = maxz;
}

/*
 *  Get highest blocked lift below a given level for a given tile.
 *
 *  Output: Highest lift that's blocked by an object, or -1 if none.
 */

inline int Chunk_cache::get_highest_blocked(int lift    // Look below this lift.
) {
	int i;    // Look downwards.
	for (i = lift - 1; i >= 0 && !TEST_TFLAGS(i); i--)
		;
	return i;
}

/*
 *  Get highest blocked lift below a given level for a given tile.
 *
 *  Output: Highest lift that's blocked by an object, or -1 if none.
 */

int Chunk_cache::get_highest_blocked(
		int lift,         // Look below this lift.
		int tx, int ty    // Square to test.
) {
	set_tflags(tx, ty, lift);
	return get_highest_blocked(lift);
}

/*
 *  Get lowest blocked lift above a given level for a given tile.
 *
 *  Output: Highest lift that's blocked by an object, or -1 if none.
 */

inline int Chunk_cache::get_lowest_blocked(int lift    // Look above this lift.
) {
	int i;    // Look upward.
	for (i = lift; i <= tflags_maxz && !TEST_TFLAGS(i); i++)
		;
	if (i > tflags_maxz) {
		return -1;
	}
	return i;
}

/*
 *  Get lowest blocked lift above a given level for a given tile.
 *
 *  Output: Lowest lift that's blocked by an object, or -1 if none.
 */

int Chunk_cache::get_lowest_blocked(
		int lift,         // Look above this lift.
		int tx, int ty    // Square to test.
) {
	set_tflags(tx, ty, 255);    // FOR NOW, look up to max.
	return get_lowest_blocked(lift);
}

/*
 *  See if a tile is water or land.
 */

inline int Check_terrain(
		Map_chunk* nlist,    // Chunk.
		int tx, int ty       // Tile within chunk.
							 //   bit2 if solid.
) {
	ShapeID flat = nlist->get_flat(tx, ty);
	// Sets: bit0 if land, bit1 if water,
	int terrain = 0;
	if (!flat.is_invalid()) {
		if (flat.get_info().is_water()) {
			terrain |= 2;
		} else if (flat.get_info().is_solid()) {
			terrain |= 4;
		} else {
			terrain |= 1;
		}
	}
	return terrain;
}

/*
 *  Is a given square occupied at a given lift?
 *
 *  Output: true if so, else false.
 *      If false (tile is free), new_lift contains the new height that
 *         an actor will be at if he walks onto the tile.
 */

bool Chunk_cache::is_blocked(
		int height,    // Height (in tiles) of obj. being
		//   tested.
		int lift,              // Given lift.
		int tx, int ty,        // Square to test.
		int&      new_lift,    // New lift returned.
		const int move_flags,
		int       max_drop,    // Max. drop/rise allowed.
		int       max_rise     // Max. rise, or -1 to use old beha-
							   //   viour (max_drop if FLY, else 1).
) {
	const bool is_ethereal   = (move_flags & MOVE_ETHEREAL) != 0;
	const bool in_mapedit    = (move_flags & MOVE_MAPEDIT) != 0;
	const bool can_walk      = (move_flags & MOVE_WALK) != 0;
	const bool can_swim      = (move_flags & MOVE_SWIM) != 0;
	const bool can_fly       = (move_flags & MOVE_FLY) != 0;
	const bool is_levitating = (move_flags & MOVE_LEVITATE) != 0;
	// Ethereal beings always return not blocked
	// and can only move horizontally
	if (is_ethereal) {
		new_lift = lift;
		return false;
	}
	// Figure max lift allowed.
	if (max_rise == -1) {
		if (in_mapedit || can_fly) {
			max_rise = max_drop;
		} else if (can_walk) {
			max_rise = 1;
		} else {
			// Swim.
			max_rise = 0;
		}
	}
	int max_lift = lift + max_rise;
	if (max_lift > 255) {
		max_lift = 255;    // As high as we can go.
	}
	set_tflags(tx, ty, max_lift + height);
	for (new_lift = lift; new_lift <= max_lift; new_lift++) {
		if (!TEST_TFLAGS(new_lift)) {
			// Not blocked?
			const int new_high = get_lowest_blocked(new_lift);
			// Not blocked above?
			if (new_high == -1 || new_high >= (new_lift + height)) {
				break;    // Okay.
			}
		}
	}
	if (new_lift > max_lift) {    // Spot not found at lift or higher?
		// Look downwards.
		new_lift = get_highest_blocked(lift) + 1;
		if (new_lift >= lift) {    // Couldn't drop?
			return true;
		}
		const int new_high = get_lowest_blocked(new_lift);
		if (new_high != -1 && new_high < new_lift + height) {
			return true;    // Still blocked above.
		}
	}
	if (new_lift <= lift) {    // Not going up?  See if falling.
		new_lift = is_levitating ? lift : get_highest_blocked(lift) + 1;
		// Don't allow fall of > max_drop.
		if (lift - new_lift > max_drop) {
			// Map-editing?  Suspend in air there.
			if (in_mapedit) {
				new_lift = lift - max_drop;
			} else {
				return true;
			}
		}
		const int new_high = get_lowest_blocked(new_lift);

		// Make sure that where we want to go is tall enough for us
		if (new_high != -1 && new_high < (new_lift + height)) {
			return true;
		}
	}

	// Found a new place to go, lets test if we can actually move there

	// Lift 0 tests
	if (new_lift == 0) {
		if (in_mapedit) {
			return false;    // Map-editor, so anything is okay.
		}
		if (!can_walk && !can_swim && !can_fly) {
			// Cannot move at all, like Reapers in BG.
			return true;
		}
		const int ter = Check_terrain(obj_list, tx, ty);
		if (can_swim && !can_walk && !can_fly && (ter & 2) == 0) {
			// Can only swim; do not allow to move outside of water.
			return true;
		}
		if (can_walk && !can_swim && !can_fly && (ter & 2) != 0) {
			// Can only walk; do not allow to move into water.
			return true;
		}
		if (!can_swim && !can_fly && (ter & 4) != 0) {
			// Can only walk and terrain is solid (and 0-height).
			return true;
		}
		return false;
	}
	// TODO: maybe worth checking for swim here as well?
	return !can_walk && !can_fly;
}

/*
 *  Activate nearby eggs.
 */

void Chunk_cache::activate_eggs(
		Game_object* obj,            // Object (actor) that's near.
		Map_chunk*   chunk,          // Chunk this is attached to.
		int tx, int ty, int tz,      // Tile (absolute).
		int from_tx, int from_ty,    // Tile walked from.
		unsigned short eggbits,      // Eggs[tile].
		bool           now           // Do them immediately.
) {
	// Ensure we exist until the end of the function.
	auto   ownHandle = shared_from_this();
	size_t i;    // Go through eggs.
	for (i = 0; i < 8 * sizeof(eggbits) - 1 && eggbits;
		 i++, eggbits = eggbits >> 1) {
		Egg_object* egg;
		if ((eggbits & 1) && i < egg_objects.size() && (egg = egg_objects[i])
			&& egg->is_active(obj, tx, ty, tz, from_tx, from_ty)) {
			egg->hatch(obj, now);
			if (chunk->get_cache() != this) {
				return;    // A teleport could have deleted us!
			}
		}
	}
	if (eggbits) {    // Check 15th bit.
					  // DON'T use an iterator here, since
					  //   the list can change as eggs are
					  //   activated, causing a CRASH!
		const size_t sz = egg_objects.size();
		for (; i < sz; i++) {
			Egg_object* egg = egg_objects[i];
			if (egg && egg->is_active(obj, tx, ty, tz, from_tx, from_ty)) {
				egg->hatch(obj, now);
				if (chunk->get_cache() != this) {
					return;    // A teleport could have deleted us!
				}
			}
		}
	}
}

void Chunk_cache::unhatch_eggs(
		Game_object* obj,            // Object (actor) that's near.
		Map_chunk*   chunk,          // Chunk this is attached to.
		int tx, int ty, int tz,      // Tile (absolute).
		int from_tx, int from_ty,    // Tile walked from.
		unsigned short eggbits,      // Eggs[tile].
		bool           now           // Do them immediately.
) {
	// Ensure we exist until the end of the function.
	auto ownHandle = shared_from_this();

	size_t i;    // Go through eggs.
	for (i = 0; i < 8 * sizeof(eggbits) - 1 && eggbits;
		 i++, eggbits = eggbits >> 1) {
		Egg_object* egg;
		if ((eggbits & 1) && i < egg_objects.size() && (egg = egg_objects[i])
			&& egg->test_unhatch(obj, tx, ty, tz, from_tx, from_ty)) {
			egg->unhatch(obj, now);
			if (chunk->get_cache() != this) {
				return;    // A teleport could have deleted us!
			}
		}
	}
	if (eggbits) {    // Check 15th bit.
					  // DON'T use an iterator here, since
					  //   the list can change as eggs are
					  //   activated, causing a CRASH!
		const size_t sz = egg_objects.size();
		i               = 0;    // Go through eggs.
		for (; i < sz; i++) {
			Egg_object* egg = egg_objects[i];
			if (egg && egg->test_unhatch(obj, tx, ty, tz, from_tx, from_ty)) {
				egg->unhatch(obj, now);
			}
			if (chunk->get_cache() != this) {
				return;    // A teleport could have deleted us!
			}
		}
	}
}

/*
 *  Find door blocking a given tile.
 */

Game_object* Chunk_cache::find_door(const Tile_coord& tile) {
	for (auto* door : doors) {
		if (door->blocks(tile)) {
			return door;    // Found it.
		}
	}
	return nullptr;
}

/*
 *  Create list for a given chunk.
 */

Map_chunk::Map_chunk(
		Game_map* m,              // Map we'll belong to.
		int chunkx, int chunky    // Absolute chunk coords.
		)
		: map(m), terrain(nullptr), objects(nullptr), first_nonflat(nullptr),
		  from_below(0), from_right(0), from_below_right(0), ice_dungeon(0x00),
		  dungeon_levels(nullptr), cache(nullptr), roof(0), cx(chunkx),
		  cy(chunky), selected(false) {}

/*
 *  Set terrain.  Even if the terrain is the same, it still reloads the
 *  'flat' objects.
 */

void Map_chunk::set_terrain(Chunk_terrain* ter) {
	if (terrain) {
		terrain->remove_client();
		// Remove objs. from terrain.
		Game_object_vector removes;
		{
			// Separate scope for Object_iterator.
			Object_iterator it(get_objects());
			Game_object*    each;
			while ((each = it.get_next()) != nullptr) {
				// Kind of nasty, I know:
				if (each->as_terrain()) {
					removes.push_back(each);
				}
			}
		}
		for (auto* remove : removes) {
			// We don't want to edit the chunks here:
			remove->Game_object::remove_this();
		}
	}
	terrain = ter;
	terrain->add_client();
	// Get RLE objects in chunk.
	for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++) {
		for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++) {
			ShapeID      id    = ter->get_flat(tilex, tiley);
			Shape_frame* shape = id.get_shape();
			if (shape && shape->is_rle()) {
				int                      shapenum = id.get_shapenum();
				int                      framenum = id.get_framenum();
				const Shape_info&        info     = id.get_info();
				const Game_object_shared obj
						= info.is_animated()
								  ? std::make_shared<Animated_object>(
											shapenum, framenum, tilex, tiley)
								  : std::make_shared<Terrain_game_object>(
											shapenum, framenum, tilex, tiley);
				add(obj.get());
			}
		}
	}
}

/*
 *  Add rendering dependencies for a new object.
 */

void Map_chunk::add_dependencies(
		Game_object*   newobj,    // Object to add.
		Ordering_info& newinfo    // Info. for new object's ordering.
) {
	Game_object*            obj;    // Figure dependencies.
	Nonflat_object_iterator next(this);
	while ((obj = next.get_next()) != nullptr) {
		// cout << "Here " << __LINE__ << " " << obj << endl;
		/* Compare returns -1 if lt, 0 if dont_care, 1 if gt. */
		int cmp = Game_object::compare(newinfo, obj);
		// TODO: Fix this properly, instead of with an ugly hack.
		// This fixes relative ordering between the Y depression and the Y
		// shapes in SI. Done so in a way that the depression is not clickable.
		if (!cmp && GAME_SI && newobj->get_shapenum() == 0xd1
			&& obj->get_shapenum() == 0xd1 && obj->get_framenum() == 17) {
			cmp = 1;
		}
		if (cmp == 1) {    // Bigger than this object?
			newobj->dependencies.insert(obj);
			obj->dependors.insert(newobj);
		} else if (cmp == -1) {    // Smaller than?
			obj->dependencies.insert(newobj);
			newobj->dependors.insert(obj);
		}
	}
}

/*
 *  Add rendering dependencies for a new object to another chunk.
 *  NOTE:  This is static.
 *
 *  Output: ->chunk that was checked.
 */

inline Map_chunk* Map_chunk::add_outside_dependencies(
		int cx, int cy,           // Chunk to check.
		Game_object*   newobj,    // Object to add.
		Ordering_info& newinfo    // Info. for new object's ordering.
) {
	Map_chunk* chunk = gmap->get_chunk(cx, cy);
	chunk->add_dependencies(newobj, newinfo);
	return chunk;
}

/*
 *  Add a game object to a chunk's list.
 *
 *  Newobj's chunk field is set to this chunk.
 */

void Map_chunk::add(Game_object* newobj    // Object to add.
) {
	newobj->chunk = this;    // Set object's chunk.
	Ordering_info            ord(gwin, newobj);
	const Game_object_shared newobj_shared = newobj->shared_from_this();
	// Put past flats.
	if (first_nonflat) {
		objects.insert_before(newobj_shared, first_nonflat);
	} else {
		objects.append(newobj_shared);
	}
	// Not flat?
	if (newobj->get_lift() || ord.info.get_3d_height()) {
		// Deal with dependencies.
		// First this chunk.
		add_dependencies(newobj, ord);
		if (from_below) {    // Overlaps from below?
			add_outside_dependencies(cx, INCR_CHUNK(cy), newobj, ord);
		}
		if (from_right) {    // Overlaps from right?
			add_outside_dependencies(INCR_CHUNK(cx), cy, newobj, ord);
		}
		if (from_below_right) {
			add_outside_dependencies(
					INCR_CHUNK(cx), INCR_CHUNK(cy), newobj, ord);
		}
		// See if newobj extends outside.
		/* Let's try boundary. YES.  This helps with statues through roofs!*/
		const bool ext_left  = (newobj->get_tx() - ord.xs) < 0 && cx > 0;
		const bool ext_above = (newobj->get_ty() - ord.ys) < 0 && cy > 0;
		if (ext_left) {
			add_outside_dependencies(DECR_CHUNK(cx), cy, newobj, ord)
					->from_right++;
			if (ext_above) {
				add_outside_dependencies(
						DECR_CHUNK(cx), DECR_CHUNK(cy), newobj, ord)
						->from_below_right++;
			}
		}
		if (ext_above) {
			add_outside_dependencies(cx, DECR_CHUNK(cy), newobj, ord)
					->from_below++;
		}
		first_nonflat = newobj;    // Inserted before old first_nonflat.
	}
	if (cache) {    // Add to cache.
		cache->update_object(this, newobj, true);
	}
	// Count light sources.
	if (ord.info.get_object_light(newobj->get_framenum()) > 0) {
		if (dungeon_levels && is_dungeon(newobj->get_tx(), newobj->get_ty())) {
			dungeon_lights.insert(newobj);
		} else {
			non_dungeon_lights.insert(newobj);
		}
	}
	if (newobj->get_lift() >= 5) {    // Looks like a roof?
		if (ord.info.get_shape_class() == Shape_info::building) {
			roof = 1;
		}
	}
}

/*
 *  Add an egg.
 */

void Map_chunk::add_egg(Egg_object* egg) {
	add(egg);    // Add it normally.
	egg->set_area();
	// Messed up Moonshade after Banes if (cache)       // Add to cache.
	need_cache()->update_egg(this, egg, true);
}

/*
 *  Remove an egg.
 */

void Map_chunk::remove_egg(Egg_object* egg) {
	if (cache) {    // Remove from cache.
		cache->update_egg(this, egg, false);
	}
	remove(egg);    // Remove it normally.
}

/*
 *  Remove a game object from this list.  The object's 'chunk' field
 *  is set to nullptr.
 */

void Map_chunk::remove(Game_object* remove) {
	assert(remove->get_chunk() == this);
	if (cache) {    // Remove from cache.
		cache->update_object(this, remove, false);
	}
	remove->clear_dependencies();    // Remove all dependencies.
	Game_map*         gmap = gwin->get_map();
	const Shape_info& info = remove->get_info();
	// See if it extends outside.
	const int frame = remove->get_framenum();
	const int tx    = remove->get_tx();
	const int ty    = remove->get_ty();
	/* Let's try boundary. YES.  Helps with statues through roofs. */
	const bool ext_left  = (tx - info.get_3d_xtiles(frame)) < 0 && cx > 0;
	const bool ext_above = (ty - info.get_3d_ytiles(frame)) < 0 && cy > 0;
	if (ext_left) {
		gmap->get_chunk(cx - 1, cy)->from_below_right--;
		if (ext_above) {
			gmap->get_chunk(cx - 1, cy - 1)->from_below_right--;
		}
	}
	if (ext_above) {
		gmap->get_chunk(cx, cy - 1)->from_below--;
	}
	if (info.get_object_light(frame) > 0) {    // Count light sources.
		if (dungeon_levels && is_dungeon(tx, ty)) {
			dungeon_lights.erase(remove);
		} else {
			non_dungeon_lights.erase(remove);
		}
	}
	if (remove == first_nonflat) {    // First nonflat?
		// Update.
		first_nonflat = remove->get_next();
		if (first_nonflat == objects.get_first()) {
			first_nonflat = nullptr;
		}
	}
	remove->set_invalid();                         // No longer part of world.
	objects.remove(remove->shared_from_this());    // Remove from list.
}

/*
 *  Is a given rectangle of tiles blocked at a given lift?
 *
 *  Output: true if so, else false.
 *      If false (tile is free), new_lift contains the new height that
 *         an actor will be at if he walks onto the tile.
 */

bool Map_chunk::is_blocked(
		int height,                // Height (along lift) to check.
		int lift,                  // Starting lift.
		int startx, int starty,    // Starting tile coords.
		int xtiles, int ytiles,    // Width, height in tiles.
		int&      new_lift,        // New lift returned.
		const int move_flags,
		int       max_drop,    // Max. drop/rise allowed.
		int       max_rise     // Max. rise, or -1 to use old beha-
							   //   viour (max_drop if FLY, else 1).
) {
	Game_map* gmap = gwin->get_map();
	int       tx;
	int       ty;
	new_lift = 0;
	startx   = (startx + c_num_tiles) % c_num_tiles;    // Watch for wrapping.
	starty   = (starty + c_num_tiles) % c_num_tiles;
	const int stopy = (starty + ytiles) % c_num_tiles;
	const int stopx = (startx + xtiles) % c_num_tiles;
	for (ty = starty; ty != stopy; ty = INCR_TILE(ty)) {
		// Get y chunk, tile-in-chunk.
		const int cy  = ty / c_tiles_per_chunk;
		const int rty = ty % c_tiles_per_chunk;
		for (tx = startx; tx != stopx; tx = INCR_TILE(tx)) {
			int        this_lift;
			Map_chunk* olist = gmap->get_chunk(tx / c_tiles_per_chunk, cy);
			olist->setup_cache();
			if (olist->is_blocked(
						height, lift, tx % c_tiles_per_chunk, rty, this_lift,
						move_flags, max_drop, max_rise)) {
				return true;
			}
			// Take highest one.
			new_lift = this_lift > new_lift ? this_lift : new_lift;
		}
	}
	return false;
}

/*
 *  Check an absolute tile position.
 *
 *  Output: true if blocked, false otherwise.
 *      Tile.tz may be updated for stepping onto square.
 */

bool Map_chunk::is_blocked(
		Tile_coord& tile,
		int         height,    // Height in tiles to check.
		const int   move_flags,
		int         max_drop,    // Max. drop/rise allowed.
		int         max_rise     // Max. rise, or -1 to use old beha-
								 //   viour (max_drop if FLY, else 1).
) {
	// Get chunk tile is in.
	Game_map*  gmap  = gwin->get_map();
	Map_chunk* chunk = gmap->get_chunk_safely(
			tile.tx / c_tiles_per_chunk, tile.ty / c_tiles_per_chunk);
	if (!chunk) {        // Outside the world?
		return false;    // Then it's not blocked.
	}
	chunk->setup_cache();    // Be sure cache is present.
	int new_lift;            // Check it within chunk.
	if (chunk->is_blocked(
				height, tile.tz, tile.tx % c_tiles_per_chunk,
				tile.ty % c_tiles_per_chunk, new_lift, move_flags, max_drop,
				max_rise)) {
		return true;
	}
	tile.tz = new_lift;
	return false;
}

/*
 *  This one is used to see if an object of dims. possibly > 1X1 can
 *  step onto an adjacent square.
 */

bool Map_chunk::is_blocked(
		// Object dims:
		int xtiles, int ytiles, int ztiles,
		const Tile_coord& from,    // Stepping from here.
		Tile_coord&       to,      // Stepping to here.  Tz updated.
		const int         move_flags,
		int               max_drop,    // Max drop/rise allowed.
		int               max_rise     // Max. rise, or -1 to use old beha-
									   //   viour (max_drop if FLY, else 1).
) {
	int vertx0;
	int vertx1;    // Get x-coords. of vert. block
	//   to right/left.
	int horizx0;
	int horizx1;    // Get x-coords of horiz. block
	//   above/below.
	int verty0;
	int verty1;    // Get y-coords of horiz. block
	//   above/below.
	int horizy0;
	int horizy1;    // Get y-coords of vert. block
	//   to right/left.
	// !Watch for wrapping.
	horizx0 = (to.tx + 1 - xtiles + c_num_tiles) % c_num_tiles;
	horizx1 = INCR_TILE(to.tx);
	if (Tile_coord::gte(to.tx, from.tx)) {    // Moving right?
		// Start to right of hot spot.
		vertx0 = INCR_TILE(from.tx);
		vertx1 = INCR_TILE(to.tx);    // Stop past dest.
	} else {                          // Moving left?
		vertx0 = (to.tx + 1 - xtiles + c_num_tiles) % c_num_tiles;
		vertx1 = (from.tx + 1 - xtiles + c_num_tiles) % c_num_tiles;
	}
	verty0 = (to.ty + 1 - ytiles + c_num_tiles) % c_num_tiles;
	verty1 = INCR_TILE(to.ty);
	if (Tile_coord::gte(to.ty, from.ty)) {    // Moving down?
		// Start below hot spot.
		horizy0 = INCR_TILE(from.ty);
		horizy1 = INCR_TILE(to.ty);    // End past dest.
		if (to.ty != from.ty) {        // Includes bottom of vert. area.
			verty1 = DECR_TILE(verty1);
		}
	} else {    // Moving up?
		horizy0 = (to.ty + 1 - ytiles + c_num_tiles) % c_num_tiles;
		horizy1 = (from.ty + 1 - ytiles + c_num_tiles) % c_num_tiles;
		// Includes top of vert. area.
		verty0 = INCR_TILE(verty0);
	}
	int x;
	int y;    // Go through horiz. part.
	int new_lift  = from.tz;
	int new_lift0 = -1;    // All lift changes must be same.
#ifdef DEBUG
	assert(Tile_coord::gte(horizy1, horizy0));
	assert(Tile_coord::gte(horizx1, horizx0));
	assert(Tile_coord::gte(verty1, verty0));
	assert(Tile_coord::gte(vertx1, vertx0));
#endif
	for (y = horizy0; y != horizy1; y = INCR_TILE(y)) {
		// Get y chunk, tile-in-chunk.
		const int cy  = y / c_tiles_per_chunk;
		const int rty = y % c_tiles_per_chunk;
		for (x = horizx0; x != horizx1; x = INCR_TILE(x)) {
			Map_chunk* olist = gmap->get_chunk(x / c_tiles_per_chunk, cy);
			olist->setup_cache();
			const int rtx = x % c_tiles_per_chunk;
			if (olist->is_blocked(
						ztiles, from.tz, rtx, rty, new_lift, move_flags,
						max_drop, max_rise)) {
				return true;
			}
			if (new_lift != from.tz) {
				if (new_lift0 == -1) {
					new_lift0 = new_lift;
				} else if (new_lift != new_lift0) {
					return true;
				}
			}
		}
	}
	// Do vert. block.
	for (x = vertx0; x != vertx1; x = INCR_TILE(x)) {
		// Get x chunk, tile-in-chunk.
		const int cx  = x / c_tiles_per_chunk;
		const int rtx = x % c_tiles_per_chunk;
		for (y = verty0; y != verty1; y = INCR_TILE(y)) {
			Map_chunk* olist = gmap->get_chunk(cx, y / c_tiles_per_chunk);
			olist->setup_cache();
			const int rty = y % c_tiles_per_chunk;
			if (olist->is_blocked(
						ztiles, from.tz, rtx, rty, new_lift, move_flags,
						max_drop, max_rise)) {
				return true;
			}
			if (new_lift != from.tz) {
				if (new_lift0 == -1) {
					new_lift0 = new_lift;
				} else if (new_lift != new_lift0) {
					return true;
				}
			}
		}
	}
	to.tz = new_lift;
	return false;    // All clear.
}

/*
 *  Get the list of tiles in a square perimeter around a given tile.
 *
 *  Output: List (8*dist) of tiles, starting in Northwest corner and going
 *         clockwise.  List is on heap.
 */

static auto Get_square(
		Tile_coord& pos,    // Center of square.
		int         dist    // Distance to perimeter (>0)
) {
	std::vector<Tile_coord> square;
	square.reserve(8 * dist);
	// Upper left corner:
	square.emplace_back(
			DECR_TILE(pos.tx, dist), DECR_TILE(pos.ty, dist), pos.tz);
	const int len = 2 * dist + 1;
	int       out = 1;
	for (int i = 1; i < len; i++, out++) {
		const auto& back = square.back();
		square.emplace_back(INCR_TILE(back.tx), back.ty, pos.tz);
	}
	// Down right side.
	for (int i = 1; i < len; i++, out++) {
		const auto& back = square.back();
		square.emplace_back(back.tx, INCR_TILE(back.ty), pos.tz);
	}
	// Bottom, going back to left.
	for (int i = 1; i < len; i++, out++) {
		const auto& back = square.back();
		square.emplace_back(DECR_TILE(back.tx), back.ty, pos.tz);
	}
	// Left side, going up.
	for (int i = 1; i < len - 1; i++, out++) {
		const auto& back = square.back();
		square.emplace_back(back.tx, DECR_TILE(back.ty), pos.tz);
	}
	return square;
}

/*
 *  Check a spot against the 'where' paramater to find_spot.
 *
 *  Output: true if it passes.
 */

inline bool Check_spot(
		Map_chunk::Find_spot_where where, int tx, int ty, int tz) {
	Game_map*  gmap  = Game_window::get_instance()->get_map();
	const int  cx    = tx / c_tiles_per_chunk;
	const int  cy    = ty / c_tiles_per_chunk;
	Map_chunk* chunk = gmap->get_chunk_safely(cx, cy);
	if (!chunk) {
		return false;
	}
	return (where == Map_chunk::inside)
		   == (chunk->is_roof(
					   tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, tz)
			   < 31);
}

/*
 *  Find a free area for an object of a given shape, looking outwards.
 *
 *  Output: Tile if successful, else (-1, -1, -1).
 */

Tile_coord Map_chunk::find_spot(
		Tile_coord pos,     // Starting point.
		int        dist,    // Distance to look outwards.  (0 means
		//   only check 'pos'.
		int shapenum,    // Shape, frame to find spot for.
		int framenum,
		int max_drop,    // Allow to drop by this much.
		int dir,         // Preferred direction (0-7), or -1 for
		//   random.
		Find_spot_where where    // Inside/outside.
) {
	const Shape_info& info = ShapeID::get_info(shapenum);
	const int         xs   = info.get_3d_xtiles(framenum);
	const int         ys   = info.get_3d_ytiles(framenum);
	const int         zs   = info.get_3d_height();
	// The 'MOVE_FLY' flag really means
	//   we can look upwards by max_drop.
	const int mflags = MOVE_WALK | MOVE_FLY;
	int       new_lift;
	// Start with original position.
	if (!Map_chunk::is_blocked(
				zs, pos.tz, pos.tx - xs + 1, pos.ty - ys + 1, xs, ys, new_lift,
				mflags, max_drop)) {
		return Tile_coord(pos.tx, pos.ty, new_lift);
	}
	if (dir < 0) {
		dir = rand() % 8;    // Choose dir. randomly.
	}
	dir = (dir + 1) % 8;                 // Make NW the 0 point.
	for (int d = 1; d <= dist; d++) {    // Look outwards.
		const int square_cnt = 8 * d;    // # tiles in square's perim.
										 // Get square (starting in NW).
		const auto square = Get_square(pos, d);
		int        index  = dir * d;    // Get index of preferred spot.
		// Get start of preferred range.
		index = (index - d / 2 + square_cnt) % square_cnt;
		for (int cnt = square_cnt; cnt; cnt--, index++) {
			const Tile_coord& p = square[index % square_cnt];
			if (!Map_chunk::is_blocked(
						zs, p.tz, p.tx - xs + 1, p.ty - ys + 1, xs, ys,
						new_lift, mflags, max_drop)
				&& (where == anywhere
					|| Check_spot(where, p.tx, p.ty, new_lift))) {
				// Use tile before deleting.
				return Tile_coord(p.tx, p.ty, new_lift);
			}
		}
	}
	return Tile_coord(-1, -1, -1);
}

/*
 *  Find a free area for an object (usually an NPC) that we want to
 *  approach a given position.
 *
 *  Output: Tile if successful, else (-1, -1, -1).
 */

Tile_coord Map_chunk::find_spot(
		const Tile_coord& pos,     // Starting point.
		int               dist,    // Distance to look outwards.  (0 means
		//   only check 'pos'.
		Game_object*    obj,         // Object that we want to move.
		int             max_drop,    // Allow to drop by this much.
		Find_spot_where where        // Inside/outside.
) {
	const Tile_coord t2 = obj->get_tile();
	// Get direction from pos. to object.
	const int dir
			= static_cast<int>(Get_direction(pos.ty - t2.ty, t2.tx - pos.tx));
	return find_spot(
			pos, dist, obj->get_shapenum(), obj->get_framenum(), max_drop, dir,
			where);
}

/*
 *  Find all desired objects within a given rectangle.
 *
 *  Output: # found, appended to vec.
 */

int Map_chunk::find_in_area(
		Game_object_vector& vec,     // Returned here.
		const TileRect&     area,    // Area to search.
		int shapenum, int framenum) {
	const int savesize = vec.size();
	// Go through interesected chunks.
	Chunk_intersect_iterator next_chunk(area);
	TileRect                 tiles;    // (Tiles within intersected chunk).
	int                      eachcx;
	int                      eachcy;
	Game_map*                gmap = gwin->get_map();
	while (next_chunk.get_next(tiles, eachcx, eachcy)) {
		Map_chunk* chunk = gmap->get_chunk_safely(eachcx, eachcy);
		if (!chunk) {
			continue;
		}
		Object_iterator next(chunk->objects);
		Game_object*    each;
		while ((each = next.get_next()) != nullptr) {
			if (each->get_shapenum() == shapenum
				&& each->get_framenum() == framenum
				&& tiles.has_world_point(each->get_tx(), each->get_ty())) {
				vec.push_back(each);
			}
		}
	}
	return vec.size() - savesize;
}

/*
 *  Test all nearby eggs when you've teleported in.
 */

void Map_chunk::try_all_eggs(
		Game_object* obj,           // Object (actor) that's near.
		int tx, int ty, int tz,     // Tile (absolute).
		int from_tx, int from_ty    // Tile walked from.
) {
	static int norecurse = 0;    // NO recursion here.
	if (norecurse) {
		return;
	}
	norecurse++;
	Game_map*        gmap = gwin->get_map();
	const Tile_coord pos  = obj->get_tile();
	const int        dist = 32;    // See if this works okay.
	const TileRect   area(pos.tx - dist, pos.ty - dist, 2 * dist, 2 * dist);
	// Go through interesected chunks.
	Chunk_intersect_iterator next_chunk(area);
	TileRect                 tiles;    // (Ignored).
	int                      eachcx;
	int                      eachcy;
	Egg_vector               eggs;    // Get them here first, as activating
	eggs.reserve(40);
	//   an egg could affect chunk's list.
	while (next_chunk.get_next(tiles, eachcx, eachcy)) {
		Map_chunk* chunk = gmap->get_chunk_safely(eachcx, eachcy);
		if (!chunk) {
			continue;
		}
		chunk->setup_cache();    // I think we should do this.
		Object_iterator next(chunk->objects);
		Game_object*    each;
		while ((each = next.get_next()) != nullptr) {
			if (each->is_egg()) {
				Egg_object* egg = each->as_egg();
				// Music eggs are causing problems.
				if (egg->get_type() != Egg_object::jukebox &&
					// And don't teleport a 2nd time.
					egg->get_type() != Egg_object::teleport
					&& egg->is_active(obj, tx, ty, tz, from_tx, from_ty)) {
					eggs.push_back(egg);
				}
			}
		}
	}
	for (auto* egg : eggs) {
		egg->hatch(obj);
	}
	norecurse--;
}

/*
 *  Add a rectangle of dungeon tiles (but only if higher!).
 */

void Map_chunk::add_dungeon_levels(TileRect& tiles, unsigned int lift) {
	if (!dungeon_levels) {
		// First one found.
		dungeon_levels = std::make_unique<unsigned char[]>(256);
	}
	const int endy = tiles.y + tiles.h;
	const int endx = tiles.x + tiles.w;
	for (int ty = tiles.y; ty < endy; ty++) {
		for (int tx = tiles.x; tx < endx; tx++) {
			if (GAME_SI) {
				// SI has roofs at random levels!!
				lift = 5;
			}
			dungeon_levels[ty * c_tiles_per_chunk + tx] = lift;
		}
	}
}

/*
 *  Set up the dungeon levels (after IFIX objects read).
 */

void Map_chunk::setup_dungeon_levels() {
	Game_map* gmap = gwin->get_map();

	Object_iterator next(objects);
	Game_object*    each;
	while ((each = next.get_next()) != nullptr) {
		// Test for mountain-tops.
		const Shape_info& shinf = each->get_info();
		if (shinf.get_shape_class() == Shape_info::building
			&& shinf.get_mountain_top_type()
					   == Shape_info::normal_mountain_top) {
			// SI shape 941, frame 0 => do whole chunk (I think).
			const TileRect area
					= (shinf.has_translucency() && each->get_framenum() == 0)
							  ? TileRect(
										cx * c_tiles_per_chunk,
										cy * c_tiles_per_chunk,
										c_tiles_per_chunk, c_tiles_per_chunk)
							  : each->get_footprint();

			// Go through interesected chunks.
			Chunk_intersect_iterator next_chunk(area);
			TileRect                 tiles;    // Rel. tiles.
			int                      cx;
			int                      cy;
			while (next_chunk.get_next(tiles, cx, cy)) {
				gmap->get_chunk(cx, cy)->add_dungeon_levels(
						tiles, each->get_lift());
			}
		}    // Ice Dungeon Pieces in SI
		else if (
				shinf.get_shape_class() == Shape_info::building
				&& shinf.get_mountain_top_type()
						   == Shape_info::snow_mountain_top) {
			// HACK ALERT! This gets 320x200 to work, but it is a hack
			// This is not exactly accurate.
			ice_dungeon
					|= 1 << ((each->get_tx() >> 3) + 2 * (each->get_ty() >> 3));

			const TileRect area = each->get_footprint();

			// Go through interesected chunks.
			Chunk_intersect_iterator next_chunk(area);
			TileRect                 tiles;    // Rel. tiles.
			int                      cx;
			int                      cy;
			while (next_chunk.get_next(tiles, cx, cy)) {
				gmap->get_chunk(cx, cy)->add_dungeon_levels(
						tiles, each->get_lift());
			}
		}
	}
	if (dungeon_levels) {    // Recount lights.
		dungeon_lights.clear();
		non_dungeon_lights.clear();
		next.reset();
		while ((each = next.get_next()) != nullptr) {
			if (each->get_info().get_object_light(each->get_framenum()) > 0) {
				if (is_dungeon(each->get_tx(), each->get_ty())) {
					dungeon_lights.insert(each);
				} else {
					non_dungeon_lights.insert(each);
				}
			}
		}
	}
}

/*
 *  Recursively apply gravity over a given rectangle that is known to be
 *  unblocked below a given lift.
 */

void Map_chunk::gravity(
		const TileRect& area,    // Unblocked tiles (in abs. coords).
		int             lift     // Lift where tiles are free.
) {
	Game_object_vector dropped;    // Gets list of objs. that dropped.
	dropped.reserve(20);
	// Go through interesected chunks.
	Chunk_intersect_iterator next_chunk(area);
	TileRect                 tiles;    // Rel. tiles.  Not used.
	int                      cx;
	int                      cy;
	int                      new_lift;
	while (next_chunk.get_next(tiles, cx, cy)) {
		Map_chunk*      chunk = gmap->get_chunk(cx, cy);
		Object_iterator objs(chunk->objects);
		Game_object*    obj;
		while ((obj = objs.get_next()) != nullptr) {
			// We DO want NPC's to fall.
			if (!obj->is_dragable() && !obj->get_info().is_npc()) {
				continue;
			}
			const Tile_coord t = obj->get_tile();
			// Get footprint.
			const TileRect foot = obj->get_footprint();
			// Above area?
			if (t.tz >= lift && foot.intersects(area) &&
				// Unblocked below itself?
				!is_blocked(
						1, t.tz - 1, foot.x, foot.y, foot.w, foot.h, new_lift,
						MOVE_ALL_TERRAIN, 0)
				&& new_lift < t.tz) {
				dropped.push_back(obj);
			}
		}
	}
	// Drop each one found.
	for (auto* obj : dropped) {
		const Tile_coord t = obj->get_tile();
		// Get footprint.
		const TileRect foot = obj->get_footprint();
		// Let drop as far as possible.
		if (!is_blocked(
					1, t.tz - 1, foot.x, foot.y, foot.w, foot.h, new_lift,
					MOVE_ALL_TERRAIN, 100)
			&& new_lift < t.tz) {
			// Drop & recurse.
			obj->move(t.tx, t.ty, new_lift);
			gravity(foot, obj->get_lift() + obj->get_info().get_3d_height());
		}
	}
}

/*
 *  Finds if there is a 'roof' above lift in tile (tx, ty)
 *  of the chunk. Point is taken 4 above lift
 *
 *  Roof can be any object, not just a literal roof
 *
 *  Output: height of the roof.
 *  A return of 31 means no roof
 *
 */
int Map_chunk::is_roof(int tx, int ty, int lift) {
	const int height = get_lowest_blocked(lift + 4, tx, ty);
	if (height == -1) {
		return 255;
	}
	return height;
}

void Map_chunk::kill_cache() {
	// Get rid of terrain
	if (terrain) {
		terrain->remove_client();
	}
	terrain = nullptr;

	// Now remove the cachce
	cache.reset();

	// Delete dungeon bits
	dungeon_levels.reset();
}

int Map_chunk::get_obj_actors(
		vector<Game_object*>& removes, vector<Actor*>& actors) {
	int  buf_size = 0;
	bool failed   = false;

	// Separate scope for Object_iterator.
	Object_iterator it(get_objects());
	Game_object*    each;
	while ((each = it.get_next()) != nullptr) {
		Actor* actor = each->as_actor();

		// Normal objects and monsters
		if (actor == nullptr
			|| (each->is_monster()
				&& each->get_flag(Obj_flags::is_temporary))) {
			removes.push_back(each);
			const int ireg_size = each->get_ireg_size();

			if (ireg_size < 0) {
				failed = true;
			} else {
				buf_size += ireg_size;
			}
		}
		// Actors/NPCs here
		else {
			actors.push_back(actor);
		}
	}

	return failed ? -1 : buf_size;
}

void Map_chunk::write(ODataSource& out, bool v2) {
	// Restore original order (sort of).
	Object_iterator_backwards next(this);
	Game_object*              obj;
	while ((obj = next.get_next()) != nullptr) {
		obj->write_ifix(&out, v2);
	}
}
