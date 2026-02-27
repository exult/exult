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
 *	This source file contains the code for the Eternal Flames of the Principles,
 *	as well as for the three items of principle.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void Eternal_Flame shape#(0x45B) () {
	var flame_id = get_item_quality() + 1;
	var flame_names = ["Truth", "Love", "Courage"];
	var shard_names = ["Falsehood", "Hatred", "Cowardice"];
	var shadowlords = ["Faulinei", "Astaroth", "Nosfentor"];

	if (event == DOUBLECLICK) {
		var npc = randomPartyMember();
		message("@This is the Eternal Flame of ");
		message(flame_names[flame_id]);
		message(". Many years ago, ");
		if (npc == AVATAR) {
			message("I");
		} else {
			message("the Avatar");
		}
		message(" used it to defeat ");
		message(shadowlords[flame_id]);
		message(", the Shadowlord of ");
		message(shard_names[flame_id]);
		message(".~@This flame is a prison for the Shard of ");
		message(shard_names[flame_id]);
		message(" -- hopefully forever.@");
		npc.say();
	} else if (event == EGG) {
		randomPartyBark("@The Flame of " + flame_names[flame_id] + "@");
	}
}

void Items_of_Principle shape#(0x464) () {
	var frnum = get_item_frame() + 1;
	var iolo_here = isNearby(IOLO);
	var frame_names = ["Book of Truth", "Candle of Love", "Bell of Courage"];
	var iolos_descriptions = [
		"@This is the Book of Truth, an ancient artifact of great power and unknown origins -- no one knows whence it came from, or how it came to be.~@It symbolizes Truth, and has been used by mages throughout the ages to cast powerful spells.~@Thou hast also used it to open the entrance to the Great Stygian Abyss many years ago.@",
		"@Ah, the Candle of Love. It hails back from the Age of Enlightenment, and symbolizes Love.~@Thou hast used it in the past to open the entrance of the Great Stygian Abyss, if thou dost remember;~@although back then, the candle was kept at a temple in Cove. It is said that only a compassionate individual may light the candle.@",
		"@The Bell of Courage! This artifact embodies the principle of Courage.~@Thou hast used it to open the entrance to the Great Stygian Abyss during the Age of Enlightenment.~@Afterwards, it was entrusted to the knights of the Order of the Silver Serpent. Some say that its sound brings great comfort to the brave.@"
	];

	if (iolo_here) {
		//Iolo the BARD gives better descriptions than anyone else:
		IOLO.say(iolos_descriptions[frnum]);
	} else {
		randomPartySay("@This is the " + frame_names[frnum] + ".@");
	}
}
