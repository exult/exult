/*
Copyright (C) 2000 The Exult Team

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
#  include <config.h>
#endif

#include "Sign_gump.h"
#include "gamewin.h"
#include "game.h"
#include "actors.h"


/*
 *  Create a sign gump.
 */

Sign_gump::Sign_gump(
    int shapenum,
    int nlines          // # of text lines.
) : Gump(nullptr, shapenum), num_lines(nlines), serpentine(false) {
	// THIS IS A HACK, but don't ask me why this is like this,
	if (Game::get_game_type() == SERPENT_ISLE && shapenum == 49) {
		// check for avatar read here
		Main_actor *avatar = gwin->get_main_actor();
		if (!avatar->get_flag(Obj_flags::read))
			serpentine = true;

		shapenum = game->get_shape("gumps/goldsign");
		set_shape(shapenum);
		set_pos();  // Recenter
	}

	if (shapenum == game->get_shape("gumps/woodsign")) {
		set_object_area(Rectangle(0, 4, 196, 92));
	} else if (shapenum == game->get_shape("gumps/tombstone")) {
		set_object_area(Rectangle(0, 8, 200, 112));
	} else if (shapenum == game->get_shape("gumps/goldsign")) {
		if (Game::get_game_type() == BLACK_GATE)
			set_object_area(Rectangle(0, 4, 232, 96));
		else            // SI
			set_object_area(Rectangle(4, 4, 312, 96));
	} else if (shapenum == game->get_shape("gumps/scroll"))
		set_object_area(Rectangle(48, 30, 146, 118));
	lines = new std::string[num_lines];
}

/*
 *  Delete sign.
 */

Sign_gump::~Sign_gump(
) {
	delete [] lines;
}

/*
 *  Add a line of text.
 */

void Sign_gump::add_text(
    int line,
    const std::string &txt
) {
	if (line < 0 || line >= num_lines)
		return;

	// check for avatar read here
	Main_actor *avatar = gwin->get_main_actor();

	if (!serpentine && avatar->get_flag(Obj_flags::read)) {
		for (const auto& ch : txt) {
			if (ch == '(') {
				lines[line] += "TH";
			} else if (ch == ')') {
				lines[line] += "EE";
			} else if (ch == '*') {
				lines[line] += "NG";
			} else if (ch == '+') {
				lines[line] += "EA";
			} else if (ch == ',') {
				lines[line] += "ST";
			} else if (ch == '|') {
				lines[line] += ' ';
			} else {
				lines[line] += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
			}
		}
	} else {
		lines[line] = txt;
	}
}

/*
 *  Paint sign.
 */

void Sign_gump::paint(
) {
	int font = 1;           // Normal runes.
	if (get_shapenum() == game->get_shape("gumps/goldsign")) {
		if (serpentine)
			font = 10;
		else
			font = 6;       // Embossed.
	} else if (serpentine)
		font = 8;
	else if (get_shapenum() == game->get_shape("gumps/tombstone"))
		font = 3;
	// Get height of 1 line.
	int lheight = sman->get_text_height(font);
	// Get space between lines.
	int lspace = (object_area.h - num_lines * lheight) / (num_lines + 1);
	// Paint the gump itself.
	paint_shape(x, y);
	int ypos = y + object_area.y;   // Where to paint next line.
	for (int i = 0; i < num_lines; i++) {
		ypos += lspace;
		if (lines[i].empty())
			continue;
		sman->paint_text(font, lines[i].c_str(),
		                 x + object_area.x +
		                 (object_area.w -
		                  sman->get_text_width(font, lines[i].c_str())) / 2,
		                 ypos);
		ypos += lheight;
	}
	gwin->set_painted();
}
