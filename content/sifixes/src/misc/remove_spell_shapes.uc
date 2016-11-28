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
 
/*	Exult does not remove spell shapes from dying newly-added NPCs.
 *	This function will be called on the death of Elissa, Shal, and
 *	Draxinar to remove those shapes.
 *	
 *	2016-08-21 Written by Knight Captain
 */

void removeSpellShapes 0x9C3 (var npc) // Newly assigned number, unused in original Usecode.
{
	var spell;
	var spells = [SHAPE_IGNITE, SHAPE_CURSE, SHAPE_SWORDSTRIKE, SHAPE_DISPEL_MAGIC, SHAPE_LONGER_IGNITE,
				  SHAPE_MULTICOLOR_TELEKINESIS, SHAPE_PARALYZE, SHAPE_TELEKINESIS, SHAPE_DEATH_BOLT, SHAPE_DOUSE,
				  SHAPE_UNNAMED_FIRE_BOLT, SHAPE_LIGHTNING_BOLT, SHAPE_FIRE_BOLT, SHAPE_DRAGON_BREATH];

	for (spell in spells with index to max)
	{
		npc->remove_cont_items(QUANTITY_ANY, spell, QUALITY_ANY, FRAME_ANY);
	}
}
