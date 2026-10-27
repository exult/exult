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
 *	Original Author: Alun Bestor
 *	Modified By: Marzo Junior
 *	Last Modified: 2006-03-19
 */
#include "npcs/npcs.uc"				//generic NPC dialogue-related functions

//For blacksmithing:
#include "misc/blacksmithing.uc"

//Milking cows, churning butter, baking, etc.
#include "misc/food_stuff.uc"

//Buckets have been changed for milking cows and baking:
#include "items/bucket.uc"
#include "items/wheat.uc"			//Grinding wheat

//#include "items/key.uc"			//Keys
#include "items/cloth.uc"			//Cloth, thread and looms

//For shearing sheep:
#include "misc/shearing.uc"

//For fishing & selling fish
#include "misc/fishing.uc"
#include "items/pocketwatch.uc"		//Corrects am/pm bug
#include "items/baby.uc"			//Allows you to put Lady Tory's baby into a cradle
#include "npcs/tory.uc"				//Lady Tory, the empath in Serpent's Hold (general dialogue improvements)

//Monsters
#include "monsters/chicken.uc"		//Chickens!

//NPCs
#include "npcs/camille.uc"			//Camille the widow in Paws (adds a good deed)
#include "npcs/willy.uc"			//Willy the baker in Britain (fixes original conversation bug)
#include "items/misc.uc"			//Miscellaneous interactions
