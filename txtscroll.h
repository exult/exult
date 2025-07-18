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

#ifndef TEXT_SCROLLER_H
#define TEXT_SCROLLER_H

#include <istream>
#include <memory>
#include <string>
#include <vector>
class Font;
class Game_window;
class Shape;
class Shape_file;
class Palette;

class TextScroller {
private:
	std::shared_ptr<Font>    font;
	Shape*                   shapes;
	std::vector<std::string> lines;
	void                     load_from_stream(std::istream& stream);

public:
	// reading from legacy U7multiobject
	TextScroller(
			const char* archive, int index, std::shared_ptr<Font> font,
			Shape* shp);
	// reading from an in-memory string
	TextScroller(
			const std::string& text_content, std::shared_ptr<Font> font,
			Shape* shp);
	~TextScroller();

	bool run(Game_window* gwin);
	int  show_line(Game_window* gwin, int left, int right, int y, int index);

	int get_count() const {
		return lines.size();
	}
};

#endif