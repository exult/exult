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

#include "ConfigSetting_widget.h"

#include "Configuration.h"
#include "DropDown_widget.h"
#include "Gump_ToggleButton.h"
#include "StringList_widget.h"
#include "gamewin.h"
#include "istring.h"
#include "listfiles.h"

#include <algorithm>
int         ConfigSetting_widget::bgstripe     = 0;
const uint8 ConfigSetting_widget::bgcolours[4] = {143, 143, 143, 143};

ConfigSetting_widget::ConfigSetting_widget(
		Gump_Base* parent, int px, int py, int butonwidth, int col2,
		const Definition& s, std::shared_ptr<Font> font, int line_gap)
		: IterableGump_widget(parent, -1, px, py, -1), setting(s), font(font) {
	if (setting.additional < 0) {
		setting.additional = 0;
	}
	// Allocate the space for the buttons and widgets
	children.resize(setting.additional + 1);
	initial.resize(setting.additional + 1);

	// Add in default choice if needed
	int default_choice = setting.find_choice(setting.default_value);
	if (default_choice == -1) {
		// add the default choice into settings
		std::string label = setting.default_value;
		if (label.empty()) {
			label = "(Default)";
		}
		setting.choices.push_back(
				{std::move(label), setting.default_value,
				 setting.default_value});
	}

	// Add in the empty choice if not required or have additional settings
	emptychoice = setting.find_choice("");
	if (emptychoice == -1 && (!setting.required || setting.additional > 0)) {
		emptychoice = setting.choices.size();
		setting.choices.push_back(
				ConfigSetting_widget::Definition::Choice{"(Unset)", "", ""});
	}

	// convert choices into string vector
	std::vector<std::string> sv_choices;

	sv_choices.reserve(setting.choices.size() + 1);
	for (auto& a : setting.choices) {
		sv_choices.push_back(a.label);
	}
	int offset_y  = 0;
	missingchoice = -1;
	for (int i = -1; i < setting.additional; i++) {
		std::string      config_key;
		std::string_view default_value;
		if (i != -1) {
			config_key    = setting.config_setting + std::to_string(i);
			default_value = std::string_view();
		} else {
			config_key    = setting.config_setting;
			default_value = setting.default_value;
		}

		int         current_index;
		std::string value;
		config->value(config_key, value, std::string());
		if (value.empty()) {
			if (!default_value.empty()) {
				current_index = setting.find_choice(default_value);
			} else {
				current_index = emptychoice;
			}
		} else {
			current_index = setting.find_choice(value);
			// Couldn't findit so choose default
			// if (current_index == -1)
			//	current_index = setting.find_choice(default_value);
		}
		// index is invalid so add to choices
		if (current_index == -1) {
			if (missingchoice == -1) {
				missingchoice = sv_choices.size();
			}
			sv_choices.push_back(value);
			current_index = missingchoice;
		}

		initial[i + 1] = current_index;

		// create widget
		switch (setting.setting_type) {
		case Definition::dropdown: {
			children[i + 1] = std::make_unique<
					CallbackButtonBase<ConfigSetting_widget, DropDown_widget>>(
					this, &ConfigSetting_widget::onselectionmb, sv_choices,
					current_index, col2, offset_y, butonwidth);

			break;
		}
		case Definition::list: {
			Modal_gump::ProceduralColours colours = {};
			children[i + 1] = std::make_unique<CallbackButtonBase<
					ConfigSetting_widget, StringList_widget>>(
					this, &ConfigSetting_widget::onselectionmb, sv_choices,
					current_index, col2, offset_y, colours, butonwidth, 0);
			break;
		}
		case Definition::button: {
			children[i + 1] = std::make_unique<
					SelfManagedCallbackToggleTextButton<ConfigSetting_widget>>(
					this, &ConfigSetting_widget::onselection, sv_choices,
					current_index, col2, offset_y, butonwidth, 0);

			break;
		}
		default:
			// do nothing
			break;
		}
		if (children[i + 1]) {
			offset_y += children[i + 1]->get_rect().h + line_gap;
		}
		if (current_index == missingchoice) {
			sv_choices.pop_back();
		}
	}
}

void ConfigSetting_widget::shift_buttons_x(int offset) {
	// we need to reposition all our children

	for (auto& child : children) {
		if (child) {
			child->set_pos(child->get_x() + offset, child->get_y());
		}
	}
}

constexpr static int Count_digits10(unsigned val) {
	int digits = 1;

	while (val /= 10) {
		digits++;
	}

	return digits;
}

static int int_to_str(unsigned val, char* str) {
	int d = Count_digits10(val);
	// Add a null terminator
	str[d + 1] = 0;
	int count  = 0;

	do {
		str[d] = '0' + val % 10;
		val /= 10;
		d--;
		count++;
	} while (val);

	return count + 1;
}

