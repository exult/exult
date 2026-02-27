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

void Frigidazzi object#(0x414) () {
	// When you give the fur cap back to Frigidazzi, she clears a flag
	// she "shouldn't". We cache the flag value and use a try...catch
	// block to get around a few hidden aborts in her usecode.
	var knew_furcap_owner = gflags[KNOWS_FURCAP_OWNER];
	try {
		Frigidazzi.original();
	}
	catch () {
	}

	if (knew_furcap_owner && !gflags[KNOWS_FURCAP_OWNER]) {
		gflags[GAVE_FURCAP_BACK] = true;
	}
}

