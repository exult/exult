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
#include <cstring>

#include "SDL_events.h"

#include "Gump_manager.h"
#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_ToggleButton.h"
#include "GameplayOptions_gump.h"
#include "exult.h"
#include "exult_flx.h"
#include "game.h"
#include "gamewin.h"
#include "mouse.h"
#include "cheat.h"
#include "Face_stats.h"
#include "Text_button.h"
#include "Enabled_button.h"
#include "font.h"
#include "array_size.h"

using std::string;

static const int rowy[] = { 4, 16, 124, 28, 40, 52, 64, 76, 88, 100, 112, 148, 136 };

static const int colx[] = { 35, 50, 120, 195, 192 };

static const char *oktext = "OK";
static const char *canceltext = "CANCEL";

static int framerates[] = { 2, 4, 6, 8, 10, -1 };
// -1 is placeholder for custom framerate
static const int num_default_rates = array_size(framerates) - 1;


static string framestring(int fr) {
	char buf[100];
	sprintf(buf, "%i fps", fr);
	return buf;
}

#ifndef __IPHONEOS__
static const char *pathfind_texts[3] = {"no", "single", "double"};
#endif

using GameplayOptions_button = CallbackTextButton<GameplayOptions_gump>;
using GameplayTextToggle = CallbackToggleTextButton<GameplayOptions_gump>;
using GameplayEnabledToggle = CallbackEnabledButton<GameplayOptions_gump>;

void GameplayOptions_gump::close() {
	save_settings();
	done = true;
}

void GameplayOptions_gump::cancel() {
	done = true;
}

void GameplayOptions_gump::build_buttons() {
	std::vector<std::string> stats = {"Disabled", "Left", "Middle", "Right"};
	buttons[id_facestats] = std::make_unique<GameplayTextToggle>(this, &GameplayOptions_gump::toggle_facestats,
	        std::move(stats), facestats, colx[3], rowy[0], 59);

	std::vector<std::string> textbgcolor = {
		"Disabled",
		"Solid Light Gray", "Dark Purple", "Bright Yellow", "Light Blue",
		"Dark Green", "Dark Red", "Purple", "Orange", "Light Gray", "Green",
		"Yellow", "Pale Blue", "Dark Green", "Red", "Bright White",
		"Dark gray", "White"
	};
	buttons[id_text_bg] = std::make_unique<GameplayTextToggle>(this, &GameplayOptions_gump::toggle_text_bg,
	        std::move(textbgcolor), text_bg, colx[3] - 41, rowy[1], 100);
	if (sman->can_use_paperdolls() && (GAME_BG ||
	                                   Game::get_game_type() == EXULT_DEVEL_GAME))
		buttons[id_paperdolls] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_paperdolls,
		        paperdolls, colx[3], rowy[12], 59);
#ifndef __IPHONEOS__
	buttons[id_fastmouse] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_fastmouse,
	        fastmouse, colx[3], rowy[3], 59);
	buttons[id_mouse3rd] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_mouse3rd,
	        mouse3rd, colx[3], rowy[4], 59);
#endif
	buttons[id_doubleclick] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_doubleclick,
	        doubleclick, colx[3], rowy[5], 59);
#ifndef __IPHONEOS__
	buttons[id_rightclick_close] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_rightclick_close,
	        rightclick_close, colx[3], rowy[6], 59);

	std::vector<std::string> pathfind_text = {"Disabled", "Single", "Double"};
	buttons[id_right_pathfind] = std::make_unique<GameplayTextToggle>(this, &GameplayOptions_gump::toggle_right_pathfind,
	        std::move(pathfind_text), right_pathfind, colx[3], rowy[7], 59);
#endif
	buttons[id_gumps_pause] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_gumps_pause,
	        gumps_pause, colx[3], rowy[8], 59);
	buttons[id_cheats] = std::make_unique<GameplayEnabledToggle>(this, &GameplayOptions_gump::toggle_cheats,
	        cheats, colx[3], rowy[9], 59);
	buttons[id_frames] = std::make_unique<GameplayTextToggle>(this, &GameplayOptions_gump::toggle_frames,
	        frametext, frames, colx[3], rowy[10], 59);

	std::vector<std::string> smooth_text = {"Disabled", "25%", "50%", "75%", "100%"};
	buttons[id_smooth_scrolling] = std::make_unique<GameplayTextToggle>(this, &GameplayOptions_gump::toggle_smooth_scrolling,
	        std::move(smooth_text), smooth_scrolling, colx[3], rowy[2], 59);
}

void GameplayOptions_gump::load_settings() {
	fastmouse = gwin->get_fastmouse(true);
	mouse3rd = gwin->get_mouse3rd();
	cheats = cheat();
	if (gwin->is_in_exult_menu()) {
		config->value("config/gameplay/facestats", facestats, -1);
		facestats += 1;
	} else
		facestats = Face_stats::get_state() + 1;
	doubleclick = 0;
	paperdolls = false;
	string pdolls;
	paperdolls = sman->are_paperdolls_enabled();
	doubleclick = gwin->get_double_click_closes_gumps();
	rightclick_close = gumpman->can_right_click_close();
	right_pathfind = gwin->get_allow_right_pathfind();
	text_bg = gwin->get_text_bg() + 1;
	gumps_pause = !gumpman->gumps_dont_pause_game();
	int realframes = 1000 / gwin->get_std_delay();

	frames = -1;
	framerates[num_default_rates] = realframes;
	for (int i = 0; i < num_default_rates; i++) {
		if (realframes == framerates[i]) {
			frames = i;
			break;
		}
	}

	int num_framerates = num_default_rates;
	if (frames == -1) {
		num_framerates++;
		frames = num_default_rates;
	}
	frametext.clear();
	for (int i = 0; i < num_framerates; i++) {
		frametext.emplace_back(framestring(framerates[i]));
	}
	smooth_scrolling = gwin->is_lerping_enabled() / 25;
}

