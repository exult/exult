/*
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

#ifndef ZOMBIE_H
#define ZOMBIE_H


#include "PathFinder.h"


class   Zombie: public PathFinder {
	int major_distance = 0;     // Distance in tiles to go.
	int major_frame_incr;       // # steps to take in faster dir.
	Tile_coord cur;         // Current pos. within world.
	// ->'s to cur.tx, cur.ty.
	short *major_coord, *minor_coord1, *minor_coord2;
	// 1 or -1 for dir. along each axis.
	int major_dir, minor_dir1, minor_dir2;
	int major_delta, minor_delta1, minor_delta2;
	// For each tile we move along major
	//   axis, we add 'minor_delta'.  When
	//   the sum >= 'major_delta', we move
	//   1 tile along minor axis, and
	//   subtract 'major_delta' from sum.
	int sum1, sum2;         // Sum of 'minor_delta''s.
public:
	Zombie(int major_incr = 1) : major_frame_incr(major_incr)
	{  }
	// Find a path from sx,sy,sz to dx,dy,dz
	// Return false if no path can be traced.
	// Return true if path found
	bool NewPath(Tile_coord const &s, Tile_coord const &d,
	            Pathfinder_client *client) override;

	// Retrieve the coordinates of the next step on the path
	bool GetNextStep(Tile_coord &n, bool &done) override;
	int get_num_steps() override { // # of steps left to take.
		return major_distance / major_frame_incr;
	}
};

#endif
