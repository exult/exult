/*
Copyright (C) 2000-2024 The Exult Team

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

#include "exceptions.h"
#include "ignore_unused_variable_warning.h"
#include "rect.h"
#include "shapeid.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL_keycode.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <vector>

class Checkmark_button;
class Container_game_object;
class Game_object;
class Game_window;
class Gump_button;
class Gump_manager;
class Gump_widget;

// Base Class shared by Gumps and Widgetss
class Gump_Base : public ShapeID, public Paintable {
public:
	Gump_Base(int shnum, int frnum, ShapeFile shfile)
			: ShapeID(shnum, frnum, shfile) {}

	virtual int  get_x() const               = 0;
	virtual int  get_y() const               = 0;
	virtual void set_pos(int newx, int newy) = 0;

	enum class MouseButton {
		Unknown = 0,
		Left    = 1,
		Middle  = 2,
		Right   = 3,
	};
	//! Get button at point in screenspace
	virtual Gump_button* on_button(int mx, int my) = 0;

	//! Transform point from screen space to local
	virtual void screen_to_local(int& sx, int& sy) const = 0;

	//! Transform point deom local to screen space
	virtual void local_to_screen(int& sx, int& sy) const = 0;

	// Basic Input events These all return true if the event was handled
	// coords are all in screen space

	virtual bool mouse_down(int mx, int my, MouseButton button) {
		ignore_unused_variable_warning(mx, my, button);
		return false;
	}

	virtual bool mouse_up(int mx, int my, MouseButton button) {
		ignore_unused_variable_warning(mx, my, button);
		return false;
	}

	virtual bool mousewheel_down(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
		return false;
	}

	virtual bool mousewheel_up(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
		return false;
	}

	virtual bool mouse_drag(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
		return false;
	}

	virtual bool key_down(
			SDL_Keycode chr,
			SDL_Keycode unicode) {    // Key pressed
		ignore_unused_variable_warning(chr, unicode);
		return false;
	}

	virtual bool text_input(const char* text) {    // complete string input
		ignore_unused_variable_warning(text);
		return false;
	}

	virtual bool is_draggable() const = 0;

	virtual Container_game_object* get_container() {
		return nullptr;
	}

	virtual void close() {}

	virtual bool run() {
		return false;
	}
};

/*
 *  A gump contains an image of an open container from "gumps.vga".
 */
class Gump : nonreplicatable, public Gump_Base {
protected:
	Gump() = delete;
	Container_game_object* container;    // What this gump shows.
	int                    x, y;         // Location on screen.
	TileRect object_area{};              // Area to paint objects in, rel. to
	using Gump_elems = std::vector<Gump_widget*>;
	Gump_elems elems;          // Includes 'checkmark'.
	bool       handles_kbd;    // Kbd can be handled by gump.
	void       set_object_area(
				  TileRect area, int checkx, int checky, bool set_check = true);

	void set_object_area(const TileRect& area) {
		set_object_area(area, 0, 0, false);
	}

	void add_elem(Gump_widget* w) {
		elems.push_back(w);
	}

public:
	friend class Gump_model;
	Gump(Container_game_object* cont, int initx, int inity, int shnum,
		 ShapeFile shfile = SF_GUMPS_VGA);
	// Create centered.
	Gump(Container_game_object* cont, int shnum,
		 ShapeFile shfile = SF_GUMPS_VGA);
	// Clone.
	Gump(Container_game_object* cont, int initx, int inity, Gump* from);
	~Gump() override;

	virtual Gump* clone(Container_game_object* obj, int initx, int inity) {
		ignore_unused_variable_warning(obj, initx, inity);
		return nullptr;
	}

	int get_x() const override {    // Get coords.
		return x;
	}

	int get_y() const override {
		return y;
	}

	void set_pos(int newx, int newy) override;

	void set_pos();    // Set centered.

	Container_game_object* get_container() override {
		return container;
	}

	virtual Container_game_object* find_actor(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
		return nullptr;
	}

	bool can_handle_kbd() const {
		return handles_kbd;
	}

	inline Container_game_object* get_cont_or_actor(int mx, int my) {
		Container_game_object* ret = find_actor(mx, my);
		if (ret) {
			return ret;
		}
		return get_container();
	}

	// Get screen rect. of obj. in here.
	TileRect get_shape_rect(const Game_object* obj) const;
	// Get screen loc. of object.
	void get_shape_location(const Game_object* obj, int& ox, int& oy) const;
	// Find obj. containing mouse point.
	virtual Game_object* find_object(int mx, int my);
	virtual TileRect     get_dirty();    // Get dirty rect. for gump+contents.
	virtual Game_object* get_owner();    // Get object this belongs to.
	// Is a given point on a button?
	Gump_button* on_button(int mx, int my) override;
	// Paint button.
	virtual bool add(
			Game_object* obj, int mx = -1, int my = -1, int sx = -1,
			int sy = -1, bool dont_check = false, bool combine = false);
	virtual void remove(Game_object* obj);
	// Paint it and its contents.
	virtual void paint_elems();
	void         paint() override;
	// Close (and delete).
	void close() override;

	// update the gump, if required
	virtual void update_gump() {}

	// Can be dragged with mouse
	bool is_draggable() const override {
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

	virtual bool     has_point(int x, int y) const;
	virtual TileRect get_rect() const;

	// Is the gump partially or completely off screen
	// Only check shape rectangle, not against the actual shape
	bool isOffscreen(bool partially = true) const;

	void screen_to_local(int& sx, int& sy) const override {
		sx -= x;
		sy -= y;
	}

	void local_to_screen(int& sx, int& sy) const override {
		sx += x;
		sy += y;
	}

	virtual bool handle_kbd_event(void* ev) {
		ignore_unused_variable_warning(ev);
		return false;
	}
};

/*
 *  A generic gump used by generic containers:
 */
class Container_gump : public Gump {
	void initialize(int shnum);    // Initialize object_area.

public:
	Container_gump(
			Container_game_object* cont, int initx, int inity, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA)
			: Gump(cont, initx, inity, shnum, shfile) {
		initialize(shnum);
	}

	// Create centered.
	Container_gump(
			Container_game_object* cont, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA)
			: Gump(cont, shnum, shfile) {
		initialize(shnum);
	}

	// This one is for cloning.
	Container_gump(
			Container_game_object* cont, int initx, int inity, Gump* from)
			: Gump(cont, initx, inity, this) {
		ignore_unused_variable_warning(from);
	}

	Gump* clone(Container_game_object* cont, int initx, int inity) override {
		return new Container_gump(cont, initx, inity, this);
	}
};

#endif
