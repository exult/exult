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

#include <vector>

class Game_map;
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

	// The z-level of the roof over the given absolute tile -- the lowest
	// blocked lift above the light (searched from max(lift+1, 4), so floor
	// lights skip low furniture and elevated lights skip themselves) -- or 5,
	// the classic wall-top threshold, when no roof is found.  This is the
	// height the room's walls reach: the room-fill blocker test and the mask's
	// wall-top stamp anchor both use it instead of a hardcoded 5, so buildings
	// with taller walls mask correctly.
	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift);

	// Build the room-fill grid a light casts, honouring tall light-blocking
	// walls (a solid stack whose top reaches z-level 5).  `rt` is the light's
	// radius in tiles; `lit` is filled with a (2*rt+1) square grid, index
	// (dty+rt)*(2*rt+1) + (dtx+rt), where dtx = tile.tx - light.tx (east +) and
	// dty = tile.ty - light.ty (south +).  A cell is 1 when light can reach that
	// tile.  The set is a flood-fill from the light's tile across passable floor,
	// stopped by tall walls (which are themselves lit as a one-tile ring).  Because
	// the reached set depends only on the enclosing room's shape -- not on where in
	// the room the light stands -- carrying a torch around a closed room keeps the
	// same mask.  `lit` is left empty when there is nothing to gate (fully lit).
	// A wall tile covered by a light_passes_through shape (window, grate) is
	// reported in `spills` as the tile just OUTSIDE the opening (absolute
	// coords): the caller renders a small radial glow there -- the light
	// spilling out of the opening -- instead of the fill crossing the wall.
	void Build_light_shadow_grid(Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Tile_coord>& spills);

	// Build the room-fill grid for a spill glow (same layout as
	// Build_light_shadow_grid), flooded from `start` -- the tile just outside
	// the window/grate the light escapes through.  The fill spreads over
	// whatever the opening looks out on and is stopped by the building's own
	// wall (the window face is lit as part of the wall ring), so the spilled
	// light shines AWAY from the room, never back inside it.
	void Build_spill_shadow_grid(const Tile_coord& start, int rt, std::vector<unsigned char>& lit);

	// Splat one radial light's soft dome (hemispherical) falloff into the
	// coverage buffer, copying the brightened source pixel wherever this light
	// is the strongest contributor so far. `cov` is a contiguous W*H buffer (row
	// stride W); `dst`, `src` and `roof` use their own line widths. `elevation`
	// is the emitter's height above the floor in game pixels: it models the
	// light as a point that height above the ground, so the pool of light domes
	// (rounded, lower-peaked) instead of reading as a flat disc. `roofpix` marks
	// the pixels of roofs (255, incl. objects standing on one) and tall exterior
	// shapes (128: tree canopies, lampposts).  When `veto_roof` is true (a light
	// that is itself under a roof) every marked pixel stays dark, so the light
	// never brightens the roof or canopy over it.  When false, marked pixels
	// instead BYPASS the tile mask below: they belong to elevated surfaces a
	// ground-level room fill cannot represent -- a canopy sprite spans many more
	// screen pixels than its single stamped tile -- so they are lit by pure
	// radial falloff, treating the whole shape as a unit (and letting street
	// lamps brighten nearby house roofs).  `is_spill` narrows that: a spill glow
	// (the interior bubble already outside its opening) lights tall shapes (128)
	// whole but never a real roof (255) -- only a true exterior light does that.
	// `dist_bias` (game px) continues another source's
	// falloff: the dome fades as if the light had already travelled that far
	// before reaching the splat centre -- used for spill glows, which are the
	// source's own bubble poking through an opening, not a new light
	// (`radius` stays the REMAINING reach).  When `mask` is non-null it is a
	// world-anchored screen-space occlusion mask (a rectangle `mask_w` x
	// `mask_h` at screen origin (`mask_ox`,`mask_oy`), row stride `mask_lw`):
	// a pixel is only lit where the mask is non-zero, so tall walls contain
	// the light within the room.  The mask is stamped from the tiles' own
	// rendered positions, so it stays fixed to the world as the light moves.
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, const unsigned char* roofpix, int roof_lw, bool veto_roof,
			bool is_spill, const unsigned char* mask, int mask_lw, int mask_ox, int mask_oy, int mask_w, int mask_h);

}    // namespace NaturalLight

#endif
