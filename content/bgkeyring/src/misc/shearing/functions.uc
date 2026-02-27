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

//create the wool beside the Avatar
void finishShearing object#() () {
	var wool = UI_create_new_object(SHAPE_WOOL);

	//vertical bale
	wool->set_item_frame(1);
	wool->set_item_flag(TEMPORARY);

	//place the new bale next to the sheep
	struct<Position> target_pos = get_object_position();
	target_pos.x += 1;
	target_pos.y += 1;

	//Todo: stick it in the players inventory if it can't be placed here
	UI_update_last_created(target_pos);

	//Make the sheep run away after the deed is done
	set_schedule_type(SHY);

	//This prevents us from shearing it twice
	set_item_flag(MET);
}

//Shear that sheep!
void shearSheep () {
	script AVATAR {
		nohalt;
		//paralyze the avatar so the player can't move during sequence
		call freeze;

		face directionFromAvatar(item);
		say("@Easy now...@");

		repeat 6 {
			actor frame standing;
			actor frame reach_1h;
			sfx SOUND_SHEARING;
			actor frame strike_1h;
			wait 2;
		};
		actor frame standing;

		//unparalyze again
		call unfreeze;
	}

	script item {
		nohalt;
		call freeze;
		wait 20;
		say "@Baaa!@";
		wait 20;
		say "@Baa-aa-aa!@";
		call unfreeze;
		//Create the wool
		call finishShearing;
	}

	//our resident sheep expert
	if (inParty(KATRINA)) {
		var barks = [
			"@Thou art doing it all wrong!@",
			"@Nay, work thy way forwards!@",
			"@Thou shouldst be a shepherd!@"
		];
		delayedBark(KATRINA, randomIndex(barks), 20);
	}
}

//Organise everything for shearing, and go to the sheep
void startShearing (var sheep) {
	UI_close_gumps();

	//The sheep is already sheared
	if (sheep->get_item_flag(SHEARED)) {
		randomPartySay("@Thou hast sheared that sheep already! Give the poor thing a chance to grow it back.@");
		return;
	}

	//make sure the shears are readied first
	if (!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_SHEARS, FRAME_ANY)) {
		randomPartySay("@Thou must have the shears in thy hand.@");
		return;
	}

	//Stop the sheep from moving around before we get there.
	//Note: they will be unfrozen once they have been sheared.
	//Unfortunately, just using a simple wait (or wait-loop)
	//doesn't stop the GRAZE schedule from making the sheep
	//wander off, so we actually have to paralyze them to keep
	//them still.
	script sheep {
		nohalt;
		call freeze;
	}

	//Get in behind the sheep (oh, the jokes never cease)
	var sheep_offsetx = [0, -1, 1];
	var sheep_offsety = [1, 1, 1];
	gotoObject(sheep, sheep_offsetx, sheep_offsety, 0, shearSheep, sheep, SCRIPTED);

	//Hat tip to Marzo for this one - this will unfreeze the
	//sheep if the player cancels the go-to
	UI_set_path_failure(unfreeze, sheep, SCRIPTED);
}
