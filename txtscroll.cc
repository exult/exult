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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "txtscroll.h"

#include "Configuration.h"
#include "common_types.h"
#include "exult_constants.h"
#include "files/U7file.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "istring.h"
#include "shapeid.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::atoi;
using std::make_unique;
using std::size_t;
using std::strchr;
using std::string;
using std::strncmp;
using std::unique_ptr;
using std::vector;

// Legacy constructor
TextScroller::TextScroller(
		const char* archive, int index, std::shared_ptr<Font> fnt, Shape* shp,
		bool translate_menu)
		: font(fnt), shapes(shp), translate_menu_chars(translate_menu) {
	auto txtobj = [&]() {
		// Hack to patch MAINSHP_FLX.
		if (!strncmp(archive, MAINSHP_FLX, sizeof(MAINSHP_FLX) - 1)) {
			return U7multiobject(archive, PATCH_MAINSHP, index);
		}
		return U7multiobject(archive, index);
	}();
	size_t len;

	const unique_ptr<unsigned char[]> txt = txtobj.retrieve(len);
	if (!txt || len <= 0) {
		return;
	}
	std::string        content(reinterpret_cast<char*>(txt.get()), len);
	std::istringstream stream(content);
	load_from_stream(stream);
}

// Modern constructor for in-memory text
TextScroller::TextScroller(
		const std::string& text_content, std::shared_ptr<Font> fnt, Shape* shp,
		bool translate_menu)
		: font(fnt), shapes(shp), translate_menu_chars(translate_menu) {
	std::istringstream stream(text_content);
	load_from_stream(stream);
}

TextScroller::~TextScroller() {
	// No longer need to 'delete text;'
}

/*
 *  Translate special characters for MENU_FONT compatibility.
 *  When using built-in fonts, the characters ™️ © are not at the positions
 *  the original used in MAINSHP.FLX' menu font.
 *  - BG/SI English/Spanish: 0x01 and 0x02 -> 0x8C and 0x8D
 *  - BG French: 0x0F and 0x1F -> 0x8C and 0x8D
 *  - BG German BG: 0x03 and 0x04 -> 0x8C and 0x8D
 */
static void translate_menu_font_chars(std::string& line) {
	// Check if we need to translate (only for original/serif fonts)
	std::string font_config;
	config->value("config/gameplay/fonts", font_config, "original");
	Pentagram::tolower(font_config);

	if (font_config == "disabled") {
		return;    // No translation needed when using mainshp.flx fonts
	}

	// Determine which characters to translate based on game language
	char from1, from2;
	if (Game::get_game_type() == BLACK_GATE
		&& Game::get_game_language() == Game_Language::GERMAN) {
		from1 = 0x03;
		from2 = 0x04;
	} else if (
			Game::get_game_type() == BLACK_GATE
			&& Game::get_game_language() == Game_Language::FRENCH) {
		from1 = 0x0F;
		from2 = 0x1F;
	} else {
		// BG English/Spanish and SI
		from1 = 0x01;
		from2 = 0x02;
	}

	// Translate the characters
	for (char& c : line) {
		if (c == from1) {
			c = static_cast<char>(0x8C);
		} else if (c == from2) {
			c = static_cast<char>(0x8D);
		}
	}
}

void TextScroller::load_from_stream(std::istream& stream) {
	string     line;
	const char CR = '\r';

	while (std::getline(stream, line)) {
		// Handle CR/LF endings
		if (!line.empty() && line.back() == CR) {
			line.pop_back();
		}
		// Translate special characters for menu font (credits/quotes only)
		if (translate_menu_chars) {
			translate_menu_font_chars(line);
		}
		lines.push_back(line);
	}
}

