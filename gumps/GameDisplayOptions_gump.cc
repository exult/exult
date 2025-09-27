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
#include "istring.h"

#include <cstring>
#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
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
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"

using std::string;

class Strings : public GumpStrings {
public:
	static auto Left() {
		return get_text_msg(0x5B0 - msg_file_start);
	}

	static auto Middle() {
		return get_text_msg(0x5B1 - msg_file_start);
	}

	static auto Right() {
		return get_text_msg(0x5B2 - msg_file_start);
	}

	static auto Vertical() {
		return get_text_msg(0x5B3 - msg_file_start);
	}

	static auto Transparent() {
		return get_text_msg(0x5B4 - msg_file_start);
	}

	static auto Black() {
		return get_text_msg(0x5B5 - msg_file_start);
	}

	static auto Green() {
		return get_text_msg(0x5B6 - msg_file_start);
	}

	static auto White() {
		return get_text_msg(0x5B7 - msg_file_start);
	}

	static auto Yellow() {
		return get_text_msg(0x5B8 - msg_file_start);
	}

	static auto Blue() {
		return get_text_msg(0x5B9 - msg_file_start);
	}

	static auto Red() {
		return get_text_msg(0x5BA - msg_file_start);
	}

	static auto Purple() {
		return get_text_msg(0x5BB - msg_file_start);
	}

	static auto SolidGray() {
		return get_text_msg(0x5BC - msg_file_start);
	}

	static auto DarkPurple() {
		return get_text_msg(0x5BD - msg_file_start);
	}

	static auto BrightYellow() {
		return get_text_msg(0x5BE - msg_file_start);
	}

	static auto LightBlue() {
		return get_text_msg(0x5BF - msg_file_start);
	}

	static auto LightGreen() {
		return get_text_msg(0x5C0 - msg_file_start);
	}

	static auto DarkRed() {
		return get_text_msg(0x5C1 - msg_file_start);
	}

	static auto Orange() {
		return get_text_msg(0x5C2 - msg_file_start);
	}

	static auto LightGray() {
		return get_text_msg(0x5C3 - msg_file_start);
	}

	static auto PaleBlue() {
		return get_text_msg(0x5C4 - msg_file_start);
	}

	static auto DarkGreen() {
		return get_text_msg(0x5C5 - msg_file_start);
	}

	static auto BrightWhite() {
		return get_text_msg(0x5C6 - msg_file_start);
	}

	static auto DarkGray() {
		return get_text_msg(0x5C7 - msg_file_start);
	}

	static auto StatusBars_() {
		return get_text_msg(0x5C8 - msg_file_start);
	}

	static auto UseShortcutBar_() {
		return get_text_msg(0x5C9 - msg_file_start);
	}

	static auto Useoutlinecolor_() {
		return get_text_msg(0x5CA - msg_file_start);
	}

	static auto Hidemissingitems_() {
		return get_text_msg(0x5CB - msg_file_start);
	}

	static auto TextBackground_() {
		return get_text_msg(0x5CC - msg_file_start);
	}

	static auto Smoothscrolling_() {
		return get_text_msg(0x5CD - msg_file_start);
	}

	static auto Skipintro_() {
		return get_text_msg(0x5CE - msg_file_start);
	}

	static auto Skipscriptedfirstscene_() {
		return get_text_msg(0x5CF - msg_file_start);
	}

	static auto UseextendedSIintro_() {
		return get_text_msg(0x5D0 - msg_file_start);
	}

	static auto Paperdolls_() {
		return get_text_msg(0x5D1 - msg_file_start);
	}

	static auto Androidautolaunch_() {
		return get_text_msg(0x5D2 - msg_file_start);
	}

	static auto Language_() {
		return get_text_msg(0x5D3 - msg_file_start);
	}

	static auto English() {
		return get_text_msg(0x5D4 - msg_file_start);
	}

	static auto French() {
		return get_text_msg(0x5D5 - msg_file_start);
	}

	static auto German() {
		return get_text_msg(0x5D6 - msg_file_start);
	}

	static auto Spanish() {
		return get_text_msg(0x5D7 - msg_file_start);
	}
};

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
	SDL_OpenURL("https://exult.info/docs.html#game_display_gump");
}

void GameDisplayOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo = {Strings::No(), Strings::Yes()};

	int y_index    = 0;
	int small_size = 44;
	int large_size = 85;

	// Status Bar Positions
	std::vector<std::string> stats
			= {Strings::Disabled(), Strings::Left(), Strings::Middle(),
			   Strings::Right(), Strings::Vertical()};
	buttons[id_facestats] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_facestats, std::move(stats),
			facestats, get_button_pos_for_label(Strings::StatusBars_()),
			yForRow(y_index), large_size);

	std::vector<std::string> sc_enabled_txt
			= {Strings::No(), Strings::Transparent(), Strings::Yes()};
	buttons[id_sc_enabled] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sc_enabled,
			std::move(sc_enabled_txt), sc_enabled, get_button_pos_for_label(Strings::UseShortcutBar_()),
			yForRow(++y_index), large_size);

	// keep in order of Pixel_colors
	// No needs to be last.
	sc_outline_txt = std::vector<std::string>{
			Strings::Black(),  Strings::Green(), Strings::White(),
			Strings::Yellow(), Strings::Blue(),  Strings::Red(),
			Strings::Purple(), Strings::No()};
	buttons[id_sc_outline] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sc_outline, sc_outline_txt,
			sc_outline, get_button_pos_for_label(Strings::Useoutlinecolor_()), yForRow(++y_index), small_size);

	buttons[id_sb_hide_missing] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_sb_hide_missing, yesNo,
			sb_hide_missing, get_button_pos_for_label(Strings::Hidemissingitems_()), yForRow(++y_index), small_size);

	std::vector<std::string> textbgcolor
			= {Strings::Disabled(),   Strings::SolidGray(),
			   Strings::DarkPurple(), Strings::BrightYellow(),
			   Strings::LightBlue(),  Strings::LightGreen(),
			   Strings::DarkRed(),    Strings::Purple(),
			   Strings::Orange(),     Strings::LightGray(),
			   Strings::Green(),      Strings::Yellow(),
			   Strings::PaleBlue(),   Strings::DarkGreen(),
			   Strings::Red(),        Strings::BrightWhite(),
			   Strings::DarkGray(),   Strings::White()};
	buttons[id_text_bg] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_text_bg,
			std::move(textbgcolor), text_bg, get_button_pos_for_label(Strings::TextBackground_()), yForRow(++y_index),
			large_size);

	std::vector<std::string> smooth_text
			= {Strings::No(), "25%", "50%", "75%", "100%"};
	buttons[id_smooth_scrolling] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_smooth_scrolling,
			std::move(smooth_text), smooth_scrolling, get_button_pos_for_label(Strings::Smoothscrolling_()), yForRow(++y_index),
			small_size);

	buttons[id_menu_intro] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_menu_intro, yesNo,
			menu_intro, get_button_pos_for_label(Strings::Skipintro_()), yForRow(++y_index), small_size);

	if (GAME_BG || gwin->is_in_exult_menu()) {
		buttons[id_usecode_intro] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_usecode_intro, yesNo,
				usecode_intro, get_button_pos_for_label(Strings::Skipscriptedfirstscene_()), yForRow(++y_index), small_size);
	}
	if (GAME_SI || gwin->is_in_exult_menu()) {
		buttons[id_extended_intro] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_extended_intro, yesNo,
				extended_intro, get_button_pos_for_label(Strings::UseextendedSIintro_()), yForRow(++y_index), small_size);
	}

	if (sman->can_use_paperdolls()
		&& (GAME_BG || Game::get_game_type() == EXULT_DEVEL_GAME)) {
		buttons[id_paperdolls] = std::make_unique<GameDisplayTextToggle>(
				this, &GameDisplayOptions_gump::toggle_paperdolls, yesNo,
				paperdolls, get_button_pos_for_label(Strings::Paperdolls_()), yForRow(++y_index), small_size);
	}
	// Android
	if (Android_getAutoLaunch) {
		buttons[id_android_autolaunch]
				= std::make_unique<GameDisplayTextToggle>(
						this, &GameDisplayOptions_gump::toggle_android_launcher,
						yesNo, android_autolaunch, get_button_pos_for_label(Strings::Androidautolaunch_()), yForRow(++y_index),
						small_size);
	}

	auto languages_txt = std::vector<std::string>{
			Strings::Default(), Strings::English(), Strings::French(),
			Strings::German(), Strings::Spanish()};
	buttons[id_language] = std::make_unique<GameDisplayTextToggle>(
			this, &GameDisplayOptions_gump::toggle_language, languages_txt,
			language, get_button_pos_for_label(Strings::Language_()), yForRow(++y_index), large_size);

	// Risize to fit all
	ResizeWidthToFitWidgets(tcb::span(buttons.data() + id_first, id_count));

	HorizontalArrangeWidgets(tcb::span(buttons.data() + id_ok, 3));

	// Right align other setting buttons
	RightAlignWidgets(tcb::span(
			buttons.data() + id_first_setting, id_count - id_first_setting));
}

