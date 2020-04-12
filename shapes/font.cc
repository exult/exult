/*
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "U7file.h"
#include "databuf.h"
#include "font.h"
#include "ibuf8.h"
#include "vgafile.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"

using std::size_t;
using std::string;
using std::strncmp;
using std::toupper;
using std::unordered_map;

FontManager fontManager;

//	Want a more restrictive test for space.
inline bool Is_space(char c) {
	return c == ' ' || c == '\n' || c == '\t';
}

/*
 *  Pass space.
 */

static const char *Pass_whitespace(
    const char *text
) {
	while (Is_space(*text))
		text++;
	return text;
}
// Just spaces and tabs:
static const char *Pass_space(
    const char *text
) {
	while (*text == ' ' || *text == '\t')
		text++;
	return text;
}

/*
 *  Pass a word.
 */

static const char *Pass_word(
    const char *text
) {
	while (*text && (*text != '^') && (!Is_space(*text) || (*text == '\f') || (*text == '\v')))
		text++;
	return text;
}

/*
 *  Draw text within a rectangular area.
 *  Special characters handled are:
 *      \n  New line.
 *      space   Word break.
 *      tab Treated like a space for now.
 *      *   Page break.
 *      ^   Uppercase next letter.
 *
 *  Output: If out of room, -offset of end of text painted.
 *      Else height of text painted.
 */

int Font::paint_text_box(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,
    int x, int y,           // Top-left corner of box.
    int w, int h,           // Dimensions.
    int vert_lead,          // Extra spacing between lines.
    bool pbreak,            // End at punctuation.
    bool center,            // Center each line.
    Cursor_info *cursor     // We set x, y if not nullptr.
) {
	const char *start = text;   // Remember the start.
	win->set_clip(x, y, w, h);
	int endx = x + w;       // Figure where to stop.
	int curx = x;
	int cury = y;
	int height = get_text_height() + vert_lead + ver_lead;
	int space_width = get_text_width(" ", 1);
	int max_lines = h / height; // # lines that can be shown.
	string *lines = new string[max_lines + 1];
	int cur_line = 0;
	const char *last_punct_end = nullptr;// ->last period, qmark, etc.
	// Last punct in 'lines':
	int last_punct_line = -1;
	int last_punct_offset = -1;
	int coff = -1;

	if (cursor) {
		coff = cursor->offset;
		cursor->x = -1;
	}
	while (*text) {
		if (text - start == coff)
			cursor->set_found(curx, cury, cur_line);
		switch (*text) {    // Special cases.
		case '\n':      // Next line.
			curx = x;
			text++;
			cur_line++;
			cury += height;
			if (cur_line >= max_lines)
				break;  // No more room.
			continue;
		case '\r':      //??
			text++;
			continue;
		case ' ':       // Space.
		case '\t': {
			// Pass space.
			const char *wrd = Pass_space(text);
			if (wrd != text) {
				int w = get_text_width(text, static_cast<uint32>(wrd - text));
				if (w <= 0)
					w = space_width;
				int nsp = w / space_width;
				lines[cur_line].append(nsp, ' ');
				if (coff > text - start &&
				        coff < wrd - start)
					cursor->set_found(
					    curx +
					    static_cast<uint32>(coff - (text - start))*space_width,
					    cury, cur_line);
				curx += nsp * space_width;
			}
			text = wrd;
			break;
		}
		}
		if (cur_line >= max_lines)
			break;

		if (*text == '*') {
			text++;
			if (cur_line)
				break;
		}
		bool ucase_next = *text == '^';
		if (ucase_next) // Skip it.
			text++;
		// Pass word & get its width.
		const char *ewrd = Pass_word(text);
		int width;
		if (ucase_next) {
			const char c = static_cast<char>(toupper(static_cast<unsigned char>(*text)));
			width = get_text_width(&c, 1u)
			        + get_text_width(text + 1, static_cast<uint32>(ewrd - text - 1));
		} else
			width = get_text_width(text, static_cast<uint32>(ewrd - text));
		if (curx + width - hor_lead > endx) {
			// Word-wrap.
			if (ucase_next)
				text--; // Put the '^' back.
			curx = x;
			cur_line++;
			cury += height;
			if (cur_line >= max_lines)
				break;  // No more room.
		}
		if (coff >= text - start && coff < ewrd - start)
			cursor->set_found(curx + get_text_width(text,
			                                        static_cast<uint32>(coff - (text - start))),
			                  cury, cur_line);
		// Store word.
		if (ucase_next) {
			lines[cur_line].push_back(static_cast<char>(toupper(static_cast<unsigned char>(*text))));
			++text;
		}
		lines[cur_line].append(text, ewrd - text);
		curx += width;
		text = ewrd;        // Continue past the word.
		// Keep loc. of punct. endings.
		if (text[-1] == '.' || text[-1] == '?' || text[-1] == '!' ||
		        text[-1] == ',' || text[-1] == '"') {
			last_punct_end = text;
			last_punct_line = cur_line;
			last_punct_offset = static_cast<int>(lines[cur_line].length());
		}
	}
	if (*text &&            // Out of room?
	        // Break off at end of punct.
	        pbreak && last_punct_end)
		text = Pass_whitespace(last_punct_end);
	else {
		last_punct_line = -1;
		if (text - start == coff && // Cursor at very end?
		        cur_line < max_lines)
			cursor->set_found(curx, cury, cur_line);
	}
	if (cursor)
		cursor->nlines = cur_line + (cur_line < max_lines);
	cury = y;           // Render text.
	for (int i = 0; i <= cur_line; i++) {
		const char *str = lines[i].c_str();
		int len = static_cast<int>(lines[i].length());
		if (i == last_punct_line)
			len = last_punct_offset;
		if (center)
			center_text(win, x + w / 2, cury, str);
		else
			paint_text(win, str, len, x, cury);
		cury += height;
		if (i == last_punct_line)
			break;
	}
	win->clear_clip();
	delete [] lines;
	if (*text)          // Out of room?
		return -static_cast<int>(text - start); // Return -offset of end.
	else                // Else return height.
		return cury - y;
}

