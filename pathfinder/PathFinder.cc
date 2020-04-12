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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "PathFinder.h"

/*
 *  Given the estimated cost from start to goal, figure the max. cost
 *  before the pathfinder should quit.
 */

int Pathfinder_client::get_max_cost(
    int cost_to_goal        // From estimate_cost(start, goal).
) {
	int max_cost = 3 * cost_to_goal;
	// (Raised from 64 on 9/4/2000).
	return max_cost < 74 ? 74 : max_cost;
}

/*
 *  Is tile at goal?
 */

bool Pathfinder_client::at_goal(
    Tile_coord const &tile,
    Tile_coord const &goal
) {
	return tile.tx == goal.tx && tile.ty == goal.ty &&
	        (goal.tz == -1 || tile.tz == goal.tz);
}
