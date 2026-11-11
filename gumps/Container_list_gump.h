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

#ifndef CONTAINER_LIST_GUMP_H
#define CONTAINER_LIST_GUMP_H

#include "Gump.h"
#include "imagebuf.h"

#include <string>
#include <vector>

class Shape_frame;

/*
 *  The list view of a container: the scroll gump, widened at run time,
 *  with one row per item on the left and the selected item enlarged on
 *  the right.
 *
 *  The gump keeps the container's own gump shape as its ShapeID, so
 *  Gump_manager and usecode still find it by that shape, and paints the
 *  scroll from a separate ShapeID.  Objects' tx/ty are left alone, so the
 *  picture view keeps its arrangement.
 */
class Container_list_gump : public Gump {
public:
	Container_list_gump(Container_game_object* cont, int initx, int inity, int shnum);

	// Is the scroll art the frame this layout was made for?
	static bool is_available();

	Gump* clone(Container_game_object* cont, int initx, int inity) override;

	TileRect get_rect() const override;
	bool     has_point(int sx, int sy) const override;
	TileRect get_dirty() override;

	TileRect     get_shape_rect(const Game_object* obj) const override;
	void         get_shape_location(const Game_object* obj, int& ox, int& oy) const override;
	Game_object* find_object(int mx, int my) override;
	void         object_pressed(Game_object* obj) override;

	bool add(
			Game_object* obj, int mx = -1, int my = -1, int sx = -1, int sy = -1, bool dont_check = false,
			bool combine = false) override;
	void remove(Game_object* obj) override;
	void paint() override;

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	void line_up();
	void line_down();
	void page_up();
	void page_down();

	// For the scroll track.
	int get_first() const {
		return first;
	}

	int  get_count();
	void scroll_to(int newfirst);
	// Darken a screen rectangle to 70%, 85% or 95%.
	void paint_shade(int w, int h, int sx, int sy, int pct);

private:
	ShapeID                   scroll;    // The scroll art.
	std::vector<Game_object*> items;     // Shown objects, in order.
	// Selected object.  Only dereferenced once it is found in items.
	Game_object*  selected = nullptr;
	int           first    = 0;    // First visible row.
	int           sel_hint = 0;    // Row to select if 'selected' goes away.
	Xform_palette shade70{};
	Xform_palette shade85{};
	Xform_palette shade95{};
	int           shade_pal = -1;    // Palette the shades were made for.

	void collect_rows();
	int  index_of(const Game_object* obj) const;
	int  selected_index() const;
	void select(Game_object* obj);
	void update_shades();
	void paint_scroll();
	void paint_list();
	void paint_panel();
	void paint_enlarged(Game_object* obj, int bx, int by, int bw, int bh);

	static std::string display_name(const Game_object* obj);
	static std::string fit_text(int fontnum, const std::string& s, int w);
};

#endif
