/**
 ** Virstone.cc - Virtue stones.
 **
 ** Written: 10/27/2000 - JSF
 **/

/*
Copyright (C) 2000  Jeffrey S. Freedman
Copyright (C) 2000-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "virstone.h"

#include "Gump_manager.h"
#include "databuf.h"
#include "gamewin.h"
#include "ucsched.h"

#include <iostream>

using std::ostream;

/*
 *  Set position from IREG data.
 */

void Virtue_stone_object::set_target_pos(
		unsigned char tilex,     // Tx within superchunk.
		unsigned char tiley,     // Ty within superchunk,
		unsigned char schunk,    // Superchunk
		unsigned char lift) {
	pos.tx = (schunk % 12) * c_tiles_per_schunk + tilex;
	pos.ty = (schunk / 12) * c_tiles_per_schunk + tiley;
	pos.tz = lift;
}

/*
 *  Write out container and its members.
 */

void Virtue_stone_object::write_ireg(ODataSource* out) {
	unsigned char  buf[20];    // 12-byte entry.
	unsigned char* ptr = write_common_ireg(12, buf);
	// Write tilex, tiley.
	Write1(ptr, pos.tx % c_tiles_per_schunk);
	Write1(ptr, pos.ty % c_tiles_per_schunk);
	// Get superchunk index.
	const int sx = pos.tx / c_tiles_per_schunk;
	const int sy = pos.ty / c_tiles_per_schunk;
	Write1(ptr, sy * 12 + sx);                          // Write superchunk #.
	Write1(ptr, static_cast<unsigned char>(pos.tz));    // Finally, lift in entry[7].??Guess+++
	Write1(ptr, 0);                                     // Entry[8] unknown.
	Write1(ptr, nibble_swap(get_lift()));               // Stone's lift in entry[9].
	Write1(ptr, map);                                   // Entry[10].  Unknown; using to store map.
	Write1(ptr, 0);                                     // Entry[11].  Unknown.
	out->write(reinterpret_cast<char*>(buf), ptr - buf);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Virtue_stone_object::get_ireg_size() {
	// These shouldn't ever happen, but you never know
	if (gumpman->find_gump(this) || Usecode_script::find(this)) {
		return -1;
	}

	return 8 + get_common_ireg_size();
}
