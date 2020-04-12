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

#ifndef TEXT_SCROLLER_H
#define TEXT_SCROLLER_H

#include <string>
#include <vector>

class Font;
class Game_window;
class Shape;
class Shape_file;
class Palette;

class TextScroller {
private:
	Font *font;
	Shape *shapes;
	std::vector<std::string> *text;
public:
	TextScroller(const char *archive, int index, Font *fnt, Shape *shp);
	~TextScroller();
	bool run(Game_window *gwin);
	int show_line(Game_window *gwin, int left, int right, int y, int index);
	int get_count();
};

#endif
