/*
Copyright (C) 2022-2024 The Exult Team

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

#include "Gump_widget.h"

#include "Gump.h"
#include "gamewin.h"

/*
 *  Is a given screen point on this widget?
 */

bool Gump_widget::on_widget(
		int mx, int my    // Point in window.
) const {
	screen_to_local(mx, my);
	Shape_frame* cshape = get_shape();
	return (cshape != nullptr) ? cshape->has_point(mx, my) : false;
}

/*
 *  Repaint checkmark, etc.
 */

void Gump_widget::paint() {
	int sx = 0;
	int sy = 0;

	local_to_screen(sx, sy);

	paint_shape(sx, sy);
}

/*
 *  Get screen area used by a gump.
 */

TileRect Gump_widget::get_rect() {
	int sx = 0;
	int sy = 0;

	local_to_screen(sx, sy);

	Shape_frame* s = get_shape();

	if (!s) {
		return TileRect(0, 0, 0, 0);
	}

	return TileRect(
			sx - s->get_xleft(), sy - s->get_yabove(), s->get_width(),
			s->get_height());
}
