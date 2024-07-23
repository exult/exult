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
#include "GameEngineOptions_gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Notebook_gump.h"
#include "Text_button.h"
#include "cheat.h"
#include "combat_opts.h"
#include "exult.h"
#include "exult_flx.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"

#include <array>
#include <cstring>
#include <iostream>

using std::string;

namespace {
	constexpr const std::array rowy{5,  17,  29,  41,  53,  65, 77,
									89, 101, 113, 125, 137, 146};
	constexpr const std::array colx{35, 50, 120, 170, 192, 209};

	const char* oktext     = "OK";
	const char* canceltext = "CANCEL";
	const char* helptext   = "HELP";

	std::array framerates{2, 4, 5, 6, 8, 10, -1};
	// -1 is placeholder for custom framerate
	constexpr const size_t num_default_rates = framerates.size() - 1;

	string framestring(int fr) {
		char buf[100];
		snprintf(buf, sizeof(buf), "%i fps", fr);
		return buf;
	}
}    // namespace

using GameEngineOptions_button = CallbackTextButton<GameEngineOptions_gump>;
using GameEngineTextToggle = CallbackToggleTextButton<GameEngineOptions_gump>;
using GameEngineEnabledToggle = CallbackEnabledButton<GameEngineOptions_gump>;

void GameEngineOptions_gump::close() {
	save_settings();
	done = true;
}

void GameEngineOptions_gump::cancel() {
	done = true;
}

void GameEngineOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.php#game_engine_gump");
}

void GameEngineOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo = {"No", "Yes"};

	int y_index    = 0;
	int small_size = 44;
	int large_size = 85;

	buttons[id_allow_autonotes] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_allow_autonotes, yesNo,
			allow_autonotes, colx[5], rowy[y_index], small_size);

	buttons[id_gumps_pause] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_gumps_pause, yesNo,
			gumps_pause, colx[5], rowy[++y_index], small_size);

	buttons[id_alternate_drop] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_alternate_drop, yesNo,
			alternate_drop, colx[5], rowy[++y_index], small_size);

	buttons[id_frames] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_frames, frametext, frames,
			colx[5], rowy[++y_index], small_size);

	buttons[id_show_hits] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_show_hits, yesNo, show_hits,
			colx[5], rowy[++y_index], small_size);

	// std::vector<std::string> modes = {"Original", "Space pauses"};
	buttons[id_mode] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_mode, yesNo, mode, colx[5],
			rowy[++y_index], small_size);

	std::vector<std::string> charmedDiff = {"Normal", "Hard"};
	buttons[id_charmDiff] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_charmDiff,
			std::move(charmedDiff), charmDiff, colx[5], rowy[++y_index],
			small_size);

	std::vector<std::string> diffs
			= {"Easiest (-3)", "Easier (-2)", "Easier (-1)", "Normal",
			   "Harder (+1)",  "Harder (+2)", "Hardest (+3)"};
	buttons[id_difficulty] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_difficulty, std::move(diffs),
			difficulty, colx[5] - 41, rowy[++y_index], large_size);

	buttons[id_cheats] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_cheats, yesNo, cheats,
			colx[5], rowy[++y_index], small_size);
}

void GameEngineOptions_gump::load_settings() {
	const string yn;
	cheats     = cheat();
	difficulty = Combat::difficulty;
	if (difficulty < -3) {
		difficulty = -3;
	} else if (difficulty > 3) {
		difficulty = 3;
	}
	difficulty += 3;    // Scale to choices (0-6).
	show_hits = Combat::show_hits ? 1 : 0;
	mode      = static_cast<int>(Combat::mode);
	if (mode < 0 || mode > 1) {
		mode = 0;
	}
	charmDiff             = Combat::charmed_more_difficult;
	alternate_drop        = gwin->get_alternate_drop();
	allow_autonotes       = gwin->get_allow_autonotes();
	gumps_pause           = !gumpman->gumps_dont_pause_game();
	frames                = -1;
	size_t num_framerates = num_default_rates;
	frametext.clear();
	// setting the framerate number to show by actually loading from config
	int fps;
	config->value("config/video/fps", fps, 5);
	// check that we don't get a negative framerate value
	if (fps <= 0) {
		fps = 5;    // default should be 5 frames
	}
	// Now we want to populate the framerates vector and match our current text
	// to the proper index
	for (size_t i = 0; i < num_framerates; i++) {
		frametext.emplace_back(framestring(framerates[i]));
		if (framerates[i] == fps) {
			frames = i;
		}
	}
}

GameEngineOptions_gump::GameEngineOptions_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(29, 2, 224, 156), -1);

	load_settings();
	build_buttons();

	// Ok
	buttons[id_ok] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::close, oktext, colx[0] - 2, rowy[12],
			50);
	// Cancel
	buttons[id_cancel] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::cancel, canceltext, colx[5] - 6,
			rowy[12], 50);
	// Help
	buttons[id_help] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::help, helptext, colx[2] - 3,
			rowy[12], 50);
}

void GameEngineOptions_gump::save_settings() {
	string str         = "no";
	Combat::difficulty = difficulty - 3;
	config->set("config/gameplay/combat/difficulty", Combat::difficulty, false);
	Combat::show_hits = (show_hits != 0);
	config->set(
			"config/gameplay/combat/show_hits", show_hits ? "yes" : "no",
			false);
	Combat::mode = static_cast<Combat::Mode>(mode);
	str          = Combat::mode == Combat::keypause ? "keypause" : "original";
	config->set("config/gameplay/combat/mode", str, false);
	Combat::charmed_more_difficult = charmDiff;
	config->set(
			"config/gameplay/combat/charmDifficulty",
			charmDiff ? "hard" : "normal", false);
	gwin->set_alternate_drop(alternate_drop);
	config->set(
			"config/gameplay/alternate_drop", alternate_drop ? "yes" : "no",
			false);
	gwin->set_allow_autonotes(allow_autonotes);
	config->set(
			"config/gameplay/allow_autonotes", allow_autonotes ? "yes" : "no",
			false);
	const int fps = framerates[frames];
	gwin->set_std_delay(1000 / fps);
	config->set("config/video/fps", fps, false);
	cheat.set_enabled(cheats != 0);
	gumpman->set_gumps_dont_pause_game(!gumps_pause);
	config->set(
			"config/gameplay/gumps_dont_pause_game", gumps_pause ? "no" : "yes",
			false);
	config->write_back();
}

void GameEngineOptions_gump::paint() {
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
			iwin->get_ib8(), "Take automatic notes:", x + colx[0],
			y + rowy[y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Gumps pause game:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Alternative drag and drop:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Speed:", x + colx[0], y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Combat Show Hits:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Combat paused with Space:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Combat Charmed Difficulty:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Combat Difficulty:", x + colx[0],
			y + rowy[++y_index] + 1);
	font->paint_text(
			iwin->get_ib8(), "Cheats:", x + colx[0], y + rowy[++y_index] + 1);
	gwin->set_painted();
}

bool GameEngineOptions_gump::mouse_down(int mx, int my, MouseButton button) {
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

bool GameEngineOptions_gump::mouse_up(int mx, int my, MouseButton button) {
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
