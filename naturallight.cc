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
#include <cstdio>
#include <cstdlib>
#include <map>
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

	// When the given absolute tile's only cover is a walkable floor-roof deck
	// -- a storey slab whose top is at or below `max_z`, with open sky above
	// it -- return the deck's top z-level.  Returns -1 when the tile is open
	// ground or genuinely roofed (covered again higher up).  Used by the spill
	// emission: a window high in a tall wall looks out OVER the neighbouring
	// one-storey room's floor-roof, and its glow lands ON that deck; it must
	// not be dropped as "interior" just because the deck registers as a roof
	// from the ground.
	int Light_tile_overlooked_deck(Game_map* gmap, int tx, int ty, int max_z) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return -1;
		}
		// The deck is a FLOOR shape (tf_floor_flag), and those are height-0
		// flats: they never set the chunk's blocked flags, so the blocked-column
		// probes cannot see them (the only blocked z over a deck tile is the
		// tall room's own eave at roof level).  Scan for a floor object
		// covering this tile instead.  An object lives in the chunk of its
		// ANCHOR (bottom-right corner) while its footprint extends up-left,
		// so a slab covering this tile can be anchored up to 7 tiles east or
		// south -- in the east/south/southeast neighbour chunk; scan the 2x2
		// chunk block.  The landing surface lies STRICTLY below the opening's
		// top (light exits below the window's top edge and passes under the
		// eave); take the highest such floor so multi-storey decks resolve to
		// the one the window overlooks.
		int       deck_top = -1;
		const int cx       = tx / c_tiles_per_chunk;
		const int cy       = ty / c_tiles_per_chunk;
		for (int dcy = 0; dcy < 2; ++dcy) {
			for (int dcx = 0; dcx < 2; ++dcx) {
				Map_chunk* const ch = gmap->get_chunk_safely((cx + dcx) % c_num_chunks, (cy + dcy) % c_num_chunks);
				if (ch == nullptr) {
					continue;
				}
				Object_iterator it(ch->get_objects());
				Game_object*    obj;
				while ((obj = it.get_next()) != nullptr) {
					const Shape_info& info = obj->get_info();
					if (!info.is_floor()) {
						continue;
					}
					if (!obj->get_footprint().has_world_point(tx, ty)) {
						continue;
					}
					const int top = obj->get_lift() + info.get_3d_height();
					if (top < max_z && top > deck_top) {
						deck_top = top;
					}
				}
			}
		}
		const int above
				= deck_top >= 0 ? chunk->get_lowest_blocked(deck_top + 1, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk) : -1;
		if (deck_top < 5) {
			return -1;    // No storey-level floor: ground pavement is not a deck.
		}
		if (above >= 0 && above < max_z) {
			// Solidly covered again BELOW the window top: another storey -- the
			// far side is interior.  Cover at or above the window top is only
			// the tall room's eave sticking out over the deck: the light passes
			// under it, and the eave is painted as a real roof (mask 255) that
			// clips the glow's overlapping pixels.
			return -1;
		}
		return deck_top;
	}

	// Is the given absolute tile a full-height wall on the storey with floor
	// `floor_z`?  Tested a few tiles above that floor so low furniture (tables,
	// chairs, ...) is not mistaken for a wall.
	bool Light_tile_wall(Game_map* gmap, int tx, int ty, int floor_z = 0) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return false;
		}
		return chunk->is_tile_occupied(tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, floor_z + 3);
	}

	// Does a light_passes_through shape cover the given absolute tile, making
	// it a SPILL opening (window, grate, glass wall)?  Such a tile stops the
	// room-fill like the wall it sits in, but the escaping light is rendered
	// as a small glow there.  Only an opening overlapping the light's own room
	// band [floor_z, roof_z) counts: the fill is a 2D slice of THAT storey, and
	// without the band test a ground-floor lamp would "escape" through the
	// second storey's window sharing the same wall tile -- a phantom beam
	// upstairs.  Doors are explicitly NOT spill openings even
	// though their open frames are in the light_passes_through list: an open
	// door leaf is a solid shape beside a doorway the flood already passes
	// through, so a spill glow on it reads as a phantom second light source
	// (and a frame mix-up makes some closed doors glow instead).  Returns the
	// opening's transmission percent (1..100), or 0 when no pass-through shape
	// covers the tile.  `top_z` (when found) receives the opening's TOP
	// z-level (lift + 3d height): a window mounted high in a wall pokes above
	// an adjoining floor-roof deck, and its spill glow must then be drawn over
	// the deck's surface instead of being storey-gated off it.
	int Light_tile_has_pass_through(Map_chunk* chunk, int tx, int ty, int floor_z, int roof_z, int* top_z = nullptr) {
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			const Shape_info& info = obj->get_info();
			if (info.is_door()) {
				continue;
			}
			const int lift = obj->get_lift();
			if (lift >= roof_z || lift + info.get_3d_height() <= floor_z) {
				continue;    // Another storey's opening, not this room's.
			}
			int  match_frame  = -2;
			bool has_explicit = false;
			bool has_wildcard = false;
			int  percent      = 100;
			if (!Shape_light_passes_through_strict(info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
				continue;
			}
			if (obj->get_footprint().has_world_point(tx, ty)) {
				if (top_z != nullptr) {
					*top_z = obj->get_lift() + info.get_3d_height();
				}
				return percent;
			}
		}
		return 0;
	}

	// Which tiles are light-blocking walls for the spatial lights?  A wall is a
	// blocking (solid) shape covering the tile that rises from the floor
	// (`floor_z`, the storey floor of the light's room) and reaches up to the
	// room's roof level `roof_z`.  Buildings have walls taller than the
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
	//
	// Stamps the verdicts for a WHOLE flood grid in one pass: each covered
	// chunk's object list is classified once and the qualifying walls'
	// footprints marked, instead of re-walking the list for every grid tile
	// (up to ~2600 tiles x dozens of objects per flood dominated the flood
	// cost, ~0.8 ms each).  Matching the per-tile scan it replaces, an object
	// only stamps tiles inside its own (anchor) chunk: other chunks' object
	// lists never contained it, so its overhang was invisible there.
	// (tx0, ty0) is the normalized world tile of grid cell (0, 0); `wall`
	// receives 1 where a tile is a wall, 0 where it is open.
	void Light_flood_stamp_walls(
			Game_map* gmap, int tx0, int ty0, int side, int roof_z, int floor_z, std::vector<unsigned char>& wall) {
		wall.assign(static_cast<size_t>(side) * side, 0);
		const int ncx = (tx0 % c_tiles_per_chunk + side - 1) / c_tiles_per_chunk;
		const int ncy = (ty0 % c_tiles_per_chunk + side - 1) / c_tiles_per_chunk;
		for (int icy = 0; icy <= ncy; ++icy) {
			for (int icx = 0; icx <= ncx; ++icx) {
				const int        ccx = (tx0 / c_tiles_per_chunk + icx) % c_num_chunks;
				const int        ccy = (ty0 / c_tiles_per_chunk + icy) % c_num_chunks;
				Map_chunk* const ch  = gmap->get_chunk_safely(ccx, ccy);
				if (ch == nullptr) {
					continue;
				}
				Object_iterator it(ch->get_objects());
				Game_object*    obj;
				while ((obj = it.get_next()) != nullptr) {
					if (obj->as_actor() != nullptr) {
						continue;    // NPCs move around; never count them as walls.
					}
					const Shape_info& info = obj->get_info();
					if (obj->is_dragable()) {
						// A loose, carryable item (a book on top of a full-height
						// shelf) is never a wall, even where the pile beneath it is
						// solid up to the roof.  Real wall segments -- including
						// shutters sitting on half walls -- are immovable (weight 0
						// or static map fixtures), so they are not skipped here.
						continue;
					}
					{
						// A light_passes_through shape/frame (iron-bar prison door,
						// grate, open door leaf, window glass) is never a wall by
						// itself: light passes through it even though it is solid to
						// movement.  A window tile often still reads as a wall via the
						// wall pieces it is embedded in, and Light_tile_pass_opening
						// then turns it into a spill opening; but where the window
						// itself is the wall's top section (castle halls with stained
						// glass high in the wall) the fill flows THROUGH the glass out
						// onto whatever lies beyond -- e.g. an adjoining floor-roof
						// deck, which the light genuinely reaches from above.  The
						// storey-gated roof mask keeps under-deck lights dark there, so
						// the through-flow lights only what it should.  (A 0% entry
						// blocks light entirely: the strict test returns false for it,
						// so the shape falls through to the normal wall tests below.)
						int  match_frame  = -2;
						bool has_explicit = false;
						bool has_wildcard = false;
						int  percent      = 100;
						if (Shape_light_passes_through_strict(
									info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
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
						continue;
					}
					if (!info.is_solid() || info.is_roof()) {
						// Only blocking shapes seal the room, and the roof (or its
						// low-hanging eaves) is not a wall.
						continue;
					}
					const int lift = obj->get_lift();
					if (lift >= roof_z) {
						continue;    // At/above the roof: not part of the room's walls.
					}
					if (lift + info.get_3d_height() < roof_z) {
						continue;    // Top below the roof: light passes over it.
					}
					const TileRect fp = obj->get_footprint();
					for (int fty = fp.y; fty < fp.y + fp.h; ++fty) {
						const int dgy = Light_tile_norm(fty - ty0);
						if (dgy >= side) {
							continue;
						}
						const int wty = Light_tile_norm(fty);
						if (wty / c_tiles_per_chunk != ccy) {
							continue;
						}
						for (int ftx = fp.x; ftx < fp.x + fp.w; ++ftx) {
							const int dgx = Light_tile_norm(ftx - tx0);
							if (dgx >= side) {
								continue;
							}
							const int wtx = Light_tile_norm(ftx);
							if (wtx / c_tiles_per_chunk != ccx) {
								continue;
							}
							if (lift > floor_z) {
								// Starts above the floor.  A hung object (shield,
								// tapestry, sign) has open space beneath it and light
								// passes under; but a shutter sitting on a half-height
								// wall piece is a real wall segment even though it
								// starts at lift 2.  Shape class cannot tell them
								// apart, so test the geometry directly: it only blocks
								// light where the space below it at THIS tile is
								// solidly filled all the way down to the floor -- no
								// gap for light to squeeze under.
								bool gap = false;
								for (int z = floor_z; z < lift; ++z) {
									if (!ch->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z)) {
										gap = true;
										break;
									}
								}
								if (gap) {
									continue;
								}
							}
							wall[static_cast<size_t>(dgy) * side + dgx] = 1;
						}
					}
				}
			}
		}
	}

	// Is the given absolute tile covered by a light_passes_through shape
	// (window, open door, grate) within the room band [floor_z, roof_z)?  Such
	// a tile is where light escapes a room even though the wall stack occupies
	// it to full height.  Returns the opening's transmission percent (1..100),
	// or 0 when there is no opening.  `top_z` (when found) receives the
	// opening's top z-level.
	int Light_tile_pass_opening(Game_map* gmap, int tx, int ty, int floor_z, int roof_z, int* top_z = nullptr) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return 0;
		}
		return Light_tile_has_pass_through(chunk, tx, ty, floor_z, roof_z, top_z);
	}

}    // namespace

