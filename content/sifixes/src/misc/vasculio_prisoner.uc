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
 *	The skeleton locked in the magic cell in Vasculio's house, Skullcrusher
 *	(issue #1015).
 *
 *	An egg (0x8BB) creates the prisoner if there isn't one and sets him
 *	begging. Unlock Magic on the cell (0x91D) turns him hostile and opens the
 *	door. Neither set a flag, so after he was killed the egg made a new
 *	prisoner the next time the Avatar came back, begging behind an open
 *	door. Now freeing him sets VASCULIO_PRISONER_FREED, and the egg does
 *	nothing once it is set.
 */

// The egg in the cell: creates the prisoner (quality 1) and starts the
// begging loop.
void VasculioPrisonerEgg 0x8BB () {
	if ((event == EGG) && gflags[VASCULIO_PRISONER_FREED]) {
		return;
	}
	VasculioPrisonerEgg.original();
}

// Unlock Magic cast on the cell door.
void VasculioPrisonerFreed 0x91D () {
	// Same test as the original: a skeleton inside the cell, on the ground
	// floor. Only then does the original free him.
	var prisoners = AVATAR->find_nearby(SHAPE_SKELETON, 40, MASK_NPC2);
	if (prisoners) {
		var pos = prisoners[1]->get_object_position();
		if ((pos[X] >= 0x432) && (pos[X] <= 0x439) && (pos[Y] >= 0x3E4) && (pos[Y] <= 0x3EB)
			&& (pos[Z] == 0)) {
			gflags[VASCULIO_PRISONER_FREED] = true;
		}
	}
	VasculioPrisonerFreed.original();
}
