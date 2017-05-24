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
 
/*	Now that Ernesto is implemented as a standard NPC with a schedule,
 *	spawning the prior version is unwanted. Rather than delete the eggs
 *	around Moonshade, this bit of code causes the eggs to do nothing.
 *
 *	His spawn schedule was done using imperfect time operators. He would
 *	be duplicated at times and not spawned at others. These were roughly:
 *	Midnight to 7AM, Sleep at 2177, 1846. Only egg with a quality check.
 *	8AM to 3PM, Tend Shop at 2193, 1863.
 *	5PM to 8PM, Patrol at 2263, 2095 but fails over to Loiter, no path eggs.
 *	After 8PM, Tend Shop again, but not clear where, possibly 2173, 1883.
 *	
 *	2016-08-14 Written by Knight Captain
 */

void eggSpawnErnesto object#(0x6C9) ()
{
	return;
}
