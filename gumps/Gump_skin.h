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

#ifndef GUMP_SKIN_H
#define GUMP_SKIN_H

#include "Mui_painter.h"
#include "Mui_theme.h"
#include "rect.h"

#include <memory>
#include <string>
#include <vector>

class Gump_info;
class Image_buffer8;
class Mui_text_font;
class ShapeID;

/*
 *  Which gumps are drawn with the menu theme (see docs/modern_ui.md,
 *  "Themed Dynamic Gumps").
 */
namespace Gump_theme {
	// The theme of the chosen menu style, or null when gumps are classic
	// (menu style classic, no game window, the Exult menu, or a theme that
	// does not load). The theme is loaded once per style name.
	std::shared_ptr<const Mui_theme> current();

	// Does a gump with this gump_info entry want the theme? gump_theme
	// mode 0 = never, 1 = always; else the list view does, and a dynamic
	// container gump does when it has a dynamic widget and no snap zones.
	bool wants(const Gump_info* info, bool list_view);
}    // namespace Gump_theme

/*
 *  A themed gump's skin: a display-resolution layer (UiLayerMenus) over
 *  the gump's game-resolution layer, at the same destination. The gump
 *  paints its panel, texts and controls here with Mui_painter, in game
 *  coordinates that are mapped to the skin; its art (objects, shapes,
 *  button art) is painted into the game layer as usual and composited on
 *  top. The game layer is then cleared, but stays, so input is unchanged.
 */
class Gump_skin {
public:
	Gump_skin() = default;
	~Gump_skin();
	Gump_skin(const Gump_skin&)            = delete;
	Gump_skin& operator=(const Gump_skin&) = delete;

	// Size and place the layer for a gump layer with game bounds b shown
	// at (dx, dy, dw, dh) on the display. False if there is no layer.
	bool begin(const Mui_theme& th, const TileRect& b, int dx, int dy, int dw, int dh, int z);
	// Composite the gump's art (its game-resolution layer buffer) on top.
	void compose(Image_buffer8* art);
	// Done painting: show the layer.
	void end();
	// Hide the layer (the gump has nothing to draw).
	void hide();

	// Game rectangle to skin pixels.
	TileRect to_skin(const TileRect& game) const;

	Mui_painter& painter() {
		return *paint;
	}

	const Mui_theme& theme() const {
		return *theme_ptr;
	}

	bool stone() const {
		return theme_ptr->frame_stone;
	}

	// The panel and frame, cached: if the cached base was made for this
	// theme, size, panel, tab and well, it is copied in and true returned;
	// else the caller paints them and calls store_base(). All rects are
	// game coordinates; tab and well may be empty.
	bool base_valid(const TileRect& panel_rect, const TileRect& tab_rect, const TileRect& well_rect);
	void store_base();

	// The panel the texts are elided to (game coordinates).
	const TileRect& get_panel() const {
		return panel_game;
	}

	// The close control's tab for the Ultima look: the checkmark's own
	// tab, else a 30x35 rect centred on the tick, inside the layer.
	TileRect tab_rect(const TileRect& check_rect, const TileRect& check_tab) const;

	// Helpers; all rectangles and points are game coordinates.
	// Ultima: stone ring with a gold rule round a parchment page, the tab
	// (if any) under it; Modern: a rounded window with a panel page.
	void panel(const TileRect& r, const TileRect& tab);
	// An inset well, for a container area.
	void well(const TileRect& r);
	void fill(const TileRect& r, Mui_colour c);
	// Text with its anchor at (ax, ay): halign 0 left, 1 right, 2 centre;
	// valign 0 top, 1 bottom, 2 centre. The size is the largest of the
	// theme face's sizes no taller than the classic font, scaled. Elided
	// to the space between the anchor and the edges of bounds (the panel
	// if null). Returns the width drawn, in game pixels.
	int text(
			const std::string& s, int ax, int ay, int halign, int valign, int classic_font, Mui_colour fg, Mui_colour bg,
			const TileRect* bounds = nullptr);
	// Plate behind a button's art, an arrow control, a groove and a knob.
	void plate(const TileRect& r, bool pushed);
	void arrow(const TileRect& r, Mui_icon which, bool pushed);
	void groove(const TileRect& r, bool vertical);
	void knob(const TileRect& r);
	// The close control on the checkmark's rect.
	void close_control(const TileRect& check_rect, bool pushed);

	// Opaque bounding box of a shape's frame, relative to its origin
	// (hotspot). Cached per shape file, shape and frame.
	static TileRect art_bounds(const ShapeID& sid);

private:
	struct Base_key {
		const Mui_theme* theme = nullptr;
		std::string      name;
		int              w = 0;
		int              h = 0;
		TileRect         panel{0, 0, 0, 0};
		TileRect         tab{0, 0, 0, 0};
		TileRect         well{0, 0, 0, 0};

		bool operator==(const Base_key& o) const {
			return theme == o.theme && name == o.name && w == o.w && h == o.h && panel == o.panel && tab == o.tab && well == o.well;
		}
	};

	int                          layer = -1;
	int                          w     = 0;
	int                          h     = 0;
	int                          ox    = 0;    // Game coords of the layer's (0, 0)
	int                          oy    = 0;
	float                        sx    = 1;    // Skin pixels per game pixel
	float                        sy    = 1;
	TileRect                     bounds{0, 0, 0, 0};    // Game bounds of the layer
	TileRect                     panel_game{0, 0, 0, 0};
	const Mui_theme*             theme_ptr = nullptr;
	Image_buffer8*               buf       = nullptr;
	std::unique_ptr<Mui_painter> paint;
	std::vector<unsigned char>   base;    // Cached panel, w * h
	Base_key                     base_key;
	Base_key                     pending_key;
	bool                         has_base = false;

	int                  gx(int x) const;    // Game to skin, one axis
	int                  gy(int y) const;
	const Mui_text_font* font_for(int classic_font) const;
};

#endif
