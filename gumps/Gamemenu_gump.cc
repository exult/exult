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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Gamemenu_gump.h"

#include "AudioOptions_gump.h"
#include "File_gump.h"
#include "GameDisplayOptions_gump.h"
#include "GameEngineOptions_gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "InputOptions_gump.h"
#include "Newfile_gump.h"
#include "Text_button.h"
#include "VideoOptions_gump.h"
#include "Yesno_gump.h"
#include "exult.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"

#include <string>

using std::string;

class Strings : public GumpStrings {
public:
	static auto LoadSaveGame() {
		return get_text_msg(0x5E0 - msg_file_start);
	}

	static auto VideoOptions() {
		return get_text_msg(0x5E1 - msg_file_start);
	}

	static auto AudioOptions() {
		return get_text_msg(0x5E2 - msg_file_start);
	}

	static auto GameDisplay() {
		return get_text_msg(0x5E3 - msg_file_start);
	}

	static auto GameEngine() {
		return get_text_msg(0x5E4 - msg_file_start);
	}

	static auto GameInput() {
		return get_text_msg(0x5E5 - msg_file_start);
	}

	static auto Quit() {
		return get_text_msg(0x5E6 - msg_file_start);
	}
};

using Gamemenu_button = CallbackTextButton<Gamemenu_gump>;

Gamemenu_gump::Gamemenu_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(0, 2, 100, yForRow(7)), -1);

	createButtons();
}

void Gamemenu_gump::createButtons() {
	int y                      = 0;
	int margin                 = 4;
	int preferred_button_width = 108;
	if (!gwin->is_in_exult_menu()) {
		buttons[id_load_save] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::loadsave, Strings::LoadSaveGame(), margin,
				yForRow(y++), preferred_button_width, 11);
	}
	buttons[id_video_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::video_options, Strings::VideoOptions(),
			margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_audio_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::audio_options, Strings::AudioOptions(),
			margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_game_engine_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::game_engine_options, Strings::GameEngine(),
			margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_game_display_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::game_display_options, Strings::GameDisplay(),
			margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_input] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::input_options, Strings::GameInput(), margin,
			yForRow(y++), preferred_button_width, 11);
#ifndef SDL_PLATFORM_IOS
	if (!gwin->is_in_exult_menu()) {
		buttons[id_quit] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::quit_exult, Strings::Quit(), margin,
				yForRow(y++), preferred_button_width, 11);
	}
#endif

	// Resize to fit all
	ResizeWidthToFitWidgets(tcb::span(buttons.data(), buttons.size()), margin);

	// Right align all the buttons
	RightAlignWidgets(tcb::span(buttons.data(), buttons.size()), margin);

	// Resize to fit the height of the buttons
	SetProceduralBackground(
			TileRect(
					0, yForRow(0) - margin, procedural_background.w,
					yForRow(y) + margin),
			-1, true);
}

//++++++ IMPLEMENT RETURN_TO_MENU!
void Gamemenu_gump::quit(bool return_to_menu) {
	ignore_unused_variable_warning(return_to_menu);
	if (!Yesno_gump::ask(Strings::Doyoureallywanttoquit_())) {
		return;
	}
	quitting_time = QUIT_TIME_YES;
	done          = true;
}

//+++++ implement actual functionality and option screens
void Gamemenu_gump::loadsave() {
	// File_gump *fileio = new File_gump();
	auto* fileio = new Newfile_gump();
	gumpman->do_modal_gump(fileio, Mouse::hand);
	if (fileio->restored_game()) {
		done = true;
	}
	delete fileio;
}

void Gamemenu_gump::video_options() {
	auto* vid_opts = new VideoOptions_gump();
	gumpman->do_modal_gump(vid_opts, Mouse::hand);

	// resolution could have changed, so recenter & repaint menu.
	set_pos();
	gwin->paint();
	gwin->show();
	delete vid_opts;

	gclock->reset_palette();
}

void Gamemenu_gump::audio_options() {
	auto* aud_opts = new AudioOptions_gump();
	gumpman->do_modal_gump(aud_opts, Mouse::hand);
	delete aud_opts;
}

void Gamemenu_gump::game_display_options() {
	auto* gp_opts = new GameDisplayOptions_gump();
	gumpman->do_modal_gump(gp_opts, Mouse::hand);
	delete gp_opts;
	// Language might have changed so recreate buttons
	createButtons();
}

void Gamemenu_gump::game_engine_options() {
	auto* cbt_opts = new GameEngineOptions_gump();
	gumpman->do_modal_gump(cbt_opts, Mouse::hand);
	delete cbt_opts;
}

void Gamemenu_gump::input_options() {
	InputOptions_gump input_opts;
	gumpman->do_modal_gump(&input_opts, Mouse::hand);
}

void Gamemenu_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	gwin->set_painted();
}

bool Gamemenu_gump::mouse_down(int mx, int my, MouseButton button) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	pushed = Gump::on_button(mx, my);
	// First try checkmark.
	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed) {    // On a button?
		pushed->push(button);
		return true;
	}

	return Modal_gump::mouse_down(mx, my, button);
}

bool Gamemenu_gump::mouse_up(int mx, int my, MouseButton button) {
	if (button != MouseButton::Left || !pushed) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		pushed->activate(MouseButton::Left);
	}
	pushed = nullptr;

	return true;
}

void Gamemenu_gump::do_exult_menu() {
	// Need to do a very small init of game data... palette, mouse, gumps
	Gamemenu_gump gmenu;
	// Does not return until gump can be deleted:
	Game_window::get_instance()->get_gump_man()->do_modal_gump(
			&gmenu, Mouse::hand);
}
