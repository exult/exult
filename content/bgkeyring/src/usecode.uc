/*
 *
 *  Copyright (C) 2006  The Exult Team
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

//Tells the compiler the game type
#game "blackgate"
// Want to enforce braces around all statements
#strictbraces "true"

//Starts autonumbering at function number 0xC00.
//I leave function numbers in the range 0xA00 to
//0xBFF for weapon functions; this is a total of
//512 unique functions. That is likely much more
//than enough...
#autonumber 0xC00

#include "headers/constants.uc"					//standard constant definitions
#include "headers/constants2.uc"				//standard constant definitions

#include "headers/bg/bg_npcs.uc"				//Black Gate npc constants
#include "headers/npcs.uc"						//New npc constants

#include "headers/bg/bg_shapes.uc"				//Black Gate shape and frame constants
#include "headers/bg/bg_shapes2.uc"				//Black Gate shape and frame constants
#include "headers/new_shapes.uc"				//Brand new shape and frame constants

#include "headers/bg/bg_gflags.uc"				//Black Gate global flags
#include "headers/bg/bg_gflags2.uc"				//Black Gate global flags
#include "headers/new_flags.uc"					//Brand new global flags

#include "headers/bg/bg_structs.uc"				//standard structure definitions
#include "headers/bg/bg_externals.uc"			//extern declarations for BG functions
#include "headers/bg/bg_externals2.uc"			//extern declarations for BG functions

#include "headers/new_items.uc"					//extern declarations for new items

#include "headers/functions.uc"					//new general-purpose functions
#include "headers/functions2.uc"				//Some new general-purpose functions
#include "headers/array_functions.uc"			//Several array functions

//NPC Spellcasting Mod
#include "spells/main_spells.uc"

//The Keyring Quest:
#include "quests/keyring.uc"

//The Codex Quest:
#include "quests/codex.uc"

//Lock Lake Cleanup:
#include "quests/locklake/cleanup_eggs.uc"		//Eggs that gradually clean Lock Lake
#include "npcs/lordheather.uc"					//Kick start Lock Lake cleaning by signing bill

//Inn Keys Reclaimed:
#include "misc/inn_key_eggs.uc"					//Deletes inn keys, tidies up beds and locks inn doors

//For the Improved Orb of the Moons:
#include "items/orb_of_the_moons.uc"			//New Orb of the Moons
#include "npcs/wisps.uc"						//Modified wisps that are compatible with the new orb

//Items modified for one reason or another:
#include "items/rings.uc"						//Fixes rings so that they must be on your finger to work
#include "items/remote_viewers.uc"				//Multimap support of orrery viewer, Isle of Fire gem and crystal balls

//From Alun Bestor's Quests & Iteractions mod:
#include "misc/iteractions.uc"					//Adds the new item iteractions

//The Crown Jewels:
#include "items/crown_jewels.uc"

//Fixes gang planck blocking check
#include "misc/is_obj_blocked.uc"

//Fixing missing conversation branch with Tseramed
#include "npcs/tseramed.uc"
