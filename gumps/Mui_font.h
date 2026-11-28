/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_FONT_H
#define MUI_FONT_H

#include "Mui_theme.h"
#include "common_types.h"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Font;
class Image_buffer8;
class Shape_file;

/*
 *  One anti-aliased bitmap font from mui_fonts.vga. Glyph pixels hold the
 *  coverage levels 1-4, remapped at draw time to a ramp from the background
 *  colour to the text colour. Text is in Exult font bytes (exultmsg.txt
 *  strings are already translated through data/fonts/font_map.txt).
 */
class Mui_text_font {
public:
	Mui_text_font(std::unique_ptr<Shape_file> shapes, int px);
	// Fallback: an unscaled classic Exult font.
	explicit Mui_text_font(std::shared_ptr<Font> classic);
	~Mui_text_font();
	Mui_text_font(const Mui_text_font&)            = delete;
	Mui_text_font& operator=(const Mui_text_font&) = delete;

	int get_px() const {
		return px;
	}

	int height() const {    // Line height.
		return line_h;
	}

	int ascent() const {    // Baseline distance from the top.
		return base;
	}

	int width(const std::string& text) const;
	int width(const char* text, size_t len) const;
	// Draw text with its top-left at (x, y). Returns the width drawn.
	int draw(Image_buffer8* buf, const std::string& text, int x, int y, uint8 fg, uint8 bg) const;
	// Shorten text with an ellipsis so it fits max_w.
	std::string elide(const std::string& text, int max_w) const;
	// Word-wrap into lines no wider than max_w ('\n' forces a break).
	std::vector<std::string> wrap(const std::string& text, int max_w) const;

private:
	std::unique_ptr<Shape_file>                              shapes;
	std::shared_ptr<Font>                                    classic;
	int                                                      px     = 0;
	int                                                      line_h = 0;
	int                                                      base   = 0;
	mutable std::map<uint16, std::array<unsigned char, 256>> trans_cache;
	const unsigned char*                                     trans_for(uint8 fg, uint8 bg) const;
};

/*
 *  The 28 themed menu fonts (4 faces x 7 sizes).
 */
class Mui_fonts {
public:
	static Mui_fonts& get();
	// Nearest available size of a face.
	const Mui_text_font* pick(Mui_face face, int px);

	bool using_fallback() const {
		return fallback != nullptr;
	}

private:
	Mui_fonts();
	std::vector<std::unique_ptr<Mui_text_font>> fonts;    // face * sizes + size
	std::unique_ptr<Mui_text_font>              fallback;
};

#endif
