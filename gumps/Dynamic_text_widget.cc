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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Dynamic_text_widget.h"

#include "Gump.h"
#include "font.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "shapeid.h"

#include <iostream>

Dynamic_text_widget::Dynamic_text_widget(Gump* par, const Dynamic_text_def& def)
		: Gump_widget(par, -1, def.pos_x, def.pos_y, 0, SF_GUMPS_VGA), field_id(def.field_id), font_num(def.font_num),
		  halign(clamp_align(def.halign)), valign(clamp_align(def.valign)) {}

Gump_widget* Dynamic_text_widget::clone(Gump* par) {
	Dynamic_text_def def;
	def.field_id = field_id;
	def.pos_x    = x;
	def.pos_y    = y;
	def.font_num = font_num;
	def.halign   = halign;
	def.valign   = valign;
	auto* w      = new Dynamic_text_widget(par, def);
	w->text      = text;
	return w;
}

void Dynamic_text_widget::paint() {
	if (text.empty()) {
		return;
	}
	int sx = x;
	int sy = y;
	// Convert to screen coords
	if (parent) {
		parent->local_to_screen(sx, sy);
		if (parent->get_debug_flags() & GUMP_DEBUG_CONSOLE) {
			std::cerr << "[DynText] field_id=" << field_id << " local=(" << x << "," << y << ")"
					  << " screen=(" << sx << "," << sy << ")"
					  << " halign=" << halign << " valign=" << valign << " text=\"" << text << "\"" << std::endl
					  << std::flush;
		}
	}

	auto* sman = Shape_manager::get_instance();
	auto  font = sman->get_font(font_num);
	if (!font) {
		return;
	}

	// Vertical alignment adjustments
	if (valign != 0) {
		const int th = font->get_text_height();
		if (valign == 1) {    // bottom: pos_y is bottom edge
			sy -= th;
		} else if (valign == 2) {    // center
			sy -= th / 2;
		}
	}

	// Horizontal alignment
	if (halign == 1) {
		// Right-aligned: pos_x is right edge
		auto* iwin = Game_window::get_instance()->get_win()->get_ib8();
		font->paint_text_right_aligned(iwin, text.c_str(), sx, sy);
	} else if (halign == 2) {
		// Centered: pos_x is center point
		auto* iwin = Game_window::get_instance()->get_win()->get_ib8();
		font->center_text(iwin, sx, sy, text.c_str());
	} else {
		// Left-aligned (default): pos_x is left edge
		sman->paint_text(font_num, text.c_str(), sx, sy);
	}
}
