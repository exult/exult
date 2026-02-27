/*
Copyright (C) 2001-2022 The Exult Team

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

#include "Text_button.h"

#include "Gump.h"
#include "font.h"
#include "gamewin.h"
#include "iwin8.h"

#define TB_FONTNAME "SMALL_BLACK_FONT"

Text_button::Text_button(Gump_Base* p, const std::string_view& str, int x, int y, int w, int h, std::shared_ptr<Font> font)
		: Basic_button(p, x, y, w, h), text(str), font(font ? std::move(font) : fontManager.get_font(TB_FONTNAME)) {
	init();
}

void Text_button::init() {
	auto draw_area = get_draw_area(false);

	const int text_height = font->get_text_height();
	if (draw_area.h < text_height) {
		height += text_height - draw_area.h;
	}

	const int text_width = font->get_text_width(text.c_str());

	if (draw_area.w < text_width + 4) {
		width += text_width + 4 - draw_area.w;
	}
	draw_area = get_draw_area(false);

	// Center text in the draw area rounded up
	text_y = (draw_area.h - text_height) / 2;
	text_x = (draw_area.w - text_width) >> 1;
}

void Text_button::paint() {
	Basic_button::paint();

	Image_window8* iwin = gwin->get_win();
	auto*          ib8  = iwin->get_ib8();

	auto draw_area = get_draw_area();
	local_to_screen(draw_area.x, draw_area.y);

	// Clip text
	auto     clipsave = ib8->SaveClip();
	TileRect newclip  = clipsave.Rect().intersect(draw_area);
	ib8->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);
	// Paint text
	font->paint_text(ib8, text.c_str(), draw_area.x + text_x, draw_area.y + text_y);
}
