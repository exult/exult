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

#ifndef VIDEOOPTIONS_GUMP_H
#define VIDEOOPTIONS_GUMP_H

#include "Modal_gump.h"
#include "imagewin/imagewin.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class VideoOptions_gump : public Modal_gump {
	static VideoOptions_gump* video_options_gump;

private:
	uint32                 resolution;
	int                    scaling;
	int                    scaler;
	int                    fullscreen;
	uint32                 game_resolution;
	std::vector<uint32>    resolutions;
	std::vector<uint32>    win_resolutions;
	std::vector<uint32>    game_resolutions;
	int                    fill_scaler;
	Image_window::FillMode fill_mode;
	bool                   has_ac;
	bool                   share_settings;

	bool                   o_share_settings;
	uint32                 o_resolution;
	int                    o_scaling;
	int                    o_scaler;
	uint32                 o_game_resolution;
	int                    o_fill_scaler;
	Image_window::FillMode o_fill_mode;

	Image_window::FillMode startup_fill_mode;

	enum button_ids {
		id_first = 0,
		id_apply = id_first,
		id_help,
		id_cancel,
		id_fullscreen,
		id_share_settings,
		id_high_dpi,
		id_resolution,    // id_resolution and all past it
		id_scaler,        // are deleted by rebuild_buttons
		id_scaling,
		id_game_resolution,
		id_fill_scaler,
		id_fill_mode,
		id_has_ac,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	VideoOptions_gump();

	static VideoOptions_gump* get_instance() {
		return video_options_gump;
	}

	// Paint it and its contents.
	void paint() override;
	void close() override;

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;

	void toggle(Gump_button* btn, int state);
	void rebuild_buttons();
	void rebuild_dynamic_buttons();

	void load_settings(bool Fullscreen);
	void save_settings();
	void cancel();
	void help();

	void set_scaling(int scaleVal) {
		scaling = scaleVal;
	}

	void set_scaler(int scalerNum) {
		scaler = scalerNum;
	}

	void set_resolution(uint32 Res) {
		resolution = Res;
	}

	void set_game_resolution(uint32 gRes) {
		game_resolution = gRes;
	}

	void set_fill_scaler(int f_scaler) {
		fill_scaler = f_scaler;
	}

	void set_fill_mode(Image_window::FillMode f_mode) {
		fill_mode = f_mode;
	}

	void toggle_resolution(int state) {
		if (fullscreen) {
			resolution = resolutions[state];
		} else {
			resolution = win_resolutions[state];
		}
	}

	void toggle_scaling(int state) {
		scaling = state;
	}

	void toggle_scaler(int state) {
		scaler = state;
		rebuild_dynamic_buttons();
	}

	void toggle_fullscreen(int state) {
		if (share_settings) {
			fullscreen = state;
			rebuild_dynamic_buttons();
		} else {
			load_settings(state);    // overwrites old settings
			rebuild_buttons();
		}
	}

	void toggle_game_resolution(int state) {
		game_resolution = game_resolutions[state];
	}

	void toggle_fill_scaler(int state) {
		fill_scaler = state;
	}

	void toggle_fill_mode(int state) {
		if (state == 0) {
			fill_mode = Image_window::Fill;
		} else if (state == 3) {
			fill_mode = startup_fill_mode;
		} else {
			fill_mode = static_cast<Image_window::FillMode>(
					(state << 1) | (has_ac ? 1 : 0));
		}
		rebuild_dynamic_buttons();
	}

	void toggle_aspect_correction(int state) {
		has_ac    = state != 0;
		fill_mode = static_cast<Image_window::FillMode>(
				(fill_mode & ~1) | (has_ac ? 1 : 0));
	}

	void toggle_share_settings(int state) {
		share_settings = state;
	}
};

#endif
