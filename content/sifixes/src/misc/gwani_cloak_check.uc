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

/*	The Gwani were intended to be able to smell any Gwani pelts or cloaks
 *	carried by the party, but the original usecode looks for wrong shapes
 *	and frames. The code was rewritten by Malignant Manor.
 *
 *	As a new function other Gwani can detect the gear, if desired later.
 *
 *	2016-07-05 Written by Malignant Manor
 *	2016-08-05 Moved to its own function by Knight Captain
 */

var gwaniCloakCheck 0x9C2 (var have_gwani_gear) {
	// Newly assigned number, unused in original Usecode.
	var gwani_pelt = hasItemCount(PARTY, 1, SHAPE_FUR_PELT, QUALITY_ANY, 9); // was using frame 8
	var gwani_cloak = hasItemCount(PARTY, 1, SHAPE_CLOAK, QUALITY_ANY, 4); // was using shape 2
	return (gwani_pelt || gwani_cloak);
}

