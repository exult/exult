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

#ifndef GUMP_H
#define GUMP_H

#include <vector>
#include "exceptions.h"
#include "rect.h"
#include "shapeid.h"
#include "ignore_unused_variable_warning.h"

class Checkmark_button;
class Container_game_object;
class Game_object;
class Game_window;
class Gump_button;
class Gump_manager;
class Gump_widget;

/*
 *  A gump contains an image of an open container from "gumps.vga".
 */
class Gump : nonreplicatable, public ShapeID, public Paintable {

protected:
	Gump() = default;
	Container_game_object *container;// What this gump shows.
	int x, y;           // Location on screen.
	Rectangle object_area{};      // Area to paint objects in, rel. to
	using Gump_elems = std::vector<Gump_widget *>;
	Gump_elems elems;       // Includes 'checkmark'.
	bool handles_kbd;       // Kbd can be handled by gump.
	void set_object_area(Rectangle const &area, int checkx, int checky);
	void set_object_area(Rectangle const &area) {
		object_area = area;
	}
	void add_elem(Gump_widget *w) {
		elems.push_back(w);
	}
public:
	friend class Gump_model;
	Gump(Container_game_object *cont, int initx, int inity, int shnum,
	     ShapeFile shfile = SF_GUMPS_VGA);
	// Create centered.
	Gump(Container_game_object *cont, int shnum,
	     ShapeFile shfile = SF_GUMPS_VGA);
	// Clone.
	Gump(Container_game_object *cont, int initx, int inity, Gump *from);
	~Gump() override;
	virtual Gump *clone(Container_game_object *obj, int initx, int inity) {
		ignore_unused_variable_warning(obj, initx, inity);
		return nullptr;
	}
	int get_x() {       // Get coords.
		return x;
	}
	int get_y() {
		return y;
	}
	void set_pos(int newx, int newy) { // Set new spot on screen.
		x = newx;
		y = newy;
	}
	void set_pos();         // Set centered.
	Container_game_object *get_container() {
		return container;
	}
	virtual Container_game_object *find_actor(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
		return nullptr;
	}
	bool can_handle_kbd() const {
		return handles_kbd;
	}
	inline Container_game_object *get_cont_or_actor(int mx, int my) {
		Container_game_object *ret = find_actor(mx, my);
		if (ret) return ret;
		return get_container();
	}
	// Get screen rect. of obj. in here.
	Rectangle get_shape_rect(const Game_object *obj) const;
	// Get screen loc. of object.
	void get_shape_location(const Game_object *obj, int &ox, int &oy) const;
	// Find obj. containing mouse point.
	virtual Game_object *find_object(int mx, int my);
	virtual Rectangle get_dirty();      // Get dirty rect. for gump+contents.
	virtual Game_object *get_owner();// Get object this belongs to.
	// Is a given point on a button?
	virtual Gump_button *on_button(int mx, int my);
	// Paint button.
	virtual bool add(Game_object *obj, int mx = -1, int my = -1,
	                int sx = -1, int sy = -1, bool dont_check = false,
	                bool combine = false);
	virtual void remove(Game_object *obj);
	// Paint it and its contents.
	void paint_elems();
	void paint() override;
	// Close (and delete).
	virtual void close();
	// update the gump, if required
	virtual void update_gump() { }
	// Can be dragged with mouse
	virtual bool is_draggable() const {
		return true;
	}
	// Close on end_gump_mode
	virtual bool is_persistent() const {
		return false;
	}
	virtual bool is_modal() const {
		return false;
	}
	// Show the hand cursor
	virtual bool no_handcursor() const {
		return false;
	}

	virtual bool has_point(int x, int y) const;
	virtual Rectangle get_rect() const;
	virtual bool handle_kbd_event(void *ev) {
		ignore_unused_variable_warning(ev);
		return false;
	}
};

/*
 *  A generic gump used by generic containers:
 */
class Container_gump : public Gump {

	void initialize(int shnum);     // Initialize object_area.

public:
	Container_gump(Container_game_object *cont, int initx, int inity,
	               int shnum, ShapeFile shfile = SF_GUMPS_VGA)
		: Gump(cont, initx, inity, shnum, shfile) {
		initialize(shnum);
	}
	// Create centered.
	Container_gump(Container_game_object *cont, int shnum,
	               ShapeFile shfile = SF_GUMPS_VGA)
		: Gump(cont, shnum, shfile) {
		initialize(shnum);
	}
	// This one is for cloning.
	Container_gump(Container_game_object *cont, int initx, int inity,
	               Gump *from)
		: Gump(cont, initx, inity, this) {
		ignore_unused_variable_warning(from);
	}
	Gump *clone(Container_game_object *cont, int initx, int inity) override {
		return new Container_gump(cont, initx, inity, this);
	}
};

#endif
