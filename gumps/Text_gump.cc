/*
Copyright (C) 2000-2025 The Exult Team

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

#include "Text_gump.h"

#include "game.h"
#include "gamewin.h"

#include <cstring>
#include <string>

using std::strchr;

bool Text_gump::begin_layer_paint(int drawx, int drawy, int& ox, int& oy, int& w, int& h, Image_buffer8*& prev_target) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin) {
		return false;
	}
	const TileRect r = get_rect();
	w                = r.w;
	h                = r.h;
	if (w <= 0 || h <= 0) {
		return false;
	}
	if (render_layer < 0 || layer_bounds.w != w || layer_bounds.h != h) {
		if (render_layer >= 0) {
			gwin->destroy_layer(render_layer);
			render_layer = -1;
		}
		render_layer = gwin->create_layer(typeid(*this).name(), w, h, 255, 0, text_gump_layer_z);
		if (render_layer < 0) {
			return false;
		}
		gwin->layer_set_ui_kind(render_layer, Image_window::UiLayerTextGumps);
	}
	layer_bounds = TileRect(0, 0, w, h);
	gwin->layer_set_z(render_layer, text_gump_layer_z);

	auto* lbuf = gwin->get_layer_ibuf(render_layer);
	if (!lbuf) {
		return false;
	}
	lbuf->clear_clip();
	lbuf->fill8(255);
	prev_target = gwin->push_render_target(lbuf);
	ox          = drawx - r.x;
	oy          = drawy - r.y;
	return true;
}

void Text_gump::end_layer_paint(int w, int h, Image_buffer8* prev_target) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin || render_layer < 0) {
		return;
	}
	gwin->pop_render_target(prev_target);
	Image_window* iwin = gwin->get_win();
	const float   f    = iwin->get_ui_scale_factor(Image_window::UiLayerTextGumps);
	const float   dw   = static_cast<float>(w) * f;
	const float   dh   = static_cast<float>(h) * f;
	const float   dx   = (static_cast<float>(iwin->get_display_width()) - dw) / 2.0f;
	const float   dy   = (static_cast<float>(iwin->get_display_height()) - dh) / 2.0f;
	gwin->layer_set_dest(render_layer, static_cast<int>(dx), static_cast<int>(dy), static_cast<int>(dw), static_cast<int>(dh));
	gwin->layer_set_visible(render_layer, true);
	gwin->layer_set_dirty(render_layer);
}

Text_gump::~Text_gump() {
	delete[] text;
	free_render_layer();
}

/*
 *  Add to the text, starting a newline.
 */

void Text_gump::add_text(const char* str) {
	std::string newtext;
	if (textlen) {
		newtext = text;
		if (newtext[textlen - 1] != '*') {
			// Scrolls in SI break page with embedded 0x00
			if (GAME_SI && get_shapenum() == game->get_shape("gumps/scroll") && !from_help) {
				newtext += "*";
			} else {
				newtext += "~";
			}
		}
	}
	newtext += str;
	delete[] text;
	text = new char[newtext.length() + 1];
	strcpy(text, newtext.c_str());
	textlen = newtext.length();
}

/*
 *  Paint a page and find where its text ends.
 *
 *  Output: Index past end of displayed page.
 */

