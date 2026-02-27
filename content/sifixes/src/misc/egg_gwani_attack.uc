/*	Copyright (C) 2016  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*	This egg appears twice in Skullcrusher, and triggers the sleeping
 *	enthralled Gwani to attack. It can also set the Automaton near the
 *	Serpent Gate to also go into combat, which means the player would
 *	miss out on his warnings to keep away from the doors as they hide
 *	things that are "not of mortal understanding."
 *
 *	2016-08-08 Submitted by Knight Captain
 */

void gwaniAttack object#(0x797) () {
	if (event == EGG) {
		if (getAvatarLocationID() == SKULLCRUSHER_MOUNTAINS) {
			// The egg still hatches, making every nearby non-party creature EVIL and attack.
			gwaniAttack.original();
			// This resets the Serpent Gate Guard so he remains at his post.
			var npc = GUARD20;
			if (!npc->get_item_flag(SI_ZOMBIE) && !npc->get_item_flag(DEAD)) {
				npc->set_schedule_type(STANDTHERE);
				npc->set_alignment(NEUTRAL);
			}
		} else {
			gwaniAttack.original();
		}
	}
}