/*
 *  Draw text at a given location (which is the upper-left corner of the
 *  place to draw.
 *
 *  Output: Width in pixels of what was drawn.
 */

int Font::paint_text(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,       // What to draw, 0-delimited.
    int xoff, int yoff,     // Upper-left corner of where to start.
    unsigned char *trans
) {
	ignore_unused_variable_warning(win);
	int x = xoff;
	int chr;
	yoff += get_text_baseline();
	if (font_shapes)
		while ((chr = *text++) != 0) {
			Shape_frame *shape = font_shapes->get_frame(static_cast<unsigned char>(chr));
			if (!shape)
				continue;
			if (trans)
				shape->paint_rle_remapped(x, yoff, trans);
			else
				shape->paint_rle(x, yoff);
			x += shape->get_width() + hor_lead;
		}
	return x - xoff;
}

/*
 *  Paint text using font from "fonts.vga".
 *
 *  Output: Width in pixels of what was painted.
 */

int Font::paint_text(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,       // What to draw.
    int textlen,            // Length of text.
    int xoff, int yoff      // Upper-left corner of where to start.
) {
	ignore_unused_variable_warning(win);
	int x = xoff;
	yoff += get_text_baseline();
	if (font_shapes)
		while (textlen--) {
			Shape_frame *shape = font_shapes->get_frame(static_cast<unsigned char>(*text++));
			if (!shape)
				continue;
			shape->paint_rle(x, yoff);
			x += shape->get_width() + hor_lead;
		}
	return x - xoff;
}

/*
 *
 *  FIXED WIDTH RENDERING
 *
 */

/*
 *  Draw text within a rectangular area.
 *  Special characters handled are:
 *      \n  New line.
 *      space   Word break.
 *      tab Treated like a space for now.
 *      *   Page break.
 *      ^   Uppercase next letter.
 *
 *  Output: If out of room, -offset of end of text painted.
 *      Else height of text painted.
 */

