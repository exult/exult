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

#ifndef CONFIGSETTING_WIDGET_H
#define CONFIGSETTING_WIDGET_H

#include "Gump_button.h"
#include "IterableGump_widget.h"

#include <map>
#include <string>
#include <variant>
#include <vector>

// A widget that represents a single config setting
// It will create the appropriate widget
// It will paint labels and zebra striping
//
class ConfigSetting_widget
		: public IterableGump_widget<
				  std::vector<std::unique_ptr<Gump_widget>>> {
public:
	class IValueWidget {
	public:
		int index;
	};

	template <typename T>
	class ValueWidget : public CallbackButtonBase<ConfigSetting_widget, T>,
						public IValueWidget {
		template <typename... Ts>
		ValueWidget(ConfigSetting_widget* par, Ts&&... args)
				: CallbackButtonBase<ConfigSetting_widget, T>(
						  par, std::forward<Ts>(args)...) {
			auto* button = dynamic_cast<Gump_button*>(this);
			if (button) {
				button->set_self_managed(true);
			}
		}
	};

	// This structure represents a setting supported by a midi driver
	struct Definition {
		std::string label;    // Human readable label for the setting
		// the config setting in exult.cfg
		// If additional > 0 this works as the base for for the extra config
		// keys setting[i] = config_setting + std::to_string(i)
		std::string config_setting;

		int  additional;    // No theoretical maximum to this
		bool required;    //  At least one setting must be set to a valid choice
		bool unique;      //  if max_count >1 then if this is set each value
						//  should be unique. Shows warning on apply

		enum SettingType {
			dropdown = 0,    // Setting should be displayed as a dropdown using
							 // DropDown_widget
			list   = 1,    // Setting should be displayed as a StringList_widget
			button = 2,    // Setting should be displayed as a
						   // Gump_ToggleTextButton
		} setting_type;

	private:
		friend class ConfigSetting_widget;

	public:
		struct Choice {
			std::string label;
			std::string value;    // Preferred Setting Value this is the value
								  // that will be saved to exult.cfg
			std::string alternative;    // Alternative Setting Value from
										// exult.cfg, will not be saved to
										// exult.cfg If there is no alternative,
										// set this to the same as value
		};

		// Valid choices for the setting, this must be filled with all the valid
		// settings Do not include the default or empty string value in this
		// unless the user is always allowed to select these Do not set these to
		// empty strings
		std::vector<Choice> choices;
		int                 find_choice(
								std::string_view value, bool case_insensitive = true) const;
		void sort_choices();

		void add_filenames_to_choices(
				const std::string& mask, bool strip_directory = true);

		// Default value for this setting.
		std::string default_value;

		Definition(
				std::string&& label, std::string&& config_key,
				unsigned additional, bool required, bool unique,
				SettingType type)
				: label(std::move(label)),
				  config_setting(std::move(config_key)), additional(additional),
				  required(required), unique(unique), setting_type(type) {}
	};

private:
	Definition setting;
	int        emptychoice;
	int        missingchoice;

	std::vector<std::unique_ptr<Gump_widget>> children;

	std::vector<int>      initial;
	std::shared_ptr<Font> font;
	static int            bgstripe;
	static const uint8    bgcolours[4];

public:
	ConfigSetting_widget() = delete;
	ConfigSetting_widget(
			Gump_Base* parent, int px, int py, int button_width,
			const Definition& setting, std::shared_ptr<Font> font,
			int line_gap);

	void shift_buttons_x(int offset);

	void paint() override;

	// Inherited via IterableGump_widget
	iterator begin() override {
		return children.begin();
	}

	iterator end() override {
		return children.end();
	}

	/// Revert the value to config setting when this was created
	void revert();

	/// Set the value to the default supplied in the definition
	void setdefault();

	// Save out to config
	void        save_to_config(bool write_file);
	std::string Validate();

	TileRect get_rect() const override;

	void onselection(Gump_widget* sender);

	void onselectionmb(Gump_widget* sender, MouseButton) {
		onselection(sender);
	}
};
#endif
