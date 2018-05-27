/*
 *  Copyright (C) 2000-2013  The Exult Team
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
#  include <config.h>
#endif

#include <cstring>
#include <cstdlib>

#include "sdl-compat.h"
#include "common_types.h"
#include "files/U7file.h"
#include "gamewin.h"
#include "shapeid.h"
#include "txtscroll.h"
#include "font.h"
#include "game.h"

#include "SDL_timer.h"
#include "SDL_events.h"
#if SDL_VERSION_ATLEAST(2, 0, 0)
#  include "SDL_stdinc.h"
#  include "SDL_scancode.h"
#else
#  include "SDL_keysym.h"
#endif

#ifndef UNDER_EMBEDDED_CE
using std::atoi;
using std::size_t;
using std::strchr;
using std::string;
using std::strlen;
using std::strncmp;
using std::vector;
#endif

TextScroller::TextScroller(const char *archive, int index, Font *fnt, Shape *shp) {
	font = fnt;
	shapes = shp;
	U7object *txtobj;
	// Hack to patch MAINSHP_FLX.
	if (!strncmp(archive, MAINSHP_FLX, sizeof(MAINSHP_FLX) - 1))
		txtobj = new U7multiobject(archive, PATCH_MAINSHP, index);
	else
		txtobj = new U7object(archive, index);
	size_t len;
	const char  CR = '\r', LF = '\n';

	char *txt, *ptr, *end;
	txt = txtobj->retrieve(len);
	delete txtobj;
	if (!txt || len <= 0) {
		delete [] txt;
		text = new vector<string>();
		return;
	}
	ptr = txt;
	end = ptr + len;

	text = new vector<string>();
	while (ptr < end) {
		char *start = ptr;
		ptr = strchr(ptr, LF);
		if (ptr) {
			if (*(ptr - 1) == CR) // It's CR/LF
				*(ptr - 1) = 0;
			else
				*ptr = 0;
			text->push_back(string(start));
			ptr += 1;
		} else
			break;
	}
	delete [] txt;
}

TextScroller::~TextScroller() {
	delete text;
}

int TextScroller::show_line(Game_window *gwin, int left, int right, int y, int index) {
	Shape_manager *sman = Shape_manager::get_instance();

	//The texts used in the main menu contains backslashed sequences that
	//indicates the output format of the lines:
	// \Px   include picture number x (frame nr. of shape passed to constructor)
	// \C    center line
	// \L    left aligned to right center line
	// \R    right aligned to left center line
	// |     carriage return (stay on same line)
	// #xxx  display character with number xxx
	string str = (*text)[index];
	const char *ptr = str.c_str();
	char *txt = new char[strlen(ptr) + 1];

	char *txtptr = txt;
	int ypos = y;
	int vspace = 2; // 2 extra pixels between lines
	// Align text to the left by default
	int align = -1;
	int xpos = left;
	int center = (right + left) / 2;
	bool add_line = true;

	while (*ptr) {
		if (!strncmp(ptr, "\\P", 2)) {
			int pix = *(ptr + 2) - '0';
			ptr += 3;
			Shape_frame *frame = shapes->get_frame(pix);
			if (frame) {
				sman->paint_shape(center - frame->get_width() / 2,
				                  ypos, frame);
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
				add_line = false;
			}
			*txtptr = 0;

			if (align < 0)
				xpos = center - font->get_text_width(txt);
			else if (align == 0)
				xpos = center - font->get_text_width(txt) / 2;
			else
				xpos = center;
			font->draw_text(gwin->get_win()->get_ib8(),
			                xpos, ypos, txt);
			if (*ptr != '|') ypos += font->get_text_height() + vspace;
			txtptr = txt;   // Go to beginning of string
			++ptr;
		} else if (*ptr == '#') {
			ptr++;
			if (*ptr == '#') { // Double hash
				*txtptr++ = *ptr++;
				continue;
			}
			char numerical[4] = {0, 0, 0, 0};
			char *num = numerical;
			while (*ptr >= '0' && *ptr <= '9')
				*num++ = *ptr++;
			*txtptr++ = atoi(numerical);
		} else
			*txtptr++ = *ptr++;
	}

	delete [] txt;
	if (add_line)
		ypos += font->get_text_height();
	return ypos;
}


bool TextScroller::run(Game_window *gwin) {
	gwin->clear_screen();
	gwin->show(1);

	int topx = (gwin->get_width() - 320) / 2;
	int topy = (gwin->get_height() - 200) / 2;
	int endy = topy + 200;
	uint32 starty = endy;
	uint32 startline = 0;
	unsigned int maxlines = text->size();
	if (!maxlines) {
		gwin->clear_screen();
		gwin->show(1);
		return false;
	}
	bool looping = true;
	bool complete = false;
	SDL_Event event;
	uint32 next_time = SDL_GetTicks() + 200;
	uint32 incr = 120;
	//  pal.apply();
	gwin->get_pal()->apply();

	while (looping) {
		int ypos = starty;
		uint32 curline = startline;
		gwin->clear_screen();
		do {
			if (curline == maxlines)
				break;
			ypos = show_line(gwin, topx, topx + 320, ypos, curline++);
			if (ypos < topy) {  // If this line doesn't appear, don't show it next time
				++startline;
				starty = ypos;
				if (startline >= maxlines) {
					looping = false;
					complete = true;
					break;
				}
			}
		} while (ypos < endy);
//		pal.apply();
		gwin->show(1);
		do {
			// this could be a problem when too many events are produced
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_RSHIFT || event.key.keysym.sym == SDLK_LSHIFT)
						incr = 0;
					else
						looping = false;
					break;

				case SDL_KEYUP:
					incr = 120;
					next_time = SDL_GetTicks();
					break;
				case SDL_MOUSEBUTTONUP:
					looping = false;
					break;
				default:
					break;
				}
			}
		} while (next_time > SDL_GetTicks());
		next_time = SDL_GetTicks() + incr;
		if (!looping)
			gwin->get_pal()->fade_out(c_fade_out_time);
		starty--;
	}
	gwin->clear_screen();
	gwin->show(1);
	return complete;
}

int TextScroller::get_count() {
	return text->size();
}
