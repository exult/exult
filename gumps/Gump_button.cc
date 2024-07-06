/*
Copyright (C) 2000-2024 The Exult Team

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

#include "Gump_button.h"

#include "Gump.h"
#include "gamewin.h"

/*
 *  Redisplay as 'pushed'.
 */

bool Gump_button::push(MouseButton button) {
	if (button == MouseButton::Left) {
		set_pushed(button);
		gwin->add_dirty(get_rect());
		return true;
	}
	return false;
}

/*
 *  Redisplay as 'unpushed'.
 */

void Gump_button::unpush(MouseButton button) {
	if (button == MouseButton::Left) {
		set_pushed(false);
		gwin->add_dirty(get_rect());
	}
}

/*
 *  Default method for double-click.
 */

bool Gump_button::activate(MouseButton button) {
	ignore_unused_variable_warning(button);
	return false;
}

void Gump_button::double_clicked(int x, int y) {
	ignore_unused_variable_warning(x, y);
}

/*
 *  Repaint checkmark, etc.
 */

void Gump_button::paint() {
	int sx = 0;
	int sy = 0;

	local_to_screen(sx, sy);

	const int prev_frame = get_framenum();
	set_frame(prev_frame + (is_pushed() ? 1 : 0));
	paint_shape(sx, sy);
	set_frame(prev_frame);
}

bool Gump_button::mouse_down(int mx, int my, MouseButton button) {
	if (!self_managed) {
		return false;
	}
	if (!on_widget(mx, my)) {
		return false;
	}

	// eat click if already pushed
	if (pushed_button == MouseButton::Unknown) {
		push(button);
	}
	return true;
}

bool Gump_button::mouse_up(int mx, int my, MouseButton button) {
	if (!self_managed) {
		return false;
	}

	if (pushed_button != button && dragging != button) {
		return button == MouseButton::Left;
	}
	// end dragging
	bool was_dragging = dragging != MouseButton::Unknown;
	dragging          = MouseButton::Unknown;

	unpush(button);
	if (on_widget(mx, my)) {
		return activate(button);
	}
	return was_dragging;
}

bool Gump_button::mouse_drag(int mx, int my) {
	if (!self_managed) {
		return false;
	}
	// not pushed and not dragging
	if (dragging == MouseButton::Unknown
		&& pushed_button == MouseButton::Unknown) {
		return false;
	}
	// Starting Drag save the dragging button
	if (dragging == MouseButton::Unknown) {
		dragging = pushed_button;
	}

	if (on_widget(mx, my)) {
		push(dragging);
	} else {
		unpush(dragging);
	}

	return true;
}
