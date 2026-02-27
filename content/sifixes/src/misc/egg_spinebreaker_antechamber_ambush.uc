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
 *  Add Selina's key back to her, which was removed in gameStartModChanges.
 *  Also sets MET on the other ambushers Palos, Deadeye, and Brunt.
 *
 *  2026-02-27 Knight Captain
 */

void eggAntechamberAmbush object#(0x73A) ()
{
	if (event == EGG)
	{
		// Add back the key removed in gameStartModChanges object#(0x9F3)
		UI_add_cont_items(SELINA, 1, SHAPE_KEY, 135, 30);
		// Help for identifying the bodies.
		// The Avatar has already MET Selina.
		UI_set_item_flag(PALOS, MET);
		UI_set_item_flag(DEADEYE, MET);
		UI_set_item_flag(BRUNT, MET);
		// If we want to later add weapons and then warm clothing,
		// we could do it here in a later commit.
	}
	// I have a full re-write of the scene, but let's stay in scope of SIfixes.
	eggAntechamberAmbush.original();
}
