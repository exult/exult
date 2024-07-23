/*
 *  Copyright (C) 2001-2024  The Exult Team
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
#	include <config.h>
#endif

#include <cstring>
#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Configuration.h"
#include "Enabled_button.h"
#include "Face_stats.h"
#include "GameDisplayOptions_gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "ShortcutBar_gump.h"
#include "Text_button.h"
#include "exult.h"
#include "exult_flx.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"

using std::string;

static const int rowy[]
		= {5, 17, 29, 41, 53, 65, 77, 89, 101, 113, 125, 137, 146, 155};
static const int colx[] = {35, 50, 120, 170, 192, 209};

static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";

using GameDisplayOptions_button = CallbackTextButton<GameDisplayOptions_gump>;
using GameDisplayTextToggle = CallbackToggleTextButton<GameDisplayOptions_gump>;
using GameDisplayEnabledToggle = CallbackEnabledButton<GameDisplayOptions_gump>;

// Android stuff

bool (*GameDisplayOptions_gump::Android_getAutoLaunch)()     = nullptr;
void (*GameDisplayOptions_gump::Android_setAutoLaunch)(bool) = nullptr;

void GameDisplayOptions_gump::SetAndroidAutoLaunchFPtrs(
		void (*setter)(bool), bool (*getter)()) {
	Android_getAutoLaunch = getter;
	Android_setAutoLaunch = setter;
}

void GameDisplayOptions_gump::close() {
	save_settings();
	done = true;
}

void GameDisplayOptions_gump::cancel() {
	done = true;
}

void GameDisplayOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.php#game_display_gump");
}

void GameDisplayOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo = {"No", "Yes"};

	int y_index    = 0;
	int small_size = 44;
	int large_size = 85;

	// Status Bar Positions
	std::vector<std::string> stats
			= {"Disabled", "Left", "Middle", "Right", "Vertical"};
	buttons[id_facestats] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_facestats, std::move(stats),
			facestats, colx[5] - 41, rowy[y_index], large_size);

	std::vector<std::string> sc_enabled_txt = {"No", "Transparent", "Yes"};
	buttons[id_sc_enabled] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sc_enabled,
			std::move(sc_enabled_txt), sc_enabled, colx[5] - 41,
			rowy[++y_index], large_size);

	// keep in order of Pixel_colors
	// "No" needs to be last.
	sc_outline_txt = std::vector<std::string>{
			"Green", "White", "Yellow", "Blue", "Red", "Purple", "Black", "No"};
	buttons[id_sc_outline] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sc_outline, sc_outline_txt,
			sc_outline, colx[5], rowy[++y_index], small_size);

	buttons[id_sb_hide_missing] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sb_hide_missing, yesNo,
			sb_hide_missing, colx[5], rowy[++y_index], small_size);

	std::vector<std::string> textbgcolor
			= {"Disabled",   "Solid Gray",  "Dark Purple", "Bright Yellow",
			   "Light Blue", "Light Green", "Dark Red",    "Purple",
			   "Orange",     "Light Gray",  "Green",       "Yellow",
			   "Pale Blue",  "Dark Green",  "Red",         "Bright White",
			   "Dark Gray",  "White"};
	buttons[id_text_bg] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_text_bg,
			std::move(textbgcolor), text_bg, colx[5] - 41, rowy[++y_index],
			large_size);

	std::vector<std::string> smooth_text = {"No", "25%", "50%", "75%", "100%"};
	buttons[id_smooth_scrolling] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_smooth_scrolling,
			std::move(smooth_text), smooth_scrolling, colx[5], rowy[++y_index],
			small_size);

	buttons[id_menu_intro] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_menu_intro, yesNo,
			menu_intro, colx[5], rowy[++y_index], small_size);

	if (GAME_BG || gwin->is_in_exult_menu()) {
		buttons[id_usecode_intro] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_usecode_intro, yesNo,
				usecode_intro, colx[5], rowy[++y_index], small_size);
	}
	if (GAME_SI || gwin->is_in_exult_menu()) {
		buttons[id_extended_intro] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_extended_intro, yesNo,
				extended_intro, colx[5], rowy[++y_index], small_size);
	}

	if (sman->can_use_paperdolls()
		&& (GAME_BG || Game::get_game_type() == EXULT_DEVEL_GAME)) {
		buttons[id_paperdolls] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_paperdolls, yesNo,
				paperdolls, colx[5], rowy[++y_index], small_size);
	}
	// Android
	if (Android_getAutoLaunch) {
		buttons[id_android_autolaunch]
				= std::make_unique<GameDisplayTextToggle>(
						this, &GameDisplayOptions_gump::toggle_android_launcher,
						yesNo, android_autolaunch, colx[5], rowy[++y_index],
						small_size);
	}
}

void GameDisplayOptions_gump::load_settings() {
	string yn;
	config->value("config/gameplay/skip_intro", yn, "no");
	usecode_intro = (yn == "yes");
	config->value("config/gameplay/extended_intro", yn, "no");
	extended_intro = (yn == "yes");
	config->value("config/gameplay/skip_splash", yn, "no");
	menu_intro      = (yn == "yes");
	sc_enabled      = gwin->get_shortcutbar_type();
	sc_outline      = gwin->get_outline_color();
	sb_hide_missing = gwin->sb_hide_missing_items();
	if (gwin->is_in_exult_menu()) {
		config->value("config/gameplay/facestats", facestats, -1);
		facestats += 1;
	} else {
		facestats = Face_stats::get_state() + 1;
	}
	paperdolls = false;
	const string pdolls;
	paperdolls       = sman->are_paperdolls_enabled();
	text_bg          = gwin->get_text_bg() + 1;
	smooth_scrolling = gwin->is_lerping_enabled() / 25;

	android_autolaunch = Android_getAutoLaunch ? Android_getAutoLaunch() : 0;
}

GameDisplayOptions_gump::GameDisplayOptions_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(29, 2, 226, 165), -1);

	for (auto& btn : buttons) {
		btn.reset();
	}

	load_settings();
	build_buttons();

	// Ok
	buttons[id_ok] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::close, oktext, colx[0] - 2,
			rowy[id_count - 1], 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::cancel, canceltext, colx[5] - 6,
			rowy[id_count - 1], 50);
	// Help
	buttons[id_help] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::help, helptext, colx[2] - 3,
			rowy[id_count - 1], 50);
}

void GameDisplayOptions_gump::save_settings() {
	if (gwin->is_in_exult_menu()) {
		config->set("config/gameplay/facestats", facestats - 1, false);
	} else {
		while (facestats != Face_stats::get_state() + 1) {
			Face_stats::AdvanceState();
		}
		Face_stats::save_config(config);
	}
	string str = "no";
	if (sc_enabled == 1) {
		str = "transparent";
	} else if (sc_enabled == 2) {
		str = "yes";
	}
	config->set("config/shortcutbar/use_shortcutbar", str, false);
	config->set(
			"config/shortcutbar/use_outline_color", sc_outline_txt[sc_outline],
			false);
	config->set(
			"config/shortcutbar/hide_missing_items",
			sb_hide_missing ? "yes" : "no", false);
	gwin->set_outline_color(static_cast<Pixel_colors>(sc_outline));
	gwin->set_sb_hide_missing_items(sb_hide_missing);
	gwin->set_shortcutbar(static_cast<uint8>(sc_enabled));
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	gwin->set_text_bg(text_bg - 1);
	config->set("config/gameplay/textbackground", text_bg - 1, false);
	if (smooth_scrolling < 0) {
		smooth_scrolling = 0;
	} else if (smooth_scrolling > 4) {
		smooth_scrolling = 4;
	}
	gwin->set_lerping_enabled(smooth_scrolling * 25);
	config->set(
			"config/gameplay/smooth_scrolling", smooth_scrolling * 25, false);
	config->set(
			"config/gameplay/skip_intro", usecode_intro ? "yes" : "no", false);
	config->set(
			"config/gameplay/extended_intro", extended_intro ? "yes" : "no",
			false);
	config->set(
			"config/gameplay/skip_splash", menu_intro ? "yes" : "no", false);
	if (sman->can_use_paperdolls()
		&& (GAME_BG || Game::get_game_type() == EXULT_DEVEL_GAME)) {
		sman->set_paperdoll_status(paperdolls != 0);
		config->set(
				"config/gameplay/bg_paperdolls", paperdolls ? "yes" : "no",
				false);
	}
	config->write_back();

	if (Android_setAutoLaunch) {
		Android_setAutoLaunch(android_autolaunch != 0);
	}
}

void GameDisplayOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	std::shared_ptr<Font> font    = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8*        iwin    = gwin->get_win();
	int                   y_index = 0;
	font->paint_text(
			iwin->get_ib8(), "Status Bars:", x + colx[0],
			y + rowy[y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Use ShortcutBar :", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Use outline color :", x + colx[1],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Hide missing items:", x + colx[1],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Text Background:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Smooth scrolling:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Skip intro:", x + colx[0],
			y + rowy[++y_index] + 1);
	if (buttons[id_usecode_intro]) {
		font->paint_text(
				iwin->get_ib8(), "Skip scripted first scene:", x + colx[0],
				y + rowy[++y_index] + 1);
	}
	if (buttons[id_extended_intro]) {
		font->paint_text(
				iwin->get_ib8(), "Use extended SI intro:", x + colx[0],
				y + rowy[++y_index] + 1);
	}
	if (buttons[id_paperdolls]) {
		font->paint_text(
				iwin->get_ib8(), "Paperdolls:", x + colx[0],
				y + rowy[++y_index] + 1);
	}
	if (buttons[id_paperdolls]) {
		font->paint_text(
				iwin->get_ib8(), "Paperdolls:", x + colx[0],
				y + rowy[++y_index] + 1);
	}
	if (buttons[id_android_autolaunch]) {
		font->paint_text(
				iwin->get_ib8(), "Android autolaunch:", x + colx[0],
				y + rowy[++y_index] + 1);
	}
	gwin->set_painted();
}

bool GameDisplayOptions_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button != MouseButton::Left && button != MouseButton::Right) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}

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

	if (pushed && !pushed->push(button)) {    // On a button?
		pushed = nullptr;
	}

	return pushed != nullptr || Modal_gump::mouse_down(mx, my, button);
}

bool GameDisplayOptions_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	if (pushed->get_pushed() != button) {
		return button == MouseButton::Left;
	}

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res || Modal_gump::mouse_up(mx, my, button);
}