GameplayOptions_gump::GameplayOptions_gump() : Modal_gump(nullptr, EXULT_FLX_GAMEPLAYOPTIONS_SHP, SF_EXULT_FLX) {
	set_object_area(Rectangle(0, 0, 0, 0), 8, 162);//++++++ ???

	for (auto& btn : buttons)
		btn.reset();

	load_settings();

	build_buttons();

	// Ok
	buttons[id_ok] = std::make_unique<GameplayOptions_button>(this, &GameplayOptions_gump::close,
	        oktext, colx[0], rowy[11]);
	// Cancel
	buttons[id_cancel] = std::make_unique<GameplayOptions_button>(this, &GameplayOptions_gump::cancel,
	        canceltext, colx[4], rowy[11]);
}

void GameplayOptions_gump::save_settings() {
	gwin->set_text_bg(text_bg - 1);
	config->set("config/gameplay/textbackground", text_bg - 1, false);
	int fps = framerates[frames];
	gwin->set_std_delay(1000 / fps);
	config->set("config/video/fps", fps, false);
	gwin->set_fastmouse(fastmouse != 0);
	config->set("config/gameplay/fastmouse", fastmouse ? "yes" : "no", false);
	gwin->set_mouse3rd(mouse3rd != 0);
	config->set("config/gameplay/mouse3rd", mouse3rd ? "yes" : "no", false);
	gwin->set_double_click_closes_gumps(doubleclick != 0);
	config->set("config/gameplay/double_click_closes_gumps",
	            doubleclick ? "yes" : "no", false);
	gumpman->set_right_click_close(rightclick_close != 0);
	config->set("config/gameplay/right_click_closes_gumps",
	            rightclick_close ? "yes" : "no" , false);
	cheat.set_enabled(cheats != 0);
	if (gwin->is_in_exult_menu())
		config->set("config/gameplay/facestats", facestats - 1 , false);
	else {
		while (facestats != Face_stats::get_state() + 1)
			Face_stats::AdvanceState();
		Face_stats::save_config(config);
	}
	if (sman->can_use_paperdolls() && (GAME_BG ||
	                                   Game::get_game_type() == EXULT_DEVEL_GAME)) {
		sman->set_paperdoll_status(paperdolls != 0);
		config->set("config/gameplay/bg_paperdolls",
		            paperdolls ? "yes" : "no", false);
	}

#ifndef __IPHONEOS__
	gwin->set_allow_right_pathfind(right_pathfind);
	config->set("config/gameplay/allow_right_pathfind", pathfind_texts[right_pathfind], false);
#endif

	gumpman->set_gumps_dont_pause_game(!gumps_pause);
	config->set("config/gameplay/gumps_dont_pause_game", gumps_pause ? "no" : "yes", false);

	if (smooth_scrolling < 0) smooth_scrolling = 0;
	else if (smooth_scrolling > 4) smooth_scrolling = 4;
	gwin->set_lerping_enabled(smooth_scrolling * 25);
	config->set("config/gameplay/smooth_scrolling", smooth_scrolling * 25, false);
	config->write_back();
}

void GameplayOptions_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons)
		if (btn)
			btn->paint();

	Font *font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8 *iwin = gwin->get_win();

	font->paint_text(iwin->get_ib8(), "Status Bars:", x + colx[0], y + rowy[0] + 1);
	font->paint_text(iwin->get_ib8(), "Text Background:", x + colx[0], y + rowy[1] + 1);
	if (buttons[id_paperdolls])
		font->paint_text(iwin->get_ib8(), "Paperdolls:", x + colx[0], y + rowy[12] + 1);
#ifndef __IPHONEOS__
	font->paint_text(iwin->get_ib8(), "Fullscreen Fast Mouse:", x + colx[0], y + rowy[3] + 1);
	font->paint_text(iwin->get_ib8(), "Use Middle Mouse Button:", x + colx[0], y + rowy[4] + 1);
#endif
	font->paint_text(iwin->get_ib8(), "Doubleclick closes Gumps:", x + colx[0], y + rowy[5] + 1);
#ifndef __IPHONEOS__
	font->paint_text(iwin->get_ib8(), "Right click closes Gumps:", x + colx[0], y + rowy[6] + 1);
	font->paint_text(iwin->get_ib8(), "Right click Pathfinds:", x + colx[0], y + rowy[7] + 1);
#endif
	font->paint_text(iwin->get_ib8(), "Gumps pause game:", x + colx[0], y + rowy[8] + 1);
	font->paint_text(iwin->get_ib8(), "Cheats:", x + colx[0], y + rowy[9] + 1);
	font->paint_text(iwin->get_ib8(), "Speed:", x + colx[0], y + rowy[10] + 1);
	font->paint_text(iwin->get_ib8(), "Smooth scrolling:", x + colx[0], y + rowy[2] + 1);
	gwin->set_painted();
}

bool GameplayOptions_gump::mouse_down(int mx, int my, int button) {
	// Only left and right buttons
	if (button != 1 && button != 3) return false;

	// We'll eat the mouse down if we've already got a button down
	if (pushed) return true;

	// First try checkmark
	pushed = Gump::on_button(mx, my);

	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed && !pushed->push(button))            // On a button?
		pushed = nullptr;

	return button == 1 || pushed != nullptr;
}

bool GameplayOptions_gump::mouse_up(int mx, int my, int button) {
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
