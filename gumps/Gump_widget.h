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

#ifndef GUMP_WIDGET_H
#define GUMP_WIDGET_H

#include "Gump.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"
#include "rect.h"
#include "shapeid.h"

class Gump;
class Game_window;
class Gump_button;

/*
 *  A gump widget, such as a button or text field:
 */
class Gump_widget : nonreplicatable, public Gump_Base {
protected:
	Gump_widget()     = default;
	Gump_Base* parent = nullptr;    // Who this is in.
	short      x, y;                // Coords. relative to parent.

public:
	friend class Gump;
	friend class Spellbook_gump;
	friend class Spellscroll_gump;

	Gump_widget(
			Gump_Base* par, int shnum, int px, int py, int frnum = 0,
			ShapeFile shfile = SF_GUMPS_VGA)
			: Gump_Base(shnum, frnum, shfile), parent(par), x(px), y(py) {}

	virtual Gump_widget* clone(Gump* par) {
		ignore_unused_variable_warning(par);
		return nullptr;
	}

	// Is a given point on the widget?
	virtual bool on_widget(int mx, int my) const;

	Gump_button* on_button(int mx, int my) override {
		ignore_unused_variable_warning(mx, my);
		return nullptr;
	}

	void paint() override;

	virtual TileRect get_rect();

	// update the widget, if required
	virtual void update_widget() {}

	bool is_draggable() const override {
		return true;
	}

	virtual Gump_button* as_button() {
		return nullptr;
	}

	// Gump_Base Implementation
	int get_x() const override {    // Get coords.
		return x;
	}

	int get_y() const override {
		return y;
	}

	void set_pos(int newx, int newy) override {
		x = newx;
		y = newy;
	}

	void screen_to_local(int& sx, int& sy) const override {
		sx -= x;
		sy -= y;
		if (parent) {
			parent->screen_to_local(sx, sy);
		}
	}

	void local_to_screen(int& sx, int& sy) const override {
		sx += x;
		sy += y;
		if (parent) {
			parent->local_to_screen(sx, sy);
		}
	}
};

#endif
