/*
 *  rect.h - Rectangles.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef RECT_H
#define RECT_H 1

#include "exult_constants.h"

#include <algorithm>

static inline bool Point_in_strip(int start, int end, int pt) noexcept {
	start = (start + c_num_tiles) % c_num_tiles;
	pt    = (pt + c_num_tiles) % c_num_tiles;
	end += start;
	// Does the strip wrap around the world?
	if (end >= c_num_tiles) {
		// Yes. Check both halves of it.
		if ((pt < start || pt >= c_num_tiles)
			&& (pt < 0 || pt >= (end % c_num_tiles))) {
			return false;
		}
	}
	// No; check whether the point is in or not.
	else if (pt < start || pt >= end) {
		return false;    // It was not.
	}
	return true;
}

/*
 *  A rectangle:
 */
class TileRect {
public:          // Let's make it all public.
	int x, y;    // Position.
	int w, h;    // Dimensions.

	TileRect(int xin, int yin, int win, int hin) noexcept
			: x(xin), y(yin), w(win), h(hin) {}

	TileRect() noexcept = default;    // An uninitialized one.

	// Is this point in it?
	bool has_point(int px, int py) const noexcept {
		return px >= x && px < x + w && py >= y && py < y + h;
	}

	// Add another to this one to get
	//  a rect. that encloses both.
	// Is this point in it (world wrap enabled!)?
	bool has_world_point(int px, int py) const noexcept {
		return Point_in_strip(x, w, px) && Point_in_strip(y, h, py);
	}

	// Add another to this one to get
	//  a rect. that encloses both.
	TileRect add(const TileRect& r2) const noexcept {
		const int xend  = x + w;
		const int yend  = y + h;
		const int xend2 = r2.x + r2.w;
		const int yend2 = r2.y + r2.h;
		TileRect  r;    // Return this.
		r.x = std::min(x, r2.x);
		r.y = std::min(y, r2.y);
		r.w = std::max(xend, xend2) - r.x;
		r.h = std::max(yend, yend2) - r.y;
		return r;
	}

	// Intersect another with this.
	TileRect intersect(const TileRect& r2) const noexcept {
		const int xend  = x + w;
		const int yend  = y + h;
		const int xend2 = r2.x + r2.w;
		const int yend2 = r2.y + r2.h;
		TileRect  r;    // Return this.
		r.x = std::max(x, r2.x);
		r.y = std::max(y, r2.y);
		r.w = std::min(xend, xend2) - r.x;
		r.h = std::min(yend, yend2) - r.y;
		return r;
	}

	// Does it intersect another?
	bool intersects(const TileRect& r2) const noexcept {
		return x < r2.x + r2.w && r2.x < x + w && y < r2.y + r2.h
			   && r2.y < y + h;
	}

	void shift(int deltax, int deltay) noexcept {
		x += deltax;
		y += deltay;
	}

	TileRect& enlarge(int delta) noexcept {    // Add delta in each dir.
		x -= delta;
		y -= delta;
		w += 2 * delta;
		h += 2 * delta;
		return *this;
	}

	int distance(
			int px, int py) const noexcept    // Get distance from a point (max.
	//   dist. along x or y coord.)
	{
		const int xdist = px <= x ? (x - px) : (px - x - w + 1);
		const int ydist = py <= y ? (y - py) : (py - y - h + 1);
		const int dist  = xdist > ydist ? xdist : ydist;
		return dist < 0 ? 0 : dist;
	}

	bool operator==(const TileRect& rect2) const noexcept {
		return x == rect2.x && y == rect2.y && w == rect2.w && h == rect2.h;
	}

	bool operator!=(const TileRect& rect2) const noexcept {
		return !(*this == rect2);
	}

	// Conversion to bool return true if any value is non zero
	operator bool()const noexcept
	{
		return h != 0 || w != 0 || x != 0 || y != 0;
	}
};

/*
 *  A 3-dim. block.
 */
class Block {
public:
	int x, y, z;    // Position.
	int w, d, h;    // Dimensions.

	Block(int xin, int yin, int zin, int win, int din, int hin) noexcept
			: x(xin), y(yin), z(zin), w(win), d(din), h(hin) {}

	Block() noexcept = default;    // An uninitialized one.

	// Is this point in it?
	bool has_point(int px, int py, int pz) const noexcept {
		return px >= x && px < x + w && py >= y && py < y + d && pz >= z
			   && pz < z + h;
	}

	bool has_world_point(int px, int py, int pz) const noexcept {
		return Point_in_strip(x, w, px) && Point_in_strip(y, d, py) && pz >= z
			   && pz < z + h;
	}
};
#endif
