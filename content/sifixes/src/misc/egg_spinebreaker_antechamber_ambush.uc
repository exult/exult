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
 *  2026-03-03 Improved method to give Selina the key, no matter what. KC
 */

void eggAntechamberAmbush object#(0x73A) ()
{
	if (event == EGG)
	{
		var destination;
		
		// Add back the key removed in gameStartModChanges object#(0x9F3)
		// UI_add_cont_items fails if Selina has no container and her hands full.
		// To more closely match the original game, Selina's key should go into
		// her backpack. UI_find_object will return a reference to it, if found.
		var selinas_backpack = UI_find_object(SELINA, SHAPE_BACKPACK, QUALITY_ANY, FRAME_ANY);
		if (selinas_backpack != 0)
		{
			UI_error_message("Selina is wearing a backpack.");
			destination = selinas_backpack;
		}
		else
		{
			UI_error_message("Selena is NOT wearing a backpack.");
			destination = SELINA;
		}
		
		// This more complicated method should ensure success in placing the key.
		// I have tested this with using hack mover to overload her with weight,
		// and filling her equipment slots, hands, etc. It works for all cases.
		var new_key = UI_create_new_object(SHAPE_KEY);
		if (new_key)
		{
			UI_set_item_frame(new_key, 30);
			var key_quality = UI_set_item_quality(new_key, 135);
			UI_clear_item_flag(new_key, TEMPORARY);
			key_quality = UI_give_last_created(destination);
			
			// Write output confirming the key's creation.
			if (key_quality == true) {
				UI_error_message("Selena's key created on her."); }
			else {
				UI_error_message("Selina's key FAILED to be created on her."); }
		}

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