int Font::paint_text_box_fixedwidth(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,
    int x, int y,           // Top-left corner of box.
    int w, int h,           // Dimensions.
    int char_width,         // Width of each character
    int vert_lead,          // Extra spacing between lines.
    int pbreak          // End at punctuation.
) {
	const char *start = text;   // Remember the start.
	win->set_clip(x, y, w, h);
	int endx = x + w;       // Figure where to stop.
	int curx = x;
	int cury = y;
	int height = get_text_height() + vert_lead + ver_lead;
	int max_lines = h / height; // # lines that can be shown.
	string *lines = new string[max_lines + 1];
	int cur_line = 0;
	const char *last_punct_end = nullptr;// ->last period, qmark, etc.
	// Last punct in 'lines':
	int last_punct_line = -1;
	int last_punct_offset = -1;

	while (*text) {
		switch (*text) {    // Special cases.
		case '\n':      // Next line.
			curx = x;
			text++;
			cur_line++;
			if (cur_line >= max_lines)
				break;  // No more room.
			continue;
		case ' ':       // Space.
		case '\t': {
			// Pass space.
			const char *wrd = Pass_space(text);
			if (wrd != text) {
				int w = static_cast<int>(wrd - text) * char_width;
				if (!w)
					w = char_width;
				int nsp = w / char_width;
				lines[cur_line].append(nsp, ' ');
				curx += nsp * char_width;
			}
			text = wrd;
			break;
		}
		}

		if (cur_line >= max_lines)
			break;

		if (*text == '*') {
			text++;
			if (cur_line)
				break;
		}
		bool ucase_next = *text == '^';
		if (ucase_next) // Skip it.
			text++;
		// Pass word & get its width.
		const char *ewrd = Pass_word(text);
		int width = static_cast<int>(ewrd - text) * char_width;
		if (curx + width - hor_lead > endx) {
			// Word-wrap.
			if (ucase_next)
				text--; // Put the '^' back.
			curx = x;
			cur_line++;
			if (cur_line >= max_lines)
				break;  // No more room.
		}

		// Store word.
		if (ucase_next) {
			lines[cur_line].push_back(static_cast<char>(toupper(static_cast<unsigned char>(*text))));
			++text;
		}
		lines[cur_line].append(text, ewrd - text);
		curx += width;
		text = ewrd;        // Continue past the word.
		// Keep loc. of punct. endings.
		if (text[-1] == '.' || text[-1] == '?' || text[-1] == '!' ||
		        text[-1] == ',' || text[-1] == '"') {
			last_punct_end = text;
			last_punct_line = cur_line;
			last_punct_offset = static_cast<int>(lines[cur_line].length());
		}
	}
	if (*text &&            // Out of room?
	        // Break off at end of punct.
	        pbreak && last_punct_end)
		text = Pass_whitespace(last_punct_end);
	else
		last_punct_line = -1;
	// Render text.
	for (int i = 0; i <= cur_line; i++) {
		const char *str = lines[i].data();
		int len = static_cast<int>(lines[i].length());
		if (i == last_punct_line)
			len = last_punct_offset;
		paint_text_fixedwidth(win, str, len, x, cury, char_width);
		cury += height;
		if (i == last_punct_line)
			break;
	}
	win->clear_clip();
	delete [] lines;
	if (*text)          // Out of room?
		return -static_cast<int>(text - start); // Return -offset of end.
	else                // Else return height.
		return cury - y;
}

/*
 *  Draw text at a given location (which is the upper-left corner of the
 *  place to draw. Text will be drawn with the fixed width specified.
 *
 *  Output: Width in pixels of what was drawn.
 */

int Font::paint_text_fixedwidth(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,       // What to draw, 0-delimited.
    int xoff, int yoff,     // Upper-left corner of where to start.
    int width           // Width of each character
) {
	ignore_unused_variable_warning(win);
	int x = xoff;
	int w;
	int chr;
	yoff += get_text_baseline();
	while ((chr = *text++) != 0) {
		Shape_frame *shape = font_shapes->get_frame(static_cast<unsigned char>(chr));
		if (!shape)
			continue;
		x += w = (width - shape->get_width()) / 2;
		shape->paint_rle(x, yoff);
		x += width - w;
	}
	return x - xoff;
}

/*
 *  Draw text at a given location (which is the upper-left corner of the
 *  place to draw. Text will be drawn with the fixed width specified.
 *
 *  Output: Width in pixels of what was drawn.
 */

int Font::paint_text_fixedwidth(
    Image_buffer8 *win,     // Buffer to paint in.
    const char *text,       // What to draw.
    int textlen,            // Length of text.
    int xoff, int yoff,     // Upper-left corner of where to start.
    int width           // Width of each character
) {
	ignore_unused_variable_warning(win);
	int w;
	int x = xoff;
	yoff += get_text_baseline();
	while (textlen--) {
		Shape_frame *shape = font_shapes->get_frame(static_cast<unsigned char>(*text++));
		if (!shape)
			continue;
		x += w = (width - shape->get_width()) / 2;
		shape->paint_rle(x, yoff);
		x += width - w;
	}
	return x - xoff;
}

/*
 *  Get the width in pixels of a 0-delimited string.
 */

int Font::get_text_width(
    const char *text
) {
	int width = 0;
	short chr;
	if (font_shapes)
		while ((chr = *text++) != 0) {
			Shape_frame *shape = font_shapes->get_frame(static_cast<unsigned char>(chr));
			if (shape)
				width += shape->get_width() + hor_lead;
		}
	return width;
}

