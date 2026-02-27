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
 *
 *	This source file contains the code for normal keys for compatibility
 *	with the keyring.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Key shape#(0x281) () {

	//If we didn't came here via double-click, leave:
	if (!(event == DOUBLECLICK)) {
		return;
	}

	//Prompt player for target:
	struct<ObjPos> target = UI_click_on_item();
	//var target_quality;

	//Target is the keyring:
	if (target->get_item_shape() == SHAPE_KEYRING) {
		get_keyring()->add_to_keyring(target, item);
		return;
	}

	KeyInternal(target,
				(get_item_quality() == target->get_item_quality()),
				["@The key doesn't fit.@", "@Maybe it is another key.@"]);
}
