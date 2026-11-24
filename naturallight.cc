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
#include "endianio.h"
#include "gamemap.h"
#include "gamewin.h"
#include "objiter.h"
#include "objs.h"
#include "ordinfo.h"
#include "palette.h"
#include "path.h"
#include "paths.h"
#include "shapeinf.h"
#include "vgafile.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
	// Shape_frame::has_point re-walks the frame's RLE runs on every call, too slow for the per-pixel
	// splat at high resolutions.  Same answer from a per-thread bitmap per frame, built once from the
	// runs.
	struct Art_bitmap {
		const unsigned char*       data = nullptr;    // Detects a freed frame whose address was reused.
		int                        x0   = 0;          // Bitmap column 0 = origin x - xleft - 1.
		int                        y0   = 0;
		int                        w    = 0;
		int                        h    = 0;
		std::vector<unsigned char> bits;
	};

	// ShapeID::get_info is a std::map lookup, too slow for the per-pixel lambdas: a flat per-thread table of
	// Shape_info pointers, dropped every frame (Flood_cache_frame_begin).
	std::atomic<uint32_t> shape_info_gen{1};

	Shape_info& Fast_info(int shape) {
		thread_local std::vector<Shape_info*> table;
		thread_local uint32_t                 gen      = 0;
		thread_local unsigned                 info_gen = 0;
		const uint32_t                        now      = shape_info_gen.load(std::memory_order_relaxed);
		const unsigned                        now_info = Shape_manager::get_instance()->get_shapes().get_info_generation();
		if (gen != now || info_gen != now_info) {
			std::fill(table.begin(), table.end(), nullptr);
			gen      = now;
			info_gen = now_info;
		}
		if (shape < 0) {
			return ShapeID::get_info(shape);
		}
		if (static_cast<size_t>(shape) >= table.size()) {
			table.resize(static_cast<size_t>(shape) + 1, nullptr);
		}
		Shape_info*& e = table[static_cast<size_t>(shape)];
		if (e == nullptr) {
			e = &ShapeID::get_info(shape);
		}
		return *e;
	}

	bool Art_has_point(const Shape_frame* fr, int x, int y) {
		if (!fr->is_rle()) {
			return fr->has_point(x, y);
		}
		thread_local std::unordered_map<const Shape_frame*, Art_bitmap> cache;
		thread_local const Shape_frame*                                 last_fr = nullptr;
		thread_local Art_bitmap*                                        last_bm = nullptr;
		const unsigned char* const                                      data    = const_cast<Shape_frame*>(fr)->get_data();
		Art_bitmap*                                                     bm      = nullptr;
		if (fr == last_fr && last_bm->data == data) {
			bm = last_bm;
		} else {
			auto it = cache.find(fr);
			if (it == cache.end() || it->second.data != data) {
				if (cache.size() > 8192) {
					cache.clear();
				}
				Art_bitmap& nb = cache[fr];
				nb.data        = data;
				nb.x0          = -fr->get_xleft() - 1;
				nb.y0          = -fr->get_yabove();
				nb.w           = fr->get_width() + 2;
				nb.h           = fr->get_height();
				nb.bits.assign(static_cast<size_t>(nb.w) * nb.h, 0);
				const unsigned char* in = data;
				int                  scanlen;
				while ((scanlen = little_endian::Read2(in)) != 0) {
					const int encoded = scanlen & 1;
					scanlen           = scanlen >> 1;
					const int scanx   = static_cast<short>(little_endian::Read2(in));
					const int scany   = static_cast<short>(little_endian::Read2(in));
					const int row     = scany - nb.y0;
					if (row >= 0 && row < nb.h) {
						// has_point is liberal by one pixel on either end of a run.
						const int c0 = std::max(scanx - 1 - nb.x0, 0);
						const int c1 = std::min(scanx + scanlen - nb.x0, nb.w - 1);
						for (int c = c0; c <= c1; ++c) {
							nb.bits[static_cast<size_t>(row) * nb.w + c] = 1;
						}
					}
					if (!encoded) {
						in += scanlen;
						continue;
					}
					for (int b = 0; b < scanlen;) {
						unsigned char bcnt   = *in++;
						const int     repeat = bcnt & 1;
						bcnt                 = bcnt >> 1;
						in += repeat ? 1 : bcnt;
						b += bcnt;
					}
				}
				it = cache.find(fr);
			}
			bm      = &it->second;
			last_fr = fr;
			last_bm = bm;
		}
		const int c = x - bm->x0;
		const int r = y - bm->y0;
		if (c < 0 || r < 0 || c >= bm->w || r >= bm->h) {
			return fr->has_point(x, y);    // Outside the frame's box: only malformed runs could hold it.
		}
		const bool hit = bm->bits[static_cast<size_t>(r) * bm->w + c] != 0;
		return hit;
	}
}    // namespace

namespace {

	inline int Light_tile_norm(int t) {
		return (t % c_num_tiles + c_num_tiles) % c_num_tiles;
	}

	// Map_chunk::is_tile_occupied blind to actors: an NPC is not geometry, and
	// one standing in a gap sealed it (a shadow cast by a passer-by).
	bool Light_occupied(Map_chunk* ch, int lx, int ly, int z) {
		if (!ch->is_tile_occupied(lx, ly, z)) {
			return false;
		}
		Game_map* const gmap   = ch->get_map();
		const int       wx     = ch->get_cx() * c_tiles_per_chunk + lx;
		const int       wy     = ch->get_cy() * c_tiles_per_chunk + ly;
		auto            covers = [&](Game_object* o) {
            const Shape_info& oi = Fast_info(o->get_shapenum());
            const int         lo = o->get_lift();
            return oi.is_solid() && z >= lo && z < lo + oi.get_3d_height() && o->get_footprint().has_world_point(wx, wy);
		};
		// Per-chunk actor lists, rebuilt each tick: the actor test runs on every
		// occupied query, the full rescan only where an actor stands.
		static std::unordered_map<const Map_chunk*, std::vector<Game_object*>> actors;
		static Uint64                                                          actors_t = 0;
		const Uint64                                                           now      = SDL_GetTicks();
		if (now != actors_t) {
			actors.clear();
			actors_t = now;
		}
		bool actor = false;
		for (int dcy = 0; dcy < 2 && !actor; ++dcy) {
			for (int dcx = 0; dcx < 2 && !actor; ++dcx) {
				Map_chunk* const nc
						= gmap->get_chunk_safely((ch->get_cx() + dcx) % c_num_chunks, (ch->get_cy() + dcy) % c_num_chunks);
				if (nc == nullptr) {
					continue;
				}
				auto ins = actors.try_emplace(nc);
				if (ins.second) {
					Object_iterator it(nc->get_objects());
					Game_object*    o;
					while ((o = it.get_next()) != nullptr) {
						if (o->as_actor() != nullptr) {
							ins.first->second.push_back(o);
						}
					}
				}
				for (Game_object* o : ins.first->second) {
					actor = actor || covers(o);
				}
			}
		}
		if (!actor) {
			return true;
		}
		// Footprints extend up-left of the anchor: scan the 2x2 chunk block.
		for (int dcy = 0; dcy < 2; ++dcy) {
			for (int dcx = 0; dcx < 2; ++dcx) {
				Map_chunk* const nc
						= gmap->get_chunk_safely((ch->get_cx() + dcx) % c_num_chunks, (ch->get_cy() + dcy) % c_num_chunks);
				if (nc == nullptr) {
					continue;
				}
				Object_iterator it(nc->get_objects());
				Game_object*    o;
				while ((o = it.get_next()) != nullptr) {
					if (o->as_actor() == nullptr && covers(o)) {
						return true;
					}
				}
			}
		}
		return false;
	}

	// Chunk_cache::get_lowest_blocked blind to actors (-1: none): the cache counts actors, so a
	// player standing on an upper floor roofed the tile under him and moved the lamp's spills below.
	int Light_lowest_blocked(Map_chunk* ch, int lx, int ly, int lift) {
		for (int z = ch->get_lowest_blocked(lift, lx, ly); z >= 0; z = ch->get_lowest_blocked(z + 1, lx, ly)) {
			if (Light_occupied(ch, lx, ly, z)) {
				return z;
			}
		}
		return -1;
	}

	// Map_chunk::is_roof blind to actors.
	int Light_chunk_roof(Map_chunk* ch, int lx, int ly, int lift) {
		const int h = Light_lowest_blocked(ch, lx, ly, lift + 4);
		return h < 0 ? 255 : h;
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
		return Light_chunk_roof(chunk, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, 0) < 31;
	}

	// When the given absolute tile's only cover is a walkable floor-roof deck
	// -- a storey slab whose top is at or below `max_z`, with open sky above
	// it -- return the deck's top z-level.  Returns -1 when the tile is open
	// ground or genuinely roofed (covered again higher up).  Used by the spill
	// emission: a window high in a tall wall looks out OVER the neighbouring
	// one-storey room's floor-roof, and its glow lands ON that deck; it must
	// not be dropped as "interior" just because the deck registers as a roof
	// from the ground.
	int Light_tile_overlooked_deck_raw(Game_map* gmap, int tx, int ty, int max_z, bool* open_sky);

	// The verdict scans a 2x2 chunk block per tile and tall-room floods ask it for many tiles on every
	// reflood, so it is cached across floods.  Static map geometry; a door or shutter toggling
	// (Invalidate_light_caches_near) and a 2 s keep-time drop it.
	struct Deck_verdict {
		int  top;
		bool open;
	};

	static std::unordered_map<uint64_t, Deck_verdict> deck_verdicts;
	static std::mutex                                 deck_verdicts_mx;
	static Uint64                                     deck_verdicts_t = 0;

	void Light_deck_verdicts_clear() {
		std::lock_guard<std::mutex> lk(deck_verdicts_mx);
		deck_verdicts.clear();
	}

	int Light_tile_overlooked_deck(Game_map* gmap, int tx, int ty, int max_z, bool* open_sky = nullptr) {
		tx = Light_tile_norm(tx);
		ty = Light_tile_norm(ty);
		const uint64_t key
				= (static_cast<uint64_t>(tx) << 32) | (static_cast<uint64_t>(ty) << 8) | static_cast<uint64_t>(max_z & 0xff);
		{
			std::lock_guard<std::mutex> lk(deck_verdicts_mx);
			const Uint64                now = SDL_GetTicks();
			if (now - deck_verdicts_t > 2000) {
				deck_verdicts.clear();
				deck_verdicts_t = now;
			}
			if (const auto it = deck_verdicts.find(key); it != deck_verdicts.end()) {
				if (open_sky != nullptr) {
					*open_sky = it->second.open;
				}
				return it->second.top;
			}
		}
		bool      open = false;
		const int top  = Light_tile_overlooked_deck_raw(gmap, tx, ty, max_z, &open);
		{
			std::lock_guard<std::mutex> lk(deck_verdicts_mx);
			deck_verdicts[key] = {top, open};
		}
		if (open_sky != nullptr) {
			*open_sky = open;
		}
		return top;
	}

	int Light_tile_overlooked_deck_raw(Game_map* gmap, int tx, int ty, int max_z, bool* open_sky) {
		if (open_sky != nullptr) {
			*open_sky = false;
		}
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return -1;
		}
		// The deck is a FLOOR shape (floor_shapes), and those are height-0
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
		int       cover_lo = -1;    // Highest floor/roof lift over the tile.
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
					const Shape_info& info = Fast_info(obj->get_shapenum());
					// A lower structure's exposed roof (gate passage) catches the glow like a deck.
					const bool surface = info.is_floor(obj->get_framenum()) || info.is_roof(obj->get_framenum());
					// So does the top of a solid block built up from the ground (a thick wall, a battlement);
					// a compact one is a trunk or post, as in the wall stamping.
					const TileRect bfp   = obj->get_footprint();
					const bool     block = !surface && info.is_solid() && !info.is_door() && obj->as_actor() == nullptr
									   && !obj->is_dragable() && (bfp.w > 2 || bfp.h > 2) && !info.has_light_passes_info();
					if (!surface && !block) {
						continue;
					}
					if (!obj->get_footprint().has_world_point(tx, ty)) {
						continue;
					}
					if (block) {
						// A mountain top is terrain (its cliff is drawn in its own art, nothing need stand under it,
						// a passage may run beneath): never the hollow furniture this skips.  Openings UNDER it see it as
						// their ceiling in emit_window, not here.
						bool hollow = false;
						for (int z = 0;
							 z < obj->get_lift() && !hollow && info.get_mountain_top_type() == Shape_info::not_mountain_top; ++z) {
							hollow = !Light_occupied(chunk, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, z);
						}
						if (hollow) {
							continue;    // Furniture on a deck, not a structure.
						}
					} else {
						cover_lo = std::max(cover_lo, obj->get_lift());
					}
					const int top = obj->get_lift() + info.get_3d_height();
					if (top < max_z && top > deck_top) {
						deck_top = top;
					}
				}
			}
		}
		const int above
				= deck_top >= 0 ? Light_lowest_blocked(chunk, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, deck_top + 1) : -1;
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
		if (open_sky != nullptr) {
			*open_sky = above < 0 && cover_lo < deck_top;
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
		return Light_occupied(chunk, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, floor_z + 3);
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
	int Light_tile_has_pass_through(
			Map_chunk* chunk, int tx, int ty, int floor_z, int roof_z, int* top_z = nullptr, int* lift_z = nullptr) {
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			const Shape_info& info = Fast_info(obj->get_shapenum());
			// An open door's leaf stands beside its doorway (the gap is the opening).  A CLOSED door that passes
			// light (a barred or glazed door, its frame listed with a percent) is an opening like a window.
			if (info.is_door() && !obj->is_closed_door()) {
				continue;
			}
			const int lift = obj->get_lift();
			if (lift >= roof_z || lift + info.get_3d_height() <= floor_z) {
				continue;    // Another storey's opening, not this room's.
			}
			const int percent = info.get_light_passes_percent(obj->get_framenum());
			if (percent <= 0) {
				continue;
			}
			if (obj->get_footprint().has_world_point(tx, ty)) {
				if (top_z != nullptr) {
					*top_z = obj->get_lift() + info.get_3d_height();
				}
				if (lift_z != nullptr) {
					*lift_z = obj->get_lift();
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
	// Stamps the verdicts for a WHOLE flood grid in one pass: each covered chunk's object list is
	// classified once and the qualifying walls' footprints marked, instead of re-walking the list
	// for every grid tile.  An object only stamps tiles inside its own (anchor) chunk: other
	// chunks' object lists do not contain it, so its overhang is invisible there.
	// (tx0, ty0) is the normalized world tile of grid cell (0, 0); `wall`
	// receives 1 where a tile is a wall, 0 where it is open.
	// `sight` (optional) receives 1 where a stamped tile also blocks line of sight: architecture,
	// not an indoor non-building solid (a keg reaching the ceiling casts no shadow).
	void Light_flood_stamp_walls(
			Game_map* gmap, int tx0, int ty0, int side, int roof_z, int floor_z, std::vector<unsigned char>& wall,
			std::vector<unsigned char>* sight = nullptr) {
		wall.assign(static_cast<size_t>(side) * side, 0);
		if (sight != nullptr) {
			sight->assign(static_cast<size_t>(side) * side, 0);
		}
		// Only a room taller than one standard course can have a wall that no
		// single shape carries to the roof.  In an ordinary room the per-shape
		// test must stand: there a window tile's column is sealed too, and
		// calling it a wall re-routes the window from the door path (the fill
		// flows through the glass) to the window-spill path, changing the light
		// in every house with an opening.
		const bool tall_room = roof_z > floor_z + 5;
		// Base and top lift of the piece stamped on each face tile, and the compact solids turned away as loose
		// objects: one standing in line with a wall run of its own base and height is that run's post (otherwise
		// a roofless tower's corner posts and short pieces leave its side open and the fill walks in round the
		// back).
		std::vector<short> wall_base(static_cast<size_t>(side) * side, -1);
		std::vector<short> wall_top(static_cast<size_t>(side) * side, -1);

		struct Post {
			size_t gi;
			int    base;
			int    top;
		};

		std::vector<Post> posts;
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
				const Shape_info& oi = Fast_info(o->get_shapenum());
				if ((!oi.is_floor(o->get_framenum()) && !oi.is_roof(o->get_framenum())) || o->as_actor() != nullptr
					|| o->is_dragable()) {
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
					const Shape_info& info = Fast_info(obj->get_shapenum());
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
						// blocks light entirely, so the shape falls through to the normal
						// wall tests below.)
						if (info.get_light_passes_percent(obj->get_framenum()) > 0) {
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
					if (!info.is_solid() || info.is_roof(obj->get_framenum())) {
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
					const int      shape_top    = lift + info.get_3d_height();
					const TileRect fp           = obj->get_footprint();
					int            blocks_sight = -1;    // Lazy: 1 architecture, 0 indoor furniture.
					auto           stamp_sight  = [&](size_t gi) {
                        if (sight == nullptr) {
                            return;
                        }
                        if (blocks_sight < 0) {
                            bool roofed = false;
                            if (info.get_shape_class() != Shape_info::building) {
                                const Tile_coord ot = obj->get_tile();
                                NaturalLight::Light_room_roof_z(gmap, ot.tx, ot.ty, ot.tz, &roofed);
                            }
                            blocks_sight = roofed ? 0 : 1;
                        }
                        if (blocks_sight > 0) {
                            (*sight)[gi] = 1;
                        }
					};
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
							// it would report a gap wherever the roof steps down,
							// although the wall reaches its own ceiling.
							// Use the roof over THIS tile when the chunk knows one.
							const int local_roof = [&] {
								const int r = Light_chunk_roof(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, floor_z);
								return (r < 31 && r > floor_z) ? r : roof_z;
							}();
							// A free-standing solid is an OBJECT, not a room wall: outdoors roof_z
							// falls back to floor_z + 5, which a tall tree clears, so each one would
							// seal the fill and throw a wedge of shadow across the spill fan.  Ask
							// the same verdict the kind channel uses -- ch->is_roof() is NOT a
							// substitute, it reports a roof over the tree's own tile.
							//
							// Only a COMPACT one, though: a trunk stands on a tile or two, while a
							// free-standing battlement (4x4, h 5) is just as roofless yet must still
							// stop the fan, shade the ground behind it and take ring arrivals on its
							// face.  Only a roofless one blocks: indoors a big non-wall solid is
							// furniture.
							const bool compact = (fp.w <= 2 && fp.h <= 2) || [&] {
								bool             roofed = false;
								const Tile_coord ot     = obj->get_tile();
								NaturalLight::Light_room_roof_z(gmap, ot.tx, ot.ty, ot.tz, &roofed);
								return roofed;
							}();
							if (compact && !NaturalLight::Object_is_wall_face(obj)) {
								if (lift <= floor_z && shape_top >= local_roof) {
									posts.push_back({static_cast<size_t>(dgy) * side + dgx, lift, shape_top});
								}
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
									if (!Light_occupied(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z)
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
								// stamped 2, not 1: it stops the fill (otherwise the
								// light walks out along the wall line and pops out
								// from under the floor roof) but never becomes a lit
								// wall FACE.
								if (!tall_room) {
									continue;
								}
								bool gap_above = false;
								for (int z = shape_top; z < local_roof; ++z) {
									if (!Light_occupied(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z)
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
								stamp_sight(static_cast<size_t>(dgy) * side + dgx);
								continue;
							}
							wall[static_cast<size_t>(dgy) * side + dgx]      = 1;
							wall_base[static_cast<size_t>(dgy) * side + dgx] = static_cast<short>(lift);
							wall_top[static_cast<size_t>(dgy) * side + dgx]  = static_cast<short>(shape_top);
							stamp_sight(static_cast<size_t>(dgy) * side + dgx);
						}
					}
				}
			}
		}
		for (bool grew = true; grew;) {
			grew = false;
			for (Post& p : posts) {
				if (wall[p.gi] == 1) {
					continue;
				}
				const int u = static_cast<int>(p.gi % side);
				const int v = static_cast<int>(p.gi / side);
				for (const auto& d : {
							 std::pair{ 1,  0},
                             std::pair{-1,  0},
                             std::pair{ 0,  1},
                             std::pair{ 0, -1}
                }) {
					const int nu = u + d.first;
					const int nv = v + d.second;
					if (nu < 0 || nv < 0 || nu >= side || nv >= side) {
						continue;
					}
					const size_t ni = static_cast<size_t>(nv) * side + nu;
					if (wall[ni] == 1 && wall_base[ni] == p.base && wall_top[ni] == p.top) {
						wall[p.gi]      = 1;
						wall_base[p.gi] = static_cast<short>(p.base);
						wall_top[p.gi]  = static_cast<short>(p.top);
						if (sight != nullptr) {
							(*sight)[p.gi] = (*sight)[ni];
						}
						grew = true;
						break;
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
	int Light_tile_pass_opening(
			Game_map* gmap, int tx, int ty, int floor_z, int roof_z, int* top_z = nullptr, int* lift_z = nullptr) {
		tx                     = Light_tile_norm(tx);
		ty                     = Light_tile_norm(ty);
		Map_chunk* const chunk = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
		if (chunk == nullptr) {
			return 0;
		}
		return Light_tile_has_pass_through(chunk, tx, ty, floor_z, roof_z, top_z, lift_z);
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
		// byte read per pixel instead of a bilinear float sample.
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

	// Does `chunk` hold a shape light passes through (window, open door, grate)?
	bool Chunk_find_light_passes_through(Map_chunk* chunk) {
		if (chunk == nullptr) {
			return false;
		}
		Object_iterator it(chunk->get_objects());
		Game_object*    obj;
		while ((obj = it.get_next()) != nullptr) {
			if (Object_passes_light(obj)) {
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
			if (!Object_passes_light(obj)) {
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
	// A tall hall has no slab at the light's storey: its room stands on the floor below,
	// where the Avatar walks, not in the masonry band of its upper wall courses.
	static Tile_coord Light_room_floor(Game_map* gmap, Tile_coord t, int src_tz) {
		const int roof = Light_room_roof_z(gmap, t.tx, t.ty, src_tz);
		while (t.tz >= 5 && Light_room_roof_z(gmap, t.tx, t.ty, t.tz - 5) == roof && !Light_tile_wall(gmap, t.tx, t.ty, t.tz - 5)) {
			t.tz -= 5;
		}
		return t;
	}

	// The level a light's fill floods from.  A viewer inside sees a tall hall down to its floor: the light
	// floods it as a lamp standing on that floor would.  A real upper storey keeps its own floor.
	static int Light_flood_z(Game_map* gmap, const Tile_coord& lt) {
		if (Game_window::get_instance()->is_main_actor_inside()) {
			const int fz = (Light_room_floor(gmap, lt, lt.tz).tz / 5) * 5;
			if (fz < (lt.tz / 5) * 5) {
				return fz;
			}
		}
		return lt.tz;
	}

	// Resolve a light source to a nearby interior tile; reports interior-ness.
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
		auto room_floor = [&](const Tile_coord& t) {
			return Light_room_floor(gmap, t, src.tz);
		};
		const bool own_roofed = Light_tile_roofed(gmap, base.tx, base.ty);
		const bool own_wall   = Light_tile_wall(gmap, base.tx, base.ty, floor_z);
		if (own_roofed && !own_wall) {
			interior = true;
			return room_floor(base);
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
				return room_floor(Tile_coord(Light_tile_norm(nx), Light_tile_norm(ny), floor_z));
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
			return Light_occupied(chunk, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, wall_tz);
		};
		auto is_roofless = [&](int tx, int ty) -> bool {
			const int        wtx   = normalize(tx);
			const int        wty   = normalize(ty);
			Map_chunk* const chunk = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (chunk == nullptr) {
				return false;
			}
			return Light_chunk_roof(chunk, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, start.tz) >= 31;
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
		// plus slack for winding around furniture.  A window centred on the
		// light would drop torches in one big hall as soon as the Avatar
		// crosses its edge; the cap only bounds cost -- lights that far away
		// are outside any glow radius on screen.
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
            return Light_occupied(chunk, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, wall_tz);
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
		auto covered_at = [&](int tx, int ty) -> bool {
			tx            = Light_tile_norm(tx);
			ty            = Light_tile_norm(ty);
			const int lcx = tx / c_tiles_per_chunk;
			const int lcy = ty / c_tiles_per_chunk;
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
						const Shape_info& info     = Fast_info(obj->get_shapenum());
						const bool        is_roof  = info.is_roof(obj->get_framenum());
						const bool        is_floor = info.is_floor(obj->get_framenum());
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
						if (obj->get_footprint().has_world_point(tx, ty)) {
							return true;
						}
					}
				}
			}
			return false;
		};
		bool covered = covered_at(lt.tx, lt.ty);
		// A cover only one tile deep -- a ledge between a wall (or more open
		// ground) and open sky -- makes no room: the light stands in the open
		// beside it, exactly as one step further out.  Same rule as the
		// window-spill ledge (no landing).
		if (covered) {
			const int        floor_z    = (lt.tz / 5) * 5;
			static const int dirs[4][2] = {
					{ 1,  0},
                    {-1,  0},
                    { 0,  1},
                    { 0, -1}
            };
			for (const auto& d : dirs) {
				const int nx = lt.tx + d[0];
				const int ny = lt.ty + d[1];
				const int ox = lt.tx - d[0];
				const int oy = lt.ty - d[1];
				// An opening (portcullis, grate) behind the light does not end
				// the room: the covered space continues through it, so only a
				// solid wall makes the cover a ledge.
				const bool wall_o = Light_tile_wall(gmap, ox, oy, floor_z)
									&& Light_tile_pass_opening(gmap, ox, oy, floor_z, floor_z + 5) == 0;
				if (!covered_at(nx, ny) && !Light_tile_wall(gmap, nx, ny, floor_z) && (wall_o || !covered_at(ox, oy))) {
					covered = false;
					break;
				}
			}
		}
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
					= same_chunk || (vis.interior_source && Tiles_in_same_enclosure(interior_tile, main_actor->get_tile())) || [&] {
						  // A light in a TALL space (a stairwell rising to the storey above) shares that upper
						  // room with a viewer standing there: flood at the VIEWER's storey from the light's
						  // column.  The flood at the light's own height reaches him only where the ground room
						  // under him happens to connect.
						  if (!vis.interior_source || gwin == nullptr) {
							  return false;
						  }
						  const Tile_coord at = main_actor->get_tile();
						  if (at.tz / 5 <= interior_tile.tz / 5) {
							  return false;
						  }
						  bool      rfound = false;
						  const int rz     = Light_room_roof_z(
                                  gwin->get_map(), interior_tile.tx, interior_tile.ty, interior_tile.tz, &rfound);
						  if (rz <= (at.tz / 5) * 5) {
							  return false;
						  }
						  return Tiles_in_same_enclosure(Tile_coord(interior_tile.tx, interior_tile.ty, (at.tz / 5) * 5), at);
					  }();
			if (in_avatar_space) {
				vis.blocked = false;
			} else if (avatar_sealed) {
				// The Avatar's enclosure is completely light-tight: no outside
				// light gets in.
				vis.blocked = true;
			} else {
				// The Avatar's room has an opening, so outside light enters -- but a
				// light source sealed inside its OWN building still can't escape to
				// reach the Avatar (a sealed building must not leak light into a
				// neighbouring open building).
				vis.blocked = !vis.source_can_escape;
			}
		}
		if (use_cache) {
			visibility_cache.store(key, now) = vis;
		}
		return vis;
	}

	// Spatial-light glow radius in game pixels for an intrinsic brightness.
	int Light_radius(int brightness) {
		return brightness * 3 * c_tilesize;    // ~3 tiles per brightness level.
	}

	// Palette tier for a brightness: 0 = candle, 1 = single light, 2 = many.
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
		// Geometric per-tile scan: cache it (Light_layers::build re-asks per
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
					if (obj->as_actor() != nullptr || obj->is_dragable() || Fast_info(obj->get_shapenum()).is_light_source()) {
						continue;    // Bodies, loose items and lamps (a wall torch) are not ceilings.
					}
					const int ol = obj->get_lift();
					if (ol < from || (roof_z >= 0 && ol >= roof_z)) {
						continue;
					}
					if (!Fast_info(obj->get_shapenum()).is_solid()) {
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

	// A spill the light sends UP onto a higher storey of its own building (its stairwell, the floor beyond a
	// stairwell wall's openings): that storey is the spill's to light, unconed and falling off from the light
	// itself.  Not light coming IN, nor a window's glow out onto an open-sky deck (a fan, as on the ground).
	bool Light_spill_upward(const Light_spill& s, int light_tz) {
		if (s.opening.tx == Light_spill_no_cone || s.floor <= light_tz / 5) {
			return false;
		}
		Game_window* const gwin = Game_window::get_instance();
		if (gwin == nullptr || gwin->get_map() == nullptr) {
			return false;
		}
		bool found = false;
		Light_room_roof_z(gwin->get_map(), s.tile.tx, s.tile.ty, s.floor * 5, &found);
		return found;
	}

	// Bit per storey (0..29) the light at lift `light_tz` sends deck spills UP onto (Light_spill_upward).
	int Light_deck_spill_storeys(const std::vector<Light_spill>& spills, int light_tz) {
		int storeys = 0;
		for (const Light_spill& sp : spills) {
			if (Light_spill_upward(sp, light_tz) && sp.floor >= 0 && sp.floor < 30) {
				storeys |= 1 << sp.floor;
			}
		}
		return storeys;
	}

	// Is the wall-line tile (wtx,wty) an OPENING at the storey whose floor is at `lo`, looking into the room
	// tile (itx,ity) behind it?  Returns the light's transmission percent, 0 when closed.  One rule for every
	// storey -- only how much light reaches it differs: a light-passing shape (glass, shutters, grate) in the
	// band passes its own percent; a bare gap (a hole in the wall, a halfwall with air above it) passes 100:
	// free at eye height in the band, solid just below it (the wall it sits on, not open ground), and a real
	// room behind (roofed above the storey's floor, not the open air over a roof).  A blocking shape standing
	// in the gap (a closed shutter on the halfwall) fills the eye height: closed.  `lift_z` receives the
	// opening's lift.
	int Light_upper_opening(Game_map* gmap, int wtx, int wty, int itx, int ity, int lo, int* lift_z) {
		wtx      = Light_tile_norm(wtx);
		wty      = Light_tile_norm(wty);
		int top  = 0;
		int olft = 0;
		if (const int pct = Light_tile_pass_opening(gmap, wtx, wty, lo, lo + 5, &top, &olft); pct > 0) {
			if (lift_z != nullptr) {
				*lift_z = olft;
			}
			return pct;
		}
		Map_chunk* const ch = gmap->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
		if (ch == nullptr) {
			return 0;
		}
		const int lx   = wtx % c_tiles_per_chunk;
		const int ly   = wty % c_tiles_per_chunk;
		bool      free = false;
		for (int z = lo + 1; z <= lo + 3 && !free; ++z) {
			free = !Light_occupied(ch, lx, ly, z);
		}
		if (!free || !Light_occupied(ch, lx, ly, lo - 1)) {
			return 0;
		}
		bool      rfound = false;
		const int rz     = Light_room_roof_z(gmap, Light_tile_norm(itx), Light_tile_norm(ity), lo, &rfound);
		if (!rfound || rz <= lo) {
			return 0;
		}
		if (lift_z != nullptr) {
			*lift_z = lo;
		}
		return 100;
	}

	// Light_room_roof_z for a light's field anchor: a light in a ceiling SLOT
	// (lit neighbours on both opposite sides under a lower ceiling) takes the
	// lower one.  `lit` is the light's (2*rt+1)^2 room grid.
	int Light_anchor_roof_z(Game_map* gmap, int tx, int ty, int lift, int rt, const unsigned char* lit, bool* found) {
		bool      ceiling = false;
		const int roof_z  = Light_room_roof_z(gmap, tx, ty, lift, &ceiling);
		if (found != nullptr) {
			*found = ceiling;
		}
		if (!ceiling || lit == nullptr || rt <= 0) {
			return roof_z;
		}
		const int side    = 2 * rt + 1;
		auto      side_rz = [&](int dx, int dy) {
            if (Grid::Dist(lit[static_cast<size_t>(rt + dy) * side + rt + dx]) == 0) {
                return roof_z;
            }
            return Light_room_roof_z(gmap, tx + dx, ty + dy, lift);
		};
		const int ns = std::max(side_rz(0, -1), side_rz(0, 1));
		const int ew = std::max(side_rz(-1, 0), side_rz(1, 0));
		return std::min(roof_z, std::min(ns, ew));
	}

	// The render skip as a ceiling: a skip set only by a lamp or a body over
	// the viewer (a wall torch overhead) is lifted to the real ceiling.
	int Viewer_ceiling_lift() {
		Game_window* const gwin = Game_window::get_instance();
		const int          skip = gwin->get_render_skip_lift();
		Actor* const       av   = gwin->get_camera_actor();
		Game_map* const    gmap = gwin->get_map();
		if (skip >= 31 || gwin->skip_lift <= skip || av == nullptr || gmap == nullptr) {
			return skip;
		}
		const Tile_coord  at = av->get_tile();
		static Tile_coord last_at(-1, -1, -1);
		static int        last_skip = -1;
		static int        last_res  = -1;
		static Uint64     last_t    = 0;
		const Uint64      now       = SDL_GetTicks();
		if (at == last_at && skip == last_skip && now - last_t < 250) {
			return last_res;
		}
		// The engine's roof test (Map_chunk::is_roof): the lowest blocked
		// level from 4 above the viewer -- minus bodies and lamps.
		const int from = at.tz + 4;
		int       ceil = 255;
		const int lcx  = at.tx / c_tiles_per_chunk;
		const int lcy  = at.ty / c_tiles_per_chunk;
		for (int dcy = -1; dcy <= 1; ++dcy) {
			for (int dcx = -1; dcx <= 1; ++dcx) {
				Map_chunk* const chunk = gmap->get_chunk_safely(lcx + dcx, lcy + dcy);
				if (chunk == nullptr) {
					continue;
				}
				Object_iterator it(chunk->get_objects());
				Game_object*    obj;
				while ((obj = it.get_next()) != nullptr) {
					const Shape_info& info = Fast_info(obj->get_shapenum());
					if (obj->as_actor() != nullptr || !info.is_solid() || info.is_light_source()) {
						continue;
					}
					const int ol = obj->get_lift();
					if (ol + std::max(info.get_3d_height(), 1) <= from || std::max(ol, from) >= ceil) {
						continue;
					}
					if (obj->get_footprint().has_world_point(at.tx, at.ty)) {
						ceil = std::max(ol, from);
					}
				}
			}
		}
		last_at   = at;
		last_skip = skip;
		last_t    = now;
		last_res  = std::max(skip, ceil);
		return last_res;
	}

	// Does the object's frame let light pass (light_passes_through; a 0% entry blocks)?
	bool Object_passes_light(Game_object* obj) {
		if (obj == nullptr) {
			return false;
		}
		return Fast_info(obj->get_shapenum()).get_light_passes_percent(obj->get_framenum()) > 0;
	}

	// A wall-shell member is solid, immovable, non-light-passing (per
	// frame), not an open door leaf, and rises to its tile's FOUND roof.
	// A lamppost (no roof overhead) or a trough under a tree canopy (does not
	// rise to it) is an OBJECT instead.
	static bool Object_is_wall_face_raw(Game_object* obj);

	// Stamp_surface asks this for every object and again for each neighbour it weighs, so it is memoized, keyed by
	// the object and re-checked against its frame and tile (a door swinging re-asks).
	bool Object_is_wall_face(Game_object* obj) {
		if (obj == nullptr) {
			return false;
		}

		struct Memo {
			int        frame;
			Tile_coord tile;
			bool       wall;
		};

		// Kept across frames (the verdict reads only the piece and its room's roof): dropped on any flood-content
		// change (doors, edited objects), every 2 s, and when the shape info is reloaded.
		static std::unordered_map<const Game_object*, Memo> memo;
		static uint64_t                                     memo_gen  = ~0ULL;
		static unsigned                                     memo_info = ~0u;
		static Uint64                                       memo_t    = 0;
		const uint64_t                                      gen       = Flood_content_generation();
		const unsigned info_gen = Shape_manager::get_instance()->get_shapes().get_info_generation();
		const Uint64   now      = SDL_GetTicks();
		if (gen != memo_gen || info_gen != memo_info || now - memo_t > 2000 || memo.size() > 65536) {
			memo.clear();
			memo_gen  = gen;
			memo_info = info_gen;
			memo_t    = now;
		}
		const int        frame = obj->get_framenum();
		const Tile_coord tile  = obj->get_tile();
		if (const auto it = memo.find(obj); it != memo.end() && it->second.frame == frame && it->second.tile == tile) {
			return it->second.wall;
		}
		const bool wall = Object_is_wall_face_raw(obj);
		memo[obj]       = {frame, tile, wall};
		return wall;
	}

	static bool Object_is_wall_face_raw(Game_object* obj) {
		if (obj == nullptr || obj->as_actor() != nullptr || obj->is_dragable()) {
			return false;
		}
		const Shape_info& info = Fast_info(obj->get_shapenum());
		if (!info.is_solid() || info.is_floor(obj->get_framenum()) || info.is_roof(obj->get_framenum())) {
			return false;
		}
		if (info.is_door() && !obj->is_closed_door()) {
			return false;    // An open leaf stands free of the wall line.
		}
		if (info.get_light_passes_percent(obj->get_framenum()) > 0) {
			return false;    // Pane / grate: glows with the light behind it.
		}
		Game_window* const gwin = Game_window::get_instance();
		Game_map* const    gmap = gwin ? gwin->get_map() : nullptr;
		if (gmap == nullptr) {
			return false;
		}
		const Tile_coord t      = obj->get_tile();
		bool             found  = false;
		const int        roof_z = Light_room_roof_z(gmap, t.tx, t.ty, t.tz, &found);
		// Per-SHAPE on purpose: accepting a stacked column here (a hall's
		// course under a window) would light halfwalls in open buildings and
		// re-mask objects drawn over them; this verdict feeds too many
		// consumers to widen.
		return found && t.tz + info.get_3d_height() >= roof_z;
	}

	// Fan perpendicular offset past the opening's mouth; `mouth` packs the
	// opening tiles on the -perp side (low byte) and +perp side (next byte).
	static inline int Cone_perp(int relx, int rely, int cdx, int cdy, int mouth) {
		const int sp = relx * cdy - rely * cdx;
		return sp >= 0 ? std::max(0, sp - ((mouth >> 8) & 0xff)) : std::max(0, -sp - (mouth & 0xff));
	}

	// The fan widens from the opening's wall line, one row behind the apex: no straight base.
	static inline bool Cone_outside(int along, int perp) {
		return along < 0 ? (along < -4 || perp > 1) : perp > along + 1;
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
	// Walk the grid cells on the straight line from (ax, ay) to (gx, gy), in 4 sub-steps per cell, skipping the
	// two end cells.  Returns false at the first cell `blocks` rejects (true if the line is clear).
	template <typename Blocks>
	static bool Line_clear(int ax, int ay, int gx, int gy, Blocks&& blocks) {
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
			if (blocks(cx, cy)) {
				return false;
			}
		}
		return true;
	}

	static void Flood_room_grid(
			Game_map* gmap, const Tile_coord& lt, int rt, int roof_z, std::vector<unsigned char>& lit,
			std::vector<Light_spill>* spills, bool light_walls = true, std::vector<unsigned char>* ring = nullptr, int cone_dx = 0,
			int cone_dy = 0, bool source_under_roof = false, int cone_mouth = 0, std::vector<Light_spill>* arrivals = nullptr) {
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
		std::vector<unsigned char> sightmap;
		Light_flood_stamp_walls(
				gmap, Light_tile_norm(lt.tx - rt), Light_tile_norm(lt.ty - rt), side, roof_z, floor_z, wallmap, &sightmap);
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
		std::vector<int>   openlift(static_cast<size_t>(side) * side, 0);
		auto               opening = [&](int gx, int gy, int* top_z = nullptr) -> int {
            short& m = openmemo[static_cast<size_t>(gy) * side + gx];
            if (m == 0) {
                int       top  = 0;
                int       olft = 0;
                const int pct = Light_tile_pass_opening(gmap, lt.tx + gx - rt, lt.ty + gy - rt, floor_z, roof_z, &top, &olft);
                m                                             = pct > 0 ? static_cast<short>(pct) : -1;
                opentop[static_cast<size_t>(gy) * side + gx]  = top;
                openlift[static_cast<size_t>(gy) * side + gx] = olft;
            }
            if (top_z != nullptr) {
                *top_z = opentop[static_cast<size_t>(gy) * side + gx];
            }
            return m > 0 ? m : 0;
		};
		// Behind a spill's apex only a DOORWAY's corridor may be walked (see the cone below): open floor with
		// headroom at the fan's own floor.  The wall line the light left through -- a sill under a gap whose
		// window shape is gone, the wall course under a raised opening -- takes no light from the fan outside it:
		// light leaving through an opening never lights the outside of the wall it just passed.
		std::vector<signed char> corridor_memo;
		auto                     corridor_cell = [&](int gx, int gy) -> bool {
            if (wallmap[static_cast<size_t>(gy) * side + gx] != 0) {
                return false;
            }
            if (corridor_memo.empty()) {
                corridor_memo.assign(static_cast<size_t>(side) * side, -1);
            }
            signed char& m = corridor_memo[static_cast<size_t>(gy) * side + gx];
            if (m < 0) {
                const int        tx = Light_tile_norm(lt.tx + gx - rt);
                const int        ty = Light_tile_norm(lt.ty + gy - rt);
                Map_chunk* const ch = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
                m = ch != nullptr && !Light_occupied(ch, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, floor_z + 1)
                                    && !Light_occupied(ch, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, floor_z + 2)
												? 1
												: 0;
            }
            return m != 0;
		};
		// Glass a full storey above a passable tile (a high window over an
		// archway): the fill walks UNDER it, so crossing does not escape.
		// Likewise a grate hung over walkable headroom (a raised portcullis): clear below z+3, as for walls.
		std::vector<signed char> overmemo(static_cast<size_t>(side) * side, 0);    // 0 unknown, 1 yes, -1 no.
		auto                     overhead_opening = [&](int gx, int gy) {
            signed char& om = overmemo[static_cast<size_t>(gy) * side + gx];
            if (om != 0) {
                return om > 0;
            }
            om              = -1;
            const int olift = openlift[static_cast<size_t>(gy) * side + gx];
            if (olift - floor_z < 3) {
                return false;
            }
            const int tx = Light_tile_norm(lt.tx + gx - rt);
            const int ty = Light_tile_norm(lt.ty + gy - rt);
            // Static solids only: the chunk's blocked cache counts actors, and an actor beside a raised
            // portcullis must not turn its tile into glass.
            const int cx = tx / c_tiles_per_chunk;
            const int cy = ty / c_tiles_per_chunk;
            for (int dcy = 0; dcy < 2; ++dcy) {
                for (int dcx = 0; dcx < 2; ++dcx) {
                    Map_chunk* const ch = gmap->get_chunk_safely((cx + dcx) % c_num_chunks, (cy + dcy) % c_num_chunks);
                    if (ch == nullptr) {
                        continue;
                    }
                    Object_iterator it(ch->get_objects());
                    Game_object*    o;
                    while ((o = it.get_next()) != nullptr) {
                        if (o->as_actor() != nullptr) {
                            continue;
                        }
                        const Shape_info& info = Fast_info(o->get_shapenum());
                        const int         lo   = o->get_lift();
                        const int         hi   = lo + info.get_3d_height();
                        if (!info.is_solid() || hi <= floor_z || lo >= floor_z + 3) {
                            continue;
                        }
                        if (o->get_footprint().has_world_point(tx, ty)) {
                            return false;
                        }
                    }
                }
            }
            om = 1;
            return true;
		};
		// A non-wall glass tile is a window only when it sits IN a wall line:
		// its run of opening tiles along x or y is closed by wall tiles at
		// both ends (the jambs).  A pane standing free on the room floor has
		// open floor beside it -- light passes it like air, no escape.  The
		// grid edge counts as a jamb (unknown beyond, keep the window).
		std::vector<signed char> inwallmemo(static_cast<size_t>(side) * side, 0);    // 0 unknown, 1 yes, -1 no.
		auto                     in_wall_line = [&](int gx, int gy) {
            signed char& wm = inwallmemo[static_cast<size_t>(gy) * side + gx];
            if (wm != 0) {
                return wm > 0;
            }
            auto jamb = [&](int x, int y, int sx, int sy) {
                for (int k = 1; k <= 8; ++k, x += sx, y += sy) {
                    if (x + sx < 0 || y + sy < 0 || x + sx >= side || y + sy >= side) {
                        return true;
                    }
                    if (tall(x + sx, y + sy)) {
                        return true;
                    }
                    if (opening(x + sx, y + sy) == 0) {
                        return false;
                    }
                }
                return false;
            };
            const bool in = (jamb(gx, gy, 1, 0) && jamb(gx, gy, -1, 0)) || (jamb(gx, gy, 0, 1) && jamb(gx, gy, 0, -1));
            wm = in ? 1 : -1;
            return in;
		};
		auto passes_through = [&](int gx, int gy) {
			return opening(gx, gy) > 0
				   && (tall(gx, gy)
					   || (openlift[static_cast<size_t>(gy) * side + gx] - floor_z < 5 && !overhead_opening(gx, gy)
						   && in_wall_line(gx, gy)));
		};
		// Ceiling-well spill candidates (stairwell / ladder openings): interior
		// roofed tiles whose own ceiling band is missing right above them.
		std::vector<std::pair<int, int>> well_cand;
		// Ceiling-cover map for the well test: which grid tiles have a ceiling
		// piece (floor slab or solid shape, band [roof_z, roof_z+4)) above them.
		// Built lazily ONCE per flood by scanning each covered chunk's object
		// list a single time, not rescanning 4 chunks for every interior tile.
		// Ceilings are usually the upper storey's FLOOR slabs: height-0/1 flats
		// that never set the chunk's blocked flags, hence the shape scan.  An
		// object anchors at its south-east corner with the footprint extending
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
						const Shape_info& info = Fast_info(obj->get_shapenum());
						if (!accept(info, obj->get_lift(), obj->get_framenum())) {
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
			build_cover_if(cover, [&](const Shape_info& info, int ol, int fr) {
				return (info.is_solid() || info.is_floor(fr)) && ol >= band_lo && ol < band_hi;
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
                build_cover_if(roofcover, [&](const Shape_info& info, int ol, int fr) {
                    return (info.is_roof(fr) && ol > 0) || (info.is_floor(fr) && ol >= 5);
                });
            }
            return roofcover[static_cast<size_t>(gy) * side + gx] != 0;
		};
		// Doorway mouth: across the step the roof runs to a wall on both sides (eaves and porches do not).
		auto gap_mouth = [&](int x, int y, int sx, int sy) -> bool {
			const int qx = sy != 0 ? 1 : 0;
			const int qy = sx != 0 ? 1 : 0;
			for (int sgn = -1; sgn <= 1; sgn += 2) {
				bool jamb = false;
				for (int k = 1; k <= 4; ++k) {
					const int ex = x + sgn * k * qx;
					const int ey = y + sgn * k * qy;
					if (ex < 0 || ey < 0 || ex >= side || ey >= side) {
						break;
					}
					if (tall(ex, ey)) {
						jamb = true;
						break;
					}
					if (!real_roofed(ex, ey)) {
						break;
					}
				}
				if (!jamb) {
					return false;
				}
			}
			return true;
		};
		// Floor-well tiles (an elevated room's stairwell / ladder hole DOWN to
		// the storey below): reached tiles whose own floor band is missing
		// beneath them.  Only elevated rooms qualify.
		std::vector<unsigned char> floorcover;
		auto                       floor_open = [&](int gx, int gy) -> bool {
            if (floorcover.empty()) {
                // -1: a slab whose top is flush with the storey floor
                // anchors one z below it.
                build_cover(floorcover, floor_z - 1, floor_z + 4);
            }
            return floorcover[static_cast<size_t>(gy) * side + gx] == 0;
		};
		// Bare window in a tall room: from room tile g, over the lower wall n, through the upper storey's wall plane.
		// Returns the landing tile (grid index) on the deck or open ground beyond, or -1.
		const bool                 tall_room = roof_z > floor_z + 5;
		const int                  bare_lo   = floor_z + 5;
		const int                  bare_hi   = std::min(roof_z, bare_lo + 31);
		const unsigned             bare_all  = tall_room ? (1u << (bare_hi - bare_lo)) - 1 : 0;
		std::vector<unsigned char> midcover;
		auto                       free_bits = [&](int gx, int gy) -> unsigned {
            const int        tx = Light_tile_norm(lt.tx + gx - rt);
            const int        ty = Light_tile_norm(lt.ty + gy - rt);
            Map_chunk* const ch = gmap->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
            if (ch == nullptr) {
                return 0;
            }
            unsigned b = 0;
            for (int z = bare_lo; z < bare_hi; ++z) {
                if (!Light_occupied(ch, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, z)) {
                    b |= 1u << (z - bare_lo);
                }
            }
            return b;
		};
		std::vector<unsigned> fbmemo(tall_room ? static_cast<size_t>(side) * side : 0, ~0u);
		std::vector<short>    deckmemo(tall_room ? static_cast<size_t>(side) * side : 0, 0);    // 0 unknown, -1 none, else top+1.
		std::vector<char>     deckopen(tall_room ? static_cast<size_t>(side) * side : 0, 0);
		auto                  fbits = [&](int x, int y) {
            unsigned& m = fbmemo[static_cast<size_t>(y) * side + x];
            if (m == ~0u) {
                m = free_bits(x, y);
            }
            return m;
		};
		auto deck_at = [&](int x, int y) -> int {
			short& m = deckmemo[static_cast<size_t>(y) * side + x];
			if (m == 0) {
				bool      open = false;
				const int top  = Light_tile_overlooked_deck(gmap, lt.tx + x - rt, lt.ty + y - rt, roof_z, &open);
				m              = top >= 0 ? static_cast<short>(top + 1) : -1;
				deckopen[static_cast<size_t>(y) * side + x] = open ? 1 : 0;
			}
			return m - 1;
		};
		std::vector<signed char> skymemo(tall_room ? static_cast<size_t>(side) * side : 0, 0);    // 0 unknown, 1 sky, -1 roofed.
		auto                     sky_at = [&](int x, int y) {
            signed char& m = skymemo[static_cast<size_t>(y) * side + x];
            if (m == 0) {
                m = Light_tile_roofed(gmap, lt.tx + x - rt, lt.ty + y - rt) ? -1 : 1;
            }
            return m > 0;
		};
		// Like a ground-floor doorway, but up a storey: the line from room tile g passes a wall plane through a
		// gap above the deck's level and lands on the first tile of the deck that is open above it.
		auto bare_over = [&](int gx, int gy, int dx, int dy) -> int {
			if (!tall_room || opening(gx, gy) > 0) {
				return -1;
			}
			if (midcover.empty()) {
				build_cover_if(midcover, [&](const Shape_info& info, int ol, int fr) {
					return (info.is_floor(fr) || info.is_roof(fr)) && ol > floor_z && ol < roof_z;
				});
			}
			// Under a gallery (or a low passage's roof) the room tile only sees that cover's underside.
			if (midcover[static_cast<size_t>(gy) * side + gx] != 0) {
				return -1;
			}
			unsigned path[5];
			path[0] = fbits(gx, gy);
			// Past a wall-line tile: a gap spanning the whole upper course shows no plane in the free bits.
			bool past_wall = false;
			for (int k = 1; k <= 4; ++k) {
				const int x = gx + k * dx;
				const int y = gy + k * dy;
				if (x < 0 || y < 0 || x >= side || y >= side || opening(x, y) > 0) {
					return -1;    // Glass takes the window path.
				}
				const unsigned f  = fbits(x, y);
				const int      dk = deck_at(x, y);
				if (dk >= 0) {
					const unsigned above = bare_all & ~((1u << std::clamp(dk - bare_lo, 0, 31)) - 1);
					if (above != 0 && (f & above) == above) {
						unsigned common = above;
						// Open sky over the surface: the line has left the room's roof, as a doorway's roofed->open step.
						bool crossed = past_wall || deckopen[static_cast<size_t>(y) * side + x] != 0;
						for (int j = 0; j < k; ++j) {
							common &= path[j];
							crossed = crossed || ((path[j] & above) != above);
						}
						if (common == 0) {
							return -1;
						}
						if (crossed) {
							return y * side + x;
						}
						// A ledge in front of the wall plane: the line passes over it.
					}
				} else if (sky_at(x, y)) {
					// Open ground beyond the gap: the glow falls to it, as through glass.
					unsigned common  = bare_all;
					bool     crossed = past_wall;
					for (int j = 0; j < k; ++j) {
						common &= path[j];
						crossed = crossed || path[j] != bare_all;
					}
					return common != 0 && crossed ? y * side + x : -1;
				}
				path[k] = f;
				// Only a wall LINE across the travel direction is crossed; running along the line (sideways
				// through a wide gap's lower course) passes no plane.
				const int  qx     = x + dy;
				const int  qy     = y + dx;
				const int  rx     = x - dy;
				const int  ry     = y - dx;
				const bool line_q = qx >= 0 && qy >= 0 && qx < side && qy < side && tall(qx, qy);
				const bool line_r = rx >= 0 && ry >= 0 && rx < side && ry < side && tall(rx, ry);
				past_wall         = past_wall || (tall(x, y) && line_q && line_r);
			}
			return -1;
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
		// LINE OF SIGHT from the fan's mouth.  The cone alone would let the
		// fill walk through a passage and fan out again past its far end,
		// lighting the passage walls' outer faces and walls beside the exit
		// that no ray from the mouth can reach.  A cell in front of the apex is
		// lit only if a straight line from some mouth tile reaches it without
		// crossing a wall cell.
		const bool               coned_fill = cone_dx != 0 || cone_dy != 0;
		std::vector<signed char> losmemo(coned_fill ? static_cast<size_t>(side) * side : 0, 0);    // 0 unknown, 1 seen, -1 hidden.
		auto                     in_view = [&](int cx, int cy) -> bool {
            signed char& m = losmemo[static_cast<size_t>(cy) * side + cx];
            if (m != 0) {
                return m > 0;
            }
            const int lo   = -(cone_mouth & 0xff);
            const int hi   = (cone_mouth >> 8) & 0xff;
            bool      seen = false;
            for (int k = lo; k <= hi && !seen; ++k) {
                const int mx = rt + k * cone_dy;
                const int my = rt - k * cone_dx;
                if (mx < 0 || my < 0 || mx >= side || my >= side || tall(mx, my)) {
                    continue;
                }
                const int ddx   = cx - mx;
                const int ddy   = cy - my;
                const int steps = 2 * std::max(std::abs(ddx), std::abs(ddy));
                bool      clear = true;
                for (int i = 1; i < steps && clear; ++i) {
                    const float t  = static_cast<float>(i) / static_cast<float>(steps);
                    const int   sx = static_cast<int>(std::floor(mx + 0.5f + t * ddx));
                    const int   sy = static_cast<int>(std::floor(my + 0.5f + t * ddy));
                    if ((sx != cx || sy != cy) && (sx != mx || sy != my) && sightmap[static_cast<size_t>(sy) * side + sx] != 0) {
                        clear = false;
                    }
                }
                seen = clear;
            }
            m = seen ? 1 : -1;
            return seen;
		};
		// A light source is "in a room" only per the OBJECT-level roof test
		// (real roof/floor shapes): the chunk is_roof column also counts the
		// light's OWN head (a streetlamp's glass top) and tree canopies --
		// which would make an exterior lamp read interior, emit spills through
		// its own light-passing head, and esc-strip its whole pool.
		const bool start_roofed = spills != nullptr && source_under_roof && roofed(rt, rt);
		// Has the fill ESCAPED the start enclosure by the time it reaches a
		// cell -- crossed a roof boundary or passed a light-passes opening?
		// Robust against eaves over walkways, unlike per-cell roofed-ness.
		std::vector<unsigned char> esc(static_cast<size_t>(side) * side, start_roofed ? 0 : 1);
		// A light standing IN an opening (a candle on the sill) is on both sides of it: the step back onto the
		// roofed room side crosses nothing.  Only when the far side is open sky does the room side resolve.
		auto inward_from_start = [&](int gx, int gy, int nx, int ny) {
			if (gx != rt || gy != rt || !roofed(nx, ny)) {
				return false;
			}
			const int ox = 2 * gx - nx;
			const int oy = 2 * gy - ny;
			return ox >= 0 && oy >= 0 && ox < side && oy < side && !roofed(ox, oy);
		};
		auto exits_through = [&](int gx, int gy, int nx, int ny) {
			return passes_through(gx, gy) && !inward_from_start(gx, gy, nx, ny);
		};
		// Percent of the light still carried at a cell: the lowest light_passes_through opening it walked through.
		std::vector<unsigned char> trans(static_cast<size_t>(side) * side, 100);
		auto                       trans_after = [&](int gx, int gy, int nx, int ny) -> unsigned char {
            const unsigned char t = trans[static_cast<size_t>(gy) * side + gx];
            return exits_through(gx, gy, nx, ny) ? std::min<unsigned char>(t, static_cast<unsigned char>(opening(gx, gy))) : t;
		};
		// A SPILL grid is built with spills == nullptr, so start_roofed is false
		// by construction and the esc strip below can never run for it -- the fan
		// paints straight into any building it reaches.  Track the mirror image
		// instead: once the fill crosses from open sky under a roof it is INSIDE
		// a foreign building, and stays so.
		std::vector<unsigned char> insid(static_cast<size_t>(side) * side, 0);
		// A spill fan that entered another roofed passage through its mouth (allowed past the inside strip): it
		// lights that passage's floor, but not the faces of its walls -- that would be light spilling twice.  The
		// cells are flagged (Ring::Mouthed_flag) for the splat's face reads; their arrivals stay for the field.
		std::vector<unsigned char> mouthed(spills == nullptr ? static_cast<size_t>(side) * side : 0, 0);
		auto                       mark_mouthed = [&](int gx, int gy, int nx, int ny, bool own_corr) {
            if (mouthed.empty()) {
                return;
            }
            const size_t g = static_cast<size_t>(gy) * side + gx;
            const size_t n = static_cast<size_t>(ny) * side + nx;
            mouthed[n]     = (mouthed[g] != 0
                          || (!own_corr && !real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0
                              && opening(gx, gy) == 0))
														   ? 1
														   : 0;
            if (mouthed[n] != 0 && ring != nullptr && !tall(nx, ny)) {
                Ring::Set(ring->data(), n, Ring::Mouthed_flag);
            }
		};
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
			// A spill's whole MOUTH starts at the apex's distance: a wide opening lights end to end,
			// not fading from its middle tile.  Positive mouth side is (cdy, -cdx) (cf. Cone_perp).
			if ((cone_dx != 0 || cone_dy != 0) && cone_mouth != 0) {
				const int mneg = cone_mouth & 0xff;
				const int mpos = (cone_mouth >> 8) & 0xff;
				for (int k = -mneg; k <= mpos; ++k) {
					const int mx = rt + k * cone_dy;
					const int my = rt - k * cone_dx;
					if (k == 0 || mx < 0 || my < 0 || mx >= side || my >= side || tall(mx, my)) {
						continue;
					}
					const size_t mi = static_cast<size_t>(my) * side + mx;
					if (visited[mi] != 0) {
						continue;
					}
					lit[mi]     = 1;
					visited[mi] = 1;
					queue.emplace_back(mx, my);
				}
			}
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
			// tile with.
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
		// A cell is escaped only if NO path reaches it without escaping: escape crossings wait until the
		// interior fill is done, or a pane in a partition beat the doorway beside it to the same room.
		const bool                      defer_escapes = start_roofed;
		std::vector<std::array<int, 4>> deferred;
		// An OUTDOOR light stops at a building's openings (a doorway between walls, glass, a grate): what it
		// lights inside is the opening's inward pools (below), centred on the opening on every storey alike.
		// So does a spill's fan reaching ANOTHER building (`arrivals`): what enters is that opening's pool.
		const bool stop_at_openings = (spills != nullptr || arrivals != nullptr) && !source_under_roof;
		// The opening's own tile is still reached (the closed glass door's outer face reads it); only the step on
		// from it into the room is not.
		std::vector<unsigned char> threshold(stop_at_openings ? static_cast<size_t>(side) * side : 0, 0);
		auto                       enters_room = [&](int gx, int gy, int nx, int ny) {
            if (!stop_at_openings) {
                return false;
            }
            const size_t gi = static_cast<size_t>(gy) * side + gx;
            const size_t ni = static_cast<size_t>(ny) * side + nx;
            if (threshold[gi] != 0) {
                return real_roofed(nx, ny) && !passes_through(nx, ny);
            }
            if (real_roofed(gx, gy) || !real_roofed(nx, ny)) {
                return false;
            }
            const int dx = nx - gx;
            const int dy = ny - gy;
            if (passes_through(nx, ny)
                || (dx != 0 && dy != 0 ? gap_mouth(nx, ny, dx, 0) || gap_mouth(nx, ny, 0, dy) : gap_mouth(nx, ny, dx, dy))) {
                threshold[ni] = 1;    // Entered, but goes no further in.
                if (ring != nullptr) {
                    Ring::Set(ring->data(), ni, Ring::Threshold_flag);
                }
                return false;
            }
            return passes_through(gx, gy);
		};
		auto crosses_out = [&](int gx, int gy, int nx, int ny) {
			return esc[static_cast<size_t>(gy) * side + gx] == 0
				   && (exits_through(gx, gy, nx, ny) || (roofed(gx, gy) && !roofed(nx, ny)));
		};
		for (;;) {
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
					// axis, starting at the mouth's own width -- wide openings emit several
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
						if (Cone_outside(along, perp)) {
							continue;
						}
						// The corridor is for a DOORWAY mouth (bare gap).  Through a
						// window it walks the fan back in through its own glass and
						// lights the room behind -- the apex sits one tile out, so the
						// first step back is always along -1.
						if (along < 0 && (opening(nx, ny) > 0 || !corridor_cell(nx, ny))) {
							continue;
						}
						if (along > 0 && !tall(nx, ny) && !in_view(nx, ny)) {
							continue;
						}
					}
					const size_t nidx = static_cast<size_t>(ny) * side + nx;
					if (spills != nullptr && esc[static_cast<size_t>(gy) * side + gx] == 0) {
						if (const int land = bare_over(gx, gy, d[0], d[1]); land >= 0) {
							const int lx  = land % side;
							const int ly  = land / side;
							bool      dup = false;
							for (const auto& [px, py, ppct, ptop, ppath, pwx, pwy] : spill_cand) {
								if (px == lx && py == ly && pwx == lx - d[0] && pwy == ly - d[1]) {
									dup = true;
									break;
								}
							}
							if (!dup) {
								const int steps = std::abs(lx - gx) + std::abs(ly - gy);
								spill_cand.emplace_back(lx, ly, 100, roof_z, gdist + steps, lx - d[0], ly - d[1]);
							}
						}
					}
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
								// As for through-flow glass: a deck's supporting wall beyond is still overlooked.
								if (in_grid
									&& (!tall(ox, oy)
										|| Light_tile_overlooked_deck(gmap, lt.tx + ox - rt, lt.ty + oy - rt, open_top) >= 0)) {
									bool dup = false;
									// Keyed on the OPENING as well as the landing tile: another opening's candidate landing there
									// first (a passage cell the deck rule drops later) must not swallow a window tile: the window's
									// run would come out one tile wide, a cone that changes with where the light stands.
									for (const auto& [px, py, ppct, ptop, ppath, pwx, pwy] : spill_cand) {
										if (px == ox && py == oy && pwx == nx && pwy == ny) {
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
					if (spills != nullptr && esc[static_cast<size_t>(gy) * side + gx] == 0 && opening(gx, gy) == 0) {
						// Through-flow INTO glass whose far side is covered: the door
						// path above never fires there (roofed -> roofed), so a tall
						// hall's window over an adjoining floor-roof deck would spill
						// nothing whenever its column is not stamped a wall.  Hand it
						// to the window path, which lands it on the deck (or drops it
						// when the far side is genuinely interior).
						int       open_top = 0;
						const int pct      = opening(nx, ny, &open_top);
						const int ox       = nx + d[0];
						const int oy       = ny + d[1];
						// A ground-floor light sees the deck's supporting wall as tall; the glass still overlooks the deck.
						if (pct > 0 && ox >= 0 && oy >= 0 && ox < side && oy < side && opening(ox, oy) == 0
							&& Light_tile_roofed(gmap, lt.tx + ox - rt, lt.ty + oy - rt)
							&& (!tall(ox, oy)
								|| Light_tile_overlooked_deck(gmap, lt.tx + ox - rt, lt.ty + oy - rt, open_top) >= 0)) {
							bool dup = false;
							for (const auto& [px, py, ppct, ptop, ppath, pwx, pwy] : spill_cand) {
								if (px == ox && py == oy && pwx == nx && pwy == ny) {
									dup = true;
									break;
								}
							}
							if (!dup) {
								spill_cand.emplace_back(ox, oy, pct, open_top, gdist + 2, nx, ny);
							}
						}
					}
					// (Spill fans may cross under roofs: the CONE already keeps the
					// fan out of its own source room, and entering neighbouring
					// interiors through openings/doorways within the fan is the
					// point -- an eave strip does not block the way in.)
					if (visited[nidx]) {
						continue;
					}
					if (enters_room(gx, gy, nx, ny)) {
						continue;
					}
					if (defer_escapes && crosses_out(gx, gy, nx, ny)) {
						deferred.push_back({nx, ny, gx, gy});
						continue;
					}
					visited[nidx] = 1;
					set_dist(nidx, gdist + 1);
					// An OPENING is the legitimate way in (window, grate, doorway), so a
					// crossing through one is not "inside": only a roof the fan reached
					// with no opening at the crossing counts.
					// The fan's own doorway corridor (behind the apex) is the way it
					// came out, not a neighbour's interior: stripping it would leave the
					// doorway floor and its jamb faces dark.
					const bool own_corridor = (cone_dx != 0 || cone_dy != 0) && (nx - rt) * cone_dx + (ny - rt) * cone_dy < 0;
					insid[nidx]             = (insid[static_cast<size_t>(gy) * side + gx] != 0
                                   || (!own_corridor && !real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0
                                       && opening(gx, gy) == 0 && !gap_mouth(nx, ny, d[0], d[1])))
													  ? 1
													  : 0;
					mark_mouthed(gx, gy, nx, ny, own_corridor);
					esc[nidx]   = esc[static_cast<size_t>(gy) * side + gx] != 0 || exits_through(gx, gy, nx, ny)
                                                || (start_roofed && roofed(gx, gy) && !roofed(nx, ny))
										  ? 1
										  : 0;
					trans[nidx] = trans_after(gx, gy, nx, ny);
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
					// chains would leak the fan sideways and behind the mouth.
					if (cone_dx != 0 || cone_dy != 0) {
						const int relx  = nx - rt;
						const int rely  = ny - rt;
						const int along = relx * cone_dx + rely * cone_dy;
						const int perp  = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
						if (Cone_outside(along, perp)) {
							continue;
						}
						if (along < 0 && (opening(nx, ny) > 0 || !corridor_cell(nx, ny))) {
							continue;
						}
						if (along > 0 && !tall(nx, ny) && !in_view(nx, ny)) {
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
						// A corner post is a wall the fill touched: flag it, but record no side arrival.
						if (ring != nullptr && wall_is_face(nx, ny)) {
							Ring::Set(ring->data(), nidx, Ring::Wall_flag);
							Ring::Set(
									ring->data(), nidx,
									esc[static_cast<size_t>(gy) * side + gx] != 0 ? Ring::Escaped_flag : Ring::Inside_flag);
						}
						continue;
					}
					if (visited[nidx] || tall(gx + d[0], gy) || tall(gx, gy + d[1])) {
						continue;
					}
					if (enters_room(gx, gy, nx, ny)) {
						continue;
					}
					if (defer_escapes && crosses_out(gx, gy, nx, ny)) {
						deferred.push_back({nx, ny, gx, gy});
						continue;
					}
					visited[nidx] = 1;
					set_dist(nidx, gdist + 1);
					const bool own_corridor_d = (cone_dx != 0 || cone_dy != 0) && (nx - rt) * cone_dx + (ny - rt) * cone_dy < 0;
					insid[nidx]               = (insid[static_cast<size_t>(gy) * side + gx] != 0
                                   || (!own_corridor_d && !real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0
                                       && opening(gx, gy) == 0 && !gap_mouth(nx, ny, d[0], 0) && !gap_mouth(nx, ny, 0, d[1])))
														? 1
														: 0;
					mark_mouthed(gx, gy, nx, ny, own_corridor_d);
					esc[nidx]   = esc[static_cast<size_t>(gy) * side + gx] != 0 || exits_through(gx, gy, nx, ny)
                                                || (start_roofed && roofed(gx, gy) && !roofed(nx, ny))
										  ? 1
										  : 0;
					trans[nidx] = trans_after(gx, gy, nx, ny);
					queue.emplace_back(nx, ny);
				}
			}
			if (deferred.empty()) {
				break;
			}
			for (const auto& [nx, ny, gx, gy] : deferred) {
				const size_t nidx = static_cast<size_t>(ny) * side + nx;
				if (visited[nidx]) {
					continue;
				}
				const size_t gidx = static_cast<size_t>(gy) * side + gx;
				visited[nidx]     = 1;
				set_dist(nidx, Grid::Dist(lit[gidx]));
				const int ddx = nx - gx;
				const int ddy = ny - gy;
				insid[nidx]   = (insid[gidx] != 0
                               || (!real_roofed(gx, gy) && real_roofed(nx, ny) && opening(nx, ny) == 0 && opening(gx, gy) == 0
                                   && !gap_mouth(nx, ny, ddx, 0) && !gap_mouth(nx, ny, 0, ddy)))
										? 1
										: 0;
				esc[nidx]     = 1;
				trans[nidx]   = trans_after(gx, gy, nx, ny);
				queue.emplace_back(nx, ny);
			}
			deferred.clear();
		}
		// Emit only the candidates whose outside tile the fill itself never
		// reached.  The fill is not strictly interior: through an open doorway
		// or wall gap it escapes and can wrap around the building, touching a
		// window from its OUTSIDE face -- the "far side" of that approach is
		// the room interior, and emitting it would spill the window's glow
		// back INSIDE.  A tile the fill already lit needs no spill glow anyway.
		// A fan's grid collects only the inward pools where it reaches another building's openings.
		std::vector<Light_spill>* const spills_out = spills != nullptr ? spills : arrivals;
		if (spills == nullptr) {
			door_cand.clear();
		}
		if (spills_out != nullptr) {
			{
			}
			// Thin candidates in COORDINATE order, not BFS arrival order: the
			// fill's order follows the light's position, and with it the
			// surviving apex set -- the fan's mouth wandered as a carried
			// torch moved.  The apex set must be pure opening geometry; only
			// each apex's radius/path may depend on the light.
			std::sort(spill_cand.begin(), spill_cand.end());
			std::sort(door_cand.begin(), door_cand.end());
			std::vector<std::pair<Tile_coord, size_t>> emitted_all;
			auto                                       emit_spill = [&](const Tile_coord& t, int pct, int floor_storey, int path,
                                  const Tile_coord& opening = Tile_coord(-1, -1, -1), int deck_z = -1, int mouth_neg = 0,
                                  int mouth_pos = 0) {
                // Ground radius 1: survivors sit <= 2 tiles apart, so the
                // +/-1 cone mouths union into the full opening width.  Keyed on
                // the SOURCE's storey like the candidate thinning above: keying
                // it on the spill's own floor thinned a high window's mouth to a
                // single bubble, leaving one 45-degree cone -- a funnel instead
                // of the wide pool a ground window makes.
                const int dedupe_r = (lt.tz / 5) >= 1 ? 4 : 1;
                for (const auto& [e, ei] : emitted_all) {
                    // Same ground = same storey: a roof-edge exit a storey up must not swallow a window's spill below it.
                    if (std::abs(t.tx - e.tx) <= dedupe_r && std::abs(t.ty - e.ty) <= dedupe_r && t.tz / 5 == e.tz / 5) {
                        // Two openings onto the same ground (a window over a door): the brighter one carries it,
                        // through the union of both mouths (a door's glass and its whole wide gap are one opening).
                        Light_spill&       s   = (*spills_out)[ei];
                        const bool         rep = pct > s.percent;
                        const Light_spill& keep
                                = rep ? Light_spill{t, pct, floor_storey, path, opening, deck_z, mouth_neg, mouth_pos} : s;
                        const Light_spill& other
                                = rep ? s : Light_spill{t, pct, floor_storey, path, opening, deck_z, mouth_neg, mouth_pos};
                        int mneg = keep.mouth_neg;
                        int mpos = keep.mouth_pos;
                        if (keep.opening.tx >= 0 && other.opening.tx >= 0) {
                            const int kdx = keep.tile.tx - keep.opening.tx, kdy = keep.tile.ty - keep.opening.ty;
                            const int odx = other.tile.tx - other.opening.tx, ody = other.tile.ty - other.opening.ty;
                            if (kdx == odx && kdy == ody && (kdx == 0) != (kdy == 0)) {
                                const int k = kdx != 0 ? other.tile.ty - keep.tile.ty : other.tile.tx - keep.tile.tx;
                                mneg = std::max(mneg, other.mouth_neg - k);
                                mpos = std::max(mpos, other.mouth_pos + k);
                            }
                        }
                        if (rep) {
                            s = keep;
                        }
                        s.mouth_neg = mneg;
                        s.mouth_pos = mpos;
                        return;
                    }
                }
                emitted_all.emplace_back(t, spills_out->size());
                spills_out->push_back({t, pct, floor_storey, path, opening, deck_z, mouth_neg, mouth_pos});
			};
			// Path distance the fill recorded at a grid tile (0 when unreached).
			auto path_at = [&](int gx, int gy) {
				const int v = Grid::Dist(lit[static_cast<size_t>(gy) * side + gx]);
				return v > 0 ? v - 1 : 0;
			};
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
                    grouped[ci]                  = 1;
                    const std::array<int, 4> e   = ends(ci);
                    const int                odx = e[0] - e[2];
                    const int                ody = e[1] - e[3];
                    const int                pdx = ody != 0 ? 1 : 0;
                    const int                pdy = odx != 0 ? 1 : 0;
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
				// The spill's storey is that of the SURFACE it lands on (sp_tz): an upper window's glow on
				// the ground below lights no battlement beside the window.
				int        sp_tz    = 0;
				const bool covered  = Light_tile_roofed(gmap, fx, fy);
				int        deck_z   = -1;
				bool       dbg_drop = false;
				if (covered) {
					// Covered -- but when the cover is only a floor-roof deck
					// the window overlooks (its top at or below the opening's
					// top, open sky above), the far side is OUTSIDE after all:
					// the glow lands ON the deck.  Place the spill at the deck's
					// storey floor so it renders at deck height and its storey
					// lets it light the deck's top surface.
					int deck_top = Light_tile_overlooked_deck(gmap, fx, fy, open_top);
					// Light can only land ON a deck it passes ABOVE.  Through a pass-through shape it passes in the
					// shape's own band (a barred window a storey up, over a mountain top roofing the room below).  Through
					// a bare gap, at every height free both at the gap and at the cell before it on the light's side, below
					// that cell's room ceiling: a passage under a high wall, roofed by that same mountain top, is open
					// to the room only below it -- the light goes INTO the passage, never onto its roof -- while a gap
					// high in a hall wall (the window shape removed) is reached from the hall up to its roof.
					if (deck_top >= 0) {
						const size_t widx = static_cast<size_t>(wy) * side + wx;
						int          pass_hi;
						if (opening(wx, wy) > 0) {
							pass_hi = opentop[widx];
						} else {
							const int  otx    = Light_tile_norm(lt.tx + wx - rt);
							const int  oty    = Light_tile_norm(lt.ty + wy - rt);
							const int  ptx    = Light_tile_norm(otx - (ox - wx));
							const int  pty    = Light_tile_norm(oty - (oy - wy));
							Map_chunk* och    = gmap->get_chunk_safely(otx / c_tiles_per_chunk, oty / c_tiles_per_chunk);
							Map_chunk* pch    = gmap->get_chunk_safely(ptx / c_tiles_per_chunk, pty / c_tiles_per_chunk);
							bool       pfound = false;
							const int  pceil  = Light_room_roof_z(gmap, ptx, pty, floor_z, &pfound);
							const int  zmax   = std::min(open_top, pfound ? pceil : open_top);
							pass_hi           = floor_z;
							for (int z = floor_z; och != nullptr && pch != nullptr && z < zmax; ++z) {
								if (!Light_occupied(och, otx % c_tiles_per_chunk, oty % c_tiles_per_chunk, z)
									&& !Light_occupied(pch, ptx % c_tiles_per_chunk, pty % c_tiles_per_chunk, z)) {
									pass_hi = z + 1;
								}
							}
						}
						if (pass_hi <= deck_top) {
							deck_top = -1;
						}
					}
					if (deck_top < 0) {
						// A gate passage, not a room: straight on it reaches open sky past the arch's cover
						// (a lowered portcullis under a beam) without meeting a wall.
						const int sdx   = ox - wx;
						const int sdy   = oy - wy;
						bool      exits = false;
						for (int k = 0; k <= 4; ++k) {
							const int gx = ox + sdx * k;
							const int gy = oy + sdy * k;
							if (gx < 0 || gy < 0 || gx >= side || gy >= side || tall(gx, gy)) {
								break;
							}
							if (!roofed(gx, gy)) {
								exits = true;
								break;
							}
						}
						if (!exits) {
							dbg_drop = true;
						}
					} else if (Light_tile_overlooked_deck(gmap, fx + (ox - wx), fy + (oy - wy), open_top) == deck_top) {
						// (A ledge one tile deep under the window is no landing: the glow passes over it to the ground.)
						sp_tz  = (deck_top / 5) * 5;
						deck_z = deck_top;
					}
				}
				const int floor = sp_tz / 5;
				{
				}
				if (dbg_drop) {
					return false;    // Far side is interior (or under another roof).
				}
				emit_spill(
						Tile_coord(Light_tile_norm(fx), Light_tile_norm(fy), sp_tz), pct, floor, cpath,
						Tile_coord(Light_tile_norm(lt.tx + wx - rt), Light_tile_norm(lt.ty + wy - rt), 0), deck_z, mouth_neg,
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
				// roofed->open step).  A light->exit axis would point a wide
				// doorway's off-axis exits sideways along the facade.
				const Tile_coord otile = Tile_coord(Light_tile_norm(lt.tx + wx - rt), Light_tile_norm(lt.ty + wy - rt), 0);
				const int        etx   = Light_tile_norm(lt.tx + ox - rt);
				const int        ety   = Light_tile_norm(lt.ty + oy - rt);
				// Anchor the bubble at the source room's own storey floor: an
				// upstairs door exits onto a walkway/deck at that level, and a
				// ground-anchored bubble would flood the wrong (ground) room
				// and slide its lattice off the deck.
				// NOT deck-anchored: raising the storey here would let the spill
				// past the whole-unit gate onto the walkway's upper wall
				// courses.
				// Tall-hall windows reach the window path instead (door_cand=0).
				int sp_tz     = floor_z;
				int door_deck = 0;
				// An upper storey's WINDOW (glass/shutters the fill walked through) has no walkway beyond it:
				// the glow falls to whatever lies below -- a lower deck, or the ground -- as the window path does.
				if (floor_z >= 5 && opening(wx, wy) > 0) {
					const int dk = Light_tile_overlooked_deck(gmap, etx, ety, floor_z + 1);
					if (dk < floor_z - 1) {
						sp_tz     = dk >= 0 ? (dk / 5) * 5 : 0;
						door_deck = dk >= 0 ? dk : -1;
					}
				}
				const int sp_floor = sp_tz / 5;
				// Glass the fill walked through on the way out (or the exit tile itself) passes only its percent.
				int pct = trans_after(wx, wy, ox, oy);
				if (passes_through(ox, oy)) {
					pct = std::min(pct, opening(ox, oy));
				}
				emit_spill(Tile_coord(etx, ety, sp_tz), pct, sp_floor, path_at(ox, oy), otile, door_deck, door_mneg, door_mpos);
				return true;
			};
			group_openings(
					door_cand.size(),
					[&](size_t i) {
						const auto& [x, y, wx, wy] = door_cand[i];
						return std::array<int, 4>{x, y, wx, wy};
					},
					emit_door);
			// Inward spills: an OUTSIDE light reaching an upper storey's window from the ground lights the room
			// behind it.  Upper storeys only (ground rooms take outside light through their doors); the glow is
			// scaled by how far below the opening the light stands: (light z + 5) / (opening lift + 5).
			if (!source_under_roof) {
				struct Inward {
					int ix, iy, wx, wy, pct, storey;
				};

				std::vector<Inward> inward;
				for (int gy = 1; gy + 1 < side; ++gy) {
					for (int gx = 1; gx + 1 < side; ++gx) {
						if (!roofed(gx, gy)) {
							continue;
						}
						// A fan's arrivals lie ahead of its mouth: never its own building's openings behind it.
						if (spills == nullptr && (gx - rt) * cone_dx + (gy - rt) * cone_dy <= 0) {
							continue;
						}
						for (const auto& d : step) {
							const int ix = gx + d[0];
							const int iy = gy + d[1];
							const int ox = gx - d[0];
							const int oy = gy - d[1];
							if (ix < 0 || iy < 0 || ix >= side || iy >= side || ox < 0 || oy < 0 || ox >= side || oy >= side) {
								continue;
							}
							// Room behind, open sky in front, and the light on the open side.
							if (!roofed(ix, iy) || roofed(ox, oy) || Grid::Dist(lit[static_cast<size_t>(oy) * side + ox]) == 0
								|| (rt - gx) * d[0] + (rt - gy) * d[1] >= 0) {
								continue;
							}
							// The light's own storey: a doorway between walls, or a light-passing shape in the wall line.
							if (stop_at_openings && real_roofed(ix, iy)) {
								int       gtop = 0;
								int       glft = 0;
								const int gtx  = Light_tile_norm(lt.tx + gx - rt);
								const int gty  = Light_tile_norm(lt.ty + gy - rt);
								int       gpct = Light_tile_pass_opening(gmap, gtx, gty, floor_z, floor_z + 5, &gtop, &glft);
								if (gpct <= 0 && gap_mouth(gx, gy, d[0], d[1])) {
									Map_chunk* const gch = gmap->get_chunk_safely(gtx / c_tiles_per_chunk, gty / c_tiles_per_chunk);
									bool             gfree = gch != nullptr;
									for (int z = floor_z; z <= floor_z + 2 && gfree; ++z) {
										gfree = !Light_occupied(gch, gtx % c_tiles_per_chunk, gty % c_tiles_per_chunk, z);
									}
									gpct = gfree ? 100 : 0;
								}
								if (gpct > 0) {
									inward.push_back({ix, iy, gx, gy, gpct, floor_z / 5});
									continue;
								}
							}
							for (int k = 1; k <= 3; ++k) {
								const int lo   = floor_z + 5 * k;
								int       olft = 0;
								const int pct  = Light_upper_opening(
                                        gmap, lt.tx + gx - rt, lt.ty + gy - rt, lt.tx + ix - rt, lt.ty + iy - rt, lo, &olft);
								if (pct <= 0) {
									continue;
								}
								// The step down is softened (sqrt): the plain ratio left only a glow seen with every
								// other light off.
								const float f
										= std::min(1.0f, std::sqrt(static_cast<float>(lt.tz + 5) / static_cast<float>(olft + 5)));
								const int ipt = std::max(1, static_cast<int>(static_cast<float>(pct) * f + 0.5f));
								inward.push_back({ix, iy, gx, gy, ipt, lo / 5});
								break;
							}
						}
					}
				}
				std::sort(inward.begin(), inward.end(), [](const Inward& a, const Inward& b) {
					return std::tie(a.ix, a.iy, a.wx, a.wy) < std::tie(b.ix, b.iy, b.wx, b.wy);
				});
				group_openings(
						inward.size(),
						[&](size_t i) {
							return std::array<int, 4>{inward[i].ix, inward[i].iy, inward[i].wx, inward[i].wy};
						},
						[&](size_t i, int mneg, int mpos) {
							const Inward& c = inward[i];
							// No cone: the light passes through and pools in the room like outside light through a
							// ground-floor door, dimmed by the ratio.  A round pool fades from its centre, so a long
							// opening gets one every 3 tiles; round pools overlap smoothly (cones left wedges).
							const int pdx = c.iy != c.wy ? 1 : 0;
							const int pdy = c.ix != c.wx ? 1 : 0;
							const int n   = mneg + mpos + 1;
							// Pools placed SYMMETRICALLY about the opening's centre, 3 tiles apart: an odd run
							// centres one on its middle tile, an even run a pair on its two middle tiles (centre
							// between them).  Cut into chunks from one end, a 4-tile window sat lopsided.
							std::vector<int> offs;    // Tile index along the run, 0..n-1.
							if (n % 2 == 1) {
								const int m = (n - 1) / 2;
								for (int j = 0; m - j >= 0; j += 3) {
									offs.push_back(m - j);
									if (j > 0 && m + j < n) {
										offs.push_back(m + j);
									}
								}
							} else {
								const int lo_m = n / 2 - 1;
								for (int j = 0; lo_m - j >= 0; j += 3) {
									offs.push_back(lo_m - j);
									offs.push_back(lo_m + 1 + j);
								}
							}
							for (const int o : offs) {
								const int mid = o - mneg;    // Offset from c along the run.
								const int swx = c.wx + pdx * mid;
								const int swy = c.wy + pdy * mid;
								// The pool's centre lies straight into the room from the window: nearer when the light
								// stands close (a steep beam), farther when it is far -- round(distance / 4), 1..3 tiles.
								// Fewer tiles when that step is not open room floor, down to the window tile itself.
								const int    idx = c.ix - c.wx;
								const int    idy = c.iy - c.wy;
								const int    ddx = swx - rt;
								const int    ddy = swy - rt;
								const double dlt = std::sqrt(static_cast<double>(ddx * ddx + ddy * ddy));
								// On the light's own storey the light comes in level: the pool sits at the opening.
								int pdist = c.storey == floor_z / 5 ? (tall(swx, swy) ? 1 : 0)
																	: std::clamp(static_cast<int>(std::lround(dlt / 4.0)), 1, 3);
								for (; pdist > 0; --pdist) {
									bool ok = true;
									for (int k = 1; k <= pdist && ok; ++k) {
										const int qx = swx + idx * k;
										const int qy = swy + idy * k;
										ok = qx >= 0 && qy >= 0 && qx < side && qy < side && !tall(qx, qy) && roofed(qx, qy);
									}
									if (ok) {
										break;
									}
								}
								const int itx = Light_tile_norm(lt.tx + swx + idx * pdist - rt);
								const int ity = Light_tile_norm(lt.ty + swy + idy * pdist - rt);
								const int dx  = swx - rt;
								const int dy  = swy - rt;
								const int pth = static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
								// Straight in, not through emit_spill: its dedupe merges pools one tile apart (the even
								// run's central pair), and an outdoor light has no outward spills to merge with.
								const Tile_coord st(itx, ity, c.storey * 5);
								emitted_all.emplace_back(st, spills_out->size());
								// The opening's own tile rides in the marker's y/z: the pool's beam goes through it.
								spills_out->push_back(
										{st, c.pct, c.storey, pth,
										 Tile_coord(
												 Light_spill_no_cone, Light_tile_norm(lt.tx + swx - rt),
												 Light_tile_norm(lt.ty + swy - rt)),
										 -1, 0, 0, idx, idy});
							}
							return true;
						});
			}
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
						&& !Light_occupied(nch, ntx % c_tiles_per_chunk, nty % c_tiles_per_chunk, roof_z - 2)) {
						return true;    // Upper floor with air beneath: a landing.
					}
				}
				return false;
			};
			// Light climbs through a ceiling only by a STAIRWELL: a hole of limited size in the room's ceiling,
			// ringed by that ceiling (the upper floor) or walls.  A tall hall's whole open centre, next to the low
			// passage or gallery the lamp stands under, is no hole: the light does not climb into it.
			std::vector<int> hole_size(static_cast<size_t>(side) * side, -1);
			auto             stair_hole = [&](int hx, int hy) {
                const size_t hi = static_cast<size_t>(hy) * side + hx;
                if (hole_size[hi] < 0) {
                    std::vector<std::pair<int, int>> q{
										{hx, hy}
                    };
                    std::vector<size_t> members{hi};
                    hole_size[hi]  = 0;
                    bool open_edge = false;
                    for (size_t qi = 0; qi < q.size(); ++qi) {
                        for (const auto& d : step) {
                            const int nx = q[qi].first + d[0];
                            const int ny = q[qi].second + d[1];
                            if (nx < 0 || ny < 0 || nx >= side || ny >= side) {
                                open_edge = true;    // Runs off the grid: no bounded hole.
                                continue;
                            }
                            const size_t ni = static_cast<size_t>(ny) * side + nx;
                            if (hole_size[ni] >= 0 || tall(nx, ny) || !ceil_open(nx, ny)) {
                                continue;
                            }
                            hole_size[ni] = 0;
                            members.push_back(ni);
                            q.emplace_back(nx, ny);
                        }
                    }
                    const int n = open_edge ? 1 << 20 : static_cast<int>(members.size());
                    for (const size_t m : members) {
                        hole_size[m] = n;
                    }
                }
                return hole_size[hi] <= 48;
			};
			std::vector<std::pair<int, int>> well_emitted;
			// A well is a hole in the light's OWN room's ceiling: reached without passing a door.  Through an open
			// door the fill may walk into a taller hall next door, whose open space beside its gallery is no well
			// of this room.
			std::vector<unsigned char> own_room(static_cast<size_t>(side) * side, 0);
			// The doors standing in this storey's band, once over the grid's chunks.
			std::vector<unsigned char> doormask(static_cast<size_t>(side) * side, 0);
			{
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
						Game_object*    o;
						while ((o = it.get_next()) != nullptr) {
							if (!Fast_info(o->get_shapenum()).is_door() || o->get_lift() < floor_z
								|| o->get_lift() >= floor_z + 5) {
								continue;
							}
							const TileRect fp = o->get_footprint();
							for (int ty = fp.y; ty < fp.y + fp.h; ++ty) {
								for (int tx = fp.x; tx < fp.x + fp.w; ++tx) {
									int gx = Light_tile_norm(tx) - tx0;
									int gy = Light_tile_norm(ty) - ty0;
									if (gx < 0) {
										gx += c_num_tiles;
									}
									if (gy < 0) {
										gy += c_num_tiles;
									}
									// An open leaf swings onto the tile beside its doorway: the doorway is next to it.
									for (int dy = -1; dy <= 1; ++dy) {
										for (int dx = -1; dx <= 1; ++dx) {
											const int ux = gx + dx;
											const int uy = gy + dy;
											if (ux >= 0 && uy >= 0 && ux < side && uy < side) {
												doormask[static_cast<size_t>(uy) * side + ux] = 1;
											}
										}
									}
								}
							}
						}
					}
				}
			}
			auto door_at = [&](int gx, int gy) {
				return doormask[static_cast<size_t>(gy) * side + gx] != 0;
			};
			{
				std::vector<std::pair<int, int>> q{
						{rt, rt}
                };
				own_room[static_cast<size_t>(rt) * side + rt] = 1;
				for (size_t qi = 0; qi < q.size(); ++qi) {
					for (const auto& d : step) {
						const int nx = q[qi].first + d[0];
						const int ny = q[qi].second + d[1];
						if (nx < 0 || ny < 0 || nx >= side || ny >= side) {
							continue;
						}
						const size_t ni = static_cast<size_t>(ny) * side + nx;
						if (own_room[ni] != 0 || Grid::Dist(lit[ni]) == 0 || tall(nx, ny) || passes_through(nx, ny)
							|| door_at(nx, ny)) {
							continue;
						}
						// A doorway in a wall line that opens straight onto a space without the room's ceiling is the
						// way into a taller hall next door, not into a stairwell of the light's own room (the leaf of
						// an open door may stand anywhere around it).
						const int bx = nx + d[0];
						const int by = ny + d[1];
						if (!ceil_open(nx, ny) && bx >= 0 && by >= 0 && bx < side && by < side && ceil_open(bx, by)
							&& gap_mouth(nx, ny, d[0], d[1])) {
							continue;
						}
						own_room[ni] = 1;
						q.emplace_back(nx, ny);
					}
				}
			}
			for (const auto& [ox, oy] : well_cand) {
				if (own_room[static_cast<size_t>(oy) * side + ox] == 0 || !stair_hole(ox, oy)) {
					continue;
				}
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
                return Line_clear(ax, ay, gx, gy, [&](int cx, int cy) {
                    return sightmap[static_cast<size_t>(cy) * side + cx] != 0 && opening(cx, cy) == 0;
                });
			};
			// Rays leave from every tile of the opening's mouth.
			const int mneg      = cone_mouth & 0xff;
			const int mpos      = (cone_mouth >> 8) & 0xff;
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
			// A FAN only: an unconed pool (light in through an opening, up a well) is not a ray bundle from one
			// point -- from its single apex a pillar in the room cast a hard-edged hole through it.
			if (spill_grid && (cone_dx != 0 || cone_dy != 0)) {
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
		// exist for spill detection and the ring's arrival data only: drop them
		// from the PAINTED field, or the room light shows outdoors as a free
		// bubble "as if there were no walls" -- outdoor light is the coned
		// SPILL grids' job.  EXCEPTION: keep exactly the escaped cells an
		// INTERIOR pixel samples.  The field lattice is anchored at wall-top,
		// 4px/z up-left of ground, so a pixel over interior tile T reads the
		// cell T + (s..s+1, s..s+1) diagonally down-right (s = roof_z/2 tiles).
		// Near south/east walls and at a doorway mouth those cells are
		// OUTDOORS; stripping them would gray the strip.  A kept cell's own
		// display position is back over the interior, so no outdoor glow can
		// result.  INTERIOR grids only: an exterior light's esc is all-ones by
		// construction and stripping it blanks the whole pool.  Wall cells
		// never carry esc and keep their ring values; the Grid::Flag shaft
		// flags survive.
		if (spills != nullptr && start_roofed) {
			// Must match the render's field anchor (Light_layers::build).
			const int anchor_z = Light_anchor_roof_z(gmap, lt.tx, lt.ty, lt.tz, rt, lit.data());
			const int olo      = anchor_z > 0 ? anchor_z / 2 : 2;
			const int ohi      = olo + 1;
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
					if (ring != nullptr) {
						Ring::Set(ring->data(), i, Ring::Handoff_flag);
					}
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
	// lights and their window/door/well spills in view, re-flooding ALL of them
	// every frame is too slow.  Results are reused for a short time-to-live --
	// and beyond it, a per-frame REFRESH BUDGET spreads the recomputation out:
	// entries created together (entering a town) would otherwise all expire
	// together, stacking dozens of floods into single frames every quarter
	// second.  A stale entry past the budget keeps being served as-is; each
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

	// Per-frame pixel -> top owner, shared by every light's splat (the lookup only depends on the frame's
	// sprites and channels, rebuilt with the mask each frame).  Entries carry the frame generation, so a
	// new frame needs no clearing; splats on worker threads may race to fill a pixel with the same value.
	static std::atomic<uint32_t>              top_owner_gen{1};
	static std::vector<std::atomic<uint64_t>> top_owner_cache;
	static std::mutex                         top_owner_cache_mx;
	static std::vector<std::atomic<uint64_t>> unit_best_cache;    // Same scheme: pixel -> unit_alpha's unit box.

	// Call once per world render: replenishes the flood cache's per-frame refresh budget (see the cache above).
	void Flood_cache_frame_begin() {
		flood_spend = 0;
		shape_info_gen.fetch_add(1, std::memory_order_relaxed);
		top_owner_gen.fetch_add(1, std::memory_order_relaxed);
	}

	// Mixed into the light-layer signatures so cached coverage rebuilds exactly
	// when flood content changes (a door opened or closed).
	uint64_t Flood_content_generation() {
		return flood_content_gen;
	}

	// A light-blocking shape near `t` changed: evict cached floods/verdicts
	// within `radius_tiles`.
	void Invalidate_light_caches_near(const Tile_coord& t, int radius_tiles) {
		Light_deck_verdicts_clear();
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
		const Shape_info& inf = Fast_info(obj->get_shapenum());
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
	// exits are open air and carry 100).  An empty grid means nothing to gate;
	// `ring`, when given, gets the per-wall-cell face arrival distances.
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
		const Tile_coord      lt0 = light_obj->get_tile();
		const Tile_coord      lt(lt0.tx, lt0.ty, Light_flood_z(gmap, lt0));
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
	// shows through the walls' interior faces).  A nonzero (cone_dx, cone_dy)
	// unit axis restricts the fill to the outward fan in front of the opening
	// (45 degrees each side); (0,0) floods freely.  A fan stops at another
	// building's openings; `arrivals` gets the inward pools they let in
	// (tile/path relative to the fan's apex, percent of the opening alone).
	void Build_spill_shadow_grid(
			const Tile_coord& start, int rt, std::vector<unsigned char>& lit, bool light_walls, std::vector<unsigned char>* ring,
			int cone_dx, int cone_dy, int cone_mouth, std::vector<Light_spill>* arrivals) {
		const int side = 2 * rt + 1;
		lit.assign(static_cast<size_t>(side) * side, 0);
		if (arrivals != nullptr) {
			arrivals->clear();
		}
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
			if (arrivals != nullptr) {
				*arrivals = hit->spills;
			}
			return;
		}
		const Flood_spend_scope    spend;
		const int                  roof_z = Light_room_roof_z(gmap, start.tx, start.ty, start.tz);
		std::vector<unsigned char> ringv;
		// A fan reaching another building's openings: the pools they let in (doorway / well exits stay unconed).
		std::vector<Light_spill> arr;
		Flood_room_grid(
				gmap, start, rt, roof_z, lit, nullptr, light_walls, &ringv, cone_dx, cone_dy, false, cone_mouth,
				cone_dx != 0 || cone_dy != 0 ? &arr : nullptr);
		Flood_cache_entry& entry = flood_cache[key];
		const bool         had   = entry.stamp != 0;
		if (had && entry.lit == lit && entry.ring == ringv && Spills_equal(entry.spills, arr)) {
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
		entry.spills = arr;
		if (arrivals != nullptr) {
			*arrivals = std::move(arr);
		}
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

	// Grow a splat's unclamped screen bbox up-left by the art boxes of the
	// sprites rooted inside it.  The caller's bounds bookkeeping MUST apply
	// this too: coverage written outside the recorded bounds is never cleared,
	// and the scroll path then translates those stale pixels across the screen.
	void Reach_extend_box(int& x0, int& y0, int x1, int y1, const Sprite_box* sprites, int nsprites) {
		if (sprites == nullptr) {
			return;
		}
		const int px0 = x0;
		const int py0 = y0;
		for (int i = 0; i < nsprites; ++i) {
			const Sprite_box& s = sprites[i];
			if (s.wall || s.fx < px0 || s.fx > x1 || s.fy < py0 || s.fy > y1) {
				continue;    // Not rooted in this pool.
			}
			if (s.x0 >= px0 && s.y0 >= py0) {
				continue;    // Already covered.
			}
			x0 = std::min(x0, static_cast<int>(s.x0));
			y0 = std::min(y0, static_cast<int>(s.y0));
		}
	}

	// The light being splatted's Light_render_info::deck_spill_storeys (set by Splat_light).
	static int Splat_deck_spill_storeys = 0;
	// The light being splatted's Light_render_info::from_source / source_dx / source_dy (set by Splat_light).
	static bool Splat_from_source = false;
	// How far Splat_light raised a light's screen centre up-left to its flame (the sprite's elevation); the free
	// dome measures ground distance from the light's FOOT, as the field does, so inside and outside views agree.
	static int Splat_emit_shift = 0;
	// The light being splatted's Light_render_info::pool_shape (set by Splat_light).
	static bool Splat_pool_shape = false;
	static int  Splat_pool_wdx   = 0;
	static int  Splat_pool_wdy   = 0;
	static int  Splat_pool_nx    = 0;
	static int  Splat_pool_ny    = 0;
	static int  Splat_source_dx  = 0;
	static int  Splat_source_dy  = 0;

	// An inward pool's shape at tile offset (rx,ry) from its centre: full within its radius (fading over the last
	// two tiles) and in the opening's FAN into the room -- from the opening (wdx,wdy) on along the inward step
	// (nx,ny), widening a tile per tile from +-1.5 tiles at the opening, a tile of penumbra.  A room corner beside
	// the door along the wall is outside it, however near the light; the room behind the door is in it.
	// Shared by the splat and Relight_sprites.
	static float Light_pool_fade(int radius, int wdx, int wdy, int nx, int ny, int rx, int ry) {
		const float fx = static_cast<float>(rx);
		const float fy = static_cast<float>(ry);
		const float d  = std::sqrt(fx * fx + fy * fy) * static_cast<float>(c_tilesize);
		float       f  = std::clamp((static_cast<float>(radius) - d) / (2.0f * static_cast<float>(c_tilesize)), 0.0f, 1.0f);
		// Only nothing BEHIND the opening's wall line (the pool spills into the room, not back out): no fan.  Its
		// centred falloff already dims a corner beside the opening.
		if (nx != 0 || ny != 0) {
			const float depth = (fx - static_cast<float>(wdx)) * static_cast<float>(nx)
								+ (fy - static_cast<float>(wdy)) * static_cast<float>(ny);
			f *= std::clamp(depth + 1.0f, 0.0f, 1.0f);
		}
		return f;
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
	void Splat_radial_light(
			unsigned char* cov, unsigned char* dstpix, const unsigned char* srcpix, int W, int H, int dst_lw, int src_lw, int sx,
			int sy, int radius, int elevation, int dist_bias, int intensity_pct, const unsigned char* roofpix, int roof_lw,
			bool veto_roof, bool is_spill, int spill_floor, int light_top_storey, int light_floor_storey, int anchor_z,
			const unsigned char* grid, int grid_rt, int grid_fx, int grid_fy, bool inside_viewer, int clip_x0, int clip_y0,
			int clip_x1, int clip_y1, const unsigned char* kindpix, int kind_lw, const unsigned char* footdx,
			const unsigned char* footdy, int foot_lw, const unsigned char* ring, int av_fx, int av_fy, const Sprite_box* sprites,
			int nsprites, int spill_deck_z, int cone_dx, int cone_dy, int cone_mouth, int light_tx, int light_ty, TileRect* wrote) {
		if (radius <= 0 || intensity_pct <= 0) {
			return;
		}
		// A pixel's stamped foot: the ground point (screen, z = 0) under the surface it shows.  The offset from the
		// pixel is 4px per lift of that surface, so foot_lift is its height in lifts.
		auto foot_x = [&](int px, int py) -> int {
			return px + static_cast<int>(footdx[static_cast<size_t>(py) * foot_lw + px]) - Foot_bias;
		};
		auto foot_y = [&](int px, int py) -> int {
			return py + static_cast<int>(footdy[static_cast<size_t>(py) * foot_lw + px]) - Foot_bias;
		};
		auto foot_lift = [&](int px, int py) -> float {
			return static_cast<float>(static_cast<int>(footdx[static_cast<size_t>(py) * foot_lw + px]) - Foot_bias) / 4.0f;
		};
		// A surface above the light's room ceiling (the upper part of a tall hall's wall a light reaches round a
		// corner from below): full at the ceiling, gone a storey and a half above it.
		auto above_ceiling_fade = [&](int px, int py) -> float {
			return std::clamp(1.0f - (foot_lift(px, py) - 5.0f * static_cast<float>(light_top_storey)) / 7.5f, 0.0f, 1.0f);
		};
		const bool spill_on_deck = spill_deck_z >= 0;
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
		const float e  = static_cast<float>(elevation > 0 ? elevation : 0);
		const float e2 = e * e;
		// A deck spill measured from its source: the reach stays the source's, the distance is the source's own.
		const bool  from_source = is_spill && Splat_from_source && grid != nullptr;
		const float bias0       = static_cast<float>(dist_bias > 0 ? dist_bias : 0);
		const float bias        = bias0;
		// The field's distance offset: a from-source deck spill measures the whole way from its source itself.
		const float field_bias = from_source ? 0.0f : bias;
		const float full       = rf + bias0;          // Ground reach of the original bubble.
		const float rf2        = full * full + e2;    // Square of its 3D reach (never 0).
		// The dome's height (1 at the source, 0 at the reach) for a travelled distance in px.  A spill continues
		// its source's falloff: the distance starts at `bias`.
		auto dome_at = [&](float travel_px) -> float {
			return 1.0f - (travel_px * travel_px + e2) / rf2;
		};
		// FAN PENUMBRA.  Build_spill_shadow_grid clips the fill to a 45-degree
		// cone in front of the opening, and that clip is binary: a cell is in
		// the fan at full brightness or out of it at nothing.  An aperture
		// casts a penumbra, so the last few tiles before the cone's edge fade
		// out instead.  Without it the edge is a straight line drawn across
		// whatever it crosses -- a tree standing on the boundary would have its
		// crown cut in half.
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
            // Rows from the opening's wall line, where the fan starts.
            const int   reach  = along + 1;
            const float half_w = 0.5f * static_cast<float>(reach + 1);
            const float band   = half_w < 1.0f ? 1.0f : (half_w > penumbra ? static_cast<float>(penumbra) : half_w);
            const float f      = static_cast<float>(reach + 1 - perp) / (band + 1.0f);
            return f >= 1.0f ? 1.0f : (f <= 0.0f ? 1.0f / (band + 1.0f) : f);
		};
		// An inward pool's shape: full inside its own radius around the opening, fading over the last two tiles.
		const bool pool_shaped = is_spill && Splat_pool_shape && grid != nullptr;
		// The direction the light travels for the "faces turned away are never hit" rule: a fan's axis, or an
		// inward pool's step into the room (the inner face of the halfwall under a window faces away from it).
		const int back_dx   = cone_dx != 0 || cone_dy != 0 ? cone_dx : (pool_shaped ? Splat_pool_nx : 0);
		const int back_dy   = cone_dx != 0 || cone_dy != 0 ? cone_dy : (pool_shaped ? Splat_pool_ny : 0);
		auto      pool_fade = [&](int gx, int gy) -> float {
            return pool_shaped ? Light_pool_fade(
                                         radius, Splat_pool_wdx, Splat_pool_wdy, Splat_pool_nx, Splat_pool_ny, gx - grid_rt,
                                         gy - grid_rt)
									: 1.0f;
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
		float uc          = 0.0f;
		float vc          = 0.0f;
		float base_px     = 0.0f;
		float path_rebase = 0.0f;    // The path's own rebase to the splat centre's cell (before the straight override).

		if (grid != nullptr) {
			uc = (static_cast<float>(sx) - grid_u) / cell;
			vc = (static_cast<float>(sy) - grid_v) / cell;
			// A lamp's distances are world distances: from its own tile, which every pixel reads at its foot.
			// So are an ELEVATED spill's: its lattice hangs at the ceiling while it is drawn at its floor, and the
			// screen centre converted to cells sat ~2.6 cells down-screen of its own tile (an inward pool's glow
			// slid toward the viewer).
			if (!is_spill || anchor_z >= 5) {
				uc = static_cast<float>(grid_rt);
				vc = static_cast<float>(grid_rt);
			}
			if (from_source) {
				uc = static_cast<float>(grid_rt + Splat_source_dx);
				vc = static_cast<float>(grid_rt + Splat_source_dy);
			}
			int cgx         = static_cast<int>(std::lround(uc));
			int cgy         = static_cast<int>(std::lround(vc));
			cgx             = std::min(std::max(cgx, 0), side - 1);
			cgy             = std::min(std::max(cgy, 0), side - 1);
			const int mbase = Grid::Dist(grid[static_cast<size_t>(cgy) * side + cgx]);
			base_px         = mbase > 0 ? static_cast<float>((mbase - 1) * c_tilesize) : 0.0f;
			const int cheb  = std::max(std::abs(cgx - grid_rt), std::abs(cgy - grid_rt));
			if (const float cap_px = static_cast<float>(cheb * c_tilesize); base_px > cap_px) {
				base_px = cap_px;
			}
			// The fill only says WHERE the deck spill reaches; its path from the landing tile is no distance.
			// Likewise an outdoor light's: an outside viewer gets its free dome (straight distance), and the
			// viewer inside must see the same brightness wherever the fill reaches.
			// And an inside light's: objects cast no shadow, so the travelled detour around a pillar or a tall
			// piece of furniture must not darken a strip behind it.
			path_rebase = base_px;
			if (from_source || !is_spill) {
				base_px = 1.0e9f;
			}
		}
		// ...but only where it sees the cell: one it reaches only around a WALL (the straight line crosses a wall
		// run) fades over the way round, or the far side of a house glows as if the walls were glass, and the
		// hidden leg of an L-shaped room lights as if the corner were not there.  A pillar or any other lone
		// blocking piece is no wall run: it casts no shadow.
		std::vector<unsigned char> around_wall;
		if (grid != nullptr && ring != nullptr && !from_source && !is_spill) {
			// A square block on the light's floor (a pillar, 2x2 or 3x3) is a lone piece even where it stands against
			// the wall: joined to it, the wall cell and its two columns made a "run" that shaded the wall behind it.
			std::vector<unsigned char> lone(static_cast<size_t>(side) * side, 0);
			if (sprites != nullptr) {
				const int   fsh = 4 * std::max({anchor_z, spill_deck_z, 0});
				const float hf  = 0.5f * static_cast<float>(c_tilesize);
				for (int i = 0; i < nsprites; ++i) {
					const Sprite_box& sb = sprites[i];
					if (sb.xts < 2 || sb.yts < 2 || sb.xts > 3 || sb.yts > 3 || sb.fz / 4 >= 5 * (light_floor_storey + 1)
						|| !Fast_info(sb.shape).is_solid()) {
						continue;
					}
					const int eu = static_cast<int>(
							std::ceil((static_cast<float>(sb.fx + sb.fz - fsh) - hf - grid_u) * inv_cell - 1e-3f));
					const int ev = static_cast<int>(
							std::ceil((static_cast<float>(sb.fy + sb.fz - fsh) - hf - grid_v) * inv_cell - 1e-3f));
					for (int v = ev - sb.yts + 1; v <= ev; ++v) {
						for (int u = eu - sb.xts + 1; u <= eu; ++u) {
							if (u >= 0 && v >= 0 && u < side && v < side) {
								lone[static_cast<size_t>(v) * side + u] = 1;
							}
						}
					}
				}
			}
			auto is_w = [&](int u, int v) {
				return u >= 0 && v >= 0 && u < side && v < side && Ring::Is_wall(ring, static_cast<size_t>(v) * side + u)
					   && lone[static_cast<size_t>(v) * side + u] == 0;
			};
			// Part of a run of at least three wall cells along either axis.
			auto wall_run = [&](int u, int v) {
				return is_w(u, v)
					   && ((is_w(u - 1, v) && is_w(u + 1, v)) || (is_w(u - 2, v) && is_w(u - 1, v))
						   || (is_w(u + 1, v) && is_w(u + 2, v)) || (is_w(u, v - 1) && is_w(u, v + 1))
						   || (is_w(u, v - 2) && is_w(u, v - 1)) || (is_w(u, v + 1) && is_w(u, v + 2)));
			};
			around_wall.assign(static_cast<size_t>(side) * side, 0);
			const int ax = grid_rt;
			const int ay = grid_rt;
			for (int gy = 0; gy < side; ++gy) {
				for (int gx = 0; gx < side; ++gx) {
					const size_t gidx = static_cast<size_t>(gy) * side + gx;
					if (Grid::Dist(grid[gidx]) == 0) {
						continue;
					}
					if (!Line_clear(ax, ay, gx, gy, wall_run)) {
						around_wall[gidx] = 1;
					}
				}
			}
		}
		// Straight-line distance (px) a cell's dome is measured over: from the splat centre, or for a spill with a
		// wide mouth from the mouth SEGMENT -- one fan lights a long opening end to end instead of fading from its middle.
		const bool mouth_dome  = is_spill && grid != nullptr && (cone_dx != 0 || cone_dy != 0) && cone_mouth != 0;
		auto       travel_from = [&](float gxf, float gyf) -> float {
            const float relx = gxf - uc;
            const float rely = gyf - vc;
            if (mouth_dome) {
                const float along = relx * static_cast<float>(cone_dx) + rely * static_cast<float>(cone_dy);
                const float perp  = Cone_perp_f(relx, rely, cone_dx, cone_dy, cone_mouth);
                return std::sqrt(along * along + perp * perp) * cell;
            }
            return std::sqrt(relx * relx + rely * rely) * cell;
		};
		// The distance a cell's dome is measured over.  Straight for a light that measures straight (the fill
		// only gates reach) -- unless the light got there only the long way round: the straight line crosses a
		// wall run, or the travelled path exceeds it by more than a few tiles (round a corner through an open
		// door, which would light a face behind the wall as if the wall were glass).  A pillar's small detour
		// stays straight.
		auto travel_for = [&](int gx, int gy, int dist_plus1) -> float {
			const float straight = travel_from(static_cast<float>(gx), static_cast<float>(gy));
			if (dist_plus1 <= 0) {
				return straight;
			}
			const float raw = static_cast<float>((dist_plus1 - 1) * c_tilesize);
			// A spill falling off from its source (a well's bubble): the light travelled to the hole (the spill's
			// own distance) and on from it along the bubble's fill.  Round a corner from there -- back over the
			// passage the lamp stands in -- the straight line from the lamp hugely understates it.
			if (from_source) {
				const float total = bias0 + raw;
				return total > straight + 3.0f * cell ? total : straight;
			}
			if (base_px < 1.0e8f) {
				const float path = std::max(raw - base_px, 0.0f);
				return std::max(path, straight);
			}
			const float path   = std::max(raw - (veto_roof ? path_rebase : 0.0f), 0.0f);
			const bool  around = (!around_wall.empty() && around_wall[static_cast<size_t>(gy) * side + gx] != 0)
								|| path > straight + 3.0f * cell;
			return around ? std::max(path, straight) : straight;
		};
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
		const int foot_shift = 4 * std::max({anchor_z, spill_deck_z, 0});
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
		// sampled z-blind by CLEAR pixels -- interior-classified wall faces among
		// them (the barn's west wall seen from inside).  An N/W arrival there
		// counts only when the viewer stands on that side, else the lamp behind
		// the wall bleeds up its interior face.  S/E stay unconditional: the
		// camera faces them, and gating all four would make the accepted side
		// flip mid-wall as the avatar walks.
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
			int d      = 0;
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
		// A wall TOP is one continuous horizontal surface, so a cell the fill reached only the long way
		// round must not sit black beside one the light enters directly (a lamp seen through a doorway).
		// Chamfer the arrivals ALONG connected wall cells, a tile per step:
		// relaxing can only ever SHORTEN a distance, so the ramp ends by
		// itself where it meets the cell's own arrival -- no reach constant,
		// and a far side stays dark because its own value already wins.
		// FACES get the same treatment on their viewer-gated arrivals: each wall sprite paints its own
		// tile's value flat across its whole art, so a real gradient would render as a hard seam where
		// two sprites meet (the 45-degree shear line down an inside wall face beside an open door).
		// There a gated 0 means "dark side", which must stay dark -- the chamfer only smooths between
		// cells that are ALREADY lit.
		// Escaped/Inside flags mean anything only for a fill that started roofed (else esc is all ones).
		bool ring_inside = false;
		if (veto_roof && ring != nullptr) {
			for (size_t i = 0, n = static_cast<size_t>(side) * side; i < n && !ring_inside; ++i) {
				ring_inside = Ring::Is_wall(ring, i) && Ring::Has(ring, i, Ring::Inside_flag);
			}
		}
		// Escaped-only arrivals belong to the spills under an interior light (cf. the field strip).
		auto escaped_only = [&](int ci) -> bool {
			return veto_roof && ring_inside && Ring::Is_foreign_wall(ring, static_cast<size_t>(ci));
		};
		constexpr unsigned char    wall_unreached = 255;
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
					const int d = escaped_only(v * side + u) ? 0 : gated ? ring_face_dist_gated(u, v) : ring_face_dist(u, v);
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
		// looks onto, so it takes that neighbouring cell's arrival -- the wall cell's
		// own side byte records when the fill crept around to touch it, which can run
		// tiles longer than the floor beside it.
		// Unreached room = dark, with no fallback to the cell's own side byte: that
		// byte is side-blind and carries the arrival from OUTSIDE, which would light
		// every interior face of a sealed room.
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
			// final -- treating it as "no answer" would let a dark room's face
			// light itself from the far side.
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
						// A thick wall is crossed; a single wall line seen end-on (open floor on BOTH sides of the
						// cell, across the step) is walked ALONG (a corner post's face reads the window down its
						// own wall line).  A wall's end (open on one side only) is still crossed.
						auto open_at = [&](int u_, int v_) {
							return u_ >= 0 && v_ >= 0 && u_ < side && v_ < side
								   && !Ring::Is_wall(ring, static_cast<size_t>(v_) * side + u_);
						};
						if (sd == 0 ? (open_at(nu - 1, nv) && open_at(nu + 1, nv)) : (open_at(nu, nv - 1) && open_at(nu, nv + 1))) {
							return -1;
						}
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
		auto side_face_dist = [&](int cu, int cv, int sidx, bool* found_room = nullptr) -> int {
			bool      found = false;
			const int d     = ring_side_dist(cu, cv, sidx, &found);
			if (found_room != nullptr) {
				*found_room = found;
			}
			if (d > 0) {
				return d;
			}
			if (found) {
				// The room IS there and it is dark: final.  Borrowing here would
				// paint this face with a lit room from the next sprite along the
				// run.  A wall cell always carries a ring arrival, so the grid
				// guard below never catches that case.
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
			if (from_source) {
				ghash = (ghash ^ static_cast<uint64_t>((Splat_source_dx + 512) * 1024 + Splat_source_dy + 512)) * 1099511628211ULL;
			}
			if (pool_shaped) {
				ghash = (ghash ^ 0x9e3779b97f4a7c15ULL) * 1099511628211ULL;
				ghash = (ghash ^ static_cast<uint64_t>((Splat_pool_wdx + 512) * 1024 + Splat_pool_wdy + 512)) * 1099511628211ULL;
				ghash = (ghash ^ static_cast<uint64_t>((Splat_pool_nx + 2) * 8 + Splat_pool_ny + 2)) * 1099511628211ULL;
			}
			const Field_key fkey{ghash, radius, elevation, dist_bias, intensity_pct, grid_rt, sx - grid_fx, sy - grid_fy};
			const Uint64    fnow = SDL_GetTicks();
			// The key carries position (sx - grid_fx), so a stale hit would draw a pool at an old alignment.
			ftmpl = field_cache.find(fkey, fnow);
			if (ftmpl == nullptr) {
				field_local.assign(static_cast<size_t>(side) * side, 0.0f);
				// A wall cell is lit differently on each of its four sides, and
				// one scalar plus a direction cannot say that: taking any lit
				// side would export the street's brightness into the room
				// behind, taking only the nearest side would block the other
				// three and punch a dark V into lit ground around the wall.
				// Keep the dome PER SIDE and let the pixel's position choose.
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
					const float travel = travel_for(gx, gy, m);
					const float tot    = travel + field_bias;
					const float dome   = dome_at(tot);
					return dome > 0.0f ? 255.0f * dome * inten * cone_fade(gx, gy) * pool_fade(gx, gy) : 0.0f;
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
							// by side answers directly.  Gated here it would
							// force N and W to 0 for an eastern viewer, and
							// ground west of any wall would blend toward nothing.
							const unsigned char* rc = ring + gidx * Ring::Bytes;
							for (int s = Ring::S; s <= Ring::W; ++s) {
								field_side[gidx * Ring::Bytes + s] = dome_for(gx, gy, Ring::Dist(rc[s]));
							}
							field_wall[gidx] = 1;
						}
						field_local[gidx] = dome_for(gx, gy, m);
					}
				}
				// The lattice holds a cell's value at its foot (+x/+y) edge, so the
				// fan's +perp flank fades within its own penumbra tiles while the
				// -x/-y flank would fade half a tile into the lit mouth beside the
				// jamb.  Mirror it -- a -x/-y flank cell takes its mouth-ward
				// neighbour's value -- so both flanks fade within their own tiles.
				if (coned) {
					const bool ns = cone_dy != 0;
					for (int a = 0; a < side; ++a) {
						for (int b = 0; b + 1 < side; ++b) {    // Ascending: reads the unshifted neighbour.
							const int gx    = ns ? b : a;
							const int gy    = ns ? a : b;
							const int relx  = gx - grid_rt;
							const int rely  = gy - grid_rt;
							const int along = relx * cone_dx + rely * cone_dy;
							if (along < 0 || (ns ? relx : rely) >= 0) {
								continue;
							}
							const int perp = Cone_perp(relx, rely, cone_dx, cone_dy, cone_mouth);
							if (perp < 1) {
								continue;
							}
							const size_t gi = static_cast<size_t>(gy) * side + gx;
							const size_t ni = ns ? gi + 1 : gi + side;
							if (((grid[gi] | grid[ni]) & Grid::Flag) != 0
								|| (!field_wall.empty() && (field_wall[gi] != 0 || field_wall[ni] != 0))) {
								continue;
							}
							// A blocked cell inside the fan stays dark.
							if (!Cone_outside(along, perp) && Grid::Dist(grid[gi]) == 0) {
								continue;
							}
							field_local[gi] = field_local[ni];
						}
					}
				}
				// Rasterize the field into the byte template over the splat's full
				// (unclipped) extent -- the same extent as the bbox below, relative
				// to the splat centre -- with bilinear sampling.
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
					const float          tv    = v - static_cast<float>(h0);
					const float*         row0  = field_local.data() + static_cast<size_t>(h0) * side;
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
						const float  tu  = u - static_cast<float>(g0);
						const float* r   = row0 + g0;
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
		// ELEVATED spills only (walkway/deck bubbles): the source sits `bias` px
		// behind the opening, so their wall glow falls off Euclidean (sqrt(d^2 +
		// bias^2)) with the lateral reach capped at 1.5x radius and the dome
		// normalized to hit 0 exactly there.  All other lights keep the linear
		// continuation (d + bias) with reach = radius.
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
			// A wide mouth's dome is measured from the mouth segment: its ends light past the centre's reach.
			if (is_spill && (cone_dx != 0 || cone_dy != 0) && cone_mouth != 0) {
				const int mext = c_tilesize * std::max(cone_mouth & 0xff, (cone_mouth >> 8) & 0xff);
				x0 -= mext;
				x1 += mext;
				y0 -= mext;
				y1 += mext;
			}
			if (!is_spill && face_data) {
				// Ground below the lattice level, read at its foot.
				x1 = std::max(x1, grid_fx + foot_shift + radius + c_tilesize);
				y1 = std::max(y1, grid_fy + foot_shift + radius + c_tilesize);
			}
		}
		// A tree crown is drawn far above its lit foot cell, yet widening this
		// bbox up-left by a flat band would also sweep in unrelated sprites and
		// light them (art copied across the screen as the view scrolls) -- and
		// the per-pixel foot channel cannot reject them, because the shear
		// clamps it.  So the reach is taken from the SPRITES themselves: only
		// boxes whose paint origin falls inside the pool are added, and only
		// their own pixels may light from the added area.
		int pool_x0 = x0;
		int pool_y0 = y0;
		// Unclamped pool, for the "rooted in this pool" test: a tree whose foot lies below the screen edge
		// still extends the box, and must still own its art there.
		const int root_x0 = x0;
		const int root_y0 = y0;
		const int root_x1 = x1;
		const int root_y1 = y1;
		Reach_extend_box(x0, y0, x1, y1, face_data ? sprites : nullptr, nsprites);
		if (wrote != nullptr) {
			*wrote = TileRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
		}
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
		// The FREE dome (no fill) at a squared screen distance: continued past the spill's bias, or for an elevated
		// spill measured euclidean from the bubble (dome_dn).
		auto free_dome = [&](float dist2) -> float {
			if (euclid_spill) {
				return 1.0f - (dist2 + bias * bias + e2) / dome_dn;
			}
			return dome_at(bias > 0.0f ? std::sqrt(dist2) + bias : std::sqrt(dist2));
		};
		// Cells ARE tiles and the lattice point is a tile's FOOT (grid_fx/fy is
		// the light tile's foot), so a tile spans (point-8, point]: the cell
		// CONTAINING a position is ceil, not nearest.  With the foot unsnapped
		// a tile's range straddles lround's boundary, so nearest would flip the
		// cell MID-TILE: through a binary gate like object_reached, a hard edge
		// across a sprite.
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
			const int   fx = foot_x(px, py) - foot_shift;
			const int   fy = foot_y(px, py) - foot_shift;
			const float pu = (static_cast<float>(fx) - grid_u) * inv_cell;
			const float pv = (static_cast<float>(fy) - grid_v) * inv_cell;
			int         cu = pos_cell(pu);
			int         cv = pos_cell(pv);
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
			const float travel = travel_for(cidx % side, cidx / side, dist_plus1);
			const float tot    = travel + field_bias;
			const float dome   = dome_at(tot);
			return dome > 0.0f
						   ? static_cast<int>(
									 amp * dome * cone_fade(cidx % side, cidx / side) * pool_fade(cidx % side, cidx / side) + 0.5f)
						   : 0;
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
		// object_alpha, object_reached and Light_tile_alpha reject it the same way.
		auto apron_cell = [&](int ci) {
			return ring != nullptr && Ring::Is_apron(ring, static_cast<size_t>(ci));
		};
		// Blend weight the last face_alpha_from call put on unlit (0) cells.
		float face_zero_w = 0.0f;
		// The last face_alpha_from call's own-cell value, before blending.
		int face_base = 0;
		// The cell the last face_alpha_from call read its face at.
		int  face_cell_u     = -1;
		int  face_cell_v     = -1;
		auto face_alpha_from = [&](float fx, float fy, int cu, int cv, bool all_sides, bool top, int iside) -> int {
			auto face_dist = [&](int u_, int v_) -> int {
				int  d;
				bool front_found = false;
				if (iside >= 0) {
					d = side_face_dist(u_, v_, iside, &front_found);
				} else if (top) {
					d = top_face_dist(u_, v_);
				} else if (all_sides) {
					d = escaped_only(v_ * side + u_) ? 0 : ring_face_dist(u_, v_);
				} else {
					d = gated_face_dist(u_, v_);
				}
				const size_t ci = static_cast<size_t>(v_) * side + u_;
				// A side face whose front space was found dark is dark, even over a reached cell (a wall
				// on a slab above the fill).
				if (d == 0 && !top && !front_found && !Ring::Is_wall(ring, ci) && !apron_cell(static_cast<int>(ci))) {
					// Nothing ever wrote a non-wall cell's side bytes, so a 0
					// from the ring here is NO ANSWER, not a dark one: take the
					// cell's own fill value, like the ground beside it.  A
					// halfwall under a window is such a cell -- the fill runs
					// straight through it -- and must not render black against
					// its own sprite's lit half where a pane clears the kind
					// byte onto the field path.
					// Wall TOPS excluded: they have their own chamfer for
					// exactly this ("reached only the long way round").
					d = Grid::Dist(grid[ci]);
				}
				return d;
			};
			// Blend the per-tile value with the neighbouring wall cells by the
			// pixel's sub-tile offset from its snapped foot (the smooth
			// gradient the bilinear field shows from inside); non-wall
			// neighbours reuse the base so the face only blends along itself.
			// An indoor light's side face is lit like the floor it looks onto: measure the distance at that open
			// cell.  At the wall's own cell the straight line always grazes its own run, so it would fall back
			// to the walked distance and a niche's back wall would read far darker than the floor at its foot.
			const int dome_ci = [&] {
				if (!veto_roof || iside < 0 || ring == nullptr) {
					return cv * side + cu;
				}
				int nu = cu, nv = cv;
				for (int st = 0; st < 4; ++st) {
					nu += iside == 1 ? 1 : 0;
					nv += iside == 0 ? 1 : 0;
					if (nu >= side || nv >= side) {
						break;
					}
					const size_t ni = static_cast<size_t>(nv) * side + nu;
					if (!Ring::Is_wall(ring, ni)) {
						return Grid::Dist(grid[ni]) != 0 && !Ring::Is_apron(ring, ni) ? static_cast<int>(ni) : cv * side + cu;
					}
				}
				return cv * side + cu;
			}();
			const int base = cell_dome(dome_ci, face_dist(cu, cv));
			// Along a side face, unreached cells are the same wall mass (a thick wall's far column has no Wall_flag).
			auto cell_a = [&](int u_, int v_, bool along) -> int {
				if (u_ < 0 || v_ < 0 || u_ >= side || v_ >= side) {
					return base;
				}
				const size_t ni = static_cast<size_t>(v_) * side + u_;
				if (!Ring::Is_wall(ring, ni) && !(along && Grid::Dist(grid[ni]) == 0 && !apron_cell(static_cast<int>(ni)))) {
					return base;
				}
				return cell_dome(v_ * side + u_, face_dist(u_, v_));
			};
			// Past half a cell the pixel is art LIFTED off its foot (a wall
			// face, and a wall top by the whole wall height), not a sub-tile
			// position: clamping there would hand the neighbour full weight and
			// leave the owner cell none at all -- the top of a north-south wall
			// would then read the dark cell behind it instead of its own lit
			// arrival.  Out of range = no blend on that axis.
			// The foot is the pixel's GROUND position, unsnapped, so its offset
			// from its own cell centre is the sub-tile position the blend needs
			// -- in [-0.5, 0.5] by construction, since cu/cv are that same
			// quantity rounded. (px - fx) is NOT it: with the foot unsnapped
			// that difference is just the sprite's elevation, constant across
			// the art, so every tile would render flat.
			const float ox  = pos_off((fx - grid_u) * inv_cell, cu);
			const float oy  = pos_off((fy - grid_v) * inv_cell, cv);
			const int   nu  = ox >= 0.0f ? cu + 1 : cu - 1;
			const int   nv  = oy >= 0.0f ? cv + 1 : cv - 1;
			const float axo = ox >= 0.0f ? ox : -ox;
			const float ayo = oy >= 0.0f ? oy : -oy;
			const float wx  = axo > 0.5f ? 0.5f : axo;
			const float wy  = ayo > 0.5f ? 0.5f : ayo;
			const float av  = static_cast<float>(base) * (1.0f - wx - wy) + static_cast<float>(cell_a(nu, cv, iside == 0)) * wx
							 + static_cast<float>(cell_a(cu, nv, iside == 1)) * wy;
			// A side face looking onto an apron cell is answered dark: the light there escaped to a spill.
			auto apron_front = [&](int u_, int v_) {
				if (iside < 0) {
					return false;
				}
				for (int st = 0; st < 4; ++st) {
					u_ += iside == 1 ? 1 : 0;
					v_ += iside == 0 ? 1 : 0;
					if (u_ >= side || v_ >= side) {
						return false;
					}
					const int ni = v_ * side + u_;
					if (!Ring::Is_wall(ring, static_cast<size_t>(ni))) {
						return apron_cell(ni);
					}
				}
				return false;
			};
			// An arrival past the reach is a dark answer; only a cell nothing arrived at is no answer.
			auto no_answer = [&](int u_, int v_, bool along) {
				if (u_ < 0 || v_ < 0 || u_ >= side || v_ >= side) {
					return base == 0 && face_dist(cu, cv) == 0 && !apron_front(cu, cv);
				}
				const size_t ni = static_cast<size_t>(v_) * side + u_;
				if (!Ring::Is_wall(ring, ni) && !(along && Grid::Dist(grid[ni]) == 0 && !apron_cell(static_cast<int>(ni)))) {
					return base == 0 && face_dist(cu, cv) == 0 && !apron_front(cu, cv);
				}
				return cell_a(u_, v_, along) == 0 && face_dist(u_, v_) == 0 && !apron_front(u_, v_);
			};
			face_zero_w = (no_answer(cu, cv, false) ? 1.0f - wx - wy : 0.0f) + (no_answer(nu, cv, iside == 0) ? wx : 0.0f)
						  + (no_answer(cu, nv, iside == 1) ? wy : 0.0f);
			face_base   = base;
			face_cell_u = cu;
			face_cell_v = cv;
			return static_cast<int>(av + 0.5f);
		};
		// The unreached non-wall TILE a side face's or top's foot stands on (inside its own mass), or -1.
		// The lattice sits ~3px up-left of the tiles (as in object_alpha), so a band foot's tile is the cell before.
		auto own_mass_cell = [&](float pu, float pv) -> int {
			const int cu = pos_cell(pu);
			const int cv = pos_cell(pv);
			const int tu = cu - (pos_off(pu, cu) < -0.15f ? 1 : 0);
			const int tv = cv - (pos_off(pv, cv) < -0.15f ? 1 : 0);
			if (grid == nullptr || tu < 0 || tv < 0 || tu >= side || tv >= side || wall_cell(tu, tv)
				|| Grid::Dist(grid[static_cast<size_t>(tv) * side + tu]) != 0 || apron_cell(tv * side + tu)) {
				return -1;
			}
			return tv * side + tu;
		};
		auto face_alpha = [&](int px, int py, bool all_sides, bool top = false, int iside = -1) -> int {
			const int   fx = foot_x(px, py) - foot_shift;
			const int   fy = foot_y(px, py) - foot_shift;
			const float pu = (static_cast<float>(fx) - grid_u) * inv_cell;
			const float pv = (static_cast<float>(fy) - grid_v) * inv_cell;
			int         cu = pos_cell(pu);
			int         cv = pos_cell(pv);
			// A wall SURFACE belongs to a WALL cell: where the lattice still
			// cannot resolve which side of a boundary the foot is on, step back
			// onto the wall, toward whichever side the foot leans.
			const int  mci      = (iside >= 0 || top) ? own_mass_cell(pu, pv) : -1;
			const bool own_mass = mci >= 0;
			if (own_mass) {
				cu = mci % side;
				cv = mci / side;
			} else if (
					ring != nullptr && grid != nullptr && cu >= 0 && cv >= 0 && cu < side && cv < side
					&& apron_cell(cv * side + cu)) {
				// A band foot rounded onto an escaped cell belongs to the tile before it: the sill over an
				// open doorway reads its own doorway tile, not the street outside (apron = 0).
				const int tu = cu - (pos_off(pu, cu) < -0.15f ? 1 : 0);
				const int tv = cv - (pos_off(pv, cv) < -0.15f ? 1 : 0);
				if ((tu != cu || tv != cv) && tu >= 0 && tv >= 0 && !apron_cell(tv * side + tu)
					&& (wall_cell(tu, tv) || Grid::Dist(grid[static_cast<size_t>(tv) * side + tu]) != 0)) {
					cu = tu;
					cv = tv;
				}
			}
			if (ring != nullptr && cu >= 0 && cv >= 0 && cu < side && cv < side && !wall_cell(cu, cv) && !own_mass) {
				const int du = pos_off(pu, cu) >= 0.0f ? 1 : -1;
				const int dv = pos_off(pv, cv) >= 0.0f ? 1 : -1;
				// A side face steps only across its own plane: along it, the
				// next cell is a DIFFERENT unit (a halfwall's face must not
				// read the wall beside it, whose side the fill may never
				// reach).
				if (iside != 1 && cv + dv >= 0 && cv + dv < side && wall_cell(cu, cv + dv)) {
					cv += dv;
				} else if (iside != 0 && cu + du >= 0 && cu + du < side && wall_cell(cu + du, cv)) {
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
				// Nothing standing there is really lit.
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
				// sealed building would light the wall course on the INSIDE.
				// Take the viewer-side faces; a wall reached only from the far
				// side is dark, and the walk below must not step out through it
				// either.
				int fd = ring_face_dist_gated(cu, cv);
				// A DOORWAY fan (open floor behind the apex, not a window's
				// wall) is light leaving through the mouth plane: a block face
				// behind that plane (the corridor's own walls, read through the
				// block's S-face art) or turned away from it (the S faces
				// flanking the mouth, facing the way the light goes) cannot
				// see it.  The room's own light owns the corridor.
				if (fd != 0 && is_spill && (cone_dx != 0 || cone_dy != 0)) {
					const int    bu = grid_rt - cone_dx;
					const int    bv = grid_rt - cone_dy;
					const size_t bi = static_cast<size_t>(bv) * side + bu;
					if (bu >= 0 && bv >= 0 && bu < side && bv < side && !Ring::Is_wall(ring, bi) && Grid::Dist(grid[bi]) != 0) {
						const int along = (cu - grid_rt) * cone_dx + (cv - grid_rt) * cone_dy;
						if (along < 0) {
							fd = 0;
						} else {
							const unsigned char* rc = ring + static_cast<size_t>(cidx) * Ring::Bytes;
							// Faces whose outward normal n has n . cone <= 0.
							int v[4];
							v[Ring::S] = cone_dy > 0 ? 0 : Ring::Dist(rc[Ring::S]);
							v[Ring::N] = cone_dy < 0 ? 0 : (av_cv < cv ? Ring::Dist(rc[Ring::N]) : 0);
							v[Ring::E] = cone_dx > 0 ? 0 : Ring::Dist(rc[Ring::E]);
							v[Ring::W] = cone_dx < 0 ? 0 : (av_cu < cu ? Ring::Dist(rc[Ring::W]) : 0);
							fd         = 0;
							for (const int s : v) {
								if (s != 0 && (fd == 0 || s < fd)) {
									fd = s;
								}
							}
						}
					}
				}
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
				if (Cone_outside(along, perp)) {
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
		// two along the lattice diagonal where the two cells' arrivals differ.
		// Blend across it as faces do (face_alpha_from), by the foot's offset
		// from its own cell centre.  Pinning every pixel to the anchor cell
		// instead (unit_obj) also hides it, but flattens a whole half wall to
		// its base tile's value.
		// An UNREACHED neighbour keeps the base: 0 there means "outside the
		// fill", and letting it average in would bleed darkness back into
		// everything standing at the pool's edge.
		auto object_alpha = [&](int px, int py, int max_walk) -> int {
			int cidx = foot_cell(px, py);
			if (cidx < 0) {
				return 0;
			}
			// The lattice sits ~3px up-left of the tiles, so a wall block's
			// last 3px rows/columns land in the floor cell beyond it.
			if (ring != nullptr && !Ring::Is_wall(ring, static_cast<size_t>(cidx))) {
				const float bpu = (static_cast<float>(foot_x(px, py) - foot_shift) - grid_u) * inv_cell;
				const float bpv = (static_cast<float>(foot_y(px, py) - foot_shift) - grid_v) * inv_cell;
				// Band pixels sit at offsets <= -0.19 (x) / -0.25 (y); a tile's own first pixel at >= -0.125.
				const int cu          = cidx % side;
				const int cv          = cidx / side;
				const int bu          = cu - (pos_off(bpu, cu) < -0.15f ? 1 : 0);
				const int bv          = cv - (pos_off(bpv, cv) < -0.15f ? 1 : 0);
				const int tries[3][2] = {
						{bu, bv},
                        {cu, bv},
                        {bu, cv}
                };
				for (const auto& t : tries) {
					const int ti = t[1] * side + t[0];
					if (t[0] >= 0 && t[1] >= 0 && ti != cidx && Ring::Is_wall(ring, static_cast<size_t>(ti))) {
						cidx = ti;
						break;
					}
				}
			}
			const int base = object_cell_alpha(cidx, max_walk);
			if (base == 0 || side <= 0) {
				return base;
			}
			const int   fx = foot_x(px, py) - foot_shift;
			const int   fy = foot_y(px, py) - foot_shift;
			const int   cu = cidx % side;
			const int   cv = cidx / side;
			const float ox = pos_off((static_cast<float>(fx) - grid_u) * inv_cell, cu);
			const float oy = pos_off((static_cast<float>(fy) - grid_v) * inv_cell, cv);
			const float ax = ox >= 0.0f ? ox : -ox;
			const float ay = oy >= 0.0f ? oy : -oy;
			const float wx = ax > 0.5f ? 0.5f : ax;
			const float wy = ay > 0.5f ? 0.5f : ay;
			auto        nb = [&](int u, int v) -> int {
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
		// Softens a unit's per-cell reach edge along one tile axis (0 = u, 1 = v) over about two tiles.
		auto axis_feather = [&](int px, int py, int axis, int max_walk, int val) -> int {
			const int fc = foot_cell(px, py);
			if (fc < 0) {
				return val;
			}
			const int cu  = fc % side;
			const int cv  = fc / side;
			const int own = object_cell_alpha(fc, max_walk);
			auto      at  = [&](int k) -> float {
                const int u = axis == 0 ? k : cu;
                const int v = axis == 1 ? k : cv;
                if (u < 0 || v < 0 || u >= side || v >= side) {
                    return static_cast<float>(own);
                }
                return static_cast<float>(object_cell_alpha(v * side + u, max_walk));
			};
			// Cell k's centre sits at k - 0.5.
			auto lin = [&](float q) -> float {
				const float c  = q + 0.5f;
				const int   k0 = static_cast<int>(std::floor(c));
				const float t  = c - static_cast<float>(k0);
				return at(k0) * (1.0f - t) + at(k0 + 1) * t;
			};
			const int   f = axis == 1 ? foot_y(px, py) - foot_shift : foot_x(px, py) - foot_shift;
			const float p = (static_cast<float>(f) - (axis == 1 ? grid_v : grid_u)) * inv_cell;
			const float b = (lin(p - 0.5f) + 2.0f * lin(p) + lin(p + 0.5f)) * 0.25f;
			if (own <= 0) {
				return std::max(val, static_cast<int>(b + 0.5f));
			}
			if (b < static_cast<float>(own)) {
				return static_cast<int>(static_cast<float>(val) * b / static_cast<float>(own) + 0.5f);
			}
			return val;
		};
		// Sprite indices (ascending) whose art box meets each 16px block of the splat box.
		constexpr int    bkt_sh = 4;
		const int        bkt_w  = x1 >= x0 ? ((x1 - x0) >> bkt_sh) + 1 : 0;
		const int        bkt_h  = y1 >= y0 ? ((y1 - y0) >> bkt_sh) + 1 : 0;
		std::vector<int> bkt_start;
		std::vector<int> bkt_ids;
		std::vector<int> bkt_all;
		auto             sprites_at = [&](int px, int py) -> std::pair<const int*, const int*> {
            if (px < x0 || px > x1 || py < y0 || py > y1) {
                if (bkt_all.size() != static_cast<size_t>(nsprites)) {
                    bkt_all.resize(static_cast<size_t>(nsprites));
                    for (int i = 0; i < nsprites; ++i) {
                        bkt_all[static_cast<size_t>(i)] = i;
                    }
                }
                return {bkt_all.data(), bkt_all.data() + bkt_all.size()};
            }
            if (bkt_start.empty()) {
                const size_t nb = static_cast<size_t>(bkt_w) * static_cast<size_t>(bkt_h);
                bkt_start.assign(nb + 1, 0);
                auto span = [&](const Sprite_box& s, int& bx0, int& by0, int& bx1, int& by1) {
                    if (s.x1 < x0 || s.x0 > x1 || s.y1 < y0 || s.y0 > y1) {
                        return false;
                    }
                    bx0 = (std::max<int>(s.x0, x0) - x0) >> bkt_sh;
                    by0 = (std::max<int>(s.y0, y0) - y0) >> bkt_sh;
                    bx1 = (std::min<int>(s.x1, x1) - x0) >> bkt_sh;
                    by1 = (std::min<int>(s.y1, y1) - y0) >> bkt_sh;
                    return true;
                };
                int bx0, by0, bx1, by1;
                for (int i = 0; i < nsprites; ++i) {
                    if (span(sprites[i], bx0, by0, bx1, by1)) {
                        for (int by = by0; by <= by1; ++by) {
                            for (int bx = bx0; bx <= bx1; ++bx) {
                                ++bkt_start[static_cast<size_t>(by) * bkt_w + bx + 1];
                            }
                        }
                    }
                }
                for (size_t b = 0; b < nb; ++b) {
                    bkt_start[b + 1] += bkt_start[b];
                }
                bkt_ids.resize(static_cast<size_t>(bkt_start[nb]));
                std::vector<int> cur(bkt_start.begin(), bkt_start.end() - 1);
                for (int i = 0; i < nsprites; ++i) {
                    if (span(sprites[i], bx0, by0, bx1, by1)) {
                        for (int by = by0; by <= by1; ++by) {
                            for (int bx = bx0; bx <= bx1; ++bx) {
                                bkt_ids[static_cast<size_t>(cur[static_cast<size_t>(by) * bkt_w + bx]++)] = i;
                            }
                        }
                    }
                }
            }
            const size_t b = static_cast<size_t>((py - y0) >> bkt_sh) * bkt_w + ((px - x0) >> bkt_sh);
            return {bkt_ids.data() + bkt_start[b], bkt_ids.data() + bkt_start[b + 1]};
		};
		// A spill lights a free-standing object as ONE unit, like a tree: the brightest floor in or beside
		// its footprint.  A solid's own tiles stay outside the fill, so per-pixel feet split a winch in two.
		// -1 when no stamped sprite owns the pixel.
		const uint32_t frame_gen = top_owner_gen.load(std::memory_order_relaxed);
		{
			std::lock_guard<std::mutex> lk(top_owner_cache_mx);
			if (top_owner_cache.size() != static_cast<size_t>(W) * H) {
				std::vector<std::atomic<uint64_t>> fresh(static_cast<size_t>(W) * H);
				for (auto& e : fresh) {
					e.store(0, std::memory_order_relaxed);
				}
				top_owner_cache.swap(fresh);
			}
			if (unit_best_cache.size() != static_cast<size_t>(W) * H) {
				std::vector<std::atomic<uint64_t>> fresh(static_cast<size_t>(W) * H);
				for (auto& e : fresh) {
					e.store(0, std::memory_order_relaxed);
				}
				unit_best_cache.swap(fresh);
			}
		}
		// The unit a pixel belongs to depends only on the frame (sprites, feet): shared across lights per frame,
		// like top_owner; and a unit's value is one per light, memoized per splat.
		std::map<int, int> unit_value_memo;
		auto               unit_best = [&](int px, int py) -> int {
            const int  pfx  = foot_x(px, py);
            const int  pfy  = foot_y(px, py);
            int        best = -1;
            int        bd   = 1 << 30;
            const auto rng  = sprites_at(px, py);
            for (const int* p = rng.first; p != rng.second; ++p) {
                const int         i = *p;
                const Sprite_box& s = sprites[i];
                if (s.wall || px < s.x0 || px > s.x1 || py < s.y0 || py > s.y1) {
                    continue;
                }
                const int ddx = s.fx + s.fz - pfx;
                const int ddy = s.fy + s.fz - pfy;
                if (ddx < 0 || ddy < 0 || ddx >= s.xts * c_tilesize || ddy >= s.yts * c_tilesize) {
                    continue;
                }
                // The art box is not the art: a wall shelf's box reaches over the table below it.
                if (const Shape_frame* fr = ShapeID(s.shape, s.frame).get_shape();
                    fr != nullptr && !Art_has_point(fr, px - s.fx, py - s.fy)) {
                    continue;
                }
                // A tie goes to the art painted later (on top): a wall shelf over the wall top behind it.
                if (ddx + ddy <= bd) {
                    bd   = ddx + ddy;
                    best = i;
                }
            }
            return best;
		};
		auto unit_alpha = [&](int px, int py) -> int {
			if (sprites == nullptr || footdx == nullptr) {
				return -1;
			}
			int best;
			if (px >= 0 && py >= 0 && px < W && py < H && unit_best_cache.size() == static_cast<size_t>(W) * H) {
				std::atomic<uint64_t>& e = unit_best_cache[static_cast<size_t>(py) * W + px];
				const uint64_t         v = e.load(std::memory_order_relaxed);
				if (static_cast<uint32_t>(v >> 32) == frame_gen) {
					best = static_cast<int>(static_cast<uint32_t>(v)) - 2;
				} else {
					best = unit_best(px, py);
					e.store((static_cast<uint64_t>(frame_gen) << 32) | static_cast<uint32_t>(best + 2), std::memory_order_relaxed);
				}
			} else {
				best = unit_best(px, py);
			}
			if (best < 0) {
				return -1;
			}
			// Architecture (a rampart run) is no single unit: it lights per pixel where the fan reaches it.
			if (sprites[best].building) {
				return -1;
			}
			if (const auto it = unit_value_memo.find(best); it != unit_value_memo.end()) {
				return it->second;
			}
			const auto rng     = sprites_at(px, py);
			auto       unit_of = [&](const Sprite_box& s) {
                const int half_t = c_tilesize / 2;
                const int ecu    = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
                const int ecv    = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
                int       a      = 0;
                for (int v = ecv - s.yts; v <= ecv + 1; ++v) {
                    for (int u = ecu - s.xts; u <= ecu + 1; ++u) {
                        if (u < 0 || v < 0 || u >= side || v >= side) {
                            continue;
                        }
                        const int  ci  = v * side + u;
                        const bool own = u > ecu - s.xts && v > ecv - s.yts && u <= ecu && v <= ecv;
                        // Beside the footprint only open floor counts: a wall cell's arrival may be its far side.
                        if (!own && ring != nullptr && Ring::Is_wall(ring, static_cast<size_t>(ci))) {
                            continue;
                        }
                        const int ca = object_cell_alpha(ci, 0);
                        if (ca > a) {
                            a = ca;
                        }
                    }
                }
                return a;
			};
			int a = unit_of(sprites[best]);
			// An object resting on another unit (armour on a table) is lit at least as that support.
			for (int cur = best, depth = 0; depth < 4; ++depth) {
				const Sprite_box& s   = sprites[cur];
				int               sup = -1;
				for (const int* p = rng.first; p != rng.second; ++p) {
					const Sprite_box& t = sprites[*p];
					if (*p == cur || t.wall || t.building || t.fz >= s.fz
						|| t.fz / 4 + Fast_info(t.shape).get_3d_height() != s.fz / 4) {
						continue;
					}
					const int sx1 = s.fx + s.fz, sy1 = s.fy + s.fz, tx1 = t.fx + t.fz, ty1 = t.fy + t.fz;
					if (sx1 > tx1 - t.xts * c_tilesize && tx1 > sx1 - s.xts * c_tilesize && sy1 > ty1 - t.yts * c_tilesize
						&& ty1 > sy1 - s.yts * c_tilesize) {
						sup = *p;
						break;
					}
				}
				if (sup < 0) {
					break;
				}
				a   = std::max(a, unit_of(sprites[sup]));
				cur = sup;
			}
			unit_value_memo[best] = a;
			return a;
		};
		// The wall box drawn on top at a pixel: the LAST stamped (paint order) whose projected 3D box contains it.  The
		// nearest foot corner could pick a block behind, whose hidden face reads 0.
		// `paint_first`: the LAST painted box holding the pixel within the 1px overhang wins (the art that landed there).
		auto wall_owner
				= [&](int px, int py, bool walls = true, bool on_top = false, int after = -1, bool paint_first = false) -> int {
			if (sprites == nullptr) {
				return -1;
			}
			const auto rng = sprites_at(px, py);
			// Art may overhang its box by a pixel: only when no box holds the pixel exactly.
			for (int tol = paint_first ? 1 : 0; tol <= 1; ++tol) {
				for (const int* p = rng.second; p != rng.first;) {
					const int         i = *--p;
					const Sprite_box& s = sprites[i];
					if (i <= after || s.wall != walls || s.strip > 0 || px < s.x0 || px > s.x1 || py < s.y0 || py > s.y1) {
						continue;
					}
					const int wx   = s.xts * c_tilesize;
					const int wy   = s.yts * c_tilesize;
					const int ddx  = s.fx + s.fz - px;
					const int ddy  = s.fy + s.fz - py;
					const int ztop = std::max(static_cast<int>(s.fz), s.fy + s.fz - wy + 1 - s.y0);
					const int zlo  = std::max({ddx - wx + 1, ddy - wy + 1, static_cast<int>(s.fz)});
					const int zhi  = std::min({ddx, ddy, ztop});
					if (zlo <= zhi + tol && (!on_top || zhi + tol >= ztop)) {
						return i;
					}
				}
			}
			return -1;
		};
		// The non-wall box whose top a top pixel belongs to: its footprint must hold the pixel's stamped foot and its
		// art the pixel.  A tree's tall 1x1 column projects over the desk beside it, and wall_owner would hand the
		// desk top to the tree.
		// The pixel's foot TILE is a floor hole (a stairwell: no slab at the storey floor over it).  The foot's
		// tile, not its rounded cell: a band foot rounds onto the next row, the well's first step.
		auto foot_tile = [&](int px, int py) -> int {
			if (footdx == nullptr || footdy == nullptr) {
				return -1;
			}
			const float pu = (static_cast<float>(foot_x(px, py) - foot_shift) - grid_u) * inv_cell;
			const float pv = (static_cast<float>(foot_y(px, py) - foot_shift) - grid_v) * inv_cell;
			const int   cu = pos_cell(pu);
			const int   cv = pos_cell(pv);
			const int   tu = cu - (pos_off(pu, cu) < -0.15f ? 1 : 0);
			const int   tv = cv - (pos_off(pv, cv) < -0.15f ? 1 : 0);
			return tu >= 0 && tv >= 0 && tu < side && tv < side ? tv * side + tu : -1;
		};
		auto foot_in_well = [&](int px, int py) -> bool {
			const int ti = foot_tile(px, py);
			return ti >= 0 && grid != nullptr && (grid[static_cast<size_t>(ti)] & Grid::Flag) != 0;
		};
		// top_owner is asked several times per pixel and the art test behind it is costly: memoized per pixel for
		// this splat (its answer depends only on the frame's sprites and channels).
		auto top_owner_raw = [&](int px, int py) -> int {
			const int wo = wall_owner(px, py, false, true);
			if (footdx == nullptr) {
				return wo;
			}
			const int pfx   = foot_x(px, py);
			const int pfy   = foot_y(px, py);
			auto      holds = [&](int i) {
                const Sprite_box& s   = sprites[i];
                const int         ddx = s.fx + s.fz - pfx;
                const int         ddy = s.fy + s.fz - pfy;
                if (ddx < 0 || ddy < 0 || ddx >= s.xts * c_tilesize || ddy >= s.yts * c_tilesize) {
                    return false;
                }
                const Shape_frame* fr = ShapeID(s.shape, s.frame).get_shape();
                return fr == nullptr || Art_has_point(fr, px - s.fx, py - s.fy);
			};
			if (wo >= 0 && holds(wo)) {
				return wo;
			}
			const auto rng = sprites_at(px, py);
			for (const int* p = rng.second; p != rng.first;) {
				const int         i = *--p;
				const Sprite_box& s = sprites[i];
				if (s.wall || s.strip > 0 || px < s.x0 || px > s.x1 || py < s.y0 || py > s.y1) {
					continue;
				}
				if (holds(i)) {
					return i;
				}
			}
			return -1;
		};
		auto top_owner = [&](int px, int py) -> int {
			if (px < 0 || py < 0 || px >= W || py >= H || top_owner_cache.size() != static_cast<size_t>(W) * H) {
				return top_owner_raw(px, py);
			}
			std::atomic<uint64_t>& e = top_owner_cache[static_cast<size_t>(py) * W + px];
			const uint64_t         v = e.load(std::memory_order_relaxed);
			if (static_cast<uint32_t>(v >> 32) == frame_gen) {
				return static_cast<int>(static_cast<uint32_t>(v)) - 2;
			}
			const int o = top_owner_raw(px, py);
			e.store((static_cast<uint64_t>(frame_gen) << 32) | static_cast<uint32_t>(o + 2), std::memory_order_relaxed);
			return o;
		};
		// The wall box a face pixel belongs to when its art overhangs the box (a statue stamped as a wall):
		// the box whose footprint holds the pixel's stamped foot and whose art covers it.
		auto wall_foot_owner = [&](int px, int py) -> int {
			if (sprites == nullptr || footdx == nullptr) {
				return -1;
			}
			const int  pfx = foot_x(px, py);
			const int  pfy = foot_y(px, py);
			const auto rng = sprites_at(px, py);
			for (const int* p = rng.second; p != rng.first;) {
				const int         i = *--p;
				const Sprite_box& s = sprites[i];
				if (!s.wall || s.strip > 0 || px < s.x0 || px > s.x1 || py < s.y0 || py > s.y1) {
					continue;
				}
				const int ddx = s.fx + s.fz - pfx;
				const int ddy = s.fy + s.fz - pfy;
				if (ddx < 0 || ddy < 0 || ddx >= s.xts * c_tilesize || ddy >= s.yts * c_tilesize) {
					continue;
				}
				const Shape_frame* fr = ShapeID(s.shape, s.frame).get_shape();
				if (fr == nullptr || Art_has_point(fr, px - s.fx, py - s.fy)) {
					return i;
				}
			}
			return -1;
		};
		// The box whose art landed on a pixel: the last painted one whose art covers it, wall or not.
		// `pane`: only a piece that lets light through (or a door) -- what stamped a pane pixel; the jamb beside a
		// door overhangs its leaf by a pixel without stamping it.
		auto art_owner = [&](int px, int py, bool pane = false) -> int {
			if (sprites == nullptr) {
				return -1;
			}
			const auto rng = sprites_at(px, py);
			for (const int* p = rng.second; p != rng.first;) {
				const int         i = *--p;
				const Sprite_box& s = sprites[i];
				if (s.strip > 0 || px < s.x0 || px > s.x1 || py < s.y0 || py > s.y1) {
					continue;
				}
				if (pane) {
					const Shape_info& si = Fast_info(s.shape);
					if (!si.is_door() && !si.has_light_passes_info()) {
						continue;
					}
				}
				const Shape_frame* fr = ShapeID(s.shape, s.frame).get_shape();
				if (fr == nullptr || Art_has_point(fr, px - s.fx, py - s.fy)) {
					return i;
				}
			}
			return -1;
		};
		// Is the owner's south (side 0) or east (side 1) face pressed against a sprite filling its whole front strip and height?
		auto obj_face_buried = [&](int px, int py, int oi, int fside) -> bool {
			if (oi < 0 || fside < 0) {
				return false;
			}
			const Sprite_box& s    = sprites[oi];
			const int         sx1  = s.fx + s.fz;
			const int         sy1  = s.fy + s.fz;
			const int         sx0  = sx1 - s.xts * c_tilesize;
			const int         sy0  = sy1 - s.yts * c_tilesize;
			const int         stop = s.fz / 4 + Fast_info(s.shape).get_3d_height();
			const auto        rng  = sprites_at(px, py);
			for (const int* p = rng.first; p != rng.second; ++p) {
				const Sprite_box& t = sprites[*p];
				if (*p == oi || t.strip > 0 || t.fz > s.fz || t.fz / 4 + Fast_info(t.shape).get_3d_height() < stop) {
					continue;
				}
				const int  tx1    = t.fx + t.fz;
				const int  ty1    = t.fy + t.fz;
				const int  tx0    = tx1 - t.xts * c_tilesize;
				const int  ty0    = ty1 - t.yts * c_tilesize;
				const bool covers = fside == 0 ? tx0 <= sx0 && tx1 >= sx1 && ty0 <= sy1 && ty1 > sy1
											   : ty0 <= sy0 && ty1 >= sy1 && tx0 <= sx1 && tx1 > sx1;
				if (covers) {
					return true;
				}
			}
			return false;
		};
		// Did the fill reach the wall a top pixel crowns?  Asked of the owner's whole footprint: a per-pixel
		// read would split one top by which cell each foot hits.
		// A top is a horizontal surface, visible from any open cell touching the footprint --
		// diagonals included: a junction post walled in on all four sides carries no arrival
		// of its own, yet the light across its corner sees its top as well as its neighbours'.
		// `fallback` false: a top with no wall cells under it counts as reached (only walls decide).
		// A solid piece standing against wall `wi` and rising at least to its top (a stair's top step against
		// the wall it climbs): the fill never touches that wall's room side, yet its top lies level with the
		// piece's, lit when the piece is.  The piece is reached when a cell in or around its footprint is.
		// Grid cell of a box's foot tile (its south-east one); may be off the grid.
		auto box_cell = [&](const Sprite_box& s, int& cu, int& cv) {
			const int half_t = c_tilesize / 2;
			cu               = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
			cv               = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
		};
		// Does the fill reach a piece: a cell in or around its footprint (open and filled, or a wall face the
		// room's own fill touched)?
		auto piece_lit = [&](int i) -> bool {
			const Sprite_box& s = sprites[i];
			int               su, sv;
			box_cell(s, su, sv);
			for (int v = sv - s.yts; v <= sv + 1; ++v) {
				for (int u = su - s.xts; u <= su + 1; ++u) {
					if (u < 0 || v < 0 || u >= side || v >= side) {
						continue;
					}
					const size_t ci = static_cast<size_t>(v) * side + u;
					if (wall_cell(u, v) ? ring_face_dist(u, v) != 0 && !escaped_only(static_cast<int>(ci))
										: !Ring::Is_apron(ring, ci) && Grid::Dist(grid[ci]) != 0) {
						return true;
					}
				}
			}
			return false;
		};
		std::map<int, int>  pane_course_memo;    // A window's course base, per window box.
		std::map<int, bool> flank_memo;
		auto                lit_flank = [&](int wi) -> bool {
            if (const auto it = flank_memo.find(wi); it != flank_memo.end()) {
                return it->second;
            }
            const Sprite_box& w = sprites[wi];
            int               wu, wv;
            box_cell(w, wu, wv);
            const int wtop = w.fz / 4 + Fast_info(w.shape).get_3d_height();
            bool      lit  = false;
            for (int i = 0; i < nsprites && !lit; ++i) {
                const Sprite_box& s = sprites[i];
                if (i == wi || s.wall || s.building || !Fast_info(s.shape).is_solid() || s.fz / 4 > w.fz / 4
                    || s.fz / 4 + Fast_info(s.shape).get_3d_height() < wtop) {
                    continue;
                }
                int su, sv;
                box_cell(s, su, sv);
                // Touching footprints: along a side, or corner to corner (a corner post diagonal to the step).
                const bool ov_u  = su - s.xts < wu && wu - w.xts < su;
                const bool ov_v  = sv - s.yts < wv && wv - w.yts < sv;
                const bool tch_u = su == wu - w.xts || su - s.xts == wu;
                const bool tch_v = sv == wv - w.yts || sv - s.yts == wv;
                if (!((ov_v && tch_u) || (ov_u && tch_v) || (tch_u && tch_v))) {
                    continue;
                }
                lit = piece_lit(i);
            }
            flank_memo[wi] = lit;
            return lit;
		};
		// A riser looks onto the next step down, not floor: lit when a lower solid piece standing on that cell is.
		std::map<std::tuple<int, int, int>, bool> lower_front_memo;
		auto                                      lower_lit_front = [&](int fu, int fw, int ob) -> bool {
            if (ob < 0 || sprites == nullptr) {
                return false;
            }
            const auto key = std::make_tuple(fu, fw, ob);
            if (const auto it = lower_front_memo.find(key); it != lower_front_memo.end()) {
                return it->second;
            }
            bool&     res   = lower_front_memo[key];
            const int obase = sprites[ob].fz / 4;
            const int otop  = obase + Fast_info(sprites[ob].shape).get_3d_height();
            int       ou, ov;
            box_cell(sprites[ob], ou, ov);
            for (int i = 0; i < nsprites; ++i) {
                const Sprite_box& s  = sprites[i];
                const int         st = s.fz / 4 + Fast_info(s.shape).get_3d_height();
                // A step: lower, yet rising past the owner's base (the support a stacked pillar stands on does not).
                if (i == ob || s.wall || !Fast_info(s.shape).is_solid() || st >= otop || st <= obase) {
                    continue;
                }
                int su, sv;
                box_cell(s, su, sv);
                const bool under = fu > ou - sprites[ob].xts && fu <= ou && fw > ov - sprites[ob].yts && fw <= ov;
                if (!under && fu > su - s.xts && fu <= su && fw > sv - s.yts && fw <= sv && piece_lit(i)) {
                    res = true;
                    return true;
                }
            }
            res = false;
            return false;
		};
		auto wall_unit_reached = [&](int px, int py, bool fallback = true) -> bool {
			const int wi = ring != nullptr ? wall_owner(px, py) : -1;
			if (wi >= 0) {
				const Sprite_box& s        = sprites[wi];
				const int         half_t   = c_tilesize / 2;
				const int         ecu      = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
				const int         ecv      = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
				bool              any_wall = false;
				bool              reached  = false;
				for (int v = ecv - s.yts; v <= ecv + 1; ++v) {
					for (int u = ecu - s.xts; u <= ecu + 1; ++u) {
						if (u < 0 || v < 0 || u >= side || v >= side) {
							continue;
						}
						const bool   foot = u > ecu - s.xts && u <= ecu && v > ecv - s.yts && v <= ecv;
						const size_t ci   = static_cast<size_t>(v) * side + u;
						if (wall_cell(u, v)) {
							if (foot) {
								any_wall = true;
								// Escaped-only arrivals belong to the spills under an interior light (cf. the field strip).
								reached = reached || (ring_face_dist(u, v) != 0 && !escaped_only(static_cast<int>(ci)));
							}
						} else if (!foot && !Ring::Is_apron(ring, ci) && Grid::Dist(grid[ci]) != 0) {
							reached = true;
						}
					}
				}
				if (any_wall) {
					return reached || lit_flank(wi);
				}
			}
			if (!fallback) {
				return true;
			}
			// Reached is the top's own cell; the blend's share of a lit neighbour is smoothing, not arrival.
			return face_alpha(px, py, true, true, -1) != 0 && face_base != 0;
		};
		// An exterior slab strip painted over the pixel after wall box `after`: its index, or -1.
		auto strip_over = [&](int px, int py, int after) -> int {
			const auto rng = sprites_at(px, py);
			for (const int* p = rng.second; p != rng.first;) {
				const int i = *--p;
				if (i <= after) {
					break;
				}
				const Sprite_box& s = sprites[i];
				if (s.strip > 0 && px >= s.x0 && px <= s.x1 && py >= s.y0 && py <= s.y1
					&& (px > s.x1 - s.strip || py > s.y1 - s.strip)) {
					return i;
				}
			}
			return -1;
		};
		// Which drawn face (0 south, 1 east) a wall pixel lies on, by its owner box's diagonal split.
		// -1 when no wall box covers it.
		auto wall_face_side = [&](int px, int py) -> int {
			const int best = wall_owner(px, py);
			if (best < 0) {
				return -1;
			}
			const Sprite_box& s = sprites[best];
			return s.fx + s.fz - px >= s.fy + s.fz - py ? 0 : 1;
		};
		// The face pixel's owner (the box whose foot stands under it, else the topmost wall box) and which of
		// its faces the pixel lies on, from the owner's own geometry: an east face plane keeps fx - x <= fy - y.
		// An OBJECT-kind pixel (a halfwall stamped as an object) is a non-wall box's: wall_foot_owner only looks
		// at wall boxes and would hand it the wall post behind it.
		auto face_owner = [&](int px, int py) -> int {
			const int k = kindpix != nullptr ? kindpix[static_cast<size_t>(py) * kind_lw + px] : -1;
			if (k >= 0 && (Kind::Is_object(k) || k == Kind::Object_top || Kind::Is_cap(k))) {
				const int to = top_owner(px, py);
				if (to >= 0) {
					return to;
				}
			}
			const int fo = wall_foot_owner(px, py);
			return fo >= 0 ? fo : wall_owner(px, py);
		};
		auto owner_face_side = [&](int px, int py, int ob) -> int {
			if (ob < 0 || sprites == nullptr) {
				return -1;
			}
			return sprites[ob].fx - px <= sprites[ob].fy - py ? 1 : 0;
		};
		// Grid cell of the owner's own foot tile (its south-east tile), -1 off grid.
		auto owner_cell = [&](int ob) -> int {
			if (ob < 0 || sprites == nullptr || grid == nullptr) {
				return -1;
			}
			const Sprite_box& s      = sprites[ob];
			const int         half_t = c_tilesize / 2;
			const int         cu     = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
			const int         cv     = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
			return cu >= 0 && cv >= 0 && cu < side && cv < side ? cv * side + cu : -1;
		};
		// A wall face (side `fs`: 0 south, 1 east) of owner `ob` looking onto a LOW wall piece standing against it
		// (a halfwall beside a window jamb): up to that piece's top the face is buried, only above it is it the
		// opening's face.  Light passes over a halfwall, so the grid has no wall there to tell.  Wall pieces only:
		// furniture against a wall (a bed) hides what it covers by being drawn over it.
		std::map<std::tuple<int, int, int>, bool> low_wall_memo;
		auto                                      face_under_low_wall = [&](int px, int py, int fs, int ob) -> bool {
            if (fs < 0 || ob < 0 || sprites == nullptr || grid == nullptr) {
                return false;
            }
            const Sprite_box& s      = sprites[ob];
            const int         half_t = c_tilesize / 2;
            const int         ecu = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
            const int         ecv = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
            // Height above the owner's own base (fx/fy are its foot at its lift).
            const int pz = fs == 1 ? (s.fx - px) / 4 : (s.fy - py) / 4;
            if (pz >= 5) {
                return false;
            }
            const int  ntx = Light_tile_norm(light_tx + ecu + (fs == 1 ? 1 : 0) - grid_rt);
            const int  nty = Light_tile_norm(light_ty + ecv + (fs == 0 ? 1 : 0) - grid_rt);
            const int  wz  = s.fz / 4 + std::max(pz, 0);
            const auto key = std::make_tuple(ntx, nty, wz);
            if (const auto it = low_wall_memo.find(key); it != low_wall_memo.end()) {
                return it->second;
            }
            bool      hit  = false;
            Game_map* gmap = Game_window::get_instance()->get_map();
            for (int dcy = 0; dcy < 2 && !hit; ++dcy) {
                for (int dcx = 0; dcx < 2 && !hit; ++dcx) {
                    Map_chunk* const ch = gmap->get_chunk_safely(
                            (ntx / c_tiles_per_chunk + dcx) % c_num_chunks, (nty / c_tiles_per_chunk + dcy) % c_num_chunks);
                    if (ch == nullptr) {
                        continue;
                    }
                    Object_iterator it(ch->get_objects());
                    Game_object*    o;
                    while ((o = it.get_next()) != nullptr) {
                        const Shape_info& oi = Fast_info(o->get_shapenum());
                        const int         lo = o->get_lift();
                        // LOW: below the owner's own top -- a piece as tall continues the run.  A CLOSED door fills its doorway
                        // however tall: the jamb's face beside its leaf is covered.
                        if (o->as_actor() != nullptr || !oi.is_solid() || wz < lo || wz >= lo + oi.get_3d_height()
                            || (lo + oi.get_3d_height() >= s.fz / 4 + Fast_info(s.shape).get_3d_height()
                                && !(oi.is_door() && o->is_closed_door()))
                            || !o->get_footprint().has_world_point(ntx, nty) || o->is_dragable()
                            || (oi.get_shape_class() != Shape_info::unusable && oi.get_shape_class() != Shape_info::building
                                && !(oi.is_door() && o->is_closed_door()))) {
                            continue;
                        }
                        hit = true;
                        break;
                    }
                }
            }
            low_wall_memo[key] = hit;
            return hit;
		};
		// Is the light on the far side of the plane of the wall face drawn at the pixel?
		auto face_behind_light = [&](int px, int py) -> bool {
			const int best = wall_owner(px, py);
			if (best < 0) {
				return false;
			}
			const Sprite_box& s      = sprites[best];
			const int         f0x    = s.fx + s.fz;
			const int         f0y    = s.fy + s.fz;
			const int         half_t = c_tilesize / 2;
			// On the box's top surface (a slab-marked wall top carries the Wall_face kind too): no plane to be behind.
			// The stamp's top rect: the art less 4*height right and below (art rising above the box is still top).
			const int ztop = s.fz + 4 * Fast_info(s.shape).get_3d_height();
			if (std::min(f0x - px, f0y - py) >= ztop) {
				return false;
			}
			if (f0x - px >= f0y - py) {
				return pos_cell((static_cast<float>(f0y - half_t - foot_shift) - grid_v) * inv_cell) >= grid_rt;
			}
			return pos_cell((static_cast<float>(f0x - half_t - foot_shift) - grid_u) * inv_cell) >= grid_rt;
		};
		auto raised_wall = [&](const Sprite_box& s) {
			return s.wall && s.fz / 4 >= 5 * (light_floor_storey + 1);
		};
		// Lowest cover (any solid, floor or roof piece) above the light's storey over a grid cell; 99 = none.
		std::vector<unsigned char> cell_ceiling;
		auto                       ceiling_over = [&](size_t ci) -> int {
            if (cell_ceiling.empty()) {
                cell_ceiling.assign(static_cast<size_t>(side) * side, 0xff);
            }
            if (cell_ceiling[ci] != 0xff) {
                return cell_ceiling[ci];
            }
            const int tx   = Light_tile_norm(light_tx - grid_rt + static_cast<int>(ci % side));
            const int ty   = Light_tile_norm(light_ty - grid_rt + static_cast<int>(ci / side));
            const int from = 5 * (light_floor_storey + 1);
            int       best = 99;
            Game_map* gmap = Game_window::get_instance()->get_map();
            for (int dcy = -1; dcy <= 1; ++dcy) {
                for (int dcx = -1; dcx <= 1; ++dcx) {
                    Map_chunk* const ch = gmap->get_chunk_safely(tx / c_tiles_per_chunk + dcx, ty / c_tiles_per_chunk + dcy);
                    if (ch == nullptr) {
                        continue;
                    }
                    Object_iterator it(ch->get_objects());
                    Game_object*    o;
                    while ((o = it.get_next()) != nullptr) {
                        const Shape_info& oi = Fast_info(o->get_shapenum());
                        const int         ol = o->get_lift();
                        if (ol < from || ol >= best || o->as_actor() != nullptr || o->is_dragable()
                            || !(oi.is_solid() || oi.is_floor(o->get_framenum()) || oi.is_roof(o->get_framenum()))) {
                            continue;
                        }
                        if (o->get_footprint().has_world_point(tx, ty)) {
                            best = ol;
                        }
                    }
                }
            }
            cell_ceiling[ci] = static_cast<unsigned char>(best);
            return best;
		};
		// A piece of the mountain itself under a deck it lands on (rock risen to the plateau beside a mountain top):
		// its top reaches the storey floor and a get_mountain_top_type() piece rests on or beside it at that level.
		// The rock's art carries no separate top stamp, so the plateau's spill read it as a face below its floor --
		// the lit plateau stopped in a hard edge where the mountain-top blocks met the rock.  Memoized per sprite.
		std::vector<signed char> mountain_side_memo;
		auto                     mountain_side = [&](int o) -> bool {
            if (o < 0 || sprites == nullptr) {
                return false;
            }
            if (mountain_side_memo.empty()) {
                mountain_side_memo.assign(static_cast<size_t>(nsprites), -1);
            }
            signed char& m = mountain_side_memo[static_cast<size_t>(o)];
            if (m >= 0) {
                return m != 0;
            }
            const Sprite_box& s   = sprites[o];
            const int         ft  = s.fz / 4 + Fast_info(s.shape).get_3d_height();
            bool              hit = false;
            int               su, sv;
            box_cell(s, su, sv);
            for (int i = 0; i < nsprites && !hit; ++i) {
                const Sprite_box& t = sprites[i];
                if (i == o || t.fz / 4 != ft || Fast_info(t.shape).get_mountain_top_type() == Shape_info::not_mountain_top) {
                    continue;
                }
                int tu, tv;
                box_cell(t, tu, tv);
                // Footprints [u-xts+1, u] x [v-yts+1, v] within one cell of each other.
                hit = su - s.xts + 1 <= tu + 1 && tu - t.xts + 1 <= su + 1 && sv - s.yts + 1 <= tv + 1 && tv - t.yts + 1 <= sv + 1;
            }
            m = hit ? 1 : 0;
            return hit;
		};
		// This spill landed ON a mountain top (the deck at its own tile is a get_mountain_top_type() piece): there is
		// no storey beneath it, only the mountain, so its cone simply continues at the plateau's height across
		// everything drawn there -- rock, the mountain wall, a cave entrance's terrain -- until it runs out.  The
		// building rules (clear pixels are ground below the storey; ground objects read at their foot) cut it into
		// hard edges along each sprite's outline.
		int  mountain_deck_memo = -1;
		auto mountain_deck      = [&]() -> bool {
            if (mountain_deck_memo < 0) {
                mountain_deck_memo = 0;
                Game_map* const gm = Game_window::get_instance()->get_map();
                if (is_spill && spill_floor >= 1 && gm != nullptr) {
                    const int tx = Light_tile_norm(light_tx);
                    const int ty = Light_tile_norm(light_ty);
                    for (int dcy = 0; dcy < 2 && mountain_deck_memo == 0; ++dcy) {
                        for (int dcx = 0; dcx < 2 && mountain_deck_memo == 0; ++dcx) {
                            Map_chunk* const ch = gm->get_chunk_safely(
                                    (tx / c_tiles_per_chunk + dcx) % c_num_chunks, (ty / c_tiles_per_chunk + dcy) % c_num_chunks);
                            if (ch == nullptr) {
                                continue;
                            }
                            Object_iterator it(ch->get_objects());
                            Game_object*    o;
                            while ((o = it.get_next()) != nullptr) {
                                const Shape_info& oi = Fast_info(o->get_shapenum());
                                if (oi.get_mountain_top_type() != Shape_info::not_mountain_top
                                    && o->get_footprint().has_world_point(tx, ty)
                                    && o->get_lift() + oi.get_3d_height() <= 5 * (spill_floor + 1)
                                    && o->get_lift() + oi.get_3d_height() >= 5 * spill_floor) {
                                    mountain_deck_memo = 1;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            return mountain_deck_memo != 0;
		};
		// The top owner at a pixel sits OVER the light: based on a storey above the light's floor, and some reached
		// OPEN cell of its footprint (not a wall the fill only rings, not a stairwell) is covered at or below that
		// base -- the light flowed in beneath the piece (a mountain block roofing a passage under a wall), so its top
		// is the roof over the light, seen from above.  A stacked hall pillar stands on wall cells; open sky has no
		// cover.  Memoized per owner for this splat.
		std::vector<signed char> roofed_over_memo;
		auto                     roofed_over_top = [&](int px, int py) -> bool {
            const int o = top_owner(px, py);
            if (o < 0) {
                return false;
            }
            if (roofed_over_memo.empty()) {
                roofed_over_memo.assign(static_cast<size_t>(nsprites), -1);
            }
            signed char& m = roofed_over_memo[static_cast<size_t>(o)];
            if (m >= 0) {
                return m != 0;
            }
            const Sprite_box& s    = sprites[o];
            const int         base = s.fz / 4;
            bool              over = false;
            if (base >= 5 * (light_floor_storey + 1)) {
                int su, sv;
                box_cell(s, su, sv);
                for (int v = sv - s.yts + 1; v <= sv && !over; ++v) {
                    for (int u = su - s.xts + 1; u <= su && !over; ++u) {
                        if (u < 0 || v < 0 || u >= side || v >= side || wall_cell(u, v)) {
                            continue;
                        }
                        const size_t        ci = static_cast<size_t>(v) * side + u;
                        const unsigned char g  = grid[ci];
                        if (Grid::Dist(g) == 0 || (g & Grid::Flag) != 0) {
                            continue;
                        }
                        over = ceiling_over(ci) <= base;
                    }
                }
            }
            m = over ? 1 : 0;
            return over;
		};
		// Is wall cell `wc` an opening at the storey with floor `lo` through which this (outdoor) light reaches:
		// it borders open-sky ground the light reaches, and Light_upper_opening finds it open looking into the
		// cell on the far side.  The one test the halfwall top in a window gap and the jamb faces beside it share;
		// a shape filling the gap (a closed shutter on the halfwall) closes it.
		std::map<std::pair<int, int>, bool> opens_memo;
		auto                                opens_to_light = [&](int wc, int lo) -> bool {
            if (wc < 0 || grid == nullptr || ring == nullptr || !Ring::Is_wall(ring, static_cast<size_t>(wc))) {
                return false;
            }
            const auto key = std::make_pair(wc, lo);
            if (const auto it = opens_memo.find(key); it != opens_memo.end()) {
                return it->second;
            }
            Game_map* const gm  = Game_window::get_instance()->get_map();
            const int       wu  = wc % side;
            const int       wv  = wc / side;
            bool            res = false;
            for (const auto& [du, dv] : {
                         std::pair{ 1,  0},
                         std::pair{-1,  0},
                         std::pair{ 0,  1},
                         std::pair{ 0, -1}
            }) {
                const int u = wu + du;
                const int v = wv + dv;
                if (u < 0 || v < 0 || u >= side || v >= side) {
                    continue;
                }
                const size_t ni = static_cast<size_t>(v) * side + u;
                if (Ring::Is_wall(ring, ni) || Grid::Dist(grid[ni]) == 0 || ceiling_over(ni) != 99) {
                    continue;
                }
                if (Light_upper_opening(
                            gm, light_tx + wu - grid_rt, light_ty + wv - grid_rt, light_tx + wu - du - grid_rt,
                            light_ty + wv - dv - grid_rt, lo, nullptr)
                    > 0) {
                    res = true;
                    break;
                }
            }
            opens_memo[key] = res;
            return res;
		};
		// The open cell a side face reads (ring_side_dist's order: own side, then the other one); -1 if none.
		auto face_front_cell = [&](int cu, int cv, int sd) -> int {
			for (const int d : {sd, sd == 0 ? 1 : 0}) {
				int nu = cu, nv = cv;
				for (int st = 0; st < 4; ++st) {
					nu += d == 1 ? 1 : 0;
					nv += d == 0 ? 1 : 0;
					if (nu >= side || nv >= side) {
						break;
					}
					const int ni = nv * side + nu;
					if (!Ring::Is_wall(ring, static_cast<size_t>(ni))) {
						return ni;
					}
				}
			}
			return -1;
		};
		// Whether the tile in front of a wall box's face (0 south, 1 east) at owner cell (cu, cv) holds
		// a solid standing from at most the box's base to at least its top: that face is never seen.
		std::map<int, bool> buried_cache;
		auto                face_buried = [&](const Sprite_box& s, int cu, int cv, int sd) -> bool {
            const int fu = cu + (sd == 1 ? 1 : 0);
            const int fv = cv + (sd == 0 ? 1 : 0);
            if (cu < 0 || cv < 0 || fu >= side || fv >= side) {
                return false;
            }
            const int  base = s.fz / 4;
            const int  need = base + Fast_info(s.shape).get_3d_height();
            const int  key  = ((fv * side + fu) * 64 + std::min(base, 63)) * 64 + std::min(need, 63);
            const auto hit  = buried_cache.find(key);
            if (hit != buried_cache.end()) {
                return hit->second;
            }
            const int tx     = Light_tile_norm(light_tx - grid_rt + fu);
            const int ty     = Light_tile_norm(light_ty - grid_rt + fv);
            bool      buried = false;
            Game_map* gmap   = Game_window::get_instance()->get_map();
            for (int dcy = -1; dcy <= 1 && !buried; ++dcy) {
                for (int dcx = -1; dcx <= 1 && !buried; ++dcx) {
                    Map_chunk* const ch = gmap->get_chunk_safely(tx / c_tiles_per_chunk + dcx, ty / c_tiles_per_chunk + dcy);
                    if (ch == nullptr) {
                        continue;
                    }
                    Object_iterator it(ch->get_objects());
                    Game_object*    o;
                    while ((o = it.get_next()) != nullptr) {
                        const Shape_info& oi = Fast_info(o->get_shapenum());
                        const int         ol = o->get_lift();
                        if (ol > base || ol + oi.get_3d_height() < need || !oi.is_solid() || o->as_actor() != nullptr
                            || o->is_dragable()) {
                            continue;
                        }
                        if (o->get_footprint().has_world_point(tx, ty)) {
                            buried = true;
                            break;
                        }
                    }
                }
            }
            buried_cache[key] = buried;
            return buried;
		};
		// The z-blind field is the light's own storey: a pixel of anything a storey up reads its foot only.
		auto above_storey = [&](int px, int py, int m) -> bool {
			const int s = Mask::Is_tall(m)        ? m - Mask::Tall
						  : Mask::Is_slab(m)      ? m - Mask::Slab
						  : Mask::Is_slab_edge(m) ? m - Mask::Slab_edge
												  : -1;
			if (s > light_floor_storey) {
				return true;
			}
			const int oi = std::max(wall_owner(px, py), wall_owner(px, py, false));
			return oi >= 0 && sprites[oi].fz / 4 >= 5 * (light_floor_storey + 1);
		};
		// A wall face pixel's light read from its owner box's own face edge: the per-pixel foot at a
		// top/face seam rounds into the block's unreached interior and read 0 (a dark stripe).
		int  wf_buried       = -1;    // Owner whose hinted face the last wall_face_alpha found buried.
		auto wall_face_alpha = [&](int px, int py, bool skip_raised = false, int side_hint = -1, int owner = -1,
								   bool ignore_buried = false) -> int {
			const int best = owner >= 0 ? owner : wall_owner(px, py);
			if (best < 0) {
				return -1;
			}
			const Sprite_box& s = sprites[best];
			// The lattice is the light's storey: a wall wholly below its floor has no owner cell on it.
			if (s.fz / 4 + Fast_info(s.shape).get_3d_height() <= 5 * light_floor_storey) {
				return -1;
			}
			const int f0x    = s.fx + s.fz;
			const int f0y    = s.fy + s.fz;
			const int half_t = c_tilesize / 2;
			// The corner diagonal itself is ambiguous; a face mark already knows its side.
			int side_ = side_hint >= 0 ? side_hint : (f0x - px >= f0y - py ? 0 : 1);
			// Ground point in lattice units: across the face at its owner cell's centre, along it
			// clamped to the box's end-cell centres (the lattice is not tile-aligned at every lift,
			// so a pixel clamp still blended the end tile into the stripped cell past it).
			auto lat_u = [&](int g) {
				return (static_cast<float>(g - foot_shift) - grid_u) * inv_cell;
			};
			auto lat_v = [&](int g) {
				return (static_cast<float>(g - foot_shift) - grid_v) * inv_cell;
			};
			float pu, pv;
			// Along the face, the end cell's centre bounds the point unless the face runs on into an
			// answered wall cell: then up to the shared edge, so adjoining pieces blend instead of stepping.
			auto along_lo = [&](int lo, int u_, int v_, int sd) {
				const bool runs
						= u_ >= 0 && v_ >= 0 && u_ < side && v_ < side && wall_cell(u_, v_) && side_face_dist(u_, v_, sd) != 0;
				return static_cast<float>(lo) - (runs ? 0.99f : 0.5f);
			};
			auto along_hi = [&](int hi, int u_, int v_, int sd) {
				const bool runs
						= u_ >= 0 && v_ >= 0 && u_ < side && v_ < side && wall_cell(u_, v_) && side_face_dist(u_, v_, sd) != 0;
				return static_cast<float>(hi) - (runs ? 0.01f : 0.5f);
			};
			auto face_point = [&](int sd) {
				if (sd == 0) {
					const int cv_ = pos_cell(lat_v(f0y - half_t));
					const int lo  = pos_cell(lat_u(f0x - s.xts * c_tilesize + half_t));
					const int hi  = pos_cell(lat_u(f0x - half_t));
					pv            = static_cast<float>(cv_) - 0.5f;
					pu = std::clamp(lat_u(px + (f0y - py)), along_lo(lo, lo - 1, cv_, sd), along_hi(hi, hi + 1, cv_, sd));
				} else {
					const int cu_ = pos_cell(lat_u(f0x - half_t));
					const int lo  = pos_cell(lat_v(f0y - s.yts * c_tilesize + half_t));
					const int hi  = pos_cell(lat_v(f0y - half_t));
					pu            = static_cast<float>(cu_) - 0.5f;
					pv = std::clamp(lat_v(py + (f0x - px)), along_lo(lo, cu_, lo - 1, sd), along_hi(hi, cu_, hi + 1, sd));
				}
			};
			face_point(side_);
			// A face standing against a solid at least as tall is hidden: a pixel the box test gives it
			// is a later sprite's 1px edge art.  Take the other face, or none if both are buried.
			if (!ignore_buried && face_buried(s, pos_cell(pu), pos_cell(pv), side_)) {
				if (side_hint >= 0) {
					wf_buried = best;
					return 0;
				}
				side_ = side_ == 0 ? 1 : 0;
				face_point(side_);
				if (face_buried(s, pos_cell(pu), pos_cell(pv), side_)) {
					return 0;
				}
			}
			const int cu = pos_cell(pu);
			const int cv = pos_cell(pv);
			if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
				return 0;
			}
			const size_t oci = static_cast<size_t>(cv) * side + cu;
			// A wall a storey up is not lit by ground lying under a ceiling no higher than its base.
			if (skip_raised && raised_wall(s)) {
				// Light leaving through an opening below cannot reach the outer face above it: the light is behind that plane.
				if ((side_ == 0 && cv >= grid_rt) || (side_ == 1 && cu >= grid_rt)) {
					return 0;
				}
				const int rc = Ring::Is_wall(ring, oci) ? face_front_cell(cu, cv, side_) : static_cast<int>(oci);
				if (rc >= 0 && ceiling_over(static_cast<size_t>(rc)) <= s.fz / 4) {
					return 0;
				}
			}
			return face_alpha_from(grid_u + pu * cell, grid_v + pv * cell, cu, cv, false, false, side_);
		};
		// A thick block's top over its own unreached interior: nearest arrival through the mass (BFS,
		// charged per step), so the whole top fades from its lit edge.  -1 = foot not in a mass.
		// Cells under a sprite whose top reaches `ztop` px: the mass a top may walk through.
		// A room's unreached floor is not mass, or a pillar's top would take the walls' outside arrival across a shut
		// room.
		std::map<int, std::vector<unsigned char>> mass_cells;
		auto                                      mass_at = [&](int ztop) -> const std::vector<unsigned char>& {
            std::vector<unsigned char>& m = mass_cells[ztop];
            if (m.empty()) {
                m.assign(static_cast<size_t>(side) * side, 0);
                const int half_t = c_tilesize / 2;
                for (int i = 0; i < nsprites; ++i) {
                    const Sprite_box& s = sprites[i];
                    if (s.strip > 0 || std::max(static_cast<int>(s.fz), s.fy + s.fz - s.yts * c_tilesize + 1 - s.y0) < ztop) {
                        continue;
                    }
                    const int ecu = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
                    const int ecv = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
                    for (int v = std::max(ecv - s.yts + 1, 0); v <= ecv && v < side; ++v) {
                        for (int u = std::max(ecu - s.xts + 1, 0); u <= ecu && u < side; ++u) {
                            m[static_cast<size_t>(v) * side + u] = 1;
                        }
                    }
                }
            }
            return m;
		};
		std::map<std::pair<int, int>, int> mass_cache;
		// `otop`: the top's owner's height in px (-1: unknown, walk freely).
		auto mass_alpha = [&](int cidx, int otop) -> int {
			constexpr int max_steps = 8;
			const size_t  ci0       = static_cast<size_t>(cidx);
			if (Grid::Dist(grid[ci0]) != 0 || Ring::Is_wall(ring, ci0) || apron_cell(cidx)) {
				return -1;
			}
			const auto key = std::make_pair(cidx, otop);
			const auto hit = mass_cache.find(key);
			if (hit != mass_cache.end()) {
				return hit->second;
			}
			const std::vector<unsigned char>* mass = otop >= 0 ? &mass_at(otop) : nullptr;
			// A wall a lift taller than the top: its arrival lights its own top, which shadows ours.
			const std::vector<unsigned char>* taller = otop >= 0 ? &mass_at(otop + 4) : nullptr;
			std::vector<int>                  seen{cidx};
			std::vector<int>                  cur{cidx};
			int                               best = 0;
			for (int step = 1; step <= max_steps && !cur.empty() && (best == 0 || best > step); ++step) {
				std::vector<int> nxt;
				for (const int c : cur) {
					const int nbs[4][2] = {
							{c % side - 1,     c / side},
                            {c % side + 1,     c / side},
                            {    c % side, c / side - 1},
                            {    c % side, c / side + 1}
                    };
					for (const auto& n : nbs) {
						if (n[0] < 0 || n[1] < 0 || n[0] >= side || n[1] >= side) {
							continue;
						}
						const int ni = n[1] * side + n[0];
						if (std::find(seen.begin(), seen.end(), ni) != seen.end()) {
							continue;
						}
						seen.push_back(ni);
						if (apron_cell(ni)) {
							continue;
						}
						const int d = Ring::Is_wall(ring, static_cast<size_t>(ni))
											  ? (escaped_only(ni) || (taller != nullptr && (*taller)[static_cast<size_t>(ni)] != 0)
														 ? 0
														 : top_face_dist(n[0], n[1]))
											  : Grid::Dist(grid[static_cast<size_t>(ni)]);
						if (d != 0) {
							if (best == 0 || d + step < best) {
								best = d + step;
							}
						} else if (
								!Ring::Is_wall(ring, static_cast<size_t>(ni))
								&& (mass == nullptr || (*mass)[static_cast<size_t>(ni)] != 0)) {
							nxt.push_back(ni);
						}
					}
				}
				cur.swap(nxt);
			}
			const int ma    = best == 0 ? 0 : cell_dome(cidx, best);
			mass_cache[key] = ma;
			return ma;
		};
		// mass_alpha for the wall top at pixel (px,py), walking only mass as tall as its owner.
		auto top_mass_alpha = [&](int px, int py) -> int {
			const int fx = foot_x(px, py) - foot_shift;
			const int fy = foot_y(px, py) - foot_shift;
			const int mci
					= own_mass_cell((static_cast<float>(fx) - grid_u) * inv_cell, (static_cast<float>(fy) - grid_v) * inv_cell);
			const int fci = mci >= 0 ? mci : foot_cell(px, py);
			if (fci < 0) {
				return -1;
			}
			const int wi = wall_owner(px, py);
			// A wall-marked object (a sill) walks its own mass, not the room floor beside it.
			const int oi = wi >= 0 ? wi : top_owner(px, py);
			return mass_alpha(
					fci, oi >= 0 ? std::max(
										   static_cast<int>(sprites[oi].fz),
										   sprites[oi].fy + sprites[oi].fz - sprites[oi].yts * c_tilesize + 1 - sprites[oi].y0)
								 : -1);
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
		// The field at the pixel's own tile: its stamped (z=0) foot carried up to the lattice, blended
		// between cell centres.  A wall is not dark ground: the blend never reaches into one.
		const bool foot_reads = !is_spill && face_data && ftmpl != nullptr;
		auto       foot_field = [&](int px, int py) -> int {
            const float pu = (static_cast<float>(foot_x(px, py) - foot_shift) - grid_u) * inv_cell;
            const float pv = (static_cast<float>(foot_y(px, py) - foot_shift) - grid_v) * inv_cell;
            const int   cu = pos_cell(pu);
            const int   cv = pos_cell(pv);
            if (cu < 0 || cv < 0 || cu >= side || cv >= side) {
                return 0;
            }
            const int ci   = cv * side + cu;
            const int base = cell_dome(ci, Grid::Dist(grid[static_cast<size_t>(ci)]));
            auto      at   = [&](int u_, int v_) -> int {
                if (u_ < 0 || v_ < 0 || u_ >= side || v_ >= side) {
                    return base;
                }
                const int ni = v_ * side + u_;
                // Nor is ground the spills took over: max-composited, a ramp toward it would leave a dim seam.
                // An apron cell is dark for this light in every reader, so the blend must not ramp toward it either.
                if (ring != nullptr
                    && (Ring::Is_wall(ring, static_cast<size_t>(ni)) || Ring::Is_handoff(ring, static_cast<size_t>(ni))
                        || Ring::Is_apron(ring, static_cast<size_t>(ni)))) {
                    return base;
                }
                return cell_dome(ni, Grid::Dist(grid[static_cast<size_t>(ni)]));
            };
            const float ox  = pos_off(pu, cu);
            const float oy  = pos_off(pv, cv);
            const int   nu  = ox >= 0.0f ? cu + 1 : cu - 1;
            const int   nv  = oy >= 0.0f ? cv + 1 : cv - 1;
            const float wx  = ox >= 0.0f ? ox : -ox;
            const float wy  = oy >= 0.0f ? oy : -oy;
            const float top = static_cast<float>(base) + (static_cast<float>(at(nu, cv)) - static_cast<float>(base)) * wx;
            const float bot
                    = static_cast<float>(at(cu, nv)) + (static_cast<float>(at(nu, nv)) - static_cast<float>(at(cu, nv))) * wx;
            return static_cast<int>(top + (bot - top) * wy + 0.5f);
		};
		// GLASS: what a pane shows is the light at its OWN tile -- the far side
		// counts, that is the point of glass -- so all four face arrivals count,
		// and an opening tile the fill flows through contributes its own cell
		// value.  The z-blind field cannot serve here: a tall window's upper
		// pixels would read cells two or three tiles up-screen and go dark.
		// How far the un-shear walk may step.  MEASURED, not derived: a window
		// high in a two-storey wall needs 5 (its art rises ~40px = 5 tiles at the
		// 4px-per-z shear).  The pane's own lift is not available here, so this
		// is a bound with headroom rather than the exact shear; the walk stops at
		// the first answer, so a larger cap only matters where every nearer cell
		// is unreached.
		// A long walk could cross the room to a far spill's own mouth; the foot
		// stamp unshears lift too, so crossing the wall is enough.
		constexpr int pane_walk_max = 2;
		auto          pane_alpha    = [&](int px, int py) -> int {
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
            // An open pane tile is its own answer: there is no wall to cross.
            const bool cross = ring == nullptr || Ring::Is_wall(ring, static_cast<size_t>(cidx));
            for (int s = 1; cross && a == 0 && s <= pane_walk_max; ++s) {
                const int wu = cidx % side + s;
                const int wv = cidx / side + s;
                if (wu >= side || wv >= side) {
                    break;
                }
                const int wi = wv * side + wu;
                const int wd = (ring != nullptr && Ring::Is_wall(ring, static_cast<size_t>(wi)))
												   ? (escaped_only(wi) ? 0 : ring_face_dist(wu, wv))
												   : (apron_cell(wi) ? 0 : Grid::Dist(grid[static_cast<size_t>(wi)]));
                if (wd != 0) {
                    a = cell_dome(wi, wd);
                }
            }
            return a;
		};
		// Sprites that extended the box past the pool, and per row the x spans their art boxes cover.
		std::vector<int>           reach_ids;
		std::vector<unsigned char> reach_row;
		if (nreach > 0 && x1 >= x0) {
			for (int i = 0; i < nsprites; ++i) {
				const Sprite_box& s = sprites[i];
				if (!s.wall && s.fx >= root_x0 && s.fx <= root_x1 && s.fy >= root_y0 && s.fy <= root_y1) {
					reach_ids.push_back(i);
				}
			}
			reach_row.resize(static_cast<size_t>(x1 - x0 + 1));
		}
		// ACTOR pixels sample like ground here -- silhouette-shaped
		// deviations would ghost in the cached static coverage; their cap at
		// the light level of their own feet is applied per frame on the
		// composite (Light_layers::build).
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
			// A construct's south face is Tall to a light south of it, Roof to the rest.
			// A low wall's face is a side face only to a spill or interior light whose fill it walls; Clear to the rest.
			// Low faces the remap below darkens for EVERY light: Roof alone is lit as a whole unit by an outdoor light.
			static std::vector<unsigned char> sdark;
			bool                              row_dark = false;
			if (roofrow != nullptr) {
				int xs = x0;
				while (xs <= x1 && !Mask::Is_tall_south(roofrow[xs]) && !Mask::Is_low_face(roofrow[xs])) {
					++xs;
				}
				if (xs <= x1) {
					static std::vector<unsigned char> srow;
					srow.resize(static_cast<size_t>(x1) + 1);
					std::copy(roofrow + x0, roofrow + x1 + 1, srow.begin() + x0);
					sdark.assign(static_cast<size_t>(x1) + 1, 0);
					for (int x = xs; x <= x1; ++x) {
						const int m = srow[x];
						if (Mask::Is_low_face(m)) {
							// On a mountain top the cone continues across the rock's faces (see mountain_deck): no storey
							// lies beneath it, and darkening them cut each rock's outline out of the cone.
							if (mountain_deck()) {
								srow[x] = static_cast<unsigned char>(Mask::Clear);
								continue;
							}
							const int ci = (footdx != nullptr && grid != nullptr) ? foot_cell(x, y) : -1;
							// A light level with or behind the face's plane cannot reach it.
							// Stairwell steps ignore facing (the steps below take the plain field): a face on a well cell stays.
							const bool well_face
									= veto_roof && ci >= 0 && grid != nullptr && (grid[static_cast<size_t>(ci)] & Grid::Flag) != 0;
							if (ci >= 0 && !well_face && (m == Mask::Low_face ? ci / side : ci % side) >= grid_rt) {
								srow[x]  = static_cast<unsigned char>(Mask::Roof);
								sdark[x] = 1;
								row_dark = true;
								continue;
							}
							// Its owner cell as floor of an upper-storey fill: the light stands on the structure, the face is under
							// its floor. Unless it faces a floor hole (a stairwell's top step): the light shines down the well onto
							// it.
							if ((is_spill || veto_roof) && light_floor_storey >= 1 && ci >= 0 && grid != nullptr && ring != nullptr
								&& !Ring::Is_wall(ring, static_cast<size_t>(ci)) && Grid::Dist(grid[static_cast<size_t>(ci)]) != 0
								&& !(veto_roof && [&] {
									   // The face's own tile is the well (a stairwell's top step), or it faces one.
									   if ((grid[static_cast<size_t>(ci)] & Grid::Flag) != 0) {
										   return true;
									   }
									   const int fu = ci % side + (m == Mask::Low_face ? 0 : 1);
									   const int fv = ci / side + (m == Mask::Low_face ? 1 : 0);
									   return fu < side && fv < side
											  && (grid[static_cast<size_t>(fv) * side + fu] & Grid::Flag) != 0;
								   }())) {
								srow[x]  = static_cast<unsigned char>(Mask::Roof);
								sdark[x] = 1;
								row_dark = true;
								continue;
							}
							// A thick wall's far column is never touched by the fill, so it has no Wall_flag: unreached counts too.
							// As Clear an interior light's z-blind field sampled stripped cells past the door: spots.
							const bool walled = (is_spill || veto_roof) && ring != nullptr && ci >= 0
												&& (Ring::Is_wall(ring, static_cast<size_t>(ci))
													|| (grid != nullptr && Grid::Dist(grid[static_cast<size_t>(ci)]) == 0));
							srow[x] = static_cast<unsigned char>(
									walled ? (m == Mask::Low_face ? Mask::Face_south : Mask::Face_east) : Mask::Clear);
							continue;
						}
						if (!Mask::Is_tall_south(m)) {
							continue;
						}
						bool lit = !veto_roof;
						if (is_spill && grid != nullptr) {
							const int ci = footdx != nullptr ? foot_cell(x, y) : -1;
							lit          = ci >= 0 && ci / side < grid_rt;
						}
						srow[x] = static_cast<unsigned char>(lit ? Mask::Tall + (m - Mask::Tall_south) : Mask::Roof);
					}
					roofrow = srow.data();
				}
			}
			if (nreach > 0) {
				std::fill(reach_row.begin(), reach_row.end(), 0);
				for (const int i : reach_ids) {
					const Sprite_box& s = sprites[i];
					if (y < s.y0 || y > s.y1) {
						continue;
					}
					const int a = std::max<int>(x0, s.x0);
					const int b = std::min<int>(x1, s.x1);
					if (a <= b) {
						std::fill(reach_row.begin() + (a - x0), reach_row.begin() + (b - x0 + 1), 1);
					}
				}
			}
			for (int x = x0; x <= x1; ++x) {
				// Outside the pool proper only the sprites that EXTENDED the box
				// may light, and only inside their own art box: membership is
				// the object's box, not the clamped per-pixel foot channel.
				if (nreach > 0 && (x < pool_x0 || y < pool_y0) && reach_row[static_cast<size_t>(x - x0)] == 0) {
					continue;
				}
				if (row_dark && sdark[static_cast<size_t>(x)] != 0) {
					continue;
				}
				// The branches below decide per light kind (veto / spill /
				// exterior) what stays dark, what is lit as a whole unit
				// (bypass_field) and what keeps sampling the propagated field.
				// A wall-face or object pixel resolves by its OWNER's surface
				// (ring arrivals / foot cell) instead of the mask heuristics.
				// An object's flat top is an object everywhere but the averted test.
				auto cap_of = [](int k) {
					return k == Kind::Object_cap ? 1 : k == Kind::Object_cap_ew ? 0 : -1;
				};
				int  cap_axis = kindrow != nullptr ? cap_of(kindrow[x]) : -1;
				bool obj_cap  = cap_axis >= 0;
				bool obj_top  = obj_cap || (kindrow != nullptr && kindrow[x] == Kind::Object_top);
				int  obj_side = kindrow == nullptr                 ? -1
								: kindrow[x] == Kind::Object_south ? 0
								: kindrow[x] == Kind::Object_east  ? 1
																   : -1;
				// A face run of 1-3px closed by tops at both ends is the junction overlapping pieces leave, not a face.
				if (obj_side >= 0) {
					const unsigned char m  = kindrow[x];
					auto                kt = [&](int xx, int yy) {
                        return kindpix[static_cast<size_t>(yy) * kind_lw + xx];
					};
					auto is_top = [](int k) {
						return k == Kind::Object_top || Kind::Is_cap(k);
					};
					int l = x;
					while (l > x0 && x - l < 3 && kindrow[l - 1] == m) {
						--l;
					}
					int r = x;
					while (r < x1 && r - x < 3 && kindrow[r + 1] == m) {
						++r;
					}
					bool seam = r - l < 3 && l > x0 && r < x1 && is_top(kindrow[l - 1]) && is_top(kindrow[r + 1]);
					int  cap  = seam ? std::max(cap_of(kindrow[l - 1]), cap_of(kindrow[r + 1])) : -1;
					if (!seam) {
						int t = y;
						while (t > y0 && y - t < 3 && kt(x, t - 1) == m) {
							--t;
						}
						int b = y;
						while (b < y1 && b - y < 3 && kt(x, b + 1) == m) {
							++b;
						}
						seam = b - t < 3 && t > y0 && b < y1 && is_top(kt(x, t - 1)) && is_top(kt(x, b + 1));
						cap  = seam ? std::max(cap_of(kt(x, t - 1)), cap_of(kt(x, b + 1))) : -1;
					}
					if (seam) {
						obj_top  = true;
						cap_axis = cap;
						obj_cap  = cap >= 0;
						obj_side = -1;
					}
				}
				// A buried face cannot be seen: what shows of it is the other face's edge (a merlon's tie diagonal).
				if (obj_side >= 0 && sprites != nullptr) {
					// Judged on the art that landed: a box behind holding the pixel exactly is overpainted by a later box's 1px
					// overhang.
					const int oi = wall_owner(x, y, false, false, -1, true);
					if (obj_face_buried(x, y, oi, obj_side) && !obj_face_buried(x, y, oi, 1 - obj_side)) {
						obj_side = 1 - obj_side;
					}
				}
				const int kv = (face_grid && kindrow != nullptr) ? ((obj_top || obj_side >= 0) ? Kind::Object : kindrow[x])
																 : Kind::Ground;
				// The MASK is the authority on "part of the shell": the wall
				// course under a window is stamped Shell_face by the shell branch
				// but classified as an object, so it would skip face_alpha and
				// take the object max instead, hotter than the wall it sits in.
				// Where the two disagree, follow the mask.
				const bool wall_obj = kv == Kind::Object && roofrow != nullptr && Mask::Is_wall(roofrow[x]);
				const bool face_px  = kv == Kind::Wall_face || wall_obj;
				const bool surf_px  = kv == Kind::Wall_face || kv == Kind::Object;
				// A slab lying on a wall is part of that wall (a bare window's sill).
				auto wall_slab = [&]() {
					if (ring == nullptr || roofrow == nullptr || !Mask::Is_slab(roofrow[x]) || footdx == nullptr) {
						return false;
					}
					const int fci = foot_cell(x, y);
					return fci >= 0 && Ring::Is_wall(ring, static_cast<size_t>(fci));
				};
				// An object face lit by a spill needs the light on its normal side: an east face east of the window is backlit.
				// A sill's faces are wall faces, so the room's own light obeys this too.
				auto obj_backlit = [&]() {
					if (obj_side < 0 || !(is_spill || (veto_roof && wall_slab()))) {
						return false;
					}
					if ((obj_side == 0 && back_dy > 0) || (obj_side == 1 && back_dx > 0)) {
						return true;
					}
					if (footdx == nullptr || footdy == nullptr) {
						return false;
					}
					const float ffx = static_cast<float>(foot_x(x, y) - foot_shift);
					const float ffy = static_cast<float>(foot_y(x, y) - foot_shift);
					const float rel = obj_side == 0 ? (ffy - grid_v) * inv_cell : (ffx - grid_u) * inv_cell;
					return rel - static_cast<float>(grid_rt) > -0.5f;
				};
				// A top is one unit, reached if any of its own cells (or those beside it) is: a rampart
				// straddling the roof's edge split dark at the cell row its top crosses onto the apron.
				const int  ti_top    = obj_top && ring != nullptr && sprites != nullptr ? top_owner(x, y) : -1;
				const bool unit_top  = ti_top >= 0 && sprites[ti_top].fz / 4 < 5 * (light_floor_storey + 1);
				auto       top_cells = [&](auto&& fn) {
                    const Sprite_box& s      = sprites[ti_top];
                    const int         half_t = c_tilesize / 2;
                    const int         ecu = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
                    const int         ecv = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
                    for (int v = ecv - s.yts; v <= ecv + 1; ++v) {
                        for (int u = ecu - s.xts; u <= ecu + 1; ++u) {
                            const bool in_u = u > ecu - s.xts && u <= ecu;
                            const bool in_v = v > ecv - s.yts && v <= ecv;
                            if ((!in_u && !in_v) || u < 0 || v < 0 || u >= side || v >= side) {
                                continue;
                            }
                            const int ci = v * side + u;
                            if (!Ring::Is_wall(ring, static_cast<size_t>(ci)) && !apron_cell(ci)) {
                                fn(ci);
                            }
                        }
                    }
				};
				auto unit_reached = [&]() {
					if (!unit_top || grid == nullptr) {
						return object_reached(x, y);
					}
					bool r = false;
					top_cells([&](int ci) {
						r = r || Grid::Dist(grid[static_cast<size_t>(ci)]) != 0;
					});
					return r;
				};
				bool bypass_field   = false;
				bool slab_edge_fade = false;    // A gallery edge over the light's ceiling (see below).

				int forced_a = -1;
				// An exposed top admitted as a facade (outer_face): the bare dome, like the shell tops beside it.
				bool bare_dome_px = false;
				// Whether the wall unit under a top pixel was reached by the fill (the veto top rule below).
				int edge_reached = -1;
				// A light-passing wall piece (Shell_face with Kind::Ground -- window
				// glass, an open shutter's panel; true wall faces carry Wall_face)
				// is a surface every light reaches like clear ground: the lit
				// room shows through it from outside, and a fan or a torch out
				// there lights it.  Gating this on the veto light alone left
				// the same piece dark from outside while it lit from inside.
				const bool pane_px
						= roofrow != nullptr && roofrow[x] == Mask::Shell_face && kindrow != nullptr && kindrow[x] == Kind::Ground;
				// A shell wall's flat TOP: a surface, not a face, so it samples
				// like clear ground and counts arrivals from every side below.
				bool top_px = roofrow != nullptr && roofrow[x] == Mask::Shell_top;
				// Interior faces carry the room side they look onto (0 south, 1
				// east); they are wall art, not a marked whole unit.
				int iside_px = roofrow == nullptr               ? -1
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
				// The same junction for any FACE mark, along EITHER axis: where a shutter's top abuts a wall top
				// the two sprites strand a short run of face between them -- 2px across for a south-facing wall, 3
				// rows down for an east-facing one.  Face marks are vetoed dark while the Shell_top at both ends is
				// exempt, so the run draws a blob at the end of a lit top.  A real face is a wide region and is
				// never closed by TOP at both ends of a run this short.
				// A face seam merged into a top belongs to the top closing it on the viewer's side (below / right).
				int seam_ox = x;
				int seam_oy = y;
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
					if (r - l < 3 && l > x0 && r < x1 && roofrow[l - 1] == Mask::Shell_top && roofrow[r + 1] == Mask::Shell_top) {
						seam    = true;
						seam_ox = r + 1;
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
							seam    = true;
							seam_oy = b + 1;
						}
					}
					if (seam) {
						top_px   = true;
						iside_px = -1;
					}
				}
				// A short Shell_face strip with INTERIOR faces at both ends is the junction of two room-side pieces
				// (a south run meeting an east run), not exterior masonry: it reads the room side of the piece after it.
				if (!top_px && iside_px < 0 && roofrow != nullptr && roofrow[x] == Mask::Shell_face) {
					int l = x;
					while (l > x0 && x - l < 3 && roofrow[l - 1] == Mask::Shell_face) {
						--l;
					}
					int r = x;
					while (r < x1 && r - x < 3 && roofrow[r + 1] == Mask::Shell_face) {
						++r;
					}
					auto room_side = [](int m) {
						return m == Mask::Face_south ? 0 : m == Mask::Face_east ? 1 : -1;
					};
					if (r - l < 3 && l > x0 && r < x1 && room_side(roofrow[l - 1]) >= 0 && room_side(roofrow[r + 1]) >= 0) {
						iside_px = room_side(roofrow[r + 1]);
					} else if (
							r - l < 3 && l > x0 && r < x1 && room_side(roofrow[l - 1]) >= 0 && roofrow[r + 1] == Mask::Shell_top) {
						// Closed by a top instead (a jamb's room face meeting the halfwall's sill): still that face.
						iside_px = room_side(roofrow[l - 1]);
					}
				}
				// The same seam one storey up: a wall standing ON a deck hides
				// all of the deck's top but the column beside its own thickness
				// strip.  That column carries the TOP's mark, which every light
				// gates by storey, so it would draw a dark hairline between the
				// lit wall and the lit edge below it.  One pixel, face on one
				// side and its own strip on the other -- a real deck top is
				// wide.
				if (!top_px && iside_px < 0 && roofrow != nullptr && x > x0 && x < x1
					&& (Mask::Is_tall(roofrow[x]) || Mask::Is_slab(roofrow[x])) && Mask::Is_slab_edge(roofrow[x + 1])
					&& Mask::Is_wall(roofrow[x - 1])) {
					top_px = true;
				}
				// Up to 3px wide where the wall's corner and its outer face strand a little more of it (a hall's
				// gallery corner): a run of deck top / shell face between a room-side face and the strip.
				if (!top_px && iside_px < 0 && roofrow != nullptr && x > x0 && x < x1) {
					auto member = [](int m) {
						return Mask::Is_tall(m) || Mask::Is_slab(m) || m == Mask::Shell_face;
					};
					if (member(roofrow[x])) {
						int l = x;
						while (l > x0 && x - l < 3 && member(roofrow[l - 1])) {
							--l;
						}
						int r = x;
						while (r < x1 && r - x < 3 && member(roofrow[r + 1])) {
							++r;
						}
						const int lm = l > x0 ? roofrow[l - 1] : 0;
						if (r - l < 3 && l > x0 && r < x1 && Mask::Is_slab_edge(roofrow[r + 1]) && Mask::Is_wall(lm)
							&& lm != Mask::Shell_face && (Mask::Is_tall(roofrow[x]) || Mask::Is_slab(roofrow[x]) || r > l)) {
							top_px = true;
						}
					}
				}
				// Interior face under a veto light: one read per face, as the shell face above it gets.
				// A light-passing piece (open shutter) stamps its faces with no kind of its own: it owns them too.
				const bool pane_face
						= kindrow != nullptr && kindrow[x] == Kind::Ground && roofrow != nullptr && Mask::Is_face(roofrow[x]);
				// A face or object standing on a storey this light's deck spills light is theirs (the floor slab's
				// rule, applied to what stands on it).
				if (veto_roof && Splat_deck_spill_storeys != 0 && kindrow != nullptr && sprites != nullptr
					&& (kindrow[x] == Kind::Wall_face || pane_face || Kind::Is_object(kindrow[x]))) {
					const int dob = face_owner(x, y);
					const int dst = dob >= 0 ? sprites[dob].fz / 4 / 5 : -1;
					if (dst > light_floor_storey && dst < 30 && ((Splat_deck_spill_storeys >> dst) & 1) != 0) {
						continue;
					}
				}
				if (veto_roof && iside_px >= 0 && !top_px && kindrow != nullptr
					&& (kindrow[x] == Kind::Wall_face || pane_face
						|| (Kind::Is_object(kindrow[x]) && roofrow != nullptr && Mask::Is_wall(roofrow[x])))
					&& face_data && grid != nullptr && ring != nullptr) {
					// A wall-marked object (a halfwall under a window) is its own face's owner.
					const int own     = kindrow[x] == Kind::Wall_face ? (wall_owner(x, y) >= 0 ? -1 : wall_foot_owner(x, y))
										: pane_face                   ? top_owner(x, y)
																	  : wall_owner(x, y, false);
					face_zero_w       = -1.0f;
					wf_buried         = -1;
					int       fa      = wall_face_alpha(x, y, false, iside_px, own);
					const int face_cu = face_cell_u;
					const int face_cv = face_cell_v;
					// A 1x1 piece (a post, a statue) is one column: its two faces only split the art along the box
					// diagonal, a hard line through a round statue.  Both look onto this room: take the brighter.
					if (kindrow[x] == Kind::Wall_face) {
						const int ob = own >= 0 ? own : wall_owner(x, y);
						if (ob >= 0 && sprites[ob].xts == 1 && sprites[ob].yts == 1) {
							const float zw = face_zero_w;
							const int   fo = wall_face_alpha(x, y, false, 1 - iside_px, ob);
							if (fo > fa) {
								fa = fo;
							} else {
								face_zero_w = zw;
							}
						}
					}
					// An unanswered wall or pane face buried against a low wall (a jamb beside a closed door): the field
					// below would light it instead.
					// The covered sliver belongs to the piece's OTHER face (as the buried seam): the jamb's outer
					// face, or the halfwall's -- dark to a light in the room, lit by one outside.
					if ((kindrow[x] == Kind::Wall_face || pane_face)
						&& face_under_low_wall(x, y, iside_px, own >= 0 ? own : face_owner(x, y))) {
						face_zero_w = -1.0f;
						fa          = std::max(wall_face_alpha(x, y, false, 1 - iside_px, own), 0);
						face_zero_w = 0.0f;    // No field share: the answer is final (fa + zw * field below).
						forced_a    = fa;
					}
					// The exact box behind has this face buried: the pixel is the 1px overhang of a box painted over it.
					if (own < 0 && wf_buried >= 0) {
						const int nb = wall_owner(x, y, true, false, wf_buried);
						if (nb >= 0) {
							const int ndx = sprites[nb].fx + sprites[nb].fz - x;
							const int ndy = sprites[nb].fy + sprites[nb].fz - y;
							if (iside_px == 1 ? ndx <= ndy : ndy <= ndx) {
								face_zero_w = -1.0f;
								fa          = wall_face_alpha(x, y, false, iside_px, nb);
							}
						} else if (fa <= 0) {
							// No box behind: the sliver is the buried box's own wall, not the field beyond (a bright
							// streak down a wall where a pillar stands against it).  Its face as drawn, or its other face
							// -- whichever the room lights: beside a pillar the drawn face still faces it.
							face_zero_w = -1.0f;
							fa          = std::max(wall_face_alpha(x, y, false, iside_px, wf_buried, true), 0);
							face_zero_w = 0.0f;
						}
					}
					// A wholly unanswered read falls through to the field below; its share of a blend must too, or the reach edge
					// dips dark.
					if (fa > 0 || (fa == 0 && face_zero_w >= 0.0f && face_zero_w < 0.999f)) {
						const int ftx = ftmpl != nullptr ? x - sx - ftmpl->x0 : -1;
						const int fld = (trow != nullptr && ftx >= 0 && ftx < ftmpl->w) ? trow[x] : 0;
						forced_a      = fa + static_cast<int>(face_zero_w * static_cast<float>(fld) + 0.5f);
					} else if (fa == 0 && face_cu >= 0 && face_cv >= 0) {
						// Unanswered, but the face looks onto open floor this light never reached (the next room
						// beyond a partition): that is a dark answer.  The field there would shine through the
						// wall.
						int fu = face_cu;
						int fw = face_cv;
						for (int st = 0; st < 4; ++st) {
							fu += iside_px == 1 ? 1 : 0;
							fw += iside_px == 0 ? 1 : 0;
							if (fu >= side || fw >= side) {
								break;
							}
							const size_t ni = static_cast<size_t>(fw) * side + fu;
							if (Ring::Is_wall(ring, ni)) {
								continue;
							}
							if (Grid::Dist(grid[ni]) == 0 && !Ring::Is_apron(ring, ni)) {
								forced_a = 0;
							}
							break;
						}
					}
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
						// A shell face looking onto ground THIS light's fill reaches (a roofed
						// passage it stands in) is lit from its open side.  Read through the
						// open cell in front of the drawn face only: escaped outdoor cells are
						// stripped or apron, so a facade outside the light's building stays
						// dark, which the ring arrivals would not keep.
						int own_face_a = 0;
						if (roofrow[x] == Mask::Shell_face && kindrow != nullptr && kindrow[x] == Kind::Wall_face && face_data
							&& grid != nullptr && ring != nullptr) {
							// A slab's facade strip a storey up is the building's rim: dark to the room below.
							const int st = sprites != nullptr ? strip_over(x, y, wall_owner(x, y)) : -1;
							// The plane rule judges the wall's own face only: a strip drawn over it is the face seen.
							if (st < 0 && !face_behind_light(x, y)) {
								own_face_a = wall_face_alpha(x, y, true);
							} else if (st >= 0 && sprites[st].fz / 4 < 5 * (light_floor_storey + 1)) {
								// The strip drawn over the wall is the face seen: read the slab's own edge.
								own_face_a = wall_face_alpha(x, y, true, -1, st);
							}
						}
						if (own_face_a > 0) {
							forced_a = own_face_a;
						} else if (
								inside_viewer && roofrow[x] == Mask::Tall && kindrow != nullptr && kindrow[x] == Kind::Wall_face) {
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
							// A unit or deck top standing in the fill on the light's own floor storey.
							// A wall FACE whose front is outside the fill (apron: reached only by escaping) faces
							// away from the room.
							const bool face_out = [&] {
								if (kindrow == nullptr || kindrow[x] != Kind::Wall_face || grid == nullptr || ring == nullptr
									|| footdx == nullptr) {
									return false;
								}
								const int fs = wall_face_side(x, y);
								const int fc = foot_cell(x, y);
								if (fs < 0 || fc < 0) {
									return false;
								}
								// A pixel on the box's top surface has no front.
								const int         oi = wall_owner(x, y);
								const Sprite_box& s  = sprites[oi];
								if (std::min(s.fx + s.fz - x, s.fy + s.fz - y) >= s.fz + 4 * Fast_info(s.shape).get_3d_height()) {
									return false;
								}
								int u = fc % side;
								int v = fc / side;
								for (int st = 0; st < 4; ++st) {
									u += fs == 1 ? 1 : 0;
									v += fs == 0 ? 1 : 0;
									if (u >= side || v >= side) {
										return false;
									}
									const size_t ni = static_cast<size_t>(v) * side + u;
									if (Ring::Is_wall(ring, ni)) {
										continue;
									}
									return Ring::Is_apron(ring, ni) || Grid::Dist(grid[ni]) == 0;
								}
								return false;
							}();
							// A unit standing ABOVE the room's ceiling (a crenellation on the roof, stamped with its
							// wall's storey by the cap rule) is not on the light's floor, whatever its mark says.
							const bool above_room = [&] {
								int oi = ti_top >= 0 ? ti_top : top_owner(x, y);
								if (oi < 0) {
									oi = wall_owner(x, y);
								}
								return oi >= 0 && sprites[oi].fz / 4 >= 5 * (light_floor_storey + 1);
							}();
							const bool own_floor = Mask::Is_tall(roofrow[x]) && roofrow[x] - Mask::Tall == light_floor_storey
												   && face_data && grid != nullptr && footdx != nullptr && unit_reached()
												   && !face_out && !above_room;
							// A slab ABOVE the light's floor over a tile outside the fill is not the hall's; over a WALL tile it is
							// that wall's top.
							const bool slab_gnd = slab_s > light_floor_storey && slab_s < light_top_storey && kindrow != nullptr
												  && kindrow[x] == Kind::Ground && face_data && grid != nullptr && ring != nullptr
												  && footdx != nullptr;
							// A storey this light's DECK spills land on is theirs to light: the light reading it through
							// the ground floor's layout under it would draw that layout on the 1st floor.
							// Everything marked on that storey: floor, and the tables and wall shelves standing on it.
							// Not the slab's thickness strip: that edge faces down into the stairwell, the light's own.
							// Nor a top raised from below (the stairwell's top step): it stands in the light's own room.
							const bool deck_spilled = !slab_edge && slab_s > light_floor_storey && slab_s < 30
													  && ((Splat_deck_spill_storeys >> slab_s) & 1) != 0 &&
													  [&] {
														  int oi = wall_foot_owner(x, y);
														  if (oi < 0) {
															  oi = top_owner(x, y);
														  }
														  // Only IN the well (no floor of that storey over the foot's tile): a
														  // ground wall's
														  // top under the floor beside it is the floor's.  Grid::Flag marks holes
														  // in the light's OWN floor only, not the one above it.
														  if (oi < 0 || sprites[oi].fz / 4 >= 5 * slab_s) {
															  return true;
														  }
														  const int ti = foot_tile(x, y);
														  return ti < 0 || ceiling_over(static_cast<size_t>(ti)) < 5 * slab_s + 5;
													  }();
							if (deck_spilled) {
								continue;
							}
							const bool slab_out = slab_gnd && !object_reached(x, y);
							// The thickness strip is no top: it faces down into the stairwell (posts there count as walls).
							if (slab_gnd && !slab_out && !slab_edge && wall_cell(foot_cell(x, y) % side, foot_cell(x, y) / side)) {
								forced_a = face_alpha(x, y, false, true);
							}
							if (!own_floor && (slab_s < 0 || slab_s >= light_top_storey || slab_out)) {
								// Whole-unit marks stay dark under an interior light,
								// OUTDOOR ones included: light that escapes an opening
								// is delivered by the spill fan alone, so a tree or a
								// rampart outside only lights when it stands in a fan.
								// Reading the escaped ring arrivals here would light every exterior unit around a
								// building with any opening, whatever direction it faces.
								// Except the thickness strip of the floor right above the light's ceiling in a tall
								// room (a gallery's edge over the hall): it sits between the lit wall below and the
								// fading wall above  and fades with them instead of a black band.
								if (slab_edge && slab_s == light_top_storey && !slab_out) {
									slab_edge_fade = true;
								} else {
									continue;
								}
							}
						}
					} else if (is_spill) {
						if (roofrow[x] == Mask::Roof) {
							continue;    // A spill never lights a real roof.
						}
						// Tiles the pixel's foot lies along the fan's axis from its landing tile; 1 = unknown.
						auto exit_along = [&]() -> int {
							const int fci = (face_data && grid != nullptr && ring != nullptr) ? foot_cell(x, y) : -1;
							if (fci < 0 || (cone_dx == 0 && cone_dy == 0)) {
								return 1;
							}
							return (fci % side - grid_rt) * cone_dx + (fci / side - grid_rt) * cone_dy;
						};
						if (slab_edge) {
							// A slab's thickness strip is the deck's own edge: sample
							// the field like the deck top beside it, or the fan breaks
							// at the strip and resumes a strip-width further south.
							// Storey-gated like the slab surface it belongs to, so a
							// ground spill does not light the edge of a slab a storey
							// above it.
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
							// under the window would light flat at its own foot value.
							// Kind cannot tell them apart (that course is an object,
							// like a crate on the deck); its FOOT CELL can -- the wall
							// the spill exited is a wall cell in the fan's own grid.
							if (kindrow != nullptr && kindrow[x] != Kind::Ground) {
								if (kindrow[x] == Kind::Wall_face) {
									// The wall's own TOP (its art top rect) is a wall top like a Shell_top: the top read.
									const int oi = sprites != nullptr ? wall_owner(x, y) : -1;
									if (oi >= 0) {
										const int h4 = 4 * Fast_info(sprites[oi].shape).get_3d_height();
										top_px       = x <= sprites[oi].x1 - h4 && y <= sprites[oi].y1 - h4;
									}
									if (!top_px) {
										continue;    // A face: dark under a spill as ever.
									}
								}
								const int sci = (face_data && grid != nullptr && ring != nullptr) ? foot_cell(x, y) : -1;
								// The course's flat TOP on that wall cell (a halfwall under closed shutters) is a wall top.
								if (!top_px && sci >= 0 && wall_cell(sci % side, sci / side) && kindrow[x] == Kind::Object_top) {
									top_px = true;
								}
								if (!top_px && sci >= 0 && wall_cell(sci % side, sci / side)) {
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
							if (exit_along() <= 0) {
								continue;
							}
						} else if (roofrow[x] - Mask::Tall > spill_floor) {
							continue;    // Higher-storey whole-unit marks.
						} else if (
								Mask::Is_tall(roofrow[x]) && kindrow != nullptr && kindrow[x] != Kind::Ground && exit_along() < 0) {
							// A unit on the wall the fan exited (its crenellation), or behind it.  A Ground-kind
							// Tall pixel is a roof top the fan lands on, its foot read at the ground lattice.
							continue;
						}
						if (roofrow[x] <= Mask::Tall) {
							// Ground-storey Tall: a standing deck object or a rampart's
							// crenellation, lit as a whole unit.  A missing kind byte
							// is NOT a reason to veto: the kind mask is owned by the
							// topmost sprite, so a decoration drawn over the merlon
							// clears its kind while the Tall mark stays, and slab
							// strips carry Slab_edge.
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
								// tree is a passable tile, and face_alpha's step back
								// onto an adjacent wall would hand it that wall's
								// arrival, lighting only the half of the crown that
								// leans toward the house.
								const int  uci        = footdx != nullptr ? foot_cell(x, y) : -1;
								const bool ring_solid = ring != nullptr && uci >= 0 && wall_cell(uci % side, uci / side);
								int        forced     = footdx != nullptr ? object_alpha(x, y, 10) : 0;
								// Lit only by the walk over a solid (its cell never reached), with
								// the light arriving mainly from the N or W: the S / E faces we
								// see are averted.  Dominant axis, so a light to the NE of a long
								// E-W rampart still leaves its S face dark.
								const float ldu = uci >= 0 ? uc - static_cast<float>(uci % side) : 0.0f;
								const float ldv = uci >= 0 ? vc - static_cast<float>(uci / side) : 0.0f;
								const bool  sees_lit
										= (ldv > 0.5f && ldv >= std::fabs(ldu)) || (ldu > 0.5f && ldu >= std::fabs(ldv));
								const bool averted
										= !ring_solid && uci >= 0 && Grid::Dist(grid[static_cast<size_t>(uci)]) == 0 && !sees_lit;
								const bool foot_lit = forced > 0;
								const int  walk_a   = forced;
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
									const int  pfx  = foot_x(x, y);
									const int  pfy  = foot_y(x, y);
									int        best = -1;
									int        bd   = 1 << 30;
									const auto rng  = sprites_at(x, y);
									for (const int* p = rng.first; p != rng.second; ++p) {
										const int         i = *p;
										const Sprite_box& s = sprites[i];
										if (s.wall || x < s.x0 || x > s.x1 || y < s.y0 || y > s.y1) {
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
                                                (static_cast<float>(sprites[best].fx - half_t - foot_shift) - grid_u) * inv_cell);
										const int wcv = pos_cell(
												(static_cast<float>(sprites[best].fy - half_t - foot_shift) - grid_v) * inv_cell);
										if (wcu >= 0 && wcv >= 0 && wcu < side && wcv < side && wall_cell(wcu, wcv)) {
											best = -1;
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
											// the light that hits it is the light on the ground it
											// covers, not the trunk tile alone.
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
												const int         scu_ = static_cast<int>(
                                                        std::lround((static_cast<float>(ccx_) - grid_u) * inv_cell));
												const int scv_ = static_cast<int>(
														std::lround((static_cast<float>(ccy_) - grid_v) * inv_cell));
												if (scu_ >= 0 && scv_ >= 0 && scu_ < side && scv_ < side) {
													const int sci_ = scv_ * side + scu_;
													const int ty_  = ccy_ - sy - ftmpl->y0;
													const int tx_  = ccx_ - sx - ftmpl->x0;
													if (Grid::Dist(grid[static_cast<size_t>(sci_)]) != 0 && !apron_cell(sci_)
														&& (ring == nullptr || !wall_cell(scu_, scv_)) && ty_ >= 0 && ty_ < ftmpl->h
														&& tx_ >= 0 && tx_ < ftmpl->w) {
														// No brighter than the trunk's own uncone'd dome: past the
														// pool's reach the art centre would still read the bright
														// mouth.
														const float tdx_  = (static_cast<float>(acu) - uc) * cell;
														const float tdy_  = (static_cast<float>(acv) - vc) * cell;
														const float ttot_ = std::sqrt(tdx_ * tdx_ + tdy_ * tdy_) + bias;
														const float tdom_ = 1.0f - (ttot_ * ttot_ + e2) / rf2;
														const int   tcap_ = tdom_ > 0.0f ? static_cast<int>(amp * tdom_ + 0.5f) : 0;
														const int   ca_   = std::min<int>(
                                                                ftmpl->alpha[static_cast<size_t>(ty_) * ftmpl->w + tx_], tcap_);
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
								// the ground 3-4 tiles down-screen of where a crown really
								// hangs.  A crown over LIT ground takes it (a tree beside
								// the pool glows where it overhangs it); but the
								// template's bilinear fringes half a cell past the fan
								// into unreached cells, and a crown over the house wall
								// beside the window would take the mouth's fringe.
								// So the cell UNDER the pixel on screen must itself be
								// lit GROUND, unless the unit's own cell already is.
								// Ground only: the wall pieces beside the window are
								// reached cells too (their ring arrival), and the crown
								// over them would take it just the same.
								// An ANCHORED unit under a coned fan takes its one value
								// above; the per-pixel overlap would split the crown
								// again.
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
								// One step only: a cap whose own and adjacent tiles are unreached is past the light.
								if (obj_cap) {
									forced = footdx != nullptr ? axis_feather(x, y, cap_axis, 1, object_alpha(x, y, 1)) : 0;
									// A deck field above the cap's storey (a gatehouse roof's spill) never falls on it.
									if (anchor_z > 5 * (roofrow[x] - Mask::Tall + 1)) {
										forced = 0;
									}
								}
								const bool face_off = obj_backlit() || (averted && (foot_lit || !anchored));
								if (obj_backlit()) {
									forced = 0;
								}
								// A block's face is lit only from reached open ground in front of it.
								// Blended along the face with the next cell, so the reach edge fades over a tile.
								if (obj_side >= 0 && !face_off && footdx != nullptr) {
									const int fc = foot_cell(x, y);
									if (fc >= 0) {
										const int fu    = fc % side + (obj_side == 1 ? 1 : 0);
										const int fcv   = fc / side + (obj_side == 0 ? 1 : 0);
										auto      front = [&](int u, int v) -> float {
                                            return u >= 0 && v >= 0 && u < side && v < side && !wall_cell(u, v)
                                                                   && Grid::Dist(grid[static_cast<size_t>(v) * side + u]) != 0
																? 1.0f
																: 0.0f;
										};
										const int ffx = foot_x(x, y) - foot_shift;
										const int ffy = foot_y(x, y) - foot_shift;
										// Along the face: v for an east face, u for a south face.
										const float off
												= obj_side == 1 ? pos_off((static_cast<float>(ffy) - grid_v) * inv_cell, fc / side)
																: pos_off((static_cast<float>(ffx) - grid_u) * inv_cell, fc % side);
										const float aoff = std::min(std::fabs(off), 0.5f);
										const int   step = off >= 0.0f ? 1 : -1;
										forced           = axis_feather(x, y, obj_side == 1 ? 1 : 0, 10, forced);
										const float r0   = front(fu, fcv);
										const float r1   = obj_side == 1 ? front(fu, fcv + step) : front(fu + step, fcv);
										const float w    = r0 * (1.0f - aoff) + r1 * aoff;
										forced           = static_cast<int>(static_cast<float>(forced) * w + 0.5f);
									}
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
							// Except an upper storey's OUTER face over open ground this light reaches (a halfwall
							// in a window gap, seen from inside): the outside view lights it whole, as a facade.
							const bool outer_face
									= slab_s > light_floor_storey && face_data && grid != nullptr && ring != nullptr
									  && kindrow != nullptr &&
									  [&] {
										  int fs = obj_side;
										  // A TOP on the building's edge (a halfwall in a window gap) is exposed like
										  // the shell tops beside it: its wall cell borders open ground this light reaches.
										  if (fs < 0 && (kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]) || top_px)) {
											  // Judged over the OWNER's footprint: a top's pixel feet straddle its tile's
											  // edge, and half a halfwall top tested the room tile behind it.
											  std::vector<int> tcs;
											  int              oi = top_owner(x, y);
											  if (oi < 0) {
												  oi = wall_foot_owner(x, y);
											  }
											  if (oi >= 0) {
												  const Sprite_box& s      = sprites[oi];
												  const int         half_t = c_tilesize / 2;
												  const int         ecu    = pos_cell(
                                                          (static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u)
                                                          * inv_cell);
												  const int ecv = pos_cell(
														  (static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v)
														  * inv_cell);
												  // The foot clamped into the footprint: across a 1-wide run the straddle is
												  // folded back, along it each tile answers for itself (a shape over the
												  // middle of a long halfwall darkens only the middle).
												  const int fc = foot_cell(x, y);
												  const int fu = fc >= 0 ? fc % side : ecu;
												  const int fv = fc >= 0 ? fc / side : ecv;
												  const int u  = std::clamp(fu, ecu - s.xts + 1, ecu);
												  const int v  = std::clamp(fv, ecv - s.yts + 1, ecv);
												  if (u >= 0 && v >= 0 && u < side && v < side) {
													  tcs.push_back(v * side + u);
												  }
											  } else if (const int tc = foot_cell(x, y); tc >= 0) {
												  tcs.push_back(tc);
											  }
											  const int tlo = oi >= 0 ? (sprites[oi].fz / 4 / 5) * 5 : 5 * (light_floor_storey + 1);
											  for (const int tc : tcs) {
												  if (opens_to_light(tc, tlo)) {
													  return true;
												  }
											  }
											  return false;
										  }
										  if (fs < 0 && kindrow[x] == Kind::Wall_face && !top_px) {
											  fs = wall_face_side(x, y);
											  // On the box's top surface there is no face (a slab mark hides the top kind).
											  const int oi = fs >= 0 ? wall_owner(x, y) : -1;
											  if (oi >= 0) {
												  const Sprite_box& s = sprites[oi];
												  if (std::min(s.fx + s.fz - x, s.fy + s.fz - y)
													  >= s.fz + 4 * Fast_info(s.shape).get_3d_height()) {
													  return false;
												  }
											  }
										  }
										  const int fc = fs >= 0 ? foot_cell(x, y) : -1;
										  if (fc < 0) {
											  return false;
										  }
										  // The very next cell: a face fronting another wall (an inner piece
										  // against the outer wall) is buried, not an outer face.
										  const int u = fc % side + (fs == 1 ? 1 : 0);
										  const int v = fc / side + (fs == 0 ? 1 : 0);
										  if (u >= side || v >= side) {
											  return false;
										  }
										  const size_t ni = static_cast<size_t>(v) * side + u;
										  return !Ring::Is_wall(ring, ni) && Grid::Dist(grid[ni]) != 0 && ceiling_over(ni) == 99;
									  }();
							if (outer_face) {
								bypass_field = true;
								bare_dome_px
										= obj_side < 0 && (kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]) || top_px);
							} else if (slab_s > light_floor_storey) {
								continue;
							} else {
								bypass_field = false;
							}
						} else {
							bypass_field = true;
						}
					}
				} else {
					// An ELEVATED spill (a stairwell bubble on an upper floor;
					// light_top_storey carries its render storey): clear pixels
					// are ground-level surfaces -- the walls and terrain BELOW
					// it, over which its z-blind field cells hang up-screen.
					// Lighting them shows as a bright beam under the storey;
					// everything the bubble may light up there is marked Tall +
					// storey.  A GROUND spill paints its fan for any viewer:
					// the shell that would catch its z-blind wash from inside
					// is stamped Shell_face (faces_exterior).
					if (roofrow && is_spill && light_top_storey >= 1) {
						// Except an interior face, pane or top of a wall standing on the spill's own floor: its room's wall.
						bool own_face = false;
						if ((iside_px >= 0 || pane_px || top_px) && sprites != nullptr) {
							// A halfwall under a window is a solid, not a wall sprite.
							// The box painted last is the one seen: a window over a lower storey's block.
							// The owner whose art is at the pixel and whose foot stands under it first: a storey-2 box
							// projected over a 1st-floor shelf may be painted later, but is not seen there.
							// A pane is its own light-passing piece (bars over the block a storey below).
							int oi = pane_px ? art_owner(x, y, true) : -1;
							// A top is its own piece's (the sill under the bars), not the block of the storey below.
							// By the art drawn there (painted last): top_owner would hand the sill's last column to the
							// block of the storey below, whose footprint holds that column's foot.
							if (oi < 0 && top_px) {
								oi = art_owner(x, y);
								if (oi < 0) {
									oi = top_owner(x, y);
								}
							}
							if (oi < 0) {
								oi = wall_foot_owner(x, y);
							}
							if (oi < 0) {
								oi = std::max(wall_owner(x, y), wall_owner(x, y, false));
							}
							auto on_floor = [&](int o) {
								if (o < 0) {
									return false;
								}
								const int fl = sprites[o].fz / 4;
								const int ft = fl + Fast_info(sprites[o].shape).get_3d_height();
								return (fl < 5 * (spill_floor + 1) && ft > 5 * spill_floor)
									   || (spill_floor >= 1 && ft == 5 * spill_floor && mountain_side(o));
							};
							own_face = on_floor(oi);
							// The art drawn there may still be the floor's piece where the foot lookup found the course
							// of the storey below (a gallery's halfwall face over the hall's wall): either qualifies.
							if (!own_face) {
								own_face = on_floor(art_owner(x, y));
							}
						}
						// Rock risen to the plateau the spill lands on (beside a mountain top, its top at the spill's floor) is
						// that floor, though its art is stamped clear like the ground below.
						if (!own_face && sprites != nullptr && spill_floor >= 1) {
							int mo = art_owner(x, y);
							if (mo < 0) {
								mo = wall_owner(x, y, false);
							}
							own_face = mountain_deck()
									   || (mo >= 0
										   && sprites[mo].fz / 4 + Fast_info(sprites[mo].shape).get_3d_height() == 5 * spill_floor
										   && mountain_side(mo));
						}
						if (!own_face) {
							continue;
						}
					}
				}
				// The shell is exterior in every view: an outdoor lamp gives it the dome from inside as well
				// as from outside.  Interior faces and everything else keep the grid, so an outdoor light
				// still cannot flood the room.  Shell_top also crowns interior walls: it takes the dome only
				// where the fill reached its wall, or an outdoor light lights a room's tops.  A whole-unit
				// exterior piece (Tall mark: a rampart's beams) takes the dome too: the outdoor light's fill
				// would give its wall-face pixels the per-face read, dark from outside.
				const bool shell_dome = !veto_roof && !is_spill && roofrow != nullptr
										&& ((Mask::Is_shell(roofrow[x])
											 // A pane's top (a closed door's thin top edge) is facade too: its threshold
											 // tile is no wall unit the fill can reach.
											 && !(roofrow[x] == Mask::Shell_top && face_grid && !wall_unit_reached(x, y)
												  && !(kindrow != nullptr && kindrow[x] == Kind::Ground)))
											|| Mask::Is_tall(roofrow[x]));
				if (shell_dome) {
					bypass_field = true;
				}
				// Architecture out in the open (steps up onto a wall walk), seen from outside: an outdoor light washes
				// it like the wall it is built against, not by its fill's reach.
				if (!shell_dome && !veto_roof && !is_spill && !inside_viewer && roofrow != nullptr && roofrow[x] == Mask::Clear
					&& kindrow != nullptr
					&& (Kind::Is_object(kindrow[x]) || kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]))) {
					// Architecture: the building class, or anything fixed in place (no weight -- the steps' class is
					// has_hp).
					const int ao = top_owner(x, y);
					if (ao >= 0 && (sprites[ao].building || Fast_info(sprites[ao].shape).get_weight() == 0)) {
						bypass_field = true;
					}
				}
				// An upper storey's jamb face looking into an exposed window gap (its front cell is the gap's
				// lower course, bordering open ground this outdoor light reaches): lit like the halfwall top in it.
				if (forced_a < 0 && !veto_roof && !is_spill && iside_px >= 0 && face_data && grid != nullptr && ring != nullptr
					&& sprites != nullptr) {
					const int oi = wall_owner(x, y);
					const int fc = oi >= 0 && sprites[oi].fz / 4 >= 5 * (light_floor_storey + 1) ? foot_cell(x, y) : -1;
					if (fc >= 0) {
						const int  u0  = fc % side + (iside_px == 1 ? 1 : 0);
						const int  v0  = fc / side + (iside_px == 0 ? 1 : 0);
						const bool gap = u0 < side && v0 < side && opens_to_light(v0 * side + u0, (sprites[oi].fz / 4 / 5) * 5);
						if (gap) {
							const int   gdx   = x - sx;
							const float dist2 = static_cast<float>(gdx) * static_cast<float>(gdx) + dy2;
							int         ga    = 0;
							if (dist2 <= rr) {
								const float dome = dome_at(bias > 0.0f ? std::sqrt(dist2) + bias : std::sqrt(dist2));
								ga               = dome > 0.0f ? static_cast<int>(amp * dome + 0.5f) : 0;
							}
							forced_a = ga;
						}
					}
				}
				int a;
				// The propagated-field branch produced `a` (the slab-edge rule below reads only that one).
				bool field_path = false;
				if (forced_a >= 0) {
					a = forced_a;
				} else if (pane_px && face_data && grid != nullptr) {
					a = pane_alpha(x, y);
					// A pane takes the light falling on it, like any surface (an open shutter is as bright as the
					// floor in front of it).  A closed DOOR does not glow from a light on its far side, whatever
					// passes its cracks or bars: it is the facade's leaf, lit from its own side like the wall around it.
					if (a > 0 && sprites != nullptr && (veto_roof || is_spill)) {
						// The pane's own piece: the art drawn there (an open shutter over a halfwall is the shutter).
						int ob = art_owner(x, y, true);
						if (ob < 0) {
							ob = art_owner(x, y);
						}
						if (ob < 0) {
							ob = top_owner(x, y);
						}
						if (ob < 0) {
							ob = wall_owner(x, y, false);
						}
						// Nor from the spill it lets out (or in): that is still the light from its far side.
						if (ob >= 0 && Fast_info(sprites[ob].shape).is_door()) {
							a = 0;
						}
					}
				} else if (face_px && !shell_dome) {
					// Shell = certified exterior masonry: whole-unit, all-side
					// arrivals; an interior face reads its own room side only.
					a = face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, iside_px);
					// A spill's face looking into a passage its fan only entered through that passage's mouth.
					if (a > 0 && is_spill && !top_px && ring != nullptr && grid != nullptr) {
						const int mob = face_owner(x, y);
						const int mfs = iside_px >= 0 ? iside_px : obj_side >= 0 ? obj_side : owner_face_side(x, y, mob);
						int       mci = mob >= 0 ? owner_cell(mob) : foot_cell(x, y);
						if (mci >= 0 && mfs >= 0) {
							int u = mci % side;
							int v = mci / side;
							for (int st = 0; st < 4; ++st) {
								u += mfs == 1 ? 1 : 0;
								v += mfs == 0 ? 1 : 0;
								if (u >= side || v >= side) {
									break;
								}
								const size_t ni = static_cast<size_t>(v) * side + u;
								if (Ring::Is_wall(ring, ni)) {
									continue;
								}
								if (Ring::Has(ring, ni, Ring::Mouthed_flag)) {
									a = 0;
								}
								break;
							}
						}
					}
					if (a > 0 && !top_px) {
						const int lo_ob = face_owner(x, y);
						const int lo_fs = iside_px >= 0 ? iside_px : owner_face_side(x, y, lo_ob);
						if (face_under_low_wall(x, y, lo_fs, lo_ob)) {
							const int other = 1 - lo_fs;
							// That face's own normal decides a fan: one pointing along the cone is never hit.
							const bool other_backlit = is_spill && ((other == 0 && cone_dy > 0) || (other == 1 && cone_dx > 0));
							a                        = lo_fs >= 0 && !other_backlit
															   ? face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, other)
															   : 0;
						}
					}
					// A face looking straight into the next wall piece is buried: the sliver its art shows at the
					// seam between two pieces belongs to the other face of the run, not to a side nothing reaches.
					// Any read there takes the brighter of the two (a blend off the buried side would come out
					// half-dark).
					if (iside_px >= 0 && face_data && grid != nullptr && ring != nullptr) {
						const int fci = foot_cell(x, y);
						if (fci >= 0) {
							const int nu = fci % side + (iside_px == 1 ? 1 : 0);
							const int nv = fci / side + (iside_px == 0 ? 1 : 0);
							if (nu < side && nv < side && Ring::Is_wall(ring, static_cast<size_t>(nv) * side + nu)) {
								a = std::max(
										a,
										face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, 1 - iside_px));
							}
						}
					}
					// Every fan ray travels with the cone, so a face whose normal points along it is never hit.
					const int fside = iside_px >= 0 ? iside_px : obj_side;
					if (is_spill && ((fside == 0 && back_dy > 0) || (fside == 1 && back_dx > 0) || obj_backlit())) {
						a = 0;
					}
					// Same arrival-only blind spot the wall-top read documents below: a
					// sill spans its window's own open fill cells, which carry
					// no arrival, so the TOP read is 0 right across the opening.
					// That branch is veto-only, so under a SPILL this one owns the
					// sill, which would otherwise render black against its lit wall.
					// Shell marks only (iside < 0): an interior face must keep
					// reading its own room's side.
					if (a <= 0 && top_px && iside_px < 0) {
						a = face_alpha(x, y, true);
					}
					if (top_px && face_data && grid != nullptr && ring != nullptr && wall_unit_reached(x, y)) {
						a = std::max(a, top_mass_alpha(x, y));
					}
					// A spill onto a deck above its own storey lights that deck, not the shell face beneath it.
					// Nor any shell the viewer sees from below that deck: every visible face stands under its floor.
					// Except a face looking INTO the spill's own roofed room (a window's jamb on that storey): the fill
					// reached the cell in front of it under a ceiling, so it is room-side, not facade.
					if (is_spill && roofrow != nullptr && roofrow[x] == Mask::Shell_face && iside_px < 0 && !top_px
						&& (anchor_z > 5 * light_top_storey
							|| (light_floor_storey >= 1 && Viewer_ceiling_lift() <= 5 * light_floor_storey))
						&& ![&] {
							   // A wall-face pixel knows its own side and foot; an object-kind one (a halfwall stamped as
							   // an object) carries the foot of whatever wall stands behind it, so ask its own box.
							   if (grid == nullptr || ring == nullptr) {
								   return false;
							   }
							   auto room_front = [&](int fci, int fs) {
								   if (fci < 0 || fs < 0) {
									   return false;
								   }
								   const int u = fci % side + (fs == 1 ? 1 : 0);
								   const int v = fci / side + (fs == 0 ? 1 : 0);
								   if (u >= side || v >= side) {
									   return false;
								   }
								   const size_t ni = static_cast<size_t>(v) * side + u;
								   return !Ring::Is_wall(ring, ni) && Grid::Dist(grid[ni]) != 0 && ceiling_over(ni) != 99;
							   };
							   const bool obj_k = Kind::Is_object(kv) || kv == Kind::Object_top || Kind::Is_cap(kv);
							   const int  ob    = face_owner(x, y);
							   // The owner's own geometry decides which face it is (by the pixel's foot, a jamb's outer
							   // face near its corner would read as its south face); a wall-face pixel's foot must
							   // agree too.
							   const bool own_room = room_front(owner_cell(ob), owner_face_side(x, y, ob));
							   return obj_k ? own_room : own_room && room_front(foot_cell(x, y), wall_face_side(x, y));
						   }()) {
						a = 0;
					}
					// Arrivals along a wall line are CONSTANT -- 4 across the
					// whole lit band, then 0 -- so a fan landing on a facade
					// would render as a flat slab ending in a cliff, while the
					// same light entering a window is soft because the field
					// carries the fan's distance.  A facade has no such field
					// (the fill stops at it), so take the fan's GEOMETRY
					// instead: the radial dome for the gradient, cone_fade for
					// the taper, and the fill's own `perp <= along` support so
					// it stops at the cone edge rather than washing round the
					// corner.
					if (is_spill && iside_px < 0 && (cone_dx != 0 || cone_dy != 0) && face_data && grid != nullptr
						&& ring != nullptr && roofrow != nullptr && Mask::Is_shell(roofrow[x])) {
						const int fci = foot_cell(x, y);
						if (fci >= 0 && a > 0) {
							// Sub-tile, from the pixel's own foot: a per-CELL perp
							// is an integer, so the taper would land in flat
							// diagonal bands across the wall.  And the fan is only
							// `along + 1` tiles wide, which 2 tiles out is 3 --
							// narrower than a fixed 3-tile penumbra, so a constant
							// width would ramp the whole facade instead of its
							// edge.
							const float ffx    = static_cast<float>(foot_x(x, y) - foot_shift);
							const float ffy    = static_cast<float>(foot_y(x, y) - foot_shift);
							const float frelx  = (ffx - grid_u) * inv_cell - static_cast<float>(grid_rt);
							const float frely  = (ffy - grid_v) * inv_cell - static_cast<float>(grid_rt);
							const float along  = frelx * static_cast<float>(cone_dx) + frely * static_cast<float>(cone_dy);
							const float perp   = Cone_perp_f(frelx, frely, cone_dx, cone_dy, cone_mouth);
							const float half_w = 0.5f * (along + 2.0f);
							const float band   = half_w < 1.0f ? 1.0f : half_w;
							float       taper  = perp <= 0.0f ? 1.0f : (along + 1.0f - perp) / band;
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
									const float dome = free_dome(dist2);
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
					// A top is its piece's own tiles' surface: band feet round onto the tile in front (a sill's lower
					// rows read the room behind the window, dark to a lamp outside).  A sill only -- a thin run: over
					// a big block (a mountain top) the brightest tile would light the whole top flat.
					// Only where the foot read found nothing (the sill's rows over the dark room): the owner lookup is
					// costly per pixel and per light, and a lit top needs no help.
					if (a <= 0 && !is_spill && kindrow != nullptr && (kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]))
						&& sprites != nullptr) {
						const int tob = top_owner(x, y);
						if (tob >= 0 && std::min(sprites[tob].xts, sprites[tob].yts) == 1) {
							int su, sv;
							box_cell(sprites[tob], su, sv);
							for (int v = sv - sprites[tob].yts + 1; v <= sv; ++v) {
								for (int u = su - sprites[tob].xts + 1; u <= su; ++u) {
									if (u >= 0 && v >= 0 && u < side && v < side) {
										a = std::max(a, object_cell_alpha(v * side + u, 0));
									}
								}
							}
						}
					}
					if (is_spill && obj_side < 0) {
						const int ua = unit_alpha(x, y);
						if (ua > a) {
							a = ua;
						}
					}
					if (obj_backlit()) {
						a = 0;
					}
					// A top on a storey above the light whose footprint the fill reached only as OPEN floor beneath it
					// (a mountain block roofing a passage under a wall): the light is under the piece -- its top is
					// the roof over the light, seen from above.  A piece standing on its own column (a stacked hall
					// pillar) has wall cells there, and a stairwell's cells keep their own rule.
					if (a > 0 && kindrow != nullptr && (kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]))
						&& sprites != nullptr && grid != nullptr && roofed_over_top(x, y)) {
						a = 0;
					}
				} else if (
						veto_roof && face_data && grid != nullptr && ring != nullptr && roofrow != nullptr
						&& (top_px
							|| (Mask::Is_slab(roofrow[x]) && kindrow != nullptr
								&& (kindrow[x] == Kind::Wall_face
									// A slab's top lying on a wall is that wall's top.
									|| ((kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x])) && wall_slab()))))) {
					// A wall TOP under a veto light has only the z-blind field,
					// which at an upper wall's height maps to cells NORTH of the
					// wall: a window sill would fade out down its own sprite.
					// Its foot carries the room's arrival instead.  The BRIGHTER
					// of the two, never just the foot read -- a ground wall's
					// top is lit correctly by the field already, and replacing
					// it outright would darken those into a stripe.
					a = face_alpha(x, y, true, true, -1);
					// The top read only consults ring ARRIVALS.  A sill spans its
					// window's own tiles, which are open fill cells with no
					// arrival at all, so it returns 0 across the opening and
					// lights only the two ends where it meets solid wall.  The
					// all-sides read takes the fill value there -- the same one
					// the glass beside it shows.
					if (a <= 0) {
						a = face_alpha(x, y, true);
					}
					// A wall face, unlike a top, is dark from behind its plane: an outer joint face above a parapet.
					if (!top_px && kindrow[x] == Kind::Wall_face && face_behind_light(x, y)) {
						continue;
					}
					// Thick block tops sit on interior cells the fill never enters: per-column stripes otherwise.
					{
						const int fci = foot_cell(x, y);
						if (fci >= 0) {
							const int ma = top_mass_alpha(x, y);
							// The walk only spreads a reached top across its own mass: a wall the fill never
							// touched (the far side of a closed passage's wall) found the light through the dark room behind it.
							if (ma > a && wall_unit_reached(x, y)) {
								a = ma;
							}
						}
					}
					// A sill one tile thin (the halfwall top under a gap whose window shape is gone): its band feet round
					// onto the tile beyond the wall, where the ring has no arrival -- a diagonal cut across the top.  Its
					// own tiles are the gap the room's fill passes: take the brightest of them, as the object path does
					// for a sill.
					if (a <= 0 && top_px && !is_spill && sprites != nullptr) {
						const int tob = top_owner(x, y);
						if (tob >= 0 && std::min(sprites[tob].xts, sprites[tob].yts) == 1) {
							int su, sv;
							box_cell(sprites[tob], su, sv);
							for (int v = sv - sprites[tob].yts + 1; v <= sv; ++v) {
								for (int u = su - sprites[tob].xts + 1; u <= su; ++u) {
									if (u >= 0 && v >= 0 && u < side && v < side) {
										a = std::max(a, object_cell_alpha(v * side + u, 0));
									}
								}
							}
						}
					}
					if (!above_storey(x, y, roofrow[x]) && ftmpl != nullptr && trow != nullptr) {
						const int tx = x - sx - ftmpl->x0;
						const int fv = (tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
						if (fv > a) {
							a = fv;
						}
					}
					// A wall top is lit only by a light that reached its wall (the Shell_top rule).
					edge_reached = static_cast<int>(wall_unit_reached(x, y, false));
					// A wall-marked object crowning the pixel (a sill): its top is seen only from its own cells or those beside it.
					// So is a wall sprite standing on no ring wall cell (a mirror against the wall): it is furniture.
					const int toi_o = top_owner(seam_ox, seam_oy);
					// A wall box painted later over the pixel hides an earlier one's top: an upper wall's top art overhangs
					// its box's top plane by a pixel or two, which the plane test alone would hand to the ground wall
					// below.
					const int toi_w = std::max(wall_owner(seam_ox, seam_oy, true, true), wall_owner(seam_ox, seam_oy));
					const int toi   = toi_o >= 0 && toi_o > toi_w ? toi_o : toi_w;
					if (edge_reached != 0 && toi >= 0) {
						const Sprite_box& s      = sprites[toi];
						const int         half_t = c_tilesize / 2;
						const int ecu  = pos_cell((static_cast<float>(s.fx + s.fz - half_t - foot_shift) - grid_u) * inv_cell);
						const int ecv  = pos_cell((static_cast<float>(s.fy + s.fz - half_t - foot_shift) - grid_v) * inv_cell);
						bool      seen = false;
						for (int v = ecv - s.yts + 1; v <= ecv && toi == toi_w && !seen; ++v) {
							for (int u = ecu - s.xts + 1; u <= ecu && !seen; ++u) {
								seen = u < 0 || v < 0 || u >= side || v >= side
									   || Ring::Is_wall(ring, static_cast<size_t>(v) * side + u);
							}
						}
						for (int v = ecv - s.yts; v <= ecv + 1 && !seen; ++v) {
							for (int u = ecu - s.xts; u <= ecu + 1 && !seen; ++u) {
								const bool in_u = u > ecu - s.xts && u <= ecu;
								const bool in_v = v > ecv - s.yts && v <= ecv;
								if ((!in_u && !in_v) || u < 0 || v < 0 || u >= side || v >= side) {
									continue;
								}
								const size_t ci = static_cast<size_t>(v) * side + u;
								seen = !Ring::Is_wall(ring, ci) && !apron_cell(static_cast<int>(ci)) && Grid::Dist(grid[ci]) != 0;
							}
						}
						if (!seen) {
							edge_reached = 0;
						}
					}
					if (edge_reached == 0) {
						a = 0;
					}
				} else if (
						veto_roof && face_data && grid != nullptr && kindrow != nullptr && (Kind::Is_object(kindrow[x]) || obj_top)
						&& !bypass_field) {
					// A veto splat disables face_grid, so kind is never consulted
					// above: an OBJECT standing outside (a fence beside a lit
					// window) has its sprite hanging over the room's lit cells and
					// would drink that field straight through the wall.  Being in
					// the fill at all decides WHETHER it is lit; the field still
					// decides how bright, or furniture bands into flat per-cell
					// blocks instead of taking the room's smooth wash.
					// A Shell_top is not such an object: the mask already
					// certified it as the building's own masonry, and a sill under
					// an OPEN shutter sits on an escaped (apron) cell, which this
					// gate would read as "not lit".
					// Nor is any other SHELL mark: a wall course under a window is
					// stamped a wall mark by the shell branch yet classified an
					// object, and outside a veto light `wall_obj` already lets the
					// mask overrule the kind.  It cannot here -- a veto forces kv
					// to Ground -- so the half wall under a window would be judged
					// a loose object and fail the fill test.
					if (roofrow != nullptr && Mask::Is_wall(roofrow[x])) {
						a = face_alpha(x, y, Mask::Is_shell(roofrow[x]), top_px, iside_px);
						// The ring read runs ~70 below the SAME face's Wall_face
						// pixels, which take the field, so one wall split into two
						// brightnesses along the kind boundary.  Only tops and
						// INTERIOR faces reach here (Shell_face is cut above),
						// which the room is meant to light.  Tops keep the
						// arrival-only read -- that is the window-sill fix.
						if (!top_px && ftmpl != nullptr && trow != nullptr && !above_storey(x, y, roofrow[x])) {
							const int tx = x - sx - ftmpl->x0;
							const int fv = (tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
							if (fv > a) {
								a = fv;
							}
						}
					} else {
						// A solid on its own wall cell (a stair step) carries no fill: its face is reached when the fill
						// touched that cell on the face's own side.
						// Only a piece standing on the light's own floor: a pillar stacked a storey up is not that cell's.
						const bool own_side_reached = [&] {
							const int ob = wall_owner(x, y, false);
							if (ob < 0 || sprites[ob].fz / 4 >= 5 * (light_floor_storey + 1)) {
								return false;
							}
							const int fc = obj_side >= 0 && ring != nullptr && footdx != nullptr ? foot_cell(x, y) : -1;
							return fc >= 0 && Ring::Is_wall(ring, static_cast<size_t>(fc)) && !escaped_only(fc)
								   && Ring::Dist(ring[static_cast<size_t>(fc) * Ring::Bytes + (obj_side == 0 ? Ring::S : Ring::E)])
											  != 0;
						}();
						if (!top_px
							&& ((!unit_reached() && !own_side_reached) || obj_backlit()
								|| obj_face_buried(x, y, wall_owner(x, y, false), obj_side))) {
							continue;
						}
						// A face is lit only from the room's floor in front of it (a halfwall over an open door faces the street).
						if (obj_side >= 0 && !top_px && ring != nullptr && footdx != nullptr) {
							const int fc = foot_cell(x, y);
							if (fc >= 0) {
								const int fu = fc % side + (obj_side == 1 ? 1 : 0);
								const int fw = fc / side + (obj_side == 0 ? 1 : 0);
								if (fu < side && fw < side
									&& (wall_cell(fu, fw) || Ring::Is_apron(ring, static_cast<size_t>(fw) * side + fu)
										|| Grid::Dist(grid[static_cast<size_t>(fw) * side + fu]) == 0)
									&& !lower_lit_front(fu, fw, wall_owner(x, y, false))) {
									continue;
								}
							}
						}
						const int tx = x - sx - ftmpl->x0;
						// The lattice is the light's storey: an object wholly below its floor has no foot cell on it.
						const int  oi = wall_owner(x, y, false);
						const bool below
								= oi >= 0
								  && sprites[oi].fz / 4 + Fast_info(sprites[oi].shape).get_3d_height() <= 5 * light_floor_storey;
						a = foot_reads && !below && (roofrow == nullptr || roofrow[x] == Mask::Clear) ? foot_field(x, y)
							: (trow != nullptr && tx >= 0 && tx < ftmpl->w)                           ? trow[x]
																									  : 0;
						// Its foot on its own wall cell reads no floor: take that cell's arrival on the face's side.
						if (!top_px && own_side_reached) {
							const int fc = foot_cell(x, y);
							a            = std::max(
                                    a, cell_dome(
                                               fc, Ring::Dist(
                                                           ring[static_cast<size_t>(fc) * Ring::Bytes
                                                                + (obj_side == 0 ? Ring::S : Ring::E)])));
						}
						// A top standing on its own WALL cell (a stair step's cap) is a wall top: the field reads 0
						// there and blended a dark spot in; take the wall's arrival like the shell tops.
						if (obj_top && ring != nullptr && footdx != nullptr) {
							const int tc = foot_cell(x, y);
							if (tc >= 0 && Ring::Is_wall(ring, static_cast<size_t>(tc))) {
								int ta = face_alpha(x, y, true, true, -1);
								if (ta <= 0) {
									ta = face_alpha(x, y, true);
								}
								a = std::max(a, ta);
							}
						}
						// The top of a piece the fill reached (a window sill in the room) faces up into the room: lit by its
						// owner's arrivals.  The z-blind field at its screen position lies over the holes of a block floor (a
						// tower's) and reads 0 across half of it.
						// So does a clear top whose foot lands on a wall or beyond it (a top stair step against the wall).
						const bool foot_lost = [&] {
							const int fc = footdx != nullptr ? foot_cell(x, y) : -1;
							return fc >= 0 && ring != nullptr
								   && (Ring::Is_wall(ring, static_cast<size_t>(fc))
									   || Grid::Dist(grid[static_cast<size_t>(fc)]) == 0);
						}();
						if (obj_top && roofrow != nullptr && (roofrow[x] != Mask::Clear || foot_lost) && unit_reached()) {
							a = std::max(a, object_alpha(x, y, 4));
						}
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
					field_path   = true;
					// Only bare ground reads at its foot: a wall's foot (a door's) is its own unreached cell.
					// So does a floor slab on the light's own storey whose foot is stamped: the z-blind read assumes the anchor
					// plane. A slab top raised from the storey below (a stairwell's top step) has no cell on this lattice, like the
					// steps beneath it: it takes the field, not the detour its foot cell got.
					const bool slab_from_below = [&] {
						if (is_spill || light_floor_storey < 1 || roofrow == nullptr || !Mask::Is_slab(roofrow[x])) {
							return false;
						}
						// The owner must stand under the pixel's own foot (a floor over a wall beside it is not that wall's top).
						int oi = wall_foot_owner(x, y);
						if (oi < 0) {
							oi = top_owner(x, y);
						}
						// A flat item lying on that top (straw, a rug) is not what the floor is: its pixels belong to
						// the top under it, or the item's outline would show as a blob reading the other way.
						const bool flat_item = oi >= 0 && sprites[oi].fz / 4 == 5 * light_floor_storey
											   && Fast_info(sprites[oi].shape).get_3d_height() <= 1 && !sprites[oi].building
											   && !Fast_info(sprites[oi].shape).is_floor(sprites[oi].frame);
						if (!flat_item
							&& (oi < 0 || sprites[oi].fz / 4 >= 5 * light_floor_storey
								|| sprites[oi].fz / 4 + Fast_info(sprites[oi].shape).get_3d_height() > 5 * light_floor_storey)) {
							return false;
						}
						// Only IN the well.  A ground block under ordinary floor -- even right beside the well -- is
						// that floor's support, and the z-blind field there carried the ground storey's shadows up.
						if (foot_in_well(x, y)) {
							return true;
						}
						// A doorway's threshold in a block floor (a tower's): the wall top under the door is no floor
						// tile of its own, and read apart from the block floor it opens onto it would draw a dark line
						// along the door.  Its foot's CELL decides.  Not a post beside a stairwell: no door stands on
						// it.
						const int fc = foot_cell(x, y);
						if (fc < 0 || (grid[static_cast<size_t>(fc)] & Grid::Flag) == 0) {
							return false;
						}
						const int pfx = foot_x(x, y);
						const int pfy = foot_y(x, y);
						for (int i = 0; i < nsprites; ++i) {
							const Sprite_box& s = sprites[i];
							if (s.fz / 4 != 5 * light_floor_storey || !Fast_info(s.shape).is_door()) {
								continue;
							}
							const int ddx = s.fx + s.fz - pfx;
							const int ddy = s.fy + s.fz - pfy;
							if (ddx >= -2 && ddy >= -2 && ddx < s.xts * c_tilesize + 2 && ddy < s.yts * c_tilesize + 2) {
								return true;
							}
						}
						return false;
					}();
					const bool own_slab = roofrow != nullptr && Mask::Is_slab(roofrow[x]) && !slab_from_below
										  && roofrow[x] - Mask::Slab == (is_spill ? spill_floor : light_floor_storey)
										  && footdx != nullptr
										  && (footdx[static_cast<size_t>(y) * foot_lw + x] != Foot_bias
											  || footdy[static_cast<size_t>(y) * foot_lw + x] != Foot_bias);
					a = roofrow != nullptr && kindrow != nullptr && kindrow[x] == Kind::Ground
										&& ((foot_reads && roofrow[x] == Mask::Clear) || (face_data && own_slab))
								? foot_field(x, y)
						: (trow != nullptr && tx >= 0 && tx < ftmpl->w) ? trow[x]
																		: 0;
					// Bare ground (its own foot) is lit only on a tile in the fill: the ceiling-anchored field
					// reaches past a wall lower than the ceiling onto the ground outside it.  Only a ground-storey
					// light's fill describes ground tiles; an upper light's fill is its own band's.
					// An upper light's unreached cells under a gallery stay lit (its band skips them), but an
					// apron is ground the fill escaped onto: nothing standing there is lit, the spills own it.
					// A floor slab on the light's own storey is bare ground too, read at its stamped foot.
					if (veto_roof && face_data && roofrow != nullptr && kindrow != nullptr && kindrow[x] == Kind::Ground
						&& ((roofrow[x] == Mask::Clear && footdx[static_cast<size_t>(y) * foot_lw + x] == Foot_bias
							 && footdy[static_cast<size_t>(y) * foot_lw + x] == Foot_bias)
							|| own_slab)
						&& (light_floor_storey == 0 ? !object_reached(x, y) : [&] {
							   const int fc = foot_cell(x, y);
							   return fc >= 0 && apron_cell(fc);
						   }())) {
						a = 0;
					}
					if (veto_roof && face_data && ring != nullptr && roofrow != nullptr && roofrow[x] != Mask::Clear
						&& above_storey(x, y, roofrow[x])) {
						a = face_alpha(x, y, true);
					}
				} else {
					// Free dome (exterior lights, and marked tall shapes /
					// roofs lit as whole units): pure radial falloff, with
					// the travelled distance continued past `bias` for spills.
					int   dx    = x - sx;
					float dist2 = static_cast<float>(dx) * static_cast<float>(dx) + dy2;
					// From the light's FOOT: a tall lamp's raised screen centre would put its outdoor pool tiles
					// up-left of where its own field (and any inside view) has it.  To the pixel's own screen
					// position, not its stamped foot: every piece of a rampart has its own feet, and the wash would
					// break at each one.
					if (Splat_emit_shift > 0) {
						const int fdx = x - (sx + Splat_emit_shift);
						const int fdy = y - (sy + Splat_emit_shift);
						dx            = fdx;
						dist2         = static_cast<float>(fdx) * static_cast<float>(fdx)
								+ static_cast<float>(fdy) * static_cast<float>(fdy);
					}
					a = 0;
					if (dist2 <= rr) {
						a = static_cast<int>(amp * free_dome(dist2) + 0.5f);
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
					// Not for an outdoor light: its fill's reach edges (and per-tile plateaus) cut hard lines across
					// a rampart the bare wash crosses smoothly.
					if (surf_px && iside_px < 0 && !wall_obj && !shell_dome && !bare_dome_px && (veto_roof || is_spill)) {
						const int oa = object_alpha(x, y, 4);
						if (oa > a) {
							a = oa;
						}
						const int sa = face_alpha(x, y, roofrow != nullptr && Mask::Is_shell(roofrow[x]), top_px, iside_px);
						if (sa > a) {
							a = sa;
						}
						if (ftmpl != nullptr && trow != nullptr) {
							int ftx = x;
							int fty = y;
							// A whole unit (tree) reads the field at its trunk: at its own screen position the crown
							// hangs over the ground behind a wall, whose shadow then cut across it, offset.
							if (roofrow != nullptr && Mask::Is_tall(roofrow[x]) && kindrow != nullptr && Kind::Is_object(kindrow[x])
								&& sprites != nullptr) {
								const int oi = top_owner(x, y);
								if (oi >= 0 && !sprites[oi].building) {
									ftx = sprites[oi].fx;
									fty = sprites[oi].fy;
								}
							}
							const int ttx = ftx - sx - ftmpl->x0;
							const int tty = fty - sy - ftmpl->y0;
							const int fv  = (ttx >= 0 && ttx < ftmpl->w && tty >= 0 && tty < ftmpl->h)
													? ftmpl->alpha[static_cast<size_t>(tty) * ftmpl->w + ttx]
													: 0;
							if (fv > a) {
								a = fv;
							}
						}
					}
				}
				// A floor slab's thickness strip is facade: read like the faces above and below it (the gallery's
				// halfwall, the hall's wall), not only by the z-blind field, which would leave a darker band between
				// them.
				if (a > 0 && field_path && face_data && grid != nullptr && ring != nullptr && roofrow != nullptr
					&& Mask::Is_slab_edge(roofrow[x])) {
					a = std::max(a, face_alpha(x, y, true));
				}
				// An outdoor light stops at an opening's own tile: what stands there (a halfwall under a window, a
				// jamb) shows the camera its south/east face, which is the ROOM side when the room lies that way --
				// in the dark, the opening's pools light it.  Its top stays.
				// A spill's fan stops there too (at another building's openings).
				// Not exterior masonry (shell, whole-unit Tall pieces): a tower's outer face is no room side.
				if (a > 0 && !veto_roof && (!is_spill || cone_dx != 0 || cone_dy != 0) && !top_px && !shell_dome && grid != nullptr
					&& ring != nullptr && kindrow != nullptr
					&& (kindrow[x] != Kind::Ground
						|| (roofrow != nullptr && (roofrow[x] == Mask::Face_south || roofrow[x] == Mask::Face_east)))
					&& kindrow[x] != Kind::Object_top && !Kind::Is_cap(kindrow[x]) && footdx != nullptr) {
					// The light came in from the north or west (an open cell it reached there, not another
					// threshold): the room -- whatever stands in it, pillars included -- lies south / east,
					// toward the camera.
					auto turned_away = [&](int tci) {
						if (tci < 0 || Ring::Is_wall(ring, static_cast<size_t>(tci))
							|| !Ring::Has(ring, static_cast<size_t>(tci), Ring::Threshold_flag)) {
							return false;
						}
						auto lit_from = [&](int du, int dv) {
							const int u = tci % side + du;
							const int v = tci / side + dv;
							if (u < 0 || v < 0) {
								return false;
							}
							const size_t ni = static_cast<size_t>(v) * side + u;
							return !Ring::Is_wall(ring, ni) && Grid::Dist(grid[ni]) != 0
								   && !Ring::Has(ring, ni, Ring::Threshold_flag);
						};
						return lit_from(0, -1) || lit_from(-1, 0);
					};
					// Only a piece standing in the light's own storey: a rampart over a gate passage is not its sill
					// (its stamped feet land in the passage below).
					int pob = -1;
					if (kindrow[x] != Kind::Ground && sprites != nullptr) {
						pob = top_owner(x, y);
						if (pob < 0) {
							pob = face_owner(x, y);
						}
					}
					const bool own_band = pob < 0 || sprites[pob].fz / 4 < 5 * ((is_spill ? spill_floor : light_floor_storey) + 1);
					bool       away     = own_band && turned_away(foot_cell(x, y));
					// Or by its OWNER's footprint on the threshold: a low piece's band feet round onto the room cell.
					const int pcell = own_band && !away ? owner_cell(pob) : -1;
					if (pcell >= 0) {
						for (int v = pcell / side - sprites[pob].yts + 1; v <= pcell / side && !away; ++v) {
							for (int u = pcell % side - sprites[pob].xts + 1; u <= pcell % side && !away; ++u) {
								away = u >= 0 && v >= 0 && turned_away(v * side + u);
							}
						}
					}
					if (away) {
						a = 0;
					}
				}
				// What lies past the threshold is not the light's: the field's soft edge into the room's first floor
				// cell.  The floor only: the top of a piece ON the threshold (a sill) rounds its feet onto that cell.
				if (a > 0 && !veto_roof && (!is_spill || cone_dx != 0 || cone_dy != 0) && grid != nullptr && ring != nullptr
					&& footdx != nullptr && kindrow != nullptr && kindrow[x] == Kind::Ground
					&& (roofrow == nullptr || roofrow[x] == Mask::Clear)) {    // Bare floor: not a roof drawn over it.
					const int ci = foot_cell(x, y);
					if (ci >= 0 && !Ring::Is_wall(ring, static_cast<size_t>(ci)) && Grid::Dist(grid[ci]) == 0) {
						const int cu = ci % side;
						const int cv = ci / side;
						for (const auto& [du, dv] : {
									 std::pair{-1,  0},
                                     std::pair{ 0, -1},
                                     std::pair{ 1,  0},
                                     std::pair{ 0,  1}
                        }) {
							const int u = cu + du;
							const int v = cv + dv;
							if (u >= 0 && v >= 0 && u < side && v < side
								&& Ring::Has(ring, static_cast<size_t>(v) * side + u, Ring::Threshold_flag)
								&& !Ring::Is_wall(ring, static_cast<size_t>(v) * side + u)) {
								a = 0;
								break;
							}
						}
					}
				}
				// The same for a piece standing IN the wall line (a halfwall under a window the light does not pass):
				// reached from its north / west side only, the camera sees its room side -- faces and top alike.
				if (a > 0 && !veto_roof && (!is_spill || cone_dx != 0 || cone_dy != 0) && !shell_dome && grid != nullptr
					&& ring != nullptr && kindrow != nullptr && footdx != nullptr
					&& (Kind::Is_object(kindrow[x]) || kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]))) {
					// By its OWNER's footprint: a low piece's band feet round onto the room cell in front of it.
					int pob = top_owner(x, y);
					if (pob < 0) {
						pob = face_owner(x, y);
					}
					const int pcell = pob >= 0 ? owner_cell(pob) : foot_cell(x, y);
					if (pcell >= 0) {
						const int eu      = pcell % side;
						const int ev      = pcell / side;
						const int xts     = pob >= 0 ? sprites[pob].xts : 1;
						const int yts     = pob >= 0 ? sprites[pob].yts : 1;
						bool      back    = false;
						bool      front   = false;
						bool      in_wall = true;
						for (int v = ev - yts + 1; v <= ev && in_wall; ++v) {
							for (int u = eu - xts + 1; u <= eu && in_wall; ++u) {
								if (u < 0 || v < 0 || u >= side || v >= side
									|| !Ring::Is_wall(ring, static_cast<size_t>(v) * side + u)) {
									in_wall = false;
									break;
								}
								const unsigned char* rc = ring + (static_cast<size_t>(v) * side + u) * Ring::Bytes;
								front                   = front || Ring::Dist(rc[Ring::S]) != 0 || Ring::Dist(rc[Ring::E]) != 0;
								back                    = back || Ring::Dist(rc[Ring::N]) != 0 || Ring::Dist(rc[Ring::W]) != 0;
							}
						}
						// Only a piece in the light's own storey's wall line: a rampart up on the wall stands above it.
						const bool own_band
								= pob < 0 || sprites[pob].fz / 4 < 5 * ((is_spill ? spill_floor : light_floor_storey) + 1);
						if (in_wall && back && !front && own_band) {
							a = 0;
						}
					}
				}
				// Read like the wall faces it lies between (their room-side read), not the all-sides strip read.
				if (slab_edge_fade && face_data && grid != nullptr && ring != nullptr) {
					const float zw = face_zero_w;
					a              = std::max({a, wall_face_alpha(x, y, false, 0), wall_face_alpha(x, y, false, 1)});
					face_zero_w    = zw;
				}
				if (a > 0 && slab_edge_fade && footdx != nullptr) {
					a = static_cast<int>(static_cast<float>(a) * above_ceiling_fade(x, y) + 0.5f);
				}
				// A piece stacked a storey up in a tall room (a pillar on a pillar): its band feet land on the floor
				// far behind it, where the z-blind field (or its top's wall read) lit it brighter than the piece below.
				// Read it from its own footprint like the one under it, tops too, and fade with height above its base.
				// Object pixels only: the hall's upper wall courses (wall-face pixels) keep their own reads.
				if (a > 0 && veto_roof && !is_spill && sprites != nullptr && kindrow != nullptr && footdx != nullptr
					&& kindrow[x] != Kind::Ground
					&& kindrow[x] != Kind::Wall_face
					// A shell piece (a sill in the wall line, its window gone) stands in no room: its own face read is 0.
					&& (roofrow == nullptr || !Mask::Is_shell(roofrow[x]))) {
					// Its box is an object's, or a wall's where the piece reaches the roof found over its own tile --
					// which depends on what the viewer's roof cut leaves standing, so a pillar's top could flip as
					// the Avatar walks away.
					int sob = wall_owner(x, y, false);
					if (sob < 0) {
						sob = wall_owner(x, y, true, top_px || Kind::Is_cap(kindrow[x]) || kindrow[x] == Kind::Object_top);
					}
					if (sob >= 0 && sprites[sob].fz / 4 >= 5 * (light_floor_storey + 1)) {
						const float zw = face_zero_w;
						int         wf = -1;
						for (const int sd : {0, 1}) {
							if (top_px || obj_side < 0 || obj_side == sd) {
								wf = std::max(wf, wall_face_alpha(x, y, false, sd, sob));
							}
						}
						face_zero_w = zw;
						if (wf >= 0) {
							a = wf;
						}
						// A top lies at its piece's top height, where its faces' upper edge fades to: the same value, no
						// seam (and fixed -- the stamped feet of a top are not its height).
						const bool  is_top = top_px || kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x]);
						const float hl
								= is_top ? static_cast<float>(sprites[sob].fz / 4 + Fast_info(sprites[sob].shape).get_3d_height())
										 : foot_lift(x, y);
						const float fade = std::clamp(1.0f - (hl - static_cast<float>(sprites[sob].fz / 4)) / 7.5f, 0.0f, 1.0f);
						a                = static_cast<int>(static_cast<float>(a) * fade + 0.5f);
					}
				}
				// The hall's upper wall courses (a storey up, below the light's ceiling) fade with height over the same
				// span as the pillars standing before them: one surface beside the other, flat lit, would look pasted
				// on.
				// A window set in that course (a barred window a storey up) is part of it: the same fade, measured from
				// the course's base, or its glass would glow at the lamp's full value beside the dim wall top.
				if (a > 0 && veto_roof && !is_spill && sprites != nullptr && kindrow != nullptr && footdx != nullptr
					&& (kindrow[x] == Kind::Wall_face || pane_px)) {
					const int floor = 5 * (light_floor_storey + 1);
					// The course's base: the wall box under the pixel, or for glass the window's own course -- the wall
					// boxes on its footprint (its box covers only part of the art).
					int base = -1;
					if (pane_px) {
						const int  win  = art_owner(x, y, true);
						const auto memo = win >= 0 ? pane_course_memo.find(win) : pane_course_memo.end();
						if (memo != pane_course_memo.end()) {
							base = memo->second;
						} else if (win >= 0) {
							int wu, wv;
							box_cell(sprites[win], wu, wv);
							for (int i = 0; i < nsprites; ++i) {
								const Sprite_box& sb = sprites[i];
								if (!sb.wall || sb.fz > sprites[win].fz) {
									continue;
								}
								int su, sv;
								box_cell(sb, su, sv);
								if (su - sb.xts < wu && wu - sprites[win].xts < su && sv - sb.yts < wv
									&& wv - sprites[win].yts < sv) {
									base = std::max(base, sb.fz / 4);
								}
							}
							// A window standing on the storey below's block (a hall-high window on the ground course) has
							// no course box of its own under it: its course starts at its own lift.
							if (base < floor) {
								base = sprites[win].fz / 4;
							}
							pane_course_memo[win] = base;
						}
					} else {
						const int wob = wall_owner(x, y);
						base          = wob >= 0 ? sprites[wob].fz / 4 : -1;
					}
					if (base >= floor && base < 5 * light_top_storey) {
						const float fade = std::clamp(1.0f - (foot_lift(x, y) - static_cast<float>(base)) / 7.5f, 0.0f, 1.0f);
						a                = static_cast<int>(static_cast<float>(a) * fade + 0.5f);
					}
				}
				// The same for a light under its room's ceiling: a piece standing at that ceiling or above (the
				// barred window and its sill a storey up, over the passage wall the lamp stands behind) is out of its
				// room -- the wall cell's arrivals below it are no light up there.
				if (a > 0 && veto_roof && !is_spill && sprites != nullptr && kindrow != nullptr && light_top_storey >= 1
					&& (kindrow[x] != Kind::Ground || pane_px)) {
					int ob = art_owner(x, y);
					if (ob < 0) {
						ob = top_owner(x, y);
					}
					if (ob < 0) {
						ob = face_owner(x, y);
					}
					if (ob >= 0 && sprites[ob].fz / 4 >= 5 * light_top_storey) {
						// A top up there (a sill) is out of reach; a face (the upper part of a tall wall the light reaches
						// below) fades with its height above the light's ceiling over about a storey and a half -- cut
						// off flat at the floor line it looked unnatural, fully lit too.
						const bool face_like = !top_px && kindrow[x] != Kind::Object_top && !Kind::Is_cap(kindrow[x]);
						float      fade      = 0.0f;
						if (face_like && footdx != nullptr) {
							fade = above_ceiling_fade(x, y);
						}
						// Unless the face looks onto a floor laid at or below its base (the room a storey up, over the
						// light's ceiling): the tall hall's wall rises over open space, this one stands in the next room.
						if (fade > 0.0f && ring != nullptr && grid != nullptr) {
							const int oc = owner_cell(ob);
							const int fs = iside_px >= 0 ? iside_px : owner_face_side(x, y, ob);
							if (oc >= 0 && fs >= 0) {
								const int fc = face_front_cell(oc % side, oc / side, fs);
								if (fc >= 0 && ceiling_over(static_cast<size_t>(fc)) <= sprites[ob].fz / 4) {
									fade = 0.0f;
								}
							}
						}
						a = static_cast<int>(static_cast<float>(a) * fade + 0.5f);
					}
				}
				// A window's spill lights the facade around its own storey: what stands a storey up -- the next
				// floor's facade strip (a flat bright band across a tall wall), the crenellations on the roof above
				// (their tops half lit, their faces not) -- stays dark to it.  A spill onto a deck too: it lights
				// that deck's storey, not the roof of the building it came out of.
				if (a > 0 && is_spill && sprites != nullptr && kindrow != nullptr && (kindrow[x] != Kind::Ground || pane_px)) {
					int ob = kindrow[x] == Kind::Wall_face ? strip_over(x, y, -1) : -1;
					// A pane (a window a storey up) is its light-passing piece's.
					if (ob < 0 && pane_px) {
						ob = art_owner(x, y, true);
					}
					if (ob < 0) {
						ob = top_owner(x, y);
					}
					if (ob < 0) {
						ob = face_owner(x, y);
					}
					if (ob >= 0 && sprites[ob].fz / 4 >= 5 * (spill_floor + 1)) {
						a = 0;
					}
				}
				// An opening's pool lights the room under its ceiling: a top up at that ceiling or above (the tower
				// beside a gate passage, the wall over it) is out in the open, not in the pool's room.  So is any
				// pixel of a whole-unit exterior mass (Tall mark) that rises to it.
				if (a > 0 && pool_shaped && kindrow != nullptr && sprites != nullptr
					&& (top_px || kindrow[x] == Kind::Object_top || Kind::Is_cap(kindrow[x])
						|| (roofrow != nullptr && Mask::Is_tall(roofrow[x])))) {
					int tob = top_owner(x, y);
					if (tob < 0) {
						tob = wall_owner(x, y);
					}
					if (tob >= 0 && sprites[tob].fz / 4 + Fast_info(sprites[tob].shape).get_3d_height() >= 5 * (spill_floor + 1)) {
						a = 0;
					}
				}
				// The same for an opening's own pool: what stands on its opening tile turns that face away from it.
				if (a > 0 && pool_shaped && !top_px && kindrow != nullptr && footdx != nullptr
					&& (Splat_pool_nx > 0 || Splat_pool_ny > 0)
					&& (kindrow[x] != Kind::Ground
						|| (roofrow != nullptr && (roofrow[x] == Mask::Face_south || roofrow[x] == Mask::Face_east)))
					&& kindrow[x] != Kind::Object_top && !Kind::Is_cap(kindrow[x])) {
					// By its OWNER's footprint: a low piece's band feet round onto the room cell in front of it.
					const int wu  = grid_rt + Splat_pool_wdx;
					const int wv  = grid_rt + Splat_pool_wdy;
					int       pob = top_owner(x, y);
					if (pob < 0) {
						pob = face_owner(x, y);
					}
					const int pcell = pob >= 0 ? owner_cell(pob) : foot_cell(x, y);
					if (pcell >= 0) {
						const int eu  = pcell % side;
						const int ev  = pcell / side;
						const int xts = pob >= 0 ? sprites[pob].xts : 1;
						const int yts = pob >= 0 ? sprites[pob].yts : 1;
						if (wu > eu - xts && wu <= eu && wv > ev - yts && wv <= ev) {
							a = 0;
						}
					}
				}
				// A spill landing on a mountain top keeps its cone across the rock risen to that plateau beside it (its top
				// at the spill's floor): the plateau's own z-blind read at the pixel, as the mountain top's surface takes.
				// The rock's art stands on the ground, and its foot read sat 4px per lift off the cone -- an offset fan.
				if (is_spill && spill_floor >= 1 && sprites != nullptr && ftmpl != nullptr && trow != nullptr) {
					const int mo = art_owner(x, y);
					if (mountain_deck()
						|| (mo >= 0 && sprites[mo].fz / 4 + Fast_info(sprites[mo].shape).get_3d_height() == 5 * spill_floor
							&& mountain_side(mo))) {
						const int tx = x - sx - ftmpl->x0;
						a            = (tx >= 0 && tx < ftmpl->w) ? trow[x] : 0;
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

	// Final coverage `cov` with the light's edges on wall faces (doors, panes) softened over ~2px into `out`;
	// world pixels a softened edge newly reveals are copied from `srcpix` into `dstpix`.
	void Soften_face_edges(
			const Splat_frame& f, const unsigned char* cov, std::vector<unsigned char>& out, unsigned char* dstpix, int dst_lw) {
		const int    W  = f.W;
		const int    H  = f.H;
		const size_t sz = static_cast<size_t>(W) * H;
		out.assign(cov, cov + sz);
		if (f.roofpix == nullptr) {
			return;
		}
		auto face = [&](int x, int y) {
			const unsigned char m = f.roofpix[static_cast<size_t>(y) * f.roof_lw + x];
			return Mask::Is_face(m)
				   || (f.kindpix != nullptr && f.kindpix[static_cast<size_t>(y) * f.kind_lw + x] == Kind::Wall_face);
		};
		// Two 3x3 box passes (a ~2px tent), each face pixel reading every neighbour but only face pixels written:
		// the lit floor or top beside a masked face keeps its value, the face takes a soft edge from it.
		// Only the same face's own pixels: across its edge (a slab, a top, the floor) another surface's light
		// would bleed onto it -- a light behind the wall would show through the face it never reaches.
		// The face test is precomputed into a key (mask value + 1 on face pixels, 0 elsewhere; same key = same
		// face), and only the box around non-zero coverage is walked -- outside it every input is 0, so the passes
		// leave the copy of `cov` unchanged.
		int bx0 = W;
		int by0 = H;
		int bx1 = -1;
		int by1 = -1;
		for (int y = 0; y < H; ++y) {
			const unsigned char* row = cov + static_cast<size_t>(y) * W;
			int                  lo  = 0;
			while (lo < W && row[lo] == 0) {
				++lo;
			}
			if (lo == W) {
				continue;
			}
			int hi = W - 1;
			while (row[hi] == 0) {
				--hi;
			}
			bx0 = std::min(bx0, lo);
			bx1 = std::max(bx1, hi);
			by0 = std::min(by0, y);
			by1 = y;
		}
		if (bx1 >= 0) {
			bx0 = std::max(bx0 - 2, 1);
			by0 = std::max(by0 - 2, 1);
			bx1 = std::min(bx1 + 2, W - 2);
			by1 = std::min(by1 + 2, H - 2);
			// Keys over the box plus its 1px rim (neighbour reads).
			const int kx0 = bx0 - 1;
			const int ky0 = by0 - 1;
			const int kw  = bx1 - bx0 + 3;
			const int kh  = by1 - by0 + 3;
			// Plain grow-only buffers: a std::vector resize value-initialized the changing box every frame.
			thread_local std::unique_ptr<uint16_t[]>      key_buf;
			thread_local size_t                           key_cap = 0;
			thread_local std::unique_ptr<unsigned char[]> prev_buf;
			thread_local size_t                           prev_cap = 0;
			if (key_cap < static_cast<size_t>(kw) * kh) {
				key_cap = static_cast<size_t>(kw) * kh;
				key_buf.reset(new uint16_t[key_cap]);
			}
			if (prev_cap < sz) {
				prev_cap = sz;
				prev_buf.reset(new unsigned char[prev_cap]);
			}
			uint16_t* const      keyp  = key_buf.get();
			unsigned char* const prevp = prev_buf.get();
			for (int y = 0; y < kh; ++y) {
				const unsigned char* mrow = f.roofpix + static_cast<size_t>(ky0 + y) * f.roof_lw + kx0;
				const unsigned char* krow
						= f.kindpix != nullptr ? f.kindpix + static_cast<size_t>(ky0 + y) * f.kind_lw + kx0 : nullptr;
				uint16_t* out_k = keyp + static_cast<size_t>(y) * kw;
				for (int x = 0; x < kw; ++x) {
					const bool is_face = Mask::Is_face(mrow[x]) || (krow != nullptr && krow[x] == Kind::Wall_face);
					out_k[x]           = is_face ? static_cast<uint16_t>(mrow[x] + 1) : 0;
				}
			}
			for (int pass = 0; pass < 2; ++pass) {
				for (int y = by0 - 1; y <= by1 + 1; ++y) {
					std::memcpy(
							prevp + static_cast<size_t>(y) * W + bx0 - 1, out.data() + static_cast<size_t>(y) * W + bx0 - 1,
							static_cast<size_t>(bx1 - bx0 + 3));
				}
				for (int y = by0; y <= by1; ++y) {
					const uint16_t* krow = keyp + static_cast<size_t>(y - ky0) * kw - kx0;
					for (int x = bx0; x <= bx1; ++x) {
						const uint16_t own = krow[x];
						if (own == 0) {
							continue;
						}
						int sum = 0;
						int n   = 0;
						for (int dy = -1; dy <= 1; ++dy) {
							const uint16_t*      kr = krow + static_cast<ptrdiff_t>(dy) * kw;
							const unsigned char* pr = prevp + static_cast<size_t>(y + dy) * W;
							for (int dx = -1; dx <= 1; ++dx) {
								if (kr[x + dx] == own) {
									sum += pr[x + dx];
									++n;
								}
							}
						}
						out[static_cast<size_t>(y) * W + x] = static_cast<unsigned char>((sum + n / 2) / n);
					}
				}
			}
		}
		// A floor's thickness strip between two wall faces on the same row (a gallery's edge across a hall wall)
		// is that wall's band: it takes the light the faces either side of it show, interpolated across it --
		// read on its own it would be darker than the wall above and below.  Never darkened.
		for (int y = 0; y < H; ++y) {
			const unsigned char* mrow = f.roofpix + static_cast<size_t>(y) * f.roof_lw;
			for (int x = 1; x + 1 < W; ++x) {
				if (!Mask::Is_slab_edge(mrow[x]) || Mask::Is_slab_edge(mrow[x - 1]) || !face(x - 1, y)) {
					continue;
				}
				int r = x;
				while (r + 1 < W && Mask::Is_slab_edge(mrow[r + 1]) && r - x < 8) {
					++r;
				}
				if (r + 1 >= W || !face(r + 1, y) || r - x >= 8) {
					x = r;
					continue;
				}
				const float lv = out[static_cast<size_t>(y) * W + x - 1];
				const float rv = out[static_cast<size_t>(y) * W + r + 1];
				const int   n  = r - x + 2;
				for (int i = x; i <= r; ++i) {
					const float    t = static_cast<float>(i - x + 1) / static_cast<float>(n);
					const int      v = static_cast<int>(lv + (rv - lv) * t + 0.5f);
					unsigned char& o = out[static_cast<size_t>(y) * W + i];
					if (v > o) {
						o = static_cast<unsigned char>(v);
					}
				}
				x = r;
			}
		}
		if (dstpix == nullptr || f.srcpix == nullptr) {
			return;
		}
		for (int y = 0; y < H; ++y) {
			for (int x = 0; x < W; ++x) {
				const size_t i = static_cast<size_t>(y) * W + x;
				if (cov[i] == 0 && out[i] != 0) {
					dstpix[static_cast<size_t>(y) * dst_lw + x] = f.srcpix[static_cast<size_t>(y) * f.src_lw + x];
				}
			}
		}
	}

	// Stamp one painted object into the surface channels.  Objects paint
	// back-to-front, so the topmost sprite at each pixel owns it.
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
		Shape_frame*                 frame              = obj->get_shape();
		if (!frame || !frame->is_rle()) {
			return;
		}
		// Only pixels a light splat actually samples matter: skip any object whose
		// sprite touches none of last frame's splat rectangles (Light_layers::build
		// records them, grown by a margin).  With no lights in view the whole mask
		// paint is skipped.  A newly appearing light or a view jump self-corrects
		// on the next frame.
		const TileRect sprite_box(sx - frame->get_xleft(), sy - frame->get_yabove(), frame->get_width(), frame->get_height());
		{
			// A skipped object is not left stale, it is left UNSTAMPED (Mask::Clear = ground), which drinks the
			// z-blind field and renders bright -- so anything the rects miss becomes a lit copy of the wall face it
			// should have masked.
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
		// A non-solid hanging (tapestry, banner, painting) standing on a WALL tile is part of that wall's face:
		// leave the wall's stamp under it, or its pixels read clear ground -- the z-blind field of whatever lies
		// beyond the wall (a lamp outside lit a tapestry inside).
		// Not a light-passing piece (open shutters on a first-floor window, in front of the wall they hang on):
		// that is a pane, stamped as one like the ground floor's.
		if (obj->as_actor() == nullptr && !Fast_info(obj->get_shapenum()).is_solid()
			&& !Fast_info(obj->get_shapenum()).is_floor(obj->get_framenum()) && !Fast_info(obj->get_shapenum()).is_light_source()
			&& !NaturalLight::Object_passes_light(obj) &&
			[&] {
				const Tile_coord t = obj->get_tile();
				// Footprints extend up-left of their anchor: the wall may live in the chunk east or south.
				for (int dcy = 0; dcy < 2; ++dcy) {
					for (int dcx = 0; dcx < 2; ++dcx) {
						Map_chunk* const ch = map->get_chunk_safely(
								(t.tx / c_tiles_per_chunk + dcx) % c_num_chunks, (t.ty / c_tiles_per_chunk + dcy) % c_num_chunks);
						if (ch == nullptr) {
							continue;
						}
						Object_iterator it(ch->get_objects());
						Game_object*    o;
						while ((o = it.get_next()) != nullptr) {
							// A wall at the hanging's own height: a secret walk-through wall piece under the upper
							// wall course above it (whose footprint shares its tile) is no tapestry on that course.
							if (o != obj && Fast_info(o->get_shapenum()).is_solid()
								&& o->get_footprint().has_world_point(t.tx, t.ty) && o->get_lift() <= t.tz
								&& o->get_lift() + Fast_info(o->get_shapenum()).get_3d_height() > t.tz
								&& NaturalLight::Object_is_wall_face(o)) {
								return true;
							}
						}
					}
				}
				return false;
			}()) {
			return;
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
			const Shape_info& rinf = Fast_info(obj->get_shapenum());
			if (!NaturalLight::Object_is_wall_face(obj) && !rinf.is_floor(obj->get_framenum()) && !rinf.is_roof(obj->get_framenum())
				&& (sprite_box.y < sy || sprite_box.x < sx)) {
				Sprite_box ob{
						static_cast<short>(sprite_box.x),
						static_cast<short>(sprite_box.y),
						static_cast<short>(sprite_box.x + sprite_box.w - 1),
						static_cast<short>(sprite_box.y + sprite_box.h - 1),
						static_cast<short>(sx),
						static_cast<short>(sy),
						static_cast<short>(obj->get_shapenum()),
						static_cast<short>(obj->get_framenum()),
						static_cast<short>(rinf.get_3d_xtiles(obj->get_framenum())),
						static_cast<short>(rinf.get_3d_ytiles(obj->get_framenum())),
						static_cast<short>(4 * obj->get_lift())};
				ob.building = rinf.get_shape_class() == Shape_info::building;
				light_sprite_boxes.push_back(ob);
			} else if (NaturalLight::Object_is_wall_face(obj)) {
				Sprite_box wb{
						static_cast<short>(sprite_box.x),
						static_cast<short>(sprite_box.y),
						static_cast<short>(sprite_box.x + sprite_box.w - 1),
						static_cast<short>(sprite_box.y + sprite_box.h - 1),
						static_cast<short>(sx),
						static_cast<short>(sy),
						static_cast<short>(obj->get_shapenum()),
						static_cast<short>(obj->get_framenum()),
						static_cast<short>(rinf.get_3d_xtiles(obj->get_framenum())),
						static_cast<short>(rinf.get_3d_ytiles(obj->get_framenum())),
						static_cast<short>(4 * obj->get_lift())};
				wb.wall = true;
				light_sprite_boxes.push_back(wb);
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
		// samples the outdoor cells hanging over it (a window's fan would light
		// the perpendicular wall around the corner).  Splat_radial_light
		// resolves Shell_face by the wall's own foot-ward cell instead.
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
		constexpr static unsigned char Kind_unset  = 0xff;    // Transient: marks the pixels a ground paint covered.
		static const Xform_palette     kind_ground = [] {
            Xform_palette x;
            for (int i = 0; i < 256; ++i) {
                x.colors[i] = Kind_unset;
            }
            return x;
		}();
		// Ground owns its pixels' FOOT too: else the feet of a lower storey's wall under it shear the upper floor's read.
		auto paint_kind_ground = [&]() {
			frame->paint_rle_transformed(light_kind_mask, sx, sy, kind_ground);
			unsigned char* kbits = light_kind_mask->get_bits();
			const int      klw   = static_cast<int>(light_kind_mask->get_line_width());
			const int      W     = static_cast<int>(light_kind_mask->get_width());
			const int      H     = static_cast<int>(light_kind_mask->get_height());
			const int      x0    = std::max(sx - frame->get_xleft(), 0);
			const int      y0    = std::max(sy - frame->get_yabove(), 0);
			const int      x1    = std::min(sx - frame->get_xleft() + frame->get_width(), W);
			const int      y1    = std::min(sy - frame->get_yabove() + frame->get_height(), H);
			const bool     feet  = light_foot_dx.size() >= static_cast<size_t>(W) * H;
			// True ground under a surface at ztop is px + 4*ztop (as object tops), kept on the footprint.
			// A flat thing lying on a floor (one unit high: a rug, scattered straw) reads the floor's own plane:
			// its unit-higher feet would sample the field half a tile off.
			const int  zh   = Fast_info(obj->get_shapenum()).get_3d_height();
			const bool flat = zh <= 1 && !Fast_info(obj->get_shapenum()).is_floor(obj->get_framenum())
							  && !Fast_info(obj->get_shapenum()).is_roof(obj->get_framenum());
			const int      ztop = obj->get_lift() + (flat ? 0 : zh);
			const TileRect gfp  = obj->get_footprint();
			const int      f0x  = sx + 4 * obj->get_lift();
			const int      f0y  = sy + 4 * obj->get_lift();
			for (int py = y0; py < y1; ++py) {
				for (int px = x0; px < x1; ++px) {
					unsigned char& k = kbits[static_cast<size_t>(py) * klw + px];
					if (k != Kind_unset) {
						continue;
					}
					k = Kind::Ground;
					if (feet) {
						const int gx = std::clamp(px + 4 * ztop, f0x - gfp.w * c_tilesize + 1, f0x);
						const int gy = std::clamp(py + 4 * ztop, f0y - gfp.h * c_tilesize + 1, f0y);
						light_foot_dx[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(Foot_bias + gx - px);
						light_foot_dy[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(Foot_bias + gy - py);
					}
				}
			}
		};
		// A floor slab's south/right THICKNESS strip (Slab_edge + storey): the band
		// the clipped top paint leaves over.  It is the deck's own edge, so a spill
		// fan must cross it (a plain Tall breaks the glow with a strip-wide dark
		// band), but it must stay dark under a room's veto light, which a Tall +
		// storey mark would not.  No geometric test identifies a deck reliably --
		// ramparts carry merlons above and passages below -- so the strip is
		// labelled.  It carries its STOREY for the same reason the slab surface
		// does: a storey-less mark would let a ground spill light the edge of a
		// slab a storey up.
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
		// Surface-identity channels: the topmost sprite at a pixel owns its KIND;
		// wall faces and objects also record their per-tile foot offset so the
		// splat can sample the owner's cell instead of the cells the sprite
		// overlaps.    Viewer-independent.
		// A loose item resting on an upper storey owns no surface of its own: the
		// branches below leave the mark beneath it alone.  It must inherit the KIND
		// and FOOT channels for the same reason -- stamping them while the mask
		// stays the slab's pairs one object's mask with another's owner cell, and
		// the splat then lights the item's silhouette at that cell's flat alpha.
		const bool inherits_mark = obj->is_dragable() && obj->get_lift() >= 5;
		if (light_kind_mask && !light_foot_dx.empty() && !inherits_mark) {
			const Shape_info& sinf = Fast_info(obj->get_shapenum());
			int               kind = Kind::Ground;
			bool              pane = false;
			// A loose item is an OBJECT whatever its solid flag says.  A non-solid
			// one would otherwise fall through to Kind::Ground and take the z-blind
			// field at its own screen position, bright over the wall it lies
			// against.
			const bool loose_item = obj->is_dragable();
			// Panes and open door leaves stay GROUND: they glow with the light
			// behind them.
			const bool pane_shape = (sinf.is_door() && !obj->is_closed_door()) || NaturalLight::Object_passes_light(obj);
			// A shutter leaf is NON-solid, yet it must pass the solid gate as a
			// pane: otherwise it stamps no FEET at all and its pixels keep the
			// previous sprite's (a hard-edged band down the shutter).
			if ((sinf.is_solid() || loose_item || pane_shape) && !sinf.is_floor(obj->get_framenum())
				&& !sinf.is_roof(obj->get_framenum())) {
				// They still need FEET: their glow belongs to the light at their
				// own tile, not to whatever their sprite hangs over up-screen.
				if (pane_shape) {
					pane = true;
				} else {
					kind = NaturalLight::Object_is_wall_face(obj) ? Kind::Wall_face : Kind::Object;
				}
			}
			if (kind == Kind::Ground && !pane) {
				paint_kind_ground();
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
				// centre: the splat's blend needs the pixel's position inside its tile, and snapping
				// would throw it away, rendering a wall top one flat value per tile.
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
				// face to its far tile -- a 1x4 wall beside an open door would read
				// its north end where the art shows the south end at the doorway.  A
				// wall's THIN axis pins the shear: its ground strip is one tile
				// wide, so the offset from that edge IS the elevation.
				// The cap is how far up-left the art may sit from f0, and f0 is the
				// z=0 anchor, so it spans the LIFT as well as the shape's height.
				// At 4*height alone a raised sprite's pixels never shear back as far
				// as their own footprint, and foot_clamp would pin the whole sprite
				// to its north-west tile.
				const int zshear = 4 * (obj->get_lift() + sinf.get_3d_height());
				// A wall BLOCK's x foot is left unclamped: clamping to its own
				// footprint pinned the art west of it to one corner, a hard seam
				// at every block boundary.
				const bool box_foot = kind == Kind::Wall_face && xts > 1 && yts > 1;
				// A box's top is its ART's top rect, as the mask cuts it (art may overhang the box a pixel).
				auto box_top = [&](int px, int py) {
					const int zh4 = 4 * sinf.get_3d_height();
					return xts > 1 && yts > 1 && px < sx - frame->get_xleft() + frame->get_width() - zh4
						   && py < sy - frame->get_yabove() + frame->get_height() - zh4;
				};
				auto shear = [&](int px, int py) {
					int s = 0;
					if (xts == 1) {
						s = f0x - c_tilesize / 2 - px;
					} else if (yts == 1) {
						s = f0y - c_tilesize / 2 - py;
					} else if (box_top(px, py)) {
						return zshear;    // The ground under a top lies 4px per z down-right.
					} else {
						// Its S face stands on its south edge, its E face on its east edge.
						s = std::min(f0x - px, f0y - py);
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
				// Only masonry (crenellations) shades by face; kegs, winches and beams take their floor whole, like trees.
				// A thin wall piece is masonry too (class 0 like trees, so the class cannot say).
				const bool thin_wall = sinf.is_solid() && zh >= 5 && std::min(xts, yts) == 1;
				// A low (2-high) building piece -- a halfwall, a balustrade -- takes its floor whole like a table: the
				// face tags would shade one face dark while a neighbouring piece of the same halfwall stays lit.
				// Crenellations and ramparts (3 high) keep their faces.
				const bool obj_faces = obj_top && ((sinf.get_shape_class() == Shape_info::building && zh >= 3) || thin_wall);
				const int  top_x1    = f0x - 4 * ztop;
				const int  top_y1    = f0y - 4 * ztop;
				const int  nom_x     = top_x1 - xts * c_tilesize + 1;
				const int  nom_y     = top_y1 - yts * c_tilesize + 1;
				// A crenellated top steps between its merlons: an art column (a row, along a N-S run) that
				// starts below (above) the box's top edge is a lower (higher) top, shifted down-right by as
				// many pixels.  A run takes its deepest step, the merlon's side face over its near end climbing to it.
				const bool       along_x = xts >= yts;
				std::vector<int> drop;
				// A crenellated piece is a RUN (longer than it is deep): a square block (a pillar) has no merlons,
				// and its art's own outline -- rounded, shaded -- would read as steps that vary with the view.
				if (obj_faces && !thin_wall && std::max(xts, yts) > 1 && xts != yts) {
					const int n = along_x ? fw : fh;
					drop.assign(static_cast<size_t>(n), 0);
					for (int i = 0; i < n; ++i) {
						int first = -1;
						if (along_x) {
							for (int y = 0; y < fh && first < 0; ++y) {
								if (sbits[static_cast<size_t>(y) * slw + i] != 0) {
									first = y0 + y;
								}
							}
						} else {
							const unsigned char* r = sbits + static_cast<size_t>(i) * slw;
							for (int x = 0; x < fw && first < 0; ++x) {
								if (r[x] != 0) {
									first = x0 + x;
								}
							}
						}
						if (first >= 0) {
							const int p                  = along_x ? x0 + i : y0 + i;
							const int expect             = (along_x ? nom_y : nom_x) + std::max(0, p - (along_x ? top_x1 : top_y1));
							drop[static_cast<size_t>(i)] = std::clamp(first - expect, -c_tilesize / 2, 4 * zh);
						}
					}
					for (int i = 0; i < n;) {
						const int sg = (drop[static_cast<size_t>(i)] > 0) - (drop[static_cast<size_t>(i)] < 0);
						if (sg == 0) {
							++i;
							continue;
						}
						int j = i, m = 0;
						for (; j < n && drop[static_cast<size_t>(j)] * sg > 0; ++j) {
							m = std::max(m, drop[static_cast<size_t>(j)] * sg);
						}
						// Narrower than half a tile is ragged art, a 1px step an overhanging rim, not an embrasure.
						for (int k = i; k < j; ++k) {
							drop[static_cast<size_t>(k)] = j - i >= c_tilesize / 2 && m >= 2 ? m * sg : 0;
						}
						i = j;
					}
				}
				// A top's foot is its own footprint, 4px per z down-right of its art,
				// on a block's walkway too: block tops read their footprint, and an
				// unsheared top would land tiles west of itself.
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
						// Art west or north of the top rect is its rim: a box's visible faces lie south-east of it.
						const int dz4 = drop.empty() ? 0 : drop[static_cast<size_t>(along_x ? x : y)];
						// Above a lowered top, in its own column, stands the merlon's side face.
						const bool side_px = dz4 > 0 && (along_x ? py < nom_y + dz4 : px < nom_x + dz4);
						const bool top_pix = obj_top && !side_px && px <= top_x1 + dz4 && py <= top_y1 + dz4;
						// A top stands over the ground 4px per z down-right of its art.
						const int s = top_pix ? 4 * ztop - dz4 : shear(px, py);
						// Per-tile feet even on an OBJECT: pinning it to its anchor
						// hands a half wall one flat value with no falloff.
						int ddx = box_foot && !box_top(px, py) ? s : foot_clamp(px + s, f0x, xts) - px;
						int ddy = foot_clamp(py + s, f0y, yts) - py;
						ddx     = std::clamp(ddx, -Foot_bias, Foot_bias - 1);
						ddy     = std::clamp(ddy, -Foot_bias, Foot_bias - 1);
						kbits[static_cast<size_t>(py) * klw + px] = static_cast<unsigned char>(
								top_pix     ? (thin_wall ? (xts == 1 ? Kind::Object_cap : Kind::Object_cap_ew) : Kind::Object_top)
								: side_px   ? (along_x ? Kind::Object_east : Kind::Object_south)
								: obj_faces ? (f0x - px >= f0y - py ? Kind::Object_south : Kind::Object_east)
											: kind);
						light_foot_dx[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(ddx + Foot_bias);
						light_foot_dy[static_cast<size_t>(py) * W + px] = static_cast<unsigned char>(ddy + Foot_bias);
					}
				}
			}
		} else if (light_kind_mask && inherits_mark) {
			// The item inherits what lies under it, but not a merlon's face side (a keg half over one would go half
			// dark).
			const int                             fw = frame->get_width();
			const int                             fh = frame->get_height();
			static std::unique_ptr<Image_buffer8> iscratch;
			if (!iscratch || static_cast<int>(iscratch->get_width()) < fw || static_cast<int>(iscratch->get_height()) < fh) {
				iscratch = std::make_unique<Image_buffer8>(
						std::max(fw, iscratch ? static_cast<int>(iscratch->get_width()) : 0),
						std::max(fh, iscratch ? static_cast<int>(iscratch->get_height()) : 0));
			}
			iscratch->fill8(0, fw, fh, 0, 0);
			frame->paint_rle_transformed(iscratch.get(), frame->get_xleft(), frame->get_yabove(), roof_set);
			unsigned char*       kbits = light_kind_mask->get_bits();
			const int            klw   = static_cast<int>(light_kind_mask->get_line_width());
			const int            W     = static_cast<int>(light_kind_mask->get_width());
			const int            H     = static_cast<int>(light_kind_mask->get_height());
			const unsigned char* sbits = iscratch->get_bits();
			const int            slw   = static_cast<int>(iscratch->get_line_width());
			const int            x0    = sx - frame->get_xleft();
			const int            y0    = sy - frame->get_yabove();
			for (int y = 0; y < fh; ++y) {
				const int py = y0 + y;
				if (py < 0 || py >= H) {
					continue;
				}
				for (int x = 0; x < fw; ++x) {
					const int px = x0 + x;
					if (px < 0 || px >= W || sbits[static_cast<size_t>(y) * slw + x] == 0) {
						continue;
					}
					unsigned char& k = kbits[static_cast<size_t>(py) * klw + px];
					if (k == Kind::Object_south || k == Kind::Object_east) {
						k = Kind::Object;
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
		// down on a rampart (or an NPC crossing it) would flip the pieces around it
		// from open-sky whole units to covered shell faces (Shell_face / Tall),
		// which the spill branch vetoes.
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
				const Shape_info& oi = Fast_info(o->get_shapenum());
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
		// Covered only by low caps (battlements) resting on `top` that are open to the sky.
		auto capped_open_sky = [&](int top) {
			const Tile_coord t   = obj->get_tile();
			const int        wtx = ((t.tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((t.ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			if (ch == nullptr) {
				return false;
			}
			bool            capped = false;
			Object_iterator it(ch->get_objects());
			Game_object*    o;
			while ((o = it.get_next()) != nullptr) {
				if (o == obj || o->as_actor() != nullptr || o->is_dragable()) {
					continue;
				}
				const Shape_info& oi = Fast_info(o->get_shapenum());
				if (!oi.is_solid() || o->get_lift() + oi.get_3d_height() <= top || !o->get_footprint().has_world_point(wtx, wty)) {
					continue;
				}
				const int otop = o->get_lift() + oi.get_3d_height();
				// Nor under the viewer's own ceiling: a tall hall's wall course capped below its roof is indoors
				// (its roof is cut from the render, so nothing static covers the cap).
				if (o->get_lift() != top || oi.get_3d_height() >= 5 || oi.is_floor(o->get_framenum())
					|| oi.is_roof(o->get_framenum()) || static_cover_above(wtx, wty, otop) || [&] {
						   const int vc = Viewer_ceiling_lift();
						   return vc < 31 && otop < vc;
					   }()) {
					return false;
				}
				capped = true;
			}
			return capped;
		};
		// Is this absolute tile under a roof ABOVE `z`?  The engine's own roof map
		// -- unlike the blocked-column probes it is not fooled by walls standing on
		// a deck, and it registers a ceiling however that ceiling is built.
		auto tile_roofed_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			return ch != nullptr && Light_chunk_roof(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z) < 31;
		};
		// Lowest thing blocking this tile's column at or above `z` (>= 31: none).
		// is_roof() starts its search 4 ABOVE the lift it is handed, so asking it
		// about a first-floor wall's own lift steps over that room's ceiling.
		auto lowest_cover_at = [&](int tx, int ty, int z) {
			const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
			const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
			Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
			return ch != nullptr ? Light_chunk_roof(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z - 4) : 255;
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
				const Shape_info& oi = Fast_info(o->get_shapenum());
				if (oi.is_floor(o->get_framenum()) || oi.is_roof(o->get_framenum()) || oi.get_3d_height() <= 0) {
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
				const int ohi = olo + std::max(Fast_info(o->get_shapenum()).get_3d_height(), 1);
				if (ohi <= z || olo >= z + 5) {
					continue;    // Not in the course resting on this top.
				}
				if (NaturalLight::Object_passes_light(o) && o->get_footprint().has_world_point(wtx, wty)) {
					return true;
				}
			}
			// A bare GAP in the course (the window shape gone): free just above the top, yet covered higher up -- a
			// lintel or the roof closes it, so it is an opening in the wall line, not a parapet under the sky.
			const int lx = wtx % c_tiles_per_chunk;
			const int ly = wty % c_tiles_per_chunk;
			if (!Light_occupied(ch, lx, ly, z) && !Light_occupied(ch, lx, ly, z + 1)) {
				const int cover = Light_chunk_roof(ch, lx, ly, z - 4);
				return cover < 31;
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
						full = Light_occupied(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z);
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
				if (o == obj
					|| (!Fast_info(o->get_shapenum()).is_floor(o->get_framenum())
						&& !Fast_info(o->get_shapenum()).is_roof(o->get_framenum()))) {
					continue;
				}
				const int base = o->get_lift() + Fast_info(o->get_shapenum()).get_3d_height();
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
		auto faces_exterior_raw = [&](bool doors_anchor = true, bool* out_south_in = nullptr, bool* out_east_in = nullptr,
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
			// The chunk blocked bitmap includes ACTORS: bitmap tests would make
			// the shell classification flicker as NPCs (or the viewer) walk by.
			// Scan the chunk's IMMOVABLE solids instead.
			auto static_solid_at = [&](int tx, int ty, int z_lo, int z_hi, bool walls_only = false) {
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
					const Shape_info& oi = Fast_info(o->get_shapenum());
					if (!oi.is_solid()) {
						continue;
					}
					const int lo = o->get_lift();
					if (lo > z_hi || lo + oi.get_3d_height() <= z_lo) {
						continue;
					}
					if (o->get_footprint().has_world_point(tx, ty) && (!walls_only || NaturalLight::Object_is_wall_face(o))) {
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
				// From the piece's own height: a sill a storey up looks out over a deck (a mountain top roofing the
				// passage beneath it), and that deck is no cover over the air the sill faces.
				return !static_cover_above(px, py, std::max(5, obj->get_lift()));
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
					if (!Fast_info(o->get_shapenum()).is_door()) {
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
			// Only the wall line buries a face: a pillar or desk stands IN the room the face looks onto.
			auto face_interior = [&](int tx, int ty) {
				return !static_solid_at(tx, ty, obj->get_lift() + 2, obj->get_lift() + 2, true) && !open_sky_tile(tx, ty);
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
		// faces_exterior reads only the static map round the piece (immovable solids, doors, cover), but scans
		// whole chunk object lists per footprint tile, so its full answer is kept across frames per piece, dropped
		// on any flood-content change (doors, edited objects) and every 2 s.
		auto faces_exterior = [&](bool doors_anchor = true, bool* out_south_in = nullptr, bool* out_east_in = nullptr,
								  uint32_t* out_south_bits = nullptr, uint32_t* out_east_bits = nullptr,
								  uint32_t* out_south_open = nullptr, uint32_t* out_east_open = nullptr) {
			struct Answer {
				int        shape, frame;
				Tile_coord tile;
				bool       shell, south_in, east_in;
				uint32_t   south_bits, east_bits, south_open, east_open;
			};
			static std::unordered_map<uint64_t, Answer> memo;
			static uint64_t                             memo_gen = ~0ULL;
			static Uint64                               memo_t   = 0;
			const Uint64                                now_fe   = SDL_GetTicks();
			if (memo_gen != flood_content_gen || now_fe - memo_t > 2000 || memo.size() > 65536) {
				memo.clear();
				memo_gen = flood_content_gen;
				memo_t   = now_fe;
			}
			const uint64_t   key   = (reinterpret_cast<uintptr_t>(obj) << 1) | (doors_anchor ? 1u : 0u);
			const int        shape = obj->get_shapenum();
			const int        frame = obj->get_framenum();
			const Tile_coord tile  = obj->get_tile();
			auto             it    = memo.find(key);
			if (it == memo.end() || it->second.shape != shape || it->second.frame != frame || !(it->second.tile == tile)) {
				Answer a{shape, frame, tile, false, false, false, 0, 0, 0, 0};
				a.shell = faces_exterior_raw(
						doors_anchor, &a.south_in, &a.east_in, &a.south_bits, &a.east_bits, &a.south_open, &a.east_open);
				it = memo.insert_or_assign(key, a).first;
			}
			const Answer& a = it->second;
			if (out_south_in != nullptr) {
				*out_south_in = a.south_in;
			}
			if (out_east_in != nullptr) {
				*out_east_in = a.east_in;
			}
			if (out_south_bits != nullptr) {
				*out_south_bits = a.south_bits;
			}
			if (out_east_bits != nullptr) {
				*out_east_bits = a.east_bits;
			}
			if (out_south_open != nullptr) {
				*out_south_open = a.south_open;
			}
			if (out_east_open != nullptr) {
				*out_east_open = a.east_open;
			}
			return a.shell;
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
		// Masonry art fills its 3D box; a statue's figure rises far past it and has no box top or faces.
		auto art_fills_box = [&]() {
			const Shape_info& bi = Fast_info(obj->get_shapenum());
			const int         zh = bi.get_3d_height();
			return frame->get_width() <= bi.get_3d_xtiles(obj->get_framenum()) * c_tilesize + 4 * zh + 4
				   && frame->get_height() <= bi.get_3d_ytiles(obj->get_framenum()) * c_tilesize + 4 * zh + 4;
		};
		int      tall_storey  = 0;
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
		const bool closed_door_leaf = Fast_info(obj->get_shapenum()).is_door() && obj->is_closed_door();
		auto       paint_shell_face = [&](bool lit_top, uint32_t south_bits = 0, uint32_t east_bits = 0, bool shell = true,
                                    bool mark_top = true, uint32_t south_open = 0, uint32_t east_open = 0,
                                    int top_mark = Mask::Shell_top, bool ext_corner = false) {
            frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_face);
            if (!lit_top) {
                return;
            }
            const int strip = 4 * Fast_info(obj->get_shapenum()).get_3d_height();
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
            const int xts  = Fast_info(obj->get_shapenum()).get_3d_xtiles(obj->get_framenum());
            const int yts  = Fast_info(obj->get_shapenum()).get_3d_ytiles(obj->get_framenum());
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
                    // A closed door's leaf is half a tile thick: only that edge is its top; the rest of the band the box's
                    // top would cover is the leaf's face (otherwise the wall's top line runs on across the door).
                    const bool is_top
                            = u < bw - strip && v < bh - strip && (!closed_door_leaf || (xts >= yts ? v : u) < c_tilesize / 2);
                    if (is_top && mark_top) {
                        // Shell_top = shell wall TOP: horizontal, so it has no viewer
                        // side.  Left CLEAR it would read as an interior face and take
                        // the viewer-gated arrivals, which for an outside light seen
                        // from inside collapse to the wrap-around path.  An
                        // UPPER-storey piece passes its storey mark instead: having no
                        // viewer side, Shell_top lights from anywhere, and a window
                        // sill one floor up would glow with a light on the far side of
                        // its wall.
                        mbits[static_cast<size_t>(py) * mlw + px] = static_cast<unsigned char>(top_mark);
                    } else {
                        // Face_south / Face_east = interior-facing wall face, lit only from the
                        // room on its SOUTH / EAST side.  Which that is, is known here and
                        // nowhere else; left as a plain 0 the splat would have to guess it from
                        // the avatar's position, and the guess flips mid-wall.
                        // A triangle whose OWN side has no room (the sprite's end cap, or a
                        // face buried against the next wall) takes the side that does: marking
                        // it for a non-room side would send the splat walking along the wall
                        // until it surfaces outdoors, lighting the inside faces of a shut cabin
                        // from the street.  But only if it is BURIED -- a face looking at the
                        // street is a real exterior face and keeps Shell_face, or the outside
                        // south face of a wall whose east side is a room would light up with
                        // that room's light.
                        const bool south    = u - v < diag;
                        const int  js       = tidx(px, py, false);
                        const int  je       = tidx(px, py, true);
                        const bool s_room   = js < 32 && (south_bits & (1u << js)) != 0;
                        const bool e_room   = je < 32 && (east_bits & (1u << je)) != 0;
                        const bool own_room = south ? s_room : e_room;
                        const bool own_open
                                = south ? (js < 32 && (south_open & (1u << js)) != 0) : (je < 32 && (east_open & (1u << je)) != 0);
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
                            // An interior corner walled in on BOTH sides borrows the nearest
                            // room through the splat's side walk; left on Shell_face it would
                            // light from outside, and cleared it would be a masked sliver.
                            // South first: the walk falls back to east by itself, and
                            // going east first can run along the wall out into the street.
                            // NOT when the piece's far side fronts OPEN AIR: then it is the
                            // building's OUTER corner, buried only against its own neighbours,
                            // and borrowing a room would let the hall's torches light the
                            // outside of the wall.
                            side_mark = Mask::Face_south;
                        }
                        if (side_mark > 0) {
                            mbits[static_cast<size_t>(py) * mlw + px] = static_cast<unsigned char>(side_mark);
                        }
                    }
                }
            }
		};
		// A pane is one face (Shell_face + Kind::Ground) -- but a closed door that lets light through keeps its
		// thin top edge, as a solid one does.
		auto paint_pane = [&]() {
			paint_shell_face(closed_door_leaf);
		};
		// A deck CONSTRUCT: pieces stacked over a lower structure's open deck (a
		// portcullis winch -- beams on the gate wall, a crossbeam on the beams,
		// the raised grate under it).  By its own lift the crossbeam is a storey
		// above the deck the whole thing stands on, so the deck's spill would
		// skip it, while the grate it covers would read as a covered pane and
		// take the ground spill through the passage below.  One unit, the storey
		// of the deck it rises from.  Returns that storey, or -1.
		auto deck_construct_storey = [&]() -> int {
			auto piece = [&](Game_object* o, bool roof = false) {
				const Shape_info& oi = Fast_info(o->get_shapenum());
				return o->as_actor() == nullptr && !o->is_dragable() && oi.is_solid() && !oi.is_floor(o->get_framenum())
					   && (roof || !oi.is_roof(o->get_framenum())) && !oi.is_light_source()
					   && (!oi.is_door() || o->is_closed_door()) && o->get_lift() >= 4;
			};
			auto for_tile = [&](int tx, int ty, auto&& fn, bool incl_self = false) {
				const int        wtx = ((tx % c_num_tiles) + c_num_tiles) % c_num_tiles;
				const int        wty = ((ty % c_num_tiles) + c_num_tiles) % c_num_tiles;
				Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
				if (ch == nullptr) {
					return;
				}
				Object_iterator it(ch->get_objects());
				Game_object*    o;
				while ((o = it.get_next()) != nullptr) {
					if ((incl_self || o != obj) && o->get_footprint().has_world_point(wtx, wty)) {
						fn(o);
					}
				}
			};
			// A roof piece resting on construct pieces over its WHOLE footprint (the crossbeam on the winch
			// beams and grate) caps the construct; a ceiling spans air over a room somewhere.
			auto construct_cap = [&](Game_object* c) {
				if (!Fast_info(c->get_shapenum()).is_roof(c->get_framenum()) || !piece(c, true)) {
					return false;
				}
				const TileRect cf = c->get_footprint();
				const int      cl = c->get_lift();
				for (int fy = cf.y; fy < cf.y + cf.h; ++fy) {
					for (int fx = cf.x; fx < cf.x + cf.w; ++fx) {
						bool held = false;
						for_tile(
								fx, fy,
								[&](Game_object* o) {
									const int ot = o->get_lift() + Fast_info(o->get_shapenum()).get_3d_height();
									held         = held || (o != c && piece(o) && ot >= cl - 1 && ot <= cl);
								},
								true);
						if (!held) {
							return false;
						}
					}
				}
				return true;
			};
			if (gwin->is_in_dungeon() || !(piece(obj) || construct_cap(obj))) {
				return -1;
			}
			// The lowest construct piece directly beneath `cur` (resting on it,
			// or a unit gap like the grate's under the crossbeam).
			auto support_of = [&](Game_object* cur) -> Game_object* {
				const TileRect cf   = cur->get_footprint();
				const int      cl   = cur->get_lift();
				Game_object*   best = nullptr;
				for (int fy = cf.y; fy < cf.y + cf.h; ++fy) {
					for (int fx = cf.x; fx < cf.x + cf.w; ++fx) {
						for_tile(fx, fy, [&](Game_object* o) {
							const int ot = o->get_lift() + Fast_info(o->get_shapenum()).get_3d_height();
							if (o != cur && piece(o) && ot >= cl - 1 && ot <= cl
								&& (best == nullptr || o->get_lift() < best->get_lift())) {
								best = o;
							}
						});
					}
				}
				return best;
			};
			const TileRect ft      = obj->get_footprint();
			const int      top     = obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height();
			bool           stacked = support_of(obj) != nullptr;
			bool           open    = true;
			for (int fy = ft.y; fy < ft.y + ft.h && open; ++fy) {
				for (int fx = ft.x; fx < ft.x + ft.w && open; ++fx) {
					for_tile(fx, fy, [&](Game_object* o) {
						const Shape_info& oi = Fast_info(o->get_shapenum());
						if (!open || o->as_actor() != nullptr || o->is_dragable() || !oi.is_solid()) {
							return;
						}
						const int ol = o->get_lift();
						const int ot = ol + oi.get_3d_height();
						if (ot <= top || ol < top) {
							return;
						}
						// Covered only by another piece that is itself open to the sky.
						if (!(piece(o) || construct_cap(o)) || static_cover_above(fx, fy, ot)) {
							open = false;
							return;
						}
						if (ol - top <= 1) {
							stacked = true;
						}
					});
				}
			}
			if (!open || !stacked) {
				return -1;
			}
			int          base = obj->get_lift();
			Game_object* cur  = obj;
			for (int i = 0; i < 4; ++i) {
				Game_object* const s = support_of(cur);
				if (s == nullptr) {
					break;
				}
				base = std::min(base, s->get_lift());
				cur  = s;
			}
			const int ds = storey_of(base + 1);
			return ds >= 1 ? ds : -1;
		};
		if (Fast_info(obj->get_shapenum()).is_floor(obj->get_framenum())) {
			// A floor slab used as the storey below's ceiling: mark its flat TOP
			// Tall + storey (not Roof) so a deck-level spill still lights it while
			// a ground window's glow is storey-gated off.  Inside, the slab's
			// front-facing thickness strip (bottom / right 4px-per-z) is left as
			// painted -- marking it would draw a dark seam along the adjoining
			// wall.  Outside, the whole sprite first gets a plain Tall face mark,
			// or the room's veto light shows as a bright beam over the strip.
			const int zs         = Fast_info(obj->get_shapenum()).get_3d_height();
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
			// storey it would be an "interior" slab, and a ground lamp in a tall hall
			// would light it from underneath through the z-blind field along the
			// walkway beneath the deck.
			// Asked of the chunk's own roof map ABOVE the slab's top: a wall
			// standing ON the deck is not a roof (which open_sky_above counted), a
			// hall's ceiling registers here even when it is the storey above's floor,
			// and the deck cannot roof itself.
			// Asked of EVERY footprint tile, not just the anchor: a floor slab is
			// several tiles, and one query for the lot would let a slab whose far end
			// lies under a hall roof call its exposed edge interior too.  Any tile
			// open to the sky makes the whole piece a deck.
			const bool slab_roofed = [&] {
				const TileRect fp = obj->get_footprint();
				const int      z  = obj->get_lift() + zs;
				for (int dy = 0; dy < fp.h; ++dy) {
					for (int dx = 0; dx < fp.w; ++dx) {
						const int        wtx = (((fp.x + dx) % c_num_tiles) + c_num_tiles) % c_num_tiles;
						const int        wty = (((fp.y + dy) % c_num_tiles) + c_num_tiles) % c_num_tiles;
						Map_chunk* const ch  = map->get_chunk_safely(wtx / c_tiles_per_chunk, wty / c_tiles_per_chunk);
						if (ch == nullptr) {
							return false;
						}
						const int rz = Light_chunk_roof(ch, wtx % c_tiles_per_chunk, wty % c_tiles_per_chunk, z);
						if (rz >= 31) {
							return false;
						}
					}
				}
				return true;
			}();
			const bool           deck_outdoors = !slab_roofed;
			const bool           slab_interior = upper_interior && top_storey >= 1 && !deck_outdoors;
			const Xform_palette& slab_pal      = slab_interior ? roof_slab[top_storey] : roof_tall[top_storey];
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
			const bool s_edge         = tile_roofed_at(slab_fp.x + slab_fp.w - 1, slab_fp.y + slab_fp.h, obj->get_lift() + zs);
			const bool e_edge         = tile_roofed_at(slab_fp.x + slab_fp.w, slab_fp.y + slab_fp.h - 1, obj->get_lift() + zs);
			const bool strip_exterior = !s_edge || !e_edge;
			// The strip always carries its own mark: it is the slab's DOWN-facing
			// edge and only a light below the slab's storey can see it, which the
			// splat cannot tell once the strip reads as the top.
			frame->paint_rle_transformed(roof_light_mask, sx, sy, strip_exterior ? roof_face : roof_strip[top_storey]);
			if (strip_exterior && strip > 0) {
				Sprite_box sb{
						static_cast<short>(sprite_box.x),
						static_cast<short>(sprite_box.y),
						static_cast<short>(sprite_box.x + sprite_box.w - 1),
						static_cast<short>(sprite_box.y + sprite_box.h - 1),
						static_cast<short>(sx),
						static_cast<short>(sy),
						static_cast<short>(obj->get_shapenum()),
						static_cast<short>(obj->get_framenum()),
						static_cast<short>(slab_fp.w),
						static_cast<short>(slab_fp.h),
						static_cast<short>(4 * obj->get_lift())};
				sb.wall  = true;
				sb.strip = static_cast<short>(strip);
				light_sprite_boxes.push_back(sb);
			}
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
					paint_kind_ground();
				}
			}
			return;
		} else if (const int ds = deck_construct_storey(); ds >= 1) {
			// Whole unit on the deck's storey, but faces matter: the TOP and the
			// EAST face take the deck's light; the SOUTH face only a light south
			// of it (Tall_south).  A light-passing piece (the grate) glows with
			// the light behind it as a whole.
			frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_tall[ds]);
			if (NaturalLight::Object_passes_light(obj)) {
				return;
			}
			const int                             strip = 4 * Fast_info(obj->get_shapenum()).get_3d_height();
			const int                             bx    = sx - frame->get_xleft();
			const int                             by    = sy - frame->get_yabove();
			const int                             bw    = frame->get_width();
			const int                             bh    = frame->get_height();
			static std::unique_ptr<Image_buffer8> cscratch;
			if (!cscratch || static_cast<int>(cscratch->get_width()) < bw || static_cast<int>(cscratch->get_height()) < bh) {
				const int csw = std::max(bw, cscratch ? static_cast<int>(cscratch->get_width()) : 0);
				const int csh = std::max(bh, cscratch ? static_cast<int>(cscratch->get_height()) : 0);
				cscratch      = std::make_unique<Image_buffer8>(csw, csh);
			}
			cscratch->fill8(0, bw, bh, 0, 0);
			frame->paint_rle_transformed(cscratch.get(), frame->get_xleft(), frame->get_yabove(), roof_set);
			unsigned char*       mbits = roof_light_mask->get_bits();
			const int            mlw   = static_cast<int>(roof_light_mask->get_line_width());
			const int            mw    = static_cast<int>(roof_light_mask->get_width());
			const int            mh    = static_cast<int>(roof_light_mask->get_height());
			const unsigned char* cbits = cscratch->get_bits();
			const int            clw   = static_cast<int>(cscratch->get_line_width());
			const int            diag  = bw - bh;
			for (int v = 0; v < bh; ++v) {
				const int py = by + v;
				if (py < 0 || py >= mh) {
					continue;
				}
				for (int u = 0; u < bw; ++u) {
					const int px = bx + u;
					if (px < 0 || px >= mw || cbits[static_cast<size_t>(v) * clw + u] == 0) {
						continue;
					}
					const bool is_top = u < bw - strip && v < bh - strip;
					if (!is_top && u - v < diag) {
						mbits[static_cast<size_t>(py) * mlw + px] = static_cast<unsigned char>(Mask::Tall_south + ds);
					}
				}
			}
			return;
		} else if (inside) {
			// +1 where it decides a covered block's top is the floor he stands on: one stair step below it
			// (a stairwell's top step) he is already climbing onto it.
			const int  av_lift       = main_actor ? main_actor->get_lift() : 0;
			const bool is_roof_shape = Fast_info(obj->get_shapenum()).is_roof(obj->get_framenum()) && obj->get_lift() > av_lift;
			// The viewer's CEILING, not the raw skip: a wall torch overhead cuts the render at its lift, but the hall's walls up
			// to the real ceiling are still the room's own -- one wall, one mask from every spot in the room.
			const int ceiling_z = Viewer_ceiling_lift();
			roof_like           = is_roof_shape || obj->get_lift() >= ceiling_z;
			const int top       = obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height();
			if (is_roof_shape && open_sky_above(top)) {
				// A LOWER structure's exposed roof still drawn while inside (a
				// gate passage, a porch below the viewer's storey -- the viewer's
				// own ceiling is hidden, so it is never this).  As a Roof mark no
				// light may touch it and it cuts a dark notch into the glow the
				// upper windows wash down the facade; clear it so it samples the
				// fields like the walls around it.
				// At storey 1+ it is a deck: Tall + storey, or a ground light filling the passage beneath lights its top.
				const int rs = storey_of(top);
				if (rs >= 1) {
					// Only the TOP is deck; the edge fascia faces away from every light that reaches it (deck above, passage
					// below).
					frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_set);
					// sx/sy already carry the lift: offset by the HEIGHT only.
					const TileRect             rfp    = obj->get_footprint();
					const int                  zh     = Fast_info(obj->get_shapenum()).get_3d_height();
					const int                  top_x1 = sx - 4 * zh;
					const int                  top_y1 = sy - 4 * zh;
					const int                  top_x0 = std::min(top_x1 - rfp.w * c_tilesize + 1, sx - frame->get_xleft());
					const int                  top_y0 = std::min(top_y1 - rfp.h * c_tilesize + 1, sy - frame->get_yabove());
					const TileRect             top_rect(top_x0, top_y0, top_x1 - top_x0 + 1, top_y1 - top_y0 + 1);
					Image_buffer::ClipRectSave clipsave(roof_light_mask);
					const TileRect             r = top_rect.intersect(clipsave.Rect());
					if (r.w > 0 && r.h > 0) {
						roof_light_mask->set_clip(r.x, r.y, r.w, r.h);
						frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_tall[rs]);
					}
					return;
				}
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
			} else if (!roof_like && top > std::max(ceiling_z, 5)) {
				// A grounded shape rising above the room's ceiling can only be an
				// exterior one (an outside tree); confirm open sky above its top
				// so nothing indoors is caught here.
				// Measured against a full storey, not the viewer's ceiling: under a
				// low passage roof the passage's own side walls would rise "above
				// the ceiling" and become whole-unit Tall, while a capped neighbour
				// block goes shell -- one wall, two masks.  At top 5 they take the
				// branches below, as they do from every other viewpoint.
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
					if (shell && Fast_info(obj->get_shapenum()).is_door() && !obj->is_closed_door()) {
						return;
					}
					// Pane rule, as in the ground and upper shell branches; an interior pane is partition.
					if (shell && NaturalLight::Object_passes_light(obj)) {
						paint_pane();
						return;
					}
					// A closed door fills its wall line: same marks as the wall.
					if ((shell || !Fast_info(obj->get_shapenum()).is_door() || obj->is_closed_door())
						&& (NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj))) {
						// A wall facing no outdoors still has a flat TOP, and a top
						// has no viewer side: stamp Shell_top with both faces left clear.
						// A non-shell wall is interior all round, so every face looks
						// onto a room somewhere; ring_side_dist finds which.  Leaving
						// such a face on Shell_face would make it all-sides and light it
						// from the OUTSIDE.
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
				// The verdict comes from the ROOF MAP beyond its south/east faces, not
				// faces_exterior -- that reasons from ground-level geometry and calls these
				// walls interior, after which they would read as Face_south/Face_east and light
				// from the room underneath.
				// A course that another solid continues straight up is part of that
				// wall's face even though its own top falls short of the ceiling --
				// which is how the half-wall under a window is built.  Judged per
				// SHAPE, Object_is_wall_face calls it a crate on a deck, and the
				// splat then masks it out of the facade around it.
				// Only for pieces that test REJECTS: a wall already reaching the ceiling has
				// the roof itself continuing its column, and certifying at the roof's height
				// would answer about the sky and certify every interior wall as facade.
				// An open shutter LEAF keeps the PANE mark (Shell_face + Kind::Ground), the one
				// classification both lights reach: the indoor one because pane_px
				// skips the veto block, the outdoor one through the shell dome.
				const bool       shape_face  = NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj);
				const Tile_coord utile       = obj->get_tile();
				const int        uwtop       = obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height();
				const bool       wall_column = !shape_face
										 && (lowest_cover_at(utile.tx, utile.ty, uwtop) == uwtop
											 || column_continued_at(utile.tx, utile.ty, uwtop));
				// Certify the two visible faces SEPARATELY, from the roof map at
				// this sprite's own height.  Handing both the shell's all-side
				// arrivals would light the inside face of a west wall -- the face
				// the room shows the camera -- from a light standing outside it.
				// faces_exterior cannot supply the bits: it reasons from
				// ground-level geometry and a storey up that answer is wrong.
				uint32_t up_s_bits = 0;
				uint32_t up_e_bits = 0;
				uint32_t up_s_open = 0;
				uint32_t up_e_open = 0;
				uint32_t up_s_bur  = 0;
				uint32_t up_e_bur  = 0;
				// Open air on the sides the camera CANNOT see.  Only the facade
				// verdict reads these: a west or north wall shows the room its
				// S/E sides front, so its top would never qualify as shell from
				// the two camera-facing sides alone.
				bool up_far_open = false;
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
									const Shape_info& oi = Fast_info(o->get_shapenum());
									if (!oi.is_solid() || o->get_lift() >= uwtop || o->get_lift() + oi.get_3d_height() <= uz
										|| !o->get_footprint().has_world_point(wtx, wty)) {
										continue;
									}
									if (oi.is_floor(o->get_framenum()) || oi.is_roof(o->get_framenum()) || oi.is_door()
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
						// Roofed above THIS storey: from the ground, an open deck's railing beside an upper wall passed as a shelf.
						if (b < uwtop && furniture_only(tx, ty) && tile_roofed_at(tx, ty, uz)) {
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
					// Open at the wall's OWN storey, the slab stamp's deck verdict: asked from the
					// ground, the outdoor deck a wall fronts would roof it and the top would go
					// interior.
					auto far_open_at = [&](int tx, int ty) {
						return side_of(tx, ty) == 0;
					};
					for (int i = 0; i < ufp.w && i < 32 && !up_far_open; ++i) {
						up_far_open = far_open_at(ufp.x + i, ufp.y - 1);
					}
					for (int i = 0; i < ufp.h && i < 32 && !up_far_open; ++i) {
						up_far_open = far_open_at(ufp.x - 1, ufp.y + i);
					}
					// A tile buried against neighbouring mass is neither room nor
					// street, but it still DRAWS, and uncertified it keeps the facade
					// default Shell_face -- black from inside under every light, and
					// most of a wall line reads buried (its own continuation).  One
					// room tile anywhere makes the piece interior masonry along its
					// whole length.
					// With NO room tile it stays uncertified on purpose: buried on
					// every side is no face at all and belongs to the storey mark
					// below.  Calling it open air instead would make a junction piece
					// facade, lit through the wall from outside.
					if ((up_s_bits | up_e_bits) != 0) {
						up_s_bits |= up_s_bur;
						up_e_bits |= up_e_bur;
					}
				}
				// The GATE is whether either visible face got certified at all.
				// Buried on both sides is no face at all, so fall through to the storey mark.
				// `up_far_open` counts: a west or north wall buried against its own
				// neighbours on both visible sides has no S/E bit at all, and without it
				// those pieces would fall through to the storey mark, a black gap mid-run
				// where the wall is split into 1x1 sprites.
				const bool face_certified = (up_s_bits | up_e_bits | up_s_open | up_e_open) != 0 || up_far_open;
				// A low piece on the building's edge (a sill whose window shape is gone): a camera-facing side open to
				// the air at its height, the opposite side roofed -- the wall line's outer masonry, not furniture on
				// the storey's floor, whose storey mark every light in the room below would light like a gallery floor.
				// Only under the building's own roof: a mountain top beside the hall (open sky over it) fronts the same
				// air and roofed hall, but it is terrain, and as shell its top would take the face path's cut cone.
				const bool boundary_piece = [&] {
					const TileRect ufp  = obj->get_footprint();
					const int      utop = obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height();
					for (int i = 0; up_e_open != 0 && i < ufp.h && i < 32; ++i) {
						if ((up_e_open >> i) & 1u && tile_roofed_at(ufp.x - 1, ufp.y + i, 0)
							&& lowest_cover_at(ufp.x + ufp.w - 1, ufp.y + i, utop) < 31) {
							return true;
						}
					}
					for (int i = 0; up_s_open != 0 && i < ufp.w && i < 32; ++i) {
						if ((up_s_open >> i) & 1u && tile_roofed_at(ufp.x + i, ufp.y - 1, 0)
							&& lowest_cover_at(ufp.x + i, ufp.y + ufp.h - 1, utop) < 31) {
							return true;
						}
					}
					return false;
				}();
				if (!gwin->is_in_dungeon() && (shape_face || wall_column || boundary_piece)
					&& (!Fast_info(obj->get_shapenum()).is_door() || obj->is_closed_door()) && face_certified) {
					// A PANE (open shutter, grate, glass) glows with whatever is
					// behind it, which the splat keys on Shell_face + Kind::Ground.  Certifying
					// its faces interior overwrote that with Face_east on the face band
					// and the storey top mark on the top band -- both dark to the
					// light outside, split down the middle of one sprite.
					if (NaturalLight::Object_passes_light(obj)) {
						paint_pane();
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
					const int up_storey = storey_of(obj->get_lift());
					// A piece carrying another course on its top (wall_column) shows
					// a SILL, not a crown: light reaches it only through whatever
					// caps it, so an open shutter or glass lights it and a closed
					// one leaves it on the storey mark.
					const bool up_sill_lit   = !wall_column || opening_above_at(utile.tx, utile.ty, uwtop);
					const bool up_fronts_air = (up_s_open | up_e_open) != 0 || up_far_open;
					const bool up_facade     = up_sill_lit && up_fronts_air;
					// A sill under a CLOSED shutter takes no light THROUGH the window,
					// but it is still exterior masonry facing the street.  Shell_face is lit by
					// an outdoor light and vetoed by an indoor one, which is exactly that
					// split; Shell_top would set top_px and light it from inside as well.
					// CAMERA-FACING sides only, unlike the top verdict above: a sill
					// is seen as a face, and a north or west one is on the building's
					// far side, where lighting it put a bright ledge the viewer
					// should never see.
					const bool up_sill_shell = !up_facade && (up_s_open | up_e_open) != 0;
					// The FACES are a separate verdict from the top: a top has no viewer
					// side and may be shell from the far side, but a face the camera
					// sees is only shell where a CAMERA-FACING side fronts open air.
					// Assumed true, a triangle buried on both sides would fall through
					// every branch and keep the Shell_face base, which the shell dome
					// lights.
					const bool up_shell    = (up_s_open | up_e_open) != 0;
					const int  up_top_mark = up_facade                            ? Mask::Shell_top
											 : up_sill_shell                      ? Mask::Shell_face
											 : (upper_interior && up_storey >= 1) ? Mask::Slab + up_storey
																				  : Mask::Tall + up_storey;
					paint_shell_face(true, up_s_bits, up_e_bits, up_shell, true, up_s_open, up_e_open, up_top_mark, up_far_open);
					return;
				}
				tall_exterior = true;
				tall_storey   = storey_of(obj->get_lift());
			} else if (
					!roof_like && top >= 5 && !open_sky_above(top) && !capped_open_sky(top)
					&& !gwin->is_in_dungeon()
					// A door is never the viewer's floor (the branch below takes no doors): it stays shell.
					&& (Fast_info(obj->get_shapenum()).is_door() || !(top < ceiling_z && top <= av_lift + 1))) {
				// Ground shell walls below the render skip (see the branch above).
				const bool shell = faces_exterior(
						true, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
				if (shell && Fast_info(obj->get_shapenum()).is_door() && !obj->is_closed_door()) {
					return;
				}
				// Same rule as "upper-shell-face": a PANE glows with what is behind it, which the splat keys on
				// Shell_face + Kind::Ground.  The generic call below certifies its faces interior and stamps a separate
				// top mark, so one shutter sprite comes out in two differently-masked bands, and the sill under it
				// keeps a top the torch cannot reach.
				// Shell or not, a pane is one surface (a shutter in the wall a neighbour's eaves overhang).  Not a door
				// inside the house: closed, it is a partition whose faces the rooms either side light.
				if (NaturalLight::Object_passes_light(obj) && (shell || !Fast_info(obj->get_shapenum()).is_door())) {
					paint_pane();
					return;
				}
				if ((shell || !Fast_info(obj->get_shapenum()).is_door() || obj->is_closed_door())
					&& (NaturalLight::Object_is_wall_face(obj) || NaturalLight::Object_passes_light(obj))) {
					paint_shell_face(true, shell_s_bits, shell_e_bits, shell, true, shell_s_open, shell_e_open);
					return;
				}
			}
			// Not chained: a covered non-wall block falls through the shell branch above into its deck top here.
			if (!tall_exterior && !roof_like && top >= 5 && top <= std::max(ceiling_z, 5)
				&& (open_sky_above(top) || capped_open_sky(top) || (top < ceiling_z && top <= av_lift + 1))
				&& !gwin->is_in_dungeon() && Fast_info(obj->get_shapenum()).is_solid()
				&& !Fast_info(obj->get_shapenum()).is_floor(obj->get_framenum()) && !Fast_info(obj->get_shapenum()).is_door()
				&& !obj->is_dragable() && !Fast_info(obj->get_shapenum()).is_light_source() && art_fills_box()) {
				// A lower structure's exposed wall (gate passage side wall): its flat
				// TOP is deck level, storey-gated like the roof beside it, or the
				// ground light filling the passage below paints it in spots.
				// COVERED under the viewer's ceiling it is the floor of the viewer's storey (a room
				// built on 5-high blocks): the interior Slab mark, as the floor tiles beside it.
				const int  ts       = storey_of(top);
				const bool top_slab = ts >= 1 && !open_sky_above(top) && !capped_open_sky(top);
				// Hiding the roof over the viewer changes what is drawn, not what a face is: street faces stay facade.
				uint32_t fx_s_open = 0;
				uint32_t fx_e_open = 0;
				if (top_slab && NaturalLight::Object_is_wall_face(obj)) {
					faces_exterior(true, nullptr, nullptr, nullptr, nullptr, &fx_s_open, &fx_e_open);
				}
				frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_clear);
				// From the footprint, as the kind mask's Object_top: wall art rarely fills its box.
				// Art overhanging up/left of the top is outline over the neighbour's top, not a face.
				const TileRect wfp    = obj->get_footprint();
				const int      top_x1 = sx - 4 * top;
				const int      top_y1 = sy - 4 * top;
				const int      top_x0 = std::min(top_x1 - wfp.w * c_tilesize + 1, sx - frame->get_xleft());
				const int      top_y0 = std::min(top_y1 - wfp.h * c_tilesize + 1, sy - frame->get_yabove());
				const TileRect top_rect(top_x0, top_y0, top_x1 - top_x0 + 1, top_y1 - top_y0 + 1);
				{
					Image_buffer::ClipRectSave clipsave(roof_light_mask);
					const TileRect             r = top_rect.intersect(clipsave.Rect());
					if (r.w > 0 && r.h > 0) {
						roof_light_mask->set_clip(r.x, r.y, r.w, r.h);
						frame->paint_rle_transformed(roof_light_mask, sx, sy, top_slab ? roof_slab[ts] : roof_tall[ts]);
					}
				}
				if (light_kind_mask) {
					Image_buffer::ClipRectSave ksave(light_kind_mask);
					const TileRect             kr = top_rect.intersect(ksave.Rect());
					if (kr.w > 0 && kr.h > 0) {
						light_kind_mask->set_clip(kr.x, kr.y, kr.w, kr.h);
						paint_kind_ground();
					}
				}
				// Faces carry their side and a foot inside their own wall cell (a block's raw-pixel foot
				// lands up to 2.5 tiles up-left, in the passage), so each reads only the ground it faces.
				{
					const int                             bx = sx - frame->get_xleft();
					const int                             by = sy - frame->get_yabove();
					const int                             bw = frame->get_width();
					const int                             bh = frame->get_height();
					static std::unique_ptr<Image_buffer8> wscratch;
					if (!wscratch || static_cast<int>(wscratch->get_width()) < bw
						|| static_cast<int>(wscratch->get_height()) < bh) {
						const int wsw = std::max(bw, wscratch ? static_cast<int>(wscratch->get_width()) : 0);
						const int wsh = std::max(bh, wscratch ? static_cast<int>(wscratch->get_height()) : 0);
						wscratch      = std::make_unique<Image_buffer8>(wsw, wsh);
					}
					wscratch->fill8(0, bw, bh, 0, 0);
					frame->paint_rle_transformed(wscratch.get(), frame->get_xleft(), frame->get_yabove(), roof_set);
					Image_buffer::ClipRectSave mclip(roof_light_mask);
					const TileRect             mr    = mclip.Rect();
					unsigned char*             mbits = roof_light_mask->get_bits();
					const int                  mlw   = static_cast<int>(roof_light_mask->get_line_width());
					const unsigned char*       wbits = wscratch->get_bits();
					const int                  wlw   = static_cast<int>(wscratch->get_line_width());
					const bool                 feet  = light_kind_mask && !light_foot_dx.empty();
					const int                  fW    = feet ? static_cast<int>(light_kind_mask->get_width()) : 0;
					const int                  fH    = feet ? static_cast<int>(light_kind_mask->get_height()) : 0;
					const int                  f0x   = sx + 4 * obj->get_lift();
					const int                  f0y   = sy + 4 * obj->get_lift();
					const int                  zpx   = 4 * Fast_info(obj->get_shapenum()).get_3d_height();
					for (int v = 0; v < bh; ++v) {
						const int py = by + v;
						if (py < mr.y || py >= mr.y + mr.h) {
							continue;
						}
						for (int u = 0; u < bw; ++u) {
							const int px = bx + u;
							if (px < mr.x || px >= mr.x + mr.w || wbits[static_cast<size_t>(v) * wlw + u] == 0) {
								continue;
							}
							if (top_rect.has_point(px, py)) {
								// Deck ground: its foot is the true ground 4px per z down-right, not a wall block's raw pixel.
								if (feet && px < fW && py < fH) {
									const int fx  = std::clamp(px + 4 * top, f0x - wfp.w * c_tilesize + 1, f0x);
									const int fy  = std::clamp(py + 4 * top, f0y - wfp.h * c_tilesize + 1, f0y);
									const int ddx = std::clamp(fx - px, -Foot_bias, Foot_bias - 1);
									const int ddy = std::clamp(fy - py, -Foot_bias, Foot_bias - 1);
									light_foot_dx[static_cast<size_t>(py) * fW + px] = static_cast<unsigned char>(ddx + Foot_bias);
									light_foot_dy[static_cast<size_t>(py) * fW + px] = static_cast<unsigned char>(ddy + Foot_bias);
								}
								continue;
							}
							const int du = f0x - px;
							const int dv = f0y - py;
							const int s  = std::min(du, dv);
							// Art may overhang the face plane by a pixel.
							if (s < -1 || s > zpx) {
								continue;
							}
							const bool south = dv <= du;
							// Only this wall's own face plane, and only pixels no other surface has claimed.
							unsigned char& mpx = mbits[static_cast<size_t>(py) * mlw + px];
							if ((south ? du - dv > wfp.w * c_tilesize : dv - du > wfp.h * c_tilesize) || mpx != Mask::Clear) {
								continue;
							}
							const int  fn   = south ? wfp.w : wfp.h;
							const int  fi   = fn - 1 - std::clamp((south ? du - dv : dv - du) >> 3, 0, fn - 1);
							const bool open = fi < 32 && ((south ? fx_s_open : fx_e_open) & (1u << fi)) != 0;
							mpx = static_cast<unsigned char>(open ? Mask::Shell_face : Mask::Low_face + (south ? 0 : 1));
							if (feet && px < fW && py < fH) {
								// 6px in from the face plane: inside the owner cell with or without the lattice's 3px shift.
								const int fx  = south ? std::clamp(px + s, f0x - wfp.w * c_tilesize + 1, f0x - 4) : f0x - 6;
								const int fy  = south ? f0y - 6 : std::clamp(py + s, f0y - wfp.h * c_tilesize + 1, f0y - 4);
								const int ddx = std::clamp(fx - px, -Foot_bias, Foot_bias - 1);
								const int ddy = std::clamp(fy - py, -Foot_bias, Foot_bias - 1);
								light_foot_dx[static_cast<size_t>(py) * fW + px] = static_cast<unsigned char>(ddx + Foot_bias);
								light_foot_dy[static_cast<size_t>(py) * fW + px] = static_cast<unsigned char>(ddy + Foot_bias);
							}
						}
					}
				}
				return;
			} else if (
					!roof_like && top < 5 && !open_sky_above(top) && Fast_info(obj->get_shapenum()).is_solid()
					&& !Fast_info(obj->get_shapenum()).is_door() && !obj->is_dragable() && !gwin->is_in_dungeon()) {
				// Low shell trim (battlement pieces beside a window), as outside.
				// Door anchors excluded: a threshold step in front of the doorway
				// is part of the fan's ground, not the shell.
				// Stamped through paint_shell_face so its faces get the same
				// Shell_top/Face_south/Face_east the wall around it gets.  Gated on faces_exterior it
				// would fall through for a building the avatar is not in, keep a clear mask and drink
				// the z-blind field.
				const bool shell = faces_exterior(
						false, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
				// Low TRIM sits on the wall line; a table or a torch bracket is furniture standing in
				// the room, and painting it as shell art would give it Face_south/Face_east +
				// Kind::Object, so wall_obj would light it as a wall face.  The two are told apart by
				// the shell verdict itself, and a piece that falls through stays Kind::Object and
				// takes object_alpha at its OWN tile, not the z-blind field.
				if (shell && !Fast_info(obj->get_shapenum()).is_light_source()) {
					// mark_top = false: a sill's flat surface sits INSIDE the room, so it takes
					// the room like its faces.  As an exposed wall top (Shell_top) it would draw
					// all-side arrivals and light from the street, and on a low piece that band
					// is most of the sprite.
					// EXCEPT on a sill: a low piece with an OPENING (window, glass, shutter) in
					// the course resting on it.  Unmarked, its band is Shell_face +
					// Kind::Object, which is neither pane_px nor top_px nor face_px under an
					// interior light, so it falls into the veto block and is cut -- the torch in
					// the room would never reach its own sills.
					// Shell_top sets top_px, which skips that block; the street view is
					// unchanged, since shell_dome already covers both shell marks alike.
					const Tile_coord ltile    = obj->get_tile();
					const bool       sill_top = opening_above_at(
                            ltile.tx, ltile.ty, obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height());
					paint_shell_face(true, shell_s_bits, shell_e_bits, shell, sill_top, shell_s_open, shell_e_open);
					return;
				}
			}
		} else {
			const bool is_roof_shape = Fast_info(obj->get_shapenum()).is_roof(obj->get_framenum());
			roof_like                = is_roof_shape || obj->get_lift() >= 5;
			const int top            = obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height();
			if (roof_like && !is_roof_shape && open_sky_above(top)) {
				// Object on an open-sky deck: Tall + storey, as in the inside
				// branch; a cap on a solid wall with no floor-roof at its base
				// stays a whole unit.
				tall_exterior = true;
				if (!solid_below() || floor_at_base()) {
					tall_storey = storey_of(obj->get_lift());
				}
			} else if (roof_like && !is_roof_shape) {
				// An upper storey's wall line seen from outside, as the ground floor's (below): a PANE glows with the
				// light behind it, and a shell piece carrying an opening is a SILL whose top the room's light crosses.
				// As Tall + storey both would stay dark to the room's own lights.
				if (!gwin->is_in_dungeon() && Fast_info(obj->get_shapenum()).is_solid() && !obj->is_dragable()
					&& !Fast_info(obj->get_shapenum()).is_light_source()) {
					const bool shell = faces_exterior(
							true, &shell_s_in, &shell_e_in, &shell_s_bits, &shell_e_bits, &shell_s_open, &shell_e_open);
					if (NaturalLight::Object_passes_light(obj) && (shell || !Fast_info(obj->get_shapenum()).is_door())) {
						paint_pane();
						return;
					}
					const Tile_coord ltile = obj->get_tile();
					if (shell
						&& opening_above_at(ltile.tx, ltile.ty, obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height())) {
						paint_shell_face(true, shell_s_bits, shell_e_bits, shell, true, shell_s_open, shell_e_open);
						return;
					}
				}
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
					const Shape_info& winfo = Fast_info(obj->get_shapenum());
					// Light-passing wall pieces (window walls, grates) are Shell_face like
					// any covered wall: their glass band lights via the veto's glass rule,
					// never the z-blind field (which would wash the wall under the glass).
					// A CLOSED door is shell as well -- left unmarked it would stay clear
					// and drink the room's field through itself.  An OPEN leaf keeps
					// glowing with the light behind it.
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
					!roof_like && !open_sky_above(top) && Fast_info(obj->get_shapenum()).is_solid()
					&& !Fast_info(obj->get_shapenum()).is_door() && !obj->is_dragable()) {
				// A LOW solid under cover (battlement trim beside a window):
				// part of the wall shell too -- left clear, its pixels z-blind
				// sample the interior field (the lit strip beneath a window).
				// A LIGHT SOURCE is not shell: Shell_face is unconditionally dark and a
				// torch at a doorway masked itself out.
				if (!Fast_info(obj->get_shapenum()).is_light_source()) {
					frame->paint_rle_transformed(roof_light_mask, sx, sy, roof_face);
					return;
				}
			}
		}
		{
		}
		// A tall EXTERIOR piece with OPEN SKY above it is outdoors -- a battlement
		// on the castle wall -- not an upper floor.  Giving it the slab mark while
		// the viewer is inside also gives it the slab's exemption in the veto
		// block (slab_s < light_top_storey admits "the upper room's own floor"),
		// so a torch in the room below would light the battlements through the
		// wall.  The slab path already draws this distinction with deck_outdoors.
		const bool tall_slab = upper_interior && tall_storey >= 1
							   && !open_sky_above(obj->get_lift() + Fast_info(obj->get_shapenum()).get_3d_height());
		frame->paint_rle_transformed(
				roof_light_mask, sx, sy,
				tall_exterior ? (tall_slab ? roof_slab[tall_storey] : roof_tall[tall_storey])
							  : (roof_like ? roof_set : roof_clear));
	}

	// Half-size in tiles of a light's room grid: the radius plus one tile of
	// rounding slack and six covering the wall-top anchor shift (mask stamps
	// draw up to 4*roof_z px up-left of the tiles), so a fill reaching the grid
	// edge does not cut off the dome's south/east fringe.
	int Light_grid_rt(int radius) {
		return radius / c_tilesize + 7;
	}

	// How far (px) from the light's screen position its pool or a spill can reach: a spill lands up to rt
	// tiles away and reaches as far again.
	int Light_screen_reach(Game_object* light_obj, int brightness) {
		const int radius = Light_radius(brightness);
		const int elev   = (Fast_info(light_obj->get_shapenum()).get_3d_height() * c_tilesize) / 2;
		return 2 * std::max(radius + elev, Light_grid_rt(radius) * c_tilesize) + 2 * c_tilesize;
	}

	// The brightness a party member's carried lights give its pool: the STRONGEST held light.
	// Actor::get_light_source sums them, which would grow the radius with every extra torch.
	int Carried_light_brightness(Actor* carrier) {
		const int carried   = carrier->get_light_source();
		int       strongest = 0;
		for (int spot = 0; spot < 18; ++spot) {
			if (const Game_object* held = carrier->get_readied(spot)) {
				strongest = std::max(strongest, held->get_info().get_object_light(held->get_framenum()));
			}
		}
		return strongest > 0 ? std::min(strongest, carried) : carried;
	}

	// Queue a light source for this frame's light layers: its own pool (`queue_main`) and the spills through the
	// openings its room fill reaches (`queue_spills`).  `moving`: a carried light, following its carrier every
	// frame.  The pool is centred on the source's foot tile; its height only rounds the dome falloff.
	void Queue_light(Game_object* light_obj, int brightness, bool moving, bool queue_main, bool queue_spills) {
		Game_window* const gwin      = Game_window::get_instance();
		Actor* const       viewer    = gwin->get_main_actor();
		const bool         inside    = gwin->is_main_actor_inside();
		const int          radius    = Light_radius(brightness);
		const int          tier      = Light_tier(brightness);
		const int          elevation = (Fast_info(light_obj->get_shapenum()).get_3d_height() * c_tilesize) / 2;
		const int          rt        = Light_grid_rt(radius);
		const Tile_coord   ltile     = light_obj->get_tile();
		// Under a roof the light keeps roof pixels dark; in the open it lights nearby house roofs.
		const bool under_roof = Light_beneath_roof(light_obj);
		// The wall ring is the window/wall-line light of the veto field.  It is off only for a light under a roof
		// while the viewer is inside another ROOM: there it peeks out as a stray glow on the interior-view wall
		// faces.  Outside viewers keep it, or the wash a light-passing window wall shows (clear pixels, z-blind)
		// loses its wall-line cells and drifts off the window (inside/outside parity).  Two tiles are in one room
		// when the same ceiling closes them, so in a tall hall the Avatar on the floor and a wall torch one storey
		// up share the room.  The Avatar's own torch has its ring off only when he is outside under a roof, where
		// it would show as a bright seam.
		bool light_walls = !inside || !under_roof;
		if (light_obj == viewer) {
			light_walls = inside || !under_roof;
		} else if (!light_walls && viewer != nullptr) {
			const Tile_coord avt = viewer->get_tile();
			light_walls          = Light_room_roof_z(gwin->get_map(), ltile.tx, ltile.ty, ltile.tz)
						  == Light_room_roof_z(gwin->get_map(), avt.tx, avt.ty, avt.tz);
		}
		std::vector<unsigned char> lit;
		std::vector<unsigned char> ring;
		std::vector<Light_spill>   spills;
		Build_light_shadow_grid(light_obj, rt, lit, spills, light_walls, &ring);
		if (queue_main) {
			int sx = 0;
			int sy = 0;
			gwin->get_shape_location(light_obj, sx, sy);
			Light_render_info& lr = gwin->get_light_layers().add_render(
					sx, sy, radius, tier, elevation, rt, ltile.tx, ltile.ty, ltile.tz, std::move(lit), under_roof, 0, false, 100, 0,
					moving, std::move(ring));
			lr.deck_spill_storeys = Light_deck_spill_storeys(spills, ltile.tz);
		}
		if (queue_spills) {
			Queue_spill_renders(ltile, radius, tier, elevation, spills, moving);
		}
	}

	// Is the viewer inside an enclosure no light can leave or enter: no light-passing shape in his chunk and no
	// passable gap in its walls?
	bool Viewer_enclosure_sealed() {
		Game_window* const gwin   = Game_window::get_instance();
		Actor* const       viewer = gwin->get_main_actor();
		if (viewer == nullptr || !gwin->is_main_actor_inside()) {
			return false;
		}
		return !Chunk_find_light_passes_through(viewer->get_chunk()) && !Enclosure_open_to_outside(viewer->get_tile());
	}

	// Queue a spill glow (Light_layers::add_render) through each opening
	// the light at `ltile` reached: remaining radius, continued falloff,
	// gated by its own fan-shaped spill grid.
	void Queue_spill_renders(
			const Tile_coord& ltile, int radius, int tier, int elevation, const std::vector<Light_spill>& spills, bool moving) {
		Game_window* const gwin = Game_window::get_instance();
		// The light's own spills, then the pools its fans let into other buildings (those spawn no more).
		std::vector<Light_spill> work(spills.begin(), spills.end());
		const size_t             own_count = work.size();
		for (size_t wi = 0; wi < work.size(); ++wi) {
			const Light_spill spill = work[wi];
			const Tile_coord& sp    = spill.tile;
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
			// Light passing in through an upper window pools unconstrained (no cone), like through a ground door.
			// So does light landing on an upper storey's deck: several coned spills from one hall lamp left
			// dark gaps between their fans on that floor (around the stairwell's table and shelves).
			if (spill.opening.tx == Light_spill_no_cone || Light_spill_upward(spill, ltile.tz)) {
				cdx = 0;
				cdy = 0;
			}
			const bool pos_hi = cdy > 0 || cdx < 0;
			const int  cmouth = (pos_hi ? spill.mouth_neg : spill.mouth_pos) | ((pos_hi ? spill.mouth_pos : spill.mouth_neg) << 8);
			// Walls in the grid for any viewer: the shell is stamped from
			// inside too, so the ring cannot wash interior faces.
			std::vector<Light_spill> arrivals;
			Build_spill_shadow_grid(sp, srt, slit, true, &sring, cdx, cdy, cmouth, wi < own_count ? &arrivals : nullptr);
			for (Light_spill a : arrivals) {
				// On from the fan's apex, through that opening as well as the first.
				a.path    = std::max(ltile.distance_2d(sp), spill.path) + a.path;
				a.percent = std::max(1, (spill.percent * a.percent + 50) / 100);
				work.push_back(a);
			}
			int ssx = 0;
			int ssy = 0;
			gwin->get_shape_location(spill.deck_z >= 0 ? Tile_coord(sp.tx, sp.ty, spill.deck_z) : sp, ssx, ssy);
			Light_render_info& lr = gwin->get_light_layers().add_render(
					ssx, ssy, spill_radius, tier, elevation, srt, sp.tx, sp.ty, sp.tz, std::move(slit), false, spill_dist, true,
					spill.percent, spill.floor, moving, std::move(sring), spill.deck_z, cdx, cdy, cmouth);
			if (Light_spill_upward(spill, ltile.tz)) {
				lr.from_source = true;
				lr.source_dx   = ltile.tx - sp.tx;
				lr.source_dy   = ltile.ty - sp.ty;
			}
			// Light in through an opening: as bright as its source is AT the opening, fading on the source's own
			// curve with the distance from it (dist_bias), centred on the opening and confined to its fan.
			if (spill.opening.tx == Light_spill_no_cone) {
				lr.pool_shape = true;
				lr.pool_wdx   = Tile_coord::delta(sp.tx, spill.opening.ty);
				lr.pool_wdy   = Tile_coord::delta(sp.ty, spill.opening.tz);
				lr.pool_nx    = spill.inward_nx;
				lr.pool_ny    = spill.inward_ny;
			}
		}
	}

	// Splat one light into `cov` and the layer's pixels; returns the splat's
	// unclamped screen bounds.  The clip window restricts the pixels written.
	TileRect Splat_light(
			const Splat_frame& f, const Light_render_info& lr, unsigned char* covp, int cx0, int cy0, int cx1, int cy1) {
		Game_window* const             gwin               = Game_window::get_instance();
		Game_map* const                map                = gwin->get_map();
		const int                      W                  = f.W;
		const int                      H                  = f.H;
		unsigned char* const           dstpix             = f.dstpix;
		const int                      dst_lw             = f.dst_lw;
		const unsigned char* const     srcpix             = f.srcpix;
		const int                      src_lw             = f.src_lw;
		const unsigned char* const     roofpix            = f.roofpix;
		const int                      roof_lw            = f.roof_lw;
		const unsigned char* const     kindpix            = f.kindpix;
		const int                      kind_lw            = f.kind_lw;
		const unsigned char* const     footdxp            = f.footdxp;
		const unsigned char* const     footdyp            = f.footdyp;
		const int                      foot_lw            = f.foot_lw;
		const std::vector<Sprite_box>& light_sprite_boxes = f.sprites;
		const bool                     inside             = f.inside;
		const int                      av_fx              = f.av_fx;
		const int                      av_fy              = f.av_fy;
		const int                      skip_above_actor   = f.skip_above_actor;
		// Masking policy: see the `inside` comment in Light_layers::build; the per-pixel
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
		int       csx       = lr.sx - emit;
		int       csy       = lr.sy - emit;
		int       dome_elev = lr.elevation;
		// A light above its room's floor (a tall hall's wall torch) is that much farther from the floor it pools on.
		if (!lr.is_spill) {
			const int rfz = (Light_room_floor(map, Tile_coord(lr.ltx, lr.lty, lr.ltz), lr.ltz).tz / 5) * 5;
			if (rfz < (lr.ltz / 5) * 5) {
				dome_elev = lr.elevation + 4 * (lr.ltz - rfz);
			}
		}
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
		// The storey the light's fill floods from (see Light_flood_z).
		const int light_floor_storey = Light_flood_z(map, Tile_coord(lr.ltx, lr.lty, lr.ltz)) / 5;
		// A light whose room is not part of this render must not
		// splat: a veto light on a cut-away storey (viewer below
		// it) would paint only its shaft band onto the room below;
		// a roofed spill whose ceiling IS drawn (viewer above its
		// storey) is hidden and would paint only its z-shifted
		// wall-ring seam over the slab sides / exterior ground.
		// A deck spill whose deck is cut away likewise: all still drawn are the ground walls' tops beneath it.
		bool hidden_room = (lr.mask_roof || (lr.is_spill && lr.ltz > 0)) && lr.ltz >= skip_above_actor;
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
		// Vote over the reached tiles near the light: a single nearest tile can be a wall ledge in the
		// light's band (roof 5 from the ground under a hall's ceiling 11).
		bool cut_hall = false;
		if (hidden_room && !lr.is_spill && !lr.lit.empty()) {
			const int gside  = 2 * lr.rt + 1;
			const int vote_r = std::min(3, lr.rt);
			int       agree = 0, differ = 0;
			for (int gy = lr.rt - vote_r; gy <= lr.rt + vote_r; ++gy) {
				for (int gx = lr.rt - vote_r; gx <= lr.rt + vote_r; ++gx) {
					const int d = std::abs(gx - lr.rt) + std::abs(gy - lr.rt);
					if (d == 0 || d > vote_r || NaturalLight::Grid::Dist(lr.lit[static_cast<size_t>(gy) * gside + gx]) == 0) {
						continue;
					}
					const int otx = Light_tile_norm(lr.ltx - lr.rt + gx);
					const int oty = Light_tile_norm(lr.lty - lr.rt + gy);
					if (NaturalLight::Light_room_roof_z(map, otx, oty, 0)
						== NaturalLight::Light_room_roof_z(map, otx, oty, lr.ltz)) {
						++agree;
					} else {
						++differ;
					}
				}
			}
			if (agree > differ) {
				hidden_room = false;
				cut_hall    = true;
			}
		}
		// An outdoor light keeps its fill for an OUTSIDE viewer too: its glow stops at the walls and openings as
		// the inside view has it (the opening's pools light the room behind), so both views of a doorway agree.
		if (!lr.lit.empty()) {
			grid    = lr.lit.data();
			grid_rt = lr.rt;
			// Anchor at the wall top whenever the light has a real
			// ceiling (a dungeon's rock ceiling counts even though
			// mask_roof is false).  Only a light with none -- an exterior
			// lamp gated while the Avatar is inside, a spill on open
			// ground -- anchors at its own storey floor: the floor+5
			// fallback would slide its field 4px per z up-left off it.
			bool      ceiling = false;
			const int roof_z  = NaturalLight::Light_anchor_roof_z(map, lr.ltx, lr.lty, lr.ltz, lr.rt, lr.lit.data(), &ceiling);
			if (lr.is_spill && ceiling && (lr.cone_dx != 0 || lr.cone_dy != 0)) {
				// Cover that ends one tile out along the fan (an eave, a ledge) is not a room over the pool.
				bool beyond = false;
				NaturalLight::Light_room_roof_z(map, lr.ltx + lr.cone_dx, lr.lty + lr.cone_dy, lr.ltz, &beyond);
				ceiling = beyond;
			}
			int anchor_z = (lr.mask_roof || ceiling) ? roof_z : (lr.ltz / 5) * 5;
			// An inward pool is floor light confined to its fan: anchored at its own floor, the ground pixels read
			// their own cells, not those 4px/z down-right of them -- past the opening, where the fan is dark.
			if (lr.is_spill && lr.pool_shape) {
				anchor_z = (lr.ltz / 5) * 5;
			}
			if (cut_hall) {
				// The walls only rise to the render cut: a lattice anchored higher maps the ground behind
				// them onto the hall's cells.
				anchor_z = std::min(anchor_z, ((skip_above_actor + 4) / 5) * 5);
			}
			field_anchor_z = anchor_z;
			// Not an opening's pool: the room it lights may be seen from outside up to where the roof covers it
			// (the drawn roof's own pixels stay dark to it).
			if (lr.is_spill && ceiling && roof_z < skip_above_actor && !lr.pool_shape) {
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
			// Not the room he stands in, though: on the first steps of its stairwell he is under the higher ceiling
			// of the well, yet still in the room.  Nor, from upstairs, a room whose light climbs its stairwell to
			// his storey: he looks down the well into it.
			const bool avatar_in_room = [&] {
				const Actor* av = gwin->get_main_actor();
				if (av == nullptr) {
					return false;
				}
				const int avs = av->get_lift() / 5;
				if (avs > light_floor_storey && avs < 30 && ((lr.deck_spill_storeys >> avs) & 1) != 0) {
					return true;
				}
				if (avs != light_floor_storey) {
					return false;
				}
				const Tile_coord at   = av->get_tile();
				const int        side = 2 * lr.rt + 1;
				const int        gx   = Tile_coord::delta(lr.ltx, at.tx) + lr.rt;
				const int        gy   = Tile_coord::delta(lr.lty, at.ty) + lr.rt;
				return gx >= 0 && gy >= 0 && gx < side && gy < side && static_cast<size_t>(gy) * side + gx < lr.lit.size()
					   && NaturalLight::Grid::Dist(lr.lit[static_cast<size_t>(gy) * side + gx]) != 0;
			}();
			if (lr.mask_roof && inside && ceiling && roof_z < skip_above_actor && !avatar_in_room) {
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
			// A field never lies below the deck it lights.
			gwin->get_shape_location(Tile_coord(wtx, wty, std::max(anchor_z, lr.spill_deck_z)), grid_fx, grid_fy);
			// Empirically the wall-top anchor lands 1px up-left of the
			// visible interior; nudge the reference back.
			grid_fx += 1;
			grid_fy += 1;
		}
		TileRect wrote(0, 0, 0, 0);
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
			// An inward spill (outside light through an upper window) lands UNDER the room's roof: it is the room's.
			const int spill_floor    = (lr.is_spill && inside && lr.spill_deck_z < 0
                                     && !(lr.spill_floor > 0 && Light_tile_roofed(map, lr.ltx, lr.lty)))
											   ? 0
											   : lr.spill_floor;
			Splat_deck_spill_storeys = lr.is_spill ? 0 : lr.deck_spill_storeys;
			Splat_from_source        = lr.is_spill && lr.from_source;
			Splat_emit_shift         = lr.is_spill ? 0 : lr.elevation;
			Splat_pool_shape         = lr.is_spill && lr.pool_shape;
			Splat_pool_wdx           = lr.pool_wdx;
			Splat_pool_wdy           = lr.pool_wdy;
			Splat_pool_nx            = lr.pool_nx;
			Splat_pool_ny            = lr.pool_ny;
			Splat_source_dx          = lr.source_dx;
			Splat_source_dy          = lr.source_dy;
			NaturalLight::Splat_radial_light(
					covp, dstpix, srcpix, W, H, dst_lw, src_lw, csx, csy, lr.radius, dome_elev, lr.dist_bias, lr.spill_percent,
					roofpix, roof_lw, mask_roof, lr.is_spill, spill_floor, light_top_storey, light_floor_storey, field_anchor_z,
					grid, grid_rt, grid_fx, grid_fy, inside, cx0, cy0, cx1, cy1, kindpix, kind_lw, footdxp, footdyp, foot_lw,
					// No ring in dungeons: they are wholly interior, the
					// classic rendering is correct there (and lights the
					// walls' near strips the face rule would darken).
					!gwin->is_in_dungeon() && lr.ring.size() == static_cast<size_t>(2 * grid_rt + 1) * (2 * grid_rt + 1) * 4
							? lr.ring.data()
							: nullptr,
					av_fx, av_fy, light_sprite_boxes.empty() ? nullptr : light_sprite_boxes.data(),
					static_cast<int>(light_sprite_boxes.size()), lr.spill_deck_z, lr.cone_dx, lr.cone_dy, lr.cone_mouth, lr.ltx,
					lr.lty, &wrote);
		}
		// Match Splat_radial_light's reach: only an ELEVATED spill's
		// dome extends sqrt(r^2 + 2*r*bias), capped at 1.5r.
		const int reach = (lr.is_spill && light_top_storey >= 1 && lr.dist_bias > 0)
								  ? std::min(
											static_cast<int>(std::ceil(std::sqrt(
													static_cast<double>(lr.radius) * lr.radius + 2.0 * lr.radius * lr.dist_bias))),
											(3 * lr.radius) / 2)
								  : lr.radius;
		int       bx0   = csx - reach;
		int       bx1   = csx + reach;
		int       by0   = csy - reach;
		int       by1   = csy + reach;
		if (grid != nullptr) {
			bx0 = std::min(bx0, grid_fx - lr.radius - c_tilesize);
			bx1 = std::max(bx1, grid_fx + lr.radius + c_tilesize);
			by0 = std::min(by0, grid_fy - lr.radius - c_tilesize);
			by1 = std::max(by1, grid_fy + lr.radius + c_tilesize);
		}
		NaturalLight::Reach_extend_box(
				bx0, by0, bx1, by1, light_sprite_boxes.empty() ? nullptr : light_sprite_boxes.data(),
				static_cast<int>(light_sprite_boxes.size()));
		const TileRect box(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
		// The splat's own box can reach further (ground read at its foot below the lattice): rects that
		// miss it leave coverage the cache never clears.
		return wrote.w > 0 ? box.add(wrote) : box;
	}

	// Lift actors and placed light sprites to the light at their feet, in
	// the scratch coverage only (seeded from `cov` on first use).
	void Relight_sprites(
			const Splat_frame& f, const std::vector<Light_render_info>& light_renders, int t, const unsigned char* cov,
			std::vector<unsigned char>& light_scratch_cov, bool& has_moving) {
		Game_window* const         gwin       = Game_window::get_instance();
		Game_map* const            map        = gwin->get_map();
		Actor* const               main_actor = gwin->get_main_actor();
		const int                  W          = f.W;
		const int                  H          = f.H;
		unsigned char* const       dstpix     = f.dstpix;
		const int                  dst_lw     = f.dst_lw;
		const unsigned char* const srcpix     = f.srcpix;
		const int                  src_lw     = f.src_lw;
		const unsigned char* const roofpix    = f.roofpix;
		const int                  roof_lw    = f.roof_lw;
		const unsigned char* const kindpix    = f.kindpix;
		const int                  kind_lw    = f.kind_lw;
		const unsigned char* const footdxp    = f.footdxp;
		const unsigned char* const footdyp    = f.footdyp;
		const int                  foot_lw    = f.foot_lw;
		const bool                 inside     = f.inside;
		static const Xform_palette lift_set   = [] {
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
				if (ot.tx == wtx && ot.ty == wty && ot.tz == lr2.ltz && Fast_info(o->get_shapenum()).is_light_source()) {
					subjects.push_back(o);
					break;
				}
			}
		}
		const size_t sz         = static_cast<size_t>(W) * H;
		const int    paint_skip = gwin->get_render_skip_lift();
		for (Game_object* act : subjects) {
			if (act == nullptr || act->get_flag(Obj_flags::dont_render)) {
				continue;
			}
			// Cut away by the roof cut: the world never paints it, so lifting
			// its silhouette would brighten whatever lies behind it.
			if (act->get_lift() >= paint_skip) {
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
			const bool           is_actor    = act->as_actor() != nullptr;
			const unsigned char* rdcov       = has_moving ? light_scratch_cov.data() : cov;
			int                  foot_a      = 0;
			const int            foot_storey = act->get_lift() / 5;
			const int            wy1         = asy + (is_actor ? c_tilesize / 2 : 2 * c_tilesize);
			const int            wx0         = asx - (is_actor ? c_tilesize : 2 * c_tilesize);
			const int            wx1         = asx + (is_actor ? c_tilesize / 2 : c_tilesize);
			for (int y = std::max(asy - c_tilesize / 2, 0); y <= std::min(wy1, H - 1); ++y) {
				for (int x = std::max(wx0, 0); x <= std::min(wx1, W - 1); ++x) {
					const unsigned char cv2 = rdcov[static_cast<size_t>(y) * W + x];
					// The floor slab of his own storey is his footing too (an upper floor is all slab).
					const int fm = roofpix != nullptr ? roofpix[static_cast<size_t>(y) * roof_lw + x] : 0;
					if (fm != 0 && !Mask::Is_low_face(fm) && !(Mask::Is_slab(fm) && fm - Mask::Slab == foot_storey)) {
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
					// The light's fill is a flood of its own storey's floor: a subject on another storey has no tile in it.
					// A spill come up a stairwell lands over the hole, where the floor below answers: its storey is its own.
					if (Light_flood_z(map, Tile_coord(lr3.ltx, lr3.lty, lr3.ltz)) / 5 != at.tz / 5
						&& !(lr3.from_source && lr3.ltz / 5 == at.tz / 5)) {
						continue;
					}
					int ta = NaturalLight::Light_tile_alpha(
							lr3.lit.empty() ? nullptr : lr3.lit.data(), lr3.rt, lr3.ring.empty() ? nullptr : lr3.ring.data(),
							lr3.ltx, lr3.lty, at.tx, at.ty, lr3.radius, lr3.elevation, lr3.dist_bias, lr3.spill_percent);
					// This pass lifts a sprite to the
					// light at its TILE, with no wall or LOS test, so it can
					// brighten a subject the splat left dark.
					// A from-source (upward) spill lights the tile at its source's own distance, as its splat does.
					// Reach is the fill's; Light_tile_alpha's own distance (landing tile + bias) gives 0 well inside it.
					if (ta > 0 && lr3.pool_shape && !lr3.from_source) {
						ta = static_cast<int>(
								static_cast<float>(ta)
										* Light_pool_fade(
												lr3.radius, lr3.pool_wdx, lr3.pool_wdy, lr3.pool_nx, lr3.pool_ny, at.tx - lr3.ltx,
												at.ty - lr3.lty)
								+ 0.5f);
					}
					const bool src_reached = lr3.from_source && !lr3.lit.empty() && [&] {
						const int side = 2 * lr3.rt + 1;
						const int gx   = at.tx - lr3.ltx + lr3.rt;
						const int gy   = at.ty - lr3.lty + lr3.rt;
						return gx >= 0 && gy >= 0 && gx < side && gy < side && static_cast<size_t>(gy) * side + gx < lr3.lit.size()
							   && Grid::Dist(lr3.lit[static_cast<size_t>(gy) * side + gx]) != 0;
					}();
					if (src_reached) {
						const float ddx  = static_cast<float>(at.tx - (lr3.ltx + lr3.source_dx));
						const float ddy  = static_cast<float>(at.ty - (lr3.lty + lr3.source_dy));
						const float d    = std::sqrt(ddx * ddx + ddy * ddy) * c_tilesize;
						const float e    = static_cast<float>(std::max(lr3.elevation, 0));
						const float full = static_cast<float>(lr3.radius + std::max(lr3.dist_bias, 0));
						const float dome = 1.0f - (d * d + e * e) / (full * full + e * e);
						const float pct  = static_cast<float>(std::min(lr3.spill_percent, 100)) / 100.0f;
						const float pf   = lr3.pool_shape ? Light_pool_fade(
                                                                  lr3.radius, lr3.pool_wdx, lr3.pool_wdy, lr3.pool_nx, lr3.pool_ny,
                                                                  at.tx - lr3.ltx, at.ty - lr3.lty)
														  : 1.0f;
						ta               = dome > 0.0f ? std::min(255, static_cast<int>(255.0f * pct * dome * pf + 0.5f)) : 0;
					}
					if (ta > foot_a) {
						foot_a = ta;
					}
				}
			}
			// An actor is capped at its footing too (light over a sill must not brighten the head above it), so
			// it runs even when its foot is dark; a light sprite with a dark foot has nothing to lift.
			bool cap_only_none = false;
			if (foot_a <= 0 && is_actor) {
				// Nothing of this tier on his box: nothing to cap.
				cap_only_none = true;
				for (int y = std::max(aby0, 0); y < std::min(aby0 + abh, H) && cap_only_none; ++y) {
					const unsigned char* crow = cov + static_cast<size_t>(y) * W;
					for (int x = std::max(abx0, 0); x < std::min(abx0 + abw, W); ++x) {
						if (crow[x] != 0) {
							cap_only_none = false;
							break;
						}
					}
				}
			}
			if (foot_a <= 0 && (!is_actor || cap_only_none)) {
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
				Ordering_info  aord(gwin, act);
				const TileRect abox(abx0, aby0, abw, abh);
				// compare() is 0 both for "no screen overlap" and for the
				// UNDECIDED pair (x apart one way, y the other -- a tree
				// south-west of him): the renderer still paints one of them
				// last, and the canopy covering him must not be skipped here,
				// or his lit silhouette shows through it.  For an undecided
				// pair that does overlap on screen take the deeper tile
				// (greater tx + ty) as the one in front.
				const Tile_coord atile = act->get_tile();
				const int        skip  = gwin->get_render_skip_lift();
				for (Game_object* obj : occ) {
					if (obj == nullptr || obj->as_actor() != nullptr) {
						continue;
					}
					// Cut away above the actor: not painted, so it covers nothing.
					if (obj->get_lift() >= skip) {
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
			// A lamp whose art is mostly covered by a wall face stands behind that wall (in the next room): the
			// world hides it, and lifting its silhouette would glow through the wall in the lamp's shape.  A
			// visible sconce on its wall stamps its own kind over its art.
			if (!is_actor && kindpix != nullptr) {
				int art    = 0;
				int walled = 0;
				for (int y = std::max(aby0, 0); y < std::min(aby0 + abh, H); ++y) {
					const unsigned char* arow = abits + static_cast<size_t>(y - aby0) * aslw;
					for (int x = std::max(abx0, 0); x < std::min(abx0 + abw, W); ++x) {
						if (arow[x - abx0] == 0) {
							continue;
						}
						++art;
						if (kindpix[static_cast<size_t>(y) * kind_lw + x] == Kind::Wall_face) {
							++walled;
						}
					}
				}
				if (art > 0 && walled * 2 > art) {
					continue;
				}
			}
			const int  own_lift = act->get_lift();
			const int  own_f0x  = asx + 4 * own_lift;
			const int  own_f0y  = asy + 4 * own_lift;
			const int  own_xts  = Fast_info(act->get_shapenum()).get_3d_xtiles(act->get_framenum());
			const int  own_yts  = Fast_info(act->get_shapenum()).get_3d_ytiles(act->get_framenum());
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
						const int over_storey = Mask::Is_slab(rm)         ? rm - Mask::Slab
												: Mask::Is_tall(rm)       ? rm - Mask::Tall
												: Mask::Is_tall_south(rm) ? rm - Mask::Tall_south
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
							// wall stands on the lamp's OWN tile -- the feet
							// match.
							continue;
						}
						// An object's top and caps carry their owner's foot too (a halfwall's top over a candle below it).
						// A PANE (a closed door, glass) is stamped with kind Ground but its own foot: a stamped foot
						// that is not the lamp's own means the pane is drawn in front of it (a torch behind a door).
						const size_t pfi        = static_cast<size_t>(y) * foot_lw + x;
						const bool   pane_front = kb == Kind::Ground && roofpix != nullptr
												&& Mask::Is_face(roofpix[static_cast<size_t>(y) * roof_lw + x])
												&& (footdxp[pfi] != Foot_bias || footdyp[pfi] != Foot_bias);
						if (Kind::Is_object(kb) || kb == Kind::Object_top || Kind::Is_cap(kb) || pane_front) {
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
					} else if (is_actor && wcov[ci] > foot_a) {
						// The light the sprite overlaps up-screen is not where he stands: a head over a window
						// sill would take the street's brightness.  He takes his footing's.
						wcov[ci] = static_cast<unsigned char>(foot_a);
					}
				}
			}
		}
	}

	/*
	 *  Light_layers: the natural-light overlay layers of Game_window.
	 */

	Light_layers::Light_layers() = default;

	Light_layers::~Light_layers() = default;

	void Light_layers::clear_renders() {
		light_renders.clear();
	}

	// Is any tier's layer showing?  Game_window::paint then promotes a partial paint to the full window.
	bool Light_layers::any_visible(Game_window& gw) const {
		for (const int handle : light_layer_handles) {
			if (handle >= 0 && gw.win->layer_is_visible(handle)) {
				return true;
			}
		}
		return false;
	}

	// A static light set changed: the caller repaints everything once (see build's full-rebuild path).
	bool Light_layers::take_repaint_pending() {
		const bool pending    = light_repaint_pending;
		light_repaint_pending = false;
		return pending;
	}

	/*
	 *  Prepare the global roof-pixel mask for a world render. Only active when a
	 *  night/dawn/dusk palette is in effect (spatial lights only brighten then);
	 *  at full day it stays inactive and costs nothing. The mask mirrors the
	 *  world ibuf so Light_layers::build can test roof coverage per pixel.
	 */

	void Light_layers::begin_roof_mask(Game_window& gw) {
		roof_light_mask_active = false;
		if (!gw.natural_light) {
			return;
		}
		Flood_cache_frame_begin();
		const int palnum = gw.pal ? gw.pal->get_palette_number() : PALETTE_DAY;
		if (palnum == PALETTE_DAY) {
			return;    // No spatial lighting at full day: no roof mask needed.
		}
		Image_buffer8* ib = gw.win ? gw.win->get_ib8() : nullptr;
		if (!ib) {
			return;
		}
		const int bw = static_cast<int>(ib->get_width());
		const int bh = static_cast<int>(ib->get_height());
		if (!roof_light_mask || static_cast<int>(roof_light_mask->get_width()) != bw
			|| static_cast<int>(roof_light_mask->get_height()) != bh) {
			roof_light_mask = std::make_unique<Image_buffer8>(bw, bh);
		}
		roof_light_mask->fill8(Mask::Clear);
		if (!light_kind_mask || static_cast<int>(light_kind_mask->get_width()) != bw
			|| static_cast<int>(light_kind_mask->get_height()) != bh) {
			light_kind_mask = std::make_unique<Image_buffer8>(bw, bh);
		}
		light_kind_mask->fill8(Kind::Ground);
		light_foot_dx.assign(static_cast<size_t>(bw) * bh, Foot_bias);
		light_foot_dy.assign(static_cast<size_t>(bw) * bh, Foot_bias);
		light_sprite_boxes.clear();
		roof_light_mask_active = true;
	}

	/*
	 *  Update the global roof mask as one object is painted.  Objects paint
	 *  back-to-front, so the topmost shape at each pixel wins: a roof shape -- or
	 *  anything standing at roof level (lift >= 5), which is just as far above an
	 *  interior light -- sets its pixels (kept dark under lights), and any ground
	 *  shape painted over them clears them (e.g. a tree overlapping a roof stays
	 *  lit). The final mask therefore marks exactly the pixels where the visible
	 *  shape sits at or above the roof.
	 */

	void Light_layers::update_roof_mask(Game_object* obj, int sx, int sy) {
		if (!roof_light_mask_active || !roof_light_mask || !obj) {
			return;
		}
		Stamp_surface(
				{roof_light_mask.get(), light_kind_mask.get(), light_foot_dx, light_foot_dy, light_sprite_boxes, light_mask_rects},
				obj, sx, sy);
	}

	/*
	 *  Destroy the natural-light overlay layers and drop any pending
	 *  per-frame light state.
	 */
	void Light_layers::destroy(Game_window& gw) {
		for (int& handle : light_layer_handles) {
			if (handle >= 0) {
				gw.win->destroy_layer(handle);
				handle = -1;
			}
		}
		light_layer_w      = -1;
		light_layer_h      = -1;
		light_layer_palnum = -2;
		light_renders.clear();
		light_mask_rects.clear();
		light_scratch_cov.clear();
		for (int t = 0; t < 3; ++t) {
			light_tier_cov[t].clear();
			light_tier_rects[t].clear();
			light_tier_mask_rects[t].clear();
			light_tier_bounds[t].clear();
			light_tier_anchor_x[t] = 0;
			light_tier_anchor_y[t] = 0;
			light_tier_sig[t]      = 0;
			light_tier_mov_cov[t].clear();
			light_tier_mov_bounds[t].clear();
			light_tier_mov_sig[t] = 0;
		}
	}

	namespace {
		// After a coverage buffer slid by (dx, dy), decide which vacated edge strips must be re-splatted now.  Small
		// steps are deferred: the vacated columns/rows copy their neighbour (the light changes little over a few
		// pixels at the screen edge) and the debt is paid once it reaches `limit` px or the scroll turns back.
		// This avoids re-splatting 1-3 px strips for every light on every scroll step.
		struct Light_strip {
			int x0, y0, x1, y1;
		};

		int Light_scroll_strips(
				unsigned char* buf, int W, int H, int dx, int dy, int& debt_x, int& debt_y, int limit, Light_strip out[4]) {
			int  n    = 0;
			auto axis = [&](int d, int& debt, bool horiz) {
				if (d == 0) {
					return;
				}
				const int  span  = horiz ? W : H;
				const bool flip  = debt != 0 && ((debt > 0) != (d > 0));
				const int  tot   = debt + d;
				auto       strip = [&](int w, bool low) {
                    w = std::min(w, span);
                    if (horiz) {
                        out[n++] = low ? Light_strip{0, 0, w - 1, H - 1} : Light_strip{W - w, 0, W - 1, H - 1};
                    } else {
                        out[n++] = low ? Light_strip{0, 0, W - 1, w - 1} : Light_strip{0, H - w, W - 1, H - 1};
                    }
				};
				if (flip) {
					const int w = std::abs(debt) + std::abs(d);
					strip(w, true);
					strip(w, false);
					debt = 0;
					return;
				}
				if (std::abs(tot) >= limit) {
					strip(std::abs(tot), tot > 0);
					debt = 0;
					return;
				}
				// Edge-extend the vacated band from its inner neighbour.
				const int a = std::abs(d);
				if (a >= span) {
					return;
				}
				if (horiz) {
					for (int y = 0; y < H; ++y) {
						unsigned char* row = buf + static_cast<size_t>(y) * W;
						if (d > 0) {
							std::memset(row, row[a], static_cast<size_t>(a));
						} else {
							std::memset(row + W - a, row[W - a - 1], static_cast<size_t>(a));
						}
					}
				} else {
					const int src = d > 0 ? a : H - a - 1;
					for (int k = 0; k < a; ++k) {
						const int y = d > 0 ? k : H - 1 - k;
						std::memcpy(buf + static_cast<size_t>(y) * W, buf + static_cast<size_t>(src) * W, static_cast<size_t>(W));
					}
				}
				debt = tot;
			};
			axis(dx, debt_x, true);
			axis(dy, debt_y, false);
			return n;
		}
	}    // namespace

	/*
	 *  Queue one light for this frame's light layers.  Returns the entry so the caller can set the
	 *  optional Light_render_info fields (deck spill falloff, inward pool shape, deck storeys).
	 */

	Light_render_info& Light_layers::add_render(
			int sx, int sy, int radius, int tier, int elevation, int rt, int ltx, int lty, int ltz, std::vector<unsigned char> lit,
			bool mask_roof, int dist_bias, bool is_spill, int spill_percent, int spill_floor, bool moving,
			std::vector<unsigned char> ring, int spill_deck_z, int cone_dx, int cone_dy, int cone_mouth) {
		return light_renders.emplace_back(Light_render_info{
				sx,      sy,         radius,    tier,           elevation,      rt,          ltx,          lty,
				ltz,     mask_roof,  dist_bias, is_spill,       spill_percent,  spill_floor, spill_deck_z, cone_dx,
				cone_dy, cone_mouth, moving,    std::move(lit), std::move(ring)});
	}

	/*
	 *  Rebuild the spatial-light overlay layers from the per-frame light_renders
	 *  list (populated by the renderer for every visible, unblocked light source).
	 *
	 *  Each brightness tier (0 = candle, 1 = single light, 2 = many lights) gets one
	 *  game-area overlay layer: a copy of the just-painted (dark, time-of-day) world
	 *  image, drawn through that tier's brighter fixed UI palette and masked by a
	 *  per-pixel radial alpha (coverage) that fades smoothly from full at the source
	 *  to zero at the radius edge. Composited over the dark base world this lights
	 *  only the area under each source, leaving the global palette untouched.
	 */

	void Light_layers::build(Game_window& gw) {
		static const Image_window::UiLayerKind kinds[3]
				= {Image_window::UiLayerLightCandle, Image_window::UiLayerLightSingle, Image_window::UiLayerLightMany};
		// Brighter tiers sit on top (greater z) so they gw.win where lights overlap;
		// all stay below conversation (z 0) and gump layers.
		static const int zvals[3] = {-1002, -1001, -1000};

		const int W = gw.get_width();
		const int H = gw.get_height();

		// Drop lights whose pool cannot reach the game area: they add nothing, and keeping them would let lights at
		// the edge of the collected range change the tier signature (a full re-splat).
		light_renders.erase(
				std::remove_if(
						light_renders.begin(), light_renders.end(),
						[W, H, &gw](const auto& lr) {
							// rt (tiles) carries the wall-top anchor slack the splat may draw up to.
							const int reach = std::max(lr.radius + lr.elevation, lr.rt * c_tilesize) + 2 * c_tilesize;
							// Test against the real scroll, not the smooth-scroll lerp (up to a tile off), so a light on
							// the edge does not flip in and out between frames.
							const int x = lr.sx + gw.avposx_ld;
							const int y = lr.sy + gw.avposy_ld;
							return x + reach < 0 || x - reach >= W || y + reach < 0 || y - reach >= H;
						}),
				light_renders.end());

		// The lights arrive in paint order, which shifts as the view scrolls.  Sort them canonically so the
		// order-sensitive tier signatures and the per-light bounds kept by index stay stable for the same lights.
		// Coverage max-combines, so the order does not change the result.
		std::stable_sort(light_renders.begin(), light_renders.end(), [](const auto& a, const auto& b) {
			return std::tie(a.tier, a.moving, a.ltx, a.lty, a.ltz, a.is_spill, a.spill_floor, a.dist_bias, a.radius, a.sx, a.sy)
				   < std::tie(b.tier, b.moving, b.ltx, b.lty, b.ltz, b.is_spill, b.spill_floor, b.dist_bias, b.radius, b.sx, b.sy);
		});

		// The splat rectangles recorded below feed NEXT frame's roof-mask clip
		// (update_roof_mask); rebuild them from scratch each frame.
		light_mask_rects.clear();

		// If the game area changed size (video settings change), the existing
		// layers are the wrong size: their coverage would be dropped, leaving a
		// full-opaque brightened layer covering the whole screen. Recreate them.
		if (W != light_layer_w || H != light_layer_h) {
			for (int t = 0; t < 3; ++t) {
				if (light_layer_handles[t] >= 0) {
					gw.destroy_layer(light_layer_handles[t]);
					light_layer_handles[t] = -1;
				}
			}
			light_layer_w = W;
			light_layer_h = H;
		}

		// Re-assert each light layer's config every build: some video/UI changes
		// (toggling infravision, changing a UI layer palette, etc.) reset the
		// per-kind configs, which would otherwise drop the brighter fixed palette.
		// Use the game's own scaler / fill scaler so the lit copy is filtered like
		// the world; placement is exact via get_game_area_dest (game_to_screen).
		static const int modes[3]
				= {Image_window::UiPaletteCandle, Image_window::UiPaletteSingleLight, Image_window::UiPaletteManyLights};
		const int gscaler = gw.win->get_scaler();
		const int gfill   = gw.win->get_fill_scaler();
		for (int t = 0; t < 3; ++t) {
			gw.win->set_ui_layer_config(kinds[t], 0, 0, gscaler, Image_window::Fill, gfill);
			gw.win->set_ui_layer_palette(kinds[t], modes[t]);
		}

		// No spatial lighting at full day or mid palette-transition: the fixed UI
		// palettes are disabled then, so just hide any existing layers.
		const int palnum = gw.pal ? gw.pal->get_palette_number() : PALETTE_DAY;
		// Refill the (brightened) fixed palettes only when the palette number
		// actually changes. Doing it every frame would overwrite the per-frame
		// colour-cycling rotation that rotate_colors applies to these overrides,
		// freezing animated flames under a light. Palette::apply also refills them
		// on every real palette change, keeping them in phase with the live palette.
		if (gw.pal && palnum != light_layer_palnum) {
			gw.pal->update_ui_layer_palettes();
			light_layer_palnum = palnum;
		}
		const bool lights_active = palnum != PALETTE_DAY && !light_renders.empty();

		// A tier only brightens if its fixed palette is actually lighter than the
		// current world palette; at dawn/dusk the world may already be brighter
		// than (say) candlelight, in which case that tier would darken the scene.
		static const int tier_palnum[3] = {PALETTE_CANDLE, PALETTE_SINGLE_LIGHT, PALETTE_MANY_LIGHTS};
		const int        world_lum      = gw.pal ? gw.pal->get_luminance() : 0;

		// Source pixels = the world image just painted (world + effects only; gumps
		// live in their own layers and must not be brightened).
		Image_buffer*        src    = gw.win->get_ibuf();
		const unsigned char* srcpix = src ? src->get_bits() : nullptr;
		const int            src_lw = src ? static_cast<int>(src->get_line_width()) : 0;

		// Global roof-pixel mask: any pixel a drawn roof covers stays dark under
		// every light so an interior light never lights up the roof over it.
		const unsigned char* roofpix = (roof_light_mask_active && roof_light_mask) ? roof_light_mask->get_bits() : nullptr;
		const int            roof_lw = roofpix ? static_cast<int>(roof_light_mask->get_line_width()) : 0;
		// Surface-identity channels for the face/object consumers.
		const unsigned char* kindpix
				= (roofpix != nullptr && light_kind_mask && !light_foot_dx.empty()) ? light_kind_mask->get_bits() : nullptr;
		const int            kind_lw = kindpix ? static_cast<int>(light_kind_mask->get_line_width()) : 0;
		const int            foot_lw = kindpix ? static_cast<int>(light_kind_mask->get_width()) : 0;
		const unsigned char* footdxp = kindpix ? light_foot_dx.data() : nullptr;
		const unsigned char* footdyp = kindpix ? light_foot_dy.data() : nullptr;
		// INSIDE, the room mask applies to every light (the roof mask on top so
		// nothing spills onto a still-drawn roof).  OUTSIDE, it still applies to a
		// light itself under a roof (lr.mask_roof) so its walls contain it, while
		// an exterior light stays unmasked and brightens nearby house roofs.
		const bool inside = gw.is_main_actor_inside();
		// The viewer's z=0 foot position, for the splat's viewer-side face rule.
		int av_fx = 0;
		int av_fy = 0;
		if (gw.main_actor != nullptr) {
			const Tile_coord at = gw.main_actor->get_tile();
			gw.get_shape_location(Tile_coord(at.tx, at.ty, 0), av_fx, av_fy);
		}

		const Splat_frame base_frame{W,       H,
									 nullptr, 0,
									 srcpix,  src_lw,
									 roofpix, roof_lw,
									 kindpix, kind_lw,
									 footdxp, footdyp,
									 foot_lw, light_sprite_boxes,
									 inside,  av_fx,
									 av_fy,   gw.skip_above_actor};

		// Per-tier signature of the STATIC (world-pinned) lights shaping this
		// frame's coverage masks.  Positions are hashed RELATIVE to the tier's
		// first static light, so a pure view scroll keeps the signature: the
		// cached mask is then reused as-is (no scroll) or translated by the
		// scroll delta.  Room-grid changes are caught by the flood generation,
		// mask changes by the mask diff in the tier loop.
		uint64_t tier_sig[3];
		// Moving lights (the carried torch and its spills) get their own
		// signature, keyed by their tiles rather than the scroll.
		uint64_t mov_sig[3];
		int      sx0[3]        = {0, 0, 0};
		int      sy0[3]        = {0, 0, 0};
		bool     has_static[3] = {false, false, false};
		{
			for (const auto& lr : light_renders) {
				if (lr.moving || lr.tier < 0 || lr.tier > 2 || has_static[lr.tier]) {
					continue;
				}
				has_static[lr.tier] = true;
				sx0[lr.tier]        = lr.sx;
				sy0[lr.tier]        = lr.sy;
			}
			auto mix = [](uint64_t h, int64_t v) {
				h ^= static_cast<uint64_t>(v);
				return h * 1099511628211ULL;
			};
			uint64_t base = mix(mix(mix(mix(14695981039346656037ULL, W), H), inside), roofpix != nullptr);
			// Hidden-room gating (Splat_light) depends on the render cut-away level.
			base = mix(base, gw.skip_above_actor);
			// Any change to a cached room grid (a door opened/closed) rebuilds
			// every tier's coverage immediately.
			base                       = mix(base, static_cast<int64_t>(Flood_content_generation()));
			const uint64_t base_static = base;
			// Neither signature keys on the screen-space mask, which changes on every scroll step: static lights
			// compare the mask in world space below, and the moving coverage is slid with the view and refreshed by the
			// mask diff and the tile change.
			tier_sig[0] = tier_sig[1] = tier_sig[2] = base_static;
			mov_sig[0] = mov_sig[1] = mov_sig[2] = base_static;
			for (const auto& lr : light_renders) {
				if (lr.tier < 0 || lr.tier > 2) {
					continue;
				}
				uint64_t h = lr.moving ? mov_sig[lr.tier] : tier_sig[lr.tier];
				// Moving lights are keyed by tile in world space, not by screen position: a carried light keeps its
				// coverage while it stays on one tile (slid with the view below).
				if (!lr.moving) {
					h = mix(h, lr.sx - sx0[lr.tier]);
					h = mix(h, lr.sy - sy0[lr.tier]);
				}
				h = mix(h, lr.radius);
				h = mix(h, lr.elevation);
				h = mix(h, lr.rt);
				h = mix(h, lr.ltx);
				h = mix(h, lr.lty);
				h = mix(h, lr.ltz);
				h = mix(h, lr.mask_roof);
				h = mix(h, lr.dist_bias);
				h = mix(h, lr.is_spill);
				h = mix(h, lr.spill_percent);
				h = mix(h, lr.spill_floor);
				h = mix(h, static_cast<int64_t>(lr.lit.size()));
				if (lr.moving) {
					mov_sig[lr.tier] = h;
				} else {
					tier_sig[lr.tier] = h;
				}
			}
		}

		// Actors stamp no mask channel, so cached coverage is silhouette-free.
		for (int t = 0; t < 3; ++t) {
			bool any = false;
			for (const auto& lr : light_renders) {
				if (lr.tier == t) {
					any = true;
					break;
				}
			}
			// Skip the tier if its palette is no brighter than the world palette.
			const int  tier_lum = gw.pal ? gw.pal->get_reference_luminance(tier_palnum[t]) : -1;
			const bool brighter = tier_lum > world_lum;
			if (!lights_active || !any || !srcpix || !brighter) {
				if (light_layer_handles[t] >= 0) {
					gw.layer_set_visible(light_layer_handles[t], false);
				}
				// While hidden, the roof mask goes unstamped (light_mask_rects
				// empty) and the view scrolls under the retained coverage.  The
				// sig would still MATCH on re-entry (it hashes light positions
				// relative to each other) and reuse that stale coverage, so force
				// a fresh rebuild + mask resample.
				light_tier_sig[t] = 0;
				continue;
			}
			// Ensure the (game-area sized) layer exists.
			int handle = light_layer_handles[t];
			if (handle < 0) {
				handle                 = gw.create_layer("gw.natural_light", W, H, 255, 0, zvals[t]);
				light_layer_handles[t] = handle;
			}
			gw.layer_set_opaque(handle, true);
			gw.layer_set_ui_kind(handle, kinds[t]);

			Image_buffer8* dst = gw.get_layer_ibuf(handle);
			if (!dst) {
				continue;
			}
			unsigned char* dstpix = dst->get_bits();
			const int      dst_lw = static_cast<int>(dst->get_line_width());

			// This tier's radial-alpha (coverage) mask holds the STATIC
			// (world-pinned) lights only.  It persists between frames: while the
			// static signature holds, a still view reuses it as-is and a scrolled
			// view translates it by the scroll delta, re-splatting only the
			// lights exposed at a screen edge; anything else rebuilds it.
			std::vector<unsigned char>& covbuf = light_tier_cov[t];
			if (covbuf.size() != static_cast<size_t>(W) * H) {
				covbuf.assign(static_cast<size_t>(W) * H, 0);
				light_tier_rects[t].clear();
				light_tier_mask_rects[t].clear();
				light_tier_bounds[t].clear();
				light_tier_sig[t] = 0;
			}
			unsigned char* cov = covbuf.data();

			Splat_frame frame = base_frame;
			frame.dstpix      = dstpix;
			frame.dst_lw      = dst_lw;

			// Derive both rect lists from the per-light unclamped bounds:
			// unclamped + margin -> next frame's roof-mask clips; clamped ->
			// the coverage rows holding non-zero alpha.
			auto rebuild_rects = [&]() {
				light_tier_rects[t].clear();
				light_tier_mask_rects[t].clear();
				for (const TileRect& b : light_tier_bounds[t]) {
					TileRect clip = b;
					light_tier_mask_rects[t].push_back(clip.enlarge(24));
					const int bx0 = std::max(b.x, 0);
					const int by0 = std::max(b.y, 0);
					const int bx1 = std::min(b.x + b.w - 1, W - 1);
					const int by1 = std::min(b.y + b.h - 1, H - 1);
					if (bx0 <= bx1 && by0 <= by1) {
						light_tier_rects[t].emplace_back(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
					}
				}
			};

			// The world animates beneath a kept mask: refresh the copied world
			// pixels over the static coverage rows.
			auto refresh_dst = [&]() {
				for (const TileRect& r : light_tier_rects[t]) {
					for (int y = r.y; y < r.y + r.h; ++y) {
						std::memcpy(
								dstpix + static_cast<size_t>(y) * dst_lw + r.x, srcpix + static_cast<size_t>(y) * src_lw + r.x,
								static_cast<size_t>(r.w));
					}
				}
			};

			// A dragged object is lifted out of the world into its own layer, so
			// the world render loses it while the cached coverage still carries
			// its splat: bare ground then shows at the object's alpha as a
			// brighter silhouette.  Re-splat for as long as the drag is in flight.
			const bool sig_ok0 = !gw.is_dragging() && light_tier_sig[t] != 0 && tier_sig[t] == light_tier_sig[t];
			int        dx      = 0;
			int        dy      = 0;
			if (sig_ok0 && has_static[t]) {
				dx = sx0[t] - light_tier_anchor_x[t];
				dy = sy0[t] - light_tier_anchor_y[t];
			}
			// The cached coverage was splatted against the previous frame's mask: keep it where the mask under its rows,
			// shifted by the scroll, is unchanged (actor pixels excepted, as in the moving sig).  Where it changed
			// (a door swung, the mask finished stamping a newly lit area), only that rectangle is re-splatted, which is
			// far cheaper than rebuilding the whole tier.
			bool                  mask_same = true;
			std::vector<TileRect> mask_dirty;    // Current screen coordinates.
			if (sig_ok0 && roofpix != nullptr) {
				const size_t sz = static_cast<size_t>(W) * H;
				mask_same       = light_prev_roof.size() == sz && light_prev_kind.size() == sz;
				for (size_t ri = 0; mask_same && ri < light_tier_rects[t].size(); ++ri) {
					const TileRect& r   = light_tier_rects[t][ri];
					const int       cx0 = std::max(r.x, -dx);
					const int       cx1 = std::min(r.x + r.w - 1, W - 1 - dx);
					int             bx0 = W;
					int             by0 = H;
					int             bx1 = -1;
					int             by1 = -1;
					for (int y = std::max(r.y, -dy); y <= std::min(r.y + r.h - 1, H - 1 - dy); ++y) {
						const unsigned char* prow = light_prev_roof.data() + static_cast<size_t>(y) * W;
						const unsigned char* pk   = light_prev_kind.data() + static_cast<size_t>(y) * W;
						const unsigned char* crow = roofpix + static_cast<size_t>(y + dy) * roof_lw + dx;
						const unsigned char* ck
								= kindpix != nullptr ? kindpix + static_cast<size_t>(y + dy) * kind_lw + dx : nullptr;
						for (int x = cx0; x <= cx1; ++x) {
							if (prow[x] != crow[x] && pk[x] != 3 && (ck == nullptr || ck[x] != 3)) {
								bx0 = std::min(bx0, x);
								bx1 = std::max(bx1, x);
								by0 = std::min(by0, y);
								by1 = y;
							}
						}
					}
					if (bx1 >= 0) {
						mask_dirty.emplace_back(bx0 + dx, by0 + dy, bx1 - bx0 + 1, by1 - by0 + 1);
					}
				}
			}
			if (mask_dirty.size() > 1) {
				// Each dirty rect re-splats every light reaching it, so merge overlapping ones.
				for (bool merged = true; merged;) {
					merged = false;
					for (size_t i = 0; i < mask_dirty.size() && !merged; ++i) {
						for (size_t j = i + 1; j < mask_dirty.size(); ++j) {
							TileRect&       a = mask_dirty[i];
							const TileRect& b = mask_dirty[j];
							if (a.x > b.x + b.w || b.x > a.x + a.w || a.y > b.y + b.h || b.y > a.y + a.h) {
								continue;
							}
							const int x0 = std::min(a.x, b.x);
							const int y0 = std::min(a.y, b.y);
							const int x1 = std::max(a.x + a.w, b.x + b.w);
							const int y1 = std::max(a.y + a.h, b.y + b.h);
							a            = TileRect(x0, y0, x1 - x0, y1 - y0);
							mask_dirty.erase(mask_dirty.begin() + static_cast<std::ptrdiff_t>(j));
							merged = true;
							break;
						}
					}
				}
			}
			if (!mask_dirty.empty()) {
				// Most of the lit area changed: one full rebuild is cheaper than clipped re-splats.
				long dirty_area = 0;
				long lit_area   = 0;
				for (const TileRect& d : mask_dirty) {
					dirty_area += static_cast<long>(d.w) * d.h;
				}
				for (const TileRect& r : light_tier_rects[t]) {
					lit_area += static_cast<long>(r.w) * r.h;
				}
				if (2 * dirty_area >= lit_area) {
					mask_dirty.clear();
					mask_same = false;
				}
			}
			const bool sig_ok     = sig_ok0 && mask_same;
			const bool reuse      = sig_ok && dx == 0 && dy == 0;
			const bool translate  = sig_ok && !reuse && std::abs(dx) < W && std::abs(dy) < H;
			bool       has_moving = false;
			{
				if (reuse) {
					// Mask unchanged and screen-aligned: just refresh the copied
					// world pixels and keep last pass's rects.
					refresh_dst();
				} else if (translate) {
					// Same static set, pure view scroll: slide the cached mask by
					// the scroll delta and zero the vacated strips.
					const int copy_w    = W - std::abs(dx);
					const int dst_x     = std::max(dx, 0);
					const int src_x     = std::max(-dx, 0);
					auto      shift_row = [&](int y) {
                        unsigned char*       drow = cov + static_cast<size_t>(y) * W;
                        const unsigned char* srow = cov + static_cast<size_t>(y - dy) * W;
                        std::memmove(drow + dst_x, srow + src_x, static_cast<size_t>(copy_w));
                        if (dx > 0) {
                            std::memset(drow, 0, static_cast<size_t>(dst_x));
                        } else if (dx < 0) {
                            std::memset(drow + copy_w, 0, static_cast<size_t>(W - copy_w));
                        }
					};
					if (dy >= 0) {
						for (int y = H - 1; y >= dy; --y) {
							shift_row(y);
						}
						for (int y = 0; y < dy; ++y) {
							std::memset(cov + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
						}
					} else {
						for (int y = 0; y < H + dy; ++y) {
							shift_row(y);
						}
						for (int y = H + dy; y < H; ++y) {
							std::memset(cov + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
						}
					}
					for (TileRect& b : light_tier_bounds[t]) {
						b.x += dx;
						b.y += dy;
					}
					rebuild_rects();
					refresh_dst();

					// Patch only the vacated strips: the translated interior is
					// already correct, so each affected light is re-splatted
					// CLIPPED to the strip(s) its bounds reach into -- a few
					// pixel columns/rows, not its whole area.
					Light_strip strips[4];
					const int   nstrips
							= Light_scroll_strips(cov, W, H, dx, dy, light_tier_debt_x[t], light_tier_debt_y[t], 8, strips);
					size_t bi = 0;
					for (const auto& lr : light_renders) {
						if (lr.tier != t || lr.moving || lr.radius <= 0) {
							continue;
						}
						if (bi >= light_tier_bounds[t].size()) {
							break;
						}
						const TileRect& b = light_tier_bounds[t][bi++];
						for (int s = 0; s < nstrips; ++s) {
							const Light_strip& st = strips[s];
							if (b.x > st.x1 || b.y > st.y1 || b.x + b.w - 1 < st.x0 || b.y + b.h - 1 < st.y0) {
								continue;
							}
							Splat_light(frame, lr, cov, st.x0, st.y0, st.x1, st.y1);
						}
					}
					light_tier_anchor_x[t] = sx0[t];
					light_tier_anchor_y[t] = sy0[t];
				} else {
					light_tier_debt_x[t] = 0;
					light_tier_debt_y[t] = 0;
					// Full rebuild: restore the all-zero coverage invariant over
					// last pass's rows first, then splat every static light.
					for (const TileRect& r : light_tier_rects[t]) {
						for (int y = r.y; y < r.y + r.h; ++y) {
							std::memset(cov + static_cast<size_t>(y) * W + r.x, 0, static_cast<size_t>(r.w));
						}
					}
					const bool content_changed = light_tier_sig[t] != tier_sig[t] || !mask_same;
					light_tier_bounds[t].clear();
					for (const auto& lr : light_renders) {
						if (lr.tier != t || lr.moving || lr.radius <= 0) {
							continue;
						}
						light_tier_bounds[t].push_back(Splat_light(frame, lr, cov));
					}
					rebuild_rects();
					light_tier_sig[t]      = tier_sig[t];
					light_tier_anchor_x[t] = sx0[t];
					light_tier_anchor_y[t] = sy0[t];
					if (content_changed) {
						// The static set changed (a light placed, removed or newly
						// scrolled into view): the roof mask this frame was stamped
						// only inside LAST frame's light rects, so the rebuild above
						// baked stale mask under the new coverage.  Repaint
						// everything (deferred: paint_dirty clears the dirty rect
						// right after this call) and rebuild for the next TWO frames:
						// the mask converges one paint->build round per frame, and a
						// single resample can still sample it half-stamped.
						light_tier_resample[t] = 2;
						light_repaint_pending  = true;
					} else if (light_tier_resample[t] > 0) {
						--light_tier_resample[t];
						light_repaint_pending = true;
					}
				}
				if ((reuse || translate) && !mask_dirty.empty()) {
					for (const TileRect& d : mask_dirty) {
						for (int y = d.y; y < d.y + d.h; ++y) {
							std::memset(cov + static_cast<size_t>(y) * W + d.x, 0, static_cast<size_t>(d.w));
						}
						size_t bi = 0;
						for (const auto& lr : light_renders) {
							if (lr.tier != t || lr.moving || lr.radius <= 0) {
								continue;
							}
							if (bi >= light_tier_bounds[t].size()) {
								break;
							}
							const TileRect& b = light_tier_bounds[t][bi++];
							if (b.x > d.x + d.w - 1 || b.y > d.y + d.h - 1 || b.x + b.w - 1 < d.x || b.y + b.h - 1 < d.y) {
								continue;
							}
							Splat_light(frame, lr, cov, d.x, d.y, d.x + d.w - 1, d.y + d.h - 1);
						}
					}
					refresh_dst();
				}
				if ((reuse || translate) && light_tier_resample[t] > 0) {
					// The mask is still converging after a content change: keep repainting so it is stamped everywhere;
					// the dirty rectangles above pick up whatever it changes.
					--light_tier_resample[t];
					light_repaint_pending = true;
				}
				// This tier's roof-mask clips for next frame.
				light_mask_rects.insert(light_mask_rects.end(), light_tier_mask_rects[t].begin(), light_tier_mask_rects[t].end());
				// Moving lights (the carried torch and its spills) never touch
				// the static cache: they live in their own cached overlay, reused
				// while the Avatar stands still and rebuilt while walking, then
				// max-combined over a scratch copy of the static mask.
				for (const auto& lr : light_renders) {
					if (lr.tier == t && lr.moving && lr.radius > 0) {
						has_moving = true;
						break;
					}
				}
				if (has_moving) {
					const size_t                sz  = static_cast<size_t>(W) * H;
					std::vector<unsigned char>& mov = light_tier_mov_cov[t];
					// World pixel of the screen origin (a tile's screen x is its world x minus this).
					const int org_x  = gw.scrolltx * c_tilesize + gw.scrolltx_lo;
					const int org_y  = gw.scrollty * c_tilesize + gw.scrollty_lo;
					const int mdx    = light_tier_mov_ox[t] - org_x;
					const int mdy    = light_tier_mov_oy[t] - org_y;
					bool      sig_eq = light_tier_mov_sig[t] != 0 && mov_sig[t] == light_tier_mov_sig[t] && mov.size() == sz;
					// Like the static coverage, the moving one was splatted against the previous frame's mask: where the
					// mask under it (shifted by the scroll, actor pixels excepted) changed, re-splat the moving lights
					// clipped to the change -- or all of it when most changed.  This also replaces a splat made before
					// the mask was fully stamped.
					int mdx0 = W, mdy0 = H, mdx1 = -1, mdy1 = -1;    // Changed box, current screen coordinates.
					if (sig_eq && roofpix != nullptr) {
						if (light_prev_roof.size() != sz || light_prev_kind.size() != sz) {
							sig_eq = false;
						}
						long mov_area = 0;
						for (size_t bi = 0; sig_eq && bi < light_tier_mov_bounds[t].size(); ++bi) {
							const TileRect& b = light_tier_mov_bounds[t][bi];
							// Current screen rect after the slide; previous pixel at (x - mdx, y - mdy).
							const int x0 = std::max({b.x + mdx, 0, mdx});
							const int x1 = std::min({b.x + mdx + b.w - 1, W - 1, W - 1 + mdx});
							const int y0 = std::max({b.y + mdy, 0, mdy});
							const int y1 = std::min({b.y + mdy + b.h - 1, H - 1, H - 1 + mdy});
							if (x0 > x1 || y0 > y1) {
								continue;
							}
							mov_area += static_cast<long>(x1 - x0 + 1) * (y1 - y0 + 1);
							for (int y = y0; y <= y1; ++y) {
								const unsigned char* crow = roofpix + static_cast<size_t>(y) * roof_lw;
								const unsigned char* ck = kindpix != nullptr ? kindpix + static_cast<size_t>(y) * kind_lw : nullptr;
								const unsigned char* prow = light_prev_roof.data() + static_cast<size_t>(y - mdy) * W - mdx;
								const unsigned char* pk   = light_prev_kind.data() + static_cast<size_t>(y - mdy) * W - mdx;
								for (int x = x0; x <= x1; ++x) {
									if (prow[x] != crow[x] && pk[x] != 3 && (ck == nullptr || ck[x] != 3)) {
										mdx0 = std::min(mdx0, x);
										mdx1 = std::max(mdx1, x);
										mdy0 = std::min(mdy0, y);
										mdy1 = y;
									}
								}
							}
						}
						if (mdx1 >= 0 && 2L * (mdx1 - mdx0 + 1) * (mdy1 - mdy0 + 1) >= mov_area) {
							sig_eq = false;    // Most of it changed: one full re-splat.
						}
					}
					const bool mov_ok = sig_eq && mdx == 0 && mdy == 0;
					if (sig_eq && !mov_ok && std::abs(mdx) < W && std::abs(mdy) < H) {
						// Same light on the same tile, the view scrolled: slide the coverage, zero the vacated strips and
						// re-splat the moving lights clipped to them.
						const int copy_w = W - std::abs(mdx);
						const int dst_x  = std::max(mdx, 0);
						const int src_x  = std::max(-mdx, 0);
						auto      shift  = [&](int y) {
                            unsigned char*       drow = mov.data() + static_cast<size_t>(y) * W;
                            const unsigned char* srow = mov.data() + static_cast<size_t>(y - mdy) * W;
                            std::memmove(drow + dst_x, srow + src_x, static_cast<size_t>(copy_w));
                            if (mdx > 0) {
                                std::memset(drow, 0, static_cast<size_t>(dst_x));
                            } else if (mdx < 0) {
                                std::memset(drow + copy_w, 0, static_cast<size_t>(W - copy_w));
                            }
						};
						if (mdy >= 0) {
							for (int y = H - 1; y >= mdy; --y) {
								shift(y);
							}
							for (int y = 0; y < mdy; ++y) {
								std::memset(mov.data() + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
							}
						} else {
							for (int y = 0; y < H + mdy; ++y) {
								shift(y);
							}
							for (int y = H + mdy; y < H; ++y) {
								std::memset(mov.data() + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
							}
						}
						for (TileRect& b : light_tier_mov_bounds[t]) {
							b.x += mdx;
							b.y += mdy;
						}
						Light_strip strips[4];
						const int   nstrips = Light_scroll_strips(
                                mov.data(), W, H, mdx, mdy, light_tier_mov_debt_x[t], light_tier_mov_debt_y[t], 8, strips);
						for (const auto& lr : light_renders) {
							if (lr.tier != t || !lr.moving || lr.radius <= 0) {
								continue;
							}
							for (int si = 0; si < nstrips; ++si) {
								const Light_strip& st = strips[si];
								Splat_light(frame, lr, mov.data(), st.x0, st.y0, st.x1, st.y1);
							}
						}
						light_tier_mov_ox[t] = org_x;
						light_tier_mov_oy[t] = org_y;
					} else if (!mov_ok) {
						if (mov.size() != sz) {
							mov.assign(sz, 0);
						} else {
							for (const TileRect& b : light_tier_mov_bounds[t]) {
								const int bx0 = std::max(b.x, 0);
								const int by0 = std::max(b.y, 0);
								const int bx1 = std::min(b.x + b.w - 1, W - 1);
								const int by1 = std::min(b.y + b.h - 1, H - 1);
								for (int y = by0; y <= by1; ++y) {
									std::memset(
											mov.data() + static_cast<size_t>(y) * W + bx0, 0, static_cast<size_t>(bx1 - bx0 + 1));
								}
							}
						}
						light_tier_mov_bounds[t].clear();
						for (const auto& lr : light_renders) {
							if (lr.tier != t || !lr.moving || lr.radius <= 0) {
								continue;
							}
							light_tier_mov_bounds[t].push_back(Splat_light(frame, lr, mov.data()));
						}
						light_tier_mov_sig[t]    = mov_sig[t];
						light_tier_mov_ox[t]     = org_x;
						light_tier_mov_oy[t]     = org_y;
						light_tier_mov_debt_x[t] = 0;
						light_tier_mov_debt_y[t] = 0;
					}
					if (sig_eq && mdx1 >= 0) {
						for (int y = mdy0; y <= mdy1; ++y) {
							std::memset(mov.data() + static_cast<size_t>(y) * W + mdx0, 0, static_cast<size_t>(mdx1 - mdx0 + 1));
						}
						for (const auto& lr : light_renders) {
							if (lr.tier == t && lr.moving && lr.radius > 0) {
								Splat_light(frame, lr, mov.data(), mdx0, mdy0, mdx1, mdy1);
							}
						}
					}
					// Composite into the scratch copy and refresh the world pixels
					// beneath the moving coverage (its splat-time dst copies go
					// stale as the world animates).
					light_scratch_cov.resize(sz);
					std::memcpy(light_scratch_cov.data(), cov, sz);
					for (const TileRect& b : light_tier_mov_bounds[t]) {
						TileRect clipr = b;
						light_mask_rects.push_back(clipr.enlarge(24));
						const int bx0 = std::max(b.x, 0);
						const int by0 = std::max(b.y, 0);
						const int bx1 = std::min(b.x + b.w - 1, W - 1);
						const int by1 = std::min(b.y + b.h - 1, H - 1);
						if (bx0 > bx1 || by0 > by1) {
							continue;
						}
						for (int y = by0; y <= by1; ++y) {
							std::memcpy(
									dstpix + static_cast<size_t>(y) * dst_lw + bx0, srcpix + static_cast<size_t>(y) * src_lw + bx0,
									static_cast<size_t>(bx1 - bx0 + 1));
							unsigned char*       srow = light_scratch_cov.data() + static_cast<size_t>(y) * W;
							const unsigned char* mrow = mov.data() + static_cast<size_t>(y) * W;
							for (int x = bx0; x <= bx1; ++x) {
								if (mrow[x] > srow[x]) {
									srow[x] = mrow[x];
								}
							}
						}
					}
				}
			}
			// Actors and placed light sprites relight per frame (scratch coverage only).
			Relight_sprites(frame, light_renders, t, cov, light_scratch_cov, has_moving);
			{
				Soften_face_edges(frame, has_moving ? light_scratch_cov.data() : cov, light_soft_cov, dstpix, dst_lw);
				gw.win->layer_set_coverage(handle, light_soft_cov.data(), W, H);
				// Align the overlay with the world's on-screen rectangle.
				int gx0 = 0;
				int gy0 = 0;
				int gdw = W;
				int gdh = H;
				gw.win->get_game_area_dest(gx0, gy0, gdw, gdh);
				gw.layer_set_dest(handle, gx0, gy0, gdw, gdh);
				gw.layer_set_visible(handle, true);
				gw.layer_set_dirty(handle);
			}
		}
		if (roofpix != nullptr) {
			const size_t sz = static_cast<size_t>(W) * H;
			light_prev_roof.resize(sz);
			light_prev_kind.assign(sz, 0);
			for (int y = 0; y < H; ++y) {
				std::memcpy(
						light_prev_roof.data() + static_cast<size_t>(y) * W, roofpix + static_cast<size_t>(y) * roof_lw,
						static_cast<size_t>(W));
				if (kindpix != nullptr) {
					std::memcpy(
							light_prev_kind.data() + static_cast<size_t>(y) * W, kindpix + static_cast<size_t>(y) * kind_lw,
							static_cast<size_t>(W));
				}
			}
		} else {
			light_prev_roof.clear();
			light_prev_kind.clear();
		}
	}

}    // namespace NaturalLight
