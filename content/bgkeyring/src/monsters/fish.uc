/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-03-19
 */

//Live fish originally didn't do a damn thing - this makes them plop
//out of the water (using unimplemented animations from the original)

const int SOUND_FISH = 40;	//splashy sound
void Fish shape#(0x1FD) () {
	if (event == PROXIMITY) {
		var frames;
		if (UI_get_item_frame(item) < 16) {
			//use unrotated (north-facing) animation set
			frames = [14, 13, 5, 15, 0];
		} else {
			//use rotated (south-facing) animation set (slightly different order too)
			frames = [30, 21, 29, 31, 16];
		}

		script item {
			//keep the fish from swimming during sequence
			call freeze;
			//(according to their schedule)

			frame frames[1];
			wait 1;
			sfx SOUND_FISH;
			frame frames[2];
			frame frames[3];
			frame frames[4];
			wait 1;
			frame frames[5];

			//let it swim around again
			call unfreeze;
		}
	}
}
