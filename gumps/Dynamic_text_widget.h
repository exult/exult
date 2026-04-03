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

#ifndef DYNAMIC_TEXT_WIDGET_H
#define DYNAMIC_TEXT_WIDGET_H

#include "Gump_widget.h"

#include <string>

struct Dynamic_text_def;

/*
 *  A data-driven text field on a gump, configured via gump_info.txt.
 *  Text content is set at runtime via usecode intrinsic UI_set_gump_text.
 */
class Dynamic_text_widget : public Gump_widget {
	int         field_id;    // Unique ID for usecode to address this field
	int         font_num;    // Font number from fonts.vga
	int         halign;      // 0=left, 1=right, 2=center
	int         valign;      // 0=top, 1=bottom, 2=center
	std::string text;        // Current displayed text (per-instance)

	// Clamp alignment value to valid range [0..2], defaulting to 0.
	static int clamp_align(int v) {
		return (v >= 0 && v <= 2) ? v : 0;
	}

public:
	Dynamic_text_widget(Gump* par, const Dynamic_text_def& def);

	Gump_widget* clone(Gump* par) override;

	void paint() override;

	int get_field_id() const {
		return field_id;
	}

	const std::string& get_text() const {
		return text;
	}

	void set_text(const std::string& t) {
		text = t;
	}

	void set_font(int f) {
		font_num = f;
	}
};

#endif
