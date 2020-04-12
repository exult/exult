/*
Copyright (C) 2000-2013 The Exult Team

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

#include <cstring>

#include "gamewin.h"
#include "Text_gump.h"

using std::strchr;
using std::strcpy;
using std::strlen;

/*
 *  Add to the text, starting a newline.
 */

void Text_gump::add_text(
    const char *str
) {
	int slen = strlen(str);     // Length of new text.
	// Allocate new space.
	char *newtext = new char[textlen + (textlen != 0) + slen + 1];
	if (textlen) {          // Copy over old.
		strcpy(newtext, text);
		// Add new line if not starting a new page and if first char of new string is
		// not a new line
		if (newtext[textlen - 1] != '*') newtext[textlen++] = '~';
	}
	strcpy(newtext + textlen, str); // Append new.
	delete [] text;
	text = newtext;
	textlen += slen;
}

/*
 *  Paint a page and find where its text ends.
 *
 *  Output: Index past end of displayed page.
 */

int Text_gump::paint_page(
    Rectangle const &box,           // Display box rel. to gump.
    int start           // Starting offset into text.
) {
	const int vlead = 1;        // Extra inter-line spacing.
	int ypos = 0;
	int textheight = sman->get_text_height(font) + vlead;
	char *str = text + start;
	while (*str && *str != '*' && ypos + textheight <= box.h) {
		if (*str == '~') {  // Empty paragraph?
			ypos += textheight;
			str++;
			continue;
		}
		// Look for page break.
		char *epage = strchr(str, '*');
		// Look for line break.
		char *eol = strchr(str, '~');
		if (epage && (!eol || eol > epage))
			eol = epage;
		if (!eol)       // No end found?
			eol = text + textlen;
		char eolchr = *eol; // Save char. at EOL.
		*eol = 0;
		int endoff = sman->paint_text_box(font, str, x + box.x,
		                                  y + box.y + ypos, box.w, box.h - ypos, vlead);
		*eol = eolchr;      // Restore char.
		if (endoff > 0) {   // All painted?
			// Value returned is height.
			str = eol + (eolchr == '~');
			ypos += endoff;
		} else {        // Out of room.
			str += -endoff;
			break;
		}
	}
	if (*str == '*')        // Saw end of page?
		str++;
	gwin->set_painted();        // Force blit.
	return str - text;        // Return offset past end.
}

/*
 *  Show next page(s) of book or scroll.
 *
 *  Output: 0 if already at end.
 */

int Text_gump::show_next_page(
) {
	if (curend >= textlen)
		return 0;     // That's all, folks.
	curtop = curend;        // Start next page or pair of pages.
	paint();            // Paint.  This updates curend.
	return 1;
}
