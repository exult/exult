/*
Copyright (C) 2024 The Exult Team

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
#include "Slider_widget.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class Mixer_gump : public Modal_gump, public Slider_widget::ICallback {
private:
	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_cancel,
		id_help,
		id_count
	};

	std::shared_ptr<Slider_widget> midislider, oggslider, sfxslider,
			speechslider, inputslider;

	std::shared_ptr<Slider_widget> GetSlider(int sx, int sy);

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	int num_sliders = 0;    // The number of sliders the gump needs room for.
							// Set in load_settings(). will be 3 or 4

	int initial_music_ogg = -1;     // Initial midi music volume when gump
									// created. -1 if disabled
	int initial_music_midi = -1;    // Initial odd music volume when gump
									// created. -1 if disabled
	int initial_sfx = -1;           // Initial sfx volume when gump created. -1
									// if disabled
	int initial_speech = -1;        // Initial speech volume when gump created.
									// -1 if disabled
	uint8 slider_track_color;    // Palette index to use to draw slider track,
								 // set in Constructor based on procedural
								 // background colour. This will usually be 143
	int slider_width  = 120;     // Width of the sliders
	int slider_height = 7;    // Slider track height. This value is set to match
							  // the height of the diamond in the constructor

public:
	Mixer_gump();
	~Mixer_gump();

	// Paint it and its contents.
	void paint() override;
	// Close the gump and save changes
	void close() override;
	// Close the gump and revert changes
	void cancel();

protected:
	void load_settings();
	void save_settings();
	void help();
	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;
	bool key_down(SDL_Keycode chr, SDL_Keycode unicode)
			override;    // Character typed.

	//
	// Implementation of Slider_widget::ICallback
	//

	void OnSliderValueChanged(Slider_widget* sender, int newvalue) override;
};

#endif
