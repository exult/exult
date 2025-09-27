/*
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef FONT_H
#define FONT_H

#include "hash_utils.h"

#include <memory>

class Image_buffer8;
class Shape_file;
class IDataSource;
struct File_spec;
class U7multiobject;

/*
 *  Cursor info. filled in by paint_text_box.
 */
struct Cursor_info {
	int offset;    // Loc. within text.
	int x, y;      // Loc. of top-left of cursor in win.
	int line;      // Line # of cursor.
	int nlines;    // Total # lines printed.

	void set_found(int cx, int cy, int l) {
		x    = cx;
		y    = cy;
		line = l;
	}
};

/*
 *  A single font:
 */
class Font {
private:
	int                         hor_lead = 0;
	int                         ver_lead = 0;
	std::unique_ptr<Shape_file> font_shapes;
	int                         highest = 0, lowest = 0;

	void calc_highlow();
	void clean_up();
	int  load_internal(IDataSource& data, int hlead, int vlead);

public:
	Font();
	Font(const File_spec& fname0, int index, int hlead = 0, int vlead = 1);
	Font(const File_spec& fname0, const File_spec& fname1, int index,
		 int hlead = 0, int vlead = 1);
	Font(Font&&) noexcept            = default;
	Font& operator=(Font&&) noexcept = default;
	~Font() noexcept                 = default;
	/**
	 *  Loads a font from a File_spec.
	 *  @param fname0   First file spec.
	 *  @param index    Number of font to load.
	 *  @param hleah    Horizontal lead of the font.
	 *  @param vleah    Vertical lead of the font.
	 *  @return 0 on success
	 */
	int load(const File_spec& fname0, int index, int hlead = 0, int vlead = 1);
	/**
	 *  Loads a font from a File_spec.
	 *  @param fname0   First file spec.
	 *  @param fname1   Second file spec.
	 *  @param index    Number of font to load.
	 *  @param hleah    Horizontal lead of the font.
	 *  @param vleah    Vertical lead of the font.
	 *  @return 0 on success
	 */
	int load(
			const File_spec& fname0, const File_spec& fname1, int index,
			int hlead = 0, int vlead = 1);
	// Text rendering:
	int paint_text_box(
			Image_buffer8* win, const char* text, int x, int y, int w, int h,
			int vert_lead = 0, bool pbreak = false, bool center = false,
			Cursor_info* cursor = nullptr, unsigned char* trans = nullptr);
	int paint_text(
			Image_buffer8* win, const char* text, int xoff, int yoff,
			unsigned char* trans = nullptr);

	int paint_text_right_aligned(
			Image_buffer8* win, const char* text, int xoff, int yoff,
			unsigned char* trans = nullptr) {
		return paint_text(win, text, xoff - get_text_width(text), yoff, trans);
	}

	int paint_text(
			Image_buffer8* win, const char* text, int textlen, int xoff,
			int yoff, unsigned char* trans = nullptr);
	int paint_text_box_fixedwidth(
			Image_buffer8* win, const char* text, int x, int y, int w, int h,
			int char_width, int vert_lead = 0, int pbreak = 0,
			unsigned char* trans = nullptr);
	int paint_text_fixedwidth(
			Image_buffer8* win, const char* text, int xoff, int yoff, int width,
			unsigned char* trans = nullptr);
	int paint_text_fixedwidth(
			Image_buffer8* win, const char* text, int textlen, int xoff,
			int yoff, int width, unsigned char* trans = nullptr);
	// Get text width.
	int get_text_width(const char* text);
	int get_text_width(const char* text, int textlen);
	// Get dimensions of text box for multiline string
	void get_text_box_dims(
			const char* text, int& width, int& height, int vert_lead = 0);
	// Get text height, baseline.
	int get_text_height();
	int get_text_baseline();
	int find_cursor(
			const char* text, int x, int y, int w, int h, int cx, int cy,
			int vert_lead);
	int find_xcursor(const char* text, int textlen, int cx);

	int draw_text(
			Image_buffer8* win, int x, int y, const char* s,
			unsigned char* trans = nullptr) {
		return paint_text(win, s, x, y, trans);
	}

	int draw_text_box(
			Image_buffer8* win, int x, int y, int w, int h, const char* s,
			unsigned char* trans = nullptr) {
		return paint_text_box(
				win, s, x, y, w, h, 0, false, false, nullptr, trans);
	}

	int center_text(
			Image_buffer8* iwin, int x, int y, const char* s,
			unsigned char* trans = nullptr);
};

/*
 *  Manage a list of fonts by name.
 */
class FontManager {
private:
	std::unordered_map<const char*, std::shared_ptr<Font>, hashstr, eqstr>
			fonts;

public:
	~FontManager();
	void add_font(
			const char* name, const File_spec& fname0, int index, int hlead = 0,
			int vlead = 1);
	void add_font(
			const char* name, const File_spec& fname0, const File_spec& fname1,
			int index, int hlead = 0, int vlead = 1);
	void                  remove_font(const char* name);
	std::shared_ptr<Font> get_font(const char* name);

	void reset();
};

extern FontManager fontManager;

#endif
