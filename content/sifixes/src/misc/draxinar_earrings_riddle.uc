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

/*	This slightly modifies Draxinar's second riddle to fix two issues.
 *	One is to remove parentheses in his dialog, which is not common in Ultima.
 *	It also fixes this misspelling: "Thou art corrcet!"
 *
 *	2016-08-04 Written by Knight Captain
 */

void DraxinarEarringsRiddle 0x822 () {
	say("@A young noblewoman is getting ready for the Royal Ball. She has several types of earrings in her jewelry box, with two made of gold, three of silver, and four made of copper.@");
	say("@The earrings, however, are all scattered about in the jewelry box.@");
	say("@Without looking into the jewelry box, how many earrings would the noblewoman have to take	out of the jewelry box to insure a matched pair?@");
	var answer = UI_input_numeric_value(0, 10, 1, 0);
	if (answer == 4) {
		// Correct answer
		say("@Thou art correct!@");
		gflags[SOLVED_EARRINGS_RIDDLE] = true;
	} else {
		// Any other answer
		say("@I'm sorry, but that is not the correct answer.@");
	}
}
