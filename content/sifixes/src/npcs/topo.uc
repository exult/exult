/*
 *
 *  Copyright (C) 2024  The Exult Team
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
 *  Topo has schedule 26 (eat at inn) for 12PM-3PM, but the location of that
 *  business activity is where he works (Ducio's shop), so if you talk to him
 *  and ask him about buying something or selling him gems he will claim that
 *  he is not at work - which is technically correct cause he is eating at the
 *  inn... Fix that by resetting the location of his eat at inn schedule to the
 *  Blue Boar.
 */

void Topo object#(0x420) () {
	TOPO->set_new_schedules([MIDNIGHT, DAWN, NOON, AFTERNOON], [SLEEP, WANDER, EAT_AT_INN, WANDER], [0x936,0x6C8, 0x949,0x786, 0x8C8,0x710, 0x948,0x788]);

	Topo.original();
}
