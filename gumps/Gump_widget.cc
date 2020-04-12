/*
Copyright (C) 2000 The Exult Team

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

#include "gamewin.h"
#include "Gump.h"
#include "Gump_widget.h"

/*
 *  Is a given screen point on this widget?
 */

bool Gump_widget::on_widget(
    int mx, int my          // Point in window.
) const {
	mx -= parent->get_x() + x;  // Get point rel. to gump.
	my -= parent->get_y() + y;
	Shape_frame *cshape = get_shape();
	return (cshape != nullptr) ? cshape->has_point(mx, my) : false;
}

/*
 *  Repaint checkmark, etc.
 */

void Gump_widget::paint(
) {
	int px = 0;
	int py = 0;

	if (parent) {
		px = parent->get_x();
		py = parent->get_y();
	}

	paint_shape(x + px, y + py);
}

/*
 *  Get screen area used by a gump.
 */

Rectangle Gump_widget::get_rect() {
	int px = x;
	int py = y;

	if (parent) {
		px += parent->get_x();
		py += parent->get_y();
	}

	Shape_frame *s = get_shape();

	if (!s) return Rectangle(0, 0, 0, 0);

	return Rectangle(px - s->get_xleft(),   py - s->get_yabove(),
	                 s->get_width(), s->get_height());
}