int Text_gump::paint_page(const TileRect& box, int start) {
	int       ypos       = 0;
	const int vlead      = 1;
	const int textheight = sman->get_text_height(font) + vlead;
	char*     str        = text + start;
	curend               = start;    // Initialize curend to the starting position

	char* lineBreak     = nullptr;
	char* pageBreakStar = nullptr;
	char* extraBreak    = nullptr;
	char* epage         = nullptr;    // page break marker
	char* eol           = nullptr;    // end-of-line marker

	while (*str && ypos + textheight <= box.h) {
		lineBreak     = strchr(str, '~');
		pageBreakStar = strchr(str, '*');

		if (GAME_BG && get_shapenum() == game->get_shape("gumps/scroll")) {
			extraBreak = strstr(str, " ~~");
			if (extraBreak) {
				// If " ~~" is preceded by "~~", ignore it.
				if (extraBreak - text >= 2 && *(extraBreak - 2) == '~' && *(extraBreak - 1) == '~') {
					extraBreak = nullptr;
				}
			}
		}

		// if extraBreak is found and it occurs at the same position
		// as the first tilde, use it as page break.
		if (extraBreak && lineBreak && extraBreak + 1 == lineBreak) {
			epage = extraBreak;
			eol   = extraBreak;
		} else if (pageBreakStar && (!lineBreak || pageBreakStar < lineBreak)) {
			epage = pageBreakStar;
			eol   = pageBreakStar;
		} else {
			eol   = lineBreak;
			epage = nullptr;    // no page break marker
		}

		if (!eol) {
			// No marker found: paint until the end of text.
			eol = text + textlen;
		}

		// Temporarily null-terminate at eol for painting.
		const char eolchr = *eol;
		*eol              = '\0';

		int endoff = sman->paint_text_box(font, str, x + box.x, y + box.y + ypos, box.w, box.h - ypos, vlead);
		// Restore the character at eol.
		*eol = eolchr;

		if (endoff > 0) {
			// Check for a page‑break marker
			if (epage && get_shapenum() == game->get_shape("gumps/scroll")) {
				if (GAME_SI && start == 0 && ypos <= 5 * textheight && !from_help) {
					// ignore page breaks on the first 5 lines of a scroll
					str    = pageBreakStar + 1;
					curend = str - text;
					ypos += endoff;
					continue;
				}
				// Calculate vertical space remaining after the current
				// block has been painted.
				int remaining = box.h - (ypos + endoff);

				// Require room for at least two extra full line.
				if (remaining >= 2 * textheight) {
					// Reserve two extra line for spacing; the rest is
					// available for the next text chunk.
					int available = remaining - (2 * textheight);
					if (available < 0) {
						available = 0;
					}

					// Use existing markers to find earliest upcoming marker
					char* earliest = text + textlen;    // default: measure to
														// end-of-text

					// Use same check for both page break markers
					if ((pageBreakStar > eol + 1 && pageBreakStar < earliest)
						|| (extraBreak > eol + (epage == extraBreak ? 3 : 1) && extraBreak < earliest)) {
						earliest = (pageBreakStar && pageBreakStar < earliest) ? pageBreakStar : extraBreak;
					}

					// Temporarily replace the character at 'earliest' with
					// '\0' to isolate the next chunk.
					char saved = *earliest;

					// PEEK MEASUREMENT:
					// To avoid drawing on top of already painted text, we
					// use an off-screen y coordinate (e.g. -1000).
					int peekHeight = sman->paint_text_box(
							font, eol + (epage == extraBreak ? 3 : 1), x + box.x,
							-1000,    // off-screen y coordinate for
									  // measurement only
							box.w, available, vlead);
					// Restore the character we replaced.
					*earliest = saved;

					if (peekHeight > 0 && peekHeight <= available + 5)    // Add 5 pixels tolerance
					{
						// The entire upcoming text fits in the available
						// space. Consume most of the extraBreak/pageBreakStar
						// marker and add an extra line spacing.
						eol += (epage == extraBreak ? 2 : 1);
						ypos += endoff;         // update vertical offset
						str    = eol;           // continue painting after the marker
						curend = str - text;    // Update curend
						continue;               // next iteration: the peeked block
												// will be painted on this page only
					} else {
						// The upcoming block does not fully fit;
						// consume the marker and break so that it will be
						// painted on the next page.
						eol += (epage == extraBreak ? 3 : 1);
						str    = eol;
						curend = str - text;    // Update curend
						break;
					}
				}
			} else if (eolchr == '~' || (lineBreak && lineBreak == eol)) {
				// Normal handling for a tilde line break.
				eol++;
			}

			// After handling markers (or if none applied), update the text
			// pointer and vertical offset.
			ypos += endoff;
			str    = eol;
			curend = str - text;    // Update curend

			// Skip any leading space at the beginning of a new line.
			if (*str == ' ') {
				str++;
			}
		} else {
			// Out of room or partial paint -> back up
			str += -endoff;
			curend = str - text;    // Update curend
			break;
		}
	}

	// After the loop, check for markers at current position
	if (pageBreakStar && pageBreakStar == str) {
		str++;    // skip "*"
	}

	curend = str - text;    // Update curend one last time after the loop
	gwin->set_painted();
	return str - text;
}

/*
 *  Show next page(s) of book or scroll.
 *
 *  Output: 0 if already at end.
 */

int Text_gump::show_next_page() {
	if (curend >= textlen) {
		return 0;    // That's all, folks.
	}
	curtop = curend;    // Start next page or pair of pages.

	// Skip any leading tildes at the beginning of the new page.
	while (curtop < textlen && text[curtop] == '~') {
		curtop++;
	}

	paint();    // Paint.  This updates curend.
	return 1;
}
