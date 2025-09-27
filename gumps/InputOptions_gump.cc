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
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "InputOptions_gump.h"
#include "Text_button.h"
#include "exult.h"
#include "font.h"
#include "gamewin.h"
#include "items.h"
#include "touchui.h"

#include <cstring>
#include <iostream>

using std::string;


class Strings : public GumpStrings {
public:
	static auto Single() {
		return get_text_msg(0x640 - msg_file_start);
	}

	static auto Double() {
		return get_text_msg(0x641 - msg_file_start);
	}

	static auto None() {
		return get_text_msg(0x644 - msg_file_start);
	}

	static auto Left() {
		return get_text_msg(0x645 - msg_file_start);
	}

	static auto Right() {
		return get_text_msg(0x646 - msg_file_start);
	}

	static auto DoubleclickclosesGumps_() {
		return get_text_msg(0x647 - msg_file_start);
	}

	static auto RightclickclosesGumps_() {
		return get_text_msg(0x648 - msg_file_start);
	}

	static auto PathfindwithRightClick_() {
		return get_text_msg(0x649 - msg_file_start);
	}

	static auto Scrollgameviewwithmouse_() {
		return get_text_msg(0x64A - msg_file_start);
	}

	static auto UseMiddleMouseButton_() {
		return get_text_msg(0x64B - msg_file_start);
	}

	static auto FullscreenFastMouse_() {
		return get_text_msg(0x64C - msg_file_start);
	}

	static auto Itemhelpermenu_() {
		return get_text_msg(0x64D - msg_file_start);
	}

	static auto DPadscreenlocation_() {
		return get_text_msg(0x64E - msg_file_start);
	}

	static auto PathfindwithLongTouch_() {
		return get_text_msg(0x64F - msg_file_start);
	}
};

using InputOptions_button = CallbackTextButton<InputOptions_gump>;
using InputTextToggle     = CallbackToggleTextButton<InputOptions_gump>;
using InputEnabledToggle  = CallbackEnabledButton<InputOptions_gump>;

void InputOptions_gump::close() {
	save_settings();
	done = true;
}

void InputOptions_gump::cancel() {
	done = true;
}

void InputOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.html#game_input_gump");
}

void InputOptions_gump::build_buttons() {
	const std::vector<std::string> yesNo = {Strings::No(), Strings::Yes()};

	int y_index = 0;

	std::vector<string> dpad_text = {Strings::None()};
	if (touchui != nullptr) {
		dpad_text.emplace_back(Strings::Left());
		dpad_text.emplace_back(Strings::Right());
	}
	buttons[id_doubleclick] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_doubleclick, yesNo, doubleclick,
			get_button_pos_for_label(Strings::DoubleclickclosesGumps_()), yForRow(y_index), 44);

	buttons[id_rightclick_close] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_rightclick_close, yesNo,
			rightclick_close,
			get_button_pos_for_label(Strings::RightclickclosesGumps_()),
			yForRow(++y_index), 44);

	std::vector<std::string> pathfind_text
			= {Strings::No(), Strings::Single(), Strings::Double()};
	buttons[id_right_pathfind] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_right_pathfind,
			std::move(pathfind_text), right_pathfind, get_button_pos_for_label(Strings::PathfindwithRightClick_()), yForRow(++y_index),
			44);

	buttons[id_scroll_mouse] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_scroll_mouse, yesNo, scroll_mouse,
			get_button_pos_for_label(Strings::Scrollgameviewwithmouse_()), yForRow(++y_index), 44);

#ifndef SDL_PLATFORM_IOS
	buttons[id_mouse3rd] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_mouse3rd, yesNo, mouse3rd, get_button_pos_for_label(Strings::UseMiddleMouseButton_()),
			yForRow(++y_index), 44);

	buttons[id_fastmouse] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_fastmouse, yesNo, fastmouse,
			get_button_pos_for_label(Strings::FullscreenFastMouse_()), yForRow(++y_index), 44);
#endif

	buttons[id_item_menu] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_item_menu, yesNo, item_menu,
			get_button_pos_for_label(Strings::Itemhelpermenu_()), yForRow(++y_index), 44);

	buttons[id_dpad_location] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_dpad_location, dpad_text,
			dpad_location, get_button_pos_for_label(Strings::DPadscreenlocation_()), yForRow(++y_index), 44);

	buttons[id_touch_pathfind] = std::make_unique<InputTextToggle>(
			this, &InputOptions_gump::toggle_touch_pathfind, yesNo,
			touch_pathfind, get_button_pos_for_label(Strings::PathfindwithLongTouch_()), yForRow(++y_index), 44);

	// Risize to fit all
	ResizeWidthToFitWidgets(tcb::span(buttons.data() + id_first, id_count));

	HorizontalArrangeWidgets(tcb::span(buttons.data() + id_ok, 3));

	// Right align other setting buttons
	RightAlignWidgets(tcb::span(
			buttons.data() + id_first_setting, id_count - id_first_setting));
}

