/**
 ** Chunks.h - Chunks (16x16 tiles) on the map.
 **
 ** Written: 10/1/98 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef CHUNKS_H
#define CHUNKS_H

#include "chunkter.h"
#include "exult_constants.h"
#include "objlist.h"
#include "rect.h"
#include "shapeid.h"
#include "tiles.h"

#include <memory>
#include <set>

class Map_chunk;
class Egg_object;
class Game_object;
class Game_map;
class Actor;
class Npc_actor;
class Image_buffer8;
class Chunk_terrain;
class ODataSource;
class Ordering_info;

/*
 *  Data cached for a chunk to speed up processing, but which doesn't need
 *  to be saved to disk:
 */
class Chunk_cache : public Game_singletons,
					public std::enable_shared_from_this<Chunk_cache> {
public:
	// For each tile, 2 bits for each lift
	// level for #objs blocking there, so
	// 8 lifts are represented.
	using blocked8z = std::unique_ptr<uint16[]>;

private:
	Map_chunk* obj_list;
	// One for each 8 lifts.
	std::vector<blocked8z> blocked;
	// ->eggs which influence this chunk.
	std::vector<Egg_object*> egg_objects;
	// Bit #i (0-14) set means that the
	// tile is within egg_object[i]'s
	// influence.  Bit 15 means it's 1 or
	// more of egg_objects[15-(num_eggs-1)].
	unsigned short eggs[256];
	// Keep special list of doors.
	std::set<Game_object*> doors;

	int get_num_eggs() {
		return egg_objects.size();
	}

	blocked8z& new_blocked_level(int zlevel);

	blocked8z& need_blocked_level(int zlevel) {
		return (static_cast<unsigned>(zlevel) >= blocked.size()
				|| !blocked[zlevel])
					   ? new_blocked_level(zlevel)
					   : blocked[zlevel];
	}

	// Set/unset blocked region.
	void set_blocked(
			int startx, int starty, int endx, int endy, int lift, int ztiles);
	void clear_blocked(
			int startx, int starty, int endx, int endy, int lift, int ztiles);
	// Add/remove object.
	void update_object(Map_chunk* chunk, Game_object* obj, bool add);
	// Set area within egg's influence.
	void set_egged(Egg_object* egg, TileRect& tiles, bool add);
	// Add egg.
	void update_egg(Map_chunk* chunk, Egg_object* egg, bool add);
	// Set up with chunk's data.
	void setup(Map_chunk* chunk);
	void set_tflags(int tx, int ty, int maxz);    // Setup flags.
	// Get highest lift blocked below a
	//   given level for a desired tile.
	int get_highest_blocked(int lift);
	int get_highest_blocked(int lift, int tx, int ty);
	int get_lowest_blocked(int lift);
	int get_lowest_blocked(int lift, int tx, int ty);
	// Is a spot occupied or inaccessible
	//   to an NPC?
	bool is_blocked(
			int height, int lift, int tx, int ty, int& new_lift,
			const int move_flags, int max_drop = 1, int max_rise = -1);
	// Activate eggs nearby.
	void activate_eggs(
			Game_object* obj, Map_chunk* chunk, int tx, int ty, int tz,
			int from_tx, int from_ty, unsigned short eggbits, bool now);

	void activate_eggs(
			Game_object* obj, Map_chunk* chunk, int tx, int ty, int tz,
			int from_tx, int from_ty, bool now) {
		const unsigned short eggbits
				= eggs[(ty % c_tiles_per_chunk) * c_tiles_per_chunk
					   + (tx % c_tiles_per_chunk)];
		if (eggbits) {
			activate_eggs(
					obj, chunk, tx, ty, tz, from_tx, from_ty, eggbits, now);
		}
	}

	void unhatch_eggs(
			Game_object* obj, Map_chunk* chunk, int tx, int ty, int tz,
			int from_tx, int from_ty, unsigned short eggbits, bool now);

	void unhatch_eggs(
			Game_object* obj, Map_chunk* chunk, int tx, int ty, int tz,
			int from_tx, int from_ty, bool now) {
		// unhatch needs eggbits at from location not to location
		const unsigned short eggbits
				= eggs[(from_ty % c_tiles_per_chunk) * c_tiles_per_chunk
					   + (from_tx % c_tiles_per_chunk)];
		if (eggbits) {
			unhatch_eggs(
					obj, chunk, tx, ty, tz, from_tx, from_ty, eggbits, now);
		}
	}

	// Find door blocking given tile.
	Game_object* find_door(const Tile_coord& t);

public:
	friend class Map_chunk;
	Chunk_cache();

	// Is there something on this tile?
	inline bool is_tile_occupied(int tx, int ty, int tz) {
		const auto* b8 = static_cast<unsigned>(tz / 8) < blocked.size()
								 ? blocked[tz / 8].get()
								 : nullptr;
		return b8 && (b8[ty * c_tiles_per_chunk + tx] & (3 << (2 * (tz % 8))));
	}
};

