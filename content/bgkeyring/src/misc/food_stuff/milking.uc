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

void milkCow () {
	var cow = item;
	//the empty bucket/pitcher used for milking
	var vessel;
	//the correct full-of-milk frame for the bucket/pitcher
	var filled_frame;

	//A bucket is being filled
	if (event == 2) {
		//get the first empty bucket we can find that the avatar is carrying
		vessel = AVATAR->get_cont_items(SHAPE_BUCKET, QUALITY_ANY, FRAME_BUCKET_EMPTY);
		filled_frame = FRAME_BUCKET_MILK;
	} else if (event == 3) {
		//A milk pitcher is being filled
		//get the first pitcher we can find that the avatar is carrying
		vessel = AVATAR->get_cont_items(SHAPE_KITCHEN_ITEM, QUALITY_ANY, FRAME_PITCHER);
		filled_frame = FRAME_PITCHER_MILK;
	}

	//place the vessel on the ground beneath the cow
	//disabled: this usually results in it being rendered on top of the cow,
	//and anyway it appears immediately rather than at the end of a leaning-down
	//animation.
	//struct<Position> target_pos = UI_get_object_position(cow);
	//target_pos.x -= 2;
	//moveToLocation(vessel, target_pos);

	if (UI_get_random(10) < 3) {
		delayedBark(AVATAR, "@Hold still...@", 15);
	}

	//UI_set_schedule_type(cow, GRAZE);

	script AVATAR {
		nohalt;
		call freeze;
		face west;
		//lean down to place the bucket
		actor frame standing;
		actor frame bowing;
		wait 2;
		actor frame standing;
		wait 2;

		repeat 3 {
			sfx SOUND_MILKING;
			actor frame ready;
			wait 2;
			actor frame standing;
			wait 2;
		};

		say "@That's a good girl!@";
		call unfreeze;
	}
	script vessel after 25 ticks frame filled_frame;
	script cow {
		nohalt;
		call freeze;
		wait 20;
		say "@MoooOOOoooo@";
		wait 5;
		call unfreeze;
	}

	//Sarky bastards
	if (UI_get_random(3) == 1) {
		delayedBark(randomPartyMember(), "@Don't we have a quest?@", 35);
	}
}

//Called by Bucket() and Pitcher() - directs the player to the cow, and
//freezes the cow in place till they get there. Calls milkCow() when the
//player arrives, using eventid to flag whether a bucket (2) or pitcher
//(3) was used
void gotoCow (var cow, var milktype) {
	UI_close_gumps();

	//stop the cow from moving around
	//UI_set_schedule_type(cow, WAIT);

	//make sure the cow is facing the right way for milking (so pathfinding
	//will work properly), and stop them moving around. Note: they will be
	//unfrozen once they have been milked, or if the player decides not to
	//milk them. Unfortunately, just using a simple wait (or wait-loop)
	//doesn't stop the GRAZE schedule from making the cow wander off, so
	//we actually have to paralyze them to keep them still.
	script cow {
		nohalt;
		call freeze;
		face east;
	}

	//Go to the cow's udder region (either side will do)
	var cow_offsetx = [-2, -2];
	var cow_offsety = [1, -4];
	gotoObject(cow, cow_offsetx, cow_offsety, 0, milkCow, cow, milktype);

	//Hat tip to Marzo for this one - this will unfreeze the cow if the player
	//cancels the go-to
	UI_set_path_failure(unfreeze, cow, SCRIPTED);
}