void ConfigSetting_widget::paint() {
	auto ib = gwin->get_win()->get_ib8();

	// Label drawing
	// Copy string to temporary buffer.
	// If a label is longer than 64 chars, someone is doing something wrong and
	// this code will truncate the string as needed
	char         m[64];
	const size_t len = setting.label.copy(
			m, std::size(m) - (Count_digits10(setting.additional) + 3), 0);

	for (int i = -1;
		 i < setting.additional && (i < 0 || size_t(i) < children.size());
		 i++) {
		auto& child = children[i + 1];

		// Paint the label for the child
		if (child) {
			// Add the suffix

			if (i != -1) {
				m[len]   = ' ';
				int p    = len + int_to_str(i, m + len);
				m[p]     = ':';
				m[p + 1] = 0;
			} else {
				// remove suffix if there is one replace with colon
				m[len]     = ':';
				m[len + 1] = 0;
			}
			TileRect childrect = child->get_rect();
			TileRect rect(0, child->get_y(), 0, 0);
			local_to_screen(rect.x, rect.y);
			rect = rect.add(childrect);
			// do stripped bg
			uint8 bgcol = bgcolours[bgstripe];
			bgstripe    = (bgstripe + 1) % std::size(bgcolours);

			if (bgcol != 0xff) {
				ib->draw_box(rect.x, rect.y, rect.w, rect.h, 0, bgcol, 0xff);
			}

			// Draw label label_offset at start of line 1 pixel down
			font->paint_text(ib, m, rect.x + 1, rect.y + 1);
		}
	}
}

void ConfigSetting_widget::revert() {
	for (int i = -1;
		 i <= setting.additional && (i < 0 || size_t(i) < children.size());
		 i++) {
		auto& child = children[i + 1];

		// set selection on child to initial value
		if (child) {
			child->setselection(initial[i + 1]);
		}
	}
}

void ConfigSetting_widget::setdefault() {
	for (int i = -1;
		 i <= setting.additional && (i < 0 || size_t(i) < children.size());
		 i++) {
		auto& child = children[i + 1];

		// set selection on child to default value
		int def = setting.choices.size();
		if (def == -1) {
			def = setting.find_choice(setting.default_value);
			if (child && def != -1) {
				child->setselection(def);
			}
		}
	}
}

void ConfigSetting_widget::save_to_config(bool write_file) {
	for (int i = -1;
		 i < setting.additional && (i < 0 || size_t(i) < children.size());
		 i++) {
		auto& child = children[i + 1];

		int sel = child->getselection();

		// do not update config for missing choice
		if (sel != missingchoice) {
			std::string config_key;
			if (i == -1) {
				config_key = setting.config_setting;
			} else {
				config_key = setting.config_setting + std::to_string(i);
			}
			config->set(config_key, setting.choices[sel].value, false);
		}
	}

	if (write_file) {
		config->write_back();
	}
}

TileRect ConfigSetting_widget::get_rect() const {
	TileRect rect{};

	local_to_screen(rect.x, rect.y);

	// Add all child rects
	for (auto& child : children) {
		if (child) {
			auto crect = child->get_rect();
			rect       = rect.add(crect);
		}
	}
	return rect;
}

void ConfigSetting_widget::onselection(Gump_widget* sender) {
	size_t index = 0;
	for (index = 0; index < children.size(); index++) {
		if (children[index].get() == sender) {
			break;
		}
	}
	// didn't find the sender
	if (index == children.size()) {
		return;
	}
	// decrement index
	index--;

	gwin->set_all_dirty();
}

std::string ConfigSetting_widget::Validate() {
	std::vector<bool> used;
	used.resize(setting.choices.size());
	bool have      = false;
	bool collision = false;

	for (int i = -1;
		 i < setting.additional && (i < 0 || size_t(i) < children.size());
		 i++) {
		auto& child = children[i + 1];

		int sel = child->getselection();
		if (sel != emptychoice) {
			have = true;

			// ignore missing choice whendetecting collisions
			if (sel != missingchoice) {
				collision = used[sel];
				used[sel] = true;
			}
		}
	}

	if (setting.unique && collision) {
		return "All values for " + setting.label + " setting must be unique. ";
	} else if (setting.required && !have) {
		return "At least One value for " + setting.label
			   + " setting must be set. ";
	}

	return std::string();
}

int ConfigSetting_widget::Definition::find_choice(
		std::string_view value, bool case_insensitive) const {
	auto found = std::find_if(
			choices.begin(), choices.end(),
			[value, case_insensitive](const Choice& choice) {
				if (case_insensitive) {
					if ((!Pentagram::strncasecmp(
								 choice.value.c_str(), value.data(),
								 value.size())
						 && value.size() == choice.value.size())
						|| (!choice.alternative.empty()
							&& !Pentagram::strncasecmp(
									choice.alternative.c_str(), value.data(),
									value.size())
							&& value.size() == choice.alternative.size())) {
						return true;
					}
				} else {
					if (choice.value == value || choice.alternative == value) {
						return true;
					}
				}
				return false;
			});

	if (found != choices.end()) {
		return found - choices.begin();
	}

	return -1;
}

void ConfigSetting_widget::Definition::sort_choices() {
	std::sort(
			choices.begin(), choices.end(),
			[](const Choice& left, const Choice& right) {
				return Pentagram::strcasecmp(
							   left.label.c_str(), right.label.c_str())
					   < 0;
			});
}

void ConfigSetting_widget::Definition::add_filenames_to_choices(
		const std::string& mask, bool strip_directory) {
	FileList files;
	U7ListFiles(mask, files, true);

	choices.reserve(choices.size() + files.size());
	for (std::string_view filename : files) {
		Choice choice{};

		// get filename without path
		if (strip_directory) {
			auto slash = filename.find_last_of("\\/");
			if (slash != std::string_view::npos) {
				filename = filename.substr(slash + 1);
			}
		}

		choice.label = choice.alternative = choice.value = filename;
		// Only add it if it doesn't already exist
		if (find_choice(filename, false) == -1) {
			choices.push_back(choice);
		}
	}
}
