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

#include "rect.h"
#include "tiles.h"

#include <cstdint>
#include <memory>
#include <vector>

class Actor;
class Game_map;
class Game_object;
class Image_buffer8;
class Game_window;
class Map_chunk;

namespace NaturalLight {

	// Values of Light_layers::roof_light_mask: which surface owns a screen pixel.
	namespace Mask {
		constexpr int Clear      = 0;      // Ground: sampled from the room field.
		constexpr int Tall       = 128;    // + storey: whole-unit tall shape (deck object, merlon, upper wall).
		constexpr int Shell_face = 132;    // Exterior wall face; with Kind::Ground it is window glass.
		constexpr int Shell_top  = 134;    // Wall top: horizontal, so lit from either side.
		constexpr int Face_south = 135;    // Interior wall face lit only from the room to its south.
		constexpr int Face_east  = 136;    // Interior wall face lit only from the room to its east.
		constexpr int Tall_south = 136;    // + storey (1..3): a deck construct's SOUTH face; Tall to lights south of it.
		constexpr int Slab       = 140;    // + storey: floor slab surface (a room's ceiling from below).
		constexpr int Slab_edge  = 144;    // + storey: floor slab's down-facing thickness strip.
		constexpr int Low_face
				= 148;    // + side (0 south, 1 east): low wall face; Clear except to a spill/interior light walled by it.
		constexpr int Roof    = 255;
		constexpr int Storeys = 4;

		constexpr bool Is_tall(int m) {
			return m >= Tall && m < Tall + Storeys;
		}

		constexpr bool Is_tall_south(int m) {
			return m > Tall_south && m < Tall_south + Storeys;
		}

		constexpr bool Is_slab(int m) {
			return m >= Slab && m < Slab + Storeys;
		}

		constexpr bool Is_slab_edge(int m) {
			return m >= Slab_edge && m < Slab_edge + Storeys;
		}

		constexpr bool Is_low_face(int m) {
			return m == Low_face || m == Low_face + 1;
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

	// Values of Light_layers::light_kind_mask: what kind of sprite owns a pixel.
	namespace Kind {
		constexpr int Ground        = 0;    // Floors, panes, open doors, loose art.
		constexpr int Wall_face     = 1;
		constexpr int Object        = 2;
		constexpr int Object_top    = 3;    // An object's flat top, lit like a deck.
		constexpr int Object_south  = 4;    // An object's south / east face: otherwise Object.
		constexpr int Object_east   = 5;
		constexpr int Object_cap    = 6;    // A thin wall piece's flat top, lit like a house wall's top (N-S run).
		constexpr int Object_cap_ew = 7;    // The same for an E-W run.

		inline bool Is_object(int k) {
			return k == Object || k == Object_south || k == Object_east;
		}

		inline bool Is_cap(int k) {
			return k == Object_cap || k == Object_cap_ew;
		}
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
		constexpr int           S     = 0;
		constexpr int           E     = 1;
		constexpr int           N     = 2;
		constexpr int           W     = 3;
		constexpr int           Bytes = 4;
		constexpr unsigned char Flag  = 0x80;

		constexpr int Wall_flag      = S;    // Cell is a wall face.
		constexpr int Apron_flag     = E;    // Escaped open cell kept for anchor-shifted sampling only.
		constexpr int Escaped_flag   = N;    // Wall touched by fill that had escaped outdoors.
		constexpr int Inside_flag    = W;    // Wall touched by interior fill.
		constexpr int Handoff_flag   = N;    // Open escaped cell stripped from the field: the spills own it.
		constexpr int Mouthed_flag   = W;    // Spill grids: open cell the fan reached inside ANOTHER roofed passage.
		constexpr int Threshold_flag = W;    // Outdoor lights' grids: an opening's own tile (the fill stops there).

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

		inline bool Is_handoff(const unsigned char* ring, size_t cell) {
			return !Is_wall(ring, cell) && Has(ring, cell, Handoff_flag);
		}

		// A wall the fill touched only after escaping: a neighbour's masonry.
		inline bool Is_foreign_wall(const unsigned char* ring, size_t cell) {
			return Is_wall(ring, cell) && Has(ring, cell, Escaped_flag) && !Has(ring, cell, Inside_flag);
		}
	}    // namespace Ring

	bool Chunk_find_light_passes_through(Map_chunk* chunk);

	bool Light_reaches_chunk_opening(Map_chunk* chunk, const Tile_coord& src, Game_object* light_obj);

	Tile_coord Resolve_interior_light_tile(const Tile_coord& src, bool& interior);

	bool Enclosure_open_to_outside(const Tile_coord& start);

	bool Tiles_in_same_enclosure(const Tile_coord& start, const Tile_coord& target);

	bool Light_beneath_roof(Game_object* light_obj);

	// Result of deciding whether one light source reaches the current viewer.
	struct LightVisibility {
		bool blocked           = false;    // Light does not reach the viewer.
		bool interior_source   = false;    // Source sits inside a roofed enclosure.
		bool source_can_escape = false;    // Its own enclosure lets light out.
	};

	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening);

