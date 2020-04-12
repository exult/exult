/*
 *  Copyright (C) 2001-2015  The Exult Team
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
#include "InputOptions_gump.h"
#include "exult.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "Text_button.h"
#include "Enabled_button.h"
#include "font.h"
#include "gamewin.h"
#include "touchui.h"

using std::string;

static const int rowy[] = { 4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124, 136, 148 };
static const int colx[] = { 35, 50, 120, 170, 192, 215 };

static const char *oktext = "OK";
static const char *canceltext = "CANCEL";

static const char *dpad_texts[3] = {"no", "left", "right"};

using InputOptions_button = CallbackTextButton<InputOptions_gump>;
using InputTextToggle = CallbackToggleTextButton<InputOptions_gump>;
using InputEnabledToggle = CallbackEnabledButton<InputOptions_gump>;

void InputOptions_gump::close() {
	save_settings();
	done = true;
}

void InputOptions_gump::cancel() {
	done = true;
}

void InputOptions_gump::build_buttons() {
	std::vector<string> dpad_text = {"Disabled"};
	if (touchui != nullptr) {
		dpad_text.emplace_back("Left");
		dpad_text.emplace_back("Right");
	}

	buttons[id_item_menu] = std::make_unique<InputEnabledToggle>(this, &InputOptions_gump::toggle_item_menu,
	        item_menu, colx[4], rowy[0], 59);

	buttons[id_dpad_location] = std::make_unique<InputTextToggle>(this, &InputOptions_gump::toggle_dpad_location,
	        dpad_text, dpad_location, colx[4], rowy[1], 59);

	buttons[id_touch_pathfind] = std::make_unique<InputEnabledToggle>(this, &InputOptions_gump::toggle_touch_pathfind,
	        touch_pathfind, colx[4], rowy[2], 59);
	// Ok
	buttons[id_ok] = std::make_unique<InputOptions_button>(this, &InputOptions_gump::close,
	        oktext, colx[0], rowy[12], 59, 11);
	// Cancel
	buttons[id_cancel] = std::make_unique<InputOptions_button>(this, &InputOptions_gump::cancel,
	        canceltext, colx[4], rowy[12], 59, 11);
}

void InputOptions_gump::load_settings() {
	//string yn;
	item_menu = gwin->get_item_menu();
	dpad_location = gwin->get_dpad_location();
	touch_pathfind = gwin->get_touch_pathfind();
}

InputOptions_gump::InputOptions_gump()
	: Modal_gump(nullptr, EXULT_FLX_GAMEPLAYOPTIONS_SHP, SF_EXULT_FLX) {
	set_object_area(Rectangle(0, 0, 0, 0), 8, 162);//++++++ ???

	load_settings();
	build_buttons();
}

void InputOptions_gump::save_settings() {
	gwin->set_item_menu(item_menu != 0);
	config->set("config/touch/item_menu",
	            item_menu ? "yes" : "no", false);

	gwin->set_dpad_location(dpad_location);
	config->set("config/touch/dpad_location", dpad_texts[dpad_location], false);

	gwin->set_touch_pathfind(touch_pathfind != 0);
	config->set("config/touch/touch_pathfind",
	            touch_pathfind ? "yes" : "no", false);

	config->write_back();

	if (touchui != nullptr) {
		touchui->onDpadLocationChanged();
	}
}

void InputOptions_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	Font *font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8 *iwin = gwin->get_win();
	font->paint_text(iwin->get_ib8(), "Item helper menu:", x + colx[0], y + rowy[0] + 1);
	font->paint_text(iwin->get_ib8(), "D-Pad screen location:", x + colx[0], y + rowy[1] + 1);
	font->paint_text(iwin->get_ib8(), "Long touch pathfind:", x + colx[0], y + rowy[2] + 1);

	gwin->set_painted();
}

bool InputOptions_gump::mouse_down(int mx, int my, int button) {
	// Only left and right buttons
	if (button != 1 && button != 3) {
		return false;
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

	// On a button?
	if (pushed && !pushed->push(button)) {
		pushed = nullptr;
	}
	return button == 1 || pushed != nullptr;
}

bool InputOptions_gump::mouse_up(int mx, int my, int button) {
	// Not Pushing a button?
	if (!pushed) {
		return false;
	}

	if (pushed->get_pushed() != button) {
		return button == 1;
	}

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res;
}
