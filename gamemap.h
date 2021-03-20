/*
 *  gamemap.h - Game map data.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef GAMEMAP_H
#define GAMEMAP_H

#include "chunks.h"
#include "exult_constants.h"
#include "flags.h"
#include "tiles.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>    // STL string
#include <vector>

class Chunk_terrain;
class Map_patch_collection;
class Game_object;
class Ireg_game_object;
class Ifix_game_object;
class Egg_object;
class Shape_info;
class Shapes_vga_file;
class IDataSource;
class ODataSource;
class Shape;
using Ireg_game_object_shared = std::shared_ptr<Ireg_game_object>;
using Ifix_game_object_shared = std::shared_ptr<Ifix_game_object>;

#define IREG_EXTENDED   254     // For shape #'s > 1023.
#define IREG_EXTENDED2  253     // For lift > 15.

/*
 *  The game map:
 */
class Game_map {
	int num;            // Map #.  Index in gwin->maps.
	// Flat chunk areas:
	static std::vector<Chunk_terrain *> *chunk_terrains;
	static std::ifstream *chunks;   // "u7chunks" file.
	static bool v2_chunks;      // True if 3 bytes/entry.
	static bool read_all_terrain;   // True if we've read them all.
	static bool chunk_terrains_modified;
	bool didinit;
	bool map_modified;      // True if any map changes from
	//   map-editing.
	// Chunk_terrain index for each chunk:
	short terrain_map[c_num_chunks][c_num_chunks];
	// A list of objects in each chunk:
	std::unique_ptr<Map_chunk> objects[c_num_chunks][c_num_chunks];
	bool schunk_read[144];      // Flag for reading in each "ifix".
	bool schunk_modified[144];  // Flag for modified "ifix".
	char *schunk_cache[144];
	int  schunk_cache_sizes[144];
	int caching_out;        // >0 in 'cache_out_schunk'.
    std::unique_ptr<Map_patch_collection> map_patches;

	Map_chunk *create_chunk(int cx, int cy);
	static Chunk_terrain *read_terrain(int chunk_num);

