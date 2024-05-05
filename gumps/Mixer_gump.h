/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef MIXER_GUMP_H
#define MIXER_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class Mixer_gump : public Modal_gump {
private:
	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_cancel,
		id_help,
		id_music_left,
		id_music_right,
		id_sfx_left,
		id_sfx_right,
		id_voc_left,
		id_voc_right,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	// Diamonds
	ShapeID music_slider;
	ShapeID sfx_slider;
	ShapeID speech_slider;

	// Scrollbar and Slider Info
	static const short sliderw;    // Width of Slider
	int                sliderx;    // Rel. pos. where diamond is shown.
	static short       slidery;
	int                step_val;    // Amount to step by.
	static int         max_val;
	static int         min_val;
	int                val;                    // Current value.
	void               set_val(int newval);    // Set to new value.
	static short       xmin, xmax;
	unsigned char      dragging;    // 1 if dragging the diamond.
	int                music_volume;
	int                sfx_volume;
	int                speech_volume;
	static float       factor;

public:
	Mixer_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	void move_slider(int dir);    // Scroll Line Button Pressed

	void scroll_left();

	void scroll_right();

	void load_settings();
	void save_settings();
	void cancel();
	void help();
	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;
};

#endif
