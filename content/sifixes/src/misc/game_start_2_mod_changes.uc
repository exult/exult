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
 *  2026-02-27 Knight Captain
 *  Creation and removing the potentially game-breaking key from Selina.
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
}
