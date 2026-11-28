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

#ifndef DYNAMIC_LIST_LAYOUT_H
#define DYNAMIC_LIST_LAYOUT_H

#include <algorithm>
#include <string>

/*
 *  Row arithmetic of Dynamic_list_widget.  Kept free of engine types so
 *  it can be tested on its own.  Rows are numbered from 0; 'first' is the
 *  first visible row and 'rows' the number of visible rows.
 */
namespace Dynamic_list_layout {
	// First visible row, kept in [0, max(0, count - rows)].
	constexpr int clamp_first(int first, int count, int rows) {
		return std::clamp(first, 0, std::max(0, count - rows));
	}

	// Smallest change to 'first' that shows row 'sel'.
	constexpr int first_showing(int first, int sel, int count, int rows) {
		if (sel >= 0) {
			if (sel < first) {
				first = sel;
			} else if (sel >= first + rows) {
				first = sel - rows + 1;
			}
		}
		return clamp_first(first, count, rows);
	}

	// Row index under a local point, or -1.  The rows start at (x, y),
	// each 'width' by 'row_h' pixels.
	constexpr int row_at(int lx, int ly, int x, int y, int width, int rows, int row_h, int first, int count) {
		if (lx < x || lx >= x + width || ly < y || ly >= y + rows * row_h) {
			return -1;
		}
		const int index = first + (ly - y) / row_h;
		return index < count ? index : -1;
	}

	/*
	 *  Shorten a string to fit w pixels, as measured by width(string):
	 *  the whole string if it fits; else the longest run of whole words
	 *  that fits with "..."; else (one long word) the longest prefix that
	 *  fits with "..."; else the longest prefix that fits.
	 */
	template <typename Width>
	std::string fit_words(const std::string& s, int w, Width width) {
		if (s.empty() || width(s) <= w) {
			return s;
		}
		const std::string dots = "...";
		// Whole words: cut at each space, longest first.
		for (size_t end = s.rfind(' '); end != std::string::npos && end > 0; end = s.rfind(' ', end - 1)) {
			size_t len = end;
			while (len > 0 && s[len - 1] == ' ') {
				len--;
			}
			if (len > 0) {
				const std::string out = s.substr(0, len) + dots;
				if (width(out) <= w) {
					return out;
				}
			}
		}
		// Part of the first word.
		for (size_t len = s.size() - 1; len > 0; len--) {
			const std::string out = s.substr(0, len) + dots;
			if (width(out) <= w) {
				return out;
			}
		}
		// Not even one letter and the dots.
		std::string out = s;
		while (!out.empty() && width(out) > w) {
			out.pop_back();
		}
		return out;
	}

	// Enlargement for a boxed preview: the largest k <= 4 that fits, at least 1.
	constexpr int preview_scale(int w, int h, int box_w, int box_h) {
		for (int k = 4; k > 1; k--) {
			if (w * k <= box_w - 8 && h * k <= box_h) {
				return k;
			}
		}
		return 1;
	}

	/*
	 *  Keyboard navigation: the nearest enabled row from 'index' moving
	 *  'dir' rows at a time (dir may be a page).  Moving past either end
	 *  stops at the last enabled row in that direction.  index < 0 (no
	 *  selection) starts before the first row going down and after the
	 *  last going up.  Returns -1 when no row is enabled.
	 */
	template <typename Disabled>
	int next_enabled(int index, int dir, int count, Disabled is_disabled) {
		if (count <= 0 || dir == 0) {
			return index >= 0 && index < count && !is_disabled(index) ? index : -1;
		}
		if (index < 0) {
			index = dir > 0 ? -1 : count;
		}
		const int step   = dir > 0 ? 1 : -1;
		int       target = std::clamp(index + dir, 0, count - 1);
		// Look on from the target in the direction of travel ...
		for (int i = target; i >= 0 && i < count; i += step) {
			if (!is_disabled(i)) {
				return i;
			}
		}
		// ... then back towards where we started.
		for (int i = target - step; i >= 0 && i < count; i -= step) {
			if (!is_disabled(i)) {
				return i;
			}
		}
		return -1;
	}
}    // namespace Dynamic_list_layout

#endif