	int Light_radius(int brightness);

	int Light_tier(int brightness);

	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift, bool* found = nullptr);

	int Light_anchor_roof_z(Game_map* gmap, int tx, int ty, int lift, int rt, const unsigned char* lit, bool* found = nullptr);

	int Viewer_ceiling_lift();

	bool Object_is_wall_face(Game_object* obj);

	bool Object_passes_light(Game_object* obj);

	constexpr int Light_spill_no_cone = -2;    // Light_spill::opening.tx: an unconstrained inward pool.

	// One spill opening found by Build_light_shadow_grid: the tile just outside
	// the opening, its transmission percent, the source's storey, and the fill's
	// PATH distance to the opening in tiles.
	struct Light_spill {
		Tile_coord tile;
		int        percent;
		int        floor;
		int        path = 0;
		// The opening's own wall tile (world coords) for window/grate spills;
		// tx < 0 for doorway/roof-edge/well exits (no pass-through shape);
		// tx == Light_spill_no_cone for light passing IN through an upper window (unconstrained pool).
		Tile_coord opening = Tile_coord(-1, -1, -1);
		// Top lift of the overlooked floor-roof deck it lands on (exterior surface
		// under open sky, exempt from the inside-viewer storey veto); -1 = none.
		int deck_z = -1;
		// Opening tiles beside `tile` along the wall, toward -x/-y and +x/+y.
		int mouth_neg = 0;
		int mouth_pos = 0;
		// Inward pools: the step from the opening into the room.
		int inward_nx = 0;
		int inward_ny = 0;
	};

	bool Light_spill_upward(const Light_spill& s, int light_tz);

	int Light_deck_spill_storeys(const std::vector<Light_spill>& spills, int light_tz);

	// One stamped sprite's screen art box plus its paint origin (the foot).
	// A tall sprite's art reaches far above the pool that lights its foot, and
	// the per-pixel foot channel is clamped by the shear, so only the object's
	// own box can say which pixels belong to it.
	struct Sprite_box {
		short x0, y0, x1, y1;
		short fx, fy;
		short shape, frame;                // Its art and shape info (owner tests).
		short xts = 1, yts = 1, fz = 0;    // Footprint tiles; fz = 4 * lift (fx + fz is the z=0 foot).
		bool  wall     = false;            // Wall-face art: only wall_face_side reads it.
		bool  building = false;            // Architecture (Shape_info::building): lit per pixel, never as a unit.
		short strip    = 0;                // > 0: an exterior slab's facade strip (its width in px), not a wall.
	};

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

	void Stamp_surface(const Surface_target& target, Game_object* obj, int sx, int sy);

	void Flood_cache_frame_begin();

	uint64_t Flood_content_generation();

	void Invalidate_light_caches_near(const Tile_coord& t, int radius_tiles);

	void Notify_object_edited(Game_object* obj);

	void Build_light_shadow_grid(
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills,
			bool light_walls = true, std::vector<unsigned char>* ring = nullptr);

	void Build_spill_shadow_grid(
			const Tile_coord& start, int rt, std::vector<unsigned char>& lit, bool light_walls = true,
			std::vector<unsigned char>* ring = nullptr, int cone_dx = 0, int cone_dy = 0, int cone_mouth = 0,
			std::vector<Light_spill>* arrivals = nullptr);

	int  Light_grid_rt(int radius);
	int  Light_screen_reach(Game_object* light_obj, int brightness);
	int  Carried_light_brightness(Actor* carrier);
	void Queue_light(Game_object* light_obj, int brightness, bool moving, bool queue_main = true, bool queue_spills = true);
	bool Viewer_enclosure_sealed();