/*
 *  Game objects are stored in a list for each chunk, sorted from top-to-
 *  bottom, left-to-right.
 */
class Map_chunk : public Game_singletons {
	Game_map*      map;        // Map we're a part of.
	Chunk_terrain* terrain;    // Flat landscape tiles.
	// ->first in list of all objs.  'Flat'
	// obs. (lift=0,ht=0) stored 1st.
	Object_list objects;
	// ->first nonflat in 'objects'.
	// Counts of overlapping objects from
	// chunks below, to right.
	Game_object*  first_nonflat;
	unsigned char from_below, from_right, from_below_right;
	unsigned char ice_dungeon;    // For SI, chunk split into 4 quadrants
	std::unique_ptr<unsigned char[]>
			dungeon_levels;    // A 'dungeon' level value for each tile.
	std::shared_ptr<Chunk_cache> cache;    // Data for chunks near player.
	unsigned char                roof;     // 1 if a roof present.
	// # light sources in chunk.
	std::set<Game_object*> dungeon_lights;
	std::set<Game_object*> non_dungeon_lights;
	unsigned char          cx, cy;      // Absolute chunk coords. of this.
	bool                   selected;    // For 'select_chunks' mode.
	void add_dungeon_levels(TileRect& tiles, unsigned int lift);
	void add_dependencies(Game_object* newobj, Ordering_info& newinfo);
	static Map_chunk* add_outside_dependencies(
			int cx, int cy, Game_object* newobj, Ordering_info& newinfo);

public:
	friend class Npc_actor;
	friend class Game_object;
	Map_chunk(Game_map* m, int chunkx, int chunky);

	Game_map* get_map() const {
		return map;
	}

	Chunk_terrain* get_terrain() const {
		return terrain;
	}

	void set_terrain(Chunk_terrain* ter);
	void add(Game_object* obj);       // Add an object.
	void add_egg(Egg_object* egg);    // Add/remove an egg.
	void remove_egg(Egg_object* egg);
	void remove(Game_object* remove);    // Remove an object.
	// Apply gravity over given area.
	static void gravity(const TileRect& area, int lift);
	// Is there a roof? Returns height
	int is_roof(int tx, int ty, int lift);

	Object_list& get_objects() {
		return objects;
	}

	Game_object* get_first_nonflat() const {
		return first_nonflat;
	}

	int get_cx() const {
		return cx;
	}

	int get_cy() const {
		return cy;
	}

	void set_selected(bool tf) {
		selected = tf;
	}

	bool is_selected() const {
		return selected;
	}

	const std::set<Game_object*>& get_dungeon_lights()
			const {    // Get #lights.
		return dungeon_lights;
	}

	const std::set<Game_object*>& get_non_dungeon_lights() const {
		return non_dungeon_lights;
	}

	ShapeID get_flat(int tilex, int tiley) const {
		return terrain ? terrain->get_flat(tilex, tiley) : ShapeID();
	}

	Image_buffer8* get_rendered_flats() {
		return terrain ? terrain->get_rendered_flats() : nullptr;
	}

	// Get/create/setup cache.
	Chunk_cache* get_cache() const {
		return cache.get();
	}

	Chunk_cache* need_cache() {
		setup_cache();
		return get_cache();
	}

	void setup_cache() {
		if (!cache) {
			cache = std::make_shared<Chunk_cache>();
			cache->setup(this);
		}
	}

