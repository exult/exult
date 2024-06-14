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

#ifndef MODAL_GUMP_H
#define MODAL_GUMP_H

#include "Gump.h"
#include "ignore_unused_variable_warning.h"

/*
 *  A modal gump object represents a 'dialog' that grabs the mouse until
 *  the user clicks okay.
 */
class Modal_gump : public Gump {
protected:
	bool         done;      // true when user clicks checkmark.
	Gump_button* pushed;    // Button currently being pushed.

public:
	Modal_gump(
			Container_game_object* cont, int initx, int inity, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA);

	// Create centered.
	Modal_gump(
			Container_game_object* cont, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA);

	bool is_done() {
		return done;
	}

	bool is_modal() const override {
		return true;
	}

	virtual bool run() {
		return false;
	}

	bool is_draggable() const override {
		return !no_dragging;
	}

	bool mouse_down(int mx, int my, MouseButton button) override;

	bool mouse_up(int mx, int my, MouseButton button) override;

	bool mouse_drag(int mx, int my) override;

public:
	enum class DragType {
		Unknown,
		Never,
		Always,
		Offscreen,
	};

	// Get the user set drag type from exult.cfg
	static DragType GetDragType();
	// Set new dragtype and update exult.cfg
	static void SetDragType(DragType newtype);

private:
	// the previous position of the mouse during a drag, INT_MIN if not dragging
	int drag_mx, drag_my;

	// the last known good position of the gump that it will be returned to if
	// the user drags it completely off screen only valid while dragging
	int lastgood_x, lastgood_y;

protected:
	// If set this gump cannot be dragged
	bool no_dragging;
};

#endif
