/*
Copyright (C) 2000-2022 The Exult Team

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

#ifndef SLIDER_GUMP_H
#define SLIDER_GUMP_H

#include "Modal_gump.h"
#include "Slider_widget.h"

class Slider_button;
// #define USE_OLD_SLIDER_GUMP 1
/*
 *  A slider for choosing a number.
 */
#ifdef USE_OLD_SLIDER_GUMP
class Slider_gump : public Modal_gump {
protected:
	int           diamondx;    // Rel. pos. where diamond is shown.
	static short  diamondy;
	int           min_val, max_val;       // Max., min. values to choose from.
	int           step_val;               // Amount to step by.
	int           val;                    // Current value.
	unsigned char dragging;               // 1 if dragging the diamond.
	int           prev_dragx;             // Prev. x-coord. of mouse.
	void          set_val(int newval);    // Set to new value.

	// Coords:
	static short leftbtnx, rightbtnx, btny;
	static short xmin, xmax;

	ShapeID diamond;    // Diamond

public:
	Slider_gump(int mival, int mxval, int step, int defval);

	int get_val() {    // Get last value set.
		return val;
	}

	// An arrow was clicked on.
	void clicked_left_arrow();
	void clicked_right_arrow();
	void move_diamond(int dir);

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(int chr) override;    // Character typed.

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;
};
#else
class Slider_gump : public Modal_gump, Slider_widget::ICallback {
protected:
	std::unique_ptr<Slider_widget> widget;

public:
	Slider_gump(int mival, int mxval, int step, int defval);

	int get_val() {    // Get last value set.
		return widget->get_val();
	}

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(int chr) override;    // Character typed.

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	// Implementaion of Slider_widget::ICallback::OnSliderValueChanged
	void OnSliderValueChanged(Slider_widget* sender, int newvalue) override;
};
#endif
#endif