int TextScroller::show_line(
		Game_window* gwin, int left, int right, int y, int index) {
	Shape_manager* sman = Shape_manager::get_instance();

	// The texts used in the main menu contains backslashed sequences that
	// indicates the output format of the lines:
	//  \Px   include picture number x (frame nr. of shape passed to constructor)
	//  \C    center line
	//  \L    left aligned to right center line
	//  \R    right aligned to left center line
	//  |     carriage return (stay on same line)
	//  #xxx  display character with number xxx
	const string& str = lines[index];
	const char*   ptr = str.c_str();
	char*         txt = new char[strlen(ptr) + 1];

	char*     txtptr   = txt;
	int       ypos     = y;
	const int vspace   = 2;    // 2 extra pixels between lines
	int       align    = -2;
	int       xpos     = left;
	const int center   = (right + left) / 2;
	bool      add_line = true;

	while (*ptr) {
		if (!strncmp(ptr, "\\P", 2)) {
			const int pix = *(ptr + 2) - '0';
			ptr += 3;
			Shape_frame* frame = shapes->get_frame(pix);
			if (frame) {
				sman->paint_shape(center - frame->get_width() / 2, ypos, frame);
				ypos += frame->get_height() + vspace;
			}
		} else if (!strncmp(ptr, "\\C", 2)) {
			ptr += 2;
			align = 0;
		} else if (!strncmp(ptr, "\\L", 2)) {
			ptr += 2;
			align = 1;
		} else if (!strncmp(ptr, "\\R", 2)) {
			ptr += 2;
			align = -1;
		} else if (*ptr == '|' || *(ptr + 1) == 0) {
			if (*(ptr + 1) == 0 && *ptr != '|') {
				*txtptr++ = *ptr;
				add_line  = false;
			}
			*txtptr = 0;

			if (align == -2) {
				xpos = left + 10;
			} else if (align == -1) {
				xpos = center - font->get_text_width(txt);
			} else if (align == 0) {
				xpos = center - font->get_text_width(txt) / 2;
			} else {
				xpos = center;
			}
			font->draw_text(gwin->get_win()->get_ib8(), xpos, ypos, txt);
			if (*ptr != '|') {
				ypos += font->get_text_height() + vspace;
			}
			txtptr = txt;    // Go to beginning of string
			++ptr;
		} else if (*ptr == '#') {
			ptr++;
			if (*ptr == 'S') {    // #S - skip/ignore this sequence
				ptr++;
				continue;
			}
			// Use the byte value of the character after # directly
			// (e.g., ## outputs 0x23, #\x8C outputs 0x8C)
			*txtptr++ = *ptr++;
		} else {
			*txtptr++ = *ptr++;
		}
	}

	delete[] txt;
	if (add_line) {
		ypos += font->get_text_height();
	}
	return ypos;
}

bool TextScroller::run(Game_window* gwin) {
	gwin->clear_screen();
	gwin->show(true);

	const int          topx      = (gwin->get_width() - 320) / 2;
	const int          topy      = (gwin->get_height() - 200) / 2;
	const int          endy      = topy + 200;
	uint32             starty    = endy;
	uint32             startline = 0;
	const unsigned int maxlines  = lines.size();
	if (!maxlines) {
		gwin->clear_screen();
		gwin->show(true);
		return false;
	}
	bool      looping  = true;
	bool      complete = false;
	SDL_Event event;
	uint32    next_time = SDL_GetTicks() + 200;
	uint32    incr      = 120;
	gwin->get_pal()->apply();

	while (looping) {
		int    ypos    = starty;
		uint32 curline = startline;
		gwin->clear_screen();
		do {
			if (curline == maxlines) {
				break;
			}
			ypos = show_line(gwin, topx, topx + 320, ypos, curline++);
			if (ypos < topy) {    // If this line doesn't appear, don't show it
								  // next time
				++startline;
				starty = ypos;
				if (startline >= maxlines) {
					looping  = false;
					complete = true;
					break;
				}
			}
		} while (ypos < endy);
		gwin->show(true);
		do {
			// this could be a problem when too many events are produced
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_EVENT_KEY_DOWN:
					if (event.key.key == SDLK_RSHIFT
						|| event.key.key == SDLK_LSHIFT) {
						incr = 0;
					} else {
						looping = false;
					}
					break;

				case SDL_EVENT_KEY_UP:
					incr      = 120;
					next_time = SDL_GetTicks();
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					looping = false;
					break;
				default:
					break;
				}
			}
		} while (next_time > SDL_GetTicks());
		next_time = SDL_GetTicks() + incr;
		if (!looping) {
			gwin->get_pal()->fade_out(c_fade_out_time);
		}
		starty--;
	}
	gwin->clear_screen();
	gwin->show(true);
	return complete;
}