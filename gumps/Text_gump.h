/*
Copyright (C) 2000-2025 The Exult Team

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

#ifndef TEXT_GUMP_H
#define TEXT_GUMP_H

#include "Gump.h"

/*
 *  A text gump is the base class for books and scrolls.
 */
class Text_gump : public Gump {
protected:
	char* text;         // The text.
	int   textlen;      // Length of text.
	int   curtop;       // Offset of top of current page.
	int   curend;       // Offset past end of current page(s).
	int   font;         // The shape in fonts.vga to use
	bool  from_help;    // Text is from help system

public:
	Text_gump(int shapenum, int fnt = 4)
			: Gump(nullptr, shapenum), text(nullptr), textlen(0), curtop(0),
			  curend(0), font(fnt), from_help(false) {}

	~Text_gump() override {
		delete[] text;
	}

	void set_from_help(bool val) {
		from_help = val;
	}

	void add_text(const char* str);    // Append text.
	int  paint_page(const TileRect& box, int start);
	// Next page of book/scroll.
	int show_next_page();
};

#endif