	// Create a 192x192 viewable map.
	void create_minimap(Shape *minimaps, const unsigned char *chunk_pixels);
	void cache_out_schunk(int schunk);
public:
	Game_map(int n);
	~Game_map();
	static void init_chunks();
	void init();            // Set up map.
	static void clear_chunks();
	void clear();           // Clear out old map.
	void read_map_data();       // Read in 'ifix', 'ireg', etc.
	static bool is_v2_chunks() {
		return v2_chunks;
	}
	bool is_caching_out() const {
		return caching_out > 0;
	}
	int get_num() const {
		return num;
	}
	inline short get_terrain_num(int cx, int cy) const {
		return terrain_map[cx][cy];
	}
	inline Map_patch_collection& get_map_patches() {
		return *map_patches;
	}
	void set_map_modified() {
		map_modified = true;
	}
	bool was_map_modified() const {
		return map_modified;
	}
	static bool was_chunk_terrain_modified() {
		return chunk_terrains_modified;
	}
	static void set_chunk_terrains_modified() {
		chunk_terrains_modified = true;
	}
	bool is_chunk_read(int cx, int cy) {
		return cx < c_num_chunks && cy < c_num_chunks &&
		       schunk_read[12 * (cy / c_chunks_per_schunk) +
		                   cx / c_chunks_per_schunk];
	}
	void ensure_chunk_read(int cx, int cy) {
		if (cx < c_num_chunks && cy < c_num_chunks) {
			int sc = 12 * (cy/c_chunks_per_schunk) + cx/c_chunks_per_schunk;
			if (!schunk_read[sc])
				get_superchunk_objects(sc);
		}
	}
	void set_ifix_modified(int cx, int cy) {
		map_modified = true;
		schunk_modified[12 * (cy / c_chunks_per_schunk) +
		                cx / c_chunks_per_schunk] = true;
	}
	// Get objs. list for a chunk.
	Map_chunk *get_chunk_unsafe(int cx, int cy) {
		return objects[cx][cy].get();
	}
	// Get/create objs. list for a chunk.
	Map_chunk *get_chunk_unchecked(int cx, int cy) {
		Map_chunk *list = get_chunk_unsafe(cx, cy);
		return list ? list : create_chunk(cx, cy);
	}
	// Get/create objs. list for a chunk.
	Map_chunk *get_chunk(int cx, int cy) {
		assert((cx >= 0) && (cx < c_num_chunks) &&
		       (cy >= 0) && (cy < c_num_chunks));
		return get_chunk_unchecked(cx, cy);
	}
	Map_chunk *get_chunk_safely(int cx, int cy) {
		if (cx >= 0 && cx < c_num_chunks &&
		    cy >= 0 && cy < c_num_chunks) {
			return get_chunk_unchecked(cx, cy);
		}
		return nullptr;
	}
	// Get "map" superchunk objs/scenery.
	void get_map_objects(int schunk);
	// Get "chunk" objects/scenery.
	void get_chunk_objects(int cx, int cy);
	static void get_all_terrain();  // Read in all terrains.
	// Get desired terrain.
	static Chunk_terrain *get_terrain(int tnum) {
		Chunk_terrain *ter = (*chunk_terrains)[tnum];
		return ter ? ter : read_terrain(tnum);
	}
	inline int get_num_chunk_terrains() const {
		return chunk_terrains->size();
	}
	// Set new terrain chunk.
	void set_chunk_terrain(int cx, int cy, int chunknum);
	char *get_mapped_name(const char *from, char *to);
	// Get ifixxxx/iregxx name.
	char *get_schunk_file_name(const char *prefix, int schunk, char *fname);
	static void write_chunk_terrains();
	void write_static();        // Write to 'static' directory.
	// Write (static) map objects.
	void write_ifix_objects(int schunk);
	// Get "ifix" objects for a superchunk.
	void get_ifix_objects(int schunk);
	// Get "ifix" objs. for given chunk.
	void get_ifix_chunk_objects(IDataSource *ifix, int vers,
	                            long filepos, int len, int cx, int cy);
	static void write_attributes(ODataSource *ireg,
	                             std::vector<std::pair<const char *, int> > &attlist);
	// Write scheduled script for obj.
	static void write_scheduled(ODataSource *ireg, Game_object *obj,
	                            bool write_mark = false);
	static int write_string(ODataSource *ireg, const char *str);
	void write_ireg();      // Write modified ireg files.
	// Write moveable objects to file.
	void write_ireg_objects(int schunk);
	// Write moveable objects to datasource.
	void write_ireg_objects(int schunk, ODataSource *ireg);
	// Get moveable objects.
	void get_ireg_objects(int schunk);
	// Read scheduled script(s) for obj.
	void read_special_ireg(IDataSource *ireg, Game_object *obj);
	void read_ireg_objects(IDataSource *ireg, int scx, int scy,
	                       Game_object *container = nullptr,
	                       unsigned long flags = (1 << Obj_flags::okay_to_take));
	Ireg_game_object_shared create_ireg_object(const Shape_info &info, int shnum,
	                                     int frnum, int tilex, int tiley, int lift);
	Ireg_game_object_shared create_ireg_object(int shnum, int frnum);
	Ifix_game_object_shared create_ifix_object(int shnum, int frnum);
	// Get all superchunk objects.
	void get_superchunk_objects(int schunk);
	bool is_tile_occupied(Tile_coord const &tile);
	// Locate chunk with desired terrain.
	bool locate_terrain(int tnum, int &cx, int &cy, bool upwards = false);
	static bool swap_terrains(int tnum);    // Swap adjacent terrain #'s.
	// Insert new terrain after 'tnum'.
	static bool insert_terrain(int tnum, bool dup = false);
	static bool delete_terrain(int tnum);
	static void commit_terrain_edits(); // End terrain-editing mode.
	static void abort_terrain_edits();
	// Search entire game for unused.
	void find_unused_shapes(unsigned char *found, int foundlen);
	// Locate shape (for EStudio).
	Game_object *locate_shape(int shapenum, bool upwards,
	                          Game_object *start, int frnum, int qual);
	static bool write_minimap();

	// Do a cache out. (cx, cy) is the center
	void cache_out(int cx, int cy);
};

#endif
