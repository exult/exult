/*
Copyright (C) 2001-2024 The Exult Team

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

#include "Gump_ToggleButton.h"

#include "gamewin.h"

bool Gump_ToggleButton::activate(MouseButton button) {
	int delta;
	if (button == MouseButton::Left) {
		delta = 2;
	} else if (button == MouseButton::Right) {
		delta = 2 * numselections - 2;
	} else {
		return false;
	}

	set_frame((get_framenum() + delta) % (2 * numselections));
	toggle(get_framenum() / 2);
	gwin->add_dirty(get_rect());
	return true;
}

bool Gump_ToggleButton::push(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		set_pushed(button);
		gwin->add_dirty(get_rect());
		return true;
	}
	return false;
}

void Gump_ToggleButton::unpush(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		set_pushed(false);
		gwin->add_dirty(get_rect());
	}
}

bool Gump_ToggleTextButton::activate(MouseButton button) {
	int       delta;
	const int numselections = selections.size();
	if (button == MouseButton::Left) {
		delta = 1;
	} else if (button == MouseButton::Right) {
		delta = numselections - 1;
	} else {
		return false;
	}

	set_frame((get_framenum() + delta) % numselections);
	text = selections[get_framenum()];
	init();
	toggle(get_framenum());
	gwin->add_dirty(get_rect());
	return true;
}

void Gump_ToggleTextButton::setselection(int selectionnum) {
	set_frame(selectionnum);
	gwin->add_dirty(get_rect());
}

bool Gump_ToggleTextButton::push(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		set_pushed(button);
		gwin->add_dirty(get_rect());
		return true;
	}
	return false;
}

void Gump_ToggleTextButton::unpush(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		set_pushed(false);
		gwin->add_dirty(get_rect());
	}
}
