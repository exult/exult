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

/* Miscellaneous item interaction scripts */

//Sword in the stone, in Carrocio's lot. Now, you too can try!
void SwordInAStone shape#(0x315) () {
	var barks;

	//Player doubleclicked on the stone - march them to it so we can start animating
	if (event == DOUBLECLICK) {
		gotoObject(item, [-3, 1, 0], [-1, 1, -1], 0, SwordInAStone, item, SCRIPTED);
	} else if (event == SCRIPTED) {
		//Player arrived at the stone, animate them trying to get it out
		script AVATAR {
			nohalt;
			//keep the avatar still, so they don't try to wander off
			call freeze;
			face directionFromAvatar(item);
			actor frame standing;

			//Get psyched
			actor frame bowing;
			wait 7;
			actor frame standing;

			//Start pulling (loop the animations so they don't time out)
			repeat 1 {
				actor frame strike_2h;
				wait 4;
			};
			say "@Nnnngggh...@";
			repeat 1 {
				actor frame raise_2h;
				wait 7;
			};
			actor frame strike_2h;
			wait 5;

			//Oops, looks like you overdid it a bit
			actor frame standing;
			actor frame bowing;
			wait 2;

			//Not as young as we used to be, eh?
			say "@Argh, my back...@";
			repeat 3 {
				actor frame bowing;
				wait 5;
			};

			actor frame bowing;
			wait 2;
			actor frame standing;

			//Let the player move around again now
			call unfreeze;
		}

		//Snide comments from the mules
		barks = ["-sigh-", "@Lord British is not out of a job yet!@", "@Stick to being the Avatar.@"];
		delayedBark(randomPartyMember(), randomIndex(barks), 75);
	}
}

/*
I'm not really sure why I had a crack at reimplementing this. I seem to remember wanting to have Carrocio march over and animate the thing rather than do it by telekinesis like he does now.
void PuppetShow 503 () {
	var took_gold;
	var var0001;
	var carrocio = UI_get_npc_object(CARROCIO);

	if (event != DOUBLECLICK) return;
	if (UI_in_usecode(item) || UI_in_usecode(carrocio)) {
		//puppet show is currently going
		return;
	}
	if(!UI_npc_nearby(carrocio)) {
		//Carrocio is nowhere in sight
		return;
	}

	carrocio->say("@Now is the time for the young and the old to dig in their pockets and give up the gold. * Dost thou wish to donate a gold piece?@");
	if (askYesNo()) {
		//Remove a gold piece
		took_gold = UI_remove_party_items(1, SHAPE_GOLD, QUALITY_ANY, FRAME_ANY, 0xFE99);
		if (took_gold) {
			UI_execute_usecode_array(item, [0x23, 0x46, 0x0000, 0x4E, 0x0B, 0xFFFF, 0x001F, 0x46, 0x0000]);
			UI_execute_usecode_array(item, [0x23, 0x27, 0x000B, 0x58, 0x0055, 0x58, 0x0053, 0x58, 0x0054, 0x27, 0x0001, 0x58, 0x0055, 0x58, 0x0053, 0x27, 0x0003, 0x58, 0x0053, 0x27, 0x0002, 0x58, 0x0055]);
		} else {
			carrocio->say("@Thy pocket must fill 'fore the puppets shall thrill! * @Return with thine gold, and thou shalt splendour behold.@");
		}
	}
	carrocio->hide();
}

//called when Carrocio reaches his destination behind the puppetstand
void runPuppets() {
	UI_execute_usecode_array(item, [0x23, 0x46, 0x0000, 0x4E, 0x0B, 0xFFFF, 0x001F, 0x46, 0x0000]);
	UI_execute_usecode_array(item, [0x23, 0x27, 0x000B, 0x58, 0x0055, 0x58, 0x0053, 0x58, 0x0054, 0x27, 0x0001, 0x58, 0x0055, 0x58, 0x0053, 0x27, 0x0003, 0x58, 0x0053, 0x27, 0x0002, 0x58, 0x0055]);
}
*/

//I couldn't resist.
void TripleCrossbow shape#(0x287) () {
	if (event == DOUBLECLICK) {
		if (inParty(IOLO)) {
			IOLO.say("@I know what thou thinkest. Thou thinkest: 'Hath he fired 60 bolts, or only 57?'",
				"~@Well, to tell thee truthfully, in all this excitement I rather lost track myself.");
			IOLO.say("@But being as this is a triple crossbow, the most powerful ranged weapon in Britannia, and would blow thine head clean off, thou must ask thyself a question: 'Do I feel lucky?'");
			IOLO.say("@Well dost thou, punk?@*");
			IOLO.hide();

			delayedBark(IOLO, "@I always wanted to say that.@", 6);
		}
	}
}

