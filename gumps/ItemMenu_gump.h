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

#ifndef ITEMMENU_GUMP_H
#define ITEMMENU_GUMP_H

#include "Modal_gump.h"
#include "gamewin.h"

#include <memory>
#include <vector>

class Gump_button;
class Game_object;

class Itemmenu_gump : public Modal_gump {
	enum Actions {
		no_action,
		item_menu,
		use_item,
		pickup_item,
		move_item,
		show_inventory
	};

public:
	Itemmenu_gump(Game_object_map_xy* mobjxy, int cx, int cy);
	Itemmenu_gump(Game_object* obj, int ox, int oy, int cx, int cy);
	~Itemmenu_gump() override;

	// Paint it and its contents.
	void paint() override;

	void close() override {
		done = true;
	}

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;

	void select_object(Game_object* obj);

	void set_use() {
		objectAction = use_item;
		close();
	}

	void set_pickup() {
		objectAction = pickup_item;
		close();
	}

	void set_move() {
		objectAction = move_item;
		close();
	}

	void set_inventory() {
		objectAction = show_inventory;
		close();
	}

	void cancel_menu() {
		objectSelected        = nullptr;
		objectSelectedClickXY = {-1, -1};
		close();
	}

	void postCloseActions();

private:
	void                                      fix_position(int num_elements);
	constexpr static const int                button_spacing_y = 22;
	std::vector<std::unique_ptr<Gump_button>> buttons;
	Game_object_map_xy                        objects;
	Game_object*                              objectSelected;
	Position2d                                objectSelectedClickXY;
	Actions                                   objectAction;
};
#endif
