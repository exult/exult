/*
 *  naturallight.cc - Geometry helpers for the spatial ("natural") lighting.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "naturallight.h"

#include "chunks.h"
#include "gamemap.h"
#include "gamewin.h"
#include "objiter.h"
#include "objs.h"
#include "path.h"
#include "paths.h"
#include "shapeinf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <tuple>
#include <utility>

namespace {

	// Does the given shape/frame let light pass through (window, open door,
	// grate, ...)?  Explicit per-frame entries take precedence over a wildcard.
	// On a hit, `percent` is the matched entry's transmission (1..100): how much
	// of the light the opening lets through.  An entry with 0% blocks light
	// entirely -- the shape is treated as if it were not listed at all (returns
	// false), so it stays a wall and never becomes a spill opening.
	bool Shape_light_passes_through_strict(
			const Shape_info& info, int frame, int& match_frame, bool& has_explicit, bool& has_wildcard, int& percent) {
		const int want_frame = frame & 31;
		has_explicit         = false;
		has_wildcard         = false;
		percent              = 100;
		bool explicit_hit    = false;
		int  explicit_pct    = 100;
		int  wildcard_pct    = 100;

		for (const auto& ent : info.get_light_passes_info()) {
			if (ent.is_invalid()) {
				continue;
			}
			const int ent_frame = ent.get_frame();
			if (ent_frame >= 0) {
				has_explicit = true;
				if (ent_frame == want_frame) {
					explicit_hit = true;
					explicit_pct = ent.get_percent();
				}
			} else if (ent_frame == -1) {
				has_wildcard = true;
				wildcard_pct = ent.get_percent();
			}
		}

		const bool hit = has_explicit ? explicit_hit : has_wildcard;
		match_frame    = hit ? (has_explicit ? want_frame : -1) : -2;
		if (hit) {
			percent = has_explicit ? explicit_pct : wildcard_pct;
			if (percent <= 0) {
				// 0%: the entry says this shape blocks light entirely.
				match_frame = -2;
				return false;
			}
		}
		return hit;
	}

	inline int Light_tile_norm(int t) {
		return (t % c_num_tiles + c_num_tiles) % c_num_tiles;
	}

	// Is the given absolute tile under a roof (i.e. inside a building)?  Tested
	// from the floor (lift 0), NOT the light's own lift: is_roof() searches from
	// lift+4, so an elevated source (candle on a table, a torch high on a wall, a
	// hanging lamp) would start the search above the roof and miss it -- making an
	// interior light read as exterior so it wrongly shines through roof and walls.
	bool Light_tile_roofed(Game_map* gmap, int tx, int ty) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return false;
		}
		return chunk->is_roof(tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, 0) < 31;
	}

	// Is the given absolute tile a full-height wall?  Tested a few tiles above the
	// floor so low furniture (tables, chairs, ...) is not mistaken for a wall.
	bool Light_tile_wall(Game_map* gmap, int tx, int ty) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return false;
		}
		return chunk->is_tile_occupied(tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, 3);
	}

	// Does a light_passes_through shape cover the given absolute tile, making
	// it a SPILL opening (window, grate, glass wall)?  Such a tile stops the
	// room-fill like the wall it sits in, but the escaping light is rendered
	// as a small glow there.  Doors are explicitly NOT spill openings even
	// though their open frames are in the light_passes_through list: an open
	// door leaf is a solid shape beside a doorway the flood already passes
	// through, so a spill glow on it reads as a phantom second light source
	// (and a frame mix-up makes some closed doors glow instead).  Returns the
	// opening's transmission percent (1..100), or 0 when no pass-through shape
	// covers the tile.
	int Light_tile_has_pass_through(Map_chunk* chunk, int tx, int ty) {
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			const Shape_info& info = obj->get_info();
			if (info.is_door()) {
				continue;
			}
			int  match_frame  = -2;
			bool has_explicit = false;
			bool has_wildcard = false;
			int  percent      = 100;
			if (!Shape_light_passes_through_strict(info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
				continue;
			}
			if (obj->get_footprint().has_world_point(tx, ty)) {
				return percent;
			}
		}
		return 0;
	}

	// Is the given absolute tile a light-blocking wall for the spatial lights: a
	// blocking (solid) shape covering the tile that rises from the floor
	// (`floor_z`, the storey floor of the light's room) and reaches up to the
	// room's roof level `roof_z` -- i.e. an actual wall.  Buildings have walls taller than the
	// classic z 5, so the level is not hardcoded; it comes from
	// Map_chunk::is_roof over the light.  Objects sitting at or above the
	// roof level (the roof itself, chimneys, items on the roof) are never walls:
	// the roof's footprint covers every interior tile, so counting it would seal
	// off the whole room.  Objects hanging ABOVE the floor (a wall decoration at
	// lift 2 whose top happens to reach the roof) are not walls either: light
	// passes under them, and their footprint must not punch wall tiles into the
	// room.  Low furniture stays below the roof and lets light
	// pass.  Only non-blocking shapes (windows, open doors -- the
	// light_passes_through list) punch a hole in the mask: they are not solid
	// walls here, and where they share a wall tile Light_tile_pass_opening turns
	// it into a spill opening.
	bool Light_tile_tall_blocker(Game_map* gmap, int tx, int ty, int roof_z, int floor_z) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return false;
		}
		// The old test -- anything solid occupying the level just below the
		// roof -- also counted a pile of stacked furniture as a wall; kept in
		// case the per-shape test below turns out to miss real walls:
		// return chunk->is_tile_occupied(
		// 		tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, roof_z - 1);
		// Temporary diagnostics: EXULT_DEBUG_LIGHT_MASK=1 prints every shape
		// covering a tested tile and why it was (not) counted as a wall,
		// deduplicated per shape/reason so the flood does not flood stderr.
		const bool dbg    = std::getenv("EXULT_DEBUG_LIGHT_MASK") != nullptr;
		auto       report = [&](Game_object* o, const Shape_info& si, const char* why) {
            if (!dbg) {
                return;
            }
            static std::set<std::pair<int, const char*>> seen;
            if (!seen.emplace(o->get_shapenum(), why).second) {
                return;
            }
            std::cerr << "[light-mask] shape=" << o->get_shapenum() << '/' << o->get_framenum() << " at(" << tx << ',' << ty
                      << ") lift=" << o->get_lift() << " h=" << si.get_3d_height()
                      << " class=" << static_cast<int>(si.get_shape_class()) << " solid=" << si.is_solid()
                      << " roofflag=" << si.is_roof() << " roof_z=" << roof_z << " floor_z=" << floor_z << " -> " << why
                      << std::endl;
		};
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			if (obj->as_actor() != nullptr) {
				continue;    // NPCs move around; never count them as walls.
			}
			if (!obj->get_footprint().has_world_point(tx, ty)) {
				continue;    // Not covering this tile.
			}
			const Shape_info& info = obj->get_info();
			if (obj->is_dragable()) {
				// A loose, carryable item (a book on top of a full-height
				// shelf) is never a wall, even where the pile beneath it is
				// solid up to the roof.  Real wall segments -- including
				// shutters sitting on half walls -- are immovable (weight 0
				// or static map fixtures), so they are not skipped here.
				report(obj, info, "skip: dragable item");
				continue;
			}
			{
				// A light_passes_through shape/frame (iron-bar prison door,
				// grate, open door leaf) is never a wall by itself: light
				// passes through it even though it is solid to movement.  A
				// WINDOW tile still reads as a wall -- the wall pieces the
				// window is embedded in qualify on their own -- and then
				// Light_tile_pass_opening turns it into a spill opening; but
				// a freestanding barred door must let the fill flow through
				// instead of sealing the cell.  (A 0% entry blocks light
				// entirely: the strict test returns false for it, so the
				// shape falls through to the normal wall tests below.)
				int  match_frame  = -2;
				bool has_explicit = false;
				bool has_wildcard = false;
				int  percent      = 100;
				if (Shape_light_passes_through_strict(
							info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
					report(obj, info, "skip: light_passes_through");
					continue;
				}
			}
			if (info.is_door() && !obj->is_closed_door()) {
				// A door whose open/closed frames are not (all) covered by the
				// light_passes_through list.  Frame numbering differs between
				// door shapes, so ask the geometry instead: is_closed_door()
				// checks whether the tiles on both sides of the leaf are
				// blocked (a closed door plugs its doorway).  An OPEN door
				// leaf is a solid box standing beside a walkable doorway --
				// light passes through the doorway, so the room is open and
				// the leaf must not read as a wall sealing it.
				report(obj, info, "skip: open door");
				continue;
			}
			if (!info.is_solid() || info.is_roof()) {
				// Only blocking shapes seal the room, and the roof (or its
				// low-hanging eaves) is not a wall.
				report(obj, info, "skip: not solid or roof-flagged");
				continue;
			}
			const int lift = obj->get_lift();
			if (lift >= roof_z) {
				report(obj, info, "skip: at/above roof");
				continue;    // At/above the roof: not part of the room's walls.
			}
			if (lift > floor_z) {
				// Starts above the floor.  A hung object (shield, tapestry,
				// sign) has open space beneath it and light passes under; but
				// a shutter sitting on a half-height wall piece is a real wall
				// segment even though it starts at lift 2.  Shape class cannot
				// tell them apart (both are typically 'has_hp', and far from
				// all wall segments carry the 'building' class), so test the
				// geometry directly: it only blocks light if the space below
				// it at THIS tile is solidly filled all the way down to the
				// floor -- no gap for light to squeeze under.
				bool gap = false;
				for (int z = floor_z; z < lift; ++z) {
					if (!chunk->is_tile_occupied(tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, z)) {
						gap = true;
						break;
					}
				}
				if (gap) {
					report(obj, info, "skip: hung above floor with gap below");
					continue;
				}
			}
			if (lift + info.get_3d_height() < roof_z) {
				report(obj, info, "skip: top below roof");
				continue;    // Top below the roof: light passes over it.
			}
			report(obj, info, "WALL");
			return true;
		}
		return false;
	}

	// Is the given absolute tile covered by a light_passes_through shape
	// (window, open door, grate)?  Such a tile is where light escapes a room
	// even though the wall stack occupies it to full height.  Returns the
	// opening's transmission percent (1..100), or 0 when there is no opening.
	int Light_tile_pass_opening(Game_map* gmap, int tx, int ty) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return 0;
		}
		return Light_tile_has_pass_through(chunk, tx, ty);
	}

}    // namespace

namespace NaturalLight {

	bool Chunk_find_light_passes_through(
			Map_chunk* chunk, int& pass_shape, int& pass_frame, int& pass_match_frame, int& pass_tx, int& pass_ty, int& pass_lift) {
		pass_shape       = -1;
		pass_frame       = -1;
		pass_match_frame = -2;
		pass_tx          = -1;
		pass_ty          = -1;
		pass_lift        = -1;
		if (chunk == nullptr) {
			return false;
		}
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			int  match_frame  = -2;
			bool has_explicit = false;
			bool has_wildcard = false;
			int  percent      = 100;
			if (Shape_light_passes_through_strict(
						obj->get_info(), obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
				if (match_frame == -1 && std::getenv("EXULT_DEBUG_LIGHT_PASS")) {
					std::cerr << "[light-pass-debug] wildcard-hit shape=" << obj->get_shapenum() << '/' << obj->get_framenum()
							  << " explicit=" << (has_explicit ? 1 : 0) << " wildcard=" << (has_wildcard ? 1 : 0) << " frames=";
					const auto& lpv = obj->get_info().get_light_passes_info();
					for (size_t i = 0; i < lpv.size(); ++i) {
						if (i) {
							std::cerr << ',';
						}
						if (lpv[i].is_invalid()) {
							std::cerr << '!';
						}
						std::cerr << lpv[i].get_frame();
					}
					std::cerr << std::endl;
				}
				pass_shape       = obj->get_shapenum();
				pass_frame       = obj->get_framenum();
				pass_match_frame = match_frame;
				pass_tx          = obj->get_tx();
				pass_ty          = obj->get_ty();
				pass_lift        = obj->get_lift();
				return true;
			}
		}
		return false;
	}

	// True if the light at interior tile `src` has an unobstructed path to any
	// light_passes_through shape (window / open door / grate) in the chunk. The
	// chunk-wide "has an opening" test is too coarse: a torch sealed behind an
	// interior wall must not leak out through a window elsewhere in the same chunk.
	// Pathfinding from the source to each opening confirms the light can actually
	// reach it.
	bool Light_reaches_chunk_opening(Map_chunk* chunk, const Tile_coord& src, Game_object* light_obj) {
		if (chunk == nullptr) {
			return false;
		}
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			int  match_frame  = -2;
			bool has_explicit = false;
			bool has_wildcard = false;
			int  percent      = 100;
			if (!Shape_light_passes_through_strict(
						obj->get_info(), obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
				continue;
			}
			Fast_pathfinder_client client(light_obj, obj, 1);
			const auto             result = Find_path(src, obj->get_center_tile(), &client);
			if (result.second) {
				return true;    // A reachable opening: light can escape through it.
			}
		}
		return false;
	}

	// Resolve a light source to a nearby interior tile and report whether it is an
	// interior source. A wall-mounted torch or sconce sits ON the wall, so its own
	// tile is the wall itself: the roof test above it can miss, and -- crucially --
	// an enclosure flood-fill started there escapes straight out through the wall
	// to the exterior, both making a sealed interior light wrongly read as exterior
	// and shine outside. Prefer the source's own floor tile; if that is a wall (or
	// has no roof of its own), fall back to an adjacent roofed, non-wall tile: the
	// room the torch faces. The returned tile is where the enclosure flood begins.
	Tile_coord Resolve_interior_light_tile(const Tile_coord& src, bool& interior) {
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		Tile_coord         base = src;
		base.tz                 = 0;
		if (gmap == nullptr) {
			interior = false;
			return base;
		}
		const bool own_roofed = Light_tile_roofed(gmap, base.tx, base.ty);
		const bool own_wall   = Light_tile_wall(gmap, base.tx, base.ty);
		if (own_roofed && !own_wall) {
			interior = true;
			return base;
		}
		// The source tile is a wall (or roofless): flood from an adjacent interior
		// floor tile instead, so a wall torch is tested from inside its room.
		static const int nbrs[8][2] = {
				{ 1,  0},
                {-1,  0},
                { 0,  1},
                { 0, -1},
                { 1,  1},
                { 1, -1},
                {-1,  1},
                {-1, -1}
        };
		for (const auto& d : nbrs) {
			const int nx = base.tx + d[0];
			const int ny = base.ty + d[1];
			if (Light_tile_roofed(gmap, nx, ny) && !Light_tile_wall(gmap, nx, ny)) {
				interior = true;
				return Tile_coord(Light_tile_norm(nx), Light_tile_norm(ny), 0);
			}
		}
		// No roofed interior neighbour: a genuine exterior source.
		interior = own_roofed;
		return base;
	}

	// Bounded flood fill from the Avatar's tile. Returns true if the roofed
	// enclosure the Avatar is standing in has a passable gap in its walls that
	// leads outside -- e.g. a doorway with no door object at all, which has no
	// light_passes_through shape to detect. Light spreads between tiles unless a
	// wall blocks it (tested a couple of tiles above the floor so low furniture is
	// not mistaken for a wall). If the flood reaches a tile with no roof, the
	// enclosure is not light-tight.
	bool Enclosure_open_to_outside(const Tile_coord& start) {
		Game_map* const gmap = Game_window::get_instance()->get_map();
		if (gmap == nullptr) {
			return false;
		}
		constexpr int R       = 12;           // Search radius in tiles.
		constexpr int W       = 2 * R + 1;    // Window width.
		const int     base_tx = start.tx - R;
		const int     base_ty = start.ty - R;
		// Height at which we test for walls: a few tiles above the floor so that
		// low furniture (tables, chairs, ...) is not mistaken for a wall, while
		// full-height building walls still block the flood.
		const int wall_tz   = start.tz + 3;
		auto      normalize = [](int t) {
            return (t % c_num_tiles + c_num_tiles) % c_num_tiles;
		};
		auto is_wall = [&](int tx, int ty) -> bool {
			const int        wtx   = normalize(tx);
			const int        wty   = normalize(ty);
			Map_chunk* const chunk = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (chunk == nullptr) {
				return true;    // Unknown -> treat as blocking.
			}
			return chunk->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, wall_tz);
		};
		auto is_roofless = [&](int tx, int ty) -> bool {
			const int        wtx   = normalize(tx);
			const int        wty   = normalize(ty);
			Map_chunk* const chunk = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (chunk == nullptr) {
				return false;
			}
			return chunk->is_roof(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, start.tz) >= 31;
		};
		std::array<bool, W * W>                visited{};
		std::array<std::pair<int, int>, W * W> queue{};
		int                                    qhead = 0;
		int                                    qtail = 0;
		auto                                   push  = [&](int tx, int ty) {
            const int lx = tx - base_tx;
            const int ly = ty - base_ty;
            if (lx < 0 || lx >= W || ly < 0 || ly >= W) {
                return;
            }
            bool& seen = visited[ly * W + lx];
            if (seen) {
                return;
            }
            seen           = true;
            queue[qtail++] = {tx, ty};
		};
		push(start.tx, start.ty);
		while (qhead < qtail) {
			const auto [tx, ty] = queue[qhead++];
			if (is_roofless(tx, ty)) {
				return true;    // Flood escaped the roof: there is an opening.
			}
			const int nbrs[4][2] = {
					{tx + 1,     ty},
                    {tx - 1,     ty},
                    {    tx, ty + 1},
                    {    tx, ty - 1}
            };
			for (const auto& n : nbrs) {
				if (!is_wall(n[0], n[1])) {
					push(n[0], n[1]);
				}
			}
		}
		return false;    // Fully enclosed within the search radius.
	}

	// Bounded flood fill from `start` through non-wall tiles (tested a couple of
	// tiles above the floor so low furniture is not a wall). Returns true if it
	// reaches `target`. Used to tell whether a light shares the Avatar's interior
	// space even across a chunk boundary: two candles in one room but in different
	// chunks, or a candle in the Avatar's own sealed room across a chunk edge,
	// would otherwise be treated as "outside light" and blocked, so only one lit.
	bool Tiles_in_same_enclosure(const Tile_coord& start, const Tile_coord& target) {
		Game_map* const gmap = Game_window::get_instance()->get_map();
		if (gmap == nullptr) {
			return false;
		}
		constexpr int R         = 12;           // Search radius in tiles.
		constexpr int W         = 2 * R + 1;    // Window width.
		const int     base_tx   = start.tx - R;
		const int     base_ty   = start.ty - R;
		const int     wall_tz   = start.tz + 3;
		auto          normalize = [](int t) {
            return (t % c_num_tiles + c_num_tiles) % c_num_tiles;
		};
		auto is_wall = [&](int tx, int ty) -> bool {
			const int        wtx   = normalize(tx);
			const int        wty   = normalize(ty);
			Map_chunk* const chunk = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (chunk == nullptr) {
				return true;    // Unknown -> treat as blocking.
			}
			return chunk->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, wall_tz);
		};
		const int tgt_tx    = normalize(target.tx);
		const int tgt_ty    = normalize(target.ty);
		auto      is_target = [&](int tx, int ty) -> bool {
            const int ddx = std::abs(Tile_coord::delta(normalize(tx), tgt_tx));
            const int ddy = std::abs(Tile_coord::delta(normalize(ty), tgt_ty));
            return ddx <= 1 && ddy <= 1;
		};
		std::array<bool, W * W>                visited{};
		std::array<std::pair<int, int>, W * W> queue{};
		int                                    qhead = 0;
		int                                    qtail = 0;
		auto                                   push  = [&](int tx, int ty) {
            const int lx = tx - base_tx;
            const int ly = ty - base_ty;
            if (lx < 0 || lx >= W || ly < 0 || ly >= W) {
                return;
            }
            bool& seen = visited[ly * W + lx];
            if (seen) {
                return;
            }
            seen           = true;
            queue[qtail++] = {tx, ty};
		};
		push(start.tx, start.ty);
		while (qhead < qtail) {
			const auto [tx, ty] = queue[qhead++];
			if (is_target(tx, ty)) {
				return true;    // Reached the Avatar: same interior space.
			}
			const int nbrs[4][2] = {
					{tx + 1,     ty},
                    {tx - 1,     ty},
                    {    tx, ty + 1},
                    {    tx, ty - 1}
            };
			for (const auto& n : nbrs) {
				if (!is_wall(n[0], n[1])) {
					push(n[0], n[1]);
				}
			}
		}
		return false;    // Not connected within the search radius.
	}

	// True if an actual roof shape covers the light's tile from above. Scans the
	// light's chunk and its neighbours (a roof spans several tiles / chunks) for
	// is_roof() objects whose footprint contains the light's tile. Unlike
	// Map_chunk::is_roof() -- which counts ANY blocking object above a lift, so a
	// tall lamp post reads as "roofed" over itself -- this looks ONLY at roof
	// shapes and ignores the light object itself. It is a stable, geometric,
	// avatar-independent test, so an interior light keeps its verdict no matter
	// where the Avatar stands (screen-space roof-mask sampling flips as Exult hides
	// roofs above/near the Avatar, which wrongly let interior lights light roofs).
	bool Light_beneath_roof(Game_object* light_obj) {
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			return false;
		}
		const Tile_coord lt  = light_obj->get_tile();
		const int        lcx = lt.tx / c_tiles_per_chunk;
		const int        lcy = lt.ty / c_tiles_per_chunk;
		for (int dcy = -1; dcy <= 1; ++dcy) {
			for (int dcx = -1; dcx <= 1; ++dcx) {
				Map_chunk* const chunk = gmap->get_chunk_safely(lcx + dcx, lcy + dcy);
				if (chunk == nullptr) {
					continue;
				}
				Object_iterator it(chunk->get_objects());
				Game_object*    obj;
				while ((obj = it.get_next()) != nullptr) {
					if (obj == light_obj || !obj->get_info().is_roof()) {
						continue;
					}
					// The roof must be at or above the light (a roof the light sits
					// on top of does not cover it).
					if (obj->get_lift() < lt.tz) {
						continue;
					}
					if (obj->get_footprint().has_world_point(lt.tx, lt.ty)) {
						return true;
					}
				}
			}
		}
		return false;
	}

	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening) {
		LightVisibility vis;
		// Resolve the source to a nearby interior tile (a wall torch sits on the
		// wall itself, whose own tile is not a room floor) and learn whether it is
		// an interior source. The resolved tile is where the enclosure flood
		// begins, so a wall-mounted light is tested from inside its room rather
		// than from the wall (which would let the flood escape straight out
		// through the wall to the exterior).
		bool             source_interior = false;
		const Tile_coord interior_tile   = Resolve_interior_light_tile(light_obj->get_tile(), source_interior);
		vis.interior_source              = source_interior;
		// Can this light source's light escape its OWN enclosure?  An exterior
		// source always can. An interior source can only if its enclosure has a
		// light_passes_through object (window / open door) or a passable physical
		// gap in its walls (a doorway with no door object). The flood-fill is only
		// run when there is no object opening, to bound the cost.
		if (!source_interior) {
			vis.source_can_escape = true;
		} else if (chunk_has_opening && Light_reaches_chunk_opening(olist, interior_tile, light_obj)) {
			// A window / open door in the chunk lets light out, but only if this
			// light can actually reach it. A torch sealed behind an interior wall
			// must not leak through a window in a different part of the same chunk
			// (the chunk-wide test is too coarse).
			vis.source_can_escape = true;
		} else {
			vis.source_can_escape = Enclosure_open_to_outside(interior_tile);
			vis.leaks_through_gap = vis.source_can_escape;
		}
		if (viewer_outside) {
			// Interior light reaches the outside viewer only if it can escape its
			// enclosure. Palette lighting is global, so this only approximates
			// spatial light.
			vis.blocked = !vis.source_can_escape;
			// Interior source seen from outside crosses one boundary (out of its
			// building).
			if (vis.interior_source && !same_chunk) {
				vis.crossings = 1;
			}
		} else {
			// Viewer inside. A light in the Avatar's own chunk always applies; so
			// does one that shares the Avatar's interior space across a chunk
			// boundary (e.g. two candles in one room but in different chunks --
			// otherwise only the candle in the Avatar's chunk would light while the
			// neighbour is blocked as "outside light" the moment the Avatar's room
			// reads as sealed).
			const bool in_avatar_space
					= same_chunk || (vis.interior_source && Tiles_in_same_enclosure(interior_tile, main_actor->get_tile()));
			if (in_avatar_space) {
				vis.blocked = false;
			} else if (avatar_sealed) {
				// The Avatar's enclosure is completely light-tight: no outside
				// light gets in.
				vis.blocked = true;
			} else {
				// The Avatar's room has an opening, so outside light enters -- but a
				// light source sealed inside its OWN building still can't escape to
				// reach the Avatar (fixes a sealed building leaking light into a
				// neighbouring open building).
				vis.blocked = !vis.source_can_escape;
				// Light enters the Avatar's building (one crossing); if the source
				// is itself indoors it also left its own building (a second
				// crossing).
				vis.crossings = 1;
				if (vis.interior_source) {
					vis.crossings += 1;
				}
			}
		}
		return vis;
	}

	int Light_radius(int brightness) {
		return brightness * 3 * c_tilesize;    // ~3 tiles per brightness level.
	}

	int Light_tier(int brightness) {
		return brightness <= 2 ? 0 : (brightness <= 4 ? 1 : 2);
	}

	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift) {
		tx = Light_tile_norm(tx);
		ty = Light_tile_norm(ty);
		// Search upward from just above the light, but from at least the
		// current STOREY's lowest possible ceiling (storey floor + 5) so
		// nothing below it ever reads as the room's roof: a lamp on a desk
		// (tz 3) with a shelf and books occupying lift 4 would otherwise get
		// roof_z 4, turning that shelf into a floor-to-"roof" wall that cuts
		// into the mask -- and a torch carried onto a podest (tz 5) must not
		// read the podest plates at z 6 as a ceiling under a z 11 roof.
		const int floor_z = (lift / 5) * 5;
		const int from    = lift + 1 > floor_z + 5 ? lift + 1 : floor_z + 5;
		// Per-shape scan instead of Chunk_cache::get_lowest_blocked: the
		// blocked bitmask includes ACTORS, so the torch-bearer's own body
		// (occupying the levels just above the light) would read as a "roof"
		// one z above the flame, collapsing roof_z to 6 on a walkway under a
		// z 11 ceiling and turning every floor plate and railing up there
		// into a floor-to-"roof" wall.  Scan the 3x3 neighbouring chunks (a
		// roof or upper floor covering this tile can be anchored in another
		// chunk) for the lowest solid, non-actor, non-carryable shape at or
		// above `from` whose footprint covers the tile.
		if (gmap == nullptr) {
			return floor_z + 5;
		}
		int       roof_z = -1;
		const int lcx    = tx / c_tiles_per_chunk;
		const int lcy    = ty / c_tiles_per_chunk;
		for (int dcy = -1; dcy <= 1; ++dcy) {
			for (int dcx = -1; dcx <= 1; ++dcx) {
				Map_chunk* const chunk = gmap->get_chunk_safely(lcx + dcx, lcy + dcy);
				if (chunk == nullptr) {
					continue;
				}
				Object_iterator it(chunk->get_objects());
				Game_object*    obj;
				while ((obj = it.get_next()) != nullptr) {
					if (obj->as_actor() != nullptr || obj->is_dragable()) {
						continue;    // Bodies and loose items are not ceilings.
					}
					const int ol = obj->get_lift();
					if (ol < from || (roof_z >= 0 && ol >= roof_z)) {
						continue;
					}
					if (!obj->get_info().is_solid()) {
						continue;
					}
					if (obj->get_footprint().has_world_point(tx, ty)) {
						roof_z = ol;
					}
				}
			}
		}
		if (roof_z >= 0 && roof_z < 31) {
			return roof_z;
		}
		// No roof overhead: the storey's classic wall-top threshold.
		return floor_z + 5;
	}

	// Shared room flood for the light masks: fill from `start` across passable
	// (non tall-wall) floor, bounded by the (2*rt+1) grid.  Walls reaching the
	// room's roof level `roof_z` stop the fill and are lit as a one-tile ring,
	// so the enclosing walls are illuminated but light does not leak past them.
	// When `spills` is given, a wall tile covered by a light_passes_through
	// shape (window, grate) reports the tile just BEYOND it -- where the
	// escaping light lands on the far side -- unless that tile is a wall too
	// (a window into another wall spills nowhere).  Likewise, when the fill
	// itself flows out from UNDER the roof into the open (an open doorway, a
	// wall gap, past a porch roof's edge), the first unroofed tile is a spill
	// point too: the ground there is already lit by the source's own gated
	// splat, but only a SPILL light (exterior semantics) can pick up marked
	// tall shapes -- trees, lampposts -- standing in that escaped light.  The
	// reached set depends only on the room's shape, not on where in it the
	// start tile lies, so a carried torch keeps a stable mask.
	static void Flood_room_grid(
			Game_map* gmap, const Tile_coord& lt, int rt, int roof_z, std::vector<unsigned char>& lit,
			std::vector<Light_spill>* spills) {
		const int                  side = 2 * rt + 1;
		std::vector<unsigned char> visited(static_cast<size_t>(side) * side, 0);
		// Breadth-first queue: the fill records each tile's PATH distance from
		// the start (in tiles, stored as distance+1 in `lit`; 0 = unreached),
		// so the splat can fade the light by the distance it actually
		// TRAVELS around walls, not just the straight-line distance.
		std::vector<std::pair<int, int>> queue;
		size_t                           qhead = 0;
		// Outside-tile grid coords + the opening's transmission percent.
		std::vector<std::tuple<int, int, int>> spill_cand;
		std::vector<std::pair<int, int>>       door_cand;    // Roofed->open exit tiles.
		// The room's floor for the wall test is the STOREY floor (storeys are 5 z
		// apart), not the light's own z: a lamp standing on a shelf at tz 4 is
		// still in a ground-floor room whose walls rise from z 0, and the books
		// beside it at lift 4 must not read as rising "from the floor".
		const int floor_z = (lt.tz / 5) * 5;
		// Memorize the per-shape wall test: each grid tile is asked several
		// times (ring pass, spill far-side check, diagonal corner pass).
		std::vector<unsigned char> tallmemo(static_cast<size_t>(side) * side, 0);    // 0 unknown, 1 wall, 2 open.
		auto                       tall = [&](int gx, int gy) {
            unsigned char& m = tallmemo[static_cast<size_t>(gy) * side + gx];
            if (m == 0) {
                m = Light_tile_tall_blocker(gmap, lt.tx + gx - rt, lt.ty + gy - rt, roof_z, floor_z) ? 1 : 2;
            }
            return m == 1;
		};
		auto opening = [&](int gx, int gy) {
			return Light_tile_pass_opening(gmap, lt.tx + gx - rt, lt.ty + gy - rt);
		};
		// Memoized per-tile roof test for the doorway-spill detection: a spill
		// is where the fill crosses from a roofed tile to an unroofed one.
		// Only an under-roof source spills through doors; an outdoor lamp's
		// fill may pass beneath a porch roof and out again, but its light is
		// exterior already and needs no continuation bubble.
		std::vector<unsigned char> roofmemo(static_cast<size_t>(side) * side, 0);    // 0 unknown, 1 roofed, 2 open.
		auto                       roofed = [&](int gx, int gy) {
            unsigned char& m = roofmemo[static_cast<size_t>(gy) * side + gx];
            if (m == 0) {
                m = Light_tile_roofed(gmap, lt.tx + gx - rt, lt.ty + gy - rt) ? 1 : 2;
            }
            return m == 1;
		};
		const bool start_roofed = spills != nullptr && roofed(rt, rt);
		queue.emplace_back(rt, rt);    // Start at the grid centre.
		visited[static_cast<size_t>(rt) * side + rt] = 1;
		lit[static_cast<size_t>(rt) * side + rt]     = 1;    // Distance 0.
		// Record a tile's path distance (+1), keeping the smallest when a wall
		// face is reached again from another direction.
		auto set_dist = [&](size_t idx, int dist) {
			const int v = (dist < 254 ? dist : 254) + 1;
			if (lit[idx] == 0 || lit[idx] > v) {
				lit[idx] = static_cast<unsigned char>(v);
			}
		};
		static const int step[4][2] = {
				{ 1,  0},
                {-1,  0},
                { 0,  1},
                { 0, -1}
        };
		static const int diag[4][2] = {
				{ 1,  1},
                { 1, -1},
                {-1,  1},
                {-1, -1}
        };
		while (qhead < queue.size()) {
			const int gx = queue[qhead].first;
			const int gy = queue[qhead].second;
			++qhead;
			const int gdist = lit[static_cast<size_t>(gy) * side + gx] - 1;
			for (const auto& d : step) {
				const int nx = gx + d[0];
				const int ny = gy + d[1];
				if (nx < 0 || ny < 0 || nx >= side || ny >= side) {
					continue;
				}
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				if (tall(nx, ny)) {
					// Wall tiles never enter the stack, so handle them on
					// EVERY approach instead of only the first: once the fill
					// escapes through one opening and wraps around the
					// building, it can reach a window's wall tile from its
					// OUTSIDE face first -- that approach's far side is the
					// interior (recording nothing), and gating on `visited`
					// would then silently drop the window's real outward
					// spill when the inside face is reached later.  Repeat
					// work is bounded (once per adjacent floor tile) and the
					// wall test is memoized.
					// Light the wall face, but do not flood past it.
					set_dist(nidx, gdist + 1);
					if (spills != nullptr) {
						const int pct = opening(nx, ny);
						if (pct > 0) {
							// Window/grate: the light escapes to the tile on the
							// FAR side of the opening, continuing in the direction
							// the fill approached from.  Record it as a candidate;
							// whether it really points OUTWARD is only known once
							// the fill is complete (see below).
							const int ox = nx + d[0];
							const int oy = ny + d[1];
							if (ox >= 0 && oy >= 0 && ox < side && oy < side && !tall(ox, oy)) {
								bool dup = false;
								for (const auto& [px, py, ppct] : spill_cand) {
									if (px == ox && py == oy) {
										dup = true;
										break;
									}
								}
								if (!dup) {
									spill_cand.emplace_back(ox, oy, pct);
								}
							}
						}
					}
					continue;
				}
				if (start_roofed && roofed(gx, gy) && !roofed(nx, ny)) {
					// The fill steps out from under the roof into the open:
					// a doorway, a wall gap, the edge of a porch roof.  That
					// first open-sky tile carries the bubble's continuation.
					// Checked BEFORE the visited gate for the same reason as
					// walls above: the wrap-around fill can visit the exit
					// tile from the open side first (no transition there),
					// which must not swallow the real roofed->open crossing.
					// Duplicates are thinned at emission.
					door_cand.emplace_back(nx, ny);
				}
				if (visited[nidx]) {
					continue;
				}
				visited[nidx] = 1;
				set_dist(nidx, gdist + 1);
				queue.emplace_back(nx, ny);
			}
			// Diagonal floor steps keep the path metric Chebyshev-like: without
			// them a diagonal walk costs its Manhattan length and the dome would
			// darken into a diamond even in an open room.  Never cut a corner:
			// a diagonal step is only allowed when BOTH orthogonal in-between
			// tiles are open, so two walls meeting corner-to-corner still seal
			// the room (the reached set stays exactly the orthogonal one).
			// Diagonal WALL neighbours are lit too (corner posts touch the room
			// only diagonally; without this the mask gets a notch at every
			// corner) but never flooded past, and spill/door detection stays
			// orthogonal-only.
			for (const auto& d : diag) {
				const int nx = gx + d[0];
				const int ny = gy + d[1];
				if (nx < 0 || ny < 0 || nx >= side || ny >= side) {
					continue;
				}
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				if (tall(nx, ny)) {
					set_dist(nidx, gdist + 1);
					continue;
				}
				if (visited[nidx] || tall(gx + d[0], gy) || tall(gx, gy + d[1])) {
					continue;
				}
				visited[nidx] = 1;
				set_dist(nidx, gdist + 1);
				queue.emplace_back(nx, ny);
			}
		}
		// Emit only the candidates whose outside tile the fill itself never
		// reached.  The fill is not strictly interior: through an open doorway
		// or wall gap it escapes and can wrap around the building, touching a
		// window from its OUTSIDE face -- the "far side" of that approach is
		// the room interior, and emitting it would spill the window's glow
		// back INSIDE.  A tile the fill already lit needs no spill glow anyway.
		if (spills != nullptr) {
			for (const auto& [ox, oy, pct] : spill_cand) {
				// Emit only if the far-side tile is under OPEN SKY.  A window
				// the fill touched from its OUTSIDE face (after escaping through
				// a door and wrapping around the building) has the room interior
				// as its far side -- roofed -- and emitting it would spill the
				// glow back INSIDE.  (Testing `lit` instead is wrong: the fill
				// wrapping around outside also reaches a window's legitimate
				// outside tile, which must NOT cancel that window's spill.)
				if (Light_tile_roofed(gmap, lt.tx + ox - rt, lt.ty + oy - rt)) {
					continue;    // Far side is interior (or under another roof).
				}
				spills->push_back({Tile_coord(Light_tile_norm(lt.tx + ox - rt), Light_tile_norm(lt.ty + oy - rt), 0), pct});
			}
			// Doorway/roof-edge spills.  Neighbouring exit tiles along a wide
			// opening would each spawn a near-identical continuation bubble
			// (same centre give or take a tile, same continued falloff), so
			// thin them out: skip a candidate within two tiles of one already
			// emitted.  Overlapping bubbles combine by max coverage, so the
			// survivors still reproduce the source's dome seamlessly.
			std::vector<std::pair<int, int>> emitted;
			for (const auto& [ox, oy] : door_cand) {
				bool near_prev = false;
				for (const auto& [ex, ey] : emitted) {
					if (std::abs(ox - ex) <= 2 && std::abs(oy - ey) <= 2) {
						near_prev = true;
						break;
					}
				}
				if (near_prev) {
					continue;
				}
				emitted.emplace_back(ox, oy);
				// A doorway / roof-edge exit is open air: full transmission.
				spills->push_back({Tile_coord(Light_tile_norm(lt.tx + ox - rt), Light_tile_norm(lt.ty + oy - rt), 0), 100});
			}
		}
	}

	void Build_light_shadow_grid(
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills) {
		const int side = 2 * rt + 1;
		lit.assign(static_cast<size_t>(side) * side, 0);    // Default: unlit; the room fills in.
		spills.clear();
		if (rt <= 0 || light_obj == nullptr) {
			lit.clear();    // No grid -> caller treats as fully lit.
			return;
		}
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			lit.clear();
			return;
		}
		const Tile_coord lt     = light_obj->get_tile();
		const int        roof_z = Light_room_roof_z(gmap, lt.tx, lt.ty, lt.tz);
		if (std::getenv("EXULT_DEBUG_LIGHT_MASK") != nullptr) {
			static Tile_coord last(-1, -1, -1);
			if (lt.tx != last.tx || lt.ty != last.ty || lt.tz != last.tz) {
				last = lt;
				std::cerr << "[light-mask] === light shape=" << light_obj->get_shapenum() << " tile=(" << lt.tx << ',' << lt.ty
						  << ',' << lt.tz << ") roof_z=" << roof_z << " floor_z=" << (lt.tz / 5) * 5 << " rt=" << rt << std::endl;
			}
		}
		Flood_room_grid(gmap, lt, rt, roof_z, lit, &spills);
	}

	void Build_spill_shadow_grid(const Tile_coord& start, int rt, std::vector<unsigned char>& lit) {
		const int side = 2 * rt + 1;
		lit.assign(static_cast<size_t>(side) * side, 0);
		if (rt <= 0) {
			lit.clear();    // No grid -> caller treats as fully lit.
			return;
		}
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			lit.clear();
			return;
		}
		// Flood from the tile just outside the opening: the glow spreads over
		// whatever the window looks out on and is stopped by the building's
		// own wall (lit as ring, so the window face glows), so the spilled
		// light can never come back INSIDE the room it escaped from.  No
		// spills are collected here: a spill does not spawn further spills.
		const int roof_z = Light_room_roof_z(gmap, start.tx, start.ty, start.tz);
		Flood_room_grid(gmap, start, rt, roof_z, lit, nullptr);
	}

	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy) {
		if (radius <= 0 || intensity_pct <= 0) {
			return;
		}
		// Transmission of the opening this light came through (spill glows):
		// scales the whole dome's brightness.  Real sources pass 100.
		const float inten = intensity_pct >= 100 ? 1.0f : static_cast<float>(intensity_pct) / 100.0f;
		const float rf    = static_cast<float>(radius);
		// Model the source as a point `elevation` px above the ground plane that
		// passes through the splat centre.  A ground pixel at 2D screen distance
		// d from the centre is at 3D distance sqrt(d^2 + e^2) from the emitter;
		// normalising by the 3D reach sqrt(r^2 + e^2) keeps the pool's ground
		// radius at r while lowering and rounding the peak -- a dome rather than
		// a flat-topped cylinder.  With elevation 0 this is exactly the old
		// quadratic 1 - (d/r)^2 falloff.
		// `dist_bias` continues another bubble's falloff: a pixel at distance d
		// from the splat centre is treated as at d + bias from the ORIGINAL
		// source, and the reach is bias + radius -- so a spill glow starts at
		// exactly the brightness the source's dome had at the opening and keeps
		// fading on the same curve, instead of peaking like a second lamp.
		const float e    = static_cast<float>(elevation > 0 ? elevation : 0);
		const float e2   = e * e;
		const float bias = static_cast<float>(dist_bias > 0 ? dist_bias : 0);
		const float full = rf + bias;           // Ground reach of the original bubble.
		const float rf2  = full * full + e2;    // Square of its 3D reach (never 0).
		// PROPAGATED LIGHT FIELD (no occlusion mask): when the room-fill grid
		// is given, the light is rendered directly from it.  Each reached tile
		// gets the dome brightness at its travelled distance -- the LONGER of
		// the straight line and the flood path, so the field agrees with the
		// free dome in the open (the path metric is Chebyshev, always <= the
		// straight line there) but fades out along the path where the light
		// had to go around walls, instead of shining through them.  Unreached
		// tiles are simply dark: containment is not a gate applied to a dome,
		// it is the absence of propagated light.
		//
		// Tiles at one z-level form a uniform c_tilesize lattice on screen,
		// pinned by ONE reference: the foot position of the light's own tile
		// (grid centre) at the wall-top anchor level.  That reference is
		// quantized to the light's TILE, so the REACHED SET -- the containment
		// boundary along the walls -- stays fixed as the source moves within
		// its tile.  A tile covers c_tilesize pixels up-and-left of its foot;
		// its centre is half a tile up-left of it.
		const int          side   = grid != nullptr ? 2 * grid_rt + 1 : 0;
		const float        cell   = static_cast<float>(c_tilesize);
		const float        half   = 0.5f * (cell - 1.0f);
		const float        grid_u = static_cast<float>(grid_fx) - half - static_cast<float>(grid_rt) * cell;
		const float        grid_v = static_cast<float>(grid_fy) - half - static_cast<float>(grid_rt) * cell;
		std::vector<float> field;
		if (grid != nullptr) {
			// The wall-top anchor pins the lattice up-and-left of the tiles'
			// floor positions (4px per z of wall height), so the grid CENTRE
			// renders up-left of the light itself.  The brightness peak,
			// however, belongs at the light's true screen position: measure
			// the per-tile straight-line distance from the SPLAT CENTRE
			// expressed in lattice coordinates, not from the grid centre --
			// otherwise the whole pool sits diagonally up-left of the source
			// by the anchor shift (more for taller walls, none for ungated
			// lights: the old "light centre shifted up-left" bug).  The peak
			// thus follows the light continuously (as the free dome always
			// did) while the reached set stays tile-quantized.
			const float uc = (static_cast<float>(sx) - grid_u) / cell;
			const float vc = (static_cast<float>(sy) - grid_v) / cell;
			// The flood path is 0 at the light's TILE (the grid centre), which
			// again renders up-left of the true centre: the lattice cell under
			// the splat centre already carries a path of a tile or two, and
			// max(euclid, path) would dim the pool right at the source.
			// Rebase the path so it is zero at that cell.
			int cgx                     = static_cast<int>(std::lround(uc));
			int cgy                     = static_cast<int>(std::lround(vc));
			cgx                         = std::min(std::max(cgx, 0), side - 1);
			cgy                         = std::min(std::max(cgy, 0), side - 1);
			const unsigned char mbase   = grid[static_cast<size_t>(cgy) * side + cgx];
			const float         base_px = mbase > 0 ? static_cast<float>((mbase - 1) * c_tilesize) : 0.0f;
			field.assign(static_cast<size_t>(side) * side, 0.0f);
			for (int gy = 0; gy < side; ++gy) {
				for (int gx = 0; gx < side; ++gx) {
					const unsigned char m = grid[static_cast<size_t>(gy) * side + gx];
					if (!m) {
						continue;    // Light never reaches this tile.
					}
					float path_px = static_cast<float>((m - 1) * c_tilesize) - base_px;
					if (path_px < 0.0f) {
						path_px = 0.0f;
					}
					const float ddx    = (static_cast<float>(gx) - uc) * cell;
					const float ddy    = (static_cast<float>(gy) - vc) * cell;
					float       travel = std::sqrt(ddx * ddx + ddy * ddy);
					if (path_px > travel) {
						travel = path_px;
					}
					const float tot  = travel + bias;
					const float dome = 1.0f - (tot * tot + e2) / rf2;
					if (dome > 0.0f) {
						field[static_cast<size_t>(gy) * side + gx] = 255.0f * dome * inten;
					}
				}
			}
		}
		// The free dome is bounded by `radius` around the splat centre; the
		// propagated field by `radius` around the GRID centre (cells beyond it
		// go dark in the per-tile dome) plus a tile of bilinear support.  The
		// two centres differ (elevation shift vs. anchor level), so the bbox
		// must cover both or the field gets cut off on one side.
		int x0 = sx - radius;
		int x1 = sx + radius;
		int y0 = sy - radius;
		int y1 = sy + radius;
		if (grid != nullptr) {
			x0 = std::min(x0, grid_fx - radius - c_tilesize);
			x1 = std::max(x1, grid_fx + radius + c_tilesize);
			y0 = std::min(y0, grid_fy - radius - c_tilesize);
			y1 = std::max(y1, grid_fy + radius + c_tilesize);
		}
		if (x0 < 0) {
			x0 = 0;
		}
		if (y0 < 0) {
			y0 = 0;
		}
		if (x1 >= W) {
			x1 = W - 1;
		}
		if (y1 >= H) {
			y1 = H - 1;
		}
		for (int y = y0; y <= y1; ++y) {
			const int            dy      = y - sy;
			const float          dy2     = static_cast<float>(dy) * static_cast<float>(dy);
			const unsigned char* roofrow = roofpix ? roofpix + y * roof_lw : nullptr;
			for (int x = x0; x <= x1; ++x) {
				// Marked pixel: 255 is a real roof (or an object standing on
				// one), 128 a tall EXTERIOR shape (tree canopy, lamppost).  An
				// under-roof light keeps both dark.  A SPILL glow -- the bubble
				// already outside the opening -- lights tall shapes as whole
				// units (bypassing the propagated field below: the sprite's
				// elevated pixels span far more screen than its ground tiles, so
				// sampling the field there would cut the shape into lit and dark
				// patches) but must never light a real roof.  A true EXTERIOR
				// light (street lamp, torch in the open) lights both.
				bool bypass_field = false;
				if (roofrow && roofrow[x]) {
					if (veto_roof) {
						continue;    // Keep dark under every interior light.
					}
					if (is_spill && roofrow[x] == 255) {
						continue;    // A spill never lights a real roof.
					}
					bypass_field = true;
				}
				int a;
				if (grid != nullptr && !bypass_field) {
					// The field bounds itself (cells beyond the pool are dark),
					// so no screen-circle clip here: the circle is centred on
					// the elevation-shifted splat centre while the field sits on
					// the anchor lattice, and clipping the field by it would cut
					// a sharp edge into the pool's up-left arc.
					// Sample the propagated field, interpolating between the
					// four surrounding tile centres so the brightness stays a
					// smooth gradient instead of 8px steps.
					float u = (static_cast<float>(x) - grid_u) / cell;
					float v = (static_cast<float>(y) - grid_v) / cell;
					if (u < 0.0f) {
						u = 0.0f;
					} else if (u > static_cast<float>(side - 1)) {
						u = static_cast<float>(side - 1);
					}
					if (v < 0.0f) {
						v = 0.0f;
					} else if (v > static_cast<float>(side - 1)) {
						v = static_cast<float>(side - 1);
					}
					int g0 = static_cast<int>(u);
					int h0 = static_cast<int>(v);
					if (g0 > side - 2) {
						g0 = side - 2;
					}
					if (h0 > side - 2) {
						h0 = side - 2;
					}
					const float  tu  = u - static_cast<float>(g0);
					const float  tv  = v - static_cast<float>(h0);
					const float* row = field.data() + static_cast<size_t>(h0) * side + g0;
					const float  top = row[0] + (row[1] - row[0]) * tu;
					const float  bot = row[side] + (row[side + 1] - row[side]) * tu;
					a                = static_cast<int>(top + (bot - top) * tv + 0.5f);
				} else {
					// Free dome (exterior lights, and marked tall shapes /
					// roofs lit as whole units): pure radial falloff, with
					// the travelled distance continued past `bias` for spills.
					const int   dx    = x - sx;
					const float dist2 = static_cast<float>(dx) * static_cast<float>(dx) + dy2;
					if (dist2 > rf * rf) {
						continue;    // Outside the pool's ground radius.
					}
					float total2 = dist2;
					if (bias > 0.0f) {
						const float tot = std::sqrt(dist2) + bias;
						total2          = tot * tot;
					}
					const float dome = 1.0f - (total2 + e2) / rf2;
					a                = static_cast<int>(255.0f * dome * inten + 0.5f);
				}
				if (a <= 0) {
					continue;
				}
				const size_t idx = static_cast<size_t>(y) * W + x;
				if (a > cov[idx]) {
					cov[idx]               = static_cast<unsigned char>(a);
					dstpix[y * dst_lw + x] = srcpix[y * src_lw + x];
				}
			}
		}
	}

}    // namespace NaturalLight
