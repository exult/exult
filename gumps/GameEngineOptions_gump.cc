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
#include "GameEngineOptions_gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Notebook_gump.h"
#include "Text_button.h"
#include "cheat.h"
#include "combat_opts.h"
#include "exult.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"

#include <array>
#include <cstring>
#include <iostream>

using std::string;

namespace {

	class Strings : public GumpStrings {
	public:
		static auto Normal() {
			return get_text_msg(0x620 - msg_file_start);
		}

		static auto Hard() {
			return get_text_msg(0x621 - msg_file_start);
		}

		static auto Easiest3() {
			return get_text_msg(0x622 - msg_file_start);
		}

		static auto Easier2() {
			return get_text_msg(0x623 - msg_file_start);
		}

		static auto Easier1() {
			return get_text_msg(0x624 - msg_file_start);
		}

		static auto Harder1() {
			return get_text_msg(0x625 - msg_file_start);
		}

		static auto Harder2() {
			return get_text_msg(0x626 - msg_file_start);
		}

		static auto Hardest3() {
			return get_text_msg(0x627 - msg_file_start);
		}

		static auto Manual() {
			return get_text_msg(0x628 - msg_file_start);
		}

		static auto Automatic() {
			return get_text_msg(0x629 - msg_file_start);
		}

		static auto Takeautomaticnotes_() {
			return get_text_msg(0x62A - msg_file_start);
		}

		static auto Gumpspausegame_() {
			return get_text_msg(0x62B - msg_file_start);
		}

		static auto Alternativedraganddrop_() {
			return get_text_msg(0x62C - msg_file_start);
		}

		static auto Speed_() {
			return get_text_msg(0x62D - msg_file_start);
		}

		static auto CombatShowHits_() {
			return get_text_msg(0x62E - msg_file_start);
		}

		static auto CombatpausedwithSpace_() {
			return get_text_msg(0x62F - msg_file_start);
		}

		static auto CombatCharmedDifficulty_() {
			return get_text_msg(0x630 - msg_file_start);
		}

		static auto CombatDifficulty_() {
			return get_text_msg(0x631 - msg_file_start);
		}

		static auto Cheats_() {
			return get_text_msg(0x632 - msg_file_start);
		}

		static auto Feeding_() {
			return get_text_msg(0x633 - msg_file_start);
		}
	};

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
	SDL_OpenURL("https://exult.info/docs.html#game_engine_gump");
}

static const int small_size = 44;
static const int large_size = 85;

void GameEngineOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo   = {Strings::No(), Strings::Yes()};
	int                            y_index = 0;

	buttons[id_allow_autonotes] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_allow_autonotes, yesNo,
			allow_autonotes, get_button_pos_for_label(Strings::Takeautomaticnotes_()), yForRow(y_index), small_size);

	buttons[id_gumps_pause] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_gumps_pause, yesNo,
			gumps_pause, get_button_pos_for_label(Strings::Gumpspausegame_()), yForRow(++y_index), small_size);

	buttons[id_alternate_drop] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_alternate_drop, yesNo,
			alternate_drop, get_button_pos_for_label(Strings::Alternativedraganddrop_()), yForRow(++y_index), small_size);

	buttons[id_frames] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_frames, frametext, frames,
			get_button_pos_for_label(Strings::Speed_()), yForRow(++y_index), small_size);

	buttons[id_show_hits] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_show_hits, yesNo, show_hits,
			get_button_pos_for_label(Strings::CombatShowHits_()), yForRow(++y_index), small_size);

	// std::vector<std::string> modes = {"Original", "Space pauses"};
	buttons[id_mode] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_mode, yesNo, mode, get_button_pos_for_label(Strings::CombatpausedwithSpace_()),
			yForRow(++y_index), small_size);

	std::vector<std::string> charmedDiff = {Strings::Normal(), Strings::Hard()};
	buttons[id_charmDiff] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_charmDiff,
			std::move(charmedDiff), charmDiff, get_button_pos_for_label(Strings::CombatCharmedDifficulty_()), yForRow(++y_index),
			small_size);

	std::vector<std::string> diffs
			= {Strings::Easiest3(), Strings::Easier2(), Strings::Easier1(),
			   Strings::Normal(),   Strings::Harder1(), Strings::Harder2(),
			   Strings::Hardest3()};
	buttons[id_difficulty] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_difficulty, std::move(diffs),
			difficulty, get_button_pos_for_label(Strings::CombatDifficulty_()), yForRow(++y_index), large_size);

	buttons[id_cheats] = std::make_unique<GameEngineTextToggle>(
			this, &GameEngineOptions_gump::toggle_cheats, yesNo, cheats,
			get_button_pos_for_label(Strings::Cheats_()), yForRow(++y_index), small_size);
	y_index_cheats_start = y_index;
	update_cheat_buttons();
}

void GameEngineOptions_gump::update_cheat_buttons() {
	int y_index = y_index_cheats_start;

	if (!cheats) {
		buttons[id_feeding].reset();
	} else {
		std::vector<std::string> feedingOpts = {
				Strings::Manual(), Strings::Automatic(), Strings::Disabled()};
		buttons[id_feeding] = std::make_unique<GameEngineTextToggle>(
				this, &GameEngineOptions_gump::toggle_feeding,
				std::move(feedingOpts), feeding, get_button_pos_for_label(Strings::Feeding_()), yForRow(++y_index),
				large_size);
	}

	// Risize to fit all
	ResizeWidthToFitWidgets(tcb::span(buttons.data() + id_first, id_count));

	HorizontalArrangeWidgets(tcb::span(buttons.data() + id_ok, 3));

	// Right align other setting buttons
	RightAlignWidgets(tcb::span(
			buttons.data() + id_first_setting, id_count - id_first_setting));
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
	feeding = int(cheat.GetFoodUse(true));
}

GameEngineOptions_gump::GameEngineOptions_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(0, 0, 100, yForRow(13)), -1);


	// Ok
	buttons[id_ok] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::close, Strings::OK(), 25,
			yForRow(12), 50);
	// Help
	buttons[id_help] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::help, Strings::HELP(), 50,
			yForRow(12), 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<GameEngineOptions_button>(
			this, &GameEngineOptions_gump::cancel, Strings::CANCEL(),
			75, yForRow(12), 50);

	load_settings();
	build_buttons();
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
	cheat.SetFoodUse(Cheat::FoodUse(feeding), false);
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
	Image_window8*        iwin    = gwin->get_win();
	int                   y_index = 0;
	font->paint_text(
			iwin->get_ib8(), Strings::Takeautomaticnotes_(), x + label_margin,
			y + yForRow(y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Gumpspausegame_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Alternativedraganddrop_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Speed_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::CombatShowHits_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::CombatpausedwithSpace_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::CombatCharmedDifficulty_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::CombatDifficulty_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Cheats_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	if (buttons[id_feeding]) {
		font->paint_text(
				iwin->get_ib8(), Strings::Feeding_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
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
