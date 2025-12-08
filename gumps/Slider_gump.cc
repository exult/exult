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

#include "Slider_gump.h"

#include "Gump_button.h"
#include "Gump_manager.h"
#include "game.h"
#include "gamewin.h"
#include "misc_buttons.h"

using std::cout;
using std::endl;

Slider_gump::Slider_gump(
		int mival, int mxval,    // Value range.
		int step,                // Amt. to change by.
		int defval,               // Default value.
		bool allow_escape
		)
		: Modal_gump(nullptr, game->get_shape("gumps/slider")), allow_escape(allow_escape) {
	widget = std::make_unique<Slider_widget>(
			this, 24, 6,
			ShapeID(game->get_shape("gumps/slider_left"), 0, SF_GUMPS_VGA),
			ShapeID(game->get_shape("gumps/slider_right"), 0, SF_GUMPS_VGA),
			ShapeID(game->get_shape("gumps/slider_diamond"), 0, SF_GUMPS_VGA),
			mival, mxval, step, defval, 64);

	set_object_area(TileRect(0, 0, 0, 0), 22, 18);
}

void Slider_gump::OnSliderValueChanged(Slider_widget* sender, int newvalue) {
	ignore_unused_variable_warning(sender, newvalue);
	gwin->add_dirty(get_rect());
}

/*
 *  Paint on screen.
 */
void Slider_gump::paint() {
	const int textx = 128;
	const int texty = 7;
	// Paint the gump itself.
	paint_shape(x, y);
	// Paint red "checkmark".
	paint_elems();
	widget->paint();
	// Print value.
	gumpman->paint_num(get_val(), x + textx, y + texty);
	gwin->set_painted();
}

/*
 *  Handle mouse-down events.
 */

bool Slider_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	Gump_button* btn = Gump::on_button(mx, my);
	if (btn) {
		pushed = btn;
	} else {
		pushed = nullptr;
	}
	if (pushed) {
		if (!pushed->push(button)) {
			pushed = nullptr;
		}
		return true;
	}
	if (widget->mouse_down(mx, my, button)) {
		return true;
	}
	return Modal_gump::mouse_down(mx, my, button);
}

/*
 *  Handle mouse-up events.
 */

bool Slider_gump::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	if (!pushed) {
		if (widget->mouse_up(mx, my, button)) {
			return true;
		}
		return Modal_gump::mouse_up(mx, my, button);
	}
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		pushed->activate(button);
	}
	pushed = nullptr;

	return true;
}

/*
 *  Mouse was dragged with left button down.
 */

bool Slider_gump::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	if (widget->mouse_drag(mx, my)) {
		return true;
	}
	return Modal_gump::mouse_drag(mx, my);
}

/*
 *  Handle ASCII character typed.
 */

bool Slider_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	switch (chr) {
	case SDLK_RETURN:
		done = true;
		return true;
	case SDLK_ESCAPE:
		if (allow_escape) {
			// Returning false here will cause Gump_manager to close the gump
			return false;
		}
		return true;
	default:
		if (widget->key_down(chr, unicode)) {
			return true;
		}
	}
	return Modal_gump::key_down(chr, unicode);
}

bool Slider_gump::mousewheel_up(int mx, int my) {
	if (widget->mousewheel_up(mx, my)) {
		return true;
	}

	return Modal_gump::mousewheel_up(mx, my);
}

bool Slider_gump::mousewheel_down(int mx, int my) {
	if (widget->mousewheel_down(mx, my)) {
		return true;
	}

	return Modal_gump::mousewheel_down(mx, my);
}
