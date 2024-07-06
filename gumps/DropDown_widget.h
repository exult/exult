/*
Copyright (C) 2025 The Exult Team

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

#ifndef DROPDOWN_WIDGET_H
#define DROPDOWN_WIDGET_H

#include "IterableGump_widget.h"
#include "Text_button.h"

#include <chrono>
#include <string_view>

// A widget that appears as a text button widget and when clicked pops up a
// stringlist the user can select from a choice This widget cannot act as a
// combo box and does not support text input
class DropDown_widget : public IterableGump_widget<
								std::vector<std::shared_ptr<Gump_widget>>> {
	std::vector<std::shared_ptr<Gump_widget>> children;
	std::vector<std::string>                  selections;
	std::shared_ptr<class StringList_widget>  list;
	std::chrono::steady_clock::time_point     delayed_close = {};
	bool active;    // Set when list is being shown

	int popup_sx;
	int popup_sy;

	enum {
		id_first  = 0,
		id_button = id_first,

		id_popup,
		id_count,
	};

	class Button : public Text_button {
	public:
		Button(DropDown_widget* parent, int width, int height)
				: Text_button(parent, std::string_view(), 0, 0, width, height) {
		}

		void paint() override;

		void set_text(const std::string_view& str);
	};

	std::shared_ptr<Button> button;

public:
	Gump_widget* Input_first() override {
		if (active) {
			return this;
		}

		return IterableGump_widget::Input_first();
	}

	DropDown_widget(
			Gump_Base* parent, const std::vector<std::string>& selections,
			int selection, int px, int py, int width);

	// If selection is -1 the current option is not in the selected options and
	// gettext shouldbe used to get the actual option
	int getselection() const override {
		return get_framenum();
	}

	void setselection(int newsel) override;

	iterator begin() override {
		return children.begin();
	}

	iterator end() override {
		return children.end();
	}

	bool on_widget(
			int mx, int my    // Point in window.

	) const override {
		return get_rect().has_point(mx, my);
	}

	bool run() override;

	// Input handling
	bool mouse_down(int mx, int my, MouseButton button) override;

	bool mouse_up(int mx, int my, MouseButton button) override;

	bool mousewheel_down(int mx, int my) override;

	bool mousewheel_up(int mx, int my) override;

	bool mouse_drag(int mx, int my) override;

	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;

	void onbutton(Gump_widget* sender, MouseButton);
	void onlist(Gump_widget* sender, MouseButton);

	virtual bool activate(MouseButton) {
		return true;
	}

	TileRect get_rect() const override;

	void paint() override;

	void show_popup(bool show);

	void set_delayed_close() {
		if (delayed_close != std::chrono::steady_clock::time_point()) {
			delayed_close = std::chrono::steady_clock::now();
		} else {
			delayed_close = std::chrono::steady_clock::now()
							+ std::chrono::milliseconds(100);
		}
	}
};
#endif