/*
 *  Get the width in pixels of a string given by length.
 */

int Font::get_text_width(
    const char *text,
    int textlen         // Length of text.
) {
	int width = 0;
	if (font_shapes)
		while (textlen--) {
			Shape_frame *shape = font_shapes->get_frame(
			                         static_cast<unsigned char>(*text++));
			if (shape)
				width += shape->get_width() + hor_lead;
		}
	return width;
}

/*
 *  Get font line-height.
 */

int Font::get_text_height(
) {
	// Note, I wont assume the fonts exist
	//Shape_frame *A = font_shapes->get_frame('A');
	//Shape_frame *y = font_shapes->get_frame('y');
	return highest + lowest + 1;
}

/*
 *  Get font baseline as the distance from the top.
 */

int Font::get_text_baseline(
) {
	//Shape_frame *A = font_shapes->get_frame('A');
	return highest;
}

/*
 *  Find cursor location within text, given (x,y) screen coords.
 *  Note:  This has to match paint_text_box().
 *
 *  Output: Offset in text if found.
 *      If not found, -offset of end of text there was room to paint.
 */

int Font::find_cursor(
    const char *text,
    int x, int y,           // Top-left corner of box.
    int w, int h,           // Dimensions.
    int cx, int cy,         // Mouse loc. to find.
    int vert_lead           // Extra spacing between lines.
) {
	const char *start = text;   // Remember the start.
	int endx = x + w;       // Figure where to stop.
	int curx = x;
	int cury = y;
	int height = get_text_height() + vert_lead + ver_lead;
	int space_width = get_text_width(" ", 1);
	int max_lines = h / height; // # lines that can be shown.
	int cur_line = 0;
	int chr;

	while ((chr = *text) != 0) {
		switch (chr) {      // Special cases.
		case '\n':      // Next line.
			if (cy >= cury && cy < cury + height &&
			        cx >= curx && cx < x + w)
				return static_cast<int>(text - start);
			++text;
			curx = x;
			cur_line++;
			cury += height;
			if (cur_line >= max_lines)
				break;  // No more room.
			continue;
		case '\r':      //??
			++text;
			continue;
		case ' ':       // Space.
		case '\t':
			if (cy >= cury && cy < cury + height &&
			        cx >= curx && cx < curx + space_width)
				return static_cast<int>(text - start);
			++text;
			curx += space_width;
			continue;
		}
		if (cur_line >= max_lines)
			break;

		if (*text == '*') {
			text++;
			if (cur_line)
				break;
		}
		bool ucase_next = *text == '^';
		if (ucase_next) // Skip it.
			text++;
		// Pass word & get its width.
		const char *ewrd = Pass_word(text);
		int width;
		if (ucase_next) {
			const char c = static_cast<char>(toupper(static_cast<unsigned char>(*text)));
			width = get_text_width(&c, 1u)
			        + get_text_width(text + 1, static_cast<uint32>(ewrd - text - 1));
		} else
			width = get_text_width(text, static_cast<uint32>(ewrd - text));
		if (curx + width - hor_lead > endx) {
			// Word-wrap.
			// Past end of this line?
			if (cy >= cury && cy < cury + height &&
			        cx >= curx && cx < x + w)
				return static_cast<int>(text - start - 1);
			curx = x;
			cur_line++;
			cury += height;
			if (cur_line >= max_lines)
				break;  // No more room.
		}
		if (cy >= cury && cy < cury + height &&
		        cx >= curx && cx < curx + width) {
			int woff = find_xcursor(text, static_cast<int>(ewrd - text), cx - curx);
			if (woff >= 0)
				return static_cast<int>(text - start) + woff;
		}
		curx += width;
		text = ewrd;        // Continue past the word.
	}
	if (cy >= cury && cy < cury + height &&     // End of last line?
	        cx >= curx && cx < x + w)
		return static_cast<int>(text - start);
	return -static_cast<int>(text - start);     // Failed, so indicate where we are.
}

/*
 *  Find an x-coord. within a piece of text.
 *
 *  Output: Offset if found, else -1.
 */

int Font::find_xcursor(
    const char *text,
    int textlen,            // Length of text.
    int cx              // Loc. to find.
) {
	const char *start = text;
	int curx = 0;
	while (textlen--) {
		Shape_frame *shape = font_shapes->get_frame(
		                         static_cast<unsigned char>(*text++));
		if (shape) {
			int w = shape->get_width() + hor_lead;
			if (cx >= curx && cx < curx + w)
				return static_cast<int>(text - 1 - start);
			curx += w;
		}
	}
	return -1;
}

