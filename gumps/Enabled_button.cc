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

#include "Enabled_button.h"

#include "gamewin.h"
#include "items.h"

/*
 *  Redisplay as 'pushed'.
 */

Enabled_button::Enabled_button(
		Gump* par, int selectionnum, int px, int py, int width, int height)
		: Text_button(par, "", px, py, width, height),
		  selections{GumpStrings::Disabled(), GumpStrings::Enabled()} {
	set_frame(selectionnum);
	text = selections[selectionnum];
	init();
}

bool Enabled_button::push(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Middle) {
		set_pushed(button);
		paint();
		gwin->set_painted();
		return true;
	}
	return false;
}

/*
 *  Redisplay as 'unpushed'.
 */

void Enabled_button::unpush(MouseButton button) {
	if (button == MouseButton::Left || button == MouseButton::Right) {
		set_pushed(false);
		paint();
		gwin->set_painted();
	}
}

bool Enabled_button::activate(MouseButton button) {
	if (button != MouseButton::Left && button != MouseButton::Right) {
		return false;
	}

	set_frame(get_framenum() + 1);
	if (get_framenum() >= 2) {
		set_frame(0);
	}
	text = selections[get_framenum()];
	init();
	toggle(get_framenum());
	paint();
	gwin->set_painted();

	return true;
}
