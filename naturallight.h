/*
 *  naturallight.h - Geometry helpers for the spatial ("natural") lighting.
 *
 *  Copyright (C) 2026  The Exult Team
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

#ifndef NATURALLIGHT_H
#define NATURALLIGHT_H 1

#include "tiles.h"

class Game_object;
class Map_chunk;

namespace NaturalLight {

	// Find the first shape in `chunk` that light passes through (window, open
	// door, grate, ...). Reports the shape/frame/tile/lift of that opening.
	bool Chunk_find_light_passes_through(
			Map_chunk* chunk, int& pass_shape, int& pass_frame, int& pass_match_frame, int& pass_tx, int& pass_ty, int& pass_lift);

	// True if the light at interior tile `src` has an unobstructed (pathfound)
	// route to any light_passes_through shape (window / open door / grate) in the
	// chunk -- so a torch sealed behind an interior wall does not leak out through
	// a window elsewhere in the same chunk.
	bool Light_reaches_chunk_opening(Map_chunk* chunk, const Tile_coord& src, Game_object* light_obj);

	// Resolve a light source to a nearby interior tile and report whether it is an
	// interior source. A wall-mounted torch's own tile is the wall itself, so we
	// prefer the source's floor tile, else an adjacent roofed non-wall tile.
	Tile_coord Resolve_interior_light_tile(const Tile_coord& src, bool& interior);

	// Bounded flood fill from `start`: true if the roofed enclosure it stands in
	// has a passable gap in its walls leading outside (e.g. a doorway with no door
	// object, which has no light_passes_through shape to detect).
	bool Enclosure_open_to_outside(const Tile_coord& start);

	// Bounded flood fill from `start` through non-wall tiles: true if it reaches
	// `target`. Tells whether a light shares the Avatar's interior space even
	// across a chunk boundary.
	bool Tiles_in_same_enclosure(const Tile_coord& start, const Tile_coord& target);

	// True if an actual roof shape covers the light's tile from above. Looks only
	// at is_roof() objects (ignoring the light itself).
	bool Light_beneath_roof(Game_object* light_obj);

}    // namespace NaturalLight

#endif
