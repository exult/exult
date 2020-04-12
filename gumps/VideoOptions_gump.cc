/*
 *  Copyright (C) 2001-2013  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iostream>
#include <string>
#include <cstring>

#include "SDL_events.h"

#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_ToggleButton.h"
#include "VideoOptions_gump.h"
#include "exult.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "gameclk.h"
#include "mouse.h"
#include "Text_button.h"
#include "palette.h"
#include "Yesno_gump.h"
#include "font.h"

using std::string;

VideoOptions_gump *VideoOptions_gump::video_options_gump = nullptr;

static const int rowy[] = { 5, 17, 29, 41, 53, 65, 77, 89, 101, 113, 130, 139, 156 };
static const int colx[] = { 35, 50, 115, 127, 130, 148 };
static const char *applytext = "APPLY";

uint32 *VideoOptions_gump::resolutions = nullptr;
int VideoOptions_gump::num_resolutions = 0;

uint32 *VideoOptions_gump::win_resolutions = nullptr;
int VideoOptions_gump::num_win_resolutions = 0;

#ifdef  __IPHONEOS__
uint32 VideoOptions_gump::game_resolutions[5] = {0, 0, 0, 0, 0};
#else
uint32 VideoOptions_gump::game_resolutions[3] = {0, 0, 0};
#endif
int VideoOptions_gump::num_game_resolutions = 0;

Image_window::FillMode VideoOptions_gump::startup_fill_mode = static_cast<Image_window::FillMode>(0);

static inline uint32 make_resolution(uint16 width, uint16 height) {
	return (uint32(width) << 16) | uint32(height);
}

static inline uint16 get_width(uint32 resolution) {
	return resolution >> 16;
}

static inline uint16 get_height(uint32 resolution) {
	return (resolution & 0xffffu);
}

static string resolutionstring(int w, int h) {
	char buf[100];
	sprintf(buf, "%ix%i", w, h);
	return buf;
}

static string resolutionstring(uint32 resolution) {
	return resolutionstring(get_width(resolution), get_height(resolution));
}

using VideoOptions_button = CallbackTextButton<VideoOptions_gump>;
using VideoTextToggle = CallbackToggleTextButton<VideoOptions_gump>;

void VideoOptions_gump::close() {
	//save_settings();

	// have to repaint everything in case resolution changed
	gwin->set_all_dirty();
	done = true;
}

void VideoOptions_gump::cancel() {
	done = true;
}

void VideoOptions_gump::rebuild_buttons() {
	// skip apply, fullscreen, and share settings
	for (int i = id_resolution; i < id_count; i++) {
		buttons[i].reset();
	}

	// the text arrays are freed by the destructors of the buttons

	std::vector<std::string> scalers;
	for (int i = 0; i < Image_window::NumScalers; i++) {
		scalers.emplace_back(Image_window::get_name_for_scaler(i));
	}
	buttons[id_scaler] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_scaler,
	        std::move(scalers), scaler, colx[2], rowy[3], 74);

	std::vector<std::string> game_restext = {"Auto", "320x200"};
#ifdef __IPHONEOS__
	game_restext.emplace_back("400x250");
	game_restext.emplace_back("480x300");
#endif
	game_restext.emplace_back(resolutionstring(game_resolutions[2]));

	buttons[id_game_resolution] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_game_resolution,
	        std::move(game_restext), game_resolution, colx[2], rowy[6], 74);

	std::vector<std::string> fill_scaler_text = {"Point", "Bilinear"};
	buttons[id_fill_scaler] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_fill_scaler,
	        std::move(fill_scaler_text), fill_scaler, colx[2], rowy[7], 74);

	int sel_fill_mode;
	has_ac = false;

	if (fill_mode == Image_window::Fill) {
		sel_fill_mode = 0;
	} else if (fill_mode == Image_window::Fit) {
		sel_fill_mode = 1;
	} else if (fill_mode == Image_window::AspectCorrectFit) {
		sel_fill_mode = 1;
		has_ac = true;
	} else if (fill_mode == Image_window::Centre) {
		sel_fill_mode = 2;
	} else if (fill_mode == Image_window::AspectCorrectCentre) {
		sel_fill_mode = 2;
		has_ac = true;
	} else {
		sel_fill_mode = 3;
	}
	std::vector<std::string> fill_mode_text = {"Fill", "Fit", "Centre"};
	if (startup_fill_mode > Image_window::AspectCorrectCentre) {
		fill_mode_text.emplace_back("Custom");
	}

	buttons[id_fill_mode] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_fill_mode,
	        std::move(fill_mode_text), sel_fill_mode, colx[2], rowy[8], 74);

	rebuild_dynamic_buttons();
}

void VideoOptions_gump::rebuild_dynamic_buttons() {
	buttons[id_resolution].reset();
	buttons[id_scaling].reset();
	buttons[id_has_ac].reset();

	int num_resolutions;
	uint32 *resolutions;
	uint32 current_res = 0;

	if (fullscreen) {
		num_resolutions = VideoOptions_gump::num_resolutions;
		resolutions = VideoOptions_gump::resolutions;
		if (!gwin->get_win()->is_fullscreen())
			current_res = make_resolution(gwin->get_win()->get_display_width(), gwin->get_win()->get_display_height());
	} else {
		num_resolutions = VideoOptions_gump::num_win_resolutions;
		resolutions = VideoOptions_gump::win_resolutions;
		current_res = make_resolution(gwin->get_win()->get_display_width(), gwin->get_win()->get_display_height());
	}

	int selected_res = 0;
	std::vector<std::string> restext;
	for (int i = 0; i < num_resolutions; i++) {
		restext.emplace_back(resolutionstring(resolutions[i]));
		if (resolutions[i] <= resolution && resolutions[selected_res] < resolutions[i]) {
			selected_res = i;
		}
		if (resolutions[i] == current_res) {
			current_res = 0;
		}
	}
	if (current_res != 0) {
		restext.emplace_back(resolutionstring(current_res));

		if (resolutions[num_resolutions] <= resolution && resolutions[selected_res] < resolutions[num_resolutions]) {
			selected_res = num_resolutions;
			resolutions[num_resolutions] = current_res;
		}

		num_resolutions++;
	}

	resolution = resolutions[selected_res];

	buttons[id_resolution] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_resolution,
	        std::move(restext), selected_res, colx[2], rowy[1], 74);

	const int max_scales = scaling > 8 && scaling <= 16 ? scaling : 8;
	const int num_scales = (scaler == Image_window::point ||
	                        scaler == Image_window::interlaced ||
	                        scaler == Image_window::bilinear) ? max_scales : 1;
	if (num_scales > 1) {
		// the text arrays is freed by the destructor of the button
		std::vector<std::string> scalingtext;
		for (int i = 0; i < num_scales; i++) {
			char buf[10];
			snprintf(buf, sizeof(buf), "x%d", i + 1);
			scalingtext.emplace_back(buf);
		}
		buttons[id_scaling] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_scaling,
		        std::move(scalingtext), scaling, colx[2], rowy[4], 74);
	} else if (scaler == Image_window::Hq3x || scaler == Image_window::_3xBR)
		scaling = 2;
	else if (scaler == Image_window::Hq4x || scaler == Image_window::_4xBR)
		scaling = 3;
	else
		scaling = 1;

	if (fill_mode == Image_window::Fit || fill_mode == Image_window::AspectCorrectFit || fill_mode == Image_window::Centre || fill_mode == Image_window::AspectCorrectCentre) {
		std::vector<std::string> ac_text = {"Disabled", "Enabled"};
		buttons[id_has_ac] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_aspect_correction,
		        std::move(ac_text), has_ac ? 1 : 0, colx[3], rowy[9], 62);
	}
}

void VideoOptions_gump::load_settings(bool Fullscreen) {
	fullscreen = Fullscreen;
	int i;
	setup_video(fullscreen, MENU_INIT);
	int w = get_width(resolution);
	int h = get_height(resolution);
	if (resolutions == nullptr) {
		fullscreen = gwin->get_win()->is_fullscreen() ? 1 : 0;
		std::map<uint32, Image_window::Resolution> Resolutions = Image_window::Resolutions;
		if (fullscreen) Resolutions[make_resolution(w, h)] = Image_window::Resolution();

		num_resolutions = Resolutions.size();
		resolutions = new uint32[num_resolutions + 1];

		i = 0;
		for (std::map<uint32, Image_window::Resolution>::const_iterator it = Resolutions.begin(); it != Resolutions.end(); ++it)
			resolutions[i++] = it->first;

		// Add in useful window resolutions
		Resolutions[make_resolution(320, 200)] = Image_window::Resolution();
		Resolutions[make_resolution(320, 240)] = Image_window::Resolution();
		Resolutions[make_resolution(400, 300)] = Image_window::Resolution();
		Resolutions[make_resolution(512, 384)] = Image_window::Resolution();
		Resolutions[make_resolution(640, 400)] = Image_window::Resolution();
		Resolutions[make_resolution(640, 480)] = Image_window::Resolution();
		Resolutions[make_resolution(960, 600)] = Image_window::Resolution();
		Resolutions[make_resolution(960, 720)] = Image_window::Resolution();
		if (!fullscreen) {
			Resolutions[make_resolution(w, h)] = Image_window::Resolution();
		}
		num_win_resolutions = Resolutions.size();
		win_resolutions = new uint32[num_win_resolutions + 1];

		i = 0;
		for (std::map<uint32, Image_window::Resolution>::const_iterator it = Resolutions.begin(); it != Resolutions.end(); ++it)
			win_resolutions[i++] = it->first;
	}
	if (startup_fill_mode == 0)
		startup_fill_mode = fill_mode;
	has_ac = false;
	int gw = get_width(game_resolution);
	int gh = get_height(game_resolution);
	if (gw == 0 && gh == 0)
		game_resolution = 0;
	else if (gw == 320 && gh == 200)
		game_resolution = 1;
	else
		game_resolution = 2;

	if (num_game_resolutions == 0) {
		game_resolutions[0] = 0;
		game_resolutions[1] = make_resolution(320, 200);
#ifdef  __IPHONEOS__
		game_resolutions[2] = make_resolution(400, 250);
		game_resolutions[3] = make_resolution(480, 300);
		game_resolutions[4] = make_resolution(gw, gh);
		num_game_resolutions = (game_resolutions[0] != game_resolutions[4]
							&& game_resolutions[1] != game_resolutions[4]
							&& game_resolutions[2] != game_resolutions[4]
							&& game_resolutions[3] != game_resolutions[4]) ? 5 : 4;
#else
		game_resolutions[2] = make_resolution(gw, gh);
		num_game_resolutions = (game_resolutions[0] != game_resolutions[2] && game_resolutions[1] != game_resolutions[2]) ? 3 : 2;
#endif
	}
	gclock->reset();
	gclock->set_palette();

	o_resolution = resolution;
	o_scaling = scaling;
	o_scaler = scaler;
	o_fill_scaler = fill_scaler;
	o_fill_mode = fill_mode;
	o_game_resolution = game_resolution;
	o_highdpi = highdpi;
}

VideoOptions_gump::VideoOptions_gump() : Modal_gump(nullptr, EXULT_FLX_VIDEOOPTIONS_SHP, SF_EXULT_FLX) {
	video_options_gump = this;
	set_object_area(Rectangle(0, 0, 0, 0), 8, 170);

	const std::vector<std::string> enabledtext = {"Disabled", "Enabled"};

	fullscreen = gwin->get_win()->is_fullscreen();
#ifndef __IPHONEOS__
	buttons[id_fullscreen] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_fullscreen,
	        enabledtext, fullscreen, colx[2], rowy[0], 74);
#endif
	config->value("config/video/highdpi", highdpi, false);
	buttons[id_high_dpi] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_high_dpi,
	        enabledtext, highdpi, colx[2], rowy[2], 74);
	config->value("config/video/share_video_settings", share_settings, false);
	
	std::vector<std::string> yesNO = {"No", "Yes"};
#ifndef __IPHONEOS__
	buttons[id_share_settings] = std::make_unique<VideoTextToggle>(this, &VideoOptions_gump::toggle_share_settings,
	        std::move(yesNO), share_settings, colx[5], rowy[11], 40);
#endif
	o_share_settings = share_settings;

	buttons[id_apply] = std::make_unique<VideoOptions_button>(this, &VideoOptions_gump::save_settings,
	        applytext, colx[4], rowy[12], 59);

	load_settings(fullscreen);

	rebuild_buttons();
}

VideoOptions_gump::~VideoOptions_gump() {
	delete [] resolutions;
	delete [] win_resolutions;
	num_resolutions = num_win_resolutions = num_game_resolutions = 0;
	resolutions = win_resolutions = nullptr;
}

void VideoOptions_gump::save_settings() {
	int resx = get_width(resolution);
	int resy = get_height(resolution);
	int gw = get_width(game_resolutions[game_resolution]);
	int gh = get_height(game_resolutions[game_resolution]);

	int tgw = gw;
	int tgh = gh;
	int tw;
	int th;
	Image_window::get_draw_dims(resx, resy, scaling + 1, fill_mode, tgw, tgh, tw, th);
	if (tw / (scaling + 1) < 320 || th / (scaling + 1) < 200) {
		if (!Yesno_gump::ask("Scaled size less than 320x200.\nExult may be unusable.\nApply anyway?", "TINY_BLACK_FONT"))
			return;
	}
	if (highdpi != o_highdpi) {
		if (!Yesno_gump::ask("After toggling HighDPI you will need to restart Exult!\nApply anyway?", "TINY_BLACK_FONT"))
			return;
	}
	gwin->resized(resx, resy, fullscreen != 0, gw, gh, scaling + 1, scaler, fill_mode,
	              fill_scaler ? Image_window::bilinear : Image_window::point);
	gclock->reset();
	gclock->set_palette();
	set_pos();
	gwin->set_all_dirty();

	if (!Countdown_gump::ask("Settings applied.\nKeep?", 20)) {
		resx = get_width(o_resolution);
		resy = get_height(o_resolution);
		gw = get_width(game_resolutions[o_game_resolution]);
		gh = get_height(game_resolutions[o_game_resolution]);
		bool o_fullscreen;
		config->value("config/video/fullscreen", o_fullscreen, true);
		if (fullscreen != o_fullscreen) // use old settings from the config
			setup_video(o_fullscreen, TOGGLE_FULLSCREEN);
		else
			gwin->resized(resx, resy, o_fullscreen, gw, gh, o_scaling + 1, o_scaler, o_fill_mode,
			              o_fill_scaler ? Image_window::bilinear : Image_window::point);
		gclock->reset();
		gclock->set_palette();
		set_pos();
		gwin->set_all_dirty();
	} else {
		config->set("config/video/fullscreen", fullscreen ? "yes" : "no", false);
		config->set("config/video/share_video_settings", share_settings ? "yes" : "no", false);
		setup_video(fullscreen != 0, SET_CONFIG, resx, resy, gw, gh, scaling + 1, scaler, fill_mode,
		            fill_scaler ? Image_window::bilinear : Image_window::point);
		config->set("config/video/highdpi", highdpi ? "yes" : "no", false);
		config->write_back();
		o_resolution = resolution;
		o_scaling = scaling;
		o_scaler = scaler;
		o_game_resolution = game_resolution;
		o_fill_mode = fill_mode;
		o_fill_scaler = fill_scaler;
		o_share_settings = share_settings;
		o_highdpi = highdpi;
	}
}

void VideoOptions_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		if (btn != nullptr) {
			btn->paint();
		}
	}

	Font *font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8 *iwin = gwin->get_win();
#ifndef __IPHONEOS__
	font->paint_text(iwin->get_ib8(), "Full Screen:", x + colx[0], y + rowy[0] + 1);
	if (fullscreen) font->paint_text(iwin->get_ib8(), "Display Mode:", x + colx[0], y + rowy[1] + 1);
	else font->paint_text(iwin->get_ib8(), "Window Size:", x + colx[0], y + rowy[1] + 1);
#else
	font->paint_text(iwin->get_ib8(), "Resolution:", x + colx[0], y + rowy[1] + 1);
#endif
	font->paint_text(iwin->get_ib8(), "HighDPI:", x + colx[0], y + rowy[2] + 1);
	font->paint_text(iwin->get_ib8(), "Scaler:", x + colx[0], y + rowy[3] + 1);
	if (buttons[id_scaling] != nullptr) font->paint_text(iwin->get_ib8(), "Scaling:", x + colx[0], y + rowy[4] + 1);
	font->paint_text(iwin->get_ib8(), "Game Area:", x + colx[0], y + rowy[6] + 1);
	font->paint_text(iwin->get_ib8(), "Fill Quality:", x + colx[0], y + rowy[7] + 1);
	font->paint_text(iwin->get_ib8(), "Fill Mode:", x + colx[0], y + rowy[8] + 1);
	if (buttons[id_has_ac] != nullptr) font->paint_text(iwin->get_ib8(), "AR Correction:", x + colx[0], y + rowy[9] + 1);
#ifndef __IPHONEOS__
	font->paint_text(iwin->get_ib8(), "Same settings for window", x + colx[0], y + rowy[10] + 1);
	font->paint_text(iwin->get_ib8(), "    and fullscreen:", x + colx[0], y + rowy[11] + 1);
#endif
	gwin->set_painted();
}

bool VideoOptions_gump::mouse_down(int mx, int my, int button) {
	// Only left and right buttons
	if (button != 1 && button != 3) return false;

	// We'll eat the mouse down if we've already got a button down
	if (pushed) return true;

	// First try checkmark
	pushed = Gump::on_button(mx, my);

	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn != nullptr && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed && !pushed->push(button))            // On a button?
		pushed = nullptr;

	return button == 1 || pushed != nullptr;
}

bool VideoOptions_gump::mouse_up(int mx, int my, int button) {
	// Not Pushing a button?
	if (!pushed) return false;

	if (pushed->get_pushed() != button) return button == 1;

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my))
		res = pushed->activate(button);
	pushed = nullptr;
	return res;
}
