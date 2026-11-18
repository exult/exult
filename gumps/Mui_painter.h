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

#ifndef MUI_PAINTER_H
#define MUI_PAINTER_H

#include "Mui_theme.h"
#include "rect.h"

#include <string>
#include <vector>

class Image_buffer8;
class Mui_text_font;
class Shape_frame;

enum class Mui_icon {
	check,
	arrow_left,
	arrow_right,
	arrow_up,
	arrow_down,
	chevron_down,
	close,
	search,
	plus,
	minus
};

enum class Mui_align {
	left,
	centre,
	right
};

/*
 *  Drawing primitives for the themed menus, painting palette indices into
 *  an 8-bit layer buffer at display resolution. Every primitive clips to
 *  the current clip rectangle.
 */
class Mui_painter {
public:
	Mui_painter(Image_buffer8* buf, const Mui_theme& theme, float scale);

	Image_buffer8* get_buf() const {
		return buf;
	}

	const Mui_theme& get_theme() const {
		return theme;
	}

	float get_scale() const {
		return scale;
	}

	// Theme metric (reference pixels) to layer pixels, at least 1.
	int px(float ref) const;

	uint8 col(Mui_colour c) const {
		return theme.colour(c);
	}

	const Mui_text_font* font(Mui_font_role role) const;

	// Clipping (intersected with the current clip); pop restores.
	void     push_clip(const TileRect& r);
	void     pop_clip();
	TileRect get_clip() const;

	void fill_rect(const TileRect& r, uint8 c);
	void frame_rect(const TileRect& r, int n, uint8 c);
	void hline(int x, int y, int w, uint8 c);
	void vline(int x, int y, int h, uint8 c);
	void line(int x0, int y0, int x1, int y1, int thick, uint8 c);
	void rounded_fill(const TileRect& r, int radius, uint8 c);
	void rounded_frame(const TileRect& r, int radius, int n, uint8 c);
	// Light top/left edges, dark bottom/right.
	void bevel(const TileRect& r, int n, uint8 light, uint8 dark);
	// Deterministic dithered textures mixing theme colours.
	void texture_parchment(const TileRect& r, uint8 base, uint8 light, uint8 dark);
	void texture_stone(const TileRect& r, uint8 base, uint8 light, uint8 dark, uint8 mortar);
	void divider(int x, int y, int w);
	void icon(Mui_icon which, const TileRect& r, uint8 c);
	void focus_ring(const TileRect& r, int radius);
	// Nearest-neighbour scale a shape frame into r, keeping its aspect.
	void blit_shape(Shape_frame* frame, const TileRect& r, bool keep_aspect = true);
	// Nearest-neighbour scale an 8-bit image into r (transparent index skipped).
	void blit_scaled(const unsigned char* pixels, int w, int h, int pitch, const TileRect& r, int transparent = -1);

	// Text. Returns the width drawn.
	int text(const std::string& s, int x, int y, Mui_font_role role, Mui_colour fg, Mui_colour bg);
	int text_idx(const std::string& s, int x, int y, Mui_font_role role, uint8 fg, uint8 bg);
	// Single line in r, vertically centred, elided to fit.
	void text_in(const std::string& s, const TileRect& r, Mui_font_role role, Mui_colour fg, Mui_colour bg, Mui_align align);
	void text_in_idx(const std::string& s, const TileRect& r, Mui_font_role role, uint8 fg, uint8 bg, Mui_align align);
	int  text_width(const std::string& s, Mui_font_role role) const;
	int  line_height(Mui_font_role role) const;

private:
	Image_buffer8*        buf;
	const Mui_theme&      theme;
	float                 scale;
	std::vector<TileRect> clips;
	void                  apply_clip();
	void                  span(int x0, int x1, int y, uint8 c);
	void                  disc(int cx, int cy, int r, uint8 c);
};

#endif
