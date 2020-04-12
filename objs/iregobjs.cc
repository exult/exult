/**
 ** Iregobjs.cc - Ireg (moveable) game objects.
 **
 ** Written: 10/1/98 - JSF
 **/

/*
Copyright (C) 1998  Jeffrey S. Freedman

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
#  include <config.h>
#endif

#include "contain.h"
#include "gamewin.h"
#include "gamemap.h"
#include "chunks.h"
#include "cheat.h"
#include "databuf.h"
#include "ucsched.h"
#include "Gump_manager.h"
#include "effects.h"

using std::ostream;

/*
 *  Get chunk coords, or 255.
 */
inline int Game_object::get_cxi() const {
	return chunk ? chunk->cx : 255;
}
inline int Game_object::get_cyi() const {
	return chunk ? chunk->cy : 255;
}

/*
 *  Paint at given spot in world.
 */

void Ireg_game_object::paint(
) {
	int x;
	int y;
	gwin->get_shape_location(this, x, y);
	if (flags & (1L << Obj_flags::invisible))
		paint_invisible(x, y);
	else
		paint_shape(x, y);
}

/*
 *  Move to a new absolute location.  This should work even if the old
 *  location is invalid (cx=cy=255).
 */

void Ireg_game_object::move(
    int newtx,
    int newty,
    int newlift,
    int newmap
) {
	if (owner) {        // Watch for this.
		owner->remove(this);
		set_invalid();      // So we can safely move it back.
	}
	Game_object::move(newtx, newty, newlift, newmap);
}

/*
 *  Remove an object from its container, or from the world.
 *  The object is deleted.
 */

void Ireg_game_object::remove_this(
    Game_object_shared *keep     // Non-null to not delete.
) {
	// Do this before all else.
	if (!keep) {
		cheat.clear_this_grabbed_actor(this->as_actor());   // Could be an actor
	} else
	    *keep = shared_from_this();
	if (owner)          // In a bag, box, or person.
		owner->remove(this);
	else {              // In the outside world.
		if (chunk)
			chunk->remove(this);
	}
}

/*
 *  Can this be dragged?
 */

bool Ireg_game_object::is_dragable(
) const {
	// 0 weight means 'too heavy'.
	return get_info().get_weight() > 0;
}

/*
 *  Write the common IREG data for an entry.
 *  Note:  Length is incremented if this is an extended entry (shape# >
 *      1023).
 *  Output: ->past data written.
 */

unsigned char *Ireg_game_object::write_common_ireg(
    int norm_len,           // Normal length (if not extended).
    unsigned char *buf      // Buffer to be filled.
) {
	unsigned char *endptr;
	int shapenum = get_shapenum();
	int framenum = get_framenum();
	if (shapenum >= 1024 || framenum >= 64) {
		*buf++ = IREG_EXTENDED;
		norm_len++;
		buf[3] = shapenum & 0xff;
		buf[4] = shapenum >> 8;
		buf[5] = framenum;
		endptr = buf + 6;
	} else {
		if (get_lift() > 15) {
			*buf++ = IREG_EXTENDED2;
		}
		buf[3] = shapenum & 0xff;
		buf[4] = ((shapenum >> 8) & 3) | (framenum << 2);
		endptr = buf + 5;
	}
	buf[0] = norm_len;
	if (owner) {
		// Coords within gump.
		buf[1] = get_tx();
		buf[2] = get_ty();
	} else {            // Coords on map.
		buf[1] = ((get_cxi() % 16) << 4) | get_tx();
		buf[2] = ((get_cyi() % 16) << 4) | get_ty();
	}
	return endptr;
}

/*
 *  Write out.
 */

void Ireg_game_object::write_ireg(
    ODataSource *out
) {
	unsigned char buf[20];      // 10-byte entry;
	uint8 *ptr = write_common_ireg(10, buf);
	*ptr++ = nibble_swap(get_lift());
	*ptr = get_quality();
	const Shape_info &info = get_info();
	if (info.has_quality_flags()) {
		// Store 'quality_flags'.
		*ptr = (get_flag(Obj_flags::invisible) ? 1 : 0) +
				(get_flag(Obj_flags::okay_to_take) ? (1 << 3) : 0);
	}
	// Special case for 'quantity' items:
	else if (get_flag(Obj_flags::okay_to_take) && info.has_quantity())
		*ptr |= 0x80;
	++ptr;
	*ptr++ = (get_flag(Obj_flags::is_temporary));
	*ptr++ = 0;         // Filler, I guess.
	*ptr++ = 0;
	*ptr++ = 0;
	out->write(reinterpret_cast<char *>(buf), ptr - buf);
	// Write scheduled usecode.
	Game_map::write_scheduled(out, this);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Ireg_game_object::get_ireg_size() {
	// These shouldn't ever happen, but you never know
	if (gumpman->find_gump(this) || Usecode_script::find(this))
		return -1;
	return 6 + get_common_ireg_size();
}