namespace NaturalLight {

	// ------------------------------------------------------------------
	// Cross-frame verdict caches.  The per-light geometric verdicts (roof
	// overhead, room roof height, viewer visibility) and the propagated
	// light field rescan chunk object lists or recompute per-tile domes
	// every frame although the underlying geometry rarely changes.  Cache
	// them for a short time-to-live -- the same pattern (and TTL) as the
	// room-fill flood cache below -- so a door opening or a light moving
	// still takes effect within a fraction of a second.  All of these are
	// only exercised on the natural-light path.
	// ------------------------------------------------------------------
	namespace {
		constexpr Uint64 light_cache_ttl = 250;    // ms.

		template <typename Key, typename Value>
		struct Verdict_cache {
			// A content-keyed cache's entries never go stale (any input change
			// changes the key), so hits refresh the stamp and the ttl is only a
			// keep-time evicting entries no longer being used.
			Uint64 ttl           = light_cache_ttl;
			bool   content_keyed = false;

			struct Entry {
				Value  value{};
				Uint64 stamp = 0;
			};

			std::map<Key, Entry> entries;

			// Fresh entry for `key`, or nullptr.  Sweeps expired entries when
			// the cache grows (carried lights change tile every step).
			Value* find(const Key& key, Uint64 now) {
				auto it = entries.find(key);
				if (it != entries.end()) {
					if (content_keyed) {
						it->second.stamp = now;
						return &it->second.value;
					}
					if (now - it->second.stamp < ttl) {
						return &it->second.value;
					}
				}
				if (entries.size() > 128) {
					for (auto sw = entries.begin(); sw != entries.end();) {
						if (now - sw->second.stamp >= ttl) {
							sw = entries.erase(sw);
						} else {
							++sw;
						}
					}
				}
				return nullptr;
			}

			Value& store(const Key& key, Uint64 now) {
				Entry& e = entries[key];
				e.stamp  = now;
				return e.value;
			}
		};

		// Light_beneath_roof: keyed by the light's tile.
		Verdict_cache<std::tuple<int, int, int>, bool> beneath_roof_cache;
		// Light_room_roof_z: keyed by (tx, ty, lift); value = (roof_z, found).
		Verdict_cache<std::tuple<int, int, int>, std::pair<int, bool>> roof_z_cache;
		// Evaluate_light_visibility: keyed by light tile + avatar tile + the
		// caller-provided per-frame flags.
		Verdict_cache<std::tuple<int, int, int, int, int, int, int>, LightVisibility> visibility_cache;
		// Splat_radial_light's propagated field: keyed by a hash of the grid
		// content plus every parameter the field depends on, including the
		// splat centre's offset from the lattice anchor -- which is scroll-
		// invariant, so a placed light keeps its field while the view moves.
		// The field is rasterized ONCE into a byte alpha template (extent in
		// pixels relative to the splat centre): per-frame splatting is then a
		// byte read per pixel instead of a bilinear float sample -- sampling
		// large mostly-lit fields every frame dominated the splat phase.
		using Field_key = std::tuple<uint64_t, int, int, int, int, int, int, int>;

		struct Field_template {
			int                        x0 = 0;    // Extent relative to the splat centre.
			int                        y0 = 0;
			int                        w  = 0;
			int                        h  = 0;
			std::vector<unsigned char> alpha;
		};

