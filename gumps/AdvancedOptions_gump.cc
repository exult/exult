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

#include "AdvancedOptions_gump.h"

#include "Configuration.h"
#include "DropDown_widget.h"
#include "Gump_button.h"
#include "StringList_widget.h"
#include "Yesno_gump.h"
#include "gamewin.h"
#include "items.h"

class Strings : public GumpStrings {
public:
	static auto Applyanyway() {
		return get_text_msg(0x5F0 - msg_file_start);
	}
};

AdvancedOptions_gump::AdvancedOptions_gump(
		std::vector<ConfigSetting_widget::Definition>* settings,
		std::string&& title, std::string&& helpurl,
		std::function<void()> applycallback)
		: Modal_gump(nullptr, -1), title(std::move(title)),
		  helpurl(std::move(helpurl)),
		  applycallback(applycallback) {
	elems.reserve(5);
	TileRect rect = TileRect(0, 0, 200, 186);
	SetProceduralBackground(rect, -1, true);

	int button_w = 100;
	int scrolly  = 20;
	// Using raw pointers because contents of elems gets deleted when Gump
	// destructor is called
	int scrollw = rect.w - 16;
	scroll      = new Scrollable_widget(
            this, label_margin-2, scrolly, scrollw, rect.h - scrolly - 16, 2,
            Scrollable_widget::ScrollbarType::Auto, true,
            procedural_colours.Shadow);
	elems.push_back(scroll);

	int max_width = 0;
	for (auto& setting : *settings) {
		auto csw = std::make_shared<ConfigSetting_widget>(
				this, 0, 0, button_w, setting, font, 2);
		max_width = std::max(max_width, csw->get_rect().w);

		scroll->add_child(std::move(csw));
	}
	// run the scroll bar so it can up update itself
	scroll->run();
	// If there is no scrollbar the widget width should be scrollw+12
	max_width        = std::max(max_width, scroll->GetUsableArea().w);
	scroll->expand(1+max_width - scroll->GetUsableArea().w, 0);
	// Shift the buttons as needed so they right align on the edge of the Scrllable
	for (auto& child : scroll->get_children_iterable()) {
			auto csw = dynamic_cast<ConfigSetting_widget*>(child.get());
			if (csw) {
				// Shift the buttons so they all right aligned
				csw->shift_buttons_x(max_width - csw->get_rect().w);
			}
		}
	

	// Apply
	int buttony    = 173;
	int button_gap = 20;
	const int id_ok      = elems.size();
	elems.push_back(
			apply = new CallbackTextButton<AdvancedOptions_gump>(
					this, &AdvancedOptions_gump::on_apply, Strings::APPLY(),
					rect.w / 2 - 25 - 50 - button_gap, buttony, 50));
	// Help
	elems.push_back(
			help = new CallbackTextButton<AdvancedOptions_gump>(
					this, &AdvancedOptions_gump::on_help, Strings::HELP(),
					rect.w / 2 + 25 + button_gap, buttony, 50));
	// Cancel
	elems.push_back(
			cancel = new CallbackTextButton<AdvancedOptions_gump>(
					this, &AdvancedOptions_gump::on_cancel, Strings::CANCEL(),
					rect.w / 2 - 25, buttony, 50));

	// resize to fit everything
	ResizeWidthToFitWidgets(tcb::span(elems.data(),elems.size()),2);
	ResizeWidthToFitText(this->title.c_str());
	HorizontalArrangeWidgets(
			tcb::span(elems.data() + id_ok, 3));

	// set all buttons in elems to self managed
	for (auto elem : elems) {
		Gump_button* button = dynamic_cast<Gump_button*>(elem);
		if (button) {
			button->set_self_managed(true);
		}
	}
}

AdvancedOptions_gump::~AdvancedOptions_gump() {}

bool AdvancedOptions_gump::mouse_down(int mx, int my, MouseButton button) {
	// try input first widget
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->mouse_down(mx, my, button)) {
			return true;
		}
	}
	// try the rest of the widgets
	Gump_widget* found = nullptr;
	while ((found = Gump_widget::findSorted(mx, my, elems, found))) {
		if (found->mouse_down(mx, my, button)) {
			return true;
		}
	}

	// pass to our parentclass
	return Modal_gump::mouse_down(mx, my, button);
}

