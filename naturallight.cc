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

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace {

	// Does the given shape/frame let light pass through (window, open door,
	// grate, ...)?  Explicit per-frame entries take precedence over a wildcard.
	bool Shape_light_passes_through_strict(
			const Shape_info& info, int frame, int& match_frame, bool& has_explicit, bool& has_wildcard) {
		const int want_frame = frame & 31;
		has_explicit         = false;
		has_wildcard         = false;
		bool explicit_hit    = false;

		for (const auto& ent : info.get_light_passes_info()) {
			if (ent.is_invalid()) {
				continue;
			}
			const int ent_frame = ent.get_frame();
			if (ent_frame >= 0) {
				has_explicit = true;
				if (ent_frame == want_frame) {
					explicit_hit = true;
				}
			} else if (ent_frame == -1) {
				has_wildcard = true;
			}
		}

		const bool hit = has_explicit ? explicit_hit : has_wildcard;
		match_frame    = hit ? (has_explicit ? want_frame : -1) : -2;
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
			if (Shape_light_passes_through_strict(obj->get_info(), obj->get_framenum(), match_frame, has_explicit, has_wildcard)) {
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
			if (!Shape_light_passes_through_strict(obj->get_info(), obj->get_framenum(), match_frame, has_explicit, has_wildcard)) {
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

	int Light_radius(int brightness) {
		return brightness * 3 * c_tilesize;    // ~3 tiles per brightness level.
	}

	int Light_tier(int brightness) {
		return brightness <= 2 ? 0 : (brightness <= 4 ? 1 : 2);
	}

	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, const unsigned char* roofpix, int roof_lw) {
		if (radius <= 0) {
			return;
		}
		const float rf = static_cast<float>(radius);
		int         x0 = sx - radius;
		int         x1 = sx + radius;
		int         y0 = sy - radius;
		int         y1 = sy + radius;
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
				if (roofrow && roofrow[x]) {
					continue;    // Roof pixel: keep dark under every light.
				}
				const int   dx   = x - sx;
				const float dist = std::sqrt(static_cast<float>(dx) * static_cast<float>(dx) + dy2);
				if (dist > rf) {
					continue;
				}
				// Soft (quadratic) falloff: stays bright well into the radius,
				// then fades gently to the edge, for a wider glow.
				const float tnorm = dist / rf;
				const int   a     = static_cast<int>(255.0f * (1.0f - tnorm * tnorm) + 0.5f);
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
