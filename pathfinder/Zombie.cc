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


#include "Zombie.h"
#include "ignore_unused_variable_warning.h"

/*
 *  Figure 'dir' (1 or -1) and abs. value of 'delta'.
 */
inline void Figure_dir(
    long delta,
    unsigned int &abs_delta,
    int &dir
) {
	if (delta >= 0) {
		dir = 1;
		abs_delta = delta;
	} else {
		dir = -1;
		abs_delta = -delta;
	}
}


/*
 *  Find path from source to destination.
 *
 *  Output: true if successful, else false.
 */
bool Zombie::NewPath(Tile_coord const &s, Tile_coord const &d, Pathfinder_client * client) {
	ignore_unused_variable_warning(client);
	src = s;            // Store start, destination.
	dest = d;
	cur = s;            // Get current coords.
	sum1 = sum2 = 0;        // Clear accumulators.
	long deltax = Tile_coord::delta(cur.tx, dest.tx);
	long deltay = Tile_coord::delta(cur.ty, dest.ty);
	long deltaz = Tile_coord::delta(cur.tz, dest.tz);
	if (!deltax && !deltay && !deltaz) { // Going nowhere?
		major_distance = 0;
		return false;
	}
	unsigned int abs_deltax;
	unsigned int abs_deltay;
	unsigned int abs_deltaz;
	int x_dir;
	int y_dir;
	int z_dir;    // Figure directions.
	Figure_dir(deltax, abs_deltax, x_dir);
	Figure_dir(deltay, abs_deltay, y_dir);
	Figure_dir(deltaz, abs_deltaz, z_dir);
	if (abs_deltaz >= abs_deltax && // Moving fastest along z?
	        abs_deltaz >= abs_deltay) {
		major_coord = &cur.tz;
		minor_coord1 = &cur.tx;
		minor_coord2 = &cur.ty;
		major_dir = z_dir;
		minor_dir1 = x_dir;
		minor_dir2 = y_dir;
		major_delta = abs_deltaz;
		minor_delta1 = abs_deltax;
		minor_delta2 = abs_deltay;
	} else if (abs_deltay >= abs_deltax &&  // Moving fastest along y?
	           abs_deltay >= abs_deltaz) {
		major_coord = &cur.ty;
		minor_coord1 = &cur.tx;
		minor_coord2 = &cur.tz;
		major_dir = y_dir;
		minor_dir1 = x_dir;
		minor_dir2 = z_dir;
		major_delta = abs_deltay;
		minor_delta1 = abs_deltax;
		minor_delta2 = abs_deltaz;
	} else {            // Moving fastest along x?
		major_coord = &cur.tx;
		minor_coord1 = &cur.ty;
		minor_coord2 = &cur.tz;
		major_dir = x_dir;
		minor_dir1 = y_dir;
		minor_dir2 = z_dir;
		major_delta = abs_deltax;
		minor_delta1 = abs_deltay;
		minor_delta2 = abs_deltaz;
	}
	major_distance = major_delta;   // How far to go.
	return true;
}

/*
 *  Get next point on path to go to (in tile coords).
 *
 *  Output: false if all done.
 */
bool Zombie::GetNextStep(Tile_coord &n, bool &done) {
	if (major_distance <= 0) {
		done = true;
		return false;
	}
	// Subtract from distance to go.
	major_distance -= major_frame_incr;
	// Accumulate change.
	sum1 += major_frame_incr * minor_delta1;
	sum2 += major_frame_incr * minor_delta2;
	// Figure change in slower axes.
	int minor_frame_incr1 = sum1 / major_delta;
	int minor_frame_incr2 = sum2 / major_delta;
	sum1 = sum1 % major_delta;  // Remove what we used.
	sum2 = sum2 % major_delta;  // Remove what we used.
	// Update coords. within world.
	*major_coord += major_dir * major_frame_incr;
	*minor_coord1 += minor_dir1 * minor_frame_incr1;
	*minor_coord2 += minor_dir2 * minor_frame_incr2;
	// Watch for wrapping.
	*major_coord = (*major_coord + c_num_tiles) % c_num_tiles;
	*minor_coord1 = (*minor_coord1 + c_num_tiles) % c_num_tiles;
	*minor_coord2 = (*minor_coord2 + c_num_tiles) % c_num_tiles;
	if (cur.tz < 0) {   // We are below ground level.
		cur.tz = 0;
		major_distance = 0;
		done = true;
		return false;
	}
	n = cur;            // Return new tile.
	done = (major_distance <= 0);   // Indicate if this is the last one.
	return true;
}
