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

enum food_levels {
	FOODLEVEL_PECKISH	= 5,	//below this level, the character is starving
								//(taking damage from hunger?)
	FOODLEVEL_WELLFED	= 10,	//below this level, the character is peckish
								//(just complaining about hunger?)
	FOODLEVEL_STUFFED	= 20,	//at or above this level, the character is stuffed
								//(this changes their barks)
	FOODLEVEL_FULL		= 25	//at or above this level, the character is full
								//(refuses more food)
};

//flags (event ids, actually) to specify what kind of milk container is being
//churned/milked with
enum milk_types {
	CHURN_WITH_BOTTLE	= 1,
	CHURN_WITH_BUCKET	= 2,
	CHURN_WITH_PITCHER	= 3,

	MILK_WITH_BUCKET	= 2,
	MILK_WITH_PITCHER	= 3
};

const int CHURN_SOUND = 4;

const int SOUND_MILKING = 2;

const int SOUND_EAT = 91;
const int SOUND_DRINK = 90;
const int BOTTLE_NUTRITION = 1;
