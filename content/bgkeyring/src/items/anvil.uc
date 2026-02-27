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

void Anvil shape#(0x3DF) () {
	//User doubleclicked the anvil - check if they're carrying a sword,
	//and if there's a swordblank on top of the anvil. If so, go to
	//the anvil and use the hammer on the swordblank.
	if (event == DOUBLECLICK) {
		if (AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_HAMMER, FRAME_ANY) ||
			//Also allow Julia's hammer:
			AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_JULIAS_HAMMER, FRAME_ANY)) {
			var swordblank = find_nearest(SHAPE_SWORDBLANK, 3);
			if (swordblank && onAnvil(swordblank, item)) {
				gotoObject(item, 0, 2, 0, useHammerOnSwordblank, swordblank, SCRIPTED);
			}
		}
	}
}