	void Queue_spill_renders(
			const Tile_coord& ltile, int radius, int tier, int elevation, const std::vector<Light_spill>& spills, bool moving);

	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer = false, int clip_x0 = 0,
			int clip_y0 = 0, int clip_x1 = -1, int clip_y1 = -1, const unsigned char* kindpix = nullptr, int kind_lw = 0,
			const unsigned char* footdx = nullptr, const unsigned char* footdy = nullptr, int foot_lw = 0,
			const unsigned char* ring = nullptr, int av_fx = 0, int av_fy = 0, const Sprite_box* sprites = nullptr,
			int nsprites = 0, int spill_deck_z = -1, int cone_dx = 0, int cone_dy = 0, int cone_mouth = 0, int light_tx = 0,
			int light_ty = 0, TileRect* wrote = nullptr);

	int Light_tile_alpha(
			const unsigned char* grid, int grid_rt, const unsigned char* ring, int ltx, int lty, int tx, int ty, int radius,
			int elevation, int dist_bias, int intensity_pct);

	// One light queued for this frame's light layers (Light_layers::add_render).
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
		// Top lift of the overlooked floor-roof deck the spill lands on (-1 = none):
		// exempt from the inside-viewer storey veto, that deck is what it exists to light.
		int spill_deck_z = -1;
		// The fan's outward axis (unit, one component), as handed to
		// Build_spill_shadow_grid: the splat needs it to fade the fan's edge.
		int cone_dx = 0, cone_dy = 0;
		// Opening width around the apex, packed as for Build_spill_shadow_grid.
		int cone_mouth = 0;
		// True for a light that moves with the Avatar (carried torch and its
		// spills): cached apart from the static coverage, so it never
		// invalidates it.
		bool moving = false;
		// Room-fill grid ((2*rt+1) square) from Build_light_shadow_grid: light
		// floods the room bounded by tall walls.  Empty = no gating.
		std::vector<unsigned char> lit;
		// Per-wall-cell face arrival distances (4 bytes/cell [S,E,N,W]; see
		// Flood_room_grid), consumed by the splat's face/object rules.
		std::vector<unsigned char> ring;
		// Bit per storey this light's DECK spills land on: those spills light that storey's floor, the light
		// itself does not (its own fill follows its storey's layout, not that one's).
		int deck_spill_storeys = 0;
		// A DECK spill measures its falloff straight from its source light (tile offset source - spill), as if
		// the light passed through the floor dimmed: tiles nearer the lamp are brighter, not those nearer the
		// spill's landing tile beyond the opening.
		bool from_source = false;
		int  source_dx   = 0;
		int  source_dy   = 0;
		// An inward pool (light in through an opening): from_source brightness, confined to a circle of the
		// spill's own radius around the opening (its pool shape), soft over the last tiles.
		bool pool_shape = false;
		// Its opening's tile, relative to the spill tile: the pool lights only what the source sees through it.
		int pool_wdx = 0;
		int pool_wdy = 0;
		int pool_nx  = 0;    // Step from the opening into the room.
		int pool_ny  = 0;
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

	TileRect Splat_light(
			const Splat_frame& f, const Light_render_info& lr, unsigned char* cov, int cx0 = 0, int cy0 = 0, int cx1 = -1,
			int cy1 = -1);

	void Relight_sprites(
			const Splat_frame& f, const std::vector<Light_render_info>& light_renders, int t, const unsigned char* cov,
			std::vector<unsigned char>& light_scratch_cov, bool& has_moving);

	void Soften_face_edges(
			const Splat_frame& f, const unsigned char* cov, std::vector<unsigned char>& out, unsigned char* dstpix, int dst_lw);

	// The natural-light overlay layers and everything they keep between frames: the lights queued this frame,
	// each brightness tier's cached coverage and the per-pixel surface channels the world render stamps.
	// Owned by Game_window.
	class Light_layers {
	public:
		Light_layers();
		~Light_layers();
		Light_layers(const Light_layers&)            = delete;
		Light_layers& operator=(const Light_layers&) = delete;

		void               clear_renders();
		Light_render_info& add_render(
				int sx, int sy, int radius, int tier, int elevation, int rt, int ltx, int lty, int ltz,
				std::vector<unsigned char> lit, bool mask_roof = false, int dist_bias = 0, bool is_spill = false,
				int spill_percent = 100, int spill_floor = 0, bool moving = false, std::vector<unsigned char> ring = {},
				int spill_deck_z = -1, int cone_dx = 0, int cone_dy = 0, int cone_mouth = 0);
		void begin_roof_mask(Game_window& gw);
		void update_roof_mask(Game_object* obj, int sx, int sy);
		void build(Game_window& gw);
		void destroy(Game_window& gw);
		bool any_visible(Game_window& gw) const;
		bool take_repaint_pending();

	private:
		// Spatial light overlays: per-frame list of light sources (in ibuf
		// game-pixel coords) that the light layers brighten around, plus the three
		// tier layer handles and a reusable coverage (radial-alpha) scratch buffer.
		std::vector<Light_render_info> light_renders;
		int                            light_layer_handles[3] = {-1, -1, -1};
		int                            light_layer_w          = -1;
		int                            light_layer_h          = -1;
		int                            light_layer_palnum     = -2;
		// Per-tier splat reuse (build): the coverage cache holds only
		// the STATIC (world-pinned) lights.  While a tier's static set is unchanged
		// relative to the world, a view scroll just translates the cached mask by
		// the scroll delta and re-splats only the lights exposed at a screen edge;
		// moving lights (carried torch + spills) have their own cache below and are
		// composited on top into a scratch copy each frame.
		std::vector<unsigned char> light_tier_cov[3];
		std::vector<TileRect>      light_tier_rects[3];         // Static coverage rows written.
		std::vector<TileRect>      light_tier_mask_rects[3];    // Next-frame roof-mask clips.
		// Unclamped splat bounds per static light (screen coords, in the tier's
		// light order); source of truth for the two rect lists above.
		std::vector<TileRect> light_tier_bounds[3];
		// First static light's screen position at the last rebuild/translate.
		int      light_tier_anchor_x[3] = {0, 0, 0};
		int      light_tier_anchor_y[3] = {0, 0, 0};
		uint64_t light_tier_sig[3]      = {0, 0, 0};
		// Rebuild countdown after the static set changes (see build).
		int light_tier_resample[3] = {0, 0, 0};
		// The previous frame's roof/kind masks (stride = width).
		std::vector<unsigned char> light_prev_roof;
		std::vector<unsigned char> light_prev_kind;
		// Composite scratch: static coverage + this frame's moving lights.
		std::vector<unsigned char> light_scratch_cov;
		std::vector<unsigned char> light_soft_cov;    // Final coverage with soft wall-face edges.
		// Moving-light overlay cache: coverage of just the moving lights (see build).
		std::vector<unsigned char> light_tier_mov_cov[3];
		std::vector<TileRect>      light_tier_mov_bounds[3];
		uint64_t                   light_tier_mov_sig[3] = {0, 0, 0};
		// World pixel of the screen origin when the moving coverage was splatted.
		int light_tier_mov_ox[3] = {0, 0, 0};
		int light_tier_mov_oy[3] = {0, 0, 0};
		// Scroll strips not yet re-splatted (edge-extended meanwhile), per tier: static and moving coverage.
		int light_tier_debt_x[3]     = {0, 0, 0};
		int light_tier_debt_y[3]     = {0, 0, 0};
		int light_tier_mov_debt_x[3] = {0, 0, 0};
		int light_tier_mov_debt_y[3] = {0, 0, 0};
		// Global roof-pixel mask, same pixel geometry as the world ibuf: built
		// during the world render (update_roof_mask), consumed by
		// build / Splat_radial_light.
		std::unique_ptr<Image_buffer8> roof_light_mask;
		bool                           roof_light_mask_active = false;
		// Last frame's splat rectangles (grown by a margin): update_roof_mask
		// only paints the mask where a sprite touches one of them.
		std::vector<TileRect> light_mask_rects;
		// Surface-identity channels, stamped beside the roof mask in painter's
		// order: per-pixel owner KIND (Kind) plus the
		// screen offset from the pixel to its owner's per-tile foot at z=0 (one
		// byte each, offset + Foot_bias).  Consumed by Splat_radial_light.
		std::unique_ptr<Image_buffer8> light_kind_mask;
		std::vector<unsigned char>     light_foot_dx, light_foot_dy;
		// This frame's stamped sprite art boxes: a tall sprite's crown sits far
		// outside the pool that lights its foot, and only its own box identifies
		// those pixels (the foot channel is shear-clamped).
		std::vector<Sprite_box> light_sprite_boxes;
		// A static light set changed this frame: schedule a full repaint AFTER
		// paint_dirty's clear_dirty so the roof mask gets stamped under the new
		// light rects next frame (see build's full-rebuild path).
		bool light_repaint_pending = false;
	};

}    // namespace NaturalLight

#endif
