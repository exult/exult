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

//Changes use message to mention sewing into garments (and to stop saying
//that it would "fetch a fair price in Minoc", as there's noone you can
//sell cloth to in the entire game!)
void Cloth shape#(0x353) () {
	if (event == DOUBLECLICK) {
		randomPartySay("@That appears to be fine cloth, thou couldst sew it into clothing if thou hast any thread.~@Or perhaps when we are in a fix, thou couldst cut it into bandages with shears.@");
	}
}

//sew a bolt of cloth into your choice of pretty garment
//Todo: different kinds of garments need different materials, possibly
//multiple materials (e.g. Avatar Costume)
void sewCloth (var cloth) {
	var garment_shape;
	var garment_frame;
	AVATAR.say("What sort of garment dost thou wish to sew from this cloth?");
	converse (["shirt", "cloak", "dress", "trousers", "Avatar costume", "cuddly toy", "nothing"]) {
		case "shirt":
			garment_shape = SHAPE_TOP;
			garment_frame = FRAME_SHIRT;
			break;
		case "cloak":
			garment_shape = SHAPE_CLOAK;
			garment_frame = UI_die_roll(0,3);
			break;
		case "dress":
			garment_shape = SHAPE_TOP;
			garment_frame = UI_die_roll(1,2);
			break;
		case "trousers":
			garment_shape = SHAPE_PANTS;
			garment_frame = UI_die_roll(0,5);
			break;
		case "Avatar costume":
			garment_shape = SHAPE_COSTUME;
			garment_frame = 0;
			break;
		case "cuddly toy":
			garment_shape = SHAPE_TOY;
			garment_frame = UI_die_roll(0,1);
			break;
		case "nothing":
			break;
	}

	if (garment_shape) {
		//cuddly toys also need wool for stuffing!
		//Note: for now this doesn't actually remove the wool, since
		//only a small amount would be needed
		if (garment_shape == SHAPE_TOY) {
			if (!PARTY->count_objects(SHAPE_WOOL, QUALITY_ANY, FRAME_ANY)) {
				AVATAR.say("Thou must also have some wool with which to stuff the toy.");
				return;
			}
		}

		remove_item();	//get rid of the thread
		//change the cloth into the garment (hopefully this will carry
		//over the right properties too: it appears to carry over
		//dimensions at least)
		cloth->set_item_shape(garment_shape);
		cloth->set_item_frame(garment_frame);

		//reset these flags just in case
		cloth->set_item_flag(OKAY_TO_TAKE);
		cloth->clear_item_flag(TEMPORARY);

		//now, say some comments about it
		avatarBark("@Voila!@");

		var comments;
		if (garment_shape == SHAPE_TOY) {
			comments = ["@Aw, 'tis so cute!@", "@Could we name him Draco?@"];
		} else {
			comments = [
				"@T'will look divine on thee!@",
				"@Tres elegant!@",
				"@'Tis all the rage in Britain.@"
			];
		}

		delayedBark(randomPartyMember(), randomIndex(comments), 10);
	}
}

extern void Loom shape#(0x105) ();
void Thread shape#(0x28E) () {
	//Player doubleclicked on thread: march them to the thread and pick it up
	//Once we have it, this function gets called again as event = SCRIPTED
	if (event == DOUBLECLICK) {
		gotoAndGet(item);
	} else if (event == SCRIPTED) {
		//Player is carrying the thread and ready to use it
		struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();

		//loom was clicked on: go to the loom and run the weaving animation
		if (target_shape == SHAPE_LOOM) {
			UI_close_gumps();

			//only go to the bottom edge of the loom
			var pos_x = [0, -1, -2];
			var pos_y = [1, 1, 1];
			var pos_z = -1;
			gotoObject(target, pos_x, pos_y, pos_z, Loom, target, 7);
		} else if (target_shape == SHAPE_CLOTH) {
			//thread was used on cloth - get sewing!
			sewCloth(target);
		} else {
			//something else was clicked on; advise the player what
			//they can do with thread
			randomPartySay("@Why dost thou not weave cloth with that thread on the loom?~@Or if thou hast cloth already, thou couldst use this thread to sew a fine garment with it.@");
		}
	}
}
