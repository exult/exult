/*
Copyright (C) 2001-2024 The Exult Team

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

#ifndef FACE_BUTTON_H
#define FACE_BUTTON_H

#include "Gump_button.h"

class Actor;
class Game_window;

/*
 *  A character's face, that will open inventory when clicked.
 */
class Face_button : public Gump_button {
protected:
	Actor* actor;    // Who this represents.
	bool   translucent;

public:
	Face_button(Gump* par, int px, int py, Actor* a);

	Actor* get_actor() {
		return actor;
	}

	void double_clicked(int x, int y) override;

	bool activate(MouseButton button) override {
		return button == MouseButton::Left;
	}

	void paint() override;

	bool push(MouseButton button) override {
		return button == MouseButton::Left;
	}

	void unpush(MouseButton) override {}

	void update_widget() override;
};

#endif
