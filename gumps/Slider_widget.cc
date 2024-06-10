#include "Slider_widget.h"

#include "Gump_button.h"
#include "Gump_manager.h"
#include "gamewin.h"

#include <SDL.h>

using std::cout;
using std::endl;

/*
 *  One of the two arrow button on the slider:
 */
class SliderWidget_button : public Gump_button {
	bool is_left;

public:
	SliderWidget_button(
			Slider_widget* par, int px, int py, ShapeID shape, bool left)
			: Gump_button(
					  par, shape.get_shapenum(), px, py, shape.get_shapefile()),
			  is_left(left) {}

	// What to do when 'clicked':
	bool activate(MouseButton button) override {
		if (button != MouseButton::Left) {
			return false;
		}
		if (is_left) {
			static_cast<Slider_widget*>(parent)->clicked_left_arrow();
		} else {
			static_cast<Slider_widget*>(parent)->clicked_right_arrow();
		}
		return true;
	}
};

Slider_widget::Slider_widget(
		Gump_Base* par, int px, int py, ShapeID sidLeft, ShapeID sidRight,
		ShapeID sidDiamond, int mival, int mxval, int step, int defval,
		int width)
		: Gump_widget(par, -1, px, py, -1), min_val(mival), max_val(mxval),
		  step_val(step), val(defval), prev_dragx(INT32_MIN) {
	auto lshape = sidLeft.get_shape();
	auto rshape = sidRight.get_shape();
	auto dshape = sidDiamond.get_shape();
	leftbtnx    = lshape->get_xleft();    //- 1;
	xmin        = leftbtnx + lshape->get_xright() + dshape->get_xleft() + 1;
	xmax        = xmin + width - dshape->get_width();
	xdist       = xmax - xmin;
	// rightbtnx   = xmax + dshape->get_xright();//+rshape->get_xleft();
	rightbtnx = leftbtnx + lshape->get_xright() + width +rshape->get_xleft()+1;
	btny        = lshape->get_height() - 1;
	// centre the diamond between button height
	int buttontop = btny + lshape->get_yabove();
	int buttonbottom = btny + lshape->get_ybelow();
	diamondy         = buttonbottom-dshape->get_ybelow()-(lshape->get_height()-dshape->get_height())/2;
	//+dshape->get_ybelow();

	diamond     = sidDiamond;

	callback = dynamic_cast<ICallback*>(par);

#ifdef DEBUG
	cout << "SliderWidget:  " << min_val << " to " << max_val << " by " << step
		 << endl;
#endif
	left  = new SliderWidget_button(this, leftbtnx, btny, sidLeft, true);
	right = new SliderWidget_button(this, rightbtnx, btny, sidRight, false);
	// Init. to middle value.
	if (defval < min_val) {
		defval = min_val;
	} else if (defval > max_val) {
		defval = max_val;
	}
	set_val(defval);
}

/*
 *  Set slider value.
 */

void Slider_widget::set_val(int newval, bool recalcdiamond) {
	val             = newval;
	if (recalcdiamond) {
		if (max_val - min_val == 0) {
			val      = 0;
			diamondx = xmin;
		} else {
			diamondx = xmin + ((val - min_val) * xdist) / (max_val - min_val);
		}
	}
	if (callback) {
		callback->OnSliderValueChanged(this, val);
	}
}

/*
 *  An arrow on the slider was clicked.
 */

void Slider_widget::clicked_left_arrow() {
	move_diamond(-step_val);
}

void Slider_widget::clicked_right_arrow() {
	move_diamond(step_val);
}

void Slider_widget::move_diamond(int dir) {
	int newval = val;
	newval += dir;
	if (newval < min_val) {
		newval = min_val;
	}
	if (newval > max_val) {
		newval = max_val;
	}

	set_val(newval);
	gwin->add_dirty(get_rect());
}

TileRect Slider_widget::get_rect() {
	// Widget has no background shape so get rect needs to calculate the
	// union of all the parts of the widget

	TileRect leftrect  = left->get_rect();
	TileRect rightrect = right->get_rect();

	auto     dshape = diamond.get_shape();
	TileRect diamondrect
			= dshape ? TileRect(
							   diamondx + dshape->get_xleft(),
							   diamondy + dshape->get_yabove(),
							   dshape->get_width(), dshape->get_width())
					 : TileRect(diamondx, diamondy, 8, 8);

	return leftrect.add(rightrect).add(diamondrect);
}

