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
 *  This file has been created from usecode found in the Exult CVS snapshot.
 *  I include it here only for convenience; I have edited it to fit the
 *  conventions used in the rest of the mod.
 *
 *	The original code was written by Jeff Freedman (aka "DrCode").
 */

void Cantra object#(0x440) () {
	// I prefer to check this flag instead:
	if (!get_item_flag(SI_ZOMBIE)) {
		Cantra.original();
	} else {
		// Changed from Jeff's code for a SI-like behavior:
		if (event == DOUBLECLICK) {
			AVATAR->item_say("Hello, Cantra.");
			CANTRA->makePartyFaceNPC();
			delayedBark(CANTRA, "@I am not Cantra!@", 2);
			CANTRA->set_schedule_type(TALK);
		} else if (event == STARTED_TALKING) {
			CANTRA->run_schedule();
			var msg = ["@I want thy flesh!@", "@I want thy blood!@", "@Blood! Blood everywhere!@", "@How hungry I am!@"];
			var rand = UI_get_random(UI_get_array_size(msg));
			item.say(msg[rand]);

			converse (["Hello, Cantra.", "bye"]) {
				case "bye":
					say("@Yes, thou mayest go away.@");
					break;

				case "Hello, Cantra." (remove):
					say("@Cantra?@");
					say("@Was that my name?@");
					add("Yes, thou art Cantra!");
					add("knight...");

				case "Yes, thou art Cantra!" (remove):
					say("@No. NO!  LIAR!@");
					break;

				case "knight..." (remove):
					say("@Knight?@");
					say("@Perhaps... or was that someone else...@");
			}
		}
	}
}
