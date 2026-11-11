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

#ifndef CONTAINER_LIST_LAYOUT_H
#define CONTAINER_LIST_LAYOUT_H

#include <algorithm>

/*
 *  Layout of the container list view (Container_list_gump).  All values
 *  are in gump-local pixels, with (0,0) at the top-left of the widened
 *  scroll.  Kept free of engine types so the arithmetic can be tested on
 *  its own.
 */
namespace Container_list_layout {
	// The scroll gump ("gumps/scroll": SI gump 49, BG gump 55): one frame,
	// hotspot at its top-left.
	constexpr int scroll_w = 224;
	constexpr int scroll_h = 173;
	// Plain parchment strip, repeated to widen the scroll.  Columns before
	// strip_x1 are drawn as they are; the rest of the scroll moves right by
	// extra_w.
	constexpr int strip_x0 = 80;
	constexpr int strip_x1 = 140;
	// 224 + 96 = 320, the narrowest game width Exult offers.
	constexpr int extra_w = 96;
	// Text area: Scroll_gump's box (51, 31, 142, 118), widened.
	constexpr int text_x = 51;
	constexpr int text_y = 31;
	constexpr int text_w = 142 + extra_w;
	constexpr int text_h = 118;
	// List column, including its scroll controls.
	constexpr int list_w = 138;
	constexpr int rows   = 8;
	constexpr int row_h  = 12;
	// First row, below the title and its rule.
	constexpr int row_y0 = text_y + 11;
	// Row width, leaving room on the right for the scroll controls.
	constexpr int row_w = list_w - 15;
	// Scroll controls: Exult's 11x12 arrow buttons and 7x7 slider.
	constexpr int ctrl_x   = text_x + list_w - 12;
	constexpr int up_y     = row_y0;
	constexpr int down_y   = row_y0 + rows * row_h - 12;
	constexpr int track_y0 = up_y + 13;
	constexpr int track_y1 = row_y0 + rows * row_h - 13;
	constexpr int slider_h = 7;
	// "N-M of T" line, below the rows.
	constexpr int count_y = row_y0 + rows * row_h + 2;
	// Right panel: the selected item.
	constexpr int panel_x = text_x + list_w + 9;
	constexpr int panel_w = text_w - list_w - 9;
	// Checkmark hotspot (the 21x21 check shape's hotspot is its
	// bottom-right).  The parchment (rows 27-147 from column 47) has no
	// free 21x21 corner, so the check sits on the left end of the bottom
	// roller (rows 148-166), covering 50-70 x 146-166.
	constexpr int check_x = 70;
	constexpr int check_y = 166;

	// Map a column of the widened gump to a column of the scroll frame.
	constexpr int source_column(int gx) {
		if (gx < strip_x1) {
			return gx;
		}
		if (gx < strip_x1 + extra_w) {
			return strip_x0 + (gx - strip_x1) % (strip_x1 - strip_x0);
		}
		return gx - extra_w;
	}

	// First visible row, kept in [0, max(0, count - rows)].
	constexpr int clamp_first(int first, int count) {
		return std::clamp(first, 0, std::max(0, count - rows));
	}

	// Smallest change to 'first' that shows row 'sel'.
	constexpr int first_showing(int first, int sel, int count) {
		if (sel >= 0) {
			if (sel < first) {
				first = sel;
			} else if (sel >= first + rows) {
				first = sel - rows + 1;
			}
		}
		return clamp_first(first, count);
	}

	// Slider offset along a track with 'travel' pixels of movement.
	constexpr int slider_offset(int first, int count, int travel) {
		if (count <= rows) {
			return 0;
		}
		return travel * clamp_first(first, count) / (count - rows);
	}

	// First visible row for a slider offset (rounded inverse of the above).
	constexpr int first_from_slider(int offset, int count, int travel) {
		if (count <= rows || travel <= 0) {
			return 0;
		}
		return clamp_first((2 * offset * (count - rows) + travel) / (2 * travel), count);
	}

	// Item index under a local point, or -1.
	constexpr int row_at(int lx, int ly, int first, int count) {
		if (lx < text_x || lx >= text_x + row_w || ly < row_y0 || ly >= row_y0 + rows * row_h) {
			return -1;
		}
		const int index = first + (ly - row_y0) / row_h;
		return index < count ? index : -1;
	}

	// Enlargement for the preview: the largest k <= 4 that fits, at least 1.
	constexpr int preview_scale(int w, int h, int box_w, int box_h) {
		for (int k = 4; k > 1; k--) {
			if (w * k <= box_w - 8 && h * k <= box_h) {
				return k;
			}
		}
		return 1;
	}
}    // namespace Container_list_layout

#endif
