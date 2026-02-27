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
 *	Original Author: Alun Bestor
 *	Modified By: Marzo Junior
 *	Last Modified: 2006-03-19
 */

/*
 *	This include file contains new (or reimplemented) common functions for NPCS
*/

// DISABLED; only used once, so there is no real need:
//// Checks whether the player is carrying the cubic prism (for truthtelling situations)
//var hasCube () {
//	return AVATAR->get_cont_items(SHAPE_PRISM, QUALITY_ANY, FRAME_CUBE);
//}

// Called when the player asks a fellowship member about 'philosophy'.
// Tweaked to add in some cleverness the second time the player asks
// someone, and also some extra behaviour when the player has the cube.
// A global flag is used for checking if the player has heard this
// bollocks already
void askAboutPhilosophy 0x91A () {
	var dialog_strings;
	if (get_npc_number() in [SASHA]) {
		//Dialog for gypsy Fellowship members; there is only one IIRC
		dialog_strings = [
			"The cube vibrates. @Our philosophy is a collection ov self-serving doctrines used to muddy the minds ov the veak and justify any immoral acts my superiors set their minds to.~I think it's vorked rather vell, don't you?@ He beams at you obliviously.",
			"The cube vibrates. @Hmmm...to be honest it all seems like a load ov tvaddle, now that I think about it. Perhaps I shan't bore you vith it after all!@",
			"@The Fellowship advances the philosophy ov 'sanguine cognition', a vay to apply a confident order ov thought to one's life, through vhat is called the Triad ov Inner Strength. The Triad is simply three basic principles that, vhen applied in unison, enable one to be more creative, satisfied, and successful in life. They are: Strive For Unity, Trust Thy Brother, and Vorthiness Precedes Revard. Strive For Unity basically means that people should cooperate and vork together. Trust Thy Brother implies that ve are all the same and that ve should not hate or fear each other. Vorthiness Precedes Revard suggests that ve must each strive to be vorthy ov that vhich ve vant out ov life.@",
			"@Oh! I just now noticed thy medallion! Thou dost already know all ov this! Thou art one ov us! Excuse me for going on and on about it!@",
			"@Dost thou vant to join?@",
			"@Then thou shouldst go immediately to see Batlin at the Fellowship Hall headquarters in Britain.@",
			"@Oh. Vell, perhaps thou canst become enlightened another time.@"
		];
	} else {
		/*	Are there any gargoyles in the Fellowship? I remember only Batlin's goon.
		*	If there are any, this would be an appropriate place to add their dialog.
		*/
		dialog_strings = [
			"The cube vibrates. @Our philosophy is a collection of self-serving doctrines used to muddy the minds of the weak and justify any immoral acts my superiors set their minds to.~I think it's worked rather well, don't you?@ He beams at you obliviously.",
			"The cube vibrates. @Hmmm...to be honest it all seems like a load of twaddle, now that I think about it. Perhaps I shan't bore you with it after all!@",
			"@The Fellowship advances the philosophy of 'sanguine cognition', a way to apply a confident order of thought to one's life, through what is called the Triad of Inner Strength. The Triad is simply three basic principles that, when applied in unison, enable one to be more creative, satisfied, and successful in life. They are: Strive For Unity, Trust Thy Brother, and Worthiness Precedes Reward. Strive For Unity basically means that people should cooperate and work together. Trust Thy Brother implies that we are all the same and that we should not hate or fear each other. Worthiness Precedes Reward suggests that we must each strive to be worthy of that which we want out of life.@",
			"@Oh! I just now noticed thy medallion! Thou dost already know all of this! Thou art one of us! Excuse me for going on and on about it!@",
			"@Dost thou want to join?@",
			"@Then thou shouldst go immediately to see Batlin at the Fellowship Hall headquarters in Britain.@",
			"@Oh. Well, perhaps thou canst become enlightened another time.@"
		];
	}
	if (AVATAR->get_cont_items(SHAPE_PRISM, QUALITY_ANY, FRAME_CUBE)) {
		//a lot of these NPCs don't even use this function; so far the only ones that do are Klog and Rankin. That's why we can get away with using 'he'.
		var knowing_npcs = [KLOG, ELYNOR, SINTAG, GORDY, RANKIN, DANAG];

		if (UI_get_npc_number(item) in knowing_npcs) {
			item.say(dialog_strings[1]);
		} else {
			item.say(dialog_strings[2]);
		}
	} else {
		if (!gflags[HEARD_PHILOSOPHY]) {
			item.say(dialog_strings[3]);
			gflags[HEARD_PHILOSOPHY] = true;
		} else {
			item.hide();
			AVATAR.say("You immediately regret asking, as they embark on the same creepily well-rehearsed speech you've heard before. Your eyes glaze over and your thoughts wander, until with relief you hear them finish.");
			AVATAR.hide();
		}

		//finally, they notice the medallion
		if (UI_wearing_fellowship()) {
			item.say(dialog_strings[4]);
		} else {
			item.say(dialog_strings[5]);

			if (askYesNo()) {
				say(dialog_strings[6]);
			} else {
				say(dialog_strings[7]);
			}
		}
	}
}
