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
 **
 *	This source file contains a modified (and vastly improved, in organizational
 *	terms) usecode function for the blacksword.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

#include "items/related_functions/arcadion_dialog.uc"		//Arcadion's dialog (broken into three functions) and related functions

void arcadionDialog object#(0x6F6) () {
	//Completelly reorganized and modularized this function. In the original, all of
	//the dialog is contained in a single function... I think things could still be
	//improved (e.g., by modifying the usecode for mirrors/gems/the black sword so that
	//they are the ones to call the dialog), but this is sufficient for the moment:

	if (!gflags[BROKE_MIRROR]) {
		item->arcadionMirrorFormDialog();
	} else if (!gflags[COMMANDED_BOND]) {
		if (event == DOUBLECLICK) {
			UI_close_gumps();
			script item after 1 ticks
				call arcadionGemFormDialog;
		} else if (event == SCRIPTED) {
			item->arcadionGemFormDialog();
		}
	} else {
		if (event == DOUBLECLICK) {
			UI_close_gumps();
			script item after 1 ticks
				call arcadionSwordFormDialog;
		} else if (event == SCRIPTED) {
			item->arcadionSwordFormDialog();
		}
	}
}

var isWorthyToKill 0x848 (var target_shape) {
	var killables = [
		SHAPE_DRAGON, SHAPE_DRAKE, SHAPE_ETHEREAL_MONSTER, SHAPE_HYDRA, SHAPE_UNICORN,
		SHAPE_FAIRY, SHAPE_WISP, SHAPE_MONSTER_CYCLOPS, SHAPE_CYCLOPS, SHAPE_DRAXINUSOM,
		SHAPE_IOLO, SHAPE_SHAMINO, SHAPE_DUPRE, SHAPE_SPARK, SHAPE_HOOK,
		SHAPE_MAGE_MALE, SHAPE_MAGE_FEMALE, SHAPE_MONSTER_MAGE
	];

	return (target_shape in killables);
}

var isUndead 0x849 (var target_shape) {
	var undead = [SHAPE_LICHE2, SHAPE_LICHE, SHAPE_SKELETON, SHAPE_GHOST2, SHAPE_GHOST_MALE, SHAPE_GHOST_FEMALE];

	return (target_shape in undead);
}

void teleportIsleOfFire object#(0x6F9) () {
	if (event == SCRIPTED) {
		if (get_item_shape() != SHAPE_BLACKSWORD) {
			//Forward to original:
			teleportIsleOfFire.original();
		} else {
			//Modified this part simply for multimap compatibility:
			var dest = [0x890, 0x5FA, 0x0, 0x0];
			if (!UI_get_item_flag(AVATAR, ON_MOVING_BARGE)) {
				UI_fade_palette(12, 1, 0);
				PARTY->move_object(dest);
				script AVATAR {
					face north;
					wait 1;
					call teleportIsleOfFire;
				}
			}
		}
	} else {
		//Forward to original:
		teleportIsleOfFire.original();
	}
}
