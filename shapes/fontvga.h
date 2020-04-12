/**
 ** Fontvga.h - Handle the 'fonts.vga' file and text rendering.
 **
 ** Written: 4/29/99 - JSF
 **/

#ifndef INCL_FONTVGA
#define INCL_FONTVGA    1

/*
Copyright (C) 1998  Jeffrey S. Freedman

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

#include "font.h"
#include "vgafile.h"
#include <vector>

/*
 *  The "fonts.vga" file:
 */
class Fonts_vga_file : public Vga_file {
	std::vector<Font> fonts;            // Fonts from fonts.vga file.
public:
	void init();
	// Text rendering:
	int paint_text_box(Image_buffer8 *win, int fontnum,
	                   const char *text, int x, int y, int w, int h,
	                   int vert_lead = 0, bool pbreak = false, bool center = false,
	                   Cursor_info *cursor = nullptr) {
		return fonts[fontnum].paint_text_box(win, text, x, y, w, h,
		                                     vert_lead, pbreak, center, cursor);
	}
	int paint_text(Image_buffer8 *win, int fontnum,
	               const char *text, int xoff, int yoff) {
		return fonts[fontnum].paint_text(win, text, xoff, yoff);
	}
	int paint_text(Image_buffer8 *win, int fontnum,
	               const char *text, int textlen, int xoff, int yoff) {
		return fonts[fontnum].paint_text(win, text, textlen,
		                                 xoff, yoff);
	}
	// Get text width.
	int get_text_width(int fontnum, const char *text) {
		return fonts[fontnum].get_text_width(text);
	}
	int get_text_width(int fontnum, const char *text, int textlen) {
		return fonts[fontnum].get_text_width(text, textlen);
	}
	// Get text height, baseline.
	int get_text_height(int fontnum) {
		return fonts[fontnum].get_text_height();
	}
	int get_text_baseline(int fontnum) {
		return fonts[fontnum].get_text_baseline();
	}

	int find_cursor(int fontnum, const char *text, int x, int y,
	                int w, int h, int cx, int cy, int vert_lead) {
		return fonts[fontnum].find_cursor(text, x, y, w, h,
		                                  cx, cy, vert_lead);
	}

	Font *get_font(int fontnum) {
		return (fontnum >= 0 &&
		        static_cast<unsigned int>(fontnum) < fonts.size()) ?
		       &(fonts[fontnum]) : nullptr;
	}
};

#endif
