/*
 *
 *  Copyright (C) 2006  The Exult Team
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
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-03-19
 */

extern void useBucketOnWell ();
void WellBase shape#(0x1D6) () {
	var bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 0);
	if (bucket) {
		var offsetx = [-5, -5];
		var offsety = [-1, -1];
		gotoObject(item, offsetx, offsety, 0, useBucketOnWell, bucket, 9);
	}
}

void Well shape#(0x2E4) () {
	var bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 0);
	if (bucket) {
		var offsetx = [-5, -5];
		var offsety = [-1, -1];
		var well = find_nearest(SHAPE_WELLBASE, 5);
		if (well) {
			gotoObject(well, offsetx, offsety, 0, useBucketOnWell, bucket, 9);
		}
	}
}
