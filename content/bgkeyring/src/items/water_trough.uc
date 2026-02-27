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

extern void useBucketOnTrough ();
void VerticalTrough shape#(0x2CF) () {
	var offsetx = [1, 1, -2, -2, 0, -1, 0, -1];
	var offsety = [-1, -2, -1, -2, 1, 1, -4, -4];
	var var0003;
	var bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 1);
	if (!bucket) {
		bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 0);
	}
	if (bucket) {
		gotoObject(item, offsetx, offsety, 0, useBucketOnTrough, bucket, 7);
	}
}

void HorizontalTrough shape#(0x2E5) () {
	var offsetx = [-1, -2, -1, -2, 1, 1, -4, -4];
	var offsety = [1, 1, -2, -2, 0, -1, 0, -1];
	var var0003;
	var bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 1);
	if (!bucket) {
		bucket = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, 0);
	}
	if (bucket) {
		gotoObject(item, offsetx, offsety, 0, useBucketOnTrough, bucket, 7);
	}
}
