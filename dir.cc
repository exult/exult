/*
 *  dir.cc - Directions.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "dir.h"

#include "exult_constants.h"

static int Wrap_Delta(int delta) {
	if (delta >= c_num_tiles / 2) {    // World-wrapping.
		return c_num_tiles / 2 - delta;
	} else if (delta <= -c_num_tiles / 2) {
		return -c_num_tiles / 2 - delta;
	} else {
		return delta;
	}
}

/*
 *  Return the direction for a given slope (0-7).
 *  NOTE:  Assumes cartesian coords, NOT screen coords. (which have y
 *      growing downwards).
 */

Direction Get_direction_NoWrap(int deltay, int deltax) {
	if (deltax == 0) {
		return deltay > 0 ? north : south;
	}
	const int dydx = (1024 * deltay) / deltax;    // Figure 1024*tan.
	if (dydx >= 0) {
		if (deltax >= 0) {    // Top-right quadrant?
			return dydx <= 424 ? east : dydx <= 2472 ? northeast : north;
		} else {    // Lower-left.
			return dydx <= 424 ? west : dydx <= 2472 ? southwest : south;
		}
	} else if (deltax >= 0) {    // Lower-right.
		return dydx >= -424 ? east : dydx >= -2472 ? southeast : south;
	} else {    // Top-left?
		return dydx >= -424 ? west : dydx >= -2472 ? northwest : north;
	}
}

/*
 *  Return the direction for a given slope (0-7).
 *  NOTE:  Assumes cartesian coords, NOT screen coords. (which have y
 *      growing downwards).
 */

Direction Get_direction(int deltay, int deltax) {
	return Get_direction_NoWrap(Wrap_Delta(deltay), Wrap_Delta(deltax));
}

/*
 *  Return the direction for a given slope (0-7), rounded to NSEW.
 *  NOTE:  Assumes cartesian coords, NOT screen coords. (which have y
 *      growing downwards).
 */

Direction Get_direction4(int deltay, int deltax) {
	deltax = Wrap_Delta(deltax);
	deltay = Wrap_Delta(deltay);
	if (deltax >= 0) {    // Right side?
		return deltay > deltax ? north : deltay < -deltax ? south : east;
	} else {    // Left side.
		return deltay > -deltax ? north : deltay < deltax ? south : west;
	}
}

/*
 *  Return the direction for a given slope (0-15).
 *  NOTE:  Assumes cartesian coords, NOT screen coords. (which have y
 *      growing downwards).
 */

int Get_direction16(int deltay, int deltax) {
	deltax = Wrap_Delta(deltax);
	deltay = Wrap_Delta(deltay);
	if (deltax == 0) {
		return deltay > 0 ? 0 : 8;
	}
	const int dydx  = (1024 * deltay) / deltax;    // Figure 1024*tan.
	const int adydx = dydx < 0 ? -dydx : dydx;
	int       angle = 0;
	if (adydx < 1533) {       // 1024*tan(5*11.25)
		if (adydx < 204) {    // 1024*tan(11.25).
			angle = 4;
		} else if (adydx < 684) {    // 1024*tan(3*11.25).
			angle = 3;
		} else {
			angle = 2;
		}
	} else {
		if (adydx < 5148) {    // 1024*tan(7*11.25).
			angle = 1;
		} else {
			angle = 0;
		}
	}

	if (deltay < 0) {    // Check quadrants.
		if (deltax > 0) {
			angle = 8 - angle;
		} else {
			angle += 8;
		}
	} else if (deltax < 0) {
		angle = 16 - angle;
	}
	return angle % 16;
}
