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
#include "gumpinf.h"

#include <memory>
#include <string>
#include <vector>

class Dynamic_list_widget;

/*
 *  The list view of a container: a background (by default the scroll
 *  gump, widened at run time) with a Dynamic_list_widget of the items on
 *  the left and the selected item enlarged on the right (a bound
 *  Dynamic_shape_widget), and the container's own picture hanging off
 *  the background's bottom-left.  One gump draws both, so they move,
 *  raise and close as one.  Where everything goes comes from the
 *  list_layout section of gump_info.txt (List_layout).
 *
 *  The gump keeps the container's own gump shape as its ShapeID, so
 *  Gump_manager and usecode still find it by that shape, and paints the
 *  background from a separate ShapeID.  x, y are the background's
 *  top-left; the picture's hotspot is at (x + pic_x, y + pic_y).
 *  object_area and the checkmark are the container's own, shifted by
 *  that offset, so the base class code places, paints, hit-tests and
 *  drops the picture's objects as the picture view does.  Objects'
 *  tx/ty are the state both views share.
 */
class Container_list_gump : public Gump {
public:
	Container_list_gump(Container_game_object* cont, int initx, int inity, int shnum);
	~Container_list_gump() override;

	// Can the list view show a container with this gump shape?  Not if
	// its layout is off, or the background is not the frame the layout
	// was made for.
	static bool is_available(int shnum);

	Gump* clone(Container_game_object* cont, int initx, int inity) override;

	TileRect get_rect() const override;
	bool     has_point(int sx, int sy) const override;
	TileRect get_dirty() override;

	TileRect     get_shape_rect(const Game_object* obj) const override;
	void         get_shape_location(const Game_object* obj, int& ox, int& oy) const override;
	Game_object* find_object(int mx, int my) override;
	void         object_pressed(Game_object* obj) override;
	bool add(Game_object* obj, int mx = -1, int my = -1, int sx = -1, int sy = -1, bool dont_check = false, bool combine = false)
			override;
	void remove(Game_object* obj) override;

	void paint() override;
	void update_gump() override;

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	// Step through the containers inside the one the gump was opened for.
	void prev_container();
	void next_container();

	// Usecode to run when the view opens (-1 = none).
	int get_open_fn() const {
		return layout.open_fn;
	}

private:
	List_layout          layout;            // Chosen when the view opens.
	ShapeID              background;        // The background art.
	Dynamic_list_widget* list = nullptr;    // The rows (in elems).
	// The container's picture: its hotspot, rel. to x, y.
	int               pic_x = 0;
	int               pic_y = 0;
	Checkmark_button* check = nullptr;    // On the picture.
	// Object last found in the picture by find_object(), or null.
	Game_object* picture_hit = nullptr;
	// The container the gump was opened for; the arrows step through the
	// containers inside it.
	std::weak_ptr<Game_object> root;

	static int  background_shape(const List_layout& L);
	static bool can_step_into(Game_object* obj);
	static void collect_nested(Container_game_object* cont, std::vector<Container_game_object*>& out);
	void        step(int delta);
	void        show_container(Container_game_object* cont);
	void        set_picture();
	bool        on_picture(int sx, int sy) const;
	void        paint_background();
	void        paint_title();
	void        paint_panel();
};

#endif
