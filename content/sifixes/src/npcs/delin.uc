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

/*  There is a very minor flag, only checked by Delin, that allows you to
 *  ask him about Batlin. It looks like it should have been set by Jendon
 *  per the flag order but that bit was removed sometime during SI's dev.
 *  Since setting flag 344 happens after asking Jendon about Batlin it is
 *  a way to check the Avatar has already knows to ask Delin about Batlin.
 *
 *  2016-07-11 Written by Knight Captain
 */

void Delin object#(0x42F) () {
	if (event == STARTED_TALKING && gflags[ASKED_JENDON_DAEMON_ARTIFACTS]) {
		gflags[ASK_DELIN_ABOUT_BATLIN] = true;
	}
	Delin.original();
}
