/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-03-19
 */
#include "misc/blacksmithing/constants.uc"
#include "misc/blacksmithing/functions.uc"
#include "misc/blacksmithing/externs.uc"

//Anvil, reimplemented to allow custom sword forging
#include "items/anvil.uc"

//Bellows, reimplemented to shift off firepit behaviour to firepit.uc
#include "items/bellows.uc"

//Firepit, reimplemented to heat up swordblanks
#include "items/firepit.uc"

//Hammer, reimplemented to allow custom sword forging
#include "items/hammer.uc"

//Swordblank, reimplemented to allow custom sword forging
#include "items/swordblank.uc"

//Trough & well, both reimplemented due changes in bucket code
#include "items/water_trough.uc"
#include "items/well.uc"

//Menion, the weaponsmith/trainer in Serpent's Hold (added swordmaking)
#include "npcs/menion.uc";
