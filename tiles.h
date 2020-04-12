/*
 *  tiles.h - Tile coords.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef INCL_TILES
#define INCL_TILES  1

#include "exult_constants.h"

#define FIX_COORD(x) (((x)+c_num_tiles)%c_num_tiles)

/*
 *  A 3D tile coordinate:
 */
class Tile_coord {
	static short neighbors[16]; // Neighboring tiles in each dir.
public:
	short tx = 0, ty = 0, tz = 0;       // Coords. within world. tz=lift.
	Tile_coord(int x, int y, int z) : tx(x), ty(y), tz(z)
	{  }
	Tile_coord() = default;
	bool operator==(Tile_coord const &t2) const {
		return t2.tx == tx && t2.ty == ty && t2.tz == tz;
	}
	bool operator!=(Tile_coord const &t2) const {
		return !(*this == t2);
	}
	int distance(Tile_coord const &t2) const {  // Distance to another tile?
		int delta = distance_2d(t2);
		int dz = t2.tz - tz;
		if (dz < 0) {
			dz = -dz;
		}
		// Take larger abs. value.
		return delta > dz ? delta : dz;
	}
	int distance_2d(Tile_coord const &t2) const { // For pathfinder.
		// Handle wrapping round the world.
		int dy = (t2.ty - ty + c_num_tiles) % c_num_tiles;
		int dx = (t2.tx - tx + c_num_tiles) % c_num_tiles;
		if (dy >= c_num_tiles / 2) {
			dy = c_num_tiles - dy;
		}
		if (dx >= c_num_tiles / 2) {
			dx = c_num_tiles - dx;
		}
		// Take larger abs. value.
		return dy > dx ? dy : dx;
	}
	int square_distance_2d(Tile_coord const &t2) const {
		int dy = (t2.ty - ty + c_num_tiles) % c_num_tiles;
		int dx = (t2.tx - tx + c_num_tiles) % c_num_tiles;
		if (dy >= c_num_tiles / 2) {
			dy = c_num_tiles - dy;
		}
		if (dx >= c_num_tiles / 2) {
			dx = c_num_tiles - dx;
		}
		return dx * dx + dy * dy;
	}
	int square_distance_3d(Tile_coord const &t2) const {
		int dy = (t2.ty - ty + c_num_tiles) % c_num_tiles;
		int dx = (t2.tx - tx + c_num_tiles) % c_num_tiles;
		int dz = t2.tz - tz;
		if (dy >= c_num_tiles / 2) {
			dy = c_num_tiles - dy;
		}
		if (dx >= c_num_tiles / 2) {
			dx = c_num_tiles - dx;
		}
		return dx * dx + dy * dy + dz * dz;
	}
	int square_distance_screen_space(Tile_coord const &t2) const {
		int dy = (t2.ty - ty + c_num_tiles) % c_num_tiles;
		int dx = (t2.tx - tx + c_num_tiles) % c_num_tiles;
		int dz = t2.tz - tz;
		if (dy >= c_num_tiles / 2) {
			dy = c_num_tiles - dy;
		}
		if (dx >= c_num_tiles / 2) {
			dx = c_num_tiles - dx;
		}
		dx = dx * 2 - dz;
		dy = dy * 2 - dz;

		return (dx * dx + dy * dy) / 4;
	}
	// Get neighbor in given dir (0-7).
	inline Tile_coord get_neighbor(int dir) const {
		return Tile_coord(
		           (tx + neighbors[2 * dir] + c_num_tiles) % c_num_tiles,
		           (ty + neighbors[2 * dir + 1] + c_num_tiles) % c_num_tiles,
		           tz);
	}
	void fixme() {
		tx = FIX_COORD(tx);
		ty = FIX_COORD(ty);
	}
	static bool gte(int t1, int t2) { // Ret. t1 >= t2 with wrapping.
		int diff = t1 - t2;
		return diff >= 0 ? (diff < c_num_tiles / 2) :
		       (diff < -c_num_tiles / 2);
	}
	// Ret. (to - from) with wrapping.
	static int delta(int from, int to) {
		int diff = to - from;
		return diff >= c_num_tiles / 2 ? (diff - c_num_tiles) :
		       (diff <= -c_num_tiles / 2 ? (diff + c_num_tiles) :
		        diff);
	}
};
// Add two coords.
inline Tile_coord operator+(Tile_coord const &a, Tile_coord const &b) {
	return Tile_coord(a.tx + b.tx, a.ty + b.ty, a.tz + b.tz);
}

#endif