	// Set/unset blocked region.
	void set_blocked(
			int startx, int starty, int endx, int endy, int lift, int ztiles,
			bool set) {
		Chunk_cache* cache = need_cache();
		if (set) {
			cache->set_blocked(startx, starty, endx, endy, lift, ztiles);
		} else {
			cache->clear_blocked(startx, starty, endx, endy, lift, ztiles);
		}
	}

	// Get highest lift blocked.
	int get_highest_blocked(int lift, int tx, int ty) {
		return need_cache()->get_highest_blocked(lift, tx, ty);
	}

	int get_lowest_blocked(int lift, int tx, int ty) {
		return need_cache()->get_lowest_blocked(lift, tx, ty);
	}

	// Is a spot occupied or inaccessible
	//  to an NPC?
	bool is_blocked(
			int height, int lift, int tx, int ty, int& new_lift,
			const int move_flags, int max_drop = 1, int max_rise = -1) {
		return cache->is_blocked(
				height, lift, tx, ty, new_lift, move_flags, max_drop, max_rise);
	}

	// Check range.
	static bool is_blocked(
			int height, int lift, int startx, int starty, int xtiles,
			int ytiles, int& new_lift, const int move_flags, int max_drop,
			int max_rise = -1);
	// Check absolute tile.
	static bool is_blocked(
			Tile_coord& tile, int height, const int move_flags,
			int max_drop = 1, int max_rise = -1);
	// Check for > 1x1 object.
	static bool is_blocked(
			int xtiles, int ytiles, int ztiles, const Tile_coord& from,
			Tile_coord& to, const int move_flags, int max_drop = 1,
			int max_rise = -1);

	// Check tile WITHIN chunk.
	bool is_tile_occupied(int tx, int ty, int tz) {
		return need_cache()->is_tile_occupied(tx, ty, tz);
	}

	enum Find_spot_where {    // For find_spot() below.
		anywhere = 0,
		inside,    // Must be inside.
		outside    // Must be outside,
	};

	// Find spot for an object.
	static Tile_coord find_spot(
			Tile_coord pos, int dist, int shapenum, int framenum,
			int max_drop = 0, int dir = -1, Find_spot_where where = anywhere);
	// For approaching 'pos' by an object:
	static Tile_coord find_spot(
			const Tile_coord& pos, int dist, Game_object* obj, int max_drop = 0,
			Find_spot_where where = anywhere);

	// Set area within egg's influence.
	void set_egged(Egg_object* egg, TileRect& tiles, bool add) {
		need_cache()->set_egged(egg, tiles, add);
	}

	void activate_eggs(
			Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty,
			bool now = false) {
		need_cache()->activate_eggs(
				obj, this, tx, ty, tz, from_tx, from_ty, now);
	}

	void unhatch_eggs(
			Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty,
			bool now = false) {
		need_cache()->unhatch_eggs(
				obj, this, tx, ty, tz, from_tx, from_ty, now);
	}

	// Find door blocking given tile.
	Game_object* find_door(const Tile_coord& t) {
		return need_cache()->find_door(t);
	}

	static int find_in_area(
			std::vector<Game_object*>& vec, const TileRect& area, int shapenum,
			int framenum);
	// Use this when teleported in.
	static void try_all_eggs(
			Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty);
	void setup_dungeon_levels();    // Set up after IFIX objs. read.

	inline bool has_dungeon() {    // Any tiles within dungeon?
		return dungeon_levels != nullptr;
	}

	// NOTE:  The following should only be
	//   called if has_dungeon()==1.
	inline int is_dungeon(int tx, int ty) {
		// Is object within dungeon? (returns height)
		return dungeon_levels[ty * c_tiles_per_chunk + tx];
	}

	// Is the dungeon an ICE dungeon.NOTE: This is a
	// Hack and splits the chunk into 4 parts. Only if
	inline bool is_ice_dungeon(
			int tx, int ty) {    // all 4 are ice, will we have an ice dungeon
		ignore_unused_variable_warning(tx, ty);
		return ice_dungeon == 0x0F;    // 0 != ((ice_dungeon >> ( (tx>>3) +
									   // 2*(ty>>3) ) )&1);
	}

	// Kill the items and the cache
	void kill_cache();
	// Get all objects and actors for use when writing memory cache.
	// returns size require to save
	int get_obj_actors(
			std::vector<Game_object*>& removes, std::vector<Actor*>& actors);

	void write(ODataSource& out, bool v2);
};

#endif
