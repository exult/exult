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

#ifndef STRINGLIST_WIDGET_H
#define STRINGLIST_WIDGET_H

#include "Gump_widget.h"
#include "Modal_gump.h"

#include <string_view>

// A widget that displays a vertical list of strings for user selection
// Uses similar interface as Gump_ToggleTextButton can be used with
// CallbackButtonBase
class StringList_widget : public Gump_widget {
	std::shared_ptr<Font>         font;
	std::vector<std::string>      selections;
	int                           width;
	int                           height;
	int                           lineheight;
	Modal_gump::ProceduralColours colours;

public:
	StringList_widget(
			Gump_Base* par, const std::vector<std::string>& s, int selectionnum,
			int px, int py, Modal_gump::ProceduralColours colours,
			int width = 0, int height = 0, std::shared_ptr<Font> font = {});

	void paint() override;

	bool mouse_down(int mx, int my, MouseButton button) override;

	// Signal selection changed for CallbackButtonBase
	virtual bool activate(MouseButton) {
		return true;
	}

	int getselection() const override {
		return get_framenum();
	}

	int get_line_height() const {
		return lineheight;
	}

	// Set a new selection. This does not bounds check and will accept any value
	void setselection(int newsel) override;

	/// Set the selection to the input string
	/// optionally adding it if it doesn't already exist
	/// \returns the index of the selection;
	int setselection(std::string_view str, bool add = false) {
		auto found  = std::find(selections.begin(), selections.end(), str);
		int  newsel = -1;
		if (found != selections.end()) {
			newsel = found - selections.begin();
		}
		// add it
		if (add && newsel == -1) {
			newsel = selections.size();
			selections.push_back(std::string(str));
		}
		if (newsel != -1) {
			setselection(newsel);
		}

		return newsel;
	}

	bool on_widget(
			int mx, int my    // Point in window.
	) const override;

	TileRect get_rect() const override;
};
#endif