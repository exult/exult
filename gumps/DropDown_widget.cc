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

#include "DropDown_widget.h"

#include "Scrollable_widget.h"
#include "StringList_widget.h"
#include "Text_button.h"
#include "gamewin.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL_keycode.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <cctype>

DropDown_widget::DropDown_widget(
		Gump_Base* par, const std::vector<std::string>& selections,
		int selection, int px, int py, int width)
		: IterableGump_widget(par, -1, px, py, selection),
		  selections(selections), active(false) {
	children.resize(id_count);
	children[id_button] = button
			= std::make_shared<CallbackButtonBase<DropDown_widget, Button>>(
					this, &DropDown_widget::onbutton, width, 0);
	Modal_gump::ProceduralColours colours = {};
	auto                          rect    = button->get_rect();
	list                                  = std::make_shared<
											 CallbackButtonBase<DropDown_widget, StringList_widget>>(
			this, &DropDown_widget::onlist, selections, selection, 0, rect.h,
			colours, 0, 0);

	button->set_self_managed(true);
	button->set_text(selections[selection]);
	set_frame(selection);
	// check where list appears on screen  and move it up if it goes off the
	// bottom
	rect = list->get_rect();

	// if diff is positive popup is off bottom of the screen so move it up by
	// diff
	int diff = rect.y + rect.h - gwin->get_height();
	if (diff > 0) {
		list->set_pos(list->get_x(), list->get_y() - diff);
	}

	if (rect.h > gwin->get_height()) {
		int sx = 0, sy = 0;
		screen_to_local(sx, sy);

		auto scrollable = std::make_shared<Scrollable_widget>(
				this, 0, sy, rect.w, gwin->get_height() - 2, 1,
				Scrollable_widget::ScrollbarType::Always, false,
				colours.Shadow);

		scrollable->add_child(list);
		children[id_popup] = scrollable;
		scrollable->set_line_height(list->get_line_height(), true);
	} else {
		children[id_popup] = list;
	}
	children[id_popup]->sort_order = Sort_Order::hidden;

	popup_sx = 0;
	popup_sy = 0;
}

void DropDown_widget::setselection(int newsel) {
	set_frame(newsel);
	// If newsel is out of bounds, something has gone seriously wrong
	std::string_view string_value;
	if (newsel >= 0 && size_t(newsel) < selections.size()) {
		string_value = selections[newsel];
	}
	list->setselection(newsel);

	// update button text
	button->set_text(string_value);
	gwin->add_dirty(get_rect());
}

bool DropDown_widget::run() {
	bool res = false;
	if (active) {
		if (delayed_close != std::chrono::steady_clock::time_point()) {
			if (delayed_close < std::chrono::steady_clock::now()) {
				show_popup(false);
				delayed_close = std::chrono::steady_clock::time_point();
			}
			res = true;
		}
		// move popup if we moved  to keep it statoc on screen
		auto popup = children[id_popup].get();
		int  curx  = popup->get_x();
		int  cury  = popup->get_y();

		local_to_screen(curx, cury);
		int diffx = curx - popup_sx;
		int diffy = cury - popup_sy;

		if (diffy != 0 || diffx != 0) {
			popup->set_pos(popup->get_x() - diffx, popup->get_y() - diffy);
			res = true;
		}
	}

	return IterableGump_widget::run() || res;
}

bool DropDown_widget::mouse_down(int mx, int my, MouseButton button) {
	if (active) {
		if (children[id_popup]->mouse_down(mx, my, button)) {
			return true;
		}
		// if child doesn't handle it, user clicked away so hide the popup and
		// go inactive

		show_popup(false);

		// if button was clicked eat the event by returning true
		if (children[id_button]->on_widget(mx, my)) {
			return true;
		}

		// click was away from us, allow event to bubble up incase ues actually
		// clicked on something else
		return false;
	}
	// pass it to the button
	return children[id_button]->mouse_down(mx, my, button);
}

bool DropDown_widget::mouse_up(int mx, int my, MouseButton button) {
	if (active) {
		if (children[id_popup]->mouse_up(mx, my, button)) {
			return true;
		}
	}
	// pass it to the button;
	return children[id_button]->mouse_up(mx, my, button);
}

bool DropDown_widget::mousewheel_down(int mx, int my) {
	if (active) {
		if (children[id_popup]->mousewheel_down(mx, my)) {
			return true;
		}
	}
	return false;
}

