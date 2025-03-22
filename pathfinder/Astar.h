/*
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

#ifndef ASTAR_H
#define ASTAR_H

#include "PathFinder.h"

#include <vector>

class Astar : public PathFinder {
	std::vector<Tile_coord> path;              // Coords. to goal.
	int                     dir        = 0;    // 1 or -1.
	int                     stop       = 0;    // Index to stop at.
	int                     next_index = 0;    // Index of next tile to return.
public:
	// Find a path from sx,sy,sz to dx,dy,dz
	// Return false if no path can be traced.
	// Return true if path found
	bool NewPath(
			const Tile_coord& s, const Tile_coord& d,
			const Pathfinder_client* client) override;

	// Retrieve the coordinates of the next step on the path
	bool GetNextStep(Tile_coord& n, bool& done) override;
	// Set to retrieve in opposite order.
	bool set_backwards() override;

	bool following_smart_path() override {    // Astar?
		return true;
	}

	int get_num_steps() override;    // # of steps left to take.
};

#endif
