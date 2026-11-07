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
 *
 *
 *	This header files has includes for all the files of the NPC Spellcasting mod.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */
#include "spells/spell_functions.uc"			//Spell functions used by a few spells
#include "spells/linear_spells.uc"				//Linear spells for NPCs
#include "spells/first_circle.uc"				//First Circle spells for NPCs
#include "spells/second_circle.uc"				//Second Circle spells for NPCs
#include "spells/third_circle.uc"				//Third Circle spells for NPCs
#include "spells/fourth_circle.uc"				//Fourth Circle spells for NPCs
#include "spells/fifth_circle.uc"				//Fifth Circle spells for NPCs
#include "spells/sixth_circle.uc"				//Sixth Circle spells for NPCs
#include "spells/seventh_circle.uc"				//Seventh Circle spells for NPCs
#include "spells/eighth_circle.uc"				//Eighth Circle spells for NPCs
#include "spells/npc_spells.uc"					//Generic spellcasting routines for NPCs
#include "items/spell_items.uc"					//For NPC spellcasting

//Spellcasting AI:
#include "spells/spell_ai.uc"

//Spellbook override:
#include "spells/spellbook_override.uc"			//Makes avatar use the new spells
#include "misc/services.uc"						//General services (selling, healing)

//Modified NPCs:
#include "npcs/jaana.uc"						//For NPC spellcasting
#include "npcs/mariah.uc"						//For NPC spellcasting
#include "npcs/csil.uc"							//Now uses the Generic Healing Service
#include "npcs/elad.uc"							//Now uses the Generic Healing Service
#include "npcs/leigh.uc"						//Now uses the Generic Healing Service
#include "npcs/inmanilem.uc"					//Now uses the Generic Healing Service
#include "npcs/chantu.uc"						//Now uses the Generic Healing Service
#include "npcs/lordbritish.uc"					//Now uses the Generic Healing Service
