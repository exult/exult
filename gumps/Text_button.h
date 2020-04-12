/*
Copyright (C) 2001-2013 The Exult Team

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

#ifndef TEXT_BUTTON_H
#define TEXT_BUTTON_H

#include "Gump_button.h"
#include <string>

class Text_button : public Gump_button {
protected:
	std::string text;
	int     text_x;
	int     text_y;
	int     width;
	int     height;

	void init();

public:
	Text_button(Gump *p, const std::string &str, int x, int y, int w = 0, int h = 0);
	void paint() override;

	bool on_widget(int mx, int my) const override;

	bool on_button(int mx, int my) const override {
		return on_widget(mx, my);
	}

};

template <typename Parent, typename... Args>
using CallbackTextButton = CallbackButtonBase<Parent, Text_button, Args...>;

#endif
