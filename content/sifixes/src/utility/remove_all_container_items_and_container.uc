/*
 *
 *  Copyright (C) 2023  The Exult Team
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

/*  2023-03-15 Added to support the new face and usecode for SmithzHorse,
 *  who appears in the Dream Realm but does not have a proper face assignment.
 *
 *  Thank you to Rhona for the artwork.
 */

// Remove all items, and the container, typically an NPC

void removeAllItemsAndContainer 0x9A3 (var npc_or_item)
{
	var container_items;
	var index;
	var max;
	var each_item;

	container_items = UI_get_cont_items(npc_or_item, SHAPE_ANY, QUALITY_ANY, FRAME_ANY);
	// If there are any items in the container, remove them.
	if (container_items)
	{
		for (each_item in container_items with index to max)
			UI_remove_item(each_item);
	}			 

	// And remove the NPC or item.
	// From the documentation: Removes the object from the game world,
	// and from the 'last created' stack. If the object is not an NPC,
	// it will de deleted; NPCs will still exist.
	UI_remove_item(npc_or_item);
	return;
}
