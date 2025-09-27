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

#include "gamewin.h"

/*
 *  Is a given screen point on this widget?
 */

bool Gump_widget::on_widget(
		int mx, int my    // Point in window.
) const {
	// if shapenum is invalid just use rect
	if (get_shapenum() < 0 || get_framenum() < 0) {
		return get_rect().has_point(mx, my);
	}
	int lx = mx, ly = my;
	screen_to_local(lx, ly);
	Shape_frame* cshape = get_shape();
	return (cshape != nullptr) ? cshape->has_point(lx, ly)
							   : get_rect().has_point(mx, my);
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

TileRect Gump_widget::get_rect() const {
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

void Gump_widget::set_pos(int newx, int newy) {
	// Set old rect dirty
	gwin->add_dirty(get_rect());
	x = newx;
	y = newy;
	// Set new rect dirty
	gwin->add_dirty(get_rect());
}

void Gump_widget::paintSorted(
		Sort_Order sort, Sort_Order& next, Sort_Order& highest) {
	if (sort_order > highest) {
		highest = sort_order;
	}
	if (sort_order > sort && (sort_order < next || sort_order < highest)) {
		next = sort_order;
	}
	if (sort_order == sort) {
		// Save clipping rect
		auto                       ibuf = gwin->get_win()->get_ibuf();
		Image_buffer::ClipRectSave clip(ibuf);
		// If top most clear clip and allow to overdraw everything
		if (sort == Sort_Order::ontop) {
			ibuf->clear_clip();
		}
		paint();
	}
}

Gump_widget* Gump_widget::findSorted(
		int sx, int sy, Sort_Order sort, Sort_Order& next, Sort_Order& highest,
		Gump_widget* before) {
	if (this == before) {
		return nullptr;
	}
	if (sort_order > highest) {
		highest = sort_order;
	}
	if (sort_order > sort && (sort_order < next || sort_order < highest)) {
		next = sort_order;
	}
	if (sort_order == sort && on_widget(sx, sy)
		&& sort_order != Sort_Order::hidden) {
		return this;
	}
	return nullptr;
}
