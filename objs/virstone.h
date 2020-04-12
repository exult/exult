/**
 ** Virstone.h - Virtue stones.
 **
 ** Written: 10/27/2000 - JSF
 **/

/*
Copyright (C) 2000  Jeffrey S. Freedman

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

#ifndef INCL_VIRSTONE
#define INCL_VIRSTONE   1

#include "iregobjs.h"

/*
 *  A virtue stone can be set to a position on the map.
 */
class Virtue_stone_object : public Ireg_game_object {
	Tile_coord pos;         // Position it teleports to.
	int map;                // Map to teleport to.
public:
	Virtue_stone_object(int shapenum, int framenum, unsigned int tilex,
	                    unsigned int tiley, unsigned int lft)
		: Ireg_game_object(shapenum, framenum, tilex, tiley, lft),
		  pos(0, 0, 0), map(0)
	{  }
	void set_target_pos(Tile_coord const &t) {  // Set/get position.
		pos = t;
	}
	void set_target_pos(unsigned char tilex, unsigned char tiley,
	                    unsigned char schunk, unsigned char lift);
	Tile_coord get_target_pos() {
		return pos;
	}
	int get_target_map() {  // Get/set map.
		return map;
	}
	void set_target_map(int m) {
		map = m;
	}
	// Write out to IREG file.
	void write_ireg(ODataSource *out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;
};

#endif
