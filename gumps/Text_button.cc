/*
Copyright (C) 2001-2013 The Exult Team

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
#  include <config.h>
#endif

#include "Text_button.h"
#include "gamewin.h"
#include "font.h"
#include "iwin8.h"
#include "Gump.h"

#define TB_FONTNAME             "SMALL_BLACK_FONT"

// Palette Indices
#define TB_OUTER_BORDER         133
#define TB_OUTER_BORDER_CORNER      142
#define TB_OUTER_BORDER_PUSHED_TOP  144
#define TB_OUTER_BORDER_PUSHED_LEFT 140

#define TB_INNER_BORDER_HIGHLIGHT   138
#define TB_INNER_BORDER_LOWLIGHT    142
#define TB_INNER_BORDER_CORNER      141
#define TB_INNER_BORDER_TR_HIGH     137
#define TB_INNER_BORDER_TR_CORNER   138
#define TB_INNER_BORDER_BL_CORNER   144

#define TB_BACKGROUND           140
#define TB_RT_HIGHLIGHT         139

Text_button::Text_button(Gump *p, const std::string &str, int x, int y, int w, int h)
	: Gump_button(p, 0, x, y, SF_OTHER), text(str), width(w), height(h) {
	init();
}

void Text_button::init() {
	// Must be at least 11 units high
	if (height < 11) height = 11;

	// Text y is based on gump height of 11
	text_y = 2 + (height - 11) / 2;

	// We will get the text width
	int text_width = fontManager.get_font("SMALL_BLACK_FONT")->get_text_width(
	                     text.c_str());

	if (width < text_width + 4) width = text_width + 4;

	// We want to find the starting point for the text (horizontal)
	text_x = (width - text_width) >> 1;
}

void Text_button::paint() {
	Image_window8 *iwin = gwin->get_win();

	int offset = 0;
	int px = x;
	int py = y;

	if (parent) {
		px += parent->get_x();
		py += parent->get_y();
	}

	// The the push dependant edges
	if (is_pushed()) {
		// Top left corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px, py);
		// Bottom left corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px, py + height - 1);
		// Top right corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px + width - 1, py);
		// Top edge
		iwin->fill8(TB_OUTER_BORDER_PUSHED_TOP, width - 2, 1, px + 1, py);
		// Left edge
		iwin->fill8(TB_OUTER_BORDER_PUSHED_TOP, 1, height - 2, px, py + 1);

		offset = 1;
	} else {
		// Bottom right corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px + width - 1, py + height - 1);
		// Bottom left corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px, py + height - 1);
		// Top right corner
		iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px + width - 1, py + height - 1);
		// Bottom edge
		iwin->fill8(TB_OUTER_BORDER, width - 2, 1, px + 1, py + height - 1);
		// Right edge
		iwin->fill8(TB_OUTER_BORDER, 1, height - 2, px + width - 1, py + 1);
	}

	// 'Outer' Top and Left Edges

	// Top left corner
	iwin->fill8(TB_OUTER_BORDER_CORNER, 1, 1, px + offset, py + offset);
	// Top edge
	iwin->fill8(TB_OUTER_BORDER, width - 2, 1, px + 1 + offset, py + offset);
	// Left edge
	iwin->fill8(TB_OUTER_BORDER, 1, height - 2, px + offset, py + 1 + offset);

	// 'Inner' Edges

	// Top left corner
	iwin->fill8(TB_INNER_BORDER_CORNER, 1, 1, px + offset + 1, py + offset + 1);
	// Top Right corner
	iwin->fill8(TB_INNER_BORDER_TR_CORNER, 1, 1, px + width + offset - 2, py + offset + 1);
	// Top Right Highlight 1
	iwin->fill8(TB_INNER_BORDER_TR_HIGH, 1, 1, px + width + offset - 3, py + offset + 1);
	// Top Right Highlight 1
	iwin->fill8(TB_INNER_BORDER_TR_HIGH, 1, 1, px + width + offset - 2, py + offset + 2);
	// Bottom left corner
	iwin->fill8(TB_INNER_BORDER_BL_CORNER, 1, 1, px + offset + 1, py + height + offset - 2);

	// Top edge
	iwin->fill8(TB_INNER_BORDER_HIGHLIGHT, width - 5, 1, px + 2 + offset, py + offset + 1);
	// Left edge
	iwin->fill8(TB_INNER_BORDER_LOWLIGHT, 1, height - 4, px + offset + 1, py + 2 + offset);
	// Right edge
	iwin->fill8(TB_INNER_BORDER_HIGHLIGHT, 1, height - 5, px + width + offset - 2, py + 3 + offset);
	// Bottom edge
	iwin->fill8(TB_INNER_BORDER_LOWLIGHT, width - 4, 1, px + 2 + offset, py + height + offset - 2);

	// Background Fill
	iwin->fill8(TB_BACKGROUND, width - 4, height - 4, px + 2 + offset, py + 2 + offset);
	// Top Right Highligh on Background
	iwin->fill8(TB_RT_HIGHLIGHT, 1, 1, px + width + offset - 3, py + offset + 2);

	fontManager.get_font(TB_FONTNAME)->paint_text(
	    iwin->get_ib8(), text.c_str(), px + text_x + offset, py + text_y + offset);
}

bool Text_button::on_widget(int mx, int my) const {
	int px = x;
	int py = y;

	if (parent) {
		px += parent->get_x();
		py += parent->get_y();
	}

	if (mx < px || mx >= px + width) return false;
	if (my < py || my >= py + height) return false;
	return true;
}
