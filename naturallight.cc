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

#include "actors.h"
#include "chunks.h"
#include "gamemap.h"
#include "gamewin.h"
#include "objiter.h"
#include "objs.h"
#include "ordinfo.h"
#include "path.h"
#include "paths.h"
#include "shapeinf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
			// under it, and the eave is painted as a real roof (Mask::Roof) that
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
		// Only a room taller than one standard course can have a wall that no
		// single shape carries to the roof.  In an ordinary room the per-shape
		// test must stand: there a window tile's column is sealed too, and
		// calling it a wall re-routes the window from the door path (the fill
		// flows through the glass) to the window-spill path, changing the light
		// in every house with an opening.
		const bool tall_room = roof_z > floor_z + 5;
		// A FLOOR slab never sets the chunk's blocked flags, so is_tile_occupied
		// reports a hole wherever one sits.  A hall's courses (lift 0 h 5, lift
		// 6 h 5) leave exactly that hole at z 5, where the storey's floor is --
		// without this the column always reads broken and the wall is invisible
		// to the flood, so the fill walks straight out through it.
		auto floor_fills = [&](Map_chunk* ch, int wtx, int wty, int z) {
			if (ch == nullptr) {
				return false;
			}
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				const Shape_info& oi = o->get_info();
				if (!oi.is_floor() || o->as_actor() != nullptr || o->is_dragable()) {
					continue;
				}
				const int lo = o->get_lift();
				const int hi = lo + std::max(oi.get_3d_height(), 1);
				if (z >= lo && z < hi && o->get_footprint().has_world_point(wtx, wty)) {
					return true;
				}
			}
			return false;
		};
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
					// A top below the roof is tested PER TILE below: in a tall hall
					// the wall is a low course capped by a window section, and no
					// single shape spans floor to roof.
					const int shape_top = lift + info.get_3d_height();
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
							// The room's roof is not flat: roof_z is measured over the
							// LIGHT's tile, and judging a wall 30 tiles away against
							// it reported a gap wherever the roof steps down -- the
							// wall reaches its own ceiling and was rejected anyway.
							// Use the roof over THIS tile when the chunk knows one.
							const int local_roof = [&] {
								const int r = ch->is_roof(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, floor_z);
								return (r < 31 && r > floor_z) ? r : roof_z;
							}();
							// A free-standing solid is an OBJECT, not a room wall:
							// outdoors roof_z falls back to floor_z + 5, which any tree
							// clears (shape 453 is h=7), so each one sealed the fill and
							// threw a wedge of shadow across the spill fan.  Ask the
							// same verdict the kind channel uses -- ch->is_roof() is
							// NOT a substitute, it reports a roof over the tree's own
							// tile and kept every tree a wall.
							// Only a COMPACT one, though: a trunk stands on a tile or
							// two, while a free-standing battlement (shape 192, 4x4,
							// h 5) is just as roofless and failed the same test, so
							// the fan ran straight through it -- no shadow behind it,
							// no ring arrival on its face, lit flat as a tree crown.
							// Only a roofless one blocks: indoors a big non-wall solid is furniture.
							const bool compact = (fp.w <= 2 && fp.h <= 2) || [&] {
								bool             roofed = false;
								const Tile_coord ot     = obj->get_tile();
								NaturalLight::Light_room_roof_z(gmap, ot.tx, ot.ty, ot.tz, &roofed);
								return roofed;
							}();
							if (compact && !NaturalLight::Object_is_wall_face(obj)) {
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
									if (!ch->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z)
										&& !floor_fills(ch, wtx, wty, z)) {
										gap = true;
										break;
									}
								}
								if (gap) {
									continue;
								}
							}
							if (shape_top < local_roof) {
								// Light passes OVER it unless the COLUMN reaches the
								// roof -- a hall's stacked courses.  Such a tile is
								// stamped 2, not 1: it stops the fill (without it the
								// light walks out along the wall line and pops out
								// from under the floor roof) but never becomes a lit
								// wall FACE, which is what made this rule worsen the
								// masking every time it was tried.
								if (!tall_room) {
									continue;
								}
								bool gap_above = false;
								for (int z = shape_top; z < local_roof; ++z) {
									if (!ch->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z)
										&& !floor_fills(ch, wtx, wty, z)) {
										gap_above = true;
										break;
									}
								}
								if (gap_above) {
									continue;
								}
								// Blocks the fill, but is not a face: see above.
								if (wall[static_cast<size_t>(dgy) * side + dgx] == 0) {
									wall[static_cast<size_t>(dgy) * side + dgx] = 2;
								}
								continue;
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

	// Find the first shape in `chunk` that light passes through (window, open
	// door, grate); reports the opening's shape/frame/tile/lift.
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

	// Decide whether the light living in `olist` (the chunk being painted)
	// reaches the viewer, composing the enclosure/opening tests above.  The
	// flags are per-frame / per-chunk facts the caller already computed.
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
		}
		if (viewer_outside) {
			// Interior light reaches the outside viewer only if it can escape its
			// enclosure. Palette lighting is global, so this only approximates
			// spatial light.
			vis.blocked = !vis.source_can_escape;
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

	// The z-level of the roof over the given absolute tile -- the lowest solid
	// cover above the light -- or floor + 5, the classic wall-top threshold,
	// when none is found.  `found` reports whether an actual ceiling shape was
	// seen (a dungeon's rock ceiling is a plain solid, not a roof shape).
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

	// Strict per-frame light-passes match; 0% entries block.
	bool Object_passes_light(Game_object* obj) {
		if (obj == nullptr) {
			return false;
		}
		const Shape_info& info = obj->get_info();
		if (!info.has_light_passes_info()) {
			return false;
		}
		int  match_frame;
		bool has_explicit;
		bool has_wildcard;
		int  percent;
		return Shape_light_passes_through_strict(info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent);
	}

	// A wall-shell member is solid, immovable, non-light-passing (strict
	// per-frame), not an open door leaf, and rises to its tile's FOUND roof.
	// A lamppost (no roof overhead) or a trough under a tree canopy (does not
	// rise to it) is an OBJECT instead.
	bool Object_is_wall_face(Game_object* obj) {
		if (obj == nullptr || obj->as_actor() != nullptr || obj->is_dragable()) {
			return false;
		}
		const Shape_info& info = obj->get_info();
		if (!info.is_solid() || info.is_floor() || info.is_roof()) {
			return false;
		}
		if (info.is_door() && !obj->is_closed_door()) {
			return false;    // An open leaf stands free of the wall line.
		}
		if (info.has_light_passes_info()) {
			int  match_frame;
			bool has_explicit;
			bool has_wildcard;
			int  percent;
			if (Shape_light_passes_through_strict(info, obj->get_framenum(), match_frame, has_explicit, has_wildcard, percent)) {
				return false;    // Pane / grate: glows with the light behind it.
			}
		}
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			return false;
		}
		const Tile_coord t      = obj->get_tile();
		bool             found  = false;
		const int        roof_z = Light_room_roof_z(gmap, t.tx, t.ty, t.tz, &found);
		// Per-SHAPE on purpose.  Accepting a stacked column here (so a hall's
		// course under a window classifies as wall face) fixed that one piece
		// but lit halfwalls in open buildings and re-masked objects drawn over
		// them: this verdict feeds too many consumers to widen safely.
		return found && t.tz + info.get_3d_height() >= roof_z;
	}

	// Fan perpendicular offset past the opening's mouth; `mouth` packs the
	// opening tiles on the -perp side (low byte) and +perp side (next byte).
	static inline int Cone_perp(int relx, int rely, int cdx, int cdy, int mouth) {
		const int sp = relx * cdy - rely * cdx;
		return sp >= 0 ? std::max(0, sp - ((mouth >> 8) & 0xff)) : std::max(0, -sp - (mouth & 0xff));
	}

	static inline float Cone_perp_f(float relx, float rely, int cdx, int cdy, int mouth) {
		const float sp = relx * static_cast<float>(cdy) - rely * static_cast<float>(cdx);
		return sp >= 0.0f ? std::max(0.0f, sp - static_cast<float>((mouth >> 8) & 0xff))
						  : std::max(0.0f, -sp - static_cast<float>(mouth & 0xff));
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
			std::vector<Light_spill>* spills, bool light_walls = true, std::vector<unsigned char>* ring = nullptr, int cone_dx = 0,
			int cone_dy = 0, bool source_under_roof = false, int cone_mouth = 0) {
		const int                  side = 2 * rt + 1;
		std::vector<unsigned char> visited(static_cast<size_t>(side) * side, 0);
		// Per-wall-cell FACE arrival distances (layout and flags: Ring in
		// naturallight.h; distance + 1, 0 = never arrived).  A wall sprite is the same art from either side, so
		// the consumer picks the arrival matching the VIEWER's side; diagonal
		// touches are corner posts and never certify a face.
		if (ring != nullptr) {
			ring->assign(static_cast<size_t>(side) * side * Ring::Bytes, 0);
		}
		// Breadth-first queue: the fill records each tile's PATH distance from
		// the start (in tiles, stored as distance+1 in `lit`; 0 = unreached),
		// so the splat can fade the light by the distance it actually
		// TRAVELS around walls, not just the straight-line distance.
		std::vector<std::pair<int, int>> queue;
		size_t                           qhead = 0;
		// Outside-tile grid coords + the opening's transmission percent + the
		// opening's TOP z-level (0 for none) + the fill's path distance to the
		// far side + the opening's own wall tile (grid coords): a window
		// mounted high enough to rise above an adjoining floor-roof deck
		// spills OVER the deck surface.
		std::vector<std::tuple<int, int, int, int, int, int, int>> spill_cand;
		// Roofed->open exit tiles + the crossing's SOURCE tile: through-flow
		// carries the fill THROUGH window glass, so a "doorway" exit whose
		// source tile is a pass-through opening is really a window spill.
		std::vector<std::tuple<int, int, int, int>> door_cand;
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
		// A tile that only BLOCKS (2) stops the fill but takes no face arrivals.
		auto wall_is_face = [&](int gx, int gy) {
			return wallmap[static_cast<size_t>(gy) * side + gx] == 1;
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
		// anchors with its lift in [band_lo, band_hi).  `accept(info, lift)`
		// overrides that test for other cover kinds (real roof shapes below).
		auto build_cover_if = [&](std::vector<unsigned char>& cover, auto&& accept) {
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
						if (!accept(info, obj->get_lift())) {
							continue;
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
		auto build_cover = [&](std::vector<unsigned char>& cover, int band_lo, int band_hi) {
			build_cover_if(cover, [&](const Shape_info& info, int ol) {
				return (info.is_solid() || info.is_floor()) && ol >= band_lo && ol < band_hi;
			});
		};
		auto ceil_open = [&](int gx, int gy) -> bool {
			if (ceilcover.empty()) {
				build_cover(ceilcover, roof_z, roof_z + 4);
			}
			return ceilcover[static_cast<size_t>(gy) * side + gx] == 0;
		};
		// REAL roof over a tile: a roof shape, or an upper storey's floor slab.
		// The chunk is_roof column used by roofed() counts ANY solid taller
		// than 4 standing on the tile -- a tree, a free-standing wall --
		// so the fan's inside test called the tile under such an object a
		// foreign interior and stripped it, with everything the fill reached
		// through it: a dark line on the ground right before a garden wall,
		// and a shadow wedge behind every tree in the fan.
		std::vector<unsigned char> roofcover;
		auto                       real_roofed = [&](int gx, int gy) -> bool {
            if (roofcover.empty()) {
                build_cover_if(roofcover, [&](const Shape_info& info, int ol) {
                    return (info.is_roof() && ol > 0) || (info.is_floor() && ol >= 5);
                });
            }
            return roofcover[static_cast<size_t>(gy) * side + gx] != 0;
		};
		// Floor-well tiles (an elevated room's stairwell / ladder hole DOWN to
		// the storey below): reached tiles whose own floor band is missing
		// beneath them.  Only elevated rooms qualify.
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
		// A light source is "in a room" only per the OBJECT-level roof test
		// (real roof/floor shapes): the chunk is_roof column also counts the
		// light's OWN head (a streetlamp's glass top) and tree canopies --
		// which made an exterior lamp read interior, emit spills through its
		// own light-passing head, and esc-strip its whole pool.
		const bool start_roofed = spills != nullptr && source_under_roof && roofed(rt, rt);
		// Has the fill ESCAPED the start enclosure by the time it reaches a
		// cell -- crossed a roof boundary or passed a light-passes opening?
		// Robust against eaves over walkways, unlike per-cell roofed-ness.
		std::vector<unsigned char> esc(static_cast<size_t>(side) * side, start_roofed ? 0 : 1);
		// A SPILL grid is built with spills == nullptr, so start_roofed is false
		// by construction and the esc strip below can never run for it -- the fan
		// paints straight into any building it reaches.  Track the mirror image
		// instead: once the fill crosses from open sky under a roof it is INSIDE
		// a foreign building, and stays so.
		std::vector<unsigned char> insid(static_cast<size_t>(side) * side, 0);
		// Record a tile's path distance (+1), keeping the smallest when a wall
		// face is reached again from another direction.
		auto set_dist = [&](size_t idx, int dist) {
			const int v = (dist < 254 ? dist : 254) + 1;
			if (lit[idx] == 0 || lit[idx] > v) {
				lit[idx] = static_cast<unsigned char>(v);
			}
		};
		// Same, for one FACE slot of a wall cell in the ring channel, keeping
		// the slot's flag bit.
		auto set_ring = [&](size_t cell, int face, int dist) {
			if (ring == nullptr) {
				return;
			}
			const int      v   = (dist < 126 ? dist : 126) + 1;
			unsigned char& r   = (*ring)[cell * Ring::Bytes + face];
			const int      cur = Ring::Dist(r);
			if (cur == 0 || cur > v) {
				r = static_cast<unsigned char>((r & Ring::Flag) | v);
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
			const int gdist = Grid::Dist(lit[static_cast<size_t>(gy) * side + gx]) - 1;
			if (spills != nullptr && start_roofed && roofed(gx, gy) && ceil_open(gx, gy)) {
				// The room's own ceiling is missing right above this interior
				// tile while a higher storey still covers it: a stairwell /
				// ladder well.  The light continues UP through it onto the
				// storey above (emitted after the fill completes).
				well_cand.emplace_back(gx, gy);
			}
			if (spills != nullptr && floor_z >= 5 && start_roofed && roofed(gx, gy) && floor_open(gx, gy)) {
				// An elevated room's floor is missing under this reached tile
				// (a stairwell down).  Grid::Flag marks the hole: the splat
				// exempts the shaft's screen projection from the elevated-light
				// clear-pixel veto, so the light keeps shining down its own well.
				// No bubble is emitted below: it escaped the lower room through
				// its doors and pooled on exterior pavement.  Roofed only: a fill
				// escaping outdoors reaches open ground with no floor above it,
				// which is not a well.
				lit[static_cast<size_t>(gy) * side + gx] |= Grid::Flag;
			}
			for (const auto& d : step) {
				const int nx = gx + d[0];
				const int ny = gy + d[1];
				if (nx < 0 || ny < 0 || nx >= side || ny >= side) {
					continue;
				}
				// SPILL CONE: light that came THROUGH an opening exits as a
				// fan in front of it (45 degrees to each side of the outward
				// axis, +/-1 tile at the mouth -- wide openings emit several
				// apexes whose fans union), never as a free bubble wrapping
				// back around the building.  Walls outside the fan get no
				// face arrivals either.  A narrow corridor BEHIND the apex
				// stays in, 4 tiles deep: the room field is esc-stripped past
				// the crossing while its wall-top anchor samples ~3 tiles
				// down-screen, so the fan must repaint the doorway strip or a
				// gray seam shows at the mouth.
				if (cone_dx != 0 || cone_dy != 0) {
					const int relx  = nx - rt;
					const int rely  = ny - rt;
					const int along = relx * cone_dx + rely * cone_dy;
					const int perp  = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
					if (along < 0 ? (along < -4 || perp > 1) : perp > along + 1) {
						continue;
					}
					// The corridor is for a DOORWAY mouth (bare gap).  Through a
					// window it walks the fan back in through its own glass and
					// lights the room behind -- the apex sits one tile out, so the
					// first step back is always along -1.
					if (along < 0 && opening(nx, ny) > 0) {
						continue;
					}
				}
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				if (tall(nx, ny)) {
					// A blocking-only tile (a hall's stacked courses) stops the
					// fill but is no FACE: it takes no arrivals and no ring bits.
					// Spill detection below still runs for it -- gating that out
					// silenced every first-floor window.
					const bool nface = wall_is_face(nx, ny);
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
					if (light_walls && nface) {
						set_dist(nidx, gdist + 1);
						if (spills == nullptr) {
							// Spill-owned grid (no well flags): mark the wall so
							// the splat's face re-probe only lands on wall cells.
							lit[nidx] |= Grid::Flag;
						}
					}
					// FACE arrival channel, independent of light_walls: record
					// which side the fill reached this wall from (stepping
					// north hits its S side, etc.).  Escaped_flag / Inside_flag mark a
					// cell reached by ESCAPED / interior fill: only cells
					// touched exclusively by escaped light may certify exterior
					// whole-unit objects.
					if (ring != nullptr && nface) {
						Ring::Set(ring->data(), nidx, Ring::Wall_flag);
						Ring::Set(
								ring->data(), nidx,
								esc[static_cast<size_t>(gy) * side + gx] != 0 ? Ring::Escaped_flag : Ring::Inside_flag);
					}
					if (!nface) {
						// No arrivals for a blocking-only tile.
					} else if (d[1] < 0) {
						set_ring(nidx, Ring::S, gdist + 1);
					} else if (d[1] > 0) {
						set_ring(nidx, Ring::N, gdist + 1);
					} else if (d[0] < 0) {
						set_ring(nidx, Ring::E, gdist + 1);
					} else if (d[0] > 0) {
						set_ring(nidx, Ring::W, gdist + 1);
					}
					if (spills != nullptr && esc[static_cast<size_t>(gy) * side + gx] == 0) {
						// ESCAPED fill wrapping outside must not spawn spills at
						// other openings it brushes -- light does not bend around
						// buildings; only the room's own openings emit.
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
								for (const auto& [px, py, ppct, ptop, ppath, pwx, pwy] : spill_cand) {
									if (px == ox && py == oy) {
										dup = true;
										break;
									}
								}
								if (!dup) {
									// +2: through the wall tile onto the far side.
									spill_cand.emplace_back(ox, oy, pct, open_top, gdist + 2, nx, ny);
								}
							}
						}
					}
					continue;
				}
				if (start_roofed && roofed(gx, gy) && !roofed(nx, ny) && esc[static_cast<size_t>(gy) * side + gx] == 0) {
					// The fill steps out from under the roof into the open:
					// a doorway, a wall gap, the edge of a porch roof.  That
					// first open-sky tile carries the bubble's continuation.
					// Checked BEFORE the visited gate for the same reason as
					// walls above: the wrap-around fill can visit the exit
					// tile from the open side first (no transition there),
					// which must not swallow the real roofed->open crossing.
					// Duplicates are thinned at emission.
					door_cand.emplace_back(nx, ny, gx, gy);
				}
				// (Spill fans may cross under roofs: the CONE already keeps the
				// fan out of its own source room, and entering neighbouring
				// interiors through openings/doorways within the fan is the
				// point -- an eave strip no longer blocks the way in.)
				if (visited[nidx]) {
					continue;
				}
				visited[nidx] = 1;
				set_dist(nidx, gdist + 1);
				// An OPENING is the legitimate way in (window, grate, doorway), so a
				// crossing through one is not "inside": only a roof the fan reached
				// with no opening at the crossing counts.
				// The fan's own doorway corridor (behind the apex) is the way it
				// came out, not a neighbour's interior: stripping it left the
				// doorway floor and its jamb faces dark.
				const bool own_corridor = (cone_dx != 0 || cone_dy != 0) && (nx - rt) * cone_dx + (ny - rt) * cone_dy < 0;
				insid[nidx] = (insid[static_cast<size_t>(gy) * side + gx] != 0
							   || (!own_corridor && !real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0
								   && opening(gx, gy) == 0))
									  ? 1
									  : 0;
				esc[nidx] = esc[static_cast<size_t>(gy) * side + gx] != 0 || opening(gx, gy) > 0
											|| (start_roofed && roofed(gx, gy) && !roofed(nx, ny))
									? 1
									: 0;
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
				// Same spill cone as the orthogonal steps -- ungated diagonal
				// chains leaked the fan sideways and behind the mouth.
				if (cone_dx != 0 || cone_dy != 0) {
					const int relx  = nx - rt;
					const int rely  = ny - rt;
					const int along = relx * cone_dx + rely * cone_dy;
					const int perp  = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
					if (along < 0 ? (along < -4 || perp > 1) : perp > along + 1) {
						continue;
					}
					if (along < 0 && opening(nx, ny) > 0) {
						continue;
					}
				}
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				if (tall(nx, ny)) {
					if (light_walls) {
						set_dist(nidx, gdist + 1);
						if (spills == nullptr) {
							lit[nidx] |= Grid::Flag;
						}
					}
					continue;
				}
				if (visited[nidx] || tall(gx + d[0], gy) || tall(gx, gy + d[1])) {
					continue;
				}
				visited[nidx] = 1;
				set_dist(nidx, gdist + 1);
				const bool own_corridor_d = (cone_dx != 0 || cone_dy != 0) && (nx - rt) * cone_dx + (ny - rt) * cone_dy < 0;
				insid[nidx] = (insid[static_cast<size_t>(gy) * side + gx] != 0
							   || (!own_corridor_d && !real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0
								   && opening(gx, gy) == 0))
									  ? 1
									  : 0;
				esc[nidx] = esc[static_cast<size_t>(gy) * side + gx] != 0 || opening(gx, gy) > 0
											|| (start_roofed && roofed(gx, gy) && !roofed(nx, ny))
									? 1
									: 0;
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
			{
			}
			// Thin candidates in COORDINATE order, not BFS arrival order: the
			// fill's order follows the light's position, and with it the
			// surviving apex set -- the fan's mouth wandered as a carried
			// torch moved.  The apex set must be pure opening geometry; only
			// each apex's radius/path may depend on the light.
			std::sort(spill_cand.begin(), spill_cand.end());
			std::sort(door_cand.begin(), door_cand.end());
			std::vector<Tile_coord> emitted_all;
			auto                    emit_spill = [&](const Tile_coord& t, int pct, int floor_storey, int path,
                                  const Tile_coord& opening = Tile_coord(-1, -1, -1), bool on_deck = false,
                                  int mouth_neg = 0, int mouth_pos = 0) {
                // Ground radius 1: survivors sit <= 2 tiles apart, so the
                // +/-1 cone mouths union into the full opening width.  Keyed on
                // the SOURCE's storey like the candidate thinning above: keying
                // it on the spill's own floor thinned a high window's mouth to a
                // single bubble, leaving one 45-degree cone -- a funnel instead
                // of the wide pool a ground window makes.
                const int dedupe_r = (lt.tz / 5) >= 1 ? 4 : 1;
                for (const Tile_coord& e : emitted_all) {
                    if (std::abs(t.tx - e.tx) <= dedupe_r && std::abs(t.ty - e.ty) <= dedupe_r && std::abs(t.tz - e.tz) <= 5) {
                        return;
                    }
                }
                emitted_all.push_back(t);
                spills->push_back({t, pct, floor_storey, path, opening, on_deck, mouth_neg, mouth_pos});
			};
			// Path distance the fill recorded at a grid tile (0 when unreached).
			auto path_at = [&](int gx, int gy) {
				const int v = Grid::Dist(lit[static_cast<size_t>(gy) * side + gx]);
				return v > 0 ? v - 1 : 0;
			};
			{
			}
			// One spill per OPENING: a contiguous run of exit tiles sharing an
			// outward step emits a single apex at its middle whose fan mouth
			// spans the run; runs near an already-emitted one are thinned.
			// ends(i) = {exit x, exit y, inner x, inner y}; emit_one returns
			// whether it emitted.
			const int open_dedupe_r  = (lt.tz / 5) >= 1 ? 4 : 1;
			auto      group_openings = [&](size_t count, auto&& ends, auto&& emit_one) {
				std::vector<char>                grouped(count, 0);
				std::vector<std::pair<int, int>> done;
				for (size_t ci = 0; ci < count; ++ci) {
					if (grouped[ci] != 0) {
						continue;
					}
					grouped[ci]                = 1;
					const std::array<int, 4> e = ends(ci);
					const int odx = e[0] - e[2];
					const int ody = e[1] - e[3];
					const int pdx = ody != 0 ? 1 : 0;
					const int pdy = odx != 0 ? 1 : 0;
					// Offset of candidate cj along this run, or -1 if off it.
					auto run_at = [&](size_t cj) {
						const std::array<int, 4> j = ends(cj);
						if (j[0] - j[2] != odx || j[1] - j[3] != ody) {
							return -1;
						}
						const int k = pdx != 0 ? j[0] - e[0] : j[1] - e[1];
						return k >= 0 && j[0] == e[0] + pdx * k && j[1] == e[1] + pdy * k ? k : -1;
					};
					int n = 1;
					for (bool found = true; found;) {
						found = false;
						for (size_t cj = ci + 1; cj < count; ++cj) {
							if (grouped[cj] != 0) {
								continue;
							}
							const int k = run_at(cj);
							if (k >= 0 && k < n) {
								grouped[cj] = 1;    // Same exit tile, already in the run.
							} else if (k == n) {
								grouped[cj] = 1;
								++n;
								found = true;
							}
						}
					}
					const int c   = (n - 1) / 2;
					size_t    mid = ci;
					for (size_t cj = ci; c > 0 && cj < count; ++cj) {
						if (run_at(cj) == c) {
							mid = cj;
							break;
						}
					}
					const std::array<int, 4> m         = ends(mid);
					bool                     near_prev = false;
					for (const auto& [ex, ey] : done) {
						if (std::abs(m[0] - ex) <= open_dedupe_r && std::abs(m[1] - ey) <= open_dedupe_r) {
							near_prev = true;
							break;
						}
					}
					if (!near_prev && emit_one(mid, c, n - 1 - c)) {
						done.emplace_back(m[0], m[1]);
					}
				}
			};
			auto emit_window = [&](size_t i, int mouth_neg, int mouth_pos) {
				const auto [ox, oy, pct, open_top, cpath, wx, wy] = spill_cand[i];
				// Emit only if the far-side tile is under OPEN SKY.  A window
				// the fill touched from its OUTSIDE face (after escaping through
				// a door and wrapping around the building) has the room interior
				// as its far side -- roofed -- and emitting it would spill the
				// glow back INSIDE.  (Testing `lit` instead is wrong: the fill
				// wrapping around outside also reaches a window's legitimate
				// outside tile, which must NOT cancel that window's spill.)
				const int fx = lt.tx + ox - rt;
				const int fy = lt.ty + oy - rt;
				// The spill's storey is that of the SURFACE it lands on, set
				// once sp_tz is decided below.  It used to be
				// max(lt.tz / 5, (open_top - 1) / 5), which tagged a pool on open
				// GROUND as storey 2 whenever the window was tall (top z 11) --
				// and a storey-2 spill reaches storey-1 and -2 marks, lighting
				// battlements and decks it never touches.  The deck case is
				// handled explicitly further down.
				int        floor   = lt.tz / 5;
				int        sp_tz   = 0;
				const bool covered = Light_tile_roofed(gmap, fx, fy);
				bool       on_deck  = false;
				bool       dbg_drop = false;
				if (covered) {
					// Covered -- but when the cover is only a floor-roof deck
					// the window overlooks (its top at or below the opening's
					// top, open sky above), the far side is OUTSIDE after all:
					// the glow lands ON the deck.  Place the spill at the deck's
					// storey floor so it renders at deck height and its storey
					// lets it light the deck's top surface.
					const int deck_top = Light_tile_overlooked_deck(gmap, fx, fy, open_top);
					if (deck_top < 0) {
						dbg_drop = true;
					} else {
						sp_tz   = (deck_top / 5) * 5;
						on_deck = true;
						// The glow lands ON the deck, so it belongs to the DECK's
						// storey.  (open_top - 1) / 5 is the window's own top and
						// runs a storey higher on a tall window, which let the
						// spill light surfaces a storey above the deck too.
						floor = deck_top / 5;
					}
				}
				{
				}
				if (dbg_drop) {
					return false;    // Far side is interior (or under another roof).
				}
				emit_spill(
						Tile_coord(Light_tile_norm(fx), Light_tile_norm(fy), sp_tz), pct, floor, cpath,
						Tile_coord(Light_tile_norm(lt.tx + wx - rt), Light_tile_norm(lt.ty + wy - rt), 0), on_deck, mouth_neg,
						mouth_pos);
				return true;
			};
			group_openings(
					spill_cand.size(),
					[&](size_t i) {
						const auto& s = spill_cand[i];
						return std::array<int, 4>{std::get<0>(s), std::get<1>(s), std::get<5>(s), std::get<6>(s)};
					},
					emit_window);
			// Doorway/roof-edge spills, including glass the fill flows through.
			auto emit_door = [&](size_t i, int door_mneg, int door_mpos) {
				const auto [ox, oy, wx, wy] = door_cand[i];
				// Carry the crossing's SOURCE tile as the spill's opening: the
				// fan axis becomes the doorway's outward NORMAL (the one-tile
				// roofed->open step).  The old light->exit fallback pointed a
				// wide doorway's off-axis exits sideways along the facade.
				const Tile_coord otile = Tile_coord(Light_tile_norm(lt.tx + wx - rt), Light_tile_norm(lt.ty + wy - rt), 0);
				const int        etx   = Light_tile_norm(lt.tx + ox - rt);
				const int        ety   = Light_tile_norm(lt.ty + oy - rt);
				// A doorway / roof-edge exit is open air: full transmission.
				// Anchor the bubble at the source room's own storey floor: an
				// upstairs door exits onto a walkway/deck at that level, and a
				// ground-anchored bubble would flood the wrong (ground) room
				// and slide its lattice off the deck.
				// NOT deck-anchored: raising the storey here let the spill past
				// the whole-unit gate and lit the walkway's upper wall courses.
				// Tall-hall windows reach the window path instead (door_cand=0).
				const int sp_tz    = floor_z;
				const int sp_floor = lt.tz / 5;
				{
				}
				emit_spill(Tile_coord(etx, ety, sp_tz), 100, sp_floor, path_at(ox, oy), otile, false, door_mneg, door_mpos);
				return true;
			};
			group_openings(
					door_cand.size(),
					[&](size_t i) {
						const auto& [x, y, wx, wy] = door_cand[i];
						return std::array<int, 4>{x, y, wx, wy};
					},
					emit_door);
			// Ceiling-well spills (stairwell / ladder openings): the light
			// climbs through the hole in its own ceiling and pools on the
			// floor of the storey ABOVE.  The continuation bubble sits at the
			// upper floor's z, so it renders at that height, its storey passes
			// the Tall + storey mask gates up there, and its own spill flood
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
		}
		// Straight-ray shadowing for SPILL grids (after emission -- the spill
		// budgets above read the path distances this pass clears).  A fan is
		// directional light, not a flow: without rays the fill curls around
		// porch stubs and building corners inside the cone.  ALL lit cells are
		// filtered against a sight line to the apex; walls that carry a
		// pass-through opening are transparent to the ray, so the fan still
		// enters neighbouring buildings through their windows, and doorways
		// (no wall at all) pass freely.  Main grids stay unfiltered: interior
		// room light legitimately bounces.
		{
			const bool spill_grid = spills == nullptr;
			auto       los_from   = [&](int ax, int ay, int gx, int gy) {
                const float dx    = static_cast<float>(gx - ax);
                const float dy    = static_cast<float>(gy - ay);
                const int   steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) * 4;
                for (int i = 1; i < steps; ++i) {
                    const float ft = static_cast<float>(i) / static_cast<float>(steps);
                    const int   cx = ax + static_cast<int>(std::lround(dx * ft));
                    const int   cy = ay + static_cast<int>(std::lround(dy * ft));
                    if ((cx == gx && cy == gy) || (cx == ax && cy == ay)) {
                        continue;    // Start / target cells never block themselves.
                    }
                    if (tall(cx, cy) && opening(cx, cy) == 0) {
                        return false;
                    }
                }
                return true;
			};
			// Rays leave from every tile of the opening's mouth, as the
			// per-tile apexes of a wide opening once did.
			const int mneg     = cone_mouth & 0xff;
			const int mpos     = (cone_mouth >> 8) & 0xff;
			auto      los_clear = [&](int gx, int gy) {
                for (int k = -mneg; k <= mpos; ++k) {
                    const int ax = rt + k * cone_dy;
                    const int ay = rt - k * cone_dx;
                    if (ax >= 0 && ay >= 0 && ax < side && ay < side && los_from(ax, ay, gx, gy)) {
                        return true;
                    }
                }
                return false;
			};
			if (spill_grid) {
				for (int gy = 0; gy < side; ++gy) {
					for (int gx = 0; gx < side; ++gx) {
						const size_t idx = static_cast<size_t>(gy) * side + gx;
						if (lit[idx] == 0 || (gx == rt && gy == rt)) {
							continue;
						}
						if (!los_clear(gx, gy)) {
							lit[idx] = 0;
						}
					}
				}
			}
		}
		// The fan's own interior strip: the mirror of the esc strip below,
		// which a spill grid can never reach.  A wall the fan merely stands
		// against keeps its face read (that lives in the ring, untouched);
		// what goes is the fill the fan carried INSIDE a neighbouring
		// building, which an exterior lamp never paints because it renders
		// from outside without a room grid at all.
		if (spills == nullptr) {
			for (size_t i = 0; i < lit.size(); ++i) {
				if (insid[i] != 0) {
					lit[i] &= Grid::Flag;
				}
			}
		}
		// The MAIN fill's ESCAPED cells (through openings, wrapped outside)
		// exist for spill detection and the ring's arrival data only: drop
		// them from the PAINTED field, or the room light shows outdoors as a
		// free bubble "as if there were no walls" -- outdoor light is the
		// coned SPILL grids' job.  EXCEPTION: keep exactly the escaped cells
		// an INTERIOR pixel samples.  The field lattice is anchored at
		// wall-top, 4px/z up-left of ground, so a pixel over interior tile T
		// reads the cell T + (s..s+1, s..s+1) diagonally down-right
		// (s = roof_z/2 tiles).  Near south/east walls and at a doorway
		// mouth those cells are OUTDOORS; stripping them grayed the strip
		// (the blob).  A kept cell's own display position is back over the
		// interior, so no outdoor glow can result.  INTERIOR grids only: an
		// exterior light's esc is all-ones by construction and stripping it
		// blanks the whole pool.  Wall cells never carry esc and keep their
		// ring values; the Grid::Flag shaft flags survive.
		if (spills != nullptr && start_roofed) {
			const int olo = roof_z > 0 ? roof_z / 2 : 2;
			const int ohi = olo + 1;
			for (size_t i = 0; i < lit.size(); ++i) {
				if (esc[i] == 0) {
					continue;
				}
				const int x      = static_cast<int>(i % static_cast<size_t>(side));
				const int y      = static_cast<int>(i / static_cast<size_t>(side));
				bool      serves = false;
				for (int o = olo; o <= ohi && !serves; ++o) {
					const int bx = x - o;
					const int by = y - o;
					serves       = bx >= 0 && by >= 0 && !tall(bx, by) && esc[static_cast<size_t>(by) * side + bx] == 0
							 && lit[static_cast<size_t>(by) * side + bx] != 0;
				}
				if (!serves) {
					lit[i] &= Grid::Flag;
				} else if (ring != nullptr) {
					// Kept for the anchor-shifted SAMPLE only -- no light reaches
					// the ground here, so flag it for consumers that read a cell
					// by geometry.
					Ring::Set(ring->data(), i, Ring::Apron_flag);
				}
			}
			// A WALL cell the fill touched only AFTER escaping (Escaped_flag set,
			// Inside_flag clear -- a neighbour's facade, or this building's own
			// outside) is never visited, so the loop above skips it and its
			// painted value survives the strip while the ground around it is
			// gone.  A veto light's field has no per-side wall values, so the
			// bilinear ramps from that lone lit cell into the dark cells beside
			// it: a 6px gradient on the ground on the FAR side of the
			// neighbour's wall, with the shutters shut.  Only the painted value
			// goes; the ring arrivals stay for the face and top reads.
			if (ring != nullptr) {
				for (size_t i = 0; i < lit.size(); ++i) {
					const unsigned char* rc = ring->data();
					if (Ring::Is_foreign_wall(rc, i)) {
						lit[i] &= Grid::Flag;
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
			std::vector<unsigned char> ring;
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

	// Replenish the flood cache's per-frame refresh budget (see the cache above).
	void Flood_cache_frame_begin() {
		flood_spend = 0;
	}

	// Mixed into the light-layer signatures so cached coverage rebuilds exactly
	// when flood content changes (a door opened or closed).
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

	// A world object was edited (frame change, shape swap, placed, removed):
	// invalidate the light caches around it when it can shape light floods.
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
			Game_object* light_obj, int rt, std::vector<unsigned char>& lit, std::vector<Light_spill>& spills, bool light_walls,
			std::vector<unsigned char>* ring) {
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
			if (ring != nullptr) {
				*ring = hit->ring;
			}
			return;
		}
		const Flood_spend_scope    spend;
		const int                  roof_z = Light_room_roof_z(gmap, lt.tx, lt.ty, lt.tz);
		std::vector<unsigned char> ringv;
		Flood_room_grid(gmap, lt, rt, roof_z, lit, &spills, light_walls, &ringv, 0, 0, Light_beneath_roof(light_obj));
		Flood_cache_entry& entry = flood_cache[key];
		const bool         had   = entry.stamp != 0;
		if (had && entry.lit == lit && entry.ring == ringv && Spills_equal(entry.spills, spills)) {
			entry.ttl = std::min<Uint64>(entry.ttl * 2, flood_cache_ttl_max);
		} else {
			entry.ttl = flood_cache_ttl;
			if (had) {
				++flood_content_gen;
			}
			entry.lit    = lit;
			entry.ring   = ringv;
			entry.spills = spills;
		}
		if (ring != nullptr) {
			*ring = std::move(ringv);
		}
		entry.stamp = now;
		entry.used  = now;
	}

	// Room-fill grid for a spill glow, flooded from `start` -- the tile just
	// outside the opening the light escapes through.  `light_walls` lights the
	// facade ring the glow falls on (viewer-outside look; from inside the ring
	// shows through the walls' interior faces).
	void Build_spill_shadow_grid(
			const Tile_coord& start, int rt, std::vector<unsigned char>& lit, bool light_walls, std::vector<unsigned char>* ring,
			int cone_dx, int cone_dy, int cone_mouth) {
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
		const Flood_cache_key key{
				start.tx, start.ty, start.tz, rt,
				(light_walls ? 0 : 3) | ((cone_dx + 1) << 2) | ((cone_dy + 1) << 4) | (cone_mouth << 6)};
		if (Flood_cache_entry* hit = Flood_cache_find(key, now)) {
			lit = hit->lit;
			if (ring != nullptr) {
				*ring = hit->ring;
			}
			return;
		}
		const Flood_spend_scope    spend;
		const int                  roof_z = Light_room_roof_z(gmap, start.tx, start.ty, start.tz);
		std::vector<unsigned char> ringv;
		Flood_room_grid(gmap, start, rt, roof_z, lit, nullptr, light_walls, &ringv, cone_dx, cone_dy, false, cone_mouth);
		Flood_cache_entry& entry = flood_cache[key];
		const bool         had   = entry.stamp != 0;
		if (had && entry.lit == lit && entry.ring == ringv) {
			entry.ttl = std::min<Uint64>(entry.ttl * 2, flood_cache_ttl_max);
		} else {
			entry.ttl = flood_cache_ttl;
			if (had) {
				++flood_content_gen;
			}
			entry.lit  = lit;
			entry.ring = ringv;
		}
		if (ring != nullptr) {
			*ring = std::move(ringv);
		}
		entry.spills.clear();
		entry.stamp = now;
		entry.used  = now;
	}

	// Ground light at one world TILE, read straight from the fill: the same
	// dome the splat paints on clear ground there, without the screen mask.
	// The relight pass needs it because a sprite's own pixels may all be
	// masked -- an actor under a drawn roof samples nothing but 255 -- while
	// the tile he stands on is squarely in the fan.  Apron cells (kept in the
	// grid only so an interior pixel's anchor-shifted sample finds light) do
	// not count: nothing standing there is really lit.
	int Light_tile_alpha(
			const unsigned char* grid, int grid_rt, const unsigned char* ring, int ltx, int lty, int tx, int ty, int radius,
			int elevation, int dist_bias, int intensity_pct) {
		if (radius <= 0 || intensity_pct <= 0) {
			return 0;
		}
		auto wrap_delta = [](int a, int b) {
			int d = Light_tile_norm(a) - Light_tile_norm(b);
			if (d > c_num_tiles / 2) {
				d -= c_num_tiles;
			} else if (d < -c_num_tiles / 2) {
				d += c_num_tiles;
			}
			return d;
		};
		const int   du     = wrap_delta(tx, ltx);
		const int   dv     = wrap_delta(ty, lty);
		const float euclid = std::sqrt(static_cast<float>(du * du + dv * dv)) * static_cast<float>(c_tilesize);
		float       travel = euclid;
		if (grid != nullptr) {
			const int side = 2 * grid_rt + 1;
			const int cu   = du + grid_rt;
			const int cv   = dv + grid_rt;
			if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
				return 0;
			}
			const size_t cidx = static_cast<size_t>(cv) * side + cu;
			const int    d    = Grid::Dist(grid[cidx]);
			if (d == 0) {
				return 0;    // The fill never reached this tile.
			}
			if (ring != nullptr && Ring::Is_apron(ring, cidx)) {
				return 0;    // Apron cell: sampling-only light.
			}
			const float path = static_cast<float>((d - 1) * c_tilesize);
			if (path > travel) {
				travel = path;
			}
		}
		const float inten = intensity_pct >= 100 ? 1.0f : static_cast<float>(intensity_pct) / 100.0f;
		const float e     = static_cast<float>(elevation > 0 ? elevation : 0);
		const float bias  = static_cast<float>(dist_bias > 0 ? dist_bias : 0);
		const float full  = static_cast<float>(radius) + bias;
		const float rf2   = full * full + e * e;
		const float tot   = travel + bias;
		const float dome  = 1.0f - (tot * tot + e * e) / rf2;
		if (dome <= 0.0f) {
			return 0;
		}
		const int a = static_cast<int>(255.0f * inten * dome + 0.5f);
		return a > 255 ? 255 : a;
	}

	// Splat one radial light: a soft dome falloff written into `cov` (W*H,
	// stride W), copying the brightened source pixel wherever this light is the
	// strongest contributor so far.  `elevation` (game px) rounds the dome;
	// `dist_bias` continues another source's falloff (spill glows);
	// `intensity_pct` (1..100) scales the whole dome.  `roofpix` is the
	// roof light mask (see Mask): `veto_roof` keeps marked pixels dark under an interior
	// light, `is_spill` gates by `spill_floor` / `light_top_storey`.  A non-null
	// `grid` renders the light as a propagated field pinned to the tile lattice
	// at (`grid_fx`,`grid_fy`) instead of a free dome.  `kindpix`/`footdx`/
	// `footdy` are the surface-identity channels and `ring` the per-wall-cell
	// face arrivals; (`av_fx`,`av_fy`) is the viewer's z=0 foot for the
	// viewer-side face rule.  A clip window (clip_x1/y1 >= 0) restricts the
	// pixels written (scroll-vacated strip patching).
	void Reach_extend_box(int& x0, int& y0, int x1, int y1, const Sprite_box* sprites, int nsprites) {
		if (sprites == nullptr) {
			return;
		}
		const int px0 = x0;
		const int py0 = y0;
		for (int i = 0; i < nsprites; ++i) {
			const Sprite_box& s = sprites[i];
			if (s.fx < px0 || s.fx > x1 || s.fy < py0 || s.fy > y1) {
				continue;    // Not rooted in this pool.
			}
			if (s.x0 >= px0 && s.y0 >= py0) {
				continue;    // Already covered.
			}
			x0 = std::min(x0, static_cast<int>(s.x0));
			y0 = std::min(y0, static_cast<int>(s.y0));
		}
	}

	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer, int clip_x0, int clip_y0,
			int clip_x1, int clip_y1, const unsigned char* kindpix, int kind_lw, const unsigned char* footdx,
			const unsigned char* footdy, int foot_lw, const unsigned char* ring, int av_fx, int av_fy,
			const Sprite_box* sprites, int nsprites, bool spill_on_deck, int cone_dx, int cone_dy, int cone_mouth) {
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
		// FAN PENUMBRA.  Build_spill_shadow_grid clips the fill to a 45-degree
		// cone in front of the opening, and that clip is binary: a cell is in
		// the fan at full brightness or out of it at nothing.  An aperture
		// casts a penumbra, so the last few tiles before the cone's edge fade
		// out instead.  Without it the edge is a straight line drawn across
		// whatever it crosses -- a tree standing on the boundary had its crown
		// cut in half, one tile row lit flat and the next black.
		const bool    coned     = grid != nullptr && (cone_dx != 0 || cone_dy != 0);
		constexpr int penumbra  = 3;    // Max tiles of taper at the cone's edge.
		auto          cone_fade = [&](int gx, int gy) -> float {
            if (!coned) {
                return 1.0f;
            }
            const int relx  = gx - grid_rt;
            const int rely  = gy - grid_rt;
            const int along = relx * cone_dx + rely * cone_dy;
            const int perp  = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
            // The band scales with the fan's half-width (as the spill face
            // taper does), so the fade only ever grows along the axis: a fixed
            // band switched on all at once and left a dark pocket at the
            // fan's flank just past the mouth.  The corridor BEHIND the apex
            // (along < 0) is the doorway strip, not an edge.
            if (along < 0) {
                return 1.0f;
            }
            const float half_w = 0.5f * static_cast<float>(along + 1);
            const float band   = half_w < 1.0f ? 1.0f : (half_w > penumbra ? static_cast<float>(penumbra) : half_w);
            const float f      = static_cast<float>(along + 2 - perp) / (band + 1.0f);
            return f >= 1.0f ? 1.0f : (f <= 0.0f ? 1.0f / (band + 1.0f) : f);
		};
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
		const int   side     = grid != nullptr ? 2 * grid_rt + 1 : 0;
		const float cell     = static_cast<float>(c_tilesize);
		const float inv_cell = 1.0f / cell;
		const float half     = 0.5f * (cell - 1.0f);
		const float grid_u   = static_cast<float>(grid_fx) - half - static_cast<float>(grid_rt) * cell;
		const float grid_v   = static_cast<float>(grid_fy) - half - static_cast<float>(grid_rt) * cell;
		// Splat centre in lattice coordinates + the path rebase (shared by the
		// field template build and the face consumer below).  The wall-top
		// anchor pins the lattice up-and-left of the tiles' floor positions
		// (4px per z of wall height), so the grid CENTRE renders up-left of the
		// light itself.  The brightness peak, however, belongs at the light's
		// true screen position: measure the per-tile straight-line distance
		// from the SPLAT CENTRE expressed in lattice coordinates, not from the
		// grid centre -- otherwise the whole pool sits diagonally up-left of
		// the source by the anchor shift.  The flood path is 0 at the light's
		// TILE (the grid centre), which again renders up-left of the true
		// centre: rebase the path so it is zero at the cell under the splat
		// centre, capped at that cell's Chebyshev lattice distance (when it is
		// only reachable the long way round -- a torch AT a wall -- rebasing by
		// the wrap path would pool full light outside a closed door).
		float uc      = 0.0f;
		float vc      = 0.0f;
		float base_px = 0.0f;
		if (grid != nullptr) {
			uc                        = (static_cast<float>(sx) - grid_u) / cell;
			vc                        = (static_cast<float>(sy) - grid_v) / cell;
			int cgx                   = static_cast<int>(std::lround(uc));
			int cgy                   = static_cast<int>(std::lround(vc));
			cgx                       = std::min(std::max(cgx, 0), side - 1);
			cgy                       = std::min(std::max(cgy, 0), side - 1);
			const int mbase           = Grid::Dist(grid[static_cast<size_t>(cgy) * side + cgx]);
			base_px                   = mbase > 0 ? static_cast<float>((mbase - 1) * c_tilesize) : 0.0f;
			const int cheb            = std::max(std::abs(cgx - grid_rt), std::abs(cgy - grid_rt));
			if (const float cap_px = static_cast<float>(cheb * c_tilesize); base_px > cap_px) {
				base_px = cap_px;
			}
		}
		std::vector<float>         field_local;
		std::vector<unsigned char> field_wall;
		std::vector<float>         field_side;
		const Field_template*      ftmpl = nullptr;
		// VETO (interior) lights keep
		// the classic field/ring rendering wholesale -- always correct there;
		// the consumers exist for exterior lights and spills.
		const bool face_data = kindpix != nullptr && footdx != nullptr && footdy != nullptr && foot_lw > 0;
		const bool face_grid = face_data && grid != nullptr && ring != nullptr && !veto_roof;
		// The lattice is anchored at the wall-top level (grid_fx/fy = the
		// light tile's foot at anchor_z, 4px per z up-left of ground) while
		// stamped feet are at z=0: shift feet by the same amount or a veto
		// light's lookups land cells down-right of the wall's ring cell.
		const int foot_shift = 4 * (anchor_z > 0 ? anchor_z : 0);
		// The viewer's lattice cell, for the field template's wall cells (see
		// ring_face_dist_gated below).
		int av_cu = grid_rt;
		int av_cv = grid_rt;
		if (face_grid) {
			av_cu = static_cast<int>(std::lround((static_cast<float>(av_fx - foot_shift) - grid_u) * inv_cell));
			av_cv = static_cast<int>(std::lround((static_cast<float>(av_fy - foot_shift) - grid_v) * inv_cell));
		}
		// Face arrival for a wall cell (dist+1, 0 = unlit).  ALL four sides
		// count: a wall sprite is the same art from either side, and an
		// unsealed building's shell must show the exterior illumination from
		// inside exactly as from outside (streetlamp SW: the W/N arrivals
		// carry the facade).  Consumed by the shell-face pixel paths, where the
		// mask already certified the pixel as a shell face.
		auto ring_face_dist = [&](int cu, int cv) -> int {
			const unsigned char* rc  = ring + (static_cast<size_t>(cv) * side + cu) * Ring::Bytes;
			int                  d   = 0;
			auto                 acc = [&d](int v) {
                if (v != 0 && (d == 0 || v < d)) {
                    d = v;
                }
			};
			acc(Ring::Dist(rc[Ring::S]));
			acc(Ring::Dist(rc[Ring::E]));
			acc(Ring::Dist(rc[Ring::N]));
			acc(Ring::Dist(rc[Ring::W]));
			return d;
		};
		// Viewer-gated variant for the FIELD TEMPLATE's wall cells: those are
		// sampled z-blind by CLEAR pixels -- interior-classified wall faces
		// among them (the barn's west wall seen from inside).  An N/W arrival
		// there counts only when the viewer stands on that side, else the
		// lamp behind the wall bleeds up its interior face.  S/E stay
		// unconditional: the camera faces them, and gating all four made the
		// accepted side FLIP mid-wall as the avatar walked, punching dark
		// blobs wherever the arrival existed on only one of the two.
		// Also reports WHICH sides produced that minimum.  The field stores one
		// scalar per cell, so it may only travel toward the side(s) that actually
		// achieved it: a wall 5 tiles from the streetlamp outside and 37 from the
		// light that crept in a far window must not export the street's
		// brightness inward under cover of its own dim inside arrival.
		auto ring_face_gated = [&](int cu, int cv, int* out_bits) -> int {
			const unsigned char* rc = ring + (static_cast<size_t>(cv) * side + cu) * Ring::Bytes;
			int                  v[4];
			v[Ring::S] = Ring::Dist(rc[Ring::S]);
			v[Ring::E] = Ring::Dist(rc[Ring::E]);
			v[Ring::N] = av_cv < cv ? Ring::Dist(rc[Ring::N]) : 0;    // Viewer north.
			v[Ring::W] = av_cu < cu ? Ring::Dist(rc[Ring::W]) : 0;    // Viewer west.
			int d = 0;
			for (int s = 0; s < 4; ++s) {
				if (v[s] != 0 && (d == 0 || v[s] < d)) {
					d = v[s];
				}
			}
			if (out_bits != nullptr) {
				int b = 0;
				for (int s = 0; s < 4; ++s) {
					if (v[s] != 0 && v[s] == d) {
						b |= 1 << s;
					}
				}
				*out_bits = b;
			}
			return d;
		};
		auto ring_face_dist_gated = [&](int cu, int cv) -> int {
			return ring_face_gated(cu, cv, nullptr);
		};
		// A wall TOP is one continuous horizontal surface, so a cell the fill
		// reached only the long way round must not sit black beside one the
		// light enters directly (a lamp through a doorway lit cols 50-51 at
		// distance 10 while the next cells along the same wall read 23).
		// Chamfer the arrivals ALONG connected wall cells, a tile per step:
		// relaxing can only ever SHORTEN a distance, so the ramp ends by
		// itself where it meets the cell's own arrival -- no reach constant,
		// and a far side stays dark because its own value already wins.
		// FACES get the same treatment on their viewer-gated arrivals: each
		// wall sprite paints its own tile's value flat across its whole art,
		// so a real gradient rendered as a hard seam where two sprites meet
		// (the 45-degree shear line down an inside wall face beside an open
		// door: one tile at 9 against its screen neighbour at 14).  There a
		// gated 0 means "dark side", which must stay dark -- the chamfer only
		// smooths between cells that are ALREADY lit.
		constexpr unsigned char wall_unreached = 255;
		std::vector<unsigned char> top_dist;
		std::vector<unsigned char> gate_dist;
		auto                       wall_cell = [&](int u_, int v_) {
            return Ring::Is_wall(ring, static_cast<size_t>(v_) * side + u_);
		};
		auto build_chamfer = [&](std::vector<unsigned char>& out, bool gated) {
			out.assign(static_cast<size_t>(side) * side, 0);
			for (int v = 0; v < side; ++v) {
				for (int u = 0; u < side; ++u) {
					if (!wall_cell(u, v)) {
						continue;    // 0 = not a wall cell; never relaxed through.
					}
					const int d = gated ? ring_face_dist_gated(u, v) : ring_face_dist(u, v);
					if (d != 0) {
						out[static_cast<size_t>(v) * side + u] = static_cast<unsigned char>(d);
					} else if (!gated) {
						out[static_cast<size_t>(v) * side + u] = wall_unreached;
					}
				}
			}
			for (int pass = 0; pass < 4; ++pass) {
				bool changed = false;
				auto relax   = [&](int u_, int v_, int nu_, int nv_) {
                    if (nu_ < 0 || nv_ < 0 || nu_ >= side || nv_ >= side) {
                        return;
                    }
                    const unsigned char nb = out[static_cast<size_t>(nv_) * side + nu_];
                    if (nb == 0 || nb == wall_unreached) {
                        return;
                    }
                    unsigned char& cur = out[static_cast<size_t>(v_) * side + u_];
                    if (nb + 1 < cur) {
                        cur     = static_cast<unsigned char>(nb + 1);
                        changed = true;
                    }
				};
				for (int v = 0; v < side; ++v) {
					for (int u = 0; u < side; ++u) {
						if (out[static_cast<size_t>(v) * side + u] == 0) {
							continue;
						}
						relax(u, v, u - 1, v);
						relax(u, v, u, v - 1);
					}
				}
				for (int v = side - 1; v >= 0; --v) {
					for (int u = side - 1; u >= 0; --u) {
						if (out[static_cast<size_t>(v) * side + u] == 0) {
							continue;
						}
						relax(u, v, u + 1, v);
						relax(u, v, u, v + 1);
					}
				}
				if (!changed) {
					break;
				}
			}
		};
		// An interior-facing wall face (Face_south / Face_east) is lit by the ROOM it
		// looks onto, so it takes that neighbouring cell's arrival -- the wall
		// cell's own side byte records when the fill crept around to touch it,
		// which ran 2 tiles longer than the floor beside it and, near the dome
		// edge, rendered the face 70 levels darker than the lit floor it meets.
		// Unreached room = dark, with no fallback to the cell's own side byte:
		// that byte is side-blind and carries the arrival from OUTSIDE, which
		// lit every interior face of a sealed room.
		// `found_room` reports whether a room was located at all: the return
		// value cannot say, since a room that IS found but dark also reads 0.
		auto ring_side_dist = [&](int cu, int cv, int sidx, bool* found_room = nullptr) -> int {
			// Step outward until the first OPEN cell: one step lands on wall
			// again both where a junction piece has a wall between it and the
			// room, and where a face looks along its own run, so a single step
			// cannot tell those apart.  The open cell it finds does: a real
			// room carries fill, a sealed one is apron.
			// Returns -1 for "no open cell within reach" (walled in / off the
			// grid), which is the only case with no answer of its own.  A room
			// that IS found and happens to be dark returns 0, and that 0 is
			// final -- treating it as "no answer" let a dark room's face light
			// itself from the far side and lit whole walls of a shut building.
			auto probe_side = [&](int sd) -> int {
				int nu = cu;
				int nv = cv;
				for (int step = 0; step < 4; ++step) {
					nu += sd == 1 ? 1 : 0;
					nv += sd == 0 ? 1 : 0;
					if (nu < 0 || nv < 0 || nu >= side || nv >= side) {
						return -1;
					}
					const size_t ni = static_cast<size_t>(nv) * side + nu;
					if (Ring::Is_wall(ring, ni)) {
						continue;    // Solid: keep crossing it.
					}
					if (Ring::Is_apron(ring, ni)) {
						// An APRON cell carries escaped light that nothing there
						// really receives (Flood_room_grid keeps it only for
						// anchor-shifted sampling); a sealed room's cells are apron.
						return 0;
					}
					return Grid::Dist(grid[ni]);
				}
				return -1;
			};
			const int d = probe_side(sidx);
			if (d >= 0) {
				if (found_room != nullptr) {
					*found_room = true;
				}
				return d;
			}
			// A corner piece walled in on its own side is still lit by the room
			// its wall borders: try the OTHER camera-facing side.  Never N/W --
			// the viewer cannot see those, and reading them is what lights a
			// sealed room from the lit one behind it.
			const int o = probe_side(sidx == 0 ? 1 : 0);
			if (found_room != nullptr) {
				*found_room = o >= 0;
			}
			return o > 0 ? o : 0;
		};
		// A wall cell with a corner post between it and the room reads 0 while
		// the very next cell along the SAME wall reads the room: a hard dark
		// seam exactly where two sprites of one wall meet.  Borrow one step
		// along the face's own run (an S face lies on an E/W line, an E face on
		// an N/S one), a tile of falloff.  Consulted ONLY when the cell's own
		// answer is 0, so no face that already had an arrival can change; and
		// one step only, where build_chamfer's unbounded relax would run a lit
		// room's arrival down the whole wall into a dark one.
		auto side_face_dist = [&](int cu, int cv, int sidx) -> int {
			bool      found = false;
			const int d     = ring_side_dist(cu, cv, sidx, &found);
			if (d > 0) {
				return d;
			}
			if (found) {
				// The room IS there and it is dark: final.  Borrowing here took a
				// lit room from the next sprite along the run and painted this
				// face with it.  A wall cell always carries a ring arrival, so
				// the grid guard below never catches that case.
				return 0;
			}
			if (Grid::Dist(grid[static_cast<size_t>(cv) * side + cu]) == 0) {
				// The fill never touched this cell, so it is inside a sealed
				// space and any arrival along the wall belongs to a space it is
				// not part of: borrowing there lit a shut building's inside
				// faces with the streetlamp on the far side of the wall.
				return 0;
			}
			int  best = 0;
			auto take = [&](int nu, int nv) {
				if (nu < 0 || nv < 0 || nu >= side || nv >= side || !wall_cell(nu, nv)) {
					return;
				}
				const int n = ring_side_dist(nu, nv, sidx);
				if (n > 0 && (best == 0 || n < best)) {
					best = n;
				}
			};
			if (sidx == 0) {
				take(cu - 1, cv);
				take(cu + 1, cv);
			} else {
				take(cu, cv - 1);
				take(cu, cv + 1);
			}
			return best > 0 ? best + 1 : 0;
		};
		auto top_face_dist = [&](int cu, int cv) -> int {
            if (top_dist.empty()) {
                build_chamfer(top_dist, false);
            }
            const unsigned char d = top_dist[static_cast<size_t>(cv) * side + cu];
            return (d == 0 || d == wall_unreached) ? 0 : d;
		};
		auto gated_face_dist = [&](int cu, int cv) -> int {
			if (gate_dist.empty()) {
				build_chamfer(gate_dist, true);
			}
			return gate_dist[static_cast<size_t>(cv) * side + cu];
		};
		if (grid != nullptr) {
			// Reuse a recently computed field: FNV-1a over the grid content
			// (the flood cache above already hands back the identical grid for
			// a resting light) plus the dome parameters.
			uint64_t     ghash = 1469598103934665603ULL;
			const size_t cells = static_cast<size_t>(side) * side;
			for (size_t i = 0; i < cells; ++i) {
				ghash = (ghash ^ grid[i]) * 1099511628211ULL;
			}
			if (face_grid) {
				// Wall cells render differently with rings: never share a
				// cached template between the two variants or across viewer
				// cells (the gated wall values depend on the avatar's side).
				for (size_t i = 0; i < cells * Ring::Bytes; ++i) {
					ghash = (ghash ^ ring[i]) * 1099511628211ULL;
				}
				ghash = (ghash ^ static_cast<uint64_t>(av_cu * 131 + av_cv + 65536)) * 1099511628211ULL;
			}
			// The fan axis is not in the grid: two spills with identical fills
			// but different axes taper different edges.
			ghash = (ghash ^ static_cast<uint64_t>((cone_dx + 1) * 8 + cone_dy + 1 + cone_mouth * 64)) * 1099511628211ULL;
			const Field_key fkey{ghash, radius, elevation, dist_bias, intensity_pct, grid_rt, sx - grid_fx, sy - grid_fy};
			const Uint64    fnow = SDL_GetTicks();
			// carries position (sx - grid_fx), so a stale hit would draw a pool
			// at an old alignment.
			ftmpl                            = field_cache.find(fkey, fnow);
			if (ftmpl == nullptr) {
				field_local.assign(static_cast<size_t>(side) * side, 0.0f);
				// A wall cell is lit differently on each of its four sides, and
				// one scalar plus a direction cannot say that: taking any lit
				// side exported the street's brightness into the room behind,
				// taking only the nearest side blocked the other three and
				// punched a dark V into lit ground around the wall.  Keep the
				// dome PER SIDE and let the pixel's position choose.
				if (face_grid) {
					field_wall.assign(static_cast<size_t>(side) * side, 0);
					field_side.assign(static_cast<size_t>(side) * side * Ring::Bytes, 0.0f);
				} else {
					field_wall.clear();
					field_side.clear();
				}
				auto dome_for = [&](int gx, int gy, int m) -> float {
					if (m == 0) {
						return 0.0f;
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
					return dome > 0.0f ? 255.0f * dome * inten * cone_fade(gx, gy) : 0.0f;
				};
				for (int gy = 0; gy < side; ++gy) {
					for (int gx = 0; gx < side; ++gx) {
						const size_t  gidx = static_cast<size_t>(gy) * side + gx;
						unsigned char m    = static_cast<unsigned char>(Grid::Dist(grid[gidx]));
						if (face_grid && Ring::Is_wall(ring, gidx)) {
							// A wall cell's screen area shows its viewer-side
							// face: use that arrival, not the any-direction ring
							// -- no bilinear sliver past a dark wall's base, while
							// ground before a lit facade keeps its light.
							int bits = 0;
							m        = static_cast<unsigned char>(ring_face_gated(gx, gy, &bits));
							// UNGATED per side: the viewer gate is a proxy for
							// "which side is this face lit from", which the split
							// by side now answers directly.  Gated here it forced
							// N and W to 0 for an eastern viewer, so ground west
							// of any wall blended toward nothing -- a dark V.
							const unsigned char* rc = ring + gidx * Ring::Bytes;
							for (int s = Ring::S; s <= Ring::W; ++s) {
								field_side[gidx * Ring::Bytes + s] = dome_for(gx, gy, Ring::Dist(rc[s]));
							}
							field_wall[gidx] = 1;
						}
						field_local[gidx] = dome_for(gx, gy, m);
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
					const unsigned char* wrow0 = field_wall.empty() ? nullptr : field_wall.data() + static_cast<size_t>(h0) * side;
					unsigned char*       out   = t.alpha.data() + static_cast<size_t>(py) * t.w;
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
						const float  tu = u - static_cast<float>(g0);
						const float* r  = row0 + g0;
						float        c00 = r[0], c10 = r[1], c01 = r[side], c11 = r[side + 1];
						if (wrow0 != nullptr) {
							// Take each wall corner's arrival for the side the pixel
							// actually lies on -- brightest of the two when it sits
							// diagonally.  A side with no arrival gives nothing, which
							// is what keeps a sunlit facade out of the room behind it,
							// while a wall lit on several sides still lights the ground
							// all around instead of leaving a dark V in it.
							const size_t r0     = static_cast<size_t>(h0) * side + g0;
							auto         corner = [&](size_t ci, float base, float du, float dv) -> float {
                                if (field_wall[ci] == 0) {
                                    return base;    // Not a wall: an ordinary fill value.
                                }
                                const float* sv   = field_side.data() + ci * 4;
                                float        best = 0.0f;
                                if (dv >= 0.0f && sv[0] > best) {
                                    best = sv[0];    // S
                                }
                                if (du >= 0.0f && sv[1] > best) {
                                    best = sv[1];    // E
                                }
                                if (dv <= 0.0f && sv[2] > best) {
                                    best = sv[2];    // N
                                }
                                if (du <= 0.0f && sv[3] > best) {
                                    best = sv[3];    // W
                                }
                                return best;
							};
							c00 = corner(r0, c00, tu, tv);
							c10 = corner(r0 + 1, c10, tu - 1.0f, tv);
							c01 = corner(r0 + side, c01, tu, tv - 1.0f);
							c11 = corner(r0 + side + 1, c11, tu - 1.0f, tv - 1.0f);
						}
						// The tile just out of a S/E opening must not ramp back to the dark
						// opening row behind it.
						if (cone_dy > 0 && h0 == grid_rt - 1 && tv > 0.0f) {
							c00 = c01;
							c10 = c11;
						}
						if (cone_dx > 0 && g0 == grid_rt - 1 && tu > 0.0f) {
							c00 = c10;
							c01 = c11;
						}
						const float top = c00 + (c10 - c00) * tu;
						const float bot = c01 + (c11 - c01) * tu;
						const int   a   = static_cast<int>(top + (bot - top) * tv + 0.5f);
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
		// Grid::Flag in the grid) were used here for a shaft-band exemption; removed
		// to restore the pre-floor-roofs clear-ground rendering (the band's
		// hard edges read as artifacts on the storey below).
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
		// a97a278af widened this bbox up-left by a flat 128 so a tree crown
		// drawn far above its lit foot cell would be visited.  Any band, at any
		// width, also sweeps in unrelated sprites and lights them, which is the
		// art copied across the screen as the view scrolls -- and the per-pixel
		// foot channel cannot reject them, because the shear clamps it.  So the
		// reach is taken from the SPRITES themselves: only boxes whose paint
		// origin falls inside the pool are added, and only their own pixels may
		// light from the added area.
		int pool_x0 = x0;
		int pool_y0 = y0;
		Reach_extend_box(x0, y0, x1, y1, face_data ? sprites : nullptr, nsprites);
		const int nreach = (x0 < pool_x0 || y0 < pool_y0) ? 1 : 0;
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
		// The pool edge must survive the same clamps as the loop bounds, or
		// `x < pool_x0` can never be true and the reach gate is a silent no-op.
		pool_x0 = std::max(pool_x0, x0);
		pool_y0 = std::max(pool_y0, y0);
		// Hot-loop constants for the free-dome path.
		const float amp     = 255.0f * inten;
		const float rr      = static_cast<float>(reach) * static_cast<float>(reach);
		const float dome_dn = rr + bias * bias + e2;
		// Cells ARE tiles and the lattice point is a tile's FOOT (grid_fx/fy is
		// the light tile's foot), so a tile spans (point-8, point]: the cell
		// CONTAINING a position is ceil, not nearest.  While the stamped foot
		// was snapped to a tile centre every pixel of a tile sat on the same
		// half-integer and lround picked one side consistently; unsnapped, the
		// tile's range straddles lround's boundary and the cell flips MID-TILE.
		// Through a binary gate like object_reached that is a hard edge across
		// a sprite -- a tree lit in half, a triangle on a wall top.
		auto pos_cell = [](float p) {
			return static_cast<int>(std::ceil(p - 1e-3f));
		};
		// Offset from that cell's CENTRE, for the blend: with ceil semantics the
		// centre of cell k sits at k - 0.5.
		auto pos_off = [](float p, int k) {
			return p - (static_cast<float>(k) - 0.5f);
		};
		// Map a pixel's stamped foot to its owner cell; -1 = out of grid.
		auto foot_cell = [&](int px, int py) -> int {
			const size_t fi = static_cast<size_t>(py) * foot_lw + px;
			const int    fx = px + static_cast<int>(footdx[fi]) - Foot_bias - foot_shift;
			const int    fy = py + static_cast<int>(footdy[fi]) - Foot_bias - foot_shift;
			const float  pu = (static_cast<float>(fx) - grid_u) * inv_cell;
			const float  pv = (static_cast<float>(fy) - grid_v) * inv_cell;
			int          cu = pos_cell(pu);
			int          cv = pos_cell(pv);
			if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
				return -1;
			}
			return cv * side + cu;
		};
		// The field's own dome at a cell, for an arrival distance+1 value.
		auto cell_dome = [&](int cidx, int dist_plus1) -> int {
			if (dist_plus1 <= 0) {
				return 0;
			}
			float path_px = static_cast<float>((dist_plus1 - 1) * c_tilesize) - base_px;
			if (path_px < 0.0f) {
				path_px = 0.0f;
			}
			const float ddx    = (static_cast<float>(cidx % side) - uc) * cell;
			const float ddy    = (static_cast<float>(cidx / side) - vc) * cell;
			float       travel = std::sqrt(ddx * ddx + ddy * ddy);
			if (path_px > travel) {
				travel = path_px;
			}
			const float tot  = travel + bias;
			const float dome = 1.0f - (tot * tot + e2) / rf2;
			return dome > 0.0f ? static_cast<int>(amp * dome * cone_fade(cidx % side, cidx / side) + 0.5f) : 0;
		};
		// WALL_FACE consumer: brightness = the fill's arrival at the
		// OWNER tile's face, never the cells the sprite overlaps up-screen.
		// Uncertified side = dark; no grid = free dome.  `all_sides`: a
		// Shell-certified face shows the whole wall unit (an unsealed
		// building's facade lit from any side reads the same in/out); an
		// interior-classified face (clear mask -- the barn's west wall from
		// inside) takes only viewer-side arrivals, or the lamp behind the
		// wall bleeds up its interior face.
		// An APRON cell's fill is kept for anchor-shifted SAMPLING only -- nothing
		// standing there is really lit -- so it contributes to no face or pane.
		// object_alpha/object_reached/Light_tile_alpha already rejected it; the
		// face and pane reads did not, and a lifted sprite whose foot shears onto
		// one read 217 against neighbours at 119-134 (a bright band down a shutter).
		auto apron_cell = [&](int ci) {
			return ring != nullptr && Ring::Is_apron(ring, static_cast<size_t>(ci));
		};
		auto face_alpha_from = [&](int fx, int fy, int cu, int cv, bool all_sides, bool top, int iside) -> int {
			auto face_dist = [&](int u_, int v_) -> int {
				int d;
				if (iside >= 0) {
					d = side_face_dist(u_, v_, iside);
				} else if (top) {
					d = top_face_dist(u_, v_);
				} else {
					d = all_sides ? ring_face_dist(u_, v_) : gated_face_dist(u_, v_);
				}
				const size_t ci = static_cast<size_t>(v_) * side + u_;
				if (d == 0 && !top && !Ring::Is_wall(ring, ci)
					&& !apron_cell(static_cast<int>(ci))) {
					// Nothing ever wrote a non-wall cell's side bytes, so a 0
					// from the ring here is NO ANSWER, not a dark one: take the
					// cell's own fill value, like the ground beside it.  A
					// halfwall under a window is such a cell -- the fill runs
					// straight through it -- and it rendered black against its
					// own sprite's lit half, where a pane had cleared the kind
					// byte onto the field path.
					// Wall TOPS excluded: they have their own chamfer for
					// exactly this ("reached only the long way round"), and the
					// case this was derived from was a Shell_face, not a top.
					d = Grid::Dist(grid[ci]);
				}
				return d;
			};
			// Blend the per-tile value with the neighbouring wall cells by the
			// pixel's sub-tile offset from its snapped foot (the smooth
			// gradient the bilinear field shows from inside); non-wall
			// neighbours reuse the base so the face only blends along itself.
			const int base   = cell_dome(cv * side + cu, face_dist(cu, cv));
			auto      cell_a = [&](int u_, int v_) -> int {
                if (u_ < 0 || v_ < 0 || u_ >= side || v_ >= side || !Ring::Is_wall(ring, static_cast<size_t>(v_) * side + u_)) {
                    return base;
                }
                return cell_dome(v_ * side + u_, face_dist(u_, v_));
			};
			// Past half a cell the pixel is art LIFTED off its foot (a wall
			// face, and a wall top by the whole wall height), not a sub-tile
			// position: clamping there would hand the neighbour full weight
			// and leave the owner cell none at all -- the top of a north-south
			// wall then read the dark cell behind it instead of its own lit
			// arrival.  Out of range = no blend on that axis.
			// The foot is the pixel's GROUND position, unsnapped, so its offset
			// from its own cell centre is the sub-tile position the blend needs
			// -- in [-0.5, 0.5] by construction, since cu/cv are that same
			// quantity rounded.  (px - fx) is NOT it: with the foot unsnapped
			// that difference is just the sprite's elevation, constant across
			// the art, which rendered every tile flat.
			const float ox  = pos_off((static_cast<float>(fx) - grid_u) * inv_cell, cu);
			const float oy  = pos_off((static_cast<float>(fy) - grid_v) * inv_cell, cv);
			const int   nu  = ox >= 0.0f ? cu + 1 : cu - 1;
			const int   nv  = oy >= 0.0f ? cv + 1 : cv - 1;
			const float axo = ox >= 0.0f ? ox : -ox;
			const float ayo = oy >= 0.0f ? oy : -oy;
			const float wx  = axo > 0.5f ? 0.5f : axo;
			const float wy  = ayo > 0.5f ? 0.5f : ayo;
			const float av = static_cast<float>(base) * (1.0f - wx - wy) + static_cast<float>(cell_a(nu, cv)) * wx
							 + static_cast<float>(cell_a(cu, nv)) * wy;
			return static_cast<int>(av + 0.5f);
		};
		auto face_alpha = [&](int px, int py, bool all_sides, bool top = false, int iside = -1) -> int {
			const size_t fi = static_cast<size_t>(py) * foot_lw + px;
			const int    fx = px + static_cast<int>(footdx[fi]) - Foot_bias - foot_shift;
			const int    fy = py + static_cast<int>(footdy[fi]) - Foot_bias - foot_shift;
			const float  pu = (static_cast<float>(fx) - grid_u) * inv_cell;
			const float  pv = (static_cast<float>(fy) - grid_v) * inv_cell;
			int          cu = pos_cell(pu);
			int          cv = pos_cell(pv);
			// A wall SURFACE belongs to a WALL cell: where the lattice still
			// cannot resolve which side of a boundary the foot is on, step back
			// onto the wall, toward whichever side the foot leans.
			if (ring != nullptr && cu >= 0 && cv >= 0 && cu < side && cv < side && !wall_cell(cu, cv)) {
				const int du = pos_off(pu, cu) >= 0.0f ? 1 : -1;
				const int dv = pos_off(pv, cv) >= 0.0f ? 1 : -1;
				if (cv + dv >= 0 && cv + dv < side && wall_cell(cu, cv + dv)) {
					cv += dv;
				} else if (cu + du >= 0 && cu + du < side && wall_cell(cu + du, cv)) {
					cu += du;
				}
			}
			if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
				return 0;
			}
			return face_alpha_from(fx, fy, cu, cv, all_sides, top, iside);
		};
		// The field at the object's OWN foot cell --
		// dark in a room shadow even where the sprite overlaps lit exterior;
		// its own cell carries the wall-ring arrival, so a tree or post in the
		// pool is lit from any side.  `max_walk` > 0 (whole-unit thick solids:
		// ramparts, crenelations) steps an UNREACHED foot cell toward the splat
		// centre to the footprint's lit near edge -- the fill can never enter a
		// wide solid's interior columns.  The walked depth is added back to the
		// arrival distance and the dome is taken at the pixel's OWN cell, so the
		// face keeps fading inward instead of reading the near edge's value flat
		// across the whole thickness.
		auto object_cell_alpha = [&](int cidx, int max_walk) -> int {
			if (ring != nullptr && Ring::Is_apron(ring, static_cast<size_t>(cidx))) {
				// An APRON cell (Flood_room_grid): escaped light kept in the grid
				// only so an interior pixel's anchor-shifted sample finds it.
				// Nothing standing there is really lit -- a fence outside an open
				// window took its dome and glowed.
				return 0;
			}
			int       cu    = cidx % side;
			int       cv    = cidx / side;
			int       d     = Grid::Dist(grid[static_cast<size_t>(cidx)]);
			int       steps = 0;
			const int tu    = static_cast<int>(std::lround(uc));
			const int tv    = static_cast<int>(std::lround(vc));
			if (ring != nullptr && Ring::Is_wall(ring, static_cast<size_t>(cidx))) {
				// A WALL cell records an arrival for whichever side the fill
				// touched, and reading it raw is side-blind: a lamp outside a
				// sealed building lit the wall course on the INSIDE.  Take the
				// viewer-side faces; a wall reached only from the far side is
				// dark, and the walk below must not step out through it either.
				const int fd = ring_face_dist_gated(cu, cv);
				return fd == 0 ? 0 : cell_dome(cidx, fd);
			}
			// The walk is for the deck BEHIND a rampart the fan hits head on.
			// From a cell outside the fan's own support it only finds the fan
			// sideways: a tree beside the window, never in the cone, lit from
			// the cells it walked into.
			if (d == 0 && max_walk > 0 && (cone_dx != 0 || cone_dy != 0)) {
				const int relx  = cu - grid_rt;
				const int rely  = cv - grid_rt;
				const int along = relx * cone_dx + rely * cone_dy;
				const int perp  = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
				if (along < 0 ? (along < -4 || perp > 1) : perp > along + 1) {
					return 0;
				}
			}
			for (int step = 0; d == 0 && step < max_walk; ++step) {
				// Dominant axis only: a diagonal walk drifts along the wall
				// toward the light and lands on CLOSER edge cells, lighting
				// interior-column pixels brighter than the near edge itself.
				const int du = tu - cu;
				const int dv = tv - cv;
				if (du == 0 && dv == 0) {
					break;
				}
				if (std::abs(du) >= std::abs(dv)) {
					cu += du > 0 ? 1 : -1;
				} else {
					cv += dv > 0 ? 1 : -1;
				}
				if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
					return 0;
				}
				++steps;
				d = Grid::Dist(grid[static_cast<size_t>(cv) * side + cu]);
			}
			if (d == 0) {
				return 0;
			}
			// Charge the walk to the path so the far side of a thick solid is
			// dimmer than its lit near edge, and keep the radial term on the
			// pixel's own cell.  Taking the near cell's dome verbatim painted
			// the whole thickness flat with a hard edge at the far side.
			return cell_dome(cidx, d + steps);
		};
		// The value above is one number per CELL, and a sprite wide enough to
		// straddle a cell boundary shows it as a STEP -- a tree crown cut in
		// two along the lattice diagonal, 67 one side and 111 the other, where
		// the two cells' arrivals differ (9 vs 4).  Faces already blend across
		// that seam (face_alpha_from); objects never did.  Blend the same way,
		// by the foot's offset from its own cell centre.  Pinning every pixel
		// to the anchor cell instead (unit_obj) also hides it, but flattens a
		// whole half wall to its base tile's value.
		// An UNREACHED neighbour keeps the base: 0 there means "outside the
		// fill", and letting it average in would bleed darkness back into
		// everything standing at the pool's edge.
		auto object_alpha = [&](int px, int py, int max_walk) -> int {
            const int cidx = foot_cell(px, py);
            if (cidx < 0) {
                return 0;
            }
            const int base = object_cell_alpha(cidx, max_walk);
            if (base == 0 || side <= 0) {
                return base;
            }
            const size_t fi = static_cast<size_t>(py) * foot_lw + px;
            const int    fx = px + static_cast<int>(footdx[fi]) - Foot_bias - foot_shift;
            const int    fy = py + static_cast<int>(footdy[fi]) - Foot_bias - foot_shift;
            const int    cu = cidx % side;
            const int    cv = cidx / side;
            const float  ox = pos_off((static_cast<float>(fx) - grid_u) * inv_cell, cu);
            const float  oy = pos_off((static_cast<float>(fy) - grid_v) * inv_cell, cv);
            const float  ax = ox >= 0.0f ? ox : -ox;
            const float  ay = oy >= 0.0f ? oy : -oy;
            const float  wx = ax > 0.5f ? 0.5f : ax;
            const float  wy = ay > 0.5f ? 0.5f : ay;
            auto         nb = [&](int u, int v) -> int {
                if (u < 0 || v < 0 || u >= side || v >= side) {
                    return base;
                }
                const int a = object_cell_alpha(v * side + u, max_walk);
                return a > 0 ? a : base;
            };
            const float av = static_cast<float>(base) * (1.0f - wx - wy)
							 + static_cast<float>(nb(ox >= 0.0f ? cu + 1 : cu - 1, cv)) * wx
							 + static_cast<float>(nb(cu, oy >= 0.0f ? cv + 1 : cv - 1)) * wy;
            return static_cast<int>(av + 0.5f);
		};
		// Is the object's own cell part of this light's fill?  Reachability
		// only: the dome's reach is a different question -- a wall course five
		// tiles from a candle is still in the room, and the field decides how
		// bright it is.  Apron cells do not count (see object_alpha).
		auto object_reached = [&](int px, int py) -> bool {
			const int cidx = foot_cell(px, py);
			if (cidx < 0) {
				return false;
			}
			if (ring != nullptr && Ring::Is_apron(ring, static_cast<size_t>(cidx))) {
				return false;
			}
			return Grid::Dist(grid[static_cast<size_t>(cidx)]) != 0;
		};
		// GLASS: what a pane shows is the light at its OWN tile -- the far side
		// counts, that is the point of glass -- so all four face arrivals count,
		// and an opening tile the fill flows through contributes its own cell
		// value.  The z-blind field cannot serve here: a tall window's upper
		// pixels read cells two or three tiles up-screen and went dark.
		// How far the un-shear walk may step.  MEASURED, not derived: a window
		// high in a two-storey wall needs 5 (its art rises ~40px = 5 tiles at
		// the 4px-per-z shear), and the old flat 4 missed it by one.  The
		// pane's own lift is not available here, so this is a bound with
		// headroom rather than the exact shear; the walk stops at the first
		// answer, so a larger cap only matters where every nearer cell is
		// unreached.
		// A long walk crossed the room to a far spill's own mouth; the foot stamp
		// unshears lift too, so crossing the wall is enough.
		constexpr int pane_walk_max = 2;
		auto pane_alpha = [&](int px, int py) -> int {
			const int cidx = foot_cell(px, py);
			if (cidx < 0) {
				return 0;
			}
			int       a = ring != nullptr ? face_alpha(px, py, true) : 0;
			const int d = apron_cell(cidx) ? 0 : Grid::Dist(grid[static_cast<size_t>(cidx)]);
			if (d != 0) {
				const int ca = cell_dome(cidx, d);
				if (ca > a) {
					a = ca;
				}
			}
			// A pane's art is LIFTED, so its upper pixels' feet shear up-left
			// off the building into cells the fill never saw -- the same window
			// then renders lit at the bottom and black above.  Undo the shear:
			// step down-right to the first cell that has an answer.
			for (int s = 1; a == 0 && s <= pane_walk_max; ++s) {
				const int wu = cidx % side + s;
				const int wv = cidx / side + s;
				if (wu >= side || wv >= side) {
					break;
				}
				const int wi = wv * side + wu;
				const int wd = (ring != nullptr && Ring::Is_wall(ring, static_cast<size_t>(wi)))
									   ? ring_face_dist(wu, wv)
									   : (apron_cell(wi) ? 0 : Grid::Dist(grid[static_cast<size_t>(wi)]));
				if (wd != 0) {
					a = cell_dome(wi, wd);
				}
			}
			return a;
		};
		// ACTOR pixels sample like ground here -- silhouette-shaped
		// deviations would ghost in the cached static coverage; their cap at
		// the light level of their own feet is applied per frame on the
		// composite (build_light_layers).
		for (int y = y0; y <= y1; ++y) {
			const int            dy      = y - sy;
			const float          dy2     = static_cast<float>(dy) * static_cast<float>(dy);
			const unsigned char* roofrow = roofpix ? roofpix + y * roof_lw : nullptr;
			const unsigned char* kindrow = face_data ? kindpix + static_cast<size_t>(y) * kind_lw : nullptr;
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
				// Outside the pool proper only the sprites that EXTENDED the box
				// may light, and only inside their own art box: membership is
				// the object's box, not the clamped per-pixel foot channel.
				if (nreach > 0 && (x < pool_x0 || y < pool_y0)) {
					bool mine = false;
					for (int i = 0; i < nsprites && !mine; ++i) {
						const Sprite_box& s = sprites[i];
						mine = s.fx >= pool_x0 && s.fx <= x1 && s.fy >= pool_y0 && s.fy <= y1 && x >= s.x0 && x <= s.x1
							   && y >= s.y0 && y <= s.y1;
					}
					if (!mine) {
						continue;
					}
				}
				// The branches below decide per light kind (veto / spill /
				// exterior) what stays dark, what is lit as a whole unit
				// (bypass_field) and what keeps sampling the propagated field.
				// A wall-face or object pixel resolves by its OWNER's surface
				// (ring arrivals / foot cell) instead of the mask heuristics.
				// An object's flat top is an object everywhere but the averted test.
				const bool obj_top = kindrow != nullptr && kindrow[x] == Kind::Object_top;
				const int  kv      = (face_grid && kindrow != nullptr) ? (obj_top ? Kind::Object : kindrow[x]) : Kind::Ground;
				// The MASK is the authority on "part of the shell": the wall
				// course under a window is stamped Shell_face by the shell branch
				// but classified as an object, so it skipped face_alpha and took
				// the object max instead -- 20-40 levels hotter than the wall it
				// sits in.  Where the two disagree, follow the mask.
				const bool wall_obj = kv == Kind::Object && roofrow != nullptr && Mask::Is_wall(roofrow[x]);
				const bool face_px = kv == Kind::Wall_face || wall_obj;
				const bool surf_px = kv == Kind::Wall_face || kv == Kind::Object;
				bool                bypass_field = false;
				int                 forced_a     = -1;
				// A light-passing wall piece (Shell_face with Kind::Ground -- window
				// glass, an open shutter's panel; true wall faces carry Wall_face)
				// is a surface every light reaches like clear ground: the lit
				// room shows through it from outside, and a fan or a torch out
				// there lights it.  Gating this on the veto light alone left
				// the same piece dark from outside while it lit from inside.
				const bool pane_px = roofrow != nullptr && roofrow[x] == Mask::Shell_face && kindrow != nullptr
									 && kindrow[x] == Kind::Ground;
				// A shell wall's flat TOP: a surface, not a face, so it samples
				// like clear ground and counts arrivals from every side below.
				bool top_px = roofrow != nullptr && roofrow[x] == Mask::Shell_top;
				// Interior faces carry the room side they look onto (0 south, 1
				// east); they are wall art, not a marked whole unit.
				int iside_px = roofrow == nullptr ? -1
							   : roofrow[x] == Mask::Face_south ? 0
							   : roofrow[x] == Mask::Face_east  ? 1
																 : -1;
				// A face exactly ONE pixel wide with wall TOP on both sides is
				// the junction two overlapping wall sprites leave behind: the
				// rectangle that splits top from face carves a face strip out of
				// every sprite, and the next sprite's top covers all of it but
				// the last column.  Read as a face it takes a room side that
				// often has no arrival at all and draws a black seam through a
				// lit wall top.  It must be ONE pixel with tops on BOTH sides:
				// a genuinely covered face is a wide region, so no pixel of it
				// qualifies.
				if (iside_px >= 0 && roofrow != nullptr && x > x0 && x < x1 && roofrow[x - 1] == Mask::Shell_top
					&& roofrow[x + 1] == Mask::Shell_top) {
					top_px   = true;
					iside_px = -1;
				} else if (
						iside_px >= 0 && roofpix != nullptr && y > y0 && y < y1
						&& roofpix[static_cast<size_t>(y - 1) * roof_lw + x] == Mask::Shell_top
						&& roofpix[static_cast<size_t>(y + 1) * roof_lw + x] == Mask::Shell_top) {
					top_px   = true;
					iside_px = -1;
				}
				// The same junction for any FACE mark, along EITHER axis: where a
				// shutter's top abuts a wall top the two sprites strand a short
				// run of face between them -- 2px across for a south-facing
				// wall, 3 rows down for an east-facing one, which is why fixing
				// one axis fixed only one orientation.  Face marks are vetoed
				// dark while the Shell_top at both ends is exempt, so the run draws a
				// blob at the end of a lit top.  A real face is a wide region
				// and is never closed by TOP at both ends of a run this short.
				if (!top_px && roofrow != nullptr && Mask::Is_face(roofrow[x])) {
					const int m    = roofrow[x];
					bool      seam = false;
					int       l    = x;
					while (l > x0 && x - l < 3 && roofrow[l - 1] == m) {
						--l;
					}
					int r = x;
					while (r < x1 && r - x < 3 && roofrow[r + 1] == m) {
						++r;
					}
					if (r - l < 3 && l > x0 && r < x1 && roofrow[l - 1] == Mask::Shell_top
						&& roofrow[r + 1] == Mask::Shell_top) {
						seam = true;
					}
					if (!seam && roofpix != nullptr) {
						auto at = [&](int yy) {
							return roofpix[static_cast<size_t>(yy) * roof_lw + x];
						};
						int t = y;
						while (t > y0 && y - t < 3 && at(t - 1) == m) {
							--t;
						}
						int b = y;
						while (b < y1 && b - y < 3 && at(b + 1) == m) {
							++b;
						}
						if (b - t < 3 && t > y0 && b < y1 && at(t - 1) == Mask::Shell_top && at(b + 1) == Mask::Shell_top) {
							seam = true;
						}
					}
					if (seam) {
						top_px   = true;
						iside_px = -1;
					}
				}
				// The same seam one storey up: a wall standing ON a deck hides
				// all of the deck's top but the column beside its own thickness
				// strip.  That column carries the TOP's mark, which every light
				// gates by storey, so it drew a dark hairline between the lit
				// wall and the lit edge below it.  One pixel, face on one side
				// and its own strip on the other -- a real deck top is wide.
				if (!top_px && iside_px < 0 && roofrow != nullptr && x > x0 && x < x1
					&& (Mask::Is_tall(roofrow[x]) || Mask::Is_slab(roofrow[x])) && Mask::Is_slab_edge(roofrow[x + 1])
					&& Mask::Is_wall(roofrow[x - 1])) {
					top_px = true;
				}
				if (roofrow && roofrow[x] && !pane_px && !top_px && iside_px < 0) {
					// A slab's TOP and its thickness STRIP are one surface: the
					// strip only carries its own value so a spill landing on the
					// top cannot light the edge facing away from it.  Every other
					// light treats them alike, or the edge draws a dark seam along
					// each slab and around every floor hole.
					const bool slab_edge = Mask::Is_slab_edge(roofrow[x]);
					int        slab_s    = -1;
					if (Mask::Is_slab(roofrow[x])) {
						slab_s = roofrow[x] - Mask::Slab;
					} else if (slab_edge) {
						slab_s = roofrow[x] - Mask::Slab_edge;
					}
					// The strip is the slab's DOWN-facing edge, and it carries its
					// own mark ONLY so this one case can be told apart: a spill that
					// landed ON the deck lights the top, never the edge facing away
					// from it.  Every other light treats strip and top alike (see
					// slab_s), exactly as when they shared a mark.
					if (slab_edge && spill_on_deck && slab_s >= spill_floor) {
						continue;
					}
					if (veto_roof) {
						if (inside_viewer && roofrow[x] == Mask::Tall && kindrow != nullptr
							&& kindrow[x] == Kind::Wall_face) {
							// A whole-unit WALL line (a porch wall rising past its
							// roof edge) in the viewer's space: sample the field
							// like an interior wall -- only objects take the
							// rampart arrival-walk below.
						} else {
							// An under-roof light keeps marked pixels dark: real
							// roofs and whole-unit tall marks (tree canopies, deck
							// objects) never light up under it.
							// EXCEPTION: a floor SLAB surface one or more storeys up
							// is something the light's fill can
							// genuinely reach -- but ONLY when the light's own room
							// roof rises ABOVE that storey (light_top_storey): the
							// upper room's own lamp lights its floor, while the light
							// of the room BELOW (its roof is that storey's floor) must
							// never brighten it from underneath.  It samples the
							// propagated field like clear ground, so where the fill
							// never reached, the field is 0 and it stays dark.
							// An outdoor deck never carries the Slab mark (see the
							// slab stamp), so this cannot light one from below.
							// Exterior wall faces stay dark here for an outside
							// viewer; panes and inside faces resolved above.
							// A floor is dark only when it is THIS light's own
							// ceiling, which light_top_storey says: a gallery in a
							// hall whose roof reaches a storey higher does not
							// cover a lamp on the hall floor.  Its side STRIP and
							// the upper WALLS that wear the slab mark read the
							// same way.
							if (slab_s < 0 || slab_s >= light_top_storey) {
								// Whole-unit marks stay dark under an interior light,
								// OUTDOOR ones included: light that escapes an opening
								// is delivered by the spill fan alone, so a tree or a
								// rampart outside only lights when it stands in a fan.
								// Reading the escaped ring arrivals here lit every
								// exterior unit around the building as soon as it had
								// any opening, whatever direction it faced.
								continue;
							}
						}
					} else if (is_spill) {
						if (roofrow[x] == Mask::Roof) {
							continue;    // A spill never lights a real roof.
						}
						if (slab_edge) {
							// A slab's thickness strip is the deck's own edge: sample
							// the field like the deck top beside it, or the fan breaks
							// at the strip and resumes a strip-width further south.
							// Storey-gated like the slab surface it belongs to: flat,
							// a ground spill lit the edge of a slab a storey above it.
							if (slab_s > spill_floor) {
								continue;
							}
							bypass_field = false;
						} else if (Mask::Is_slab(roofrow[x])) {
							if (slab_s > spill_floor) {
								continue;    // A slab on a storey above the spill's.
							}
							// With the roof hidden EVERY upper-storey surface carries
							// the slab mark, walls included, so the storey test alone
							// also admits the wall the spill came out of -- the course
							// under the window lit flat at its own foot value.  Kind
							// cannot tell them apart (that course is an object, like a
							// crate on the deck); its FOOT CELL can -- the wall the
							// spill exited is a wall cell in the fan's own grid.
							if (kindrow != nullptr && kindrow[x] != Kind::Ground) {
								if (kindrow[x] == Kind::Wall_face) {
									continue;    // A face: dark under a spill as ever.
								}
								const int sci
										= (face_data && grid != nullptr && ring != nullptr) ? foot_cell(x, y) : -1;
								if (sci >= 0 && wall_cell(sci % side, sci / side)) {
									continue;
								}
							}
							bypass_field = false;
						} else if (roofrow[x] == Mask::Shell_face) {
							// A facade across the street just STANDS in the fan, so
							// skipping every shell face left it black.  Keep it on the
							// ring read below, whose cell_dome is cone-gated.
							// The wall the spill EXITED stands on the cone's apex
							// plane, where cone_fade is deliberately 1 -- the mouth
							// must not taper -- so nothing else holds the fan off its
							// own facade, and the half wall under the window lit from
							// the light behind it.  Only what the fan travels TO can
							// be lit by it.
							const int fci = (face_data && grid != nullptr && ring != nullptr) ? foot_cell(x, y) : -1;
							if (fci >= 0 && (cone_dx != 0 || cone_dy != 0)) {
								const int along
										= (fci % side - grid_rt) * cone_dx + (fci / side - grid_rt) * cone_dy;
								if (along <= 0) {
									continue;
								}
							}
						} else if (roofrow[x] - Mask::Tall > spill_floor) {
							continue;    // Higher-storey whole-unit marks.
						}
						if (roofrow[x] <= Mask::Tall) {
							// Ground-storey Tall: a standing deck object or a
							// rampart's crenellation, lit as a whole unit.  A missing
							// kind byte is NOT a reason to veto: the kind mask is owned
							// by the topmost sprite, so a decoration drawn over the
							// merlon clears its kind while the Tall mark stays, and the
							// slab strips this veto used to catch carry Slab_edge now.
							if (grid == nullptr) {
								// Gridless spill: treat as fully lit (old bypass).
								bypass_field = true;
							} else {
								// Light the unit by the fan's value at its OWN foot
								// cell (whole crown, dome-faded -- geometry, not
								// screen overlap, so it cannot cut out as the fan's
								// screen shape shifts), topped by the fan field where
								// the sprite overlaps it; outside both it stays dark
								// (no floating whole-unit block over dark ground).
								// A solid's foot cell never enters the fill, so its
								// arrival lives in the RING; and a rampart is deep --
								// the fan stops at its outer face and the whole
								// walkway behind it borrows that arrival, charged by
								// the distance walked so it still fades.  The limit
								// is how far a deck may extend behind its face.
								// The ring read is for a solid that IS a wall cell.  A
								// tree is a passable tile now, and face_alpha's step
								// back onto an adjacent wall handed it that wall's
								// arrival: the half of the crown whose feet lean
								// toward the house lit, the other half stayed dark.
								const int  uci        = footdx != nullptr ? foot_cell(x, y) : -1;
								const bool ring_solid = ring != nullptr && uci >= 0 && wall_cell(uci % side, uci / side);
								int        forced     = footdx != nullptr ? object_alpha(x, y, 10) : 0;
								// Lit only by the walk over a solid (its cell never reached), with
								// the light arriving mainly from the N or W: the S / E faces we
								// see are averted.  Dominant axis, so a light to the NE of a long
								// E-W rampart still leaves its S face dark.
								const float       ldu       = uci >= 0 ? uc - static_cast<float>(uci % side) : 0.0f;
								const float       ldv       = uci >= 0 ? vc - static_cast<float>(uci / side) : 0.0f;
								const bool        sees_lit  = (ldv > 0.5f && ldv >= std::fabs(ldu))
													  || (ldu > 0.5f && ldu >= std::fabs(ldv));
								const bool        averted   = !ring_solid && uci >= 0
													   && Grid::Dist(grid[static_cast<size_t>(uci)]) == 0 && !sees_lit;
								const bool        foot_lit  = forced > 0;
								const int         walk_a    = forced;
								// ONE unit, ONE cell.  The stamped feet of a crown spread
								// over the footprint (and shear past it), so with the
								// trunk's tile in the fan half the crown's feet still
								// fell in cells outside it: a tree lit in two.  Light the
								// whole unit by its ANCHOR tile -- the sprite's paint
								// foot, which the reach pass already records -- and keep
								// the per-pixel read only as a floor (a rampart's
								// walkway still fades along its length).
								// Not for a unit standing on WALL cells: that is a wall
								// the fan stopped at (a battlement), and its face must
								// keep the per-pixel ring read and its falloff -- the
								// anchor's 3x3 crown lit each 4x4 segment flat at the
								// brightest ground beside it, a hard-edged block.  Asked
								// per UNIT at its anchor tile's centre, not per pixel:
								// the lattice puts a tile's last foot rows in the next
								// cell, so a per-pixel test would anchor those stripes.
								bool anchored = false;
								if (sprites != nullptr && footdx != nullptr) {
									const size_t ufi  = static_cast<size_t>(y) * foot_lw + x;
									const int    pfx  = x + static_cast<int>(footdx[ufi]) - Foot_bias;
									const int    pfy  = y + static_cast<int>(footdy[ufi]) - Foot_bias;
									int          best = -1;
									int          bd   = 1 << 30;
									for (int i = 0; i < nsprites; ++i) {
										const Sprite_box& s = sprites[i];
										if (x < s.x0 || x > s.x1 || y < s.y0 || y > s.y1) {
											continue;
										}
										// The stamped foot lies in the owner's footprint,
										// up-left of its anchor foot.
										const int ddx = s.fx - pfx;
										const int ddy = s.fy - pfy;
										if (ddx < 0 || ddy < 0 || ddx >= 4 * c_tilesize || ddy >= 4 * c_tilesize) {
											continue;
										}
										if (ddx + ddy <= bd) {
											bd   = ddx + ddy;
											best = i;
										}
									}
									if (best >= 0 && ring != nullptr) {
										const int half_t = c_tilesize / 2;
										const int wcu    = pos_cell(
                                                (static_cast<float>(sprites[best].fx - half_t - foot_shift) - grid_u)
                                                * inv_cell);
										const int wcv    = pos_cell(
												(static_cast<float>(sprites[best].fy - half_t - foot_shift) - grid_v)
												* inv_cell);
										if (wcu >= 0 && wcv >= 0 && wcu < side && wcv < side && wall_cell(wcu, wcv)) {
											best    = -1;
										}
									}
									if (best >= 0) {
										anchored        = true;
										const int   afx = sprites[best].fx;
										const int   afy = sprites[best].fy;
										const float au  = (static_cast<float>(afx - foot_shift) - grid_u) * inv_cell;
										const float av  = (static_cast<float>(afy - foot_shift) - grid_v) * inv_cell;
										const int   acu = pos_cell(au);
										const int   acv = pos_cell(av);
										if (acu >= 0 && acv >= 0 && acu < side && acv < side) {
											int aa = object_cell_alpha(acv * side + acu, 10);
											// A crown spreads a tile or so around its trunk, so
											// the light that hits it is the light on the ground
											// it covers, not the trunk tile alone: the trunk on
											// the fan's quarter band under a crown over the full
											// pool read 39 against 171 on the grass beneath it.
											// Brightest reached open cell of the 3x3 around the
											// anchor; the trunk must be reached at all for any
											// of it to count.
											// Unless the trunk sits just past the fan's edge: its
											// crown still reaches into the fan, at half the light.
											const bool edge_crown = aa <= 0 && (cone_dx != 0 || cone_dy != 0);
											if (aa > 0 || edge_crown) {
												int na_best = 0;
												for (int dv_ = -1; dv_ <= 1; ++dv_) {
													for (int du_ = -1; du_ <= 1; ++du_) {
														const int nu_ = acu + du_;
														const int nv_ = acv + dv_;
														if (nu_ < 0 || nv_ < 0 || nu_ >= side || nv_ >= side) {
															continue;
														}
														const int ni_ = nv_ * side + nu_;
														if (apron_cell(ni_) || (ring != nullptr && wall_cell(nu_, nv_))) {
															continue;
														}
														const int na_ = cell_dome(ni_, Grid::Dist(grid[static_cast<size_t>(ni_)]));
														if (na_ > na_best) {
															na_best = na_;
														}
													}
												}
												if (aa <= 0) {
													na_best /= 2;
												}
												if (na_best > aa) {
													aa = na_best;
												}
											}
											// Trunk outside the fan, crown drawn over the pool: the whole unit
											// takes the field under its art centre, if that is lit open ground.
											if (edge_crown && ftmpl != nullptr) {
												const Sprite_box& sb_  = sprites[best];
												const int         ccx_ = (sb_.x0 + sb_.x1) / 2;
												const int         ccy_ = (sb_.y0 + sb_.y1) / 2;
												const int scu_ = static_cast<int>(std::lround((static_cast<float>(ccx_) - grid_u) * inv_cell));
												const int scv_ = static_cast<int>(std::lround((static_cast<float>(ccy_) - grid_v) * inv_cell));
												if (scu_ >= 0 && scv_ >= 0 && scu_ < side && scv_ < side) {
													const int sci_ = scv_ * side + scu_;
													const int ty_  = ccy_ - sy - ftmpl->y0;
													const int tx_  = ccx_ - sx - ftmpl->x0;
													if (Grid::Dist(grid[static_cast<size_t>(sci_)]) != 0 && !apron_cell(sci_)
														&& (ring == nullptr || !wall_cell(scu_, scv_)) && ty_ >= 0 && ty_ < ftmpl->h
														&& tx_ >= 0 && tx_ < ftmpl->w) {
														// No brighter than the trunk's own uncone'd dome: past the
														// pool's reach the art centre still read the bright mouth.
														const float tdx_  = (static_cast<float>(acu) - uc) * cell;
														const float tdy_  = (static_cast<float>(acv) - vc) * cell;
														const float ttot_ = std::sqrt(tdx_ * tdx_ + tdy_ * tdy_) + bias;
														const float tdom_ = 1.0f - (ttot_ * ttot_ + e2) / rf2;
														const int   tcap_ = tdom_ > 0.0f ? static_cast<int>(amp * tdom_ + 0.5f) : 0;
														const int   ca_ = std::min<int>(ftmpl->alpha[static_cast<size_t>(ty_) * ftmpl->w + tx_], tcap_);
														if (ca_ > aa) {
															aa = ca_;
														}
													}
												}
											}
											if (aa > forced) {
												forced = aa;
											}
											// The ground light AT the trunk, bilinear-smoothed: the
											// same field sample the clear pixel at its foot takes.
											if (forced > 0 && ftmpl != nullptr && afy >= 0 && afy < H) {
												const int ty_ = afy - sy - ftmpl->y0;
												const int tx_ = afx - sx - ftmpl->x0;
												if (ty_ >= 0 && ty_ < ftmpl->h && tx_ >= 0 && tx_ < ftmpl->w) {
													const int fa_ = ftmpl->alpha[static_cast<size_t>(ty_) * ftmpl->w + tx_];
													if (fa_ > forced) {
														forced = fa_;
													}
												}
											}
										}
									}
								}
								if (ring_solid && ring != nullptr && footdx != nullptr) {
									const int ra = face_alpha(x, y, true);
									if (ra > forced) {
										forced = ra;
									}
								}
								// The z-blind field at the pixel's OWN screen position is
								// the ground 3-4 tiles down-screen of where a crown
								// really hangs.  A crown over LIT ground takes it (a
								// tree beside the pool glows where it overhangs it);
								// but the template's bilinear fringes half a cell past
								// the fan into unreached cells, and a crown over the
								// house wall beside the window took the mouth's fringe.
								// So the cell UNDER the pixel on screen must itself be
								// lit GROUND, unless the unit's own cell already is.
								// Ground only: the wall pieces beside the window are
								// reached cells too (their ring arrival), and the crown
								// over them took the mouth's fringe just the same.
								// An ANCHORED unit under a coned fan took its one value
								// above; the per-pixel overlap would split the crown again
								// (119 over the mouth, 39 beside it).
								bool trow_ok = forced > 0 && !(anchored && (cone_dx != 0 || cone_dy != 0));
								if (!trow_ok && !anchored && grid != nullptr) {
									const int scu = static_cast<int>(std::lround((static_cast<float>(x) - grid_u) * inv_cell));
									const int scv = static_cast<int>(std::lround((static_cast<float>(y) - grid_v) * inv_cell));
									if (scu >= 0 && scv >= 0 && scu < side && scv < side) {
										const int sci = scv * side + scu;
										trow_ok       = Grid::Dist(grid[static_cast<size_t>(sci)]) != 0 && !apron_cell(sci)
												  && (ring == nullptr || !wall_cell(scu, scv));
									}
								}
								if (trow_ok && ftmpl != nullptr && trow != nullptr) {
									const int ttx = x - sx - ftmpl->x0;
									if (ttx >= 0 && ttx < ftmpl->w && trow[x] > forced) {
										forced = trow[x];
									}
								}
								// 0 stays on the forced path (a = 0 drops below).
								// A flat top still takes the walk's own arrival.
								if (averted && (foot_lit || !anchored)) {
									forced = obj_top ? walk_a : 0;
								}
								forced_a = forced > 0 ? forced : 0;
							}
						} else {
							// An upper-storey Tall mark (storey >= 1: a
							// floor-slab TOP, an upper wall) sits on the tile
							// lattice like clear ground: keep sampling the
							// propagated field so the spill pools across the deck
							// bounded by its walls.
							bypass_field = false;
						}
					} else {
						// A floor SLAB surface is a room's ceiling
						// seen from beneath when the light is lower: keep it dark,
						// or a ground torch lights the whole floor above it through
						// the walls.  At or below the light's own storey it is
						// ground to it and samples the field, so walls block it.
						// Everything else -- an exterior building's upper-storey
						// wall mass (Tall), roofs -- is lit as a
						// whole unit: an outdoor light illuminates a facade to its
						// full height.
						if (slab_s >= 0) {
							if (slab_s > light_floor_storey) {
								continue;
							}
							bypass_field = false;
						} else {
							bypass_field = true;
						}
					}
				} else {
					// An ELEVATED spill (a stairwell bubble on an upper
					// floor; light_top_storey carries its render storey):
					// clear pixels are ground-level surfaces -- the walls
					// and terrain BELOW it, over which its z-blind field
					// cells hang up-screen.  Lighting them shows as a
					// bright beam under the storey; everything the bubble
					// may light up there is marked Tall + storey.  A GROUND
					// spill paints its fan for any viewer: the shell that
					// used to catch its z-blind wash from inside is stamped
					// Shell_face now (faces_exterior).
					if (roofrow && is_spill && light_top_storey >= 1) {
						continue;
					}
				}
				// build_light_layers attaches the room grid only when the
				// viewer is INSIDE, so the same outdoor lamp rendered a
				// building's shell as per-tile face plateaus from inside and
				// as the free dome from outside.  The shell is exterior in
				// both views: give it the dome either way.  Interior faces
				// and everything else keep the grid, so an outdoor
				// light still cannot flood the room.
				const bool shell_dome = !veto_roof && !is_spill && roofrow != nullptr && Mask::Is_shell(roofrow[x]);
				if (shell_dome) {
					bypass_field = true;
				}
				int a;
				if (forced_a >= 0) {
					a = forced_a;
				} else if (pane_px && face_data && grid != nullptr) {
					a = pane_alpha(x, y);
				} else if (face_px && !shell_dome) {
					// Shell = certified exterior masonry: whole-unit, all-side
					// arrivals; an interior face reads its own room side only.
					a = face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, iside_px);
					// Same arrival-only blind spot the top read documents below: a
					// sill spans its window's own open fill cells, which carry
					// no arrival, so the TOP read is 0 right across the opening.
					// That branch is veto-only, so under a SPILL this one owns
					// the sill and it rendered black against its lit wall.
					// Shell marks only (iside < 0): an interior face must keep
					// reading its own room's side.
					if (a <= 0 && top_px && iside_px < 0) {
						a = face_alpha(x, y, true);
					}
					// Arrivals along a wall line are CONSTANT -- 4 across the
					// whole lit band, then 0 -- so a fan landing on a facade
					// rendered as a flat slab ending in a cliff, while the same
					// light entering a window is soft because the field carries
					// the fan's distance.  A facade has no such field (the fill
					// stops at it), so take the fan's GEOMETRY instead: the
					// radial dome for the gradient, cone_fade for the taper, and
					// the fill's own `perp <= along + 1` support so it stops at
					// the cone edge rather than washing round the corner (which
					// is what the bare dome did).
					if (is_spill && iside_px < 0 && (cone_dx != 0 || cone_dy != 0)
						&& face_data && grid != nullptr && ring != nullptr && roofrow != nullptr
						&& Mask::Is_shell(roofrow[x])) {
						const int fci = foot_cell(x, y);
						if (fci >= 0 && a > 0) {
							// Sub-tile, from the pixel's own foot: a per-CELL perp
							// is an integer, so the taper landed in flat diagonal
							// bands across the wall.  And the fan is only
							// `along + 1` tiles wide, which 2 tiles out is 3 --
							// narrower than a fixed 3-tile penumbra, so a constant
							// width ramped the whole facade instead of its edge.
							const size_t ffi   = static_cast<size_t>(y) * foot_lw + x;
							const float  ffx   = static_cast<float>(x + static_cast<int>(footdx[ffi]) - Foot_bias - foot_shift);
							const float  ffy   = static_cast<float>(y + static_cast<int>(footdy[ffi]) - Foot_bias - foot_shift);
							const float  frelx = (ffx - grid_u) * inv_cell - static_cast<float>(grid_rt);
							const float  frely = (ffy - grid_v) * inv_cell - static_cast<float>(grid_rt);
							const float  along = frelx * static_cast<float>(cone_dx) + frely * static_cast<float>(cone_dy);
							const float  perp  = Cone_perp_f(frelx, frely, cone_dx, cone_dy, cone_mouth);
							const float half_w = 0.5f * (along + 1.0f);
							const float band   = half_w < 1.0f ? 1.0f : half_w;
							float       taper  = (along + 1.0f - perp) / band;
							if (taper < 0.0f) {
								taper = 0.0f;
							} else if (taper > 1.0f) {
								taper = 1.0f;
							}
							int dome_a = 0;
							if (along > 0.0f && taper > 0.0f) {
								const int   ddx   = x - sx;
								const float dist2 = static_cast<float>(ddx) * static_cast<float>(ddx) + dy2;
								if (dist2 <= rr) {
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
									if (dome > 0.0f) {
										dome_a = static_cast<int>(amp * dome * taper + 0.5f);
									}
								}
							}
							// The arrival read keeps the SUPPORT (only it knows
							// about occlusion); the taper supplies the edge and
							// the dome the gradient.
							a = dome_a < a ? dome_a : a;
						}
					}
				} else if (surf_px && !bypass_field) {
					// Whole-unit Tall marks keep the free-dome bypass: a tree
					// crown near the lamp glows even though its trunk stands
					// beyond the pool.
					a = object_alpha(x, y, 0);
				} else if (
						veto_roof && face_data && grid != nullptr && ring != nullptr && roofrow != nullptr
						&& (top_px
							|| (Mask::Is_slab(roofrow[x]) && kindrow != nullptr && kindrow[x] == Kind::Wall_face))) {
					// A wall TOP under a veto light has only the z-blind field,
					// which at an upper wall's height maps to cells NORTH of the
					// wall: a window sill faded 74 -> 2 down its own sprite.
					// Its foot carries the room's arrival instead.  The BRIGHTER
					// of the two, never just the foot read -- a ground wall's
					// top is lit correctly by the field already, and replacing
					// it outright darkened those into a stripe.
					// (Worth retrying only because `light_walls` was broken when
					// this was first tried: the ring then had no wall arrivals
					// at all, so the foot read had nothing and produced flat
					// plateaus.)
					a = face_alpha(x, y, true, true, -1);
					// The top read only consults ring ARRIVALS.  A sill spans its
					// window's own tiles, which are open fill cells with no
					// arrival at all, so it returned 0 across the opening and lit
					// only at the two ends where it meets solid wall.  The
					// all-sides read takes the fill value there -- the same one
					// the glass beside it shows.
					if (a <= 0) {
						a = face_alpha(x, y, true);
					}
					if (ftmpl != nullptr && trow != nullptr) {
						const int tx = x - sx - ftmpl->x0;
						const int fv = (tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
						if (fv > a) {
							a = fv;
						}
					}
				} else if (
						veto_roof && face_data && grid != nullptr && kindrow != nullptr
						&& (kindrow[x] == Kind::Object || obj_top) && !bypass_field) {
					// A veto splat disables face_grid, so kind is never consulted
					// above: an OBJECT standing outside (a fence beside a lit
					// window) has its sprite hanging over the room's lit cells and
					// drank that field straight through the wall.  Being in the
					// fill at all decides WHETHER it is lit; the field still
					// decides how bright, or furniture bands into flat per-cell
					// blocks instead of taking the room's smooth wash.
					// A Shell_top is not such an object: the mask already
					// certified it as the building's own masonry, and a sill under
					// an OPEN shutter sits on an escaped (apron) cell, which this
					// gate read as "not lit" -- dark on the south and east walls
					// while the north and west ones, whose cells are inside, lit.
					// Nor is any other SHELL mark: a wall course under a window
					// is stamped a wall mark by the shell branch yet classified
					// an object, and outside a veto light `wall_obj` already lets
					// the mask overrule the kind.  It cannot here -- a veto
					// forces kv to Ground -- so the half wall under a window was
					// judged a loose object, failed the fill test and went dark.
					if (roofrow != nullptr && Mask::Is_wall(roofrow[x])) {
						a = face_alpha(x, y, Mask::Is_shell(roofrow[x]), top_px, iside_px);
						// The ring read runs ~70 below the SAME face's Wall_face
						// pixels, which take the field, so one wall split into two
						// brightnesses along the kind boundary.  Only tops and
						// INTERIOR faces reach here (Shell_face is cut above),
						// which the room is meant to light.  Tops keep the
						// arrival-only read -- that is the window-sill fix.
						if (!top_px && ftmpl != nullptr && trow != nullptr) {
							const int tx = x - sx - ftmpl->x0;
							const int fv = (tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
							if (fv > a) {
								a = fv;
							}
						}
					} else {
						if (!top_px && !object_reached(x, y)) {
							continue;
						}
						const int tx = x - sx - ftmpl->x0;
						a            = (trow != nullptr && tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
					}
				} else if (grid != nullptr && !bypass_field) {
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
					a                 = 0;
					if (dist2 <= rr) {
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
					// A whole-unit object pixel also takes the brightest of its
					// owner cells' arrivals (smoothed), its foot cell's field and
					// the z-blind field at its own screen position: a tree crown
					// keeps its near-lamp glow, a rampart lights along its length
					// with the same wide wash the room field gives it from inside.
					// An interior FACE is excluded: it has a positive
					// answer from the room it looks onto, and the z-blind field
					// here belongs to the lit room the sprite hangs over
					// up-screen -- it lit every inside face of a sealed room.
					// A shell pixel takes the bare dome: oa/fa are the per-tile
					// plateaus this path exists to avoid.
					if (surf_px && iside_px < 0 && !wall_obj && !shell_dome) {
						const int oa = object_alpha(x, y, 4);
						if (oa > a) {
							a = oa;
						}
						const int sa = face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, iside_px);
						if (sa > a) {
							a = sa;
						}
						if (ftmpl != nullptr && trow != nullptr) {
							const int ttx = x - sx - ftmpl->x0;
							const int fv  = (ttx >= 0 && ttx < ftmpl->w) ? trow[x] : 0;
							if (fv > a) {
								a = fv;
							}
						}
					}
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

	void Stamp_surface(const Surface_target& target, Game_object* obj, int sx, int sy) {
		Image_buffer8* const         roof_light_mask    = target.mask;
		Image_buffer8* const         light_kind_mask    = target.kind;
		std::vector<unsigned char>&  light_foot_dx      = target.foot_dx;
		std::vector<unsigned char>&  light_foot_dy      = target.foot_dy;
		std::vector<Sprite_box>&     light_sprite_boxes = target.sprites;
		const std::vector<TileRect>& light_mask_rects   = target.cull_rects;
		Game_window* const           gwin               = Game_window::get_instance();
		Game_map* const              map                = gwin->get_map();
		Actor* const                 main_actor         = gwin->get_main_actor();
		Shape_frame* frame = obj->get_shape();
		if (!frame || !frame->is_rle()) {
			return;
		}
		// Only pixels a light splat actually samples matter: skip any object whose
		// sprite touches none of last frame's splat rectangles (build_light_layers
		// records them, grown by a margin).  With no lights in view the whole mask
		// paint is skipped.  A newly appearing light or a view jump self-corrects
		// on the next frame.
		const TileRect sprite_box(sx - frame->get_xleft(), sy - frame->get_yabove(), frame->get_width(), frame->get_height());
		{
			// A skipped object is not left stale, it is left UNSTAMPED
			// (Mask::Clear = ground), which drinks the z-blind field and renders
			// bright -- so anything the rects miss becomes a lit copy of the wall
			// face it should have masked.
			bool touched = false;
			for (const TileRect& r : light_mask_rects) {
				if (sprite_box.intersects(r)) {
					touched = true;
					break;
				}
			}
			if (!touched) {
				return;
			}
		}
		// Actors leave every channel untouched (mask, kind, feet): they move each
		// frame, and any actor-shaped stamp bakes ghosts into the cached static
		// coverage.  Their pixels light like the surface behind them.
		if (obj->as_actor() != nullptr) {
			return;
		}
		// Record the art box for the reach pass.  Only free-standing OBJECTS get
		// one: a wall face is resolved inside the pool by the ring/face rule, and
		// reaching for it lights shell art beyond the light's radius -- the wall
		// piece that appears copied across the screen as the view scrolls.
		{
			const Shape_info& rinf = obj->get_info();
			if (!NaturalLight::Object_is_wall_face(obj) && !rinf.is_floor() && !rinf.is_roof()
				&& (sprite_box.y < sy || sprite_box.x < sx)) {
				light_sprite_boxes.push_back(
						{static_cast<short>(sprite_box.x), static_cast<short>(sprite_box.y),
						 static_cast<short>(sprite_box.x + sprite_box.w - 1),
						 static_cast<short>(sprite_box.y + sprite_box.h - 1), static_cast<short>(sx),
						 static_cast<short>(sy)});
			}
		}
		// Mask values: see NaturalLight::Mask.  How each light kind treats them is
		// decided in Splat_radial_light's pixel loop.
		static const Xform_palette roof_set = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = Mask::Roof;
			}
			return x;
		}();
		static const std::array<Xform_palette, Mask::Storeys> roof_tall = [] {
			std::array<Xform_palette, Mask::Storeys> xs{};
			for (int s = 0; s < Mask::Storeys; ++s) {
				for (int i = 0; i < 256; ++i) {
					xs[s].colors[i] = static_cast<unsigned char>(Mask::Tall + s);
				}
			}
			return xs;
		}();
		static const Xform_palette roof_clear = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = Mask::Clear;
			}
			return x;
		}();
		// Exterior ground-wall FACE (viewer outside, wall under its roof): its
		// sprite renders up-screen of its foot, so as a CLEAR pixel it z-blind
		// samples the outdoor cells hanging over it (a window's fan lit the
		// perpendicular wall around the corner).  Splat_radial_light resolves
		// Shell_face by the wall's own foot-ward cell instead.
		static const Xform_palette roof_face = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = Mask::Shell_face;
			}
			return x;
		}();
		// Kind stamps for a slab edge painted as facade: Shell_face with a Ground
		// kind is the PANE rule (window glass, lit by whatever is behind it), so
		// the edge has to read as a wall FACE.  Floors stamp no kind of their own.
		static const Xform_palette kind_wall_face = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = Kind::Wall_face;
			}
			return x;
		}();
		static const Xform_palette kind_ground = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = Kind::Ground;
			}
			return x;
		}();
		// A floor slab's south/right THICKNESS strip (Slab_edge + storey): the band
		// the clipped top paint leaves over.  It is the deck's own edge, so a spill
		// fan must cross it (a plain Tall breaks the glow with a strip-wide dark
		// band), but it must stay dark under a room's veto light, which a Tall +
		// storey mark would not.  No geometric test identifies a deck reliably --
		// ramparts carry merlons above and passages below -- so the strip is
		// labelled.  It carries its STOREY for the same reason the slab surface
		// does: a storey-less mark let a ground spill light the edge of a slab a
		// storey up.
		static const std::array<Xform_palette, Mask::Storeys> roof_strip = [] {
			std::array<Xform_palette, Mask::Storeys> a{};
			for (int s = 0; s < Mask::Storeys; ++s) {
				for (int i = 0; i < 256; ++i) {
					a[s].colors[i] = static_cast<unsigned char>(Mask::Slab_edge + s);
				}
			}
			return a;
		}();
		// A storey's floor SLAB surface (Slab + storey).  Distinct from the Tall
		// mark an exterior upper-storey wall carries: a slab is a
		// room's ceiling seen from beneath, so a light on a lower storey must
		// never reach it, while an outdoor light must light an exterior wall
		// whatever storey it belongs to.
		static const std::array<Xform_palette, Mask::Storeys> roof_slab = [] {
			std::array<Xform_palette, Mask::Storeys> a{};
			for (int s = 0; s < Mask::Storeys; ++s) {
				for (int i = 0; i < 256; ++i) {
					a[s].colors[i] = static_cast<unsigned char>(Mask::Slab + s);
				}
			}
			return a;
		}();
		// Surface-identity channels (face design §4.1): the topmost sprite at a
		// pixel owns its KIND; wall faces and objects also record their per-tile
		// foot offset so the splat can sample the owner's cell instead of the
		// cells the sprite overlaps.  Viewer-independent (R5).
		// A loose item resting on an upper storey owns no surface of its own: the
		// branches below leave the mark beneath it alone.  It must inherit the KIND
		// and FOOT channels for the same reason -- stamping them while the mask
		// stays the slab's pairs one object's mask with another's owner cell, and
		// the splat then lights the item's silhouette at that cell's flat alpha.
		const bool inherits_mark = obj->is_dragable() && obj->get_lift() >= 5;
		if (light_kind_mask && !light_foot_dx.empty() && !inherits_mark) {
			const Shape_info& sinf = obj->get_info();
			int               kind = Kind::Ground;
			bool              pane = false;
			// A loose item is an OBJECT whatever its solid flag says.  A non-solid
			// one fell through to Kind::Ground and drank the z-blind field at
			// its own screen position -- bright over the wall it lies against --
			// while its SOLID neighbours on the same surface stay objects and render
			// correctly.
			const bool loose_item = obj->is_dragable();
			// Panes and open door leaves stay GROUND: they glow with the light
			// behind them.
			const bool pane_shape
					= (sinf.is_door() && !obj->is_closed_door()) || NaturalLight::Object_passes_light(obj);
			// A shutter leaf is NON-solid, so the solid gate skipped the whole
			// block and it stamped no FEET at all -- every one of its pixels kept
			// the previous sprite's, and a neighbour's object unit stamp handed
			// half the leaf one far-away anchor cell.  That is the hard-edged band
			// down a shutter, at the boundary of the other sprite's box.
			if ((sinf.is_solid() || loose_item || pane_shape) && !sinf.is_floor() && !sinf.is_roof()) {
				// They still need FEET: their glow belongs to the light at their
				// own tile, not to whatever their sprite hangs over up-screen.
				if (pane_shape) {
					pane = true;
				} else {
					kind = NaturalLight::Object_is_wall_face(obj) ? Kind::Wall_face : Kind::Object;
				}
			}
			if (kind == Kind::Ground && !pane) {
				frame->paint_rle_transformed(light_kind_mask, sx, sy, kind_ground);
			} else {
				// Per-pixel foot offsets vary across the sprite, so rasterize its
				// coverage into a scratch buffer and write the channels by hand.
				const int                             fw = frame->get_width();
				const int                             fh = frame->get_height();
				static std::unique_ptr<Image_buffer8> scratch;
				if (!scratch || static_cast<int>(scratch->get_width()) < fw || static_cast<int>(scratch->get_height()) < fh) {
					const int sw = std::max(fw, scratch ? static_cast<int>(scratch->get_width()) : 0);
					const int sh = std::max(fh, scratch ? static_cast<int>(scratch->get_height()) : 0);
					scratch      = std::make_unique<Image_buffer8>(sw, sh);
				}
				scratch->fill8(0, fw, fh, 0, 0);
				frame->paint_rle_transformed(scratch.get(), frame->get_xleft(), frame->get_yabove(), roof_set);
				unsigned char*       kbits = light_kind_mask->get_bits();
				const int            klw   = static_cast<int>(light_kind_mask->get_line_width());
				const int            W     = static_cast<int>(light_kind_mask->get_width());
				const int            H     = static_cast<int>(light_kind_mask->get_height());
				const unsigned char* sbits = scratch->get_bits();
				const int            slw   = static_cast<int>(scratch->get_line_width());
				// Hot spot = anchor tile's bottom-right pixel shifted up-left by
				// 4px per z (Get_shape_location); undo the lift for the z=0 foot.
				const int f0x = sx + 4 * obj->get_lift();
				const int f0y = sy + 4 * obj->get_lift();
				const int xts = sinf.get_3d_xtiles(obj->get_framenum());
				const int yts = sinf.get_3d_ytiles(obj->get_framenum());
				// Same footprint tile as the tile-centre snap would pick, but WITHOUT collapsing to its
				// centre: the splat's blend needs the pixel's position inside its
				// tile, and snapping threw it away, so a wall top rendered one flat
				// value per tile -- the staircase of lit blocks along a lit wall.
				// The phase cannot be recovered downstream: within a tile the snapped
				// offset runs a sawtooth whose origin is the sprite's own elevation,
				// so one offset cannot carry both the tile and the place in it.
				auto foot_clamp = [](int p, int f0, int nt) {
					const int lo = f0 - (nt - 1) * c_tilesize - (c_tilesize - 1);
					return p < lo ? lo : (p > f0 ? f0 : p);
				};
				// Elevated art is drawn up-LEFT 4px per z, so a face pixel stands
				// over a ground point DOWN-RIGHT of it along the diagonal.  Snapping
				// the raw screen position ignores that and hands a tall wall's whole
				// face to its far tile -- a 1x4 wall beside an open door read its
				// north end (arrival 14) where the art actually shows the south end
				// at the doorway (arrival 9), which is the seam down an inside wall
				// face.  A wall's THIN axis pins the shear: its ground strip is one
				// tile wide, so the offset from that edge IS the elevation.
				// The cap is how far up-left the art may sit from f0, and f0 is the
				// z=0 anchor, so it spans the LIFT as well as the shape's height.
				// At 4*height alone a raised sprite's pixels never shear back as far
				// as their own footprint and foot_clamp pinned the whole sprite to
				// its north-west tile: a 1x4 sill read one cell for all four tiles,
				// so a run of them stepped 155 / 71 / 49 between sprites instead of
				// fading along the wall.  (Invisible until wall tops began reading
				// their foot at all -- before that they were on the z-blind field.)
				const int zshear = 4 * (obj->get_lift() + sinf.get_3d_height());
				// A wall BLOCK's x foot is left unclamped: clamping to its own
				// footprint pinned the art west of it to one corner, a hard seam
				// at every block boundary.
				const bool box_foot = kind == Kind::Wall_face && xts > 1 && yts > 1;
				auto      shear  = [&](int px, int py) {
	                int s = 0;
	                if (xts == 1) {
	                    s = f0x - c_tilesize / 2 - px;
	                } else if (yts == 1) {
	                    s = f0y - c_tilesize / 2 - py;
	                } else {
	                    return 0;
	                }
	                return s < 0 ? 0 : (s > zshear ? zshear : s);
				};
				const int x0 = sx - frame->get_xleft();
				const int y0 = sy - frame->get_yabove();
				// An object's flat TOP (its box's top rectangle on screen) stamps
				// Kind::Object_top: a spill walked over the deck lights it while the faces
				// turned from the light stay dark.
				const int ztop = obj->get_lift() + sinf.get_3d_height();
				// Only art that fills its box has a top there: a tree crown spills far
				// past its 1x1 trunk box, whose top rect lands mid-crown.
				const int  zh      = sinf.get_3d_height();
				const bool boxlike = fw <= xts * c_tilesize + 4 * zh + 4 && fh <= yts * c_tilesize + 4 * zh + 4;
				const bool obj_top = kind == Kind::Object && zh > 0 && boxlike;
				const int         top_x1     = f0x - 4 * ztop;
				const int         top_y1     = f0y - 4 * ztop;
				const int         top_x0     = top_x1 - xts * c_tilesize + 1;
				const int         top_y0     = top_y1 - yts * c_tilesize + 1;
				// Unshear a top only on a wall block's walkway, whose own top reads
				// unsheared; furniture on a floor keeps the sheared foot of its room.
				bool on_block = false;
				if (obj_top && obj->get_lift() > 0) {
					const Tile_coord t = obj->get_tile();
					const Tile_coord below(t.tx, t.ty, obj->get_lift() - 1);
					for (int cy = t.ty / c_tiles_per_chunk; cy <= (t.ty + 3) / c_tiles_per_chunk && !on_block; ++cy) {
						for (int cx = t.tx / c_tiles_per_chunk; cx <= (t.tx + 3) / c_tiles_per_chunk && !on_block; ++cx) {
							Map_chunk* const ch = map->get_chunk_safely(cx, cy);
							if (ch == nullptr) {
								continue;
							}
							Object_iterator it(ch->get_objects());
							Game_object*    b;
							while ((b = it.get_next()) != nullptr) {
								if (b == obj || !b->blocks(below)) {
									continue;
								}
								const Shape_info& binf = b->get_info();
								on_block = binf.get_3d_xtiles(b->get_framenum()) > 1 && binf.get_3d_ytiles(b->get_framenum()) > 1
										   && NaturalLight::Object_is_wall_face(b);
								break;
							}
						}
					}
				}
				for (int y = 0; y < fh; ++y) {
					const int py = y0 + y;
					if (py < 0 || py >= H) {
						continue;
					}
					const unsigned char* srow = sbits + static_cast<size_t>(y) * slw;
					for (int x = 0; x < fw; ++x) {
						if (srow[x] == 0) {
							continue;
						}
						const int px = x0 + x;
						if (px < 0 || px >= W) {
							continue;
						}
						const int s = shear(px, py);
						// Per-tile feet even on an OBJECT: pinning it to its anchor
						// hands a half wall one flat value with no falloff.
						int        ddx     = box_foot ? 0 : foot_clamp(px + s, f0x, xts) - px;
						int        ddy     = foot_clamp(py + s, f0y, yts) - py;
						const bool top_pix = obj_top && px >= top_x0 && px <= top_x1 && py >= top_y0 && py <= top_y1;
						// A top reads unsheared like a wall block's top (box_foot), or its
						// light lands 4px per z west of the deck's.
						if (top_pix && on_block) {
							ddx = 0;
							ddy = foot_clamp(py, f0y, yts) - py;
						}
						ddx = std::clamp(ddx, -Foot_bias, Foot_bias - 1);
						ddy = std::clamp(ddy, -Foot_bias, Foot_bias - 1);
						kbits[static_cast<size_t>(py) * klw + px]
								= static_cast<unsigned char>(top_pix ? Kind::Object_top : kind);
						light_foot_dx[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(ddx + Foot_bias);
						light_foot_dy[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(ddy + Foot_bias);
					}
				}
			}
		}
		// An object standing ON a roof must stay as dark as the roof under it.
		// "Roof level" is the render-skip lift while inside (NOT a hardcoded z 5:
		// a tall room's visible upper wall segments are walls, not roofs) and the
		// classic z 5 while outside.  Inside, an is_roof() shape only counts when
		// ABOVE the Avatar: some interiors use roof shapes as flooring.  Ground
		// objects merely drawn in front of a roof still clear their pixels.
		const bool inside = gwin->is_main_actor_inside();
		// True when nothing is drawn above this object's top -- it stands under
		// open sky, so a spill / exterior light may treat it as a whole unit.
		// Structural cover only, for the same reason faces_exterior scans objects:
		// the chunk blocked bitmap counts ACTORS and loose items, so a crate set
		// down on a rampart (or an NPC crossing it) flipped the pieces around it
		// from open-sky whole units to covered shell faces (Shell_face / Tall),
		// which the spill branch vetoes -- a black patch far wider than the sprite.
		auto static_cover_above = [&](int tx, int ty, int z) {
			tx                  = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			ty                  = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch = map->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
			if (ch == nullptr) {
				return false;
			}
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				if (o == obj || o->as_actor() != nullptr || o->is_dragable()) {
					continue;
				}
				const Shape_info& oi = o->get_info();
				if (!oi.is_solid()) {
					continue;
				}
				if (o->get_lift() + oi.get_3d_height() <= z) {
					continue;
				}
				if (o->get_footprint().has_world_point(tx, ty)) {
					return true;
				}
			}
			return false;
		};
		auto open_sky_above = [&](int top) {
			const Tile_coord t = obj->get_tile();
			return !static_cover_above(t.tx, t.ty, top);
		};
		// Is this absolute tile under a roof ABOVE `z`?  The engine's own roof map
		// -- unlike the blocked-column probes it is not fooled by walls standing on
		// a deck, and it registers a ceiling however that ceiling is built.
		auto tile_roofed_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			return ch != nullptr && ch->is_roof(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z) < 31;
		};
		// Lowest thing blocking this tile's column at or above `z` (>= 31: none).
		// is_roof() starts its search 4 ABOVE the lift it is handed, so asking it
		// about a first-floor wall's own lift steps over that room's ceiling.
		auto lowest_cover_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			return ch != nullptr ? ch->is_roof(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z - 4) : 255;
		};
		// Does a wall-line piece sit EXACTLY on `z`, continuing this column up?
		// The window capping a half wall is light-passing and non-blocking, so it
		// never enters the blocked map lowest_cover_at reads.
		auto column_continued_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (ch == nullptr) {
				return false;
			}
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				if (o == obj || o->as_actor() != nullptr || o->is_dragable()) {
					continue;
				}
				const Shape_info& oi = o->get_info();
				if (oi.is_floor() || oi.is_roof() || oi.get_3d_height() <= 0) {
					continue;
				}
				if (o->get_lift() == z && o->get_footprint().has_world_point(wtx, wty)) {
					return true;
				}
			}
			return false;
		};
		// Is there an OPENING (window glass, grate, open shutter) in the course
		// just above `z`?  A half wall's top is a sill: light reaches it only
		// through whatever caps it, so a closed shutter leaves it dark.
		auto opening_above_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (ch == nullptr) {
				return false;
			}
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				if (o == obj || o->as_actor() != nullptr) {
					continue;
				}
				const int olo = o->get_lift();
				const int ohi = olo + std::max(o->get_info().get_3d_height(), 1);
				if (ohi <= z || olo >= z + 5) {
					continue;    // Not in the course resting on this top.
				}
				if (NaturalLight::Object_passes_light(o) && o->get_footprint().has_world_point(wtx, wty)) {
					return true;
				}
			}
			return false;
		};
		// Solid support from the ground to the object's lift (a crenellation
		// capping a rampart): it lights with the wall beneath as one whole unit.
		// A gap below (a deck object -- the room's air under the floor-roof)
		// makes it belong to its storey instead.  ANY tile of the footprint with a
		// full column counts: a piece straddling the wall's outer edge still rests
		// on the wall, and judging it by the anchor tile alone marked it Tall +
		// storey, which the spill branch vetoes.
		auto solid_below = [&]() {
			const TileRect ft   = obj->get_footprint();
			const int      lift = obj->get_lift();
			if (lift <= 0) {
				return true;
			}
			for (int fy = ft.y; fy < ft.y + ft.h; ++fy) {
				for (int fx = ft.x; fx < ft.x + ft.w; ++fx) {
					const int        wtx = ((fx % c_num_tiles) + c_num_tiles) % c_num_tiles;
					const int        wty = ((fy % c_num_tiles) + c_num_tiles) % c_num_tiles;
					Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
					if (ch == nullptr) {
						continue;
					}
					bool full = true;
					for (int z = 0; z < lift && full; ++z) {
						full = ch->is_tile_occupied(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z);
					}
					if (full) {
						return true;
					}
				}
			}
			return false;
		};
		// A floor-roof / roof piece right at the object's base: a deck object
		// even where the wall line below it is solid to the ground.
		auto floor_at_base = [&]() {
			const Tile_coord t     = obj->get_tile();
			Map_chunk* const chunk = map->get_chunk_safely(t.tx / c_tiles_per_chunk, t.ty / c_tiles_per_chunk);
			if (chunk == nullptr) {
				return false;
			}
			Object_iterator it(chunk->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				if (o == obj || (!o->get_info().is_floor() && !o->get_info().is_roof())) {
					continue;
				}
				const int base = o->get_lift() + o->get_info().get_3d_height();
				if (base < obj->get_lift() - 1 || base > obj->get_lift()) {
					continue;
				}
				if (o->get_footprint().has_world_point(t.tx, t.ty)) {
					return true;
				}
			}
			return false;
		};
		// Which face of this piece the camera shows: an E-W piece its SOUTH
		// face, a N-S piece its EAST (or narrow WEST-shell edge) face, a square
		// piece per the wall line running through it.  That face is EXTERIOR
		// when the tile column 1-2 tiles out on that side is not this
		// building's roofed walkable interior -- open sky OR another
		// structure's solid mass (the city wall beside the east facade) both
		// count as outdoors.  Interior walls' camera sides are roofed rooms.
		auto faces_exterior = [&](bool doors_anchor = true, bool* out_south_in = nullptr, bool* out_east_in = nullptr,
								  uint32_t* out_south_bits = nullptr, uint32_t* out_east_bits = nullptr,
								  uint32_t* out_south_open = nullptr, uint32_t* out_east_open = nullptr) {
			const TileRect ft = obj->get_footprint();
			if (out_south_bits != nullptr) {
				*out_south_bits = 0;
			}
			if (out_east_bits != nullptr) {
				*out_east_bits = 0;
			}
			if (out_south_open != nullptr) {
				*out_south_open = 0;
			}
			if (out_east_open != nullptr) {
				*out_east_open = 0;
			}
			if (out_south_in != nullptr) {
				*out_south_in = false;
			}
			if (out_east_in != nullptr) {
				*out_east_in = false;
			}
			// The chunk blocked bitmap includes ACTORS: bitmap tests made the
			// shell classification flicker as NPCs (or the viewer) walked by.
			// Scan the chunk's IMMOVABLE solids instead.
			auto static_solid_at = [&](int tx, int ty, int z_lo, int z_hi) {
				tx                  = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
				ty                  = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
				Map_chunk* const ch = map->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
				if (ch == nullptr) {
					return false;
				}
				Object_iterator it(ch->get_objects());
				Game_object*    o;
				while ((o = it.get_next()) != nullptr) {
					if (o == obj || o->as_actor() != nullptr || o->is_dragable()) {
						continue;
					}
					const Shape_info& oi = o->get_info();
					if (!oi.is_solid()) {
						continue;
					}
					const int lo = o->get_lift();
					if (lo > z_hi || lo + oi.get_3d_height() <= z_lo) {
						continue;
					}
					if (o->get_footprint().has_world_point(tx, ty)) {
						return true;
					}
				}
				return false;
			};
			// Probe outward from a face: open sky right beyond = exterior; a
			// solid column right beyond = the face is PRESSED against another
			// mass (a desk against the room wall, a zero-gap city wall) and is
			// hidden -- never shell; a roofed walkable strip (under an eave) is
			// exterior only when open sky follows it.
			auto open_sky_tile = [&](int px, int py) {
				return !static_cover_above(px, py, 5);
			};
			auto probe_open = [&](int tx, int ty, int dx, int dy) {
				if (open_sky_tile(tx, ty)) {
					return true;
				}
				if (static_solid_at(tx, ty, 1, 1) && static_solid_at(tx, ty, 4, 4)) {
					return false;
				}
				return open_sky_tile(tx + dx, ty + dy);
			};
			auto south_open = [&]() {
				for (int i = 0; i < ft.w; ++i) {
					if (probe_open(ft.x + i, ft.y + ft.h, 0, 1)) {
						return true;
					}
				}
				return false;
			};
			auto east_open = [&]() {
				for (int i = 0; i < ft.h; ++i) {
					if (probe_open(ft.x + ft.w, ft.y + i, 1, 0)) {
						return true;
					}
				}
				return false;
			};
			// A door anchored on/next to the tile: a DOORWAY continues the wall
			// line even though its tile is unoccupied (open leaf swung aside) --
			// the stub flanking an entrance is shell, not furniture.
			auto door_at = [&](int tx, int ty) {
				const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
				const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
				Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
				if (ch == nullptr) {
					return false;
				}
				Object_iterator it(ch->get_objects());
				Game_object*    o;
				while ((o = it.get_next()) != nullptr) {
					if (!o->get_info().is_door()) {
						continue;
					}
					const Tile_coord t = o->get_tile();
					if (std::abs(Tile_coord::delta(t.tx, wtx)) <= 1 && std::abs(Tile_coord::delta(t.ty, wty)) <= 1) {
						return true;
					}
				}
				return false;
			};
			// An immovable solid at mid-wall height: the wall line continues.
			auto wall_at = [&](int tx, int ty) {
				return static_solid_at(tx, ty, obj->get_lift() + 2, obj->get_lift() + 2);
			};
			// Is the space beyond a face a ROOM?  Walkable (no static solid) and
			// roofed (not open sky).  This is a POSITIVE test: "not the exterior
			// face" is not the same as interior -- a wall line's own continuation
			// is solid mass and belongs to neither side.  Only a positively
			// interior face may drop the shell mask.
			auto face_interior = [&](int tx, int ty) {
				return !static_solid_at(tx, ty, obj->get_lift() + 2, obj->get_lift() + 2) && !open_sky_tile(tx, ty);
			};
			// The mirror test: walkable and UNROOFED, i.e. the face looks at the
			// street.  Distinct from "not interior", which also covers a face
			// buried against solid mass -- and those two must not be treated alike.
			auto face_outdoor = [&](int tx, int ty) {
				return !static_solid_at(tx, ty, obj->get_lift() + 2, obj->get_lift() + 2) && open_sky_tile(tx, ty);
			};
			bool south_in = false;
			for (int i = 0; i < ft.w; ++i) {
				if (face_interior(ft.x + i, ft.y + ft.h)) {
					south_in = true;
					if (out_south_bits != nullptr && i < 32) {
						*out_south_bits |= 1u << i;
					}
				} else if (out_south_open != nullptr && i < 32 && face_outdoor(ft.x + i, ft.y + ft.h)) {
					*out_south_open |= 1u << i;
				}
			}
			bool east_in = false;
			for (int i = 0; i < ft.h; ++i) {
				if (face_interior(ft.x + ft.w, ft.y + i)) {
					east_in = true;
					if (out_east_bits != nullptr && i < 32) {
						*out_east_bits |= 1u << i;
					}
				} else if (out_east_open != nullptr && i < 32 && face_outdoor(ft.x + ft.w, ft.y + i)) {
					*out_east_open |= 1u << i;
				}
			}
			if (out_south_in != nullptr) {
				*out_south_in = south_in;
			}
			if (out_east_in != nullptr) {
				*out_east_in = east_in;
			}
			auto decide = [&]() -> bool {
				const int mx = ft.x + ft.w / 2;
				const int my = ft.y + ft.h / 2;
				// A low piece sitting IN the doorway path (door within a tile of its
				// own body) is the fan's threshold, not shell trim.
				if (!doors_anchor && (door_at(mx, my) || door_at(ft.x, ft.y) || door_at(ft.x + ft.w - 1, ft.y + ft.h - 1))) {
					return false;
				}
				const bool wn = wall_at(mx, ft.y - 1) || (doors_anchor && door_at(mx, ft.y - 1));
				const bool ws = wall_at(mx, ft.y + ft.h) || (doors_anchor && door_at(mx, ft.y + ft.h));
				const bool ww = wall_at(ft.x - 1, my) || (doors_anchor && door_at(ft.x - 1, my));
				const bool we = wall_at(ft.x + ft.w, my) || (doors_anchor && door_at(ft.x + ft.w, my));
				if (ft.w > ft.h) {
					// E-W pieces show their SOUTH face along the run; the piece at the
					// run's EAST END also presents an east face to whatever lies
					// beyond it.  Only a wall LINE member is shell (a freestanding
					// desk classifies as wall face too).
					return (ww || we) && (south_open() || east_open());
				}
				auto ns_shell = [&]() {
					// N-S LINE members only; shell iff a visible face adjoins the
					// outdoors.  A run shows its EAST face along its length and its
					// SOUTH face at the end tile -- testing only the east face left
					// the south end of every N-S wall unmasked.  A face on a roofed
					// walkable space (room, porch, passage) is INTERIOR: lit by
					// whatever light reaches that space, and paint_shell_face clears
					// that band again.
					return (wn || ws) && (east_open() || south_open());
				};
				if (ft.h > ft.w) {
					return ns_shell();
				}
				// Square piece: classify by the wall line running through it.  NE/NW
				// corners show the room their S/E faces -- interior -- and stay clear.
				if (wn && ws) {
					return ns_shell();
				}
				if (ww && we) {
					return south_open();    // South wall line.
				}
				if (wn && ww) {
					return south_open() || east_open();    // SE corner.
				}
				if (wn && we) {
					return south_open();    // SW corner.
				}
				// Single-anchor stubs: line membership is along the FACE being
				// stamped -- W/E anchors carry a south face, N/S anchors an east
				// face.  A piece whose only wall neighbour is BEHIND its face (a
				// desk against a wall) is furniture, not shell.  Whether the face
				// itself looks into a room is answered by face_interior, not by
				// probing the far side: a door jamb has the threshold to its south
				// yet its east face is plain outdoors.
				if ((ww || we) && south_open()) {
					return true;
				}
				return (wn || ws) && east_open() && !east_in;
			};
			const bool fext = decide();
			return fext;
		};
		// A placed light source does NOT exempt a wall: mask state is pure
		// geometry (interior faces light with their space whether the sconce
		// on them burns or not).
		bool roof_like;
		bool tall_exterior = false;
		// Storey a Tall mark belongs to: a
		// spill lights it only from the same or a higher storey.
		auto storey_of = [](int lift) {
			return std::clamp(lift / 5, 0, Mask::Storeys - 1);
		};
		int  tall_storey   = 0;
		bool     shell_s_in   = false;
		bool     shell_e_in   = false;
		uint32_t shell_s_bits = 0;
		uint32_t shell_e_bits = 0;
		uint32_t shell_s_open = 0;
		uint32_t shell_e_open = 0;
		// With the roof hidden, every upper-storey surface on screen is an INTERIOR
		// one (Slab): only its own storey may light it, so a ground-floor
		// torch cannot wash the floor above.  From outside the same surfaces are the
		// building's exterior -- facade and walkable floor-roof deck -- and an
		// outdoor light must light them to full height (Tall).
		const bool upper_interior = inside;
		// Paint a shell wall: whole sprite Shell_face; with `lit_top` (inside view) its
		// flat TOP band (sprite minus the 4px-per-z bottom/right face strip)
		// stays CLEAR so the room's field washes it like the interior walls'
		// tops -- the faces stay dark wall.  (A Tall top mark fails the
		// room's own storey gate and would stay dark.)
		auto paint_shell_face = [&](bool lit_top, uint32_t south_bits = 0, uint32_t east_bits = 0, bool shell = true,
									bool mark_top = true, uint32_t south_open = 0, uint32_t east_open = 0,
									int top_mark = Mask::Shell_top, bool ext_corner = false) {
			frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_face);
			if (!lit_top) {
				return;
			}
			const int strip = 4 * obj->get_info().get_3d_height();
			const int bx    = sx - frame->get_xleft();
			const int by    = sy - frame->get_yabove();
			const int bw    = frame->get_width();
			const int bh    = frame->get_height();
			// Interior-ness is per TILE, not per sprite: a 4-tile wall can face
			// the room along most of its run and another wall at its end.  Undo
			// the elevation shear to find which footprint tile a pixel shows (the
			// same inversion the foot stamp uses), then ask that tile.
			const int f0x  = sx + 4 * obj->get_lift();
			const int f0y  = sy + 4 * obj->get_lift();
			const int xts  = obj->get_info().get_3d_xtiles(obj->get_framenum());
			const int yts  = obj->get_info().get_3d_ytiles(obj->get_framenum());
			const int zsh  = strip;
			auto      tidx = [&](int px, int py, bool along_y) {
	            int s = 0;
	            if (xts == 1) {
	                s = f0x - c_tilesize / 2 - px;
	            } else if (yts == 1) {
	                s = f0y - c_tilesize / 2 - py;
	            }
	            s              = s < 0 ? 0 : (s > zsh ? zsh : s);
	            const int n    = along_y ? yts : xts;
	            int       from = along_y ? ((f0y - (py + s)) >> 3) : ((f0x - (px + s)) >> 3);
	            from           = from < 0 ? 0 : (from >= n ? n - 1 : from);
	            return n - 1 - from;    // Footprint index, counted from its origin.
			};
			// The three surfaces a wall sprite draws -- flat top, south face, east
			// face -- have OVERLAPPING bounding boxes: both faces are sheared 4px
			// per z, so the south face's foot end lies inside the east band's
			// columns.  Clearing an interior face by rectangle therefore erased
			// the other face too.  The faces separate along the diagonal
			// (u - v) = bw - bh; the top keeps its rectangle (it overlaps the
			// faces by a single row/column).
			static std::unique_ptr<Image_buffer8> fscratch;
			if (!fscratch || static_cast<int>(fscratch->get_width()) < bw || static_cast<int>(fscratch->get_height()) < bh) {
				const int fsw = std::max(bw, fscratch ? static_cast<int>(fscratch->get_width()) : 0);
				const int fsh = std::max(bh, fscratch ? static_cast<int>(fscratch->get_height()) : 0);
				fscratch      = std::make_unique<Image_buffer8>(fsw, fsh);
			}
			// Rasterized coverage: wall sprites overlap, so only pixels this shape
			// owns may be cleared.
			fscratch->fill8(0, bw, bh, 0, 0);
			frame->paint_rle_transformed(fscratch.get(), frame->get_xleft(), frame->get_yabove(), roof_set);
			unsigned char*       mbits = roof_light_mask->get_bits();
			const int            mlw   = static_cast<int>(roof_light_mask->get_line_width());
			const int            mw    = static_cast<int>(roof_light_mask->get_width());
			const int            mh    = static_cast<int>(roof_light_mask->get_height());
			const unsigned char* fbits = fscratch->get_bits();
			const int            flw   = static_cast<int>(fscratch->get_line_width());
			const int            diag  = bw - bh;
			for (int v = 0; v < bh; ++v) {
				const int py = by + v;
				if (py < 0 || py >= mh) {
					continue;
				}
				const unsigned char* frow = fbits + static_cast<size_t>(v) * flw;
				for (int u = 0; u < bw; ++u) {
					if (frow[u] == 0) {
						continue;
					}
					const int px = bx + u;
					if (px < 0 || px >= mw) {
						continue;
					}
					const bool is_top = u < bw - strip && v < bh - strip;
					if (is_top && mark_top) {
						// Shell_top = shell wall TOP: horizontal, so it has no viewer
						// side.  Left CLEAR it read as an interior face and took
						// the viewer-gated arrivals, which for an outside light
						// seen from inside collapse to the wrap-around path -- dark
						// tops over a lit face.  An UPPER-storey piece passes its
						// storey mark instead: having no viewer side, Shell_top lights
						// from anywhere, and a window sill one floor up glowed with
						// the campfire on the far side of its wall.
						mbits[static_cast<size_t>(py) * mlw + px] = static_cast<unsigned char>(top_mark);
					} else {
						// Face_south / Face_east = interior-facing wall face, lit only from the
						// room on its SOUTH / EAST side.  Which that is, is known
						// here and nowhere else; left as a plain 0 the splat had to
						// guess it from the avatar's position, and the guess flipped
						// mid-wall and punched dark blobs at the flip.
						// A triangle whose OWN side has no room (the sprite's end cap,
						// or a face buried against the next wall) takes the side that
						// does: marking it for a non-room side sent the splat walking
						// along the wall until it surfaced outdoors, which lit the
						// inside faces of a shut cabin from the street.  But only if it
						// is BURIED -- a face looking at the street is a real exterior
						// face and keeps Shell_face, or the outside south face of a wall whose
						// east side is a room lit up with that room's light.
						const bool south      = u - v < diag;
						const int  js         = tidx(px, py, false);
						const int  je         = tidx(px, py, true);
						const bool s_room     = js < 32 && (south_bits & (1u << js)) != 0;
						const bool e_room     = je < 32 && (east_bits & (1u << je)) != 0;
						const bool own_room   = south ? s_room : e_room;
						const bool own_open   = south ? (js < 32 && (south_open & (1u << js)) != 0)
													  : (je < 32 && (east_open & (1u << je)) != 0);
						const bool other_room = south ? e_room : s_room;
						const int  face_own   = south ? Mask::Face_south : Mask::Face_east;
						const int  face_other = south ? Mask::Face_east : Mask::Face_south;
						int        side_mark  = -1;
						if (own_room) {
							side_mark = face_own;
						} else if (own_open) {
							side_mark = -1;    // Faces the street: the Shell_face mask stands.
						} else if (other_room) {
							side_mark = face_other;
						} else if (!shell && !ext_corner) {
							// An interior corner walled in on BOTH sides borrows the
							// nearest room through the splat's side walk; left on Shell_face it
							// lit from outside, and cleared it was the masked sliver.
							// South first: the walk falls back to east by itself, and
							// going east first can run along the wall out into the street.
							// NOT when the piece's far side fronts OPEN AIR: then it is
							// the building's OUTER corner, buried only against its own
							// neighbours, and borrowing a room let the hall's torches
							// light the outside of the wall.
							side_mark = Mask::Face_south;
						}
						if (side_mark > 0) {
							mbits[static_cast<size_t>(py) * mlw + px] = static_cast<unsigned char>(side_mark);
						}
					}
				}
			}
		};
		if (obj->get_info().is_floor()) {
			// A floor slab used as the storey below's ceiling: mark its flat TOP
			// Tall + storey (not Roof) so a deck-level spill still lights it while
			// a ground window's glow is storey-gated off.  Inside, the slab's
			// front-facing thickness strip (bottom / right 4px-per-z) is left as
			// painted -- marking it would draw a dark seam along the adjoining
			// wall.  Outside, the whole sprite first gets a plain Tall face mark,
			// or the room's veto light shows as a bright beam over the strip.
			const int zs         = obj->get_info().get_3d_height();
			const int top_storey = storey_of(obj->get_lift() + zs);
			const int strip      = 4 * zs;
			// Only an ELEVATED slab (top reaching z 5+) is a storey's ceiling /
			// deck.  Ground-level flooring (a stone floor, a paved yard) is just
			// floor: clear its pixels so the room's light brushes it normally.
			if (obj->get_lift() + zs < 5) {
				frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_clear);
				return;
			}
			// An upper floor with nothing ROOFING it is an outdoor deck, not this
			// room's ceiling, even while the viewer is inside: keep it Tall + storey
			// so only its own storey's lights and spills reach it.  Marked Slab +
			// storey it was an "interior" slab, and a ground lamp in a tall hall lit
			// it from underneath -- the z-blind field carried the fill along the
			// walkway beneath the deck and up through it.
			// Asked of the chunk's own roof map ABOVE the slab's top: a wall
			// standing ON the deck is not a roof (which open_sky_above counted), a
			// hall's ceiling registers here even when it is the storey above's floor,
			// and the deck cannot roof itself.
			// Asked of EVERY footprint tile, not just the anchor: a 1014 floor is
			// several tiles and one query for the lot let a slab whose far end lies
			// under a hall roof call its exposed edge interior too -- one slab
			// bright beside its neighbour that answered the same question with
			// "deck".  Any tile open to the sky makes the whole piece a deck.
			const bool        slab_roofed = [&] {
	            const TileRect fp = obj->get_footprint();
	            const int      z  = obj->get_lift() + zs;
	            for (int dy = 0; dy < fp.h; ++dy) {
	                for (int dx = 0; dx < fp.w; ++dx) {
	                    const int wtx = (((fp.x + dx) % c_num_tiles) + c_num_tiles) % c_num_tiles;
	                    const int wty = (((fp.y + dy) % c_num_tiles) + c_num_tiles) % c_num_tiles;
	                    Map_chunk* const ch = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
	                    if (ch == nullptr) {
	                        return false;
	                    }
	                    const int rz = ch->is_roof(wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z);
	                    if (rz >= 31) {
	                        return false;
	                    }
	                }
	            }
	            return true;
			}();
			const bool           deck_outdoors = !slab_roofed;
			const bool           slab_interior = upper_interior && top_storey >= 1 && !deck_outdoors;
			const Xform_palette& slab_pal  = slab_interior ? roof_slab[top_storey] : roof_tall[top_storey];
			// Which way the thickness strip LOOKS decides it: the strip is the
			// slab's south/east edge, so it faces the tiles just beyond them.  Over
			// a stairwell hole those are still inside the room and the edge lights
			// with it; on the building's rim they are open ground, and that edge is
			// part of the facade -- Shell_face, dark to the room's light like the wall
			// below it.  Nothing in the splat can tell the two apart: same light,
			// storey, mask and delivery path.
			const TileRect slab_fp = obj->get_footprint();
			// At the slab's OWN height, not z=0: a deck ROOFS the ground beneath
			// it, so at ground level every deck neighbour reads as covered and no
			// deck edge ever classified as facade.  Above the deck, an outdoor
			// neighbour is open sky while a stairwell hole still has the hall's
			// roof over it.
			const bool s_edge = tile_roofed_at(slab_fp.x + slab_fp.w - 1, slab_fp.y + slab_fp.h, obj->get_lift() + zs);
			const bool e_edge = tile_roofed_at(slab_fp.x + slab_fp.w, slab_fp.y + slab_fp.h - 1, obj->get_lift() + zs);
			const bool     strip_exterior = !s_edge || !e_edge;
			// The strip always carries its own mark: it is the slab's DOWN-facing
			// edge and only a light below the slab's storey can see it, which the
			// splat cannot tell once the strip reads as the top.
			frame->paint_rle_transformed(roof_light_mask, sx, sy, strip_exterior ? roof_face : roof_strip[top_storey]);
			Image_buffer::ClipRectSave clipsave(roof_light_mask);
			const TileRect             top_rect(
	                sx - frame->get_xleft(), sy - frame->get_yabove(), frame->get_width() - strip, frame->get_height() - strip);
			const TileRect r = top_rect.intersect(clipsave.Rect());
			if (r.w > 0 && r.h > 0) {
				roof_light_mask->set_clip(r.x, r.y, r.w, r.h);
				frame->paint_rle_transformed(roof_light_mask, sx, sy, slab_pal);
			}
			if (strip_exterior && light_kind_mask) {
				// Whole sprite as wall face, then the TOP back to ground: only the
				// strip keeps Kind::Wall_face, so the deck surface still samples the field.
				frame->paint_rle_transformed(light_kind_mask, sx, sy, kind_wall_face);
				Image_buffer::ClipRectSave ksave(light_kind_mask);
				const TileRect             kr = top_rect.intersect(ksave.Rect());
				if (kr.w > 0 && kr.h > 0) {
					light_kind_mask->set_clip(kr.x, kr.y, kr.w, kr.h);
					frame->paint_rle_transformed(light_kind_mask, sx, sy, kind_ground);
				}
			}
			return;
		} else if (inside) {
			const int  av_lift       = main_actor ? main_actor->get_lift() : 0;
			const bool is_roof_shape = obj->get_info().is_roof() && obj->get_lift() > av_lift;
			roof_like                = is_roof_shape || obj->get_lift() >= gwin->get_render_skip_lift();
			const int top            = obj->get_lift() + obj->get_info().get_3d_height();
			if (is_roof_shape && open_sky_above(top)) {
				// A LOWER structure's exposed roof still drawn while inside (a
				// gate passage, a porch below the viewer's storey -- the viewer's
				// own ceiling is hidden, so it is never this).  As a Roof mark no
				// light may touch it and it cuts a dark notch into the glow the
				// upper windows wash down the facade; clear it so it samples the
				// fields like the walls around it.
				frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_clear);
				return;
			}
			if (roof_like && !is_roof_shape && open_sky_above(top)) {
				// An object standing on an open-sky deck (castle battlements,
				// roof crenellations): mark Tall + storey, so only its own
				// storey's lights and spills reach it -- a ground window's glow
				// must not paint the crenellations a storey above it.  A cap on
				// a solid wall with no floor-roof at its base (a rampart
				// crenellation) stays a whole unit with the wall instead.
				tall_exterior = true;
				if (!solid_below() || floor_at_base()) {
					tall_storey = storey_of(obj->get_lift());
				}
			} else if (!roof_like && top > gwin->get_render_skip_lift()) {
				// A grounded shape rising above the room's ceiling can only be an
				// exterior one (an outside tree); confirm open sky above its top
				// so nothing indoors is caught here.
				if (open_sky_above(top)) {
					tall_exterior = true;
				} else if (obj->get_lift() >= 5) {
					// Blocked above and starting at an upper storey: a
					// second-storey wall segment or upstairs furniture.  Loose
					// items (books on a shelf top at the storey ABOVE the room's
					// floor) get no mark of their own: their storey equals the
					// ceiling slab's and no cutoff can admit one without the
					// other -- they inherit the shelf/wall/floor mark beneath
					// and light with the room.
					if (inherits_mark) {
						return;
					}
					tall_exterior = true;
					tall_storey   = storey_of(obj->get_lift());
				} else if (!gwin->is_in_dungeon()) {
					// Covered, grounded and camera-facing the outdoors: the
					// building shell seen from inside -- same rules as the
					// outside view.  Wall faces and light-passing pieces are Shell_face
					// (dark under lights; panes take the glass rule), a CLOSED
					// door is shell like the wall it fills, an OPEN leaf stays
					// unmarked; anything else falls through clear.
					const bool shell = faces_exterior(
							true, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
					if (shell && obj->get_info().is_door() && !obj->is_closed_door()) {
						return;
					}
					// Pane rule, as in the ground and upper shell branches.
					if (NaturalLight::Object_passes_light(obj)) {
						paint_shell_face(false);
						return;
					}
					if ((shell || !obj->get_info().is_door())
						&& (NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj))) {
						// A wall facing no outdoors still has a flat TOP, and a top
						// has no viewer side: stamp Shell_top with both faces left clear.
						// A non-shell wall is interior all round, so every face looks
						// onto a room somewhere; ring_side_dist finds which.  Leaving
						// such a face on Shell_face made it all-sides and lit it from the
						// OUTSIDE, which lights a sealed room's walls.
						paint_shell_face(true, shell_s_bits, shell_e_bits, shell, true, shell_s_open, shell_e_open);
						return;
					}
				}
			} else if (!roof_like && obj->get_lift() >= 5) {
				// Upper-storey walls / furnishings below the render skip: mark
				// Tall + storey so z-blind lights from lower floors stay off them
				// while their own storey's lights still reach them.  Loose items
				// inherit the underlying mark (see above).
				if (inherits_mark) {
					return;
				}
				// An upper-storey wall fronting the open air is facade: send it
				// through the shell painter so it carries Shell_face and stays dark under
				// a room below, instead of a storey mark the slab exception washes.
				// The verdict comes from the ROOF MAP beyond its south/east faces,
				// not faces_exterior -- that reasons from ground-level geometry and
				// calls these walls interior, after which they read as Face_south/Face_east and
				// light from the room underneath, the same defect one storey up.
				// A course that another solid continues straight up is part of that
				// wall's face even though its own top falls short of the ceiling --
				// which is how the half-wall under a window is built.  Judged per
				// SHAPE, Object_is_wall_face calls it a crate on a deck, and the
				// splat then masks it out of the facade around it.
				// Only for pieces that test REJECTS: a wall already reaching the
				// ceiling has the roof itself continuing its column, and certifying
				// at the roof's height answered about the sky --
				// every interior wall then certified as facade and went black.
				// An open shutter LEAF keeps the PANE mark (Shell_face + Kind::Ground), the one
				// classification both lights reach: the indoor one because pane_px
				// skips the veto block, the outdoor one through the shell dome.
				const bool shape_face = NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj);
				const Tile_coord utile = obj->get_tile();
				const int        uwtop = obj->get_lift() + obj->get_info().get_3d_height();
				const bool       wall_column
						= !shape_face
						  && (lowest_cover_at(utile.tx, utile.ty, uwtop) == uwtop
							  || column_continued_at(utile.tx, utile.ty, uwtop));
				// Certify the two visible faces SEPARATELY, from the roof map at
				// this sprite's own height.  Handing both the shell's all-side
				// arrivals lit the inside face of a west wall -- the face the room
				// shows the camera -- from a campfire standing outside it.
				// faces_exterior cannot supply the bits: it reasons from
				// ground-level geometry and a storey up that answer is wrong.
				uint32_t          up_s_bits       = 0;
				uint32_t          up_e_bits       = 0;
				uint32_t          up_s_open       = 0;
				uint32_t          up_e_open       = 0;
				uint32_t          up_s_bur        = 0;
				uint32_t          up_e_bur        = 0;
				// Open air on the sides the camera CANNOT see.  Only the facade
				// verdict reads these: a west or north wall shows the room its
				// S/E sides front, so its top would never qualify as shell from
				// the two camera-facing sides alone.
				bool              up_far_open     = false;
				{
					const TileRect ufp = obj->get_footprint();
					const int      uz  = obj->get_lift();
					// 1 = a ROOM behind this face (a ceiling clears the wall's own
					// top), 0 = open air, -1 = buried against neighbouring mass,
					// which is neither and takes no bit.
					// A shelf against the wall is not mass: under a roof its tile is
					// still room.
					auto furniture_only = [&](int tx, int ty) {
	                    const int wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
	                    const int wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
	                    for (int cy = wty / c_tiles_per_chunk; cy <= (wty + 7) / c_tiles_per_chunk; ++cy) {
	                        for (int cx = wtx / c_tiles_per_chunk; cx <= (wtx + 7) / c_tiles_per_chunk; ++cx) {
	                            Map_chunk* const ch = map->get_chunk_safely(cx, cy);
	                            if (ch == nullptr) {
	                                continue;
	                            }
	                            Object_iterator it(ch->get_objects());
	                            Game_object*    o;
	                            while ((o = it.get_next()) != nullptr) {
	                                if (o == obj || o->as_actor() != nullptr || o->is_dragable()) {
	                                    continue;
	                                }
	                                const Shape_info& oi = o->get_info();
	                                if (!oi.is_solid() || o->get_lift() >= uwtop
	                                    || o->get_lift() + oi.get_3d_height() <= uz
	                                    || !o->get_footprint().has_world_point(wtx, wty)) {
	                                    continue;
	                                }
	                                if (oi.is_floor() || oi.is_roof() || oi.is_door()
	                                    || NaturalLight::Object_is_wall_face(o) || NaturalLight::Object_passes_light(o)) {
	                                    return false;
	                                }
	                            }
	                        }
	                    }
	                    return true;
					};
					auto side_of = [&](int tx, int ty) {
						const int b = lowest_cover_at(tx, ty, uz);
						if (b < uwtop && furniture_only(tx, ty) && tile_roofed_at(tx, ty, 0)) {
							return 1;
						}
						return b >= 31 ? 0 : (b >= uwtop ? 1 : -1);
					};
					for (int i = 0; i < ufp.w && i < 32; ++i) {
						const int s = side_of(ufp.x + i, ufp.y + ufp.h);
						if (s == 1) {
							up_s_bits |= 1u << i;
						} else if (s == 0) {
							up_s_open |= 1u << i;
						} else {
							up_s_bur |= 1u << i;
						}
					}
					for (int i = 0; i < ufp.h && i < 32; ++i) {
						const int s = side_of(ufp.x + ufp.w, ufp.y + i);
						if (s == 1) {
							up_e_bits |= 1u << i;
						} else if (s == 0) {
							up_e_open |= 1u << i;
						} else {
							up_e_bur |= 1u << i;
						}
					}
					// side_of alone is not enough here: it reads "nothing blocking
					// above uz", and a FLOOR SLAB ceiling sets no blocked flag, so an
					// ordinary interior tile answers open sky.  Require the neighbour
					// to be unroofed from the GROUND as well -- the standard outdoors
					// test -- or a sill's course certifies as facade.
					auto far_open_at = [&](int tx, int ty) {
						return side_of(tx, ty) == 0 && !tile_roofed_at(tx, ty, 0);
					};
					for (int i = 0; i < ufp.w && i < 32 && !up_far_open; ++i) {
						up_far_open = far_open_at(ufp.x + i, ufp.y - 1);
					}
					for (int i = 0; i < ufp.h && i < 32 && !up_far_open; ++i) {
						up_far_open = far_open_at(ufp.x - 1, ufp.y + i);
					}
					// A tile buried against neighbouring mass is neither room nor
					// street, but it still DRAWS, and uncertified it keeps the
					// facade default Shell_face -- black from inside under every light.
					// Most of a wall line reads buried (its own continuation), so
					// that default blackened whole walls.  One room tile anywhere
					// makes the piece interior masonry along its whole length.
					// With NO room tile it stays uncertified on purpose: buried on
					// every side is no face at all and belongs to the storey mark
					// below.  Calling it open air instead made a junction piece
					// facade, and the campfire outside lit it through the wall.
					if ((up_s_bits | up_e_bits) != 0) {
						up_s_bits |= up_s_bur;
						up_e_bits |= up_e_bur;
					}
				}
				// The GATE is whether either visible face got certified at all.
				// Buried on both sides is no face at all, so fall through to the storey mark.
				// `up_far_open` counts: a west or north wall buried against its own
				// neighbours on both visible sides has no S/E bit at all, and
				// without it those pieces fell through to the storey mark -- a
				// two-tile black gap mid-run where the wall happens to be split
				// into 1x1 sprites.
				const bool face_certified = (up_s_bits | up_e_bits | up_s_open | up_e_open) != 0 || up_far_open;
				if (!gwin->is_in_dungeon() && (shape_face || wall_column)
					&& (!obj->get_info().is_door() || obj->is_closed_door()) && face_certified) {
					// A PANE (open shutter, grate, glass) glows with whatever is
					// behind it, which the splat keys on Shell_face + Kind::Ground.  Certifying
					// its faces interior overwrote that with Face_east on the face band
					// and the storey top mark on the top band -- both dark to the
					// light outside, split down the middle of one sprite.
					if (NaturalLight::Object_passes_light(obj)) {
						paint_shell_face(false);
						return;
					}
					// A top fronting OPEN AIR on ANY side is the building's outer
					// shell and takes the ground shell's Shell_top.  All four sides
					// count: a west or north wall's exterior side is the one the
					// camera cannot see, and testing only S/E cut its top off
					// mid-run at the first corner piece that happened to face the
					// street.  Fronting rooms all round is an interior partition
					// and keeps the storey mark, so no light reaches its top
					// through the wall.
					const int         up_storey     = storey_of(obj->get_lift());
					// A piece carrying another course on its top (wall_column) shows
					// a SILL, not a crown: light reaches it only through whatever
					// caps it, so an open shutter or glass lights it and a closed
					// one leaves it on the storey mark.
					const bool        up_sill_lit
							= !wall_column || opening_above_at(utile.tx, utile.ty, uwtop);
					const bool        up_fronts_air = (up_s_open | up_e_open) != 0 || up_far_open;
					const bool        up_facade     = up_sill_lit && up_fronts_air;
					// A sill under a CLOSED shutter takes no light THROUGH the window,
					// but it is still exterior masonry facing the street.  Shell_face is lit by
					// an outdoor light and vetoed by an indoor one, which is exactly that
					// split; Shell_top would set top_px and light it from inside as well.
					// CAMERA-FACING sides only, unlike the top verdict above: a sill
					// is seen as a face, and a north or west one is on the building's
					// far side, where lighting it put a bright ledge the viewer
					// should never see.
					const bool        up_sill_shell = !up_facade && (up_s_open | up_e_open) != 0;
					// The FACES are a separate verdict from the top: a top has no
					// viewer side and may be shell from the far side, but a face
					// the camera sees is only shell where a CAMERA-FACING side
					// fronts open air.  Hardcoded true here, a triangle buried on
					// both sides fell through every branch and kept the Shell_face base,
					// which the shell dome lights.
					const bool        up_shell      = (up_s_open | up_e_open) != 0;
					const int         up_top_mark   = up_facade       ? Mask::Shell_top
													: up_sill_shell ? Mask::Shell_face
													: (upper_interior && up_storey >= 1)
															? Mask::Slab + up_storey
															: Mask::Tall + up_storey;
					paint_shell_face(
							true, up_s_bits, up_e_bits, up_shell, true, up_s_open, up_e_open, up_top_mark, up_far_open);
					return;
				}
				tall_exterior = true;
				tall_storey   = storey_of(obj->get_lift());
			} else if (!roof_like && top >= 5 && !open_sky_above(top) && !gwin->is_in_dungeon()) {
				// Ground shell walls below the render skip (see the branch above).
				const bool shell = faces_exterior(
						true, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
				if (shell && obj->get_info().is_door() && !obj->is_closed_door()) {
					return;
				}
				// Same rule as "upper-shell-face": a PANE glows with what is behind
				// it, which the splat keys on Shell_face + Kind::Ground.  The generic call below
				// certifies its faces interior and stamps a separate top mark, so
				// one shutter sprite comes out in two differently-masked bands, and
				// the sill under it keeps a top the torch cannot reach.  Only the
				// upper storey had this path; the ground floor kept the split.
				if (NaturalLight::Object_passes_light(obj)) {
					paint_shell_face(false);
					return;
				}
				if ((shell || !obj->get_info().is_door())
					&& (NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj))) {
					paint_shell_face(true, shell_s_bits, shell_e_bits, shell, true, shell_s_open, shell_e_open);
					return;
				}
			} else if (
					!roof_like && top < 5 && !open_sky_above(top) && obj->get_info().is_solid() && !obj->get_info().is_door()
					&& !obj->is_dragable() && !gwin->is_in_dungeon()) {
				// Low shell trim (battlement pieces beside a window), as outside.
				// Door anchors excluded: a threshold step in front of the doorway
				// is part of the fan's ground, not the shell.
				// Stamped through paint_shell_face so its faces get the same
				// Shell_top/Face_south/Face_east the wall around it gets.  Gated on faces_exterior it
				// fell through for a building the avatar is not in, kept a clear mask and
				// drank the z-blind field -- another house's halfwall lit up inside.
				const bool shell = faces_exterior(
						false, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
				// Low TRIM sits on the wall line; a table or a torch bracket is
				// furniture standing in the room, and painting it as shell art gave
				// it Face_south/Face_east + Kind::Object, so wall_obj lit it as a wall face.  The two
				// are told apart by the shell verdict itself, which this branch
				// previously used only to pick the mark value.  Unlike the Shell_face/Shell_top
				// fall-through that motivated dropping the gate, a piece that falls
				// through now stays Kind::Object and takes object_alpha at its OWN tile,
				// not the z-blind field.
				if (shell && !obj->get_info().is_light_source()) {
					// mark_top = false: a sill's flat surface sits INSIDE the room, so
					// it takes the room like its faces.  As an exposed wall top (Shell_top)
					// it drew all-side arrivals and lit from the street, and on a low
					// piece that band is most of the sprite.
					// EXCEPT on a sill: a low piece with an OPENING (window, glass,
					// shutter) in the course resting on it.  Unmarked, its band is
					// Shell_face + Kind::Object, which is neither pane_px nor top_px nor face_px
					// under an interior light, so it falls into the veto block and
					// is cut -- the torch in the room never reached its own sills.
					// Shell_top sets top_px, which skips that block; the street view is
					// unchanged, since shell_dome already covers both shell marks alike.
					const Tile_coord ltile = obj->get_tile();
					const bool       sill_top
							= opening_above_at(ltile.tx, ltile.ty, obj->get_lift() + obj->get_info().get_3d_height());
					paint_shell_face(true, shell_s_bits, shell_e_bits, shell, sill_top, shell_s_open, shell_e_open);
					return;
				}
			}
		} else {
			const bool is_roof_shape = obj->get_info().is_roof();
			roof_like                = is_roof_shape || obj->get_lift() >= 5;
			const int top            = obj->get_lift() + obj->get_info().get_3d_height();
			if (roof_like && !is_roof_shape && open_sky_above(top)) {
				// Object on an open-sky deck: Tall + storey, as in the inside
				// branch; a cap on a solid wall with no floor-roof at its base
				// stays a whole unit.
				tall_exterior = true;
				if (!solid_below() || floor_at_base()) {
					tall_storey = storey_of(obj->get_lift());
				}
			} else if (roof_like && !is_roof_shape) {
				// A second storey's wall / furnishing seen from outside: mark
				// Tall + storey so only its own storey's lights and spills reach it.
				tall_exterior = true;
				tall_storey   = storey_of(obj->get_lift());
			} else if (!roof_like && top >= 5) {
				// A grounded shape tall enough to reach roof level (tree,
				// lamppost): under open sky mark it as a whole unit so masked
				// lights don't cut the canopy into lit and dark patches; under
				// cover it is a wall in the building's shell -- mark its face
				// (Shell_face), except window panes and door leaves, which keep
				// glowing with the light behind them.
				if (open_sky_above(top)) {
					tall_exterior = true;
				} else {
					const Shape_info& winfo = obj->get_info();
					// Light-passing wall pieces (window walls, grates) are Shell_face like
					// any covered wall: their glass band lights via the veto's glass
					// rule, never the z-blind field (which washed the wall under the
					// glass).  A CLOSED door is shell as well -- left unmarked it
					// stayed clear and drank the room's field through itself, and
					// the neighbour wall's stamp showed through its art.  An OPEN
					// leaf keeps glowing with the light behind it.
					if (winfo.is_door() && !obj->is_closed_door()) {
						return;
					}
					// A LIGHT SOURCE is never shell.  Shell_face is unconditionally dark
					// under both light kinds, so a torch standing under an eave
					// masked itself out.  Object_is_wall_face cannot serve here:
					// the wall course under a window is Kind::Object as well.
					if (!winfo.is_light_source()) {
						paint_shell_face(false);
						return;
					}
				}
			} else if (
					!roof_like && !open_sky_above(top) && obj->get_info().is_solid() && !obj->get_info().is_door()
					&& !obj->is_dragable()) {
				// A LOW solid under cover (battlement trim beside a window):
				// part of the wall shell too -- left clear, its pixels z-blind
				// sample the interior field (the lit strip beneath a window).
				// A LIGHT SOURCE is not shell: Shell_face is unconditionally dark and a
				// torch at a doorway masked itself out.
				if (!obj->get_info().is_light_source()) {
					frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_face);
					return;
				}
			}
		}
		// A tall EXTERIOR piece with OPEN SKY above it is outdoors -- a battlement
		// on the castle wall -- not an upper floor.  Giving it the slab mark while
		// the viewer is inside also gives it the slab's exemption in the veto
		// block (slab_s < light_top_storey admits "the upper room's own floor"),
		// so a torch in the room below lit the battlements through the wall.  The
		// slab path already draws this distinction with deck_outdoors.
		const bool tall_slab = upper_interior && tall_storey >= 1
							   && !open_sky_above(obj->get_lift() + obj->get_info().get_3d_height());
		frame->paint_rle_transformed(
				roof_light_mask, sx, sy,
				tall_exterior ? (tall_slab ? roof_slab[tall_storey] : roof_tall[tall_storey])
							  : (roof_like ? roof_set : roof_clear));
	}

	void Queue_spill_renders(
			const Tile_coord& ltile, int radius, int tier, int elevation, const std::vector<Light_spill>& spills,
			bool moving) {
		Game_window* const gwin = Game_window::get_instance();
		for (const Light_spill& spill : spills) {
			const Tile_coord& sp = spill.tile;
			// Budget by the fill's PATH to the opening, not the straight
			// line: the wrap-around fill also exits under other buildings'
			// eaves, where the light has already spent its whole reach.
			const int spill_dist   = std::max(ltile.distance_2d(sp), spill.path) * c_tilesize;
			const int spill_radius = radius - spill_dist;
			if (spill_radius <= 0) {
				continue;
			}
			// +7: rounding slack plus the wall-top anchor shift (as the source's grid).
			const int                  srt = spill_radius / c_tilesize + 7;
			std::vector<unsigned char> slit;
			std::vector<unsigned char> sring;
			// Outward fan axis: window spills exit through their opening
			// wall tile; doorway/well exits fall back to the light->exit
			// dominant axis.  (0,0) = unconstrained.
			int cdx = spill.opening.tx >= 0 ? Tile_coord::delta(spill.opening.tx, sp.tx) : Tile_coord::delta(ltile.tx, sp.tx);
			int cdy = spill.opening.tx >= 0 ? Tile_coord::delta(spill.opening.ty, sp.ty) : Tile_coord::delta(ltile.ty, sp.ty);
			if (std::abs(cdx) >= std::abs(cdy)) {
				cdx = cdx > 0 ? 1 : (cdx < 0 ? -1 : 0);
				cdy = 0;
			} else {
				cdy = cdy > 0 ? 1 : -1;
				cdx = 0;
			}
			const bool pos_hi = cdy > 0 || cdx < 0;
			const int  cmouth = (pos_hi ? spill.mouth_neg : spill.mouth_pos) | ((pos_hi ? spill.mouth_pos : spill.mouth_neg) << 8);
			// Walls in the grid for any viewer: the shell is stamped from
			// inside too, so the ring cannot wash interior faces.
			Build_spill_shadow_grid(sp, srt, slit, true, &sring, cdx, cdy, cmouth);
			int ssx = 0;
			int ssy = 0;
			gwin->get_shape_location(sp, ssx, ssy);
			gwin->add_light_render(
					ssx, ssy, spill_radius, tier, elevation, srt, sp.tx, sp.ty, sp.tz, std::move(slit), false, spill_dist, true,
					spill.percent, spill.floor, moving, std::move(sring), spill.on_deck, cdx, cdy, cmouth);
		}
	}

	TileRect Splat_light(
			const Splat_frame& f, const Light_render_info& lr, unsigned char* covp, int cx0, int cy0, int cx1, int cy1) {
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const map = gwin->get_map();
		const int W = f.W;
		const int H = f.H;
		unsigned char* const dstpix = f.dstpix;
		const int dst_lw = f.dst_lw;
		const unsigned char* const srcpix = f.srcpix;
		const int src_lw = f.src_lw;
		const unsigned char* const roofpix = f.roofpix;
		const int roof_lw = f.roof_lw;
		const unsigned char* const kindpix = f.kindpix;
		const int kind_lw = f.kind_lw;
		const unsigned char* const footdxp = f.footdxp;
		const unsigned char* const footdyp = f.footdyp;
		const int foot_lw = f.foot_lw;
		const std::vector<Sprite_box>& light_sprite_boxes = f.sprites;
		const bool inside = f.inside;
		const int av_fx = f.av_fx;
		const int av_fy = f.av_fy;
		const int skip_above_actor = f.skip_above_actor;
		// Masking policy: see the `inside` comment in build_light_layers; the per-pixel
		// roof / tall / storey semantics live in Splat_radial_light.
		const bool mask_roof = roofpix && lr.mask_roof;
		// The dome (intensity + falloff) emits from the flame up on the
		// sprite, not the tile foot get_shape_location returns, so raise
		// only its centre up-and-left by the sprite's height (plus the
		// foot-corner residual).  A SPILL's centre stays at the opening's
		// outside tile -- its elevation only shapes the continued falloff
		// -- so it is not shifted.
		const int foot_bias = c_tilesize / 4;
		const int emit      = (lr.is_spill ? 0 : lr.elevation) + foot_bias;
		const int csx       = lr.sx - emit;
		const int csy       = lr.sy - emit;
		// Gated lights render as a propagated field from the room-fill
		// grid (see Splat_radial_light).  The screen reference pinning
		// the grid is the foot position of the light's own tile at the
		// TOP of the room's walls (walls render 4px per z up-left of
		// their tile) -- a fixed property of the room, so the field
		// stays pinned to the walls as the source moves.
		const unsigned char* grid    = nullptr;
		int                  grid_rt = 0;
		int                  grid_fx = 0;
		int                  grid_fy = 0;
		// Veto lights: the storey their room roof reaches; spills: the
		// bubble's render storey (gating rules in Splat_radial_light).
		int light_top_storey = 0;
		// The field's lattice anchor level (locates the shaft band of
		// an elevated light's floor holes in Splat_radial_light).
		int field_anchor_z = 0;
		// A light whose room is not part of this render must not
		// splat: a veto light on a cut-away storey (viewer below
		// it) would paint only its shaft band onto the room below;
		// a roofed spill whose ceiling IS drawn (viewer above its
		// storey) is hidden and would paint only its z-shifted
		// wall-ring seam over the slab sides / exterior ground.
		bool hidden_room = lr.mask_roof && lr.ltz >= skip_above_actor;
		// ...but a TALL HALL is ONE room from floor to ceiling: stepping
		// under a lower roof cuts the render above the Avatar's head while
		// the hall's floor is still on screen and still lit, so dropping
		// its torches blacked out the whole hall.  The case this guards
		// against is a real upper STOREY, where a slab separates the light
		// from the ground.  Ask an OPEN tile of the light's own room, not
		// the light's tile -- a wall sconce stands IN the masonry, where
		// the query answers with the wall itself (roof_0=9 for a hall whose
		// ceiling is 11).  Same roof from the ground as from the light's
		// height => no slab between => one room.
		if (hidden_room && !lr.is_spill && !lr.lit.empty()) {
			const int gside = 2 * lr.rt + 1;
			int       otx = -1, oty = -1, bestd = 1 << 30;
			for (int gy = 0; gy < gside; ++gy) {
				for (int gx = 0; gx < gside; ++gx) {
					if (NaturalLight::Grid::Dist(lr.lit[static_cast<size_t>(gy) * gside + gx]) == 0) {
						continue;
					}
					const int d = std::abs(gx - lr.rt) + std::abs(gy - lr.rt);
					if (d == 0 || d >= bestd) {
						continue;
					}
					bestd = d;
					otx   = ((lr.ltx - lr.rt + gx) % c_num_tiles + c_num_tiles) % c_num_tiles;
					oty   = ((lr.lty - lr.rt + gy) % c_num_tiles + c_num_tiles) % c_num_tiles;
				}
			}
			if (otx >= 0
				&& NaturalLight::Light_room_roof_z(map, otx, oty, 0)
						   == NaturalLight::Light_room_roof_z(map, otx, oty, lr.ltz)) {
				hidden_room = false;
			}
		}
		if ((inside || lr.mask_roof || lr.is_spill) && !lr.lit.empty()) {
			grid    = lr.lit.data();
			grid_rt = lr.rt;
			// Anchor at the wall top whenever the light has a real
			// ceiling (a dungeon's rock ceiling counts even though
			// mask_roof is false).  Only a light with none -- an exterior
			// lamp gated while the Avatar is inside, a spill on open
			// ground -- anchors at its own storey floor: the floor+5
			// fallback would slide its field 4px per z up-left off it.
			bool      ceiling  = false;
			const int roof_z   = NaturalLight::Light_room_roof_z(map, lr.ltx, lr.lty, lr.ltz, &ceiling);
			const int anchor_z = (lr.mask_roof || ceiling) ? roof_z : (lr.ltz / 5) * 5;
			field_anchor_z     = anchor_z;
			if (lr.is_spill && ceiling && roof_z < skip_above_actor) {
				hidden_room = true;
			}
			// Viewer inside: a gated light whose ceiling is STILL DRAWN
			// (a room under a deck while the Avatar stands under a higher
			// ceiling) has an invisible interior -- the drawn slab covers
			// it.  Its z-blind field would still wash the room's CLEAR
			// wall pixels rising past the slab edge (a lit "crack" between
			// floor-roof and wall).  Escaped light stays with its spills.
			// Outside viewers keep the splat: its field paints the escaped
			// ground light through openings (top = 0 keeps marks dark).
			if (lr.mask_roof && inside && ceiling && roof_z < skip_above_actor) {
				hidden_room = true;
			}
			if (lr.mask_roof) {
				// Cutoff = the ceiling slab TOP's storey: anchor_z is the
				// slab's BOTTOM lift while marks carry the top's storey, so
				// anchor_z/5 under-counts a roof at floor+4 (darkens the
				// room's own upper-storey furnishings -- shelved books)
				// and (anchor_z+4)/5 over-counts a roof at storey*5+1
				// (lights the slab from below).  (anchor_z+1)/5 matches the
				// slab top for any slab height >= 1 and stays above the
				// room's floor storey.  Viewer OUTSIDE: every visible mark
				// is an exterior face (the interior is under the drawn
				// roof); the z-blind interior field paints those patchily,
				// and exterior faces are the spills' job -- keep all marks
				// dark (0 fails every storey test).
				light_top_storey = inside ? (anchor_z + 1) / 5 : 0;
			} else if (lr.is_spill) {
				light_top_storey = lr.ltz / 5;
			}
			const int wtx = ((lr.ltx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int wty = ((lr.lty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			gwin->get_shape_location(Tile_coord(wtx, wty, anchor_z), grid_fx, grid_fy);
			// Empirically the wall-top anchor lands 1px up-left of the
			// visible interior; nudge the reference back.
			grid_fx += 1;
			grid_fy += 1;
		}
		if (!hidden_room) {
			// Inside viewer: every visible storey mark is an INTERIOR
			// face/surface (the z-blind field would paint the neighbour
			// rooms' wall faces with outdoor pool cells hanging over
			// them up-screen), so an outdoor spill keeps them all dark:
			// spill_floor 0 fails every upper-storey Tall test while a ground-storey Tall
			// (canopies, deck objects) keeps its whole-unit free dome.
			// A spill placed ON an overlooked floor-roof deck is exempt: that
			// deck is exterior surface under open sky, and lighting it is the
			// whole point of a high window's spill.
			const int spill_floor = (lr.is_spill && inside && !lr.spill_on_deck) ? 0 : lr.spill_floor;
			NaturalLight::Splat_radial_light(
					covp, dstpix, srcpix, W, H, dst_lw, src_lw, csx, csy, lr.radius, lr.elevation, lr.dist_bias,
					lr.spill_percent, roofpix, roof_lw, mask_roof, lr.is_spill, spill_floor, light_top_storey, lr.ltz / 5,
					field_anchor_z, grid, grid_rt, grid_fx, grid_fy, inside, cx0, cy0, cx1, cy1, kindpix, kind_lw, footdxp,
					footdyp, foot_lw,
					// No ring in dungeons: they are wholly interior, the
					// classic rendering is correct there (and lights the
					// walls' near strips the face rule would darken).
					!gwin->is_in_dungeon() && lr.ring.size() == static_cast<size_t>(2 * grid_rt + 1) * (2 * grid_rt + 1) * 4
							? lr.ring.data()
							: nullptr,
					av_fx, av_fy, light_sprite_boxes.empty() ? nullptr : light_sprite_boxes.data(),
					static_cast<int>(light_sprite_boxes.size()), lr.spill_on_deck, lr.cone_dx, lr.cone_dy, lr.cone_mouth);
		}
		// Match Splat_radial_light's reach: only an ELEVATED spill's
		// dome extends sqrt(r^2 + 2*r*bias), capped at 1.5r.
		const int reach
				= (lr.is_spill && light_top_storey >= 1 && lr.dist_bias > 0)
						  ? std::min(
									static_cast<int>(std::ceil(std::sqrt(
											static_cast<double>(lr.radius) * lr.radius + 2.0 * lr.radius * lr.dist_bias))),
									(3 * lr.radius) / 2)
						  : lr.radius;
		int bx0 = csx - reach;
		int bx1 = csx + reach;
		int by0 = csy - reach;
		int by1 = csy + reach;
		if (grid != nullptr) {
			bx0 = std::min(bx0, grid_fx - lr.radius - c_tilesize);
			bx1 = std::max(bx1, grid_fx + lr.radius + c_tilesize);
			by0 = std::min(by0, grid_fy - lr.radius - c_tilesize);
			by1 = std::max(by1, grid_fy + lr.radius + c_tilesize);
		}
		NaturalLight::Reach_extend_box(
				bx0, by0, bx1, by1, light_sprite_boxes.empty() ? nullptr : light_sprite_boxes.data(),
				static_cast<int>(light_sprite_boxes.size()));
		return TileRect(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
	}

	void Relight_sprites(
			const Splat_frame& f, const std::vector<Light_render_info>& light_renders, int t, const unsigned char* cov,
			std::vector<unsigned char>& light_scratch_cov, bool& has_moving) {
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const map = gwin->get_map();
		Actor* const main_actor = gwin->get_main_actor();
		const int W = f.W;
		const int H = f.H;
		unsigned char* const dstpix = f.dstpix;
		const int dst_lw = f.dst_lw;
		const unsigned char* const srcpix = f.srcpix;
		const int src_lw = f.src_lw;
		const unsigned char* const roofpix = f.roofpix;
		const int roof_lw = f.roof_lw;
		const unsigned char* const kindpix = f.kindpix;
		const int kind_lw = f.kind_lw;
		const unsigned char* const footdxp = f.footdxp;
		const unsigned char* const footdyp = f.footdyp;
		const int foot_lw = f.foot_lw;
		const bool inside = f.inside;
		static const Xform_palette lift_set = [] {
			Xform_palette x;
			for (int i = 0; i < 256; ++i) {
				x.colors[i] = 255;
			}
			return x;
		}();
		std::vector<Game_object*> subjects;
		{
			Actor_vector alist;
			if (main_actor != nullptr) {
				main_actor->find_nearby_actors(alist, c_any_shapenum, 24, 0x28);
				alist.push_back(main_actor);
			}
			subjects.assign(alist.begin(), alist.end());
		}
		for (const auto& lr2 : light_renders) {
			if (lr2.is_spill || lr2.moving || lr2.tier != t) {
				continue;
			}
			// A roofed light's sprite is hidden behind the shell from
			// outside: don't lift it (this replaces the occluder erase,
			// whose binary paint-order compare wrongly wiped visible
			// sconces -- [lift] foot_a>0 lifted=0).
			if (!inside && lr2.mask_roof) {
				continue;
			}
			// Mirror: an EXTERIOR light's sprite seen from inside -- the
			// viewer's shell may cover it in the world render, and lifting
			// would stamp its glow through the wall (streetlamp dome
			// bleeding into the barn).  Same trade-off as above when it is
			// actually visible through an opening: sprite unbrightened,
			// pool unaffected.
			if (inside && !lr2.mask_roof) {
				continue;
			}
			const int        wtx = ((lr2.ltx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((lr2.lty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (ch == nullptr) {
				continue;
			}
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				const Tile_coord ot = o->get_tile();
				if (ot.tx == wtx && ot.ty == wty && ot.tz == lr2.ltz && o->get_info().is_light_source()) {
					subjects.push_back(o);
					break;
				}
			}
		}
		const size_t sz = static_cast<size_t>(W) * H;
		for (Game_object* act : subjects) {
			if (act == nullptr || act->get_flag(Obj_flags::dont_render)) {
				continue;
			}
			Shape_frame* fr = act->get_shape();
			if (fr == nullptr) {
				continue;
			}
			int asx = 0;
			int asy = 0;
			gwin->get_shape_location(act, asx, asy);
			const int abx0 = asx - fr->get_xleft();
			const int aby0 = asy - fr->get_yabove();
			const int abw  = fr->get_width();
			const int abh  = fr->get_height();
			if (abx0 + abw <= 0 || aby0 + abh <= 0 || abx0 >= W || aby0 >= H) {
				continue;
			}
			// Ground light at his feet: brightest CLEAR-pixel coverage in
			// a small window around the foot hotspot (marked pixels --
			// walls, slabs -- are not pool light).  A wall-mounted sconce
			// has no clear ground at its mount: reach further down-screen
			// to the floor its own pool lights.
			const bool           is_actor = act->as_actor() != nullptr;
			const unsigned char* rdcov    = has_moving ? light_scratch_cov.data() : cov;
			int                  foot_a   = 0;
			const int            wy1      = asy + (is_actor ? c_tilesize / 2 : 2 * c_tilesize);
			const int            wx0      = asx - (is_actor ? c_tilesize : 2 * c_tilesize);
			const int            wx1      = asx + (is_actor ? c_tilesize / 2 : c_tilesize);
			for (int y = std::max(asy - c_tilesize / 2, 0); y <= std::min(wy1, H - 1); ++y) {
				for (int x = std::max(wx0, 0); x <= std::min(wx1, W - 1); ++x) {
					const unsigned char cv2 = rdcov[static_cast<size_t>(y) * W + x];
					if (roofpix != nullptr && roofpix[static_cast<size_t>(y) * roof_lw + x] != 0) {
						continue;
					}
					if (cv2 > foot_a) {
						foot_a = cv2;
					}
				}
			}
			// Screen pixels can be a dead end: under a drawn roof or against
			// a shell wall every pixel at his feet is masked, so the sample
			// above finds nothing though he stands in the pool.  Ask the
			// lights themselves what reaches his TILE.
			{
				const Tile_coord at = act->get_tile();
				for (const auto& lr3 : light_renders) {
					if (lr3.tier != t || lr3.radius <= 0) {
						continue;
					}
					const int ta = NaturalLight::Light_tile_alpha(
							lr3.lit.empty() ? nullptr : lr3.lit.data(), lr3.rt, lr3.ring.empty() ? nullptr : lr3.ring.data(),
							lr3.ltx, lr3.lty, at.tx, at.ty, lr3.radius, lr3.elevation, lr3.dist_bias, lr3.spill_percent);
					if (ta > foot_a) {
						foot_a = ta;
					}
				}
			}
			if (foot_a <= 0) {
				continue;
			}
			if (!has_moving) {
				light_scratch_cov.resize(sz);
				std::memcpy(light_scratch_cov.data(), cov, sz);
				has_moving = true;    // Route layer_set_coverage to the scratch.
			}
			// Rasterize the sprite's coverage, then lift its pixels.
			static std::unique_ptr<Image_buffer8> ascratch;
			if (!ascratch || static_cast<int>(ascratch->get_width()) < abw || static_cast<int>(ascratch->get_height()) < abh) {
				const int nw = std::max(abw, ascratch ? static_cast<int>(ascratch->get_width()) : 0);
				const int nh = std::max(abh, ascratch ? static_cast<int>(ascratch->get_height()) : 0);
				ascratch     = std::make_unique<Image_buffer8>(nw, nh);
			}
			ascratch->fill8(0, abw, abh, 0, 0);
			fr->paint_rle_transformed(ascratch.get(), fr->get_xleft(), fr->get_yabove(), lift_set);
			// Erase every sprite the renderer paints AFTER the actor (the
			// real paint order -- a foot-offset heuristic misjudges thick
			// multi-tile walls, shining his light through them): what
			// covers his sprite must not be lifted.  ACTORS ONLY: the
			// binary compare mis-orders wall/roof sprites against wall
			// decorations and wiped visible sconces; light sprites are
			// instead gated by viewer-vs-roof above.
			static const Xform_palette erase_set = [] {
				Xform_palette x;
				for (int i = 0; i < 256; ++i) {
					x.colors[i] = 0;
				}
				return x;
			}();
			if (is_actor) {
				Game_object_vector occ;
				act->find_nearby(occ, c_any_shapenum, 10, 0x80);
				Ordering_info aord(gwin, act);
				const TileRect abox(abx0, aby0, abw, abh);
				// compare() is 0 both for "no screen overlap" and for the
				// UNDECIDED pair (x apart one way, y the other -- a tree
				// south-west of him): the renderer still paints one of them
				// last, and the canopy that covered him was skipped here, so
				// his lit silhouette showed through it.  For an undecided
				// pair that does overlap on screen take the deeper tile
				// (greater tx + ty) as the one in front.
				const Tile_coord atile = act->get_tile();
				for (Game_object* obj : occ) {
					if (obj == nullptr || obj->as_actor() != nullptr) {
						continue;
					}
					const int cmp = Game_object::compare(aord, obj);
					if (cmp == 1) {
						continue;    // Paints before him.
					}
					if (cmp == 0) {
						if (!gwin->get_shape_rect(obj).intersects(abox)) {
							continue;
						}
						const Tile_coord ot = obj->get_tile();
						if (ot.tx + ot.ty < atile.tx + atile.ty) {
							continue;    // Behind him.
						}
					}
					Shape_frame* ofr = obj->get_shape();
					if (ofr == nullptr) {
						continue;
					}
					int osx = 0;
					int osy = 0;
					gwin->get_shape_location(obj, osx, osy);
					ofr->paint_rle_transformed(ascratch.get(), osx - abx0, osy - aby0, erase_set);
				}
			}
			const unsigned char* abits = ascratch->get_bits();
			const int            aslw  = static_cast<int>(ascratch->get_line_width());
			unsigned char*       wcov  = light_scratch_cov.data();
			// A LIGHT SOURCE cannot use the ordering compare above (it
			// mis-orders wall sprites against their decorations and wipes
			// visible sconces), so occlusion is decided from the surface
			// channels instead: the kind mask records the TOPMOST sprite's
			// foot at each pixel, so a foot pointing at another object's
			// tile means that object is drawn in front of the lamp.  Ground
			// carries no foot, so it stays permissive -- a sconce whose own
			// shape is not solid must keep lighting up.
			const int  own_lift = act->get_lift();
			const int  own_f0x  = asx + 4 * own_lift;
			const int  own_f0y  = asy + 4 * own_lift;
			const int  own_xts  = act->get_info().get_3d_xtiles(act->get_framenum());
			const int  own_yts  = act->get_info().get_3d_ytiles(act->get_framenum());
			const bool own_test = !is_actor && kindpix != nullptr && footdxp != nullptr && footdyp != nullptr;
			for (int y = std::max(aby0, 0); y < std::min(aby0 + abh, H); ++y) {
				const unsigned char* arow = abits + static_cast<size_t>(y - aby0) * aslw;
				for (int x = std::max(abx0, 0); x < std::min(abx0 + abw, W); ++x) {
					if (arow[x - abx0] == 0) {
						continue;
					}
					if (roofpix != nullptr) {
						const unsigned char rm = roofpix[static_cast<size_t>(y) * roof_lw + x];
						if (rm == Mask::Roof) {
							continue;    // Never light a drawn roof.
						}
						// An outdoor deck carries Tall + storey, an indoor upper
						// floor Slab + storey; both are a floor drawn over this
						// pixel.
						const int over_storey = Mask::Is_slab(rm)   ? rm - Mask::Slab
											  : Mask::Is_tall(rm) ? rm - Mask::Tall
																		: -1;
						if (over_storey > 0 && own_lift < over_storey * 5) {
							// A floor SLAB is drawn over this pixel and the
							// subject stands below it: the world hides him
							// there, so brightening it shows his sprite
							// through the floor.  A subject standing ON the
							// slab (lift at or above its storey) keeps his.
							continue;
						}
					}
					if (own_test) {
						const unsigned char kb = kindpix[static_cast<size_t>(y) * kind_lw + x];
						if (kb == Kind::Wall_face) {
							// A WALL FACE is the topmost sprite here, so the wall
							// (or the door filling it) is drawn in front of the
							// lamp.  The foot test alone cannot catch it when the
							// wall stands on the lamp's OWN tile -- the feet match
							// and the lamp's silhouette bled through the mask.
							continue;
						}
						if (kb == Kind::Object) {
							const size_t fi  = static_cast<size_t>(y) * foot_lw + x;
							const int    dfx = own_f0x - (x + static_cast<int>(footdxp[fi]) - Foot_bias);
							const int    dfy = own_f0y - (y + static_cast<int>(footdyp[fi]) - Foot_bias);
							if (dfx < 0 || dfy < 0 || dfx > own_xts * c_tilesize || dfy > own_yts * c_tilesize) {
								continue;    // Another object owns this pixel.
							}
						}
					}
					const size_t ci = static_cast<size_t>(y) * W + x;
					if (wcov[ci] < foot_a) {
						wcov[ci]                                    = static_cast<unsigned char>(foot_a);
						dstpix[static_cast<size_t>(y) * dst_lw + x] = srcpix[static_cast<size_t>(y) * src_lw + x];
					}
				}
			}
		}
	}

}    // namespace NaturalLight
