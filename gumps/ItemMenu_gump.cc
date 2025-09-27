/*
Copyright (C) 2011-2024 The Exult Team

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

#include "ItemMenu_gump.h"

#include "Gump_button.h"
#include "Gump_manager.h"
#include "Text_button.h"
#include "actors.h"
#include "cheat.h"
#include "exult.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "items.h"
#include "shapeinf.h"

using Itemmenu_button = CallbackTextButton<Itemmenu_gump>;
using Itemmenu_object = CallbackTextButton<Itemmenu_gump, Game_object*>;
using ObjectParams    = Itemmenu_object::CallbackParams;

class Strings : public GumpStrings {
public:
	static auto ShowInventory() {
		return get_text_msg(0x650 - msg_file_start);
	}

	static auto Talkto() {
		return get_text_msg(0x651 - msg_file_start);
	}

	static auto ShowContents() {
		return get_text_msg(0x652 - msg_file_start);
	}

	static auto Interactwith() {
		return get_text_msg(0x653 - msg_file_start);
	}

	static auto Pickup() {
		return get_text_msg(0x654 - msg_file_start);
	}

	static auto Moveto() {
		return get_text_msg(0x655 - msg_file_start);
	}

	static auto Donothing() {
		return get_text_msg(0x656 - msg_file_start);
	}
};

void Itemmenu_gump::select_object(Game_object* obj) {
	objectSelected = obj;
	auto it        = objects.find(obj);
	assert(it != objects.cend());
	objectSelectedClickXY = it->second;
	close();
}

int clamp(int val, int low, int high) {
	assert(!(high < low));
	if (val < low) {
		return low;
	}
	if (high < val) {
		return high;
	}
	return val;
}

void Itemmenu_gump::fix_position(int num_elements) {
	const int w           = Game_window::get_instance()->get_width();
	const int h           = Game_window::get_instance()->get_height();
	const int menu_height = clamp(num_elements * button_spacing_y, 0, h);
	x                     = clamp(x, 0, w - 100);
	y                     = clamp(y, 0, h - menu_height);
}

Itemmenu_gump::Itemmenu_gump(Game_object_map_xy* mobjxy, int cx, int cy)
		: Modal_gump(
				  nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP,
				  SF_EXULT_FLX) {
	objectSelected        = nullptr;
	objectSelectedClickXY = {-1, -1};
	objectAction          = no_action;
	// set_object_area(TileRect(0, 0, 0, 0), -1, -1);//++++++ ???
	int       btop = 0;
	const int maxh
			= Game_window::get_instance()->get_height() - 2 * button_spacing_y;
	for (auto it = mobjxy->begin(); it != mobjxy->end() && btop < maxh; it++) {
		Game_object* o    = it->first;
		std::string  name = o->get_name();
		// Skip objects with no name.
		if (name.empty()) {
			continue;
		}
		objects[o] = it->second;
		buttons.push_back(std::make_unique<Itemmenu_object>(
				this, &Itemmenu_gump::select_object, ObjectParams{o}, name, 10,
				btop, 59, 20));
		btop += button_spacing_y;
	}
	buttons.push_back(std::make_unique<Itemmenu_button>(
			this, &Itemmenu_gump::cancel_menu, Strings::Cancel(), 10, btop, 59,
			20));
	fix_position(buttons.size());
}

Itemmenu_gump::Itemmenu_gump(Game_object* obj, int ox, int oy, int cx, int cy)
		: Modal_gump(
				  nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP,
				  SF_EXULT_FLX) {
	// Ths gump cannot be dragged at this time
	no_dragging = true;

	objectSelected                     = obj;
	objectAction                       = item_menu;
	objectSelectedClickXY              = {ox, oy};
	int                           btop = 0;
	const Shape_info&             info = objectSelected->get_info();
	const Shape_info::Shape_class cls  = info.get_shape_class();
	const bool                    is_npc_or_monster
			= cls == Shape_info::human || cls == Shape_info::monster;
	const bool in_party = objectSelected->get_flag(Obj_flags::in_party);
	if (in_party || (is_npc_or_monster && cheat.in_pickpocket())) {
		buttons.push_back(std::make_unique<Itemmenu_button>(
				this, &Itemmenu_gump::set_inventory, Strings::ShowInventory(),
				10, btop, 59, 20));
		btop += button_spacing_y;
	}
	const bool is_avatar = objectSelected == gwin->get_main_actor();
	if (!is_avatar
		&& ((is_npc_or_monster && !cheat.in_pickpocket())
			|| (cls == Shape_info::container && !info.is_container_locked())
			|| objectSelected->usecode_exists())) {
		std::string useText;
		if (is_npc_or_monster) {
			useText = Strings::Talkto();
		} else if (cls == Shape_info::container) {
			useText = Strings::ShowContents();
		} else {
			useText = Strings::Interactwith();
		}
		buttons.push_back(std::make_unique<Itemmenu_button>(
				this, &Itemmenu_gump::set_use, useText, 10, btop, 59, 20));
		btop += button_spacing_y;
	}
	if (cheat.in_hack_mover() || objectSelected->is_dragable()) {
		buttons.push_back(std::make_unique<Itemmenu_button>(
				this, &Itemmenu_gump::set_pickup, Strings::Pickup(), 10, btop,
				59, 20));
		btop += button_spacing_y;
		buttons.push_back(std::make_unique<Itemmenu_button>(
				this, &Itemmenu_gump::set_move, Strings::Moveto(), 10, btop, 59,
				20));
		btop += button_spacing_y;
	}
	buttons.push_back(std::make_unique<Itemmenu_button>(
			this, &Itemmenu_gump::cancel_menu, Strings::Donothing(), 10, btop,
			59, 20));
	fix_position(buttons.size());
}

Itemmenu_gump::~Itemmenu_gump() {
	postCloseActions();
}

void Itemmenu_gump::paint() {
	for (auto& objPos : objects) {
		const auto& obj = objPos.first;
		obj->paint_outline(CHARMED_PIXEL);
	}
	if (objectSelected) {
		objectSelected->paint_outline(PROTECT_PIXEL);
	}
	Modal_gump::paint();
	for (auto& btn : buttons) {
		btn->paint();
	}
	gwin->set_painted();
}

bool Itemmenu_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button != MouseButton::Left && button != MouseButton::Right) {
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
			if (btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}
	// On a button?
	if (pushed && !pushed->push(button)) {
		pushed = nullptr;
	}
	return button == MouseButton::Left || pushed != nullptr;
}

bool Itemmenu_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		close();
		return false;
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
	return res;
}

void Itemmenu_gump::postCloseActions() {
	if (!objectSelected) {
		return;
	}
	Game_window* gwin = Game_window::get_instance();
	switch (objectAction) {
	case show_inventory: {
		auto* act = objectSelected->as_actor();
		if (act != nullptr) {
			act->show_inventory();
		}
		break;
	}
	case use_item:
		objectSelected->activate();
		break;
	case pickup_item: {
		Main_actor*      ava    = gwin->get_main_actor();
		const Tile_coord avaLoc = ava->get_tile();
		const int        avaX = (avaLoc.tx - gwin->get_scrolltx()) * c_tilesize;
		const int        avaY = (avaLoc.ty - gwin->get_scrollty()) * c_tilesize;
		auto*            tmpObj = gwin->find_object(avaX, avaY);
		if (tmpObj != ava) {
			// Avatar isn't in a good spot...
			// Let's give up for now :(
			break;
		}
		if (gwin->start_dragging(
					objectSelectedClickXY.x, objectSelectedClickXY.y)
			&& gwin->drag(avaX, avaY)) {
			gwin->drop_dragged(avaX, avaY, true);
		}
		break;
	}
	case move_item: {
		int tmpX;
		int tmpY;
		if (Get_click(tmpX, tmpY, Mouse::greenselect, nullptr, true)
			&& gwin->start_dragging(
					objectSelectedClickXY.x, objectSelectedClickXY.y)
			&& gwin->drag(tmpX, tmpY)) {
			gwin->drop_dragged(tmpX, tmpY, true);
		}
		break;
	}
	case no_action: {
		// Make sure menu is visible on the screen
		// This will draw a selection menu for the object
		Itemmenu_gump itemgump(
				objectSelected, objectSelectedClickXY.x,
				objectSelectedClickXY.y, x, y);
		gwin->get_gump_man()->do_modal_gump(&itemgump, Mouse::hand);
		break;
	}
	case item_menu:
		break;
	}
}
