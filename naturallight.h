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
class Image_buffer8;
class TileRect;
class Map_chunk;

namespace NaturalLight {

	// Values of Game_window::roof_light_mask: which surface owns a screen pixel.
	namespace Mask {
		constexpr int Clear      = 0;      // Ground: sampled from the room field.
		constexpr int Tall       = 128;    // + storey: whole-unit tall shape (deck object, merlon, upper wall).
		constexpr int Shell_face = 132;    // Exterior wall face; with Kind::Ground it is window glass.
		constexpr int Shell_top  = 134;    // Wall top: horizontal, so lit from either side.
		constexpr int Face_south = 135;    // Interior wall face lit only from the room to its south.
		constexpr int Face_east  = 136;    // Interior wall face lit only from the room to its east.
		constexpr int Slab       = 140;    // + storey: floor slab surface (a room's ceiling from below).
		constexpr int Slab_edge  = 144;    // + storey: floor slab's down-facing thickness strip.
		constexpr int Roof       = 255;
		constexpr int Storeys    = 4;

		constexpr bool Is_tall(int m) {
			return m >= Tall && m < Tall + Storeys;
		}

		constexpr bool Is_slab(int m) {
			return m >= Slab && m < Slab + Storeys;
		}

		constexpr bool Is_slab_edge(int m) {
			return m >= Slab_edge && m < Slab_edge + Storeys;
		}

		// Any wall face, exterior or interior.
		constexpr bool Is_face(int m) {
			return m == Shell_face || m == Face_south || m == Face_east;
		}

		// Exterior wall masonry (face or top): lit from all sides as one unit.
		constexpr bool Is_shell(int m) {
			return m == Shell_face || m == Shell_top;
		}

		// Any wall mark: a face of either side, or a top.
		constexpr bool Is_wall(int m) {
			return Is_face(m) || m == Shell_top;
		}
	}    // namespace Mask

	// Values of Game_window::light_kind_mask: what kind of sprite owns a pixel.
	namespace Kind {
		constexpr int Ground     = 0;    // Floors, panes, open doors, loose art.
		constexpr int Wall_face  = 1;
		constexpr int Object     = 2;
		constexpr int Object_top = 3;    // An object's flat top, lit like a deck.
	}    // namespace Kind

	// light_foot_dx/dy store a signed screen offset as an unsigned byte.
	constexpr int Foot_bias = 128;

	// Room-fill grid cell: path distance + 1 in the low 7 bits (0 = unreached).
	namespace Grid {
		// Room grid: floor hole of a down-well.  Spill grid: wall cell.
		constexpr unsigned char Flag = 0x80;

		constexpr int Dist(unsigned char c) {
			return c & 0x7f;
		}
	}    // namespace Grid

	// Ring channel: per grid cell one byte per face [S, E, N, W] holding that
	// face's arrival distance + 1 in the low 7 bits; each byte's high bit is a
	// separate cell flag.
	namespace Ring {
		constexpr int S     = 0;
		constexpr int E     = 1;
		constexpr int N     = 2;
		constexpr int W     = 3;
		constexpr int Bytes = 4;
		constexpr unsigned char Flag = 0x80;

		constexpr int Wall_flag    = S;    // Cell is a wall face.
		constexpr int Apron_flag   = E;    // Escaped open cell kept for anchor-shifted sampling only.
		constexpr int Escaped_flag = N;    // Wall touched by fill that had escaped outdoors.
		constexpr int Inside_flag  = W;    // Wall touched by interior fill.

		constexpr int Dist(unsigned char c) {
			return c & 0x7f;
		}

		inline bool Has(const unsigned char* ring, size_t cell, int flag) {
			return (ring[cell * Bytes + flag] & Flag) != 0;
		}

		inline void Set(unsigned char* ring, size_t cell, int flag) {
			ring[cell * Bytes + flag] |= Flag;
		}

		inline bool Is_wall(const unsigned char* ring, size_t cell) {
			return Has(ring, cell, Wall_flag);
		}

