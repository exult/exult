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

//Corrected bug that treats noon (12:00-12:59) as 'am'
void Pocketwatch shape#(0x9F) () {
	if (event != DOUBLECLICK) {
		return;
	}

	var hour = UI_game_hour();
	var period;
	if (hour < 12) {
		period = "am";
	} else {
		period = "pm";
	}

	hour %= 12;
	if (hour == 0) {
		hour = 12;
	}

	var minute = UI_game_minute();
	if (minute < 10) {
		minute = "0" + minute;
	}

	var msg = " " + hour + ":" + minute + period;
	item_say(msg);
}
