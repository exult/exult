/*  Copyright (C) 2016  The Exult Team
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
 
/*	The opening sequence in SI is partly done via an egg that spawns a rabbit
 *	that is on Patrol. The Avatar's proximity to this rabbit triggers the
 *	function connected to it, handling the teleportion of the Avatar to a
 *	spot with the ship on the ground and triggers Iolo's opening speech.
 *	It also sets the Tournament flag on many NPCs since this is not saved in
 *	their initial settings. That NPC flag is not used in BG.
 *
 *	This code sets the Tournament flag on some of The Silver Seed NPCs.
 *
 *	2016-09-01 Written by Knight Captain
 */

void funcRabbit shape#(0x32B) ()
{
	if ((event == PROXIMITY) && (UI_get_schedule_type(item) == PATROL))
	{
		funcRabbit.original();
		// The original function seems to wipe certain NPC flags and then resets
		// them as needed. My guess is this was done to clear anything left over
		// from the BG NPCs since their names and alignments were not fully wiped.
		// Any new NPC flags must be set after this point.

		ELISSA->set_item_flag(SI_TOURNAMENT);
		SHAL->set_item_flag(SI_TOURNAMENT);
		DRAXINAR->set_item_flag(SI_TOURNAMENT);
	}
	else
	funcRabbit.original();
}
