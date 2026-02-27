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
 *	This source file contains usecode for several items that allow remote
 *	viewing. This is for multimap support, basically.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

// externs
extern void startSpeech 0x940 (var var0000);

void Crystal_Ball shape#(0x2D9) () {
	if (event == DOUBLECLICK) {
		UI_close_gumps();
		var qual = get_item_quality();
		var pos;
		if ((qual == 0) || (qual > 7)) {
			startSpeech(0x19);
			return;
		} else if (qual == 1) {
			if (gflags[0x157]) {
				pos = [0xA82, 0x970, 0x3, 0x0];
			} else {
				pos = [0xA82, 0x9A0, 0x3, 0x0];
			}
		} else if (qual == 2) {
			pos = [0x38F, 0x47D, 0x3, 0x0];
		} else if (qual == 3) {
			pos = [0xA28, 0xA25, 0x3, 0x0];
		} else if (qual == 4) {
			pos = [0xAF7, 0xD7, 0x3, 0x0];
		} else if (qual == 5) {
			pos = [0x9F, 0x540, 0x3, 0x0];
		} else if (qual == 6) {
			pos = [0x609, 0x773, 0x3, 0x0];
		} else if (qual == 7) {
			pos = [0x928, 0x597, 0x3, 0x0];
		} else if (qual == 255) {
			return;
		}
		UI_display_area(pos);
	}
}

void Gem shape#(0x2F8) () {
	var framenum = get_item_frame();
	if (event == DOUBLECLICK) {
		if (framenum == 14) {
			if (!inIsleOfFire()) {
				UI_display_area([0x88F, 0x610, 0x6, 0x0]);
			} else {
				remove_item();
				UI_play_sound_effect(SOUND_GLASS_SHATTER);
			}
		} else {
			Gem.original();
		}
	} else {
		Gem.original();
	}
}

extern void setOrreryPosition object#(0x6E1) ();

void Orrery_Viewer shape#(0x302) () {
	if ((event == DOUBLECLICK) && inMagicStorm()) {
		UI_close_gumps();
		UI_center_view([0xB4C, 0x58C]);
		event = EGG;
		0->setOrreryPosition();
		UI_display_area([0xB4C, 0x58C, 0x3, 0x0]);
	}
}