Font::Font() = default;

Font::Font(
    const File_spec &fname0,
    int index,
    int hlead,
    int vlead
) {
	load(fname0, index, hlead, vlead);
}

Font::Font(
    const File_spec &fname0,
    const File_spec &fname1,
    int index,
    int hlead,
    int vlead
) {
	load(fname0, fname1, index, hlead, vlead);
}

void Font::clean_up() {
	font_shapes.reset();
}

/**
 *  Loads a font from a multiobject.
 *  @param font_obj Where we are loading from.
 *  @param hleah    Horizontal lead of the font.
 *  @param vleah    Vertical lead of the font.
 */
int Font::load_internal(
    IDataSource& data,
    int hlead,
    int vlead
) {
	if (!data.good()) {
		font_shapes.reset();
		hor_lead = 0;
		ver_lead = 0;
	} else {
		// Is it an IFF archive?
		char hdr[5] = {0};
		data.read(hdr, 4);
		data.seek(0);
		if (!strncmp(hdr, "font", 4))
			data.skip(8);      // Yes, skip first 8 bytes.
		font_shapes = std::make_unique<Shape_file>(&data);
		hor_lead = hlead;
		ver_lead = vlead;
		calc_highlow();
	}
	return 0;
}

/**
 *  Loads a font from a File_spec.
 *  @param fname0   First file spec.
 *  @param index    Number of font to load.
 *  @param hleah    Horizontal lead of the font.
 *  @param vleah    Vertical lead of the font.
 */
int Font::load(
    const File_spec &fname0,
    int index,
    int hlead,
    int vlead
) {
	clean_up();
	IExultDataSource data(fname0, index);
	return load_internal(data, hlead, vlead);
}

/**
 *  Loads a font from a File_spec.
 *  @param fname0   First file spec.
 *  @param fname1   Second file spec.
 *  @param index    Number of font to load.
 *  @param hleah    Horizontal lead of the font.
 *  @param vleah    Vertical lead of the font.
 */
int Font::load(
    const File_spec &fname0,
    const File_spec &fname1,
    int index,
    int hlead,
    int vlead
) {
	clean_up();
	IExultDataSource data(fname0, fname1, index);
	return load_internal(data, hlead, vlead);
}

int Font::center_text(Image_buffer8 *win, int x, int y, const char *s) {
	return draw_text(win, x - get_text_width(s) / 2, y, s);
}

void Font::calc_highlow() {
	bool unset = true;

	for (int i = 0; i < font_shapes->get_num_frames(); i++) {
		Shape_frame *f = font_shapes->get_frame(i);

		if (!f) continue;

		if (unset) {
			unset = false;
			highest = f->get_yabove();
			lowest = f->get_ybelow();
			continue;
		}

		if (f->get_yabove() > highest) highest = f->get_yabove();
		if (f->get_ybelow() > lowest) lowest = f->get_ybelow();
	}
}

FontManager::~FontManager() {
	reset();
}

/**
 *  Loads a font from a File_spec.
 *  @param name Name to give to this font.
 *  @param fname0   First file spec.
 *  @param index    Number of font to load.
 *  @param hleah    Horizontal lead of the font.
 *  @param vleah    Vertical lead of the font.
 */
void FontManager::add_font(
    const char *name,
    const File_spec &fname0,
    int index,
    int hlead,
    int vlead
) {
	remove_font(name);

	Font *font = new Font(fname0, index, hlead, vlead);

	fonts[name] = font;
}

/**
 *  Loads a font from a File_spec.
 *  @param name Name to give to this font.
 *  @param fname0   First file spec.
 *  @param fname1   Second file spec.
 *  @param index    Number of font to load.
 *  @param hleah    Horizontal lead of the font.
 *  @param vleah    Vertical lead of the font.
 */
void FontManager::add_font(
    const char *name,
    const File_spec &fname0,
    const File_spec &fname1,
    int index,
    int hlead,
    int vlead
) {
	remove_font(name);

	Font *font = new Font(fname0, fname1, index, hlead, vlead);

	fonts[name] = font;
}

void FontManager::remove_font(const char *name) {
	if (fonts[name] != nullptr) {
		delete fonts[name];
		fonts.erase(name);
	}
}

Font *FontManager::get_font(const char *name) {
	return fonts[name];
}

void FontManager::reset() {
	unordered_map<const char *, Font *, hashstr, eqstr>::iterator i;

	for (i = fonts.begin(); i != fonts.end(); ++i) {
		delete(*i).second;
	}

	fonts.clear();
}
