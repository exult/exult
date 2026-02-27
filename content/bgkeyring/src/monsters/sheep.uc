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

//More highly sophisticated behaviour, for sheep this time!
//Also an accusation of sheep-fucking, from our good friend Dupre.
void Sheep shape#(0x3CA) () {
	item_say("@Baa-aa-aa@");
	if (event == DOUBLECLICK) {
		if (UI_get_random(10) < 3 && inParty(SHAMINO) && canTalk(SHAMINO)) {
			delayedBark(SHAMINO, "@I like sheep.@", 6);
			if (inParty(DUPRE) && canTalk(DUPRE)) {
				delayedBark(DUPRE, "@Aye, I hath heard that about thee!@", 16);
				delayedBark(SHAMINO, "@Hey!@", 26);
			}
		}
	}
}