void InputOptions_gump::load_settings() {
	doubleclick      = 0;
	doubleclick      = gwin->get_double_click_closes_gumps();
	rightclick_close = gumpman->can_right_click_close();
	right_pathfind   = gwin->get_allow_right_pathfind();
	scroll_mouse     = gwin->can_scroll_with_mouse();
	fastmouse        = gwin->get_fastmouse(true);
	mouse3rd         = gwin->get_mouse3rd();
	item_menu        = gwin->get_item_menu();
	dpad_location    = gwin->get_dpad_location();
	touch_pathfind   = gwin->get_touch_pathfind();
}


InputOptions_gump::InputOptions_gump()
		: Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(0, 0, 100, yForRow(13)), -1);

	load_settings();

	// Ok
	buttons[id_ok] = std::make_unique<InputOptions_button>(
			this, &InputOptions_gump::close, Strings::OK(), 25,
			yForRow(12), 50);
	// Help
	buttons[id_help] = std::make_unique<InputOptions_button>(
			this, &InputOptions_gump::help, Strings::HELP(), 50,
			yForRow(12), 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<InputOptions_button>(
			this, &InputOptions_gump::cancel, Strings::CANCEL(), 75,
			yForRow(12), 50);

	build_buttons();
}

void InputOptions_gump::save_settings() {
	gwin->set_double_click_closes_gumps(doubleclick != 0);
	config->set(
			"config/gameplay/double_click_closes_gumps",
			doubleclick ? "yes" : "no", false);

	gumpman->set_right_click_close(rightclick_close != 0);
	config->set(
			"config/gameplay/right_click_closes_gumps",
			rightclick_close ? "yes" : "no", false);

	gwin->set_allow_right_pathfind(right_pathfind);
	const char* pathfind_texts[3] = {"no", "single", "double"};
	config->set(
			"config/gameplay/allow_right_pathfind",
			pathfind_texts[right_pathfind], false);

	config->set(
			"config/gameplay/scroll_with_mouse", scroll_mouse ? "yes" : "no",
			false);
	gwin->set_mouse_with_scroll(scroll_mouse);
	gwin->set_item_menu(item_menu != 0);
	config->set("config/touch/item_menu", item_menu ? "yes" : "no", false);

	gwin->set_mouse3rd(mouse3rd != 0);
	config->set("config/gameplay/mouse3rd", mouse3rd ? "yes" : "no", false);

	gwin->set_fastmouse(fastmouse != 0);
	config->set("config/gameplay/fastmouse", fastmouse ? "yes" : "no", false);

	gwin->set_dpad_location(dpad_location);

	const char* dpad_texts[3] = {"no", "left", "right"};
	config->set("config/touch/dpad_location", dpad_texts[dpad_location], false);

	gwin->set_touch_pathfind(touch_pathfind != 0);
	config->set(
			"config/touch/touch_pathfind", touch_pathfind ? "yes" : "no",
			false);

	config->write_back();

	if (touchui != nullptr) {
		touchui->onDpadLocationChanged();
	}
}

void InputOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	Image_window8*        iwin    = gwin->get_win();
	int                   y_index = 0;
	font->paint_text(
			iwin->get_ib8(), Strings::DoubleclickclosesGumps_(), x + label_margin,
			y + yForRow(y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::RightclickclosesGumps_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::PathfindwithRightClick_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::Scrollgameviewwithmouse_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	if (buttons[id_mouse3rd]) {
		font->paint_text(
				iwin->get_ib8(), Strings::UseMiddleMouseButton_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	if (buttons[id_fastmouse]) {
		font->paint_text(
				iwin->get_ib8(), Strings::FullscreenFastMouse_(), x + label_margin,
				y + yForRow(++y_index) + 1);
	}
	font->paint_text(
			iwin->get_ib8(), Strings::Itemhelpermenu_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::DPadscreenlocation_(), x + label_margin,
			y + yForRow(++y_index) + 1);
	font->paint_text(
			iwin->get_ib8(), Strings::PathfindwithLongTouch_(), x + label_margin,
			y + yForRow(++y_index) + 1);

	gwin->set_painted();
}

bool InputOptions_gump::mouse_down(int mx, int my, MouseButton button) {
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

	// On a button?
	if (pushed && !pushed->push(button)) {
		pushed = nullptr;
	}
	return pushed != nullptr || Modal_gump::mouse_down(mx, my, button);
}

bool InputOptions_gump::mouse_up(int mx, int my, MouseButton button) {
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
