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

	// Find the first light-passing shape (window, open door, grate) in `chunk`.
	bool Chunk_find_light_passes_through(
			Map_chunk* chunk, int& pass_shape, int& pass_frame, int& pass_match_frame, int& pass_tx, int& pass_ty, int& pass_lift);

	// Can the light at interior tile `src` actually reach an opening in the chunk?
	bool Light_reaches_chunk_opening(Map_chunk* chunk, const Tile_coord& src, Game_object* light_obj);

	// Resolve a light source to a nearby interior tile; reports interior-ness.
	Tile_coord Resolve_interior_light_tile(const Tile_coord& src, bool& interior);

	// Does the roofed enclosure around `start` have a passable gap leading outside?
	bool Enclosure_open_to_outside(const Tile_coord& start);

	// Do `start` and `target` share one interior space?
	bool Tiles_in_same_enclosure(const Tile_coord& start, const Tile_coord& target);

	// Does an actual roof shape cover the light's tile from above?
	bool Light_beneath_roof(Game_object* light_obj);

	// Result of deciding whether one light source reaches the current viewer.
	struct LightVisibility {
		bool blocked           = false;    // Light does not reach the viewer.
		int  crossings         = 0;        // Inside<->outside boundaries crossed.
		bool interior_source   = false;    // Source sits inside a roofed enclosure.
		bool source_can_escape = false;    // Its own enclosure lets light out.
		bool leaks_through_gap = false;    // Escape is via a physical wall gap.
	};

	// Decide whether the light living in `olist` reaches the viewer.
	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening);

	// Spatial-light glow radius in game pixels for an intrinsic brightness.
	int Light_radius(int brightness);

	// Palette tier for a brightness: 0 = candle, 1 = single light, 2 = many.
	int Light_tier(int brightness);

	// The z-level of the roof over the given absolute tile, or floor + 5 when
	// none (`found` reports which).
	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift, bool* found = nullptr);

	// Is this object part of a building's wall shell?  Drives the kind channel.
	bool Object_is_wall_face(Game_object* obj);

	// Does this object's current frame let light pass (window pane, grate)?
	bool Object_passes_light(Game_object* obj);

	// One spill opening found by Build_light_shadow_grid: the tile just outside
	// the opening, its transmission percent, the source's storey, and the fill's
	// PATH distance to the opening in tiles.
	struct Light_spill {
		Tile_coord tile;
		int        percent;
		int        floor;
		int        path = 0;
		// The opening's own wall tile (world coords) for window/grate spills;
		// tx < 0 for doorway/roof-edge/well exits (no pass-through shape).
		Tile_coord opening = Tile_coord(-1, -1, -1);
	};

	// Call once per world render: replenishes the flood cache's refresh budget.
	void Flood_cache_frame_begin();

	// Bumped whenever a flood refresh changes a cached room grid.
	uint64_t Flood_content_generation();

	// A light-blocking shape near `t` changed: evict cached floods/verdicts
	// within `radius_tiles`.
	void Invalidate_light_caches_near(const Tile_coord& t, int radius_tiles);

	// A world object was edited: invalidate the light caches around it when it
	// can shape light floods.  Cheap no-op otherwise.
	void Notify_object_edited(Game_object* obj);

	// Build the room-fill grid a light casts ((2*rt+1)^2 cells of flood path
	// distance + 1, 0 = unreached; empty = nothing to gate), the openings it
	// escapes through and, when `ring` is given, the per-wall-cell face
	// arrival distances.
	void Build_light_shadow_grid(
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills,
			bool light_walls = true, std::vector<unsigned char>* ring = nullptr);

	// Build the room-fill grid for a spill glow (same layout), flooded from
	// `start` -- the tile just outside the opening.  A nonzero (cone_dx,
	// cone_dy) unit axis restricts the fill to the outward fan in front of
	// the opening (45 degrees each side); (0,0) floods freely.
	void Build_spill_shadow_grid(
			const Tile_coord& start, int rt, std::vector<unsigned char>& lit, bool light_walls = true,
			std::vector<unsigned char>* ring = nullptr, int cone_dx = 0, int cone_dy = 0);

	// Splat one radial light's dome falloff into the coverage buffer and copy
	// the brightened source pixels it wins.  Parameters at the definition.
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer = false, int clip_x0 = 0,
			int clip_y0 = 0, int clip_x1 = -1, int clip_y1 = -1, const unsigned char* kindpix = nullptr, int kind_lw = 0,
			const unsigned char* footdx = nullptr, const unsigned char* footdy = nullptr, int foot_lw = 0,
			const unsigned char* ring = nullptr, int av_fx = 0, int av_fy = 0);

}    // namespace NaturalLight

#endif