bool DropDown_widget::mousewheel_up(int mx, int my) {
	if (active) {
		if (children[id_popup]->mousewheel_up(mx, my)) {
			return true;
		}
	}
	return false;
}

bool DropDown_widget::mouse_drag(int mx, int my) {
	if (active) {
		if (children[id_popup]->mouse_drag(mx, my)) {
			return true;
		}
	}
	return false;
}

bool DropDown_widget::character_input(
		int chr, int unicode, bool shift_pressed) {
	ignore_unused_variable_warning(unicode, shift_pressed);
	if (!active) {
		return false;
	}
	switch (chr) {
		// Return acts as activation of list
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		onlist(list.get(), MouseButton::Unknown);
		break;

	case SDLK_ESCAPE:
		show_popup(false);
		break;

	case SDLK_UP:
		// move selection up
		list->setselection(std::max(0, list->getselection() - 1));
		break;

	case SDLK_DOWN:
		// move selection down
		{
			int newsel = std::min<int>(
					selections.size(), std::max(0, list->getselection() + 1));
			list->setselection(newsel);
			set_frame(newsel);
		}
		break;

	default:
		break;
	}

	return true;
}

void DropDown_widget::onbutton(Gump_widget*, MouseButton) {
	show_popup(true);
}

void DropDown_widget::onlist(Gump_widget*, MouseButton mbutton) {
	size_t newsel = list->getselection();
	set_frame(newsel);
	// If newsel is out of bounds, something has gone seriously wrong
	std::string_view string_value;
	if (newsel < selections.size()) {
		string_value = selections[newsel];
	}

	// update button text
	button->set_text(string_value);

	// Hide list
	set_delayed_close();

	// call activate to signal our parent
	activate(mbutton);
}

TileRect DropDown_widget::get_rect() const {
	TileRect rect(0, 0, 0, 11);
	local_to_screen(rect.x, rect.y);
	return button->get_rect().add(rect);
}

void DropDown_widget::paint() {}

void DropDown_widget::show_popup(bool show) {
	auto popup  = children[id_popup].get();
	auto button = static_cast<Button*>(children[id_button].get());
	// Showing
	if (show) {
		active = true;
		button->set_pushed(true);

		popup->sort_order = Sort_Order::ontop;
		// position it in the correct place under the button
		auto rect = button->get_rect();
		popup->set_pos(0, rect.h);

		// makesure it is not off the bottom of the screen
		rect = popup->get_rect();

		// if diff is positive popup is off bottom or right of the screen so
		// move it on by diff
		int diffx = std::max(0, rect.x + rect.w - gwin->get_width());
		int diffy = std::max(0, rect.y + rect.h - gwin->get_height());

		if (diffy > 0 || diffx > 0) {
			popup->set_pos(
					popup_sx = popup->get_x() - diffx,
					popup_sy = popup->get_y() - diffy);
			local_to_screen(popup_sx, popup_sy);
		} else {
			popup_sx = rect.x;
			popup_sy = rect.y;
		}
	}
	/// Hiding
	else {
		popup->sort_order = Sort_Order::hidden;
		active            = false;
		button->set_pushed(false);
	}
	gwin->set_all_dirty();
}

void DropDown_widget::Button::paint() {
	std::string save = {};
	save.swap(text);
	Text_button::paint();

	save.swap(text);

	auto*      ib8      = gwin->get_win()->get_ib8();
	const auto clipsave = ib8->SaveClip();
	int        offset   = is_pushed() ? 1 : 0;
	int        px       = 0;
	int        py       = 0;

	local_to_screen(px, py);
	int arrowx = px + width - 6 + offset;
	int arrowy = py + height - 4 + offset;
	//
	// Draw down arrow

	for (int i = 0; i < 4; i++) {
		ib8->fill_hline8(0, 1 + i * 2, arrowx - i, arrowy - i);
	}

	// Clip text
	TileRect newclip = clipsave.Rect().intersect(TileRect(
			px + offset, py + offset, width - 10 - offset, height - offset));
	ib8->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);
	// Paint Text
	font->paint_text(
			ib8, text.c_str(), px + text_x + offset, py + text_y + offset);
}

void DropDown_widget::Button::set_text(const std::string_view& str) {
	// Truncate the string to fit current button size and centre it
	for (int i = str.size(); i > 0; i--) {
		int text_w = font->get_text_width(str.data(), i);
		if (text_w <= width - 10) {
			text_x = std::max(2, (width - 10 - text_w) >> 1);
			text   = str.substr(0, i);
			break;
		}
	}
}