void GameDisplayOptions_gump::load_settings() {
	string value;
	config->value("config/gameplay/skip_intro", value, "no");
	usecode_intro = (value == "yes");
	config->value("config/gameplay/extended_intro", value, "no");
	extended_intro = (value == "yes");
	config->value("config/gameplay/skip_splash", value, "no");
	menu_intro      = (value == "yes");
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
	config->value("config/gameplay/language", value, "");
	Pentagram::tolower(value);
	if (value == "en") {
		language = 1;
	} else if (value == "fr") {
		language = 2;
	} else if (value == "de") {
		language = 3;
	} else if (value == "es") {
		language = 4;
	} else {
		language = 0;
	}
}

GameDisplayOptions_gump::GameDisplayOptions_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(0, 2, 100, yForRow(13)), -1);

	for (auto& btn : buttons) {
		btn.reset();
	}

	// Ok
	buttons[id_ok] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::close, Strings::OK(), 15,
			yForRow(12), 50);
	// Help
	buttons[id_help] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::help, Strings::HELP(), 50,
			yForRow(12), 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<GameDisplayOptions_button>(
			this, &GameDisplayOptions_gump::cancel, Strings::CANCEL(),
			75, yForRow(12), 50);

	load_settings();
	build_buttons();
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
	gwin->set_extended_intro(extended_intro);
	config->set(
			"config/gameplay/skip_splash", menu_intro ? "yes" : "no", false);
	if (sman->can_use_paperdolls()
		&& (GAME_BG || Game::get_game_type() == EXULT_DEVEL_GAME)) {
		sman->set_paperdoll_status(paperdolls != 0);
		config->set(
				"config/gameplay/bg_paperdolls", paperdolls ? "yes" : "no",
				false);
	}
	if (Android_setAutoLaunch) {
		Android_setAutoLaunch(android_autolaunch != 0);
	}

	const char* langcodes[] = {"", "en", "fr", "de", "es"};
	if (language >= 0 && size_t(language) < std::size(langcodes)) {
		config->set("config/gameplay/language", langcodes[language], false);

		// Setup text incase language changed
		Game::setup_text();
	}
	config->write_back();
}

void GameDisplayOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	Image_window8*        iwin    = gwin->get_win();
	int                   y_index = 0;
	font->paint_text(
			iwin->get_ib8(), Strings::StatusBars_(), x + label_margin,
			y + yForRow(y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::UseShortcutBar_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Useoutlinecolor_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Hidemissingitems_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::TextBackground_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Smoothscrolling_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Skipintro_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	if (buttons[id_usecode_intro]) {
		font->paint_text(
				iwin->get_ib8(), Strings::Skipscriptedfirstscene_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	if (buttons[id_extended_intro]) {
		font->paint_text(
				iwin->get_ib8(), Strings::UseextendedSIintro_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	if (buttons[id_paperdolls]) {
		font->paint_text(
				iwin->get_ib8(), Strings::Paperdolls_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	if (buttons[id_android_autolaunch]) {
		font->paint_text(
				iwin->get_ib8(), Strings::Androidautolaunch_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	if (buttons[id_language]) {
		font->paint_text(
				iwin->get_ib8(), Strings::Language_(), x + label_margin,
				y + yForRow(++y_index) + 1);
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
