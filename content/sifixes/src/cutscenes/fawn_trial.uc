/*
 *
 *  Copyright (C) 2013  The Exult Team
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

extern void FawnTrialTrigger object#(0x7F8) ();
extern var getTrialNPC 0x8AC (var getid);
extern void partySetTrialFacing 0x8C5 ();

void FawnTrialBarks 0x939 (var quality) {
	if (quality == 0x00F8) {
		var trialnpc = getTrialNPC(true);
		UI_move_object(trialnpc, [0x407, 0x679]);
		UI_si_path_run_usecode(trialnpc, [0x407, 0x66B], SI_PATH_SUCCESS, UI_get_npc_object(trialnpc), FawnTrialTrigger, false);
		partySetTrialFacing();
		delayedBark(trialnpc, "@" + getPoliteTitle() + "...@", 10);
		delayedBark(AVATAR, "@" + getTrialNPC(false) + "!@", 5);
		if (YELINDA->get_item_flag(ASLEEP)) {
			YELINDA->clear_item_flag(ASLEEP);
		}
		abort;
	} else if (quality == 0x00FD) {
		script item {
			nohalt;
			wait 2;
			face south;
			continue;
			actor frame standing;
		}
		if (gflags[FAWN_TRIAL_DONE_FIRST_DAY]) {
			script KYLISTA {
				nohalt;
				actor frame bowing;
				wait 3;
				say "@Hear ye!@";
				actor frame standing;
				wait 3;
				actor frame cast_up;
				sfx 30;
				wait 3;
				actor frame cast_out;
				sfx 30;
				wait 3;
				actor frame standing;
				sfx 30;
				wait 3;
				call FawnTrialTrigger;
			}
		} else {
			delayedBark(YELINDA, "@Zulith...@", 0x000F);
			script YELINDA after 10 ticks {
				nohalt;
				actor frame bowing;
				wait 3;
				actor frame standing;
				wait 2;
				face south;
				wait 2;
				call FawnTrialTrigger;
			}
		}
		abort;
	} else {
		FawnTrialBarks.original(quality);
	}
}

void Bolt shape#(0x2d3) () {
	if (event == DOUBLECLICK) {
		var ii = 0;
		while (ii < 30000) {
			event = SCRIPTED;
			item->Bolt();
			ii += 1;
		}
	}
}

