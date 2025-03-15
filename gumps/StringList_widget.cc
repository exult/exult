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
#include "StringList_widget.h"

#include "gamewin.h"

StringList_widget::StringList_widget(
		Gump_Base* par, const std::vector<std::string>& s, int selectionnum,
		int px, int py, Modal_gump::ProceduralColours colours, int w, int h,
		std::shared_ptr<Font> f)
		: Gump_widget(par, -1, px, py, selectionnum), font(f), selections(s),
		  width(w), height(h), colours(colours) {
	if (!font) {
		font = fontManager.get_font("SMALL_BLACK_FONT");
	}

	lineheight = font->get_text_height() + 2;

	if (height == 0) {
		height = lineheight * selections.size() + 2;
	}
	if (width == 0) {
		for (std::string& line : selections) {
			width = std::max(width, font->get_text_width(line.c_str()) + 2);
		}
	}
}

void StringList_widget::paint() {
	auto ib = gwin->get_win()->get_ib8();
	int  sx = 0, sy = 0;
	local_to_screen(sx, sy);

	// draw beveled_box with highlight and shadow swapped
	ib->draw_beveled_box(
			sx, sy, width, height, 1, colours.Background, colours.Shadow,
			colours.Shadow, colours.Highlight, colours.Highlight2);

	Image_buffer8::ClipRectSave clipsave(ib);

	for (int line = 0; line < int(selections.size()); line++) {
		int      liney   = sy + line * lineheight + 1;
		TileRect newclip = clipsave.Rect().intersect(
				TileRect(sx, liney, width, lineheight));
		ib->set_clip(newclip.x, newclip.y, newclip.w, newclip.h);

		// if we are selected draw background highlight
		if (getselection() == line) {
			ib->draw_box(
					sx + 1, liney, width - 2, lineheight, 0, colours.Highlight,
					0xff);
		}

		// draw separator line
		if (line >= 1) {
			ib->fill_hline8(colours.border, width - 2, sx + 1, liney);
		}

		// Draw Text
		font->paint_text(ib, selections[line].c_str(), sx + 1, liney + 2);
	}
}

bool StringList_widget::mouse_down(int mx, int my, MouseButton button) {
	screen_to_local(mx, my);

	if (my < 0 || my > height || mx < 0 || mx > width) {
		return false;
	}

	if (button == MouseButton::Left) {
		set_frame(my / lineheight);

		activate(button);
		gwin->add_dirty(get_rect());
	}
	return true;
}

void StringList_widget::setselection(int newsel) {
	set_frame(newsel);
	gwin->add_dirty(get_rect());
}

bool StringList_widget::on_widget(int mx, int my) const {
	return get_rect().has_point(mx, my);
}

TileRect StringList_widget::get_rect() const {
	int sx = 0, sy = 0;
	local_to_screen(sx, sy);
	return TileRect(sx, sy, width, height);
}
