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
#include "ignore_unused_variable_warning.h"
#include "misc_buttons.h"

using std::cout;
using std::endl;
#ifdef USE_OLD_SLIDER_GUMP
/*
 *  Statics:
 */

short Slider_gump::leftbtnx  = 31;
short Slider_gump::rightbtnx = 103;
short Slider_gump::btny      = 14;
short Slider_gump::diamondy  = 6;
short Slider_gump::xmin      = 36;    // Min., max. positions of diamond.
short Slider_gump::xmax      = 91;

/*
 *  One of the two arrow button on the slider:
 */
class Slider_button : public Gump_button {
	bool is_left;

public:
	Slider_button(Gump* par, int px, int py, int shapenum, bool left)
			: Gump_button(par, shapenum, px, py), is_left(left) {}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Handle click on a slider's arrow.
 */

bool Slider_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	if (is_left) {
		static_cast<Slider_gump*>(parent)->clicked_left_arrow();
	} else {
		static_cast<Slider_gump*>(parent)->clicked_right_arrow();
	}
	return true;
}

/*
 *  Set slider value.
 */

void Slider_gump::set_val(int newval) {
	val                    = newval;
	static const int xdist = xmax - xmin;
	if (max_val - min_val == 0) {
		val      = 0;
		diamondx = xmin;
	} else {
		diamondx = xmin + ((val - min_val) * xdist) / (max_val - min_val);
	}
}

/*
 *  Create a slider.
 */

Slider_gump::Slider_gump(
		int mival, int mxval,    // Value range.
		int step,                // Amt. to change by.
		int defval               // Default value.
		)
		: Modal_gump(nullptr, game->get_shape("gumps/slider")), min_val(mival),
		  max_val(mxval), step_val(step), val(defval), dragging(0),
		  prev_dragx(0) {
	diamond = ShapeID(game->get_shape("gumps/slider_diamond"), 0, SF_GUMPS_VGA);
	set_object_area(TileRect(0, 0, 0, 0), 6, 30);

#	ifdef DEBUG
	cout << "Slider:  " << min_val << " to " << max_val << " by " << step
		 << endl;
#	endif
	add_elem(new Slider_button(
			this, leftbtnx, btny, game->get_shape("gumps/slider_left"), true));
	add_elem(new Slider_button(
			this, rightbtnx, btny, game->get_shape("gumps/slider_right"),
			false));
	// Init. to middle value.
	if (defval < min_val) {
		defval = min_val;
	} else if (defval > max_val) {
		defval = max_val;
	}
	set_val(defval);
}

/*
 *  An arrow on the slider was clicked.
 */

void Slider_gump::clicked_left_arrow() {
	move_diamond(-step_val);
}

void Slider_gump::clicked_right_arrow() {
	move_diamond(step_val);
}

void Slider_gump::move_diamond(int dir) {
	int newval = val;
	newval += dir;
	if (newval < min_val) {
		newval = min_val;
	}
	if (newval > max_val) {
		newval = max_val;
	}

	set_val(newval);
	paint();
	gwin->set_painted();
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
	// Paint slider diamond.
	diamond.paint_shape(x + diamondx, y + diamondy);
	// Print value.
	gumpman->paint_num(val, x + textx, y + texty);
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

	dragging         = 0;
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
	// See if on diamond.
	Shape_frame* d_shape = diamond.get_shape();
	if (d_shape->has_point(mx - (x + diamondx), my - (y + diamondy))) {
		// Start to drag it.
		dragging   = 1;
		prev_dragx = mx;
	} else {
		if (my - get_y() < diamondy
			|| my - get_y() > diamondy + d_shape->get_height()) {
			return Modal_gump::mouse_down(mx, my, button);
		}
		if (mx - get_x() < xmin || mx - get_x() > xmax) {
			return Modal_gump::mouse_down(mx, my, button);
		}
		diamondx               = mx - get_x();
		static const int xdist = xmax - xmin;
		int delta = (diamondx - xmin) * (max_val - min_val) / xdist;
		// Round down to nearest step.
		delta -= delta % step_val;
		const int newval = min_val + delta;
		if (newval != val) {    // Set value.
			val = newval;
		}
		paint();
	}

	return true;
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

	if (dragging) {      // Done dragging?
		set_val(val);    // Set diamond in correct pos.
		paint();
		gwin->set_painted();
		dragging = 0;
	}
	if (!pushed) {
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
	ignore_unused_variable_warning(mx, my);
	if (!dragging) {
		return Modal_gump::mouse_drag(mx, my);
	}
	diamondx += mx - prev_dragx;
	prev_dragx = mx;
	if (diamondx < xmin) {    // Stop at ends.
		diamondx = xmin;
	} else if (diamondx > xmax) {
		diamondx = xmax;
	}
	static const int xdist = xmax - xmin;
	int              delta = (diamondx - xmin) * (max_val - min_val) / xdist;
	// Round down to nearest step.
	delta -= delta % step_val;
	const int newval = min_val + delta;
	if (newval != val) {    // Set value.
		val = newval;
	}
	paint();
	return true;
}

/*
 *  Handle ASCII character typed.
 */

bool Slider_gump::key_down(int chr) {
	switch (chr) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		done = true;
		break;
	case SDLK_LEFT:
		clicked_left_arrow();
		break;
	case SDLK_RIGHT:
		clicked_right_arrow();
		break;
	}
	return true;
}

bool Slider_gump::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		move_diamond(-10 * step_val);
	} else {
		move_diamond(-step_val);
	}
	return true;
}

bool Slider_gump::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		move_diamond(10 * step_val);
	} else {
		move_diamond(step_val);
	}
	return true;
}

void Slider_gump::OnSliderValueChanged(Slider_widget* sender, int newvalue) {}
#else

Slider_gump::Slider_gump(
		int mival, int mxval,    // Value range.
		int step,                // Amt. to change by.
		int defval               // Default value.
		)
		: Modal_gump(nullptr, game->get_shape("gumps/slider")) {
	widget = std::make_unique<Slider_widget>(
			this, 24, 6,
			ShapeID(game->get_shape("gumps/slider_left"), 0, SF_GUMPS_VGA),
			ShapeID(game->get_shape("gumps/slider_right"), 0, SF_GUMPS_VGA),
			ShapeID(game->get_shape("gumps/slider_diamond"), 0, SF_GUMPS_VGA),
			mival, mxval, step, defval, 64);

	set_object_area(TileRect(0, 0, 0, 0), 6, 30);
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

bool Slider_gump::key_down(int chr) {
	switch (chr) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		done = true;
		return true;
	default:
		if (widget->key_down(chr)) {
			return true;
		}
	}
	return Modal_gump::key_down(chr);
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
#endif
