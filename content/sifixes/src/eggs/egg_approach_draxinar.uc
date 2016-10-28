/*	Copyright (C) 2016  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/*	In original release of The Silver Seed, the green dragon Draxinar just
 *	stands in one spot. Now that he has been converted to a "real" numbered
 *	NPC with a schedule, we can have him react to hearing the Avatar approach.
 *	
 *	Since we want to set a value for this new egg and set it on the map, an
 *	unused SI object number has been assigned rather than relying on the
 *	autonumber function which could change the value in later recompilations.
 *	
 *	2016-10-27 Written by Knight Captain
 */

void eggApproachDraxinar object#(0x6F2) ()
{
	if (!DRAXINAR->get_item_flag(MET) && DRAXINAR->get_schedule_type(!SLEEP))
	{
		delayedBark(DRAXINAR, "@Hmmm.@", 0);
		DRAXINAR->set_schedule_type(TALK);
	}
}
