/*  Copyright (C) 2016  The Exult Team
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

/*	Draxinar's Cloth Riddle's answer should be 13, not 14 as in the release.
 *	Fortunately each riddle is its own function, so it can be fixed without
 *  having to re-code Draxinar completely.
 *
 *	Draxinar's main conversation and second riddle also have spelling mistakes.
 *
 *	2016-07-11 Written by Knight Captain
 */

void DraxinarClothRiddle 0x824 () {
	say("@A clothier named Sedrick has received an order to make five dresses, all with the same pattern. Sedrick has three designs of embroidered cloth laying on a table in his workroom.@");
	say("@Sedrick tells his apprentice to fetch five pieces of cloth with the same embroidered pattern. The lazy apprentice doesn't bother looking through the piles of cloth, he just takes an armful.@");
	say("@Discounting pure luck, how many pieces of cloth would the apprentice have to grab to insure that Sedrick could make five dresses with the same embroidery pattern?@");
	var answer = UI_input_numeric_value(0, 30, 1, 0);
	if (answer == 13) {
		// Correct answer
		say("@Verily, thou hath a good head upon thy shoulders!@");
		gflags[SOLVED_CLOTH_RIDDLE] = true;
	} else if (answer == 14) {
		// The original 'correct' incorrect answer.
		say("@'Tis not it, though I thought it was for two decades!@");
	} else {
		say("@I'm sorry, but that is not the correct answer.@");
	}
}