		Verdict_cache<Field_key, Field_template> field_cache{1000, true, {}};
	}    // namespace

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
		// Keep the light's own STOREY floor (any storey, not just ground): an
		// upstairs light's enclosure flood must run on its floor, not in the
		// (possibly sealed) room below it.
		const int floor_z = (src.tz / 5) * 5;
		base.tz           = floor_z;
		if (gmap == nullptr) {
			interior = false;
			return base;
		}
		const bool own_roofed = Light_tile_roofed(gmap, base.tx, base.ty);
		const bool own_wall   = Light_tile_wall(gmap, base.tx, base.ty, floor_z);
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
			if (Light_tile_roofed(gmap, nx, ny) && !Light_tile_wall(gmap, nx, ny, floor_z)) {
				interior = true;
				return Tile_coord(Light_tile_norm(nx), Light_tile_norm(ny), floor_z);
			}
		}
		// No roofed interior neighbour: a genuine exterior source.
		interior = own_roofed;
		return base;
	}

	// Bounded flood fill from `start`. Returns true if the roofed enclosure it
	// stands in has a passable gap in its walls that leads outside -- e.g. a
	// doorway with no door object at all, which has no light_passes_through
	// shape to detect. Walls are tested a couple of tiles above the floor so
	// low furniture does not block; reaching a roofless tile means an opening.
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
		auto normalize = [](int t) {
			return (t % c_num_tiles + c_num_tiles) % c_num_tiles;
		};
		const int tgt_tx = normalize(target.tx);
		const int tgt_ty = normalize(target.ty);
		// Window centred midway between light and Avatar, sized to cover both
		// plus slack for winding around furniture.  A fixed window centred on
		// the light made torches in one big hall flip off the moment the
		// Avatar crossed its edge (~12 tiles); the cap only bounds cost --
		// lights that far away are outside any glow radius on screen.
		const int dtx = Tile_coord::delta(normalize(start.tx), tgt_tx);
		const int dty = Tile_coord::delta(normalize(start.ty), tgt_ty);
		const int sep = std::max(std::abs(dtx), std::abs(dty));
		const int R   = std::min(sep / 2 + 12, 40);    // Search radius in tiles.
		const int W   = 2 * R + 1;                     // Window width.
		if (sep / 2 + 12 > 40) {
			return false;    // Too far for the flood to certify anything.
		}
		const int wall_tz = start.tz + 3;
		const int base_tx = start.tx + dtx / 2 - R;
		const int base_ty = start.ty + dty / 2 - R;
		auto      is_wall = [&](int tx, int ty) -> bool {
            const int        wtx   = normalize(tx);
            const int        wty   = normalize(ty);
            Map_chunk* const chunk = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
            if (chunk == nullptr) {
                return true;    // Unknown -> treat as blocking.
            }
            return chunk->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, wall_tz);
		};
		auto is_target = [&](int tx, int ty) -> bool {
			const int ddx = std::abs(Tile_coord::delta(normalize(tx), tgt_tx));
			const int ddy = std::abs(Tile_coord::delta(normalize(ty), tgt_ty));
			return ddx <= 1 && ddy <= 1;
		};
		std::vector<char>                visited(static_cast<size_t>(W) * W, 0);
		std::vector<std::pair<int, int>> queue(static_cast<size_t>(W) * W);
		int                              qhead = 0;
		int                              qtail = 0;
		auto                             push  = [&](int tx, int ty) {
            const int lx = tx - base_tx;
            const int ly = ty - base_ty;
            if (lx < 0 || lx >= W || ly < 0 || ly >= W) {
                return;
            }
            char& seen = visited[static_cast<size_t>(ly) * W + lx];
            if (seen) {
                return;
            }
            seen           = 1;
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

	// True if an actual roof shape covers the light's tile from above.  Scans
	// the light's chunk and its neighbours (a roof spans chunks) for is_roof()
	// / is_floor() objects whose footprint contains the tile, ignoring the
	// light itself.  Purely geometric and avatar-independent -- unlike
	// Map_chunk::is_roof(), which counts ANY blocking object above a lift and
	// flips as Exult hides roofs near the Avatar.
	bool Light_beneath_roof(Game_object* light_obj) {
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			return false;
		}
		const Tile_coord lt = light_obj->get_tile();
		// Purely geometric, so cacheable by tile alone (TTL keeps it honest
		// against roofs being added / removed).
		const Uint64                    now = SDL_GetTicks();
		const std::tuple<int, int, int> key{lt.tx, lt.ty, lt.tz};
		if (const bool* hit = beneath_roof_cache.find(key, now)) {
			return *hit;
		}
		const bool covered = [&]() -> bool {
			const int lcx = lt.tx / c_tiles_per_chunk;
			const int lcy = lt.ty / c_tiles_per_chunk;
			for (int dcy = -1; dcy <= 1; ++dcy) {
				for (int dcx = -1; dcx <= 1; ++dcx) {
					Map_chunk* const chunk = gmap->get_chunk_safely(lcx + dcx, lcy + dcy);
					if (chunk == nullptr) {
						continue;
					}
					Object_iterator it(chunk->get_objects());
					Game_object*    obj;
					while ((obj = it.get_next()) != nullptr) {
						if (obj == light_obj) {
							continue;
						}
						const Shape_info& info     = obj->get_info();
						const bool        is_roof  = info.is_roof();
						const bool        is_floor = info.is_floor();
						if (!is_roof && !is_floor) {
							continue;
						}
						// The cover must be ABOVE the light.  A roof the light sits
						// directly under counts (roof lift >= light tz).  A floor slab
						// used as the storey's ceiling counts only when STRICTLY above:
						// a light standing ON the deck must stay exterior (lit by the
						// open sky), not be roofed by the very slab it stands on.
						if (is_roof ? (obj->get_lift() < lt.tz) : (obj->get_lift() <= lt.tz)) {
							continue;
						}
						if (obj->get_footprint().has_world_point(lt.tx, lt.ty)) {
							return true;
						}
					}
				}
			}
			return false;
		}();
		beneath_roof_cache.store(key, now) = covered;
		return covered;
	}

	LightVisibility Evaluate_light_visibility(
			Game_object* light_obj, Map_chunk* olist, Game_object* main_actor, bool viewer_outside, bool same_chunk,
			bool avatar_sealed, bool chunk_has_opening) {
		// Cache the whole verdict per (light tile, avatar tile, frame flags).
		// The enclosure floods behind it are the expensive part.  Strictly
		// gated on natural light: the legacy palette path also calls this and
		// must keep its exact per-frame behaviour.
		Game_window* const gwin      = Game_window::get_instance();
		const bool         use_cache = gwin != nullptr && gwin->get_natural_light() && main_actor != nullptr;
		const Uint64       now       = SDL_GetTicks();
		std::tuple<int, int, int, int, int, int, int> key;
		if (use_cache) {
			const Tile_coord lt = light_obj->get_tile();
			const Tile_coord at = main_actor->get_tile();
			const int        flags
					= (viewer_outside ? 1 : 0) | (same_chunk ? 2 : 0) | (avatar_sealed ? 4 : 0) | (chunk_has_opening ? 8 : 0);
			key = {lt.tx, lt.ty, lt.tz, at.tx, at.ty, at.tz, flags};
			if (const LightVisibility* hit = visibility_cache.find(key, now)) {
				return *hit;
			}
		}
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
		if (use_cache) {
			visibility_cache.store(key, now) = vis;
		}
		return vis;
	}

	int Light_radius(int brightness) {
		return brightness * 3 * c_tilesize;    // ~3 tiles per brightness level.
	}

	int Light_tier(int brightness) {
		return brightness <= 2 ? 0 : (brightness <= 4 ? 1 : 2);
	}

	int Light_room_roof_z(Game_map* gmap, int tx, int ty, int lift, bool* found) {
		if (found != nullptr) {
			*found = false;
		}
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
		// Geometric per-tile scan: cache it (build_light_layers re-asks per
		// light per frame on top of the flood builders' own calls).
		const Uint64                    now = SDL_GetTicks();
		const std::tuple<int, int, int> key{tx, ty, lift};
		if (const std::pair<int, bool>* hit = roof_z_cache.find(key, now)) {
			if (found != nullptr) {
				*found = hit->second;
			}
			return hit->first;
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
			roof_z_cache.store(key, now) = {roof_z, true};
			if (found != nullptr) {
				*found = true;
			}
			return roof_z;
		}
		// No roof overhead: the storey's classic wall-top threshold.
		roof_z_cache.store(key, now) = {floor_z + 5, false};
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
			std::vector<Light_spill>* spills, bool light_walls = true) {
		const int                  side = 2 * rt + 1;
		std::vector<unsigned char> visited(static_cast<size_t>(side) * side, 0);
		// Breadth-first queue: the fill records each tile's PATH distance from
		// the start (in tiles, stored as distance+1 in `lit`; 0 = unreached),
		// so the splat can fade the light by the distance it actually
		// TRAVELS around walls, not just the straight-line distance.
		std::vector<std::pair<int, int>> queue;
		size_t                           qhead = 0;
		// Outside-tile grid coords + the opening's transmission percent + the
		// opening's TOP z-level (0 for none) + the fill's path distance to the
		// far side: a window mounted high enough to rise above an adjoining
		// floor-roof deck spills OVER the deck surface.
		std::vector<std::tuple<int, int, int, int, int>> spill_cand;
		std::vector<std::pair<int, int>>                 door_cand;    // Roofed->open exit tiles.
		// The room's floor for the wall test is the STOREY floor (storeys are 5 z
		// apart), not the light's own z: a lamp standing on a shelf at tz 4 is
		// still in a ground-floor room whose walls rise from z 0, and the books
		// beside it at lift 4 must not read as rising "from the floor".
		const int floor_z = (lt.tz / 5) * 5;
		// Wall map for the whole grid, stamped in one pass over the covered
		// chunks' object lists (see Light_flood_stamp_walls).
		std::vector<unsigned char> wallmap;
		Light_flood_stamp_walls(gmap, Light_tile_norm(lt.tx - rt), Light_tile_norm(lt.ty - rt), side, roof_z, floor_z, wallmap);
		auto tall = [&](int gx, int gy) {
			return wallmap[static_cast<size_t>(gy) * side + gx] != 0;
		};
		// Memoized opening test: wall tiles are handled on EVERY approach (see
		// the wall branch below, needed for wrap-around fills), but the
		// chunk-object scan behind the opening test must still run only ONCE
		// per tile -- unmemoized it slowed a running player to a walk.
		std::vector<short> openmemo(static_cast<size_t>(side) * side, 0);    // 0 unknown, -1 none, else pct.
		std::vector<int>   opentop(static_cast<size_t>(side) * side, 0);
		auto               opening = [&](int gx, int gy, int* top_z = nullptr) -> int {
            short& m = openmemo[static_cast<size_t>(gy) * side + gx];
            if (m == 0) {
                int       top = 0;
                const int pct = Light_tile_pass_opening(gmap, lt.tx + gx - rt, lt.ty + gy - rt, floor_z, roof_z, &top);
                m                                            = pct > 0 ? static_cast<short>(pct) : -1;
                opentop[static_cast<size_t>(gy) * side + gx] = top;
            }
            if (top_z != nullptr) {
                *top_z = opentop[static_cast<size_t>(gy) * side + gx];
            }
            return m > 0 ? m : 0;
		};
		// Ceiling-well spill candidates (stairwell / ladder openings): interior
		// roofed tiles whose own ceiling band is missing right above them.
		std::vector<std::pair<int, int>> well_cand;
		// Ceiling-cover map for the well test: which grid tiles have a ceiling
		// piece (floor slab or solid shape, band [roof_z, roof_z+4)) above them.
		// Built lazily ONCE per flood by scanning each covered chunk's object
		// list a single time -- a per-tile probe rescanning 4 chunks for every
		// interior tile slowed a running player to a walk.  Ceilings are
		// usually the upper storey's FLOOR slabs: height-0/1 flats that never
		// set the chunk's blocked flags, hence the shape scan.  An object
		// anchors at its south-east corner with the footprint extending
		// up-left, so scan one extra chunk row/col east and south of the grid.
		std::vector<unsigned char> ceilcover;
		// One-pass cover-map builder shared by the ceiling and floor well
		// tests: marks the grid tiles over/under which a solid or floor piece
		// anchors with its lift in [band_lo, band_hi).
		auto build_cover = [&](std::vector<unsigned char>& cover, int band_lo, int band_hi) {
			cover.assign(static_cast<size_t>(side) * side, 0);
			const int tx0 = Light_tile_norm(lt.tx - rt);
			const int ty0 = Light_tile_norm(lt.ty - rt);
			const int ncx = (tx0 % c_tiles_per_chunk + side - 1) / c_tiles_per_chunk + 1;
			const int ncy = (ty0 % c_tiles_per_chunk + side - 1) / c_tiles_per_chunk + 1;
			for (int icy = 0; icy <= ncy; ++icy) {
				for (int icx = 0; icx <= ncx; ++icx) {
					Map_chunk* const ch = gmap->get_chunk_safely(
							(tx0 / c_tiles_per_chunk + icx) % c_num_chunks, (ty0 / c_tiles_per_chunk + icy) % c_num_chunks);
					if (ch == nullptr) {
						continue;
					}
					Object_iterator it(ch->get_objects());
					Game_object*    obj;
					while ((obj = it.get_next()) != nullptr) {
						if (obj->as_actor() != nullptr || obj->is_dragable()) {
							continue;    // Bodies and loose items are not cover.
						}
						const Shape_info& info = obj->get_info();
						if (!info.is_solid() && !info.is_floor()) {
							continue;
						}
						const int ol = obj->get_lift();
						if (ol < band_lo || ol >= band_hi) {
							continue;    // Not in the cover band.
						}
						const TileRect fp = obj->get_footprint();
						for (int fty = fp.y; fty < fp.y + fp.h; ++fty) {
							const int dgy = Light_tile_norm(fty - ty0);
							if (dgy >= side) {
								continue;
							}
							for (int ftx = fp.x; ftx < fp.x + fp.w; ++ftx) {
								const int dgx = Light_tile_norm(ftx - tx0);
								if (dgx >= side) {
									continue;
								}
								cover[static_cast<size_t>(dgy) * side + dgx] = 1;
							}
						}
					}
				}
			}
		};
		auto ceil_open = [&](int gx, int gy) -> bool {
			if (ceilcover.empty()) {
				build_cover(ceilcover, roof_z, roof_z + 4);
			}
			return ceilcover[static_cast<size_t>(gy) * side + gx] == 0;
		};
		// Floor-well spill candidates (an elevated room's stairwell / ladder
		// hole DOWN to the storey below): reached tiles whose own floor band
		// is missing beneath them.  Only elevated rooms qualify.
		std::vector<std::pair<int, int>> down_cand;
		std::vector<unsigned char>       floorcover;
		auto                             floor_open = [&](int gx, int gy) -> bool {
            if (floorcover.empty()) {
                // -1: a slab whose top is flush with the storey floor
                // anchors one z below it.
                build_cover(floorcover, floor_z - 1, floor_z + 4);
            }
            return floorcover[static_cast<size_t>(gy) * side + gx] == 0;
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
		if (!tall(rt, rt)) {
			// The light stands on open floor: flood outward from it.  Its own
			// tile is always lit -- the flame is visible there.
			lit[static_cast<size_t>(rt) * side + rt]     = 1;    // Distance 0.
			visited[static_cast<size_t>(rt) * side + rt] = 1;
			queue.emplace_back(rt, rt);
		} else {
			// A wall-mounted torch sits ON the wall tile itself.  Flooding
			// outward from it would spread to BOTH faces of the wall and light
			// the far side as if the wall were not there.  Instead seed the
			// fill from the room the torch faces -- the orthogonally adjacent
			// non-wall tiles that are ROOFED (interior).  The wall tile itself
			// follows the wall-RING policy: lit like the ring so the flame's
			// spot glows from inside, but dark when light_walls is off (viewer
			// outside) -- its cell straddles both faces of the wall, and at
			// full brightness it shines through a closed door it shares the
			// tile with (the torch-in-doorway leak).
			if (light_walls) {
				set_dist(static_cast<size_t>(rt) * side + rt, 0);
			}
			bool seeded = false;
			for (const auto& d : step) {
				const int nx = rt + d[0];
				const int ny = rt + d[1];
				if (nx < 0 || ny < 0 || nx >= side || ny >= side || tall(nx, ny) || !roofed(nx, ny)) {
					continue;
				}
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				visited[nidx]     = 1;
				set_dist(nidx, 1);
				queue.emplace_back(nx, ny);
				seeded = true;
			}
			if (!seeded) {
				// No roofed interior neighbour (a freestanding or fully exposed
				// wall): fall back to flooding from the wall tile itself.
				lit[static_cast<size_t>(rt) * side + rt]     = 1;    // Distance 0.
				visited[static_cast<size_t>(rt) * side + rt] = 1;
				queue.emplace_back(rt, rt);
			}
		}
		while (qhead < queue.size()) {
			const int gx = queue[qhead].first;
			const int gy = queue[qhead].second;
			++qhead;
			const int gdist = (lit[static_cast<size_t>(gy) * side + gx] & 0x7f) - 1;
			if (spills != nullptr && start_roofed && roofed(gx, gy) && ceil_open(gx, gy)) {
				// The room's own ceiling is missing right above this interior
				// tile while a higher storey still covers it: a stairwell /
				// ladder well.  The light continues UP through it onto the
				// storey above (emitted after the fill completes).
				well_cand.emplace_back(gx, gy);
			}
			if (spills != nullptr && floor_z >= 5 && start_roofed && roofed(gx, gy) && floor_open(gx, gy)) {
				// An elevated room's floor is missing under this reached tile
				// (a stairwell down): the light continues DOWN onto the storey
				// below (emitted after the fill completes).  The high bit marks
				// the hole in the grid itself: the splat exempts the shaft's
				// screen projection from the elevated-light clear-pixel veto,
				// so the light keeps shining down its own well.  Roofed only:
				// a fill escaping outdoors reaches open ground with no floor
				// above it, which is not a well.
				lit[static_cast<size_t>(gy) * side + gx] |= 0x80;
				down_cand.emplace_back(gx, gy);
			}
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
					// Light the wall face, but do not flood past it -- unless
					// light_walls is off (an interior light seen from OUTSIDE):
					// then the wall ring is left dark so the one-tile ring does
					// not show as a bright seam beam between a wall top and an
					// adjoining floor-roof deck.  Spill detection still runs, so
					// the window/opening glow is unaffected.
					if (light_walls) {
						set_dist(nidx, gdist + 1);
						if (spills == nullptr) {
							// Spill-owned grid (no well flags): mark the wall so
							// the splat's face re-probe only lands on wall cells.
							lit[nidx] |= 0x80;
						}
					}
					if (spills != nullptr) {
						int       open_top = 0;
						const int pct      = opening(nx, ny, &open_top);
						if (pct > 0) {
							// Window/grate: the light escapes to the tile on the
							// FAR side of the opening, continuing in the direction
							// the fill approached from.  Record it as a candidate;
							// whether it really points OUTWARD is only known once
							// the fill is complete (see below).
							const int  ox      = nx + d[0];
							const int  oy      = ny + d[1];
							const bool in_grid = ox >= 0 && oy >= 0 && ox < side && oy < side;
							if (in_grid && !tall(ox, oy)) {
								bool dup = false;
								for (const auto& [px, py, ppct, ptop, ppath] : spill_cand) {
									if (px == ox && py == oy) {
										dup = true;
										break;
									}
								}
								if (!dup) {
									// +2: through the wall tile onto the far side.
									spill_cand.emplace_back(ox, oy, pct, open_top, gdist + 2);
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
				if (spills == nullptr && !roofed(gx, gy) && roofed(nx, ny)) {
					// A spill's glow was emitted OUT of a building: crossing
					// back under a roof (a neighbouring room's open doorway or
					// shutterless window hole) would pool the glow inside that
					// room.  Fully dark: even a lit threshold cell smears a
					// visible stripe onto the room via bilinear sampling.
					continue;
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
					if (light_walls) {
						set_dist(nidx, gdist + 1);
						if (spills == nullptr) {
							lit[nidx] |= 0x80;
						}
					}
					continue;
				}
				if (visited[nidx] || tall(gx + d[0], gy) || tall(gx, gy + d[1])) {
					continue;
				}
				if (spills == nullptr && !roofed(gx, gy) && roofed(nx, ny)) {
					// Same open-sky containment as the orthogonal steps.
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
			std::vector<Tile_coord> emitted_all;
			auto                    emit_spill = [&](const Tile_coord& t, int pct, int floor_storey, int path) {
                const int dedupe_r = std::max(2, floor_storey >= 1 ? 4 : 2);
                for (const Tile_coord& e : emitted_all) {
                    if (std::abs(t.tx - e.tx) <= dedupe_r && std::abs(t.ty - e.ty) <= dedupe_r && std::abs(t.tz - e.tz) <= 5) {
                        return;
                    }
                }
                emitted_all.push_back(t);
                spills->push_back({t, pct, floor_storey, path});
			};
			// Path distance the fill recorded at a grid tile (0 when unreached).
			auto path_at = [&](int gx, int gy) {
				const int v = lit[static_cast<size_t>(gy) * side + gx] & 0x7f;
				return v > 0 ? v - 1 : 0;
			};
			// Like doorway/well spills below, nearby wall/window candidates can
			// represent the same physical opening and would otherwise spawn
			// overlapping continuation bubbles with near-identical falloff.
			std::vector<std::pair<int, int>> spill_emitted;
			for (const auto& [ox, oy, pct, open_top, cpath] : spill_cand) {
				const int dedupe_r  = std::max(2, (lt.tz / 5) >= 1 ? 4 : 2);
				bool      near_prev = false;
				for (const auto& [ex, ey] : spill_emitted) {
					if (std::abs(ox - ex) <= dedupe_r && std::abs(oy - ey) <= dedupe_r) {
						near_prev = true;
						break;
					}
				}
				if (near_prev) {
					continue;
				}
				// Emit only if the far-side tile is under OPEN SKY.  A window
				// the fill touched from its OUTSIDE face (after escaping through
				// a door and wrapping around the building) has the room interior
				// as its far side -- roofed -- and emitting it would spill the
				// glow back INSIDE.  (Testing `lit` instead is wrong: the fill
				// wrapping around outside also reaches a window's legitimate
				// outside tile, which must NOT cancel that window's spill.)
				const int fx = lt.tx + ox - rt;
				const int fy = lt.ty + oy - rt;
				// The spill's storey gates it off higher-storey deck surfaces
				// (roof mask 128 + storey).  Take the HIGHER of the light's own
				// storey and the opening's top: a window mounted high in a wall
				// (top z ABOVE the next storey's floor) pokes above an
				// adjoining floor-roof deck, so its glow is drawn OVER the deck
				// surface.  A top flush with the storey floor (a standard
				// ground window ends at z 5) does NOT reach that storey: the
				// light exits below it, so `open_top - 1`.
				int        floor   = std::max(lt.tz / 5, (open_top - 1) / 5);
				int        sp_tz   = 0;
				const bool covered = Light_tile_roofed(gmap, fx, fy);
				if (covered) {
					// Covered -- but when the cover is only a floor-roof deck
					// the window overlooks (its top at or below the opening's
					// top, open sky above), the far side is OUTSIDE after all:
					// the glow lands ON the deck.  Place the spill at the deck's
					// storey floor so it renders at deck height and its storey
					// lets it light the deck's top surface.
					const int deck_top = Light_tile_overlooked_deck(gmap, fx, fy, open_top);
					if (deck_top < 0) {
						continue;    // Far side is interior (or under another roof).
					}
					sp_tz = (deck_top / 5) * 5;
					floor = std::max(floor, deck_top / 5);
				}
				spill_emitted.emplace_back(ox, oy);
				emit_spill(Tile_coord(Light_tile_norm(fx), Light_tile_norm(fy), sp_tz), pct, floor, cpath);
			}
			// Doorway/roof-edge spills.  Neighbouring exit tiles along a wide
			// opening would each spawn a near-identical continuation bubble
			// (same centre give or take a tile, same continued falloff), so
			// thin them out: skip a candidate within two tiles of one already
			// emitted.  Overlapping bubbles combine by max coverage, so the
			// survivors still reproduce the source's dome seamlessly.
			std::vector<std::pair<int, int>> emitted;
			for (const auto& [ox, oy] : door_cand) {
				const int door_dedupe_r = std::max(2, (lt.tz / 5) >= 1 ? 4 : 2);
				bool      near_prev     = false;
				for (const auto& [ex, ey] : emitted) {
					if (std::abs(ox - ex) <= door_dedupe_r && std::abs(oy - ey) <= door_dedupe_r) {
						near_prev = true;
						break;
					}
				}
				if (near_prev) {
					continue;
				}
				emitted.emplace_back(ox, oy);
				// A doorway / roof-edge exit is open air: full transmission.
				// Anchor the bubble at the source room's own storey floor: an
				// upstairs door exits onto a walkway/deck at that level, and a
				// ground-anchored bubble would flood the wrong (ground) room
				// and slide its lattice off the deck.
				emit_spill(
						Tile_coord(Light_tile_norm(lt.tx + ox - rt), Light_tile_norm(lt.ty + oy - rt), floor_z), 100, lt.tz / 5,
						path_at(ox, oy));
			}
			// Ceiling-well spills (stairwell / ladder openings): the light
			// climbs through the hole in its own ceiling and pools on the
			// floor of the storey ABOVE.  The continuation bubble sits at the
			// upper floor's z, so it renders at that height, its storey passes
			// the 128+storey mask gates up there, and its own spill flood
			// (started at roof_z) is bounded by the UPPER room's walls.  A
			// genuine well has a LANDING: an adjacent tile carrying the upper
			// floor with open air beneath it (the room's own headspace).
			// Without one, the "missing ceiling" is just a taller adjoining
			// space the fill flowed into (a low room's door into a high hall),
			// and no upward bubble is emitted.  Like doorways, neighbouring
			// well tiles would spawn near-identical bubbles, so thin
			// candidates within two tiles of one emitted.
			auto has_landing = [&](int gx, int gy) {
				for (const auto& d : step) {
					const int nx = gx + d[0];
					const int ny = gy + d[1];
					if (nx < 0 || ny < 0 || nx >= side || ny >= side || ceil_open(nx, ny)) {
						continue;    // Off-grid, or no upper floor over this neighbour.
					}
					const int        ntx = Light_tile_norm(lt.tx + nx - rt);
					const int        nty = Light_tile_norm(lt.ty + ny - rt);
					Map_chunk* const nch = gmap->get_chunk_safely(ntx / c_tiles_per_chunk, nty / c_tiles_per_chunk);
					if (nch != nullptr && roof_z >= 2
						&& !nch->is_tile_occupied(ntx % c_tiles_per_chunk, nty % c_tiles_per_chunk, roof_z - 2)) {
						return true;    // Upper floor with air beneath: a landing.
					}
				}
				return false;
			};
			std::vector<std::pair<int, int>> well_emitted;
			for (const auto& [ox, oy] : well_cand) {
				bool near_prev = false;
				for (const auto& [ex, ey] : well_emitted) {
					if (std::abs(ox - ex) <= 2 && std::abs(oy - ey) <= 2) {
						near_prev = true;
						break;
					}
				}
				if (near_prev || !has_landing(ox, oy)) {
					continue;
				}
				well_emitted.emplace_back(ox, oy);
				emit_spill(
						Tile_coord(Light_tile_norm(lt.tx + ox - rt), Light_tile_norm(lt.ty + oy - rt), roof_z), 100, roof_z / 5,
						path_at(ox, oy));
			}
			// Floor-well spills: the mirror of the ceiling wells above.  The
			// bubble sits on the storey BELOW (its z / floor let it light the
			// ground-level surfaces the elevated room's own gated splat must
			// not touch).  NOT emitted at the hole tile itself: from there
			// Light_room_roof_z looks straight up the well to the building's
			// roof, giving the bubble's flood the wrong room (the lower walls
			// no longer bound it) and sliding its field anchor 4px per z off.
			// Emit at a LANDING neighbour instead -- one still carrying the
			// elevated floor with open air beneath (the lower room's own
			// headspace), where the lower ceiling is found right above.
			// Thinned like the other candidates.
			auto down_landing = [&](int gx, int gy, int& lx, int& ly) {
				for (const auto& d : step) {
					const int nx = gx + d[0];
					const int ny = gy + d[1];
					if (nx < 0 || ny < 0 || nx >= side || ny >= side || floor_open(nx, ny)) {
						continue;    // Off-grid, or no floor over this neighbour either.
					}
					const int        ntx = Light_tile_norm(lt.tx + nx - rt);
					const int        nty = Light_tile_norm(lt.ty + ny - rt);
					Map_chunk* const nch = gmap->get_chunk_safely(ntx / c_tiles_per_chunk, nty / c_tiles_per_chunk);
					if (nch != nullptr && floor_z >= 2
						&& !nch->is_tile_occupied(ntx % c_tiles_per_chunk, nty % c_tiles_per_chunk, floor_z - 2)) {
						lx = nx;
						ly = ny;
						return true;    // Floor above with air beneath: the room below.
					}
				}
				return false;
			};
			// A/B: down-well continuation bubbles disabled -- the ground-level
			// bubble escapes its room through doors and pools on exterior
			// pavement (scene2b's circled artifact); the in-grid 0x80 shaft
			// band already lets the elevated light shine down its own well.
			constexpr bool emit_down_well_spills = false;
			if (emit_down_well_spills) {
				std::vector<std::pair<int, int>> down_emitted;
				for (const auto& [ox, oy] : down_cand) {
					bool near_prev = false;
					for (const auto& [ex, ey] : down_emitted) {
						if (std::abs(ox - ex) <= 2 && std::abs(oy - ey) <= 2) {
							near_prev = true;
							break;
						}
					}
					int lx = 0;
					int ly = 0;
					if (near_prev || !down_landing(ox, oy, lx, ly)) {
						continue;
					}
					down_emitted.emplace_back(ox, oy);
					emit_spill(
							Tile_coord(Light_tile_norm(lt.tx + lx - rt), Light_tile_norm(lt.ty + ly - rt), floor_z - 5), 100,
							(floor_z - 5) / 5, path_at(ox, oy));
				}
			}
		}
		// Straight-ray shadowing, RING CELLS ONLY (after emission -- the spill
		// budgets above read the path distances this pass clears).  A spill
		// grid's facade ring otherwise wraps around the building corner (the
		// flood reaches the far wall from the outside) and the 132 face gate
		// would light the perpendicular wall's face off it.  Open ground
		// cells are NOT filtered: shadowing them reshaped the outdoor pools
		// into hard-edged cones (rejected look) -- the pools keep the flood's
		// radial falloff.
		{
			const bool spill_grid = spills == nullptr;
			auto       los_clear  = [&](int gx, int gy) {
                const float dx    = static_cast<float>(gx - rt);
                const float dy    = static_cast<float>(gy - rt);
                const int   steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) * 4;
                for (int i = 1; i < steps; ++i) {
                    const float ft = static_cast<float>(i) / static_cast<float>(steps);
                    const int   cx = rt + static_cast<int>(std::lround(dx * ft));
                    const int   cy = rt + static_cast<int>(std::lround(dy * ft));
                    if ((cx == gx && cy == gy) || (cx == rt && cy == rt)) {
                        continue;    // Start / target cells never block themselves.
                    }
                    if (tall(cx, cy)) {
                        return false;
                    }
                }
                return true;
			};
			if (spill_grid) {
				for (int gy = 0; gy < side; ++gy) {
					for (int gx = 0; gx < side; ++gx) {
						const size_t idx = static_cast<size_t>(gy) * side + gx;
						if (lit[idx] == 0 || (gx == rt && gy == rt) || !tall(gx, gy)) {
							continue;
						}
						if (!los_clear(gx, gy)) {
							lit[idx] = 0;
						}
					}
				}
			}
		}
	}

	// Cross-frame cache for room-fill floods.  A flood depends only on its
	// start tile and the static geometry (walls, doors, ceilings -- actors are
	// ignored), yet it rescans chunk object lists tile by tile; with several
	// lights and their window/door/well spills in view, re-flooding ALL of
	// them every frame slowed a running player to a walk.  Results are reused
	// for a short time-to-live -- and beyond it, a per-frame REFRESH BUDGET
	// spreads the recomputation out: entries created together (entering a
	// town) would otherwise all expire together, stacking dozens of floods
	// into single frames every quarter second (a measured 7-10 ms/frame
	// stutter).  A stale entry past the budget keeps being served as-is; each
	// frame refreshes the next few stale entries round-robin, so every entry
	// still tracks a door opening or closing within a fraction of a second.
	// The budget is TIME, not a count: room floods cost up to ~2 ms while
	// spill floods are far cheaper, and misses (new lights entering view,
	// which always compute) charge it too so they displace refreshes.
	namespace {
		struct Flood_cache_entry {
			std::vector<unsigned char> lit;
			std::vector<Light_spill>   spills;
			Uint64                     stamp = 0;    // When the flood was computed.
			Uint64                     used  = 0;    // When it was last requested.
			// Per-entry refresh interval: doubles each time a refresh returns
			// identical content (a resting room), snaps back on any change.
			Uint64 ttl = 250;
		};

		// Key: tile x/y/z, grid radius, flavour (0 = spill flood, 1 = room
		// flood with wall ring, 2 = room flood without).
		using Flood_cache_key = std::tuple<int, int, int, int, int>;
		std::map<Flood_cache_key, Flood_cache_entry> flood_cache;
		constexpr Uint64                             flood_cache_ttl     = 250;     // ms.
		constexpr Uint64                             flood_cache_ttl_max = 1000;    // ms (backoff cap).
		constexpr Uint64                             flood_cache_keep    = 1000;    // ms unused -> evict.
		uint64_t                                     flood_spend         = 0;       // Perf ticks this frame.
		// Bumped whenever a refresh actually CHANGES a cached grid (a door
		// opened/closed): the layer signatures mix it in, so cached coverage
		// rebuilds exactly when flood content changes.
		uint64_t flood_content_gen = 0;

		bool Spills_equal(const std::vector<Light_spill>& a, const std::vector<Light_spill>& b) {
			if (a.size() != b.size()) {
				return false;
			}
			for (size_t i = 0; i < a.size(); ++i) {
				if (a[i].tile != b[i].tile || a[i].percent != b[i].percent || a[i].floor != b[i].floor) {
					return false;
				}
			}
			return true;
		}

		uint64_t Flood_budget_ticks() {
			static const uint64_t budget = SDL_GetPerformanceFrequency() / 1000;    // ~1 ms.
			return budget;
		}

		// RAII: charge the enclosed flood computation to this frame's budget.
		struct Flood_spend_scope {
			uint64_t t0 = SDL_GetPerformanceCounter();

			~Flood_spend_scope() {
				flood_spend += SDL_GetPerformanceCounter() - t0;
			}
		};

		Flood_cache_entry* Flood_cache_find(const Flood_cache_key& key, Uint64 now) {
			auto it = flood_cache.find(key);
			if (it != flood_cache.end()) {
				it->second.used = now;
				if (now - it->second.stamp < it->second.ttl) {
					return &it->second;
				}
				// Expired: recompute only within this frame's budget, else
				// keep serving the stale grid until its round-robin turn.
				if (flood_spend >= Flood_budget_ticks()) {
					return &it->second;
				}
				return nullptr;
			}
			if (flood_cache.size() > 128) {
				// Sweep entries no light has ASKED for lately (stale-but-served
				// entries stay: they belong to lights still on screen) so the
				// cache cannot grow unbounded (carried lights change tile every
				// step).
				for (auto sw = flood_cache.begin(); sw != flood_cache.end();) {
					if (now - sw->second.used >= flood_cache_keep) {
						sw = flood_cache.erase(sw);
					} else {
						++sw;
					}
				}
			}
			return nullptr;
		}
	}    // namespace

	void Flood_cache_frame_begin() {
		flood_spend = 0;
	}

	uint64_t Flood_content_generation() {
		return flood_content_gen;
	}

	void Invalidate_light_caches_near(const Tile_coord& t, int radius_tiles) {
		// A light-blocking shape changed (a shutter or door opened/closed):
		// drop the cached floods and verdicts it may have shaped so the next
		// frame refloods them immediately, instead of waiting out the TTL
		// (up to a second with backoff).  Localized: only entries whose
		// centre lies within the changed tile's reach are evicted, so the
		// per-frame refresh budget still protects against mass refloods.
		auto near_tile = [&](int tx, int ty) {
			const int dx = std::abs(Light_tile_norm(tx) - Light_tile_norm(t.tx));
			const int dy = std::abs(Light_tile_norm(ty) - Light_tile_norm(t.ty));
			const int wx = std::min(dx, c_num_tiles - dx);    // World wrap.
			const int wy = std::min(dy, c_num_tiles - dy);
			return wx <= radius_tiles && wy <= radius_tiles;
		};
		for (auto it = flood_cache.begin(); it != flood_cache.end();) {
			// Key: tile x/y/z, grid radius, flavour; the grid reaches its
			// radius past the centre, so include it in the distance test.
			if (near_tile(std::get<0>(it->first), std::get<1>(it->first))) {
				it = flood_cache.erase(it);
			} else {
				++it;
			}
		}
		for (auto it = beneath_roof_cache.entries.begin(); it != beneath_roof_cache.entries.end();) {
			if (near_tile(std::get<0>(it->first), std::get<1>(it->first))) {
				it = beneath_roof_cache.entries.erase(it);
			} else {
				++it;
			}
		}
		for (auto it = roof_z_cache.entries.begin(); it != roof_z_cache.entries.end();) {
			if (near_tile(std::get<0>(it->first), std::get<1>(it->first))) {
				it = roof_z_cache.entries.erase(it);
			} else {
				++it;
			}
		}
		for (auto it = visibility_cache.entries.begin(); it != visibility_cache.entries.end();) {
			if (near_tile(std::get<0>(it->first), std::get<1>(it->first))) {
				it = visibility_cache.entries.erase(it);
			} else {
				++it;
			}
		}
		// Rebuild the cached light-layer coverage right away too.
		++flood_content_gen;
	}

	void Notify_object_edited(Game_object* obj) {
		Game_window* const gwin = Game_window::get_instance();
		if (obj == nullptr || gwin == nullptr || !gwin->get_natural_light() || obj->as_actor() != nullptr) {
			return;
		}
		const Shape_info& inf = obj->get_info();
		// Solid frame/shape-changers (shutters open/close by SHAPE swap, not
		// frame) rewall rooms; animated shapes change frames every tick and
		// are never room walls, so they must not thrash the flood cache.
		if (inf.is_door() || inf.has_light_passes_info() || (inf.is_solid() && !inf.is_animated())) {
			static const char* const dbg = std::getenv("EXULT_DEBUG_LIGHT_MASK");
			if (dbg) {
				std::fprintf(
						stderr, "[naturallight] object-edit shape=%d frame=%d door=%d passes=%d solid=%d t=%u\n",
						obj->get_shapenum(), obj->get_framenum(), static_cast<int>(inf.is_door()),
						static_cast<int>(inf.has_light_passes_info()), static_cast<int>(inf.is_solid()),
						static_cast<unsigned>(SDL_GetTicks()));
			}
			Invalidate_light_caches_near(obj->get_tile(), 24);
		}
	}

	// Room-fill flood from the light's tile across passable floor, stopped by
	// tall walls (lit as a one-tile ring when `light_walls`; turned off for an
	// interior light viewed from outside, where the ring shows as a bright
	// seam).  Grid layout: index (dty+rt)*(2*rt+1) + (dtx+rt), dtx/dty relative
	// to the light's tile; a nonzero cell holds the flood PATH distance + 1
	// (Chebyshev, in tiles) -- how far the light travels around walls -- so the
	// splat can fade by the longer of path and straight line.  The reached set
	// depends only on the room's shape, so a torch carried around a closed room
	// keeps the same mask.  A wall tile covered by a light_passes_through shape
	// becomes a `spills` entry: the tile just OUTSIDE the opening (absolute
	// coords) plus the opening's transmission percent (doorway / roof-edge
	// exits are open air and carry 100).
	void Build_light_shadow_grid(
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills, bool light_walls) {
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
		const Tile_coord      lt  = light_obj->get_tile();
		const Uint64          now = SDL_GetTicks();
		const Flood_cache_key key{lt.tx, lt.ty, lt.tz, rt, light_walls ? 1 : 2};
		if (Flood_cache_entry* hit = Flood_cache_find(key, now)) {
			lit    = hit->lit;
			spills = hit->spills;
			return;
		}
		const Flood_spend_scope spend;
		const int               roof_z = Light_room_roof_z(gmap, lt.tx, lt.ty, lt.tz);
		Flood_room_grid(gmap, lt, rt, roof_z, lit, &spills, light_walls);
		Flood_cache_entry& entry = flood_cache[key];
		const bool         had   = entry.stamp != 0;
		if (had && entry.lit == lit && Spills_equal(entry.spills, spills)) {
			entry.ttl = std::min<Uint64>(entry.ttl * 2, flood_cache_ttl_max);
		} else {
			entry.ttl = flood_cache_ttl;
			if (had) {
				++flood_content_gen;
			}
			entry.lit    = lit;
			entry.spills = spills;
		}
		entry.stamp = now;
		entry.used  = now;
	}

	void Build_spill_shadow_grid(const Tile_coord& start, int rt, std::vector<unsigned char>& lit, bool light_walls) {
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
		const Uint64          now = SDL_GetTicks();
		const Flood_cache_key key{start.tx, start.ty, start.tz, rt, light_walls ? 0 : 3};
		if (Flood_cache_entry* hit = Flood_cache_find(key, now)) {
			lit = hit->lit;
			return;
		}
		const Flood_spend_scope spend;
		const int               roof_z = Light_room_roof_z(gmap, start.tx, start.ty, start.tz);
		Flood_room_grid(gmap, start, rt, roof_z, lit, nullptr, light_walls);
		Flood_cache_entry& entry = flood_cache[key];
		const bool         had   = entry.stamp != 0;
		if (had && entry.lit == lit) {
			entry.ttl = std::min<Uint64>(entry.ttl * 2, flood_cache_ttl_max);
		} else {
			entry.ttl = flood_cache_ttl;
			if (had) {
				++flood_content_gen;
			}
			entry.lit = lit;
		}
		entry.spills.clear();
		entry.stamp = now;
		entry.used  = now;
	}

	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer, int clip_x0, int clip_y0,
			int clip_x1, int clip_y1) {
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
		const int             side     = grid != nullptr ? 2 * grid_rt + 1 : 0;
		const float           cell     = static_cast<float>(c_tilesize);
		const float           inv_cell = 1.0f / cell;
		const float           half     = 0.5f * (cell - 1.0f);
		const float           grid_u   = static_cast<float>(grid_fx) - half - static_cast<float>(grid_rt) * cell;
		const float           grid_v   = static_cast<float>(grid_fy) - half - static_cast<float>(grid_rt) * cell;
		std::vector<float>    field_local;
		const Field_template* ftmpl = nullptr;
		if (grid != nullptr) {
			// Reuse a recently computed field: FNV-1a over the grid content
			// (the flood cache above already hands back the identical grid for
			// a resting light) plus the dome parameters.
			uint64_t     ghash = 1469598103934665603ULL;
			const size_t cells = static_cast<size_t>(side) * side;
			for (size_t i = 0; i < cells; ++i) {
				ghash = (ghash ^ grid[i]) * 1099511628211ULL;
			}
			const Field_key fkey{ghash, radius, elevation, dist_bias, intensity_pct, grid_rt, sx - grid_fx, sy - grid_fy};
			const Uint64    fnow = SDL_GetTicks();
			ftmpl                = field_cache.find(fkey, fnow);
			if (ftmpl == nullptr) {
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
				const unsigned char mbase   = grid[static_cast<size_t>(cgy) * side + cgx] & 0x7f;
				float               base_px = mbase > 0 ? static_cast<float>((mbase - 1) * c_tilesize) : 0.0f;
				// The rebase only absorbs the anchor/elevation lattice shift:
				// the shifted cell sits Chebyshev(cell, centre) tiles from the
				// light's own tile.  When it is only reachable the long way
				// round -- a torch AT a wall, the shifted cell lying on the
				// wall's FAR side, its path the wrap around the whole building
				// -- rebasing by that path would zero the entire detour and
				// pool full light outside a closed door.  Cap it.
				const int cheb = std::max(std::abs(cgx - grid_rt), std::abs(cgy - grid_rt));
				if (const float cap_px = static_cast<float>(cheb * c_tilesize); base_px > cap_px) {
					base_px = cap_px;
				}
				field_local.assign(static_cast<size_t>(side) * side, 0.0f);
				for (int gy = 0; gy < side; ++gy) {
					for (int gx = 0; gx < side; ++gx) {
						const unsigned char m = grid[static_cast<size_t>(gy) * side + gx] & 0x7f;
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
							const float fv                                   = 255.0f * dome * inten;
							field_local[static_cast<size_t>(gy) * side + gx] = fv;
						}
					}
				}
				// Rasterize the field into the byte template over the splat's full
				// (unclipped) extent -- the same extent as the bbox below, relative
				// to the splat centre -- with the identical bilinear sampling the
				// pixel loop used to do per frame.
				Field_template t;
				t.x0 = std::min(-radius, grid_fx - sx - radius - c_tilesize);
				t.y0 = std::min(-radius, grid_fy - sy - radius - c_tilesize);
				t.w  = std::max(radius, grid_fx - sx + radius + c_tilesize) - t.x0 + 1;
				t.h  = std::max(radius, grid_fy - sy + radius + c_tilesize) - t.y0 + 1;
				t.alpha.assign(static_cast<size_t>(t.w) * t.h, 0);
				for (int py = 0; py < t.h; ++py) {
					float v = (static_cast<float>(sy + t.y0 + py) - grid_v) * inv_cell;
					if (v < 0.0f) {
						v = 0.0f;
					} else if (v > static_cast<float>(side - 1)) {
						v = static_cast<float>(side - 1);
					}
					int h0 = static_cast<int>(v);
					if (h0 > side - 2) {
						h0 = side - 2;
					}
					const float    tv   = v - static_cast<float>(h0);
					const float*   row0 = field_local.data() + static_cast<size_t>(h0) * side;
					unsigned char* out  = t.alpha.data() + static_cast<size_t>(py) * t.w;
					for (int px = 0; px < t.w; ++px) {
						float u = (static_cast<float>(sx + t.x0 + px) - grid_u) * inv_cell;
						if (u < 0.0f) {
							u = 0.0f;
						} else if (u > static_cast<float>(side - 1)) {
							u = static_cast<float>(side - 1);
						}
						int g0 = static_cast<int>(u);
						if (g0 > side - 2) {
							g0 = side - 2;
						}
						const float  tu  = u - static_cast<float>(g0);
						const float* r   = row0 + g0;
						const float  top = r[0] + (r[1] - r[0]) * tu;
						const float  bot = r[side] + (r[side + 1] - r[side]) * tu;
						const int    a   = static_cast<int>(top + (bot - top) * tv + 0.5f);
						if (a > 0) {
							out[px] = static_cast<unsigned char>(a > 255 ? 255 : a);
						}
					}
				}
				Field_template& stored = field_cache.store(fkey, fnow);
				stored                 = std::move(t);
				ftmpl                  = &stored;
			}
		}
		// Screen projections of the room's own floor holes (well cells flagged
		// 0x80 in the grid) were used here for a shaft-band exemption; removed
		// to restore the pre-floor-roofs clear-ground rendering (the band's
		// hard edges read as artifacts on the storey below).
		(void)light_floor_storey;
		(void)anchor_z;
		// ELEVATED spills only (walkway/deck bubbles): the source sits `bias`
		// px behind the opening, so their wall glow falls off Euclidean
		// (sqrt(d^2 + bias^2)) with the lateral reach capped at 1.5x radius
		// and the dome normalized to hit 0 exactly there.  All other lights
		// keep the classic linear continuation (d + bias) with reach =
		// radius -- the long-standing ground-building rendering.
		const bool  euclid_spill = is_spill && light_top_storey >= 1 && bias > 0.0f;
		const float lat_rr       = rf * rf + 2.0f * rf * bias;
		const int   reach = euclid_spill ? std::min(static_cast<int>(std::ceil(std::sqrt(lat_rr))), (3 * radius) / 2) : radius;
		// The free dome is bounded by `reach` around the splat centre; the
		// propagated field by `radius` around the GRID centre (cells beyond it
		// go dark in the per-tile dome) plus a tile of bilinear support.  The
		// two centres differ (elevation shift vs. anchor level), so the bbox
		// must cover both or the field gets cut off on one side.
		int x0 = sx - reach;
		int x1 = sx + reach;
		int y0 = sy - reach;
		int y1 = sy + reach;
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
		// Optional clip window (scroll-vacated strip patching).
		if (clip_x1 >= 0 || clip_y1 >= 0) {
			x0 = std::max(x0, clip_x0);
			y0 = std::max(y0, clip_y0);
			x1 = std::min(x1, clip_x1);
			y1 = std::min(y1, clip_y1);
			if (x0 > x1 || y0 > y1) {
				return;
			}
		}
		// Hot-loop constants for the free-dome path.
		const float amp     = 255.0f * inten;
		const float rr      = static_cast<float>(reach) * static_cast<float>(reach);
		const float dome_dn = rr + bias * bias + e2;
		for (int y = y0; y <= y1; ++y) {
			const int            dy      = y - sy;
			const float          dy2     = static_cast<float>(dy) * static_cast<float>(dy);
			const unsigned char* roofrow = roofpix ? roofpix + y * roof_lw : nullptr;
			// Template row for this scanline, pre-offset so trow[x] is the
			// field alpha at screen x.  The euclid-spill bbox can extend past
			// the template (reach > radius), so rows outside it stay null.
			const unsigned char* trow = nullptr;
			if (ftmpl != nullptr) {
				const int trow_y = y - sy - ftmpl->y0;
				if (trow_y >= 0 && trow_y < ftmpl->h) {
					trow = ftmpl->alpha.data() + static_cast<size_t>(trow_y) * ftmpl->w - (sx + ftmpl->x0);
				}
			}
			for (int x = x0; x <= x1; ++x) {
				// Roof-mask byte: 255 = real roof, 128 + storey = tall shape or
				// upper-storey surface.  The branches below decide per light
				// kind (veto / spill / exterior) what stays dark, what is lit
				// as a whole unit (bypass_field) and what keeps sampling the
				// propagated field.
				bool bypass_field = false;
				if (roofrow && roofrow[x]) {
					if (veto_roof) {
						// An under-roof light keeps marked pixels dark: real
						// roofs (255) and whole-unit tall marks (128 -- tree
						// canopies, deck objects) never light up under it.
						// EXCEPTION: a surface one or more storeys up (129+ --
						// a floor-slab TOP, an upper-storey wall or furnishing)
						// is something the light's fill can genuinely reach --
						// but ONLY when the light's own room roof rises ABOVE
						// that storey (light_top_storey): the upper room's own
						// lamp lights its walls, while the light of the room
						// BELOW (its roof is that storey's floor) must never
						// brighten them from underneath.  It samples the
						// propagated field like clear ground, so where the fill
						// never reached, the field is 0 and it stays dark.
						if (roofrow[x] < 129 || roofrow[x] - 128 >= light_top_storey) {
							continue;    // Keep dark under this interior light.
						}
					} else if (is_spill) {
						if (roofrow[x] == 255) {
							continue;    // A spill never lights a real roof.
						}
						if (roofrow[x] == 132) {
							// Exterior wall FACE: its pixels overlap the lattice
							// cells BEHIND it up-screen, so the plain field sample
							// borrows the window fan's ground light for a wall
							// facing the other way.  Walk toward the foot (4px
							// per z, down-right) to the wall's own RING cell
							// (0x80-flagged in spill grids) and light the face
							// only where that ring carries light: the facade
							// near the opening glows, the far side of the
							// corner does not.
							if (grid == nullptr) {
								continue;
							}
							bool ring_lit = false;
							for (int stp = 0; stp <= 7; ++stp) {
								const float pu = (static_cast<float>(x + stp * 4) - grid_u) * inv_cell;
								const float pv = (static_cast<float>(y + stp * 4) - grid_v) * inv_cell;
								const int   cu = static_cast<int>(std::lround(pu));
								const int   cv = static_cast<int>(std::lround(pv));
								if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
									break;
								}
								const unsigned char m = grid[static_cast<size_t>(cv) * side + cu];
								if (m & 0x80) {
									ring_lit = (m & 0x7f) != 0;
									break;
								}
							}
							if (!ring_lit) {
								continue;
							}
							// Falls through to the normal field sample below.
						} else if (roofrow[x] - 128 > spill_floor) {
							// The marked surface sits on a HIGHER storey than
							// the spill's source: a ground-floor window's glow
							// must not brighten a floor-roof deck above it.
							continue;
						}
						// An upper-storey mark (128 + storey, storey >= 1: a
						// floor-slab TOP, an upper wall) sits on the tile
						// lattice like clear ground: keep sampling the
						// propagated field so the spill pools across the deck
						// bounded by its walls (the pre-floor-roofs rendering)
						// and wall faces fade quickly away from the opening.
						// Only plain 128 marks -- standing deck objects,
						// canopies, whose sprites span far more screen than
						// their tiles -- take the whole-unit free-dome bypass.
						if (roofrow[x] < 129) {
							bypass_field = true;
						} else {
							bypass_field = false;
						}
					} else {
						bypass_field = true;
					}
				} else {
					if (roofrow && is_spill && (inside_viewer || light_top_storey >= 1)) {
						// An ELEVATED spill (a stairwell bubble on an upper
						// floor; light_top_storey carries its render storey):
						// clear pixels are ground-level surfaces -- the walls
						// and terrain BELOW it, over which its z-blind field
						// cells hang up-screen.  Lighting them shows as a
						// bright beam under the storey; everything the bubble
						// may light up there is marked 128 + storey.
						// INSIDE viewer: other buildings' ground walls have
						// CLEAR faces that render up-screen of their feet and
						// z-blind-sample the spill's lit outdoor cells (glow
						// on a neighbour facade through nothing).  Escaped
						// ground light is already the main splat's field
						// continuing through the opening with true path
						// falloff -- the spill keeps only its whole-unit
						// 128 marks (tree canopies) when the viewer is inside.
						continue;
					}
				}
				int a;
				if (grid != nullptr && !bypass_field) {
					// The field bounds itself within the template (cells beyond
					// the pool are dark, their template bytes 0); OUTSIDE the
					// template -- the euclid-spill ring between radius and reach
					// -- the field is definitionally dark, so clip instead of
					// reading past the alpha buffer.  No screen-circle clip: the
					// circle is centred on the elevation-shifted splat centre
					// while the field sits on the anchor lattice, and clipping
					// the field by it would cut a sharp edge into the pool's
					// up-left arc.
					const int tx = x - sx - ftmpl->x0;
					a            = (trow != nullptr && tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
				} else {
					// Free dome (exterior lights, and marked tall shapes /
					// roofs lit as whole units): pure radial falloff, with
					// the travelled distance continued past `bias` for spills.
					const int   dx    = x - sx;
					const float dist2 = static_cast<float>(dx) * static_cast<float>(dx) + dy2;
					if (dist2 > rr) {
						continue;    // Outside the pool's ground reach.
					}
					float dome;
					if (euclid_spill) {
						dome = 1.0f - (dist2 + bias * bias + e2) / dome_dn;
					} else {
						float total2 = dist2;
						if (bias > 0.0f) {
							const float tot = std::sqrt(dist2) + bias;
							total2          = tot * tot;
						}
						dome = 1.0f - (total2 + e2) / rf2;
					}
					a = static_cast<int>(amp * dome + 0.5f);
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