/*
 *  Paint on screen.
 */

void Slider_widget::paint() {
	int sx = diamondx;
	int sy = diamondy;

	local_to_screen(sx, sy);
	diamond.paint_shape(sx, sy);

	left->paint();
	right->paint();
}

Gump_button* Slider_widget::on_button(int mx, int my) {
	if (left->on_widget(mx, my)) {
		return left;
	}
	if (right->on_widget(mx, my)) {
		return right;
	}

	return nullptr;
}

/*
 *  Handle mouse-down events.
 */

bool Slider_widget::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Gump_widget::mouse_down(mx, my, button);
	}

	Gump_button* btn = on_button(mx, my);
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
	int          lx = mx, ly = my;
	screen_to_local(lx, ly);
	if (d_shape->has_point(lx - diamondx, ly - diamondy)) {
		// Start to drag it.
		prev_dragx = mx;
	} else {
		if (ly < diamondy || ly > diamondy + d_shape->get_height()) {
			return Gump_widget::mouse_down(mx, my, button);
		}
		if (lx < xmin || lx > xmax) {
			return Gump_widget::mouse_down(mx, my, button);
		}
		diamondx               = lx;
		int delta = (diamondx - xmin) * (max_val - min_val) / xdist;
		// Round down to nearest step.
		delta -= delta % step_val;
		set_val(min_val + delta, false);

		gwin->add_dirty(get_rect());
	}

	return true;
}

/*
 *  Handle mouse-up events.
 */

bool Slider_widget::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Gump_widget::mouse_up(mx, my, button);
	}

	if (prev_dragx != INT32_MIN) {    // Done dragging?
		set_val(val);                 // Set diamond in correct pos.
		gwin->add_dirty(get_rect());
		prev_dragx = INT32_MIN;
	}
	if (!pushed) {
		return Gump_widget::mouse_up(mx, my, button);
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
#ifdef DEBUG
#	define DEBUG_VAL(v)  std::cout << #v ": " << (v) << std::endl
#else
#	define DEBUG_VAL(v) do { } while (0)
#endif

bool Slider_widget::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	ignore_unused_variable_warning(mx, my);
	if (prev_dragx == INT32_MIN) {
		return Gump_widget::mouse_drag(mx, my);
	}
	// clamp the mouse position to the slidable region
	int lx = mx, ly = my;
	screen_to_local(lx, ly);
	DEBUG_VAL(mx);
	if (lx > rightbtnx) {
		mx -= lx - rightbtnx;
	} else if (lx < xmin) {
		mx += xmin - lx;
	}

	DEBUG_VAL(diamondx);
	DEBUG_VAL(mx);
	DEBUG_VAL(prev_dragx);
	DEBUG_VAL(mx - prev_dragx);
	diamondx += mx - prev_dragx;
	prev_dragx = mx;
	if (diamondx < xmin) {    // Stop at ends.
		diamondx = xmin;
	} else if (diamondx > xmax) {
		diamondx = xmax;
	}
	DEBUG_VAL(diamondx);
	DEBUG_VAL(xdist);
	DEBUG_VAL(xmax);
	DEBUG_VAL(xmin);
	int              delta = (diamondx - xmin) * (max_val - min_val) / xdist;
	DEBUG_VAL(delta);
	DEBUG_VAL(diamondx);
	DEBUG_VAL(max_val);
	DEBUG_VAL(min_val);

	// Round down to nearest step.
	delta -= delta % step_val;
	DEBUG_VAL(delta);
	DEBUG_VAL(step_val);
	set_val(min_val + delta, false);

	// paint();
	gwin->add_dirty(get_rect());
	return true;
}

/*
 *  Handle ASCII character typed.
 */

bool Slider_widget::key_down(int chr) {
	switch (chr) {
	case SDLK_LEFT:
		clicked_left_arrow();
		break;
	case SDLK_RIGHT:
		clicked_right_arrow();
		break;
	}
	return true;
}

bool Slider_widget::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & KMOD_ALT) {
		move_diamond(-10 * step_val);
	} else {
		move_diamond(-step_val);
	}
	return true;
}

bool Slider_widget::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & KMOD_ALT) {
		move_diamond(10 * step_val);
	} else {
		move_diamond(step_val);
	}
	return true;
}
