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
 *	This source file contains miscellaneous array functions.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Returns a list of targets for friendly spells
var getFriendlyTargetList (var caster, var dist) {
	if ((caster->get_npc_object()) in UI_get_party_list()) {
		//Caster is in the party; return party:
		return UI_get_party_list();
	} else {
		//Caster not in party
		//Get all nearby NPCs:
		var npclist = getNearbyNonPartyNPCs(dist);
		var retlist = [];
		//Get caster's alignment:
		var align = caster->get_alignment();
		for (npc in npclist) {
			//For each NPC in the list,
			if (align == npc->get_alignment()) {
				//Add NPC to list if align matches caster's:
				retlist = [retlist, npc];
			}
		}
		if (align == 0) {
			//Add party to list if caster is friendly:
			retlist << UI_get_party_list();
		}
		return retlist;
	}
}

//Returns a list of targets for offensive spells
var getEnemyTargetList (var caster, var dist) {
	if ((caster->get_npc_object()) in UI_get_party_list()) {
		//If caster in party, return all non-party NPCs:
		return getNearbyNonPartyNPCs(dist);
	} else {
		//Caster not in party
		//Get all nearby NPCs:
		var npclist = getNearbyNonPartyNPCs(dist);
		var retlist = [];
		//Get caster's alignment:
		var align = caster->get_alignment();
		for (npc in npclist) {
			//For each NPC in the list,
			if (align != npc->get_alignment()) {
				//Add NPC to list if align doesn't match caster's:
				retlist = [retlist, npc];
			}
		}
		if (align != 0) {
			//Add party to list if caster is not friendly:
			retlist << UI_get_party_list();
		}
		return retlist;
	}
}

//Removes element # elemindex from the array and returns resulting array
var removeFromArrayByIndex (var array, var elemindex) {
	//var count = 1;
	var retarray = [];

	for (elem in array with index) {
		if (index != elemindex) {
			retarray << elem;
		}
	}
	return retarray;
}

//Given an NPC list, returns only those with a given flag set
var filterListByFlag(var list, var flag, var state) {
	var ret_list = [];

	for (npc in list) {
		if (npc->get_item_flag(flag) == state) {
			ret_list << npc;
		}
	}
	return ret_list;
}

//Given an NPC list, returns only those equiped with a given set of objects
//in a given spot
var filterListByEquipedObject(var list, var shapes, var spot) {
	var ret_list = [];
	if (UI_get_array_size(shapes) == 1) {
		shapes = [shapes];
	}

	for (npc in list) {
		if ((npc->get_readied(spot))->get_item_shape() in shapes) {
			ret_list << npc;
		}
	}
	return ret_list;
}

//Given an NPC list, returns only those hit points less than (max hps)*frac/4
var filterListByRelHits(var list, var frac) {
	var ret_list = [];
	for (npc in list) {
		var hps = 4*npc->get_npc_prop(HEALTH);
		var str = npc->get_npc_prop(STRENGTH);
		if (hps<str*frac) {
			ret_list << npc;
		}
	}
	return ret_list;
}

//Set-theoretical subtraction of sets; returns all the elements in list
//which are not in removes
var removeItemsFromList(var list, var removes) {
	var newarray = [];

	if (UI_get_array_size(removes) > 0) {
		if (UI_get_array_size(removes) == 1) {
			removes = [removes];
		}

		for (spell in list) {
			if (!(spell in removes)) {
				newarray << spell;
			}
		}
	} else {
		newarray = list;
	}

	return newarray;
}

//Set-theoretical intersection of sets; returns all the elements in list1
//which are also in list2
var intersectLists(var list1, var list2) {
	var newarray = [];

	if (UI_get_array_size(list2) > 0) {
		if (UI_get_array_size(list2) == 1) {
			list2 = [list2];
		}

		for (spell in list1) {
			if (spell in list2) {
				newarray << spell;
			}
		}
	} else {
		newarray = [];
	}

	return newarray;
}

var filterBodyList(var list, var body) {
	var newarray = [];

	if (UI_get_array_size(list) > 0) {
		for (currbody in list) {
			if (currbody->get_body_npc() != body->get_body_npc()) {
				newarray << currbody;
			}
		}
	} else {
		newarray = [];
	}

	return newarray;
}


//Returns a version of <array> that has <obj> stripped from it.
var removeFromArray(var array, var obj) {
	var newarray = [];

	if (UI_get_array_size(array) > 0) {
		for (curr in array) {
			if (curr != obj) {
				newarray << curr;
			}
		}
	} else {
		newarray = [];
	}

	return newarray;
}
