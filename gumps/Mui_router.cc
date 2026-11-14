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

#include "Mui_router.h"

#include "Configuration.h"
#include "Gump_manager.h"
#include "Mui_game_menu.h"
#include "Mui_journey_log.h"
#include "Mui_options.h"
#include "Mui_theme.h"
#include "gamewin.h"
#include "mouse.h"

namespace Mui {
	std::string menu_style() {
		std::string style;
		// Reading does not create the key; classic is the default.
		config->value("config/gameplay/menu_style", style, "classic");
		return style.empty() ? std::string("classic") : style;
	}

	bool themed() {
		const std::string style = menu_style();
		if (style == "classic") {
			return false;
		}
		Game_window* gwin = Game_window::get_instance();
		if (!gwin || gwin->is_in_exult_menu()) {
			return false;
		}
		return Mui_theme::load(style) != nullptr;
	}

	bool open_game_menu() {
		if (!themed()) {
			return false;
		}
		Mui_game_menu menu;
		Game_window::get_instance()->get_gump_man()->do_modal_gump(&menu, Mouse::hand);
		return true;
	}

	bool open_options(const char* tab) {
		if (!themed()) {
			return false;
		}
		Game_window*       gwin = Game_window::get_instance();
		Mui_options_window options(tab);
		gwin->get_gump_man()->do_modal_gump(&options, Mouse::hand);
		gwin->set_all_dirty();
		return true;
	}

	bool open_journey_log(bool* restored) {
		if (restored) {
			*restored = false;
		}
		if (!themed()) {
			return false;
		}
		Game_window*       gwin = Game_window::get_instance();
		Mui_journey_window log;
		gwin->get_gump_man()->do_modal_gump(&log, Mouse::hand);
		gwin->set_all_dirty();
		if (restored) {
			*restored = log.restored_game();
		}
		return true;
	}

	bool open_cheats() {
		// There is no themed cheat window yet.
		return false;
	}

	std::string cycle_menu_style() {
		const std::string cur  = menu_style();
		std::string       next = "classic";
		if (cur == "classic") {
			next = "ultima";
		} else if (cur == "ultima") {
			next = "modern";
		}
		config->set("config/gameplay/menu_style", next, true);
		return next;
	}
}    // namespace Mui
