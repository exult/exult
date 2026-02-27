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
 */

void DeskItem shape#(0x2A3) () {
	if (!gflags[TIME_FORMAT_24_HOURS]) {
		DeskItem.original();
		abort;
	}

	if (event == DOUBLECLICK) {
		var framenum = UI_get_item_frame(item);
		if (framenum == 21) {

			var hour = UI_game_hour();
			if (hour <= 9) {
				hour = ("0" + hour);
			}

			var minute = UI_game_minute();
			if (minute <= 9) {
				minute = ("0" + minute);
			}

			var bark = hour + ":" + minute;
			if (UI_in_gump_mode()) {
				UI_item_say(item, bark);
			} else {
				bark = "@" + bark + "@";
				UI_item_say(AVATAR, bark);
			}
			return;
		}
	}
	DeskItem.original();
}

void Sundial shape#(0x11C) () {
	if (!gflags[TIME_FORMAT_24_HOURS]) {
		Sundial.original();
		abort;
	}

	if (event == DOUBLECLICK) {
		var hour = UI_game_hour();
		if (hour == 12) {
			item_say("Noon");
		} else if ((hour >= 6) && (hour <= 20)) {
			item_say(UI_game_hour() + " o'clock");
		} else {
			var bark = "@^<Avatar>, I believe the important part of the word sundial is `sun'.@";
			partyUtters(1, bark, bark, false);
		}
	}
}
