/*
Copyright (C) 2026 The Exult Team

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

#include "Mui_game_menu.h"

#include "AudioOptions_gump.h"
#include "GameDisplayOptions_gump.h"
#include "GameEngineOptions_gump.h"
#include "Gump_manager.h"
#include "InputOptions_gump.h"
#include "Mui_router.h"
#include "Newfile_gump.h"
#include "UIOptions_gump.h"
#include "VideoOptions_gump.h"
#include "Yesno_gump.h"
#include "cheat.h"
#include "exult.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"

namespace {
	class Strings : public GumpStrings {
	public:
		static auto GameMenu() {
			return get_text_msg(0xC10 - msg_file_start);
		}

		static auto ReturnToGame() {
			return get_text_msg(0xC11 - msg_file_start);
		}

		static auto JourneyLog() {
			return get_text_msg(0xC12 - msg_file_start);
		}

		static auto Options() {
			return get_text_msg(0xC13 - msg_file_start);
		}

		static auto Cheats() {
			return get_text_msg(0xC14 - msg_file_start);
		}

		static auto Quit() {
			return get_text_msg(0xC15 - msg_file_start);
		}

		static auto ReturnHelp() {
			return get_text_msg(0xC16 - msg_file_start);
		}

		static auto JourneyHelp() {
			return get_text_msg(0xC17 - msg_file_start);
		}

		static auto OptionsHelp() {
			return get_text_msg(0xC18 - msg_file_start);
		}

		static auto CheatsHelp() {
			return get_text_msg(0xC19 - msg_file_start);
		}

		static auto QuitHelp() {
			return get_text_msg(0xC1A - msg_file_start);
		}

		static auto Back() {
			return get_text_msg(0xC1D - msg_file_start);
		}

		static auto BackHelp() {
			return get_text_msg(0xC1E - msg_file_start);
		}

		static auto ClassicHelp() {
			return get_text_msg(0xC1F - msg_file_start);
		}

		// Classic option screen names (Gamemenu_gump).
		static auto ClassicOption(int which) {
			static const unsigned ids[] = {0x5E1, 0x5E2, 0x5E7, 0x5E4, 0x5E3, 0x5E5};
			return get_text_msg(ids[which] - msg_file_start);
		}
	};
}    // namespace

Mui_game_menu::Mui_game_menu() : Mui_window(Strings::GameMenu()) {}

void Mui_game_menu::build(Mui_box& body) {
	body.gap = 10;
	auto add = [&](const char* id, const std::string& text, Mui_button::Style style, const std::string& help,
				   std::function<void()> fn) {
		auto* b   = body.add<Mui_button>(id, text, style, std::move(fn));
		b->fill_w = true;
		b->help   = help;
		return b;
	};
	if (options_page) {
		static const char* ids[] = {"menu.video", "menu.audio", "menu.ui", "menu.engine", "menu.display", "menu.input"};
		for (int i = 0; i < 6; i++) {
			add(ids[i], Strings::ClassicOption(i), Mui_button::Style::secondary, Strings::ClassicHelp(), [this, i] {
				post([this, i] {
					classic_options(i);
				});
			});
		}
		add("menu.back", Strings::Back(), Mui_button::Style::primary, Strings::BackHelp(), [this] {
			options_page = false;
			rebuild();
		});
		return;
	}
	add("menu.return", Strings::ReturnToGame(), Mui_button::Style::primary, Strings::ReturnHelp(), [this] {
		post([this] {
			finish();
		});
	});
	add("menu.journey", Strings::JourneyLog(), Mui_button::Style::secondary, Strings::JourneyHelp(), [this] {
		post([this] {
			journey_log();
		});
	});
	add("menu.options", Strings::Options(), Mui_button::Style::secondary, Strings::OptionsHelp(), [this] {
		post([this] {
			options();
		});
	});
	if (cheat()) {
		add("menu.cheats", Strings::Cheats(), Mui_button::Style::secondary, Strings::CheatsHelp(), [this] {
			post([this] {
				cheats();
			});
		});
	}
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	add("menu.quit", Strings::Quit(), Mui_button::Style::destructive, Strings::QuitHelp(), [this] {
		post([this] {
			quit();
		});
	});
#endif
}

void Mui_game_menu::journey_log() {
	bool restored = false;
	if (Mui::open_journey_log(&restored)) {
		if (restored) {
			finish();
		} else {
			follow_style();
		}
		return;
	}
	// Classic save/restore screen (themed menus without a Journey Log).
	auto* fileio = new Newfile_gump();
	gumpman->do_modal_gump(fileio, Mouse::hand);
	restored = fileio->restored_game();
	delete fileio;
	if (restored) {
		finish();
	}
}

void Mui_game_menu::options() {
	if (Mui::open_options()) {
		// The menu style or the language may have changed there.
		follow_style();
		return;
	}
	options_page = true;
	rebuild();
}

void Mui_game_menu::classic_options(int which) {
	Modal_gump* g = nullptr;
	switch (which) {
	case 0:
		g = new VideoOptions_gump();
		break;
	case 1:
		g = new AudioOptions_gump();
		break;
	case 2:
		g = new UIOptions_gump();
		break;
	case 3:
		g = new GameEngineOptions_gump();
		break;
	case 4:
		g = new GameDisplayOptions_gump();
		break;
	default:
		g = new InputOptions_gump();
		break;
	}
	gumpman->do_modal_gump(g, Mouse::hand);
	delete g;
	// Resolution or language may have changed.
	rebuild();
	gwin->set_all_dirty();
}

void Mui_game_menu::cheats() {
	if (Mui::open_cheats()) {
		follow_style();
		return;
	}
	cheat.cheat_screen();
	set_dirty();
}

void Mui_game_menu::follow_style() {
	if (!Mui::themed()) {
		// Classic now: Mui::open_game_menu() opens the classic menu.
		classic_now = true;
		finish();
		return;
	}
	switch_theme(Mui::menu_style());
}

void Mui_game_menu::quit() {
	if (!Yesno_gump::ask(GumpStrings::Doyoureallywanttoquit_())) {
		return;
	}
	quitting_time = QUIT_TIME_YES;
	finish();
}
