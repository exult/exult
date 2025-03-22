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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Astar.h"

#include "path.h"

/*
 *  Find path from source to destination.
 *
 *  Output: true if successful, else false.
 */
bool Astar::NewPath(
		const Tile_coord& s, const Tile_coord& d,
		const Pathfinder_client* client) {
	auto [new_path, success] = Find_path(s, d, client);

	src        = s;    // Store start, destination.
	dest       = d;
	path       = std::move(new_path);
	next_index = 0;
	dir        = 1;
	stop       = path.size();
	return success;
}

/*
 *  Get next point on path to go to (in tile coords).
 *
 *  Output: false if all done.
 */
bool Astar::GetNextStep(Tile_coord& n, bool& done) {
	if (next_index == stop) {
		done = true;
		return false;
	}
	n = path[next_index];
	next_index += dir;
	done = (next_index == stop);
	return true;
}

/*
 *  Set to traverse backwards.
 *
 *  Output: true always (we succeeded).
 */
bool Astar::set_backwards() {
	dir        = -1;
	stop       = -1;
	next_index = path.size() - 1;
	return true;
}

/*
 *  Get # steps left.
 */
int Astar::get_num_steps() {
	return (stop - next_index) * dir;
}
