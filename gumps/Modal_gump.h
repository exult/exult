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

#ifndef MODAL_GUMP_H
#define MODAL_GUMP_H

#include "Gump.h"
#include "SDL_events.h"
#include "ignore_unused_variable_warning.h"

/*
 *  A modal gump object represents a 'dialog' that grabs the mouse until
 *  the user clicks okay.
 */
class Modal_gump : public Gump {

protected:
	bool done;          // true when user clicks checkmark.
	Gump_button *pushed;        // Button currently being pushed.

public:
	Modal_gump(Container_game_object *cont, int initx, int inity,
	           int shnum, ShapeFile shfile = SF_GUMPS_VGA)
		: Gump(cont, initx, inity, shnum, shfile), done(false),
		  pushed(nullptr)
	{  }
	// Create centered.
	Modal_gump(Container_game_object *cont, int shnum,
	           ShapeFile shfile = SF_GUMPS_VGA)
		: Gump(cont, shnum, shfile), done(false), pushed(nullptr)
	{  }
	bool is_done() {
		return done;
	}
	// Handle events:
	virtual bool mouse_down(int mx, int my, int button) = 0;
	virtual bool mouse_up(int mx, int my, int button) = 0;
	virtual void mousewheel_down() { }
	virtual void mousewheel_up() { }
	virtual void mouse_drag(int mx, int my) {
		ignore_unused_variable_warning(mx, my);
	}
	virtual void key_down(int chr) { // Key pressed
		ignore_unused_variable_warning(chr);
	}
	virtual void text_input(int chr, int unicode) { // Character typed (unicode)
		ignore_unused_variable_warning(chr, unicode);
	}
	virtual void text_input(const char *text) { // complete string input
		ignore_unused_variable_warning(text);
	}
	bool is_modal() const override {
		return true;
	}

	virtual bool run() {
		return false;
	}

};

#endif