		inline bool Is_apron(const unsigned char* ring, size_t cell) {
			return !Is_wall(ring, cell) && Has(ring, cell, Apron_flag);
		}

		// A wall the fill touched only after escaping: a neighbour's masonry.
		inline bool Is_foreign_wall(const unsigned char* ring, size_t cell) {
			return Is_wall(ring, cell) && Has(ring, cell, Escaped_flag) && !Has(ring, cell, Inside_flag);
		}
	}    // namespace Ring

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
		bool interior_source   = false;    // Source sits inside a roofed enclosure.
		bool source_can_escape = false;    // Its own enclosure lets light out.
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
		// Placed ON an overlooked floor-roof deck (exterior surface under open
		// sky), so the inside-viewer storey veto must not apply to it.
		bool on_deck = false;
		// Opening tiles beside `tile` along the wall, toward -x/-y and +x/+y.
		int mouth_neg = 0;
		int mouth_pos = 0;
	};

	// One stamped sprite's screen art box plus its paint origin (the foot).
	// A tall sprite's art reaches far above the pool that lights its foot, and
	// the per-pixel foot channel is clamped by the shear, so only the object's
	// own box can say which pixels belong to it.
	struct Sprite_box {
		short x0, y0, x1, y1;
		short fx, fy;
	};

	// Grow a splat's unclamped screen bbox up-left by the art boxes of the
	// sprites rooted inside it.  The caller's bounds bookkeeping MUST apply
	// this too: coverage written outside the recorded bounds is never cleared,
	// and the scroll path then translates those stale pixels across the screen.
	void Reach_extend_box(int& x0, int& y0, int x1, int y1, const Sprite_box* sprites, int nsprites);

	// The per-pixel surface channels the world render stamps for the splat
	// (owned by Game_window; see Mask, Kind and Foot_bias).
	struct Surface_target {
		Image_buffer8*               mask;
		Image_buffer8*               kind;
		std::vector<unsigned char>&  foot_dx;
		std::vector<unsigned char>&  foot_dy;
		std::vector<Sprite_box>&     sprites;
		const std::vector<TileRect>& cull_rects;    // Last frame's splat rects: skip sprites outside.
	};

	// Stamp one painted object into the surface channels.  Objects paint
	// back-to-front, so the topmost sprite at each pixel owns it.
	void Stamp_surface(const Surface_target& target, Game_object* obj, int sx, int sy);

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
			std::vector<unsigned char>* ring = nullptr, int cone_dx = 0, int cone_dy = 0, int cone_mouth = 0);

	// Queue a spill glow (Game_window::add_light_render) through each opening
	// the light at `ltile` reached: remaining radius, continued falloff,
	// gated by its own fan-shaped spill grid.
	void Queue_spill_renders(
			const Tile_coord& ltile, int radius, int tier, int elevation, const std::vector<Light_spill>& spills,
			bool moving);

	// Splat one radial light's dome falloff into the coverage buffer and copy
	// the brightened source pixels it wins.  Parameters at the definition.
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer = false, int clip_x0 = 0,
			int clip_y0 = 0, int clip_x1 = -1, int clip_y1 = -1, const unsigned char* kindpix = nullptr, int kind_lw = 0,
			const unsigned char* footdx = nullptr, const unsigned char* footdy = nullptr, int foot_lw = 0,
			const unsigned char* ring = nullptr, int av_fx = 0, int av_fy = 0, const Sprite_box* sprites = nullptr,
			int nsprites = 0, bool spill_on_deck = false, int cone_dx = 0, int cone_dy = 0, int cone_mouth = 0);

	// The dome brightness this light puts on the GROUND of one world tile,
	// straight from its fill -- no screen mask, no sprite.  For subjects whose
	// own pixels are all masked (an actor under a drawn roof or against a shell
	// wall) yet who stand in the light.  Definition carries the details.
	int Light_tile_alpha(
			const unsigned char* grid, int grid_rt, const unsigned char* ring, int ltx, int lty, int tx, int ty, int radius,
			int elevation, int dist_bias, int intensity_pct);

	// One light queued for this frame's light layers (Game_window::add_light_render).
	struct Light_render_info {
		int sx, sy;    // Center of the light in game-pixel (ibuf) coords.
		int radius;    // Radius in game pixels.
		int tier;      // 0 = candle, 1 = single light, 2 = many lights.
		// Emitter height above the floor in game px; rounds the dome falloff
		// in Splat_radial_light.
		int elevation = 0;
		// Radius in tiles (grid half-size); 0 when there is no occlusion grid.
		int rt = 0;
		// The light's own tile (world coords): anchors the room-fill grid on
		// screen so the mask stays fixed to the walls as the light moves.
		int ltx = 0, lty = 0, ltz = 0;
		// True if the light is itself under a roof (Light_beneath_roof): roof
		// pixels stay dark for it.
		bool mask_roof = false;
		// Spill glows: distance (game px) already travelled to the spill
		// point; the splat continues the source's falloff.  0 for real sources.
		int dist_bias = 0;
		// True for a spill glow (semantics in Splat_radial_light).
		bool is_spill = false;
		// The opening's transmission percent (1..100); real sources pass 100.
		int spill_percent = 100;
		// Spill glows: the source's storey (tz / 5), gating storeyed roof
		// mask pixels (NaturalLight::Mask) in Splat_radial_light.
		int spill_floor = 0;
		// Spill placed on an overlooked floor-roof deck: exempt from the
		// inside-viewer storey veto, that deck is what it exists to light.
		bool spill_on_deck = false;
		// The fan's outward axis (unit, one component), as handed to
		// Build_spill_shadow_grid: the splat needs it to fade the fan's edge.
		int cone_dx = 0, cone_dy = 0;
		// Opening width around the apex, packed as for Build_spill_shadow_grid.
		int cone_mouth = 0;
		// True for a light that moves with the Avatar (carried torch and its
		// spills): splatted fresh every frame on top of the cached static mask
		// instead of invalidating it.
		bool moving = false;
		// Room-fill grid ((2*rt+1) square) from Build_light_shadow_grid: light
		// floods the room bounded by tall walls.  Empty = no gating.
		std::vector<unsigned char> lit;
		// Per-wall-cell face arrival distances (4 bytes/cell [S,E,N,W]; see
		// Flood_room_grid), consumed by the splat's face/object rules.
		std::vector<unsigned char> ring;
	};

	// One light tier's splat inputs: the world image, the layer copy it
	// brightens, the surface channels and the viewer.
	struct Splat_frame {
		int                            W, H;
		unsigned char*                 dstpix;
		int                            dst_lw;
		const unsigned char*           srcpix;
		int                            src_lw;
		const unsigned char*           roofpix;
		int                            roof_lw;
		const unsigned char*           kindpix;
		int                            kind_lw;
		const unsigned char*           footdxp;
		const unsigned char*           footdyp;
		int                            foot_lw;
		const std::vector<Sprite_box>& sprites;
		bool                           inside;
		int                            av_fx, av_fy;    // Viewer's z=0 foot on screen.
		int                            skip_above_actor;
	};

	// Splat one light into `cov` and the layer's pixels; returns the splat's
	// unclamped screen bounds.  The clip window restricts the pixels written.
	TileRect Splat_light(
			const Splat_frame& f, const Light_render_info& lr, unsigned char* cov, int cx0 = 0, int cy0 = 0, int cx1 = -1,
			int cy1 = -1);

	// Lift actors and placed light sprites to the light at their feet, in
	// the scratch coverage only (seeded from `cov` on first use).
	void Relight_sprites(
			const Splat_frame& f, const std::vector<Light_render_info>& light_renders, int t, const unsigned char* cov,
			std::vector<unsigned char>& light_scratch_cov, bool& has_moving);

}    // namespace NaturalLight

#endif
