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

#include <cstdint>
#include <vector>

class Game_map;
class Game_object;
class Map_chunk;

namespace NaturalLight {

	// Find the first shape in `chunk` that light passes through (window, open
	// door, grate, ...). Reports the shape/frame/tile/lift of that opening.
	bool Chunk_find_light_passes_through(
			Map_chunk* chunk, int& pass_shape, int& pass_frame, int& pass_match_frame, int& pass_tx, int& pass_ty, int& pass_lift);

	// True if the light at interior tile `src` has a pathfound route to a
	// light_passes_through opening (window / open door / grate) in the chunk.
	bool Light_reaches_chunk_opening(Map_chunk* chunk, const Tile_coord& src, Game_object* light_obj);

	// Resolve a light source to a nearby interior tile (a wall torch's own
	// tile is the wall itself) and report whether it is an interior source.
	Tile_coord Resolve_interior_light_tile(const Tile_coord& src, bool& interior);

	// Bounded flood fill from `start`: true if the roofed enclosure it stands
	// in has a passable gap in its walls leading outside.
	bool Enclosure_open_to_outside(const Tile_coord& start);

	// Bounded flood fill through non-wall tiles: true if `start` and `target`
	// share one interior space (even across a chunk boundary).
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
	// reaches the viewer, composing the enclosure/opening tests above.  The
	// flags are per-frame / per-chunk facts the caller already computed.
	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening);

	// Spatial-light glow radius in game pixels for an intrinsic brightness.
	int Light_radius(int brightness);

	// Palette tier for a brightness: 0 = candle, 1 = single light, 2 = many.
	int Light_tier(int brightness);

	// The z-level of the roof over the given absolute tile -- the lowest solid
	// cover above the light -- or floor + 5, the classic wall-top threshold,
	// when none is found.  `found` (optional) reports whether an actual ceiling
	// shape was seen, as opposed to the floor + 5 fallback (a dungeon's rock
	// ceiling is a plain solid, not a roof shape).
	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift, bool* found = nullptr);

	// One spill opening found by Build_light_shadow_grid: the tile just outside
	// the opening the light escapes through, the opening's transmission percent
	// (1..100; doorways are open air and carry 100), and the storey the source
	// light stands on (tz / 5).
	struct Light_spill {
		Tile_coord tile;
		int        percent;
		int        floor;
	};

	// Call once per world render, before the chunks are painted: replenishes
	// the flood cache's per-frame refresh budget (see Build_light_shadow_grid).
	void Flood_cache_frame_begin();

	// Bumped whenever a flood refresh actually changes a cached room grid (a
	// door opened or closed); mixed into the light-layer signatures so cached
	// coverage rebuilds exactly when flood content changes.
	uint64_t Flood_content_generation();

	// Build the room-fill grid a light casts.  `lit` becomes a (2*rt+1)^2 grid
	// centred on the light's tile; each cell is 0 (light cannot reach that
	// tile) or the flood PATH distance + 1 in tiles.  An empty `lit` means
	// nothing to gate (fully lit).  Openings the fill reaches are reported in
	// `spills`; `light_walls` lights the room's one-tile wall ring.  Details at
	// the definition.
	void Build_light_shadow_grid(
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills,
			bool light_walls = true);

	// Build the room-fill grid for a spill glow (same layout), flooded from
	// `start` -- the tile just outside the opening the light escapes through.
	void Build_spill_shadow_grid(const Tile_coord& start, int rt, std::vector<unsigned char>& lit);

	// Splat one radial light's soft dome falloff into the coverage buffer,
	// copying the brightened source pixel wherever this light is the strongest
	// contributor so far.  `cov` is a contiguous W*H buffer (row stride W);
	// `dst`, `src` and `roof` use their own line widths.  `elevation` (game px)
	// rounds the dome; `dist_bias` continues another source's falloff (spill
	// glows); `intensity_pct` (1..100) scales the whole dome (the opening's
	// transmission).  `roofpix` marks roofs (255) and tall / upper-storey
	// surfaces (128 + storey): `veto_roof` keeps marked pixels dark under an
	// interior light, while `is_spill` with `spill_floor` / `light_top_storey`
	// gates what a spill or under-roof light may still reach (see the pixel
	// loop in the definition).  When `grid` is non-null it is the light's
	// room-fill grid and the light renders as a propagated field pinned to the
	// tile lattice at (`grid_fx`,`grid_fy`) instead of a free dome.  A non-empty
	// clip window (clip_x1/y1 >= 0) restricts the pixels written -- used to
	// patch only the strips a scroll-translation vacated.
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, const unsigned char* grid, int grid_rt,
			int grid_fx, int grid_fy, int clip_x0 = 0, int clip_y0 = 0, int clip_x1 = -1, int clip_y1 = -1);

}    // namespace NaturalLight

#endif
