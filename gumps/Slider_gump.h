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

#ifndef SLIDER_GUMP_H
#define SLIDER_GUMP_H

#include "Modal_gump.h"
#include "Slider_widget.h"

class Slider_button;

/*
 *  A slider for choosing a number.
 */

class Slider_gump : public Modal_gump, Slider_widget::ICallback {
protected:
	std::unique_ptr<Slider_widget> widget;
	bool                           allow_escape = false;

public:
	Slider_gump(int mival, int mxval, int step, int defval, bool allow_escape);

	int get_val() {    // Get last value set.
		return widget->getselection();
	}

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	bool run() override {
		return widget->run();
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;    // Character typed.

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	// Implementaion of Slider_widget::ICallback::OnSliderValueChanged
	void OnSliderValueChanged(Slider_widget* sender, int newvalue) override;
};
#endif
