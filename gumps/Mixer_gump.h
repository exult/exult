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

	std::shared_ptr<Slider_widget> musicslider, sfxslider, speechslider,
			inputslider;

	std::shared_ptr<Slider_widget> GetSlider(int sx, int sy);

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	bool music_is_ogg;    // music slider only represents ogg music volume level
						  // if oggs are enabled and there is not a midi track
						  // playing when the gump is created

	int initial_music;
	int initial_sfx;
	int initial_speech;
	bool closed = false;
		

public:
	Mixer_gump();
	~Mixer_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

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
	bool key_down(int chr) override;    // Character typed.

	// Inherited via ICallback
	void OnSliderValueChanged(Slider_widget* sender, int newvalue) override;
};

#endif