bool AdvancedOptions_gump::mouse_up(int mx, int my, MouseButton button) {
	// try input first widget
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->mouse_up(mx, my, button)) {
			return true;
		}
	}
	// try the rest of the widgets
	Gump_widget* found = nullptr;

	while ((found = Gump_widget::findSorted(mx, my, elems, found))) {
		if (found->mouse_up(mx, my, button)) {
			return true;
		}
	}
	// pass to our parentclass
	return Modal_gump::mouse_up(mx, my, button);
}

bool AdvancedOptions_gump::mouse_drag(int mx, int my) {
	// try input first widget
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->mouse_drag(mx, my)) {
			return true;
		}
	}

	// try the rest of the widgets
	Gump_widget* found = nullptr;

	while ((found = Gump_widget::findSorted(mx, my, elems, found))) {
		if (found->mouse_drag(mx, my)) {
			return true;
		}
	}
	// pass to our parentclass
	return Modal_gump::mouse_drag(mx, my);
}

bool AdvancedOptions_gump::mousewheel_down(int mx, int my) {
	// try input first widget
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->mousewheel_down(mx, my)) {
			return true;
		}
	}

	// try the rest of the widgets
	Gump_widget* found = nullptr;

	while ((found = Gump_widget::findSorted(mx, my, elems, found))) {
		if (found->mousewheel_down(mx, my)) {
			return true;
		}
	}
	// pass to our parentclass
	return Modal_gump::mousewheel_down(mx, my);
}

bool AdvancedOptions_gump::mousewheel_up(int mx, int my) {
	// try input first widget
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->mousewheel_up(mx, my)) {
			return true;
		}
	}

	// try the rest of the widgets
	Gump_widget* found = nullptr;

	while ((found = Gump_widget::findSorted(mx, my, elems, found))) {
		if (found->mousewheel_up(mx, my)) {
			return true;
		}
	}
	// pass to our parentclass
	return Modal_gump::mousewheel_up(mx, my);
}

bool AdvancedOptions_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	// try input first widget only
	for (auto& child : elems) {
		auto found = child->Input_first();
		if (found && found->key_down(chr, unicode)) {
			return true;
		}
	}
	// pass to our parentclass
	return Modal_gump::key_down(chr, unicode);
}

void AdvancedOptions_gump::paint() {
	auto ib = gwin->get_win()->get_ib8();
	Modal_gump::paint();
	int titlex    = label_margin;
	int titley    = 1;
	local_to_screen(titlex, titley);
	// Draw Title
	font->paint_text_box(
			ib, title.c_str(), titlex, titley, procedural_background.w, 30);

	Image_buffer::ClipRectSave clip(ib);
	TileRect                   newclip = clip.Rect().intersect(get_rect());
	ib->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);
	Gump_widget::paintSorted(elems);
}

bool AdvancedOptions_gump::run() {
	int res = Modal_gump::run();

	for (auto elem : elems) {
		res |= elem->run();
	}

	return res;
}

void AdvancedOptions_gump::paint_elems() {}

void AdvancedOptions_gump::on_apply() {
	// First step validate
	for (auto& child : scroll->get_children_iterable()) {
		auto csw = dynamic_cast<ConfigSetting_widget*>(child.get());
		if (csw) {
			std::string validation_message = csw->Validate();
			if (!validation_message.empty()) {
				validation_message += Strings::Applyanyway();
				if (!Yesno_gump::ask(
							validation_message.c_str(), nullptr,
							"TINY_BLACK_FONT")) {
					return;
				}
			}
		}
	}
	// then apply settings if validation passed or user is forcing
	for (auto& child : scroll->get_children_iterable()) {
		auto csw = dynamic_cast<ConfigSetting_widget*>(child.get());
		if (csw) {
			csw->save_to_config(false);
		}
	}
	config->write_back();
	if (applycallback) {
		applycallback();
	}
}

void AdvancedOptions_gump::on_cancel() {
	close();
}

void AdvancedOptions_gump::on_help() {
	SDL_OpenURL(helpurl.c_str());
}
