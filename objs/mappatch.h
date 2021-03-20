/**
 ** Mappatch.h - Patches to the game map.
 **
 ** Written: 10-18-2001
 **/

/*
Copyright (C) 2001 The Exult Team

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

#ifndef INCL_MAPPATCH
#define INCL_MAPPATCH   1

#include <map>
#include <list>
#include <memory>
#include "tiles.h"

class Game_object;

/*
 *  Specify an object by location, shape, etc.
 */
class Object_spec {
public:
	Tile_coord loc;         // Where it is.
	int shapenum;           // Shape #, or -359 for 'dont care'.
	int framenum;           // Frame #, or -359.
	int quality;            // Quality, or -359.
	Object_spec(Tile_coord const &t, int shnum = c_any_shapenum,
	            int frnum = c_any_framenum, int qual = c_any_qual)
		: loc(t), shapenum(shnum), framenum(frnum), quality(qual)
	{  }
};

/*
 *  Base class for map patches:
 */
class Map_patch {
	Object_spec spec;       // Specifies object to modify.
public:
	friend class Map_patch_collection;
	Map_patch(Object_spec s) : spec(s)
	{  }
	virtual ~Map_patch() = default;
	Game_object *find();        // Find matching object.
	virtual bool apply() = 0;   // Perform action.
};

// Sigh, this is needed to prevent compiler error with MSVC
using Map_patch_list = std::list<std::unique_ptr<Map_patch>>;
using Map_patch_map = std::map<int, Map_patch_list>;

/*
 *  Remove an object.
 */
class Map_patch_remove : public Map_patch {
	bool all;           // Delete all matching.
public:
	Map_patch_remove(Object_spec s, bool a = false) : Map_patch(s), all(a)
	{  }
	bool apply() override;       // Perform action.
};

/*
 *  Move/modify an object.
 */
class Map_patch_modify : public Map_patch {
	Object_spec mod;        // Modification.
public:
	Map_patch_modify(Object_spec s, Object_spec m) : Map_patch(s), mod(m)
	{  }
	bool apply() override;       // Perform action.
};

/*
 *  Here's a collection of patches, organized by superchunk.
 */
class Map_patch_collection {
	Map_patch_map patches;
public:
	~Map_patch_collection() = default;
	void add(std::unique_ptr<Map_patch> p);     // Add a patch.
	void apply(int schunk);     // Apply for given superchunk.
};

#endif
