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

	// Result of deciding whether one light source reaches the current viewer.
	struct LightVisibility {
		bool blocked           = false;    // Light does not reach the viewer.
		int  crossings         = 0;        // Inside<->outside boundaries crossed.
		bool interior_source   = false;    // Source sits inside a roofed enclosure.
		bool source_can_escape = false;    // Its own enclosure lets light out.
		bool leaks_through_gap = false;    // Escape is via a physical wall gap.
	};

	// Decide whether the light living in `olist` (the chunk being painted)
	// reaches the viewer, by composing the enclosure/opening tests above.
	// `viewer_outside`, `same_chunk`, `avatar_sealed` and `chunk_has_opening` are
	// the per-frame / per-chunk facts the caller already computed; `main_actor`
	// is the viewer whose enclosure the light must reach.
	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening);

	// Map a light's intrinsic brightness (object_light / carried strength) to the
	// spatial-light glow radius in game pixels (~3 tiles per brightness level).
	int Light_radius(int brightness);

	// Map a light's intrinsic brightness to its palette tier:
	// 0 = candle, 1 = single light, 2 = many lights.
	int Light_tier(int brightness);

	// Splat one radial light's soft dome (hemispherical) falloff into the
	// coverage buffer, copying the brightened source pixel wherever this light
	// is the strongest contributor so far. `cov` is a contiguous W*H buffer (row
	// stride W); `dst`, `src` and `roof` use their own line widths. `elevation`
	// is the emitter's height above the floor in game pixels: it models the
	// light as a point that height above the ground, so the pool of light domes
	// (rounded, lower-peaked) instead of reading as a flat disc. When `roofpix`
	// is non-null, any pixel it marks stays dark so the light never brightens a
	// roof over it.
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, const unsigned char* roofpix, int roof_lw);

}    // namespace NaturalLight

#endif
