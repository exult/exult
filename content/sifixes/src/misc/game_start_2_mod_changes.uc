/*
 *
 *  Copyright (C) 2026  The Exult Team
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

/*
 *  To match my in-progress mod, I use this new function for all mod changes
 *  that occur at the start of the game.
 *
 *  This should only be used for NPC changes, as map changes outside of the
 *  nine superchunks around the Avatar will not work as expected.
 *
 *  2026-02-27 Knight Captain
 *  Creation and removing the potentially game-breaking key from Selina.
 *  2026-03-12 Delin and Olon fix by KC.
 */

void gameStartModChanges object#(0x9F3) () // 2547
{
	// The key Selina starts with can potentially be used to skip parts of
	// Spinebreaker, permanently preventing the player from accessing areas.
	// By removing this key, the player cannot get the key until Selina is
	// slain after those areas of Spinebreaker. See these issue links:
	// https://github.com/exult/exult/issues/310
	// https://github.com/exult/exult/issues/731
	// Doing this via code to avoid editing binary files using Exult Studio.
	UI_remove_cont_items(SELINA, 1, SHAPE_KEY, 135, 30);
	
	// In Fawn, Jendon serves food to Lady Yelinda and other leaders at 9PM.
	// Delin and Olon sit waiting for food in The Broken Oar, never eating.
	// See this issue link:
	// https://github.com/exult/exult/issues/919
	// Delin sits with Alyssand, she faces east at 0931, 1747, in EAT.
	UI_modify_schedule(DELIN, NIGHT, EAT, [0935, 1747]);
	// Make Olon the Drunken Sailor Man eat. There is no DRINK activity.
	UI_modify_schedule(OLON, NIGHT, EAT, [0946, 1759]);
}
