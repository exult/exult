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

#ifndef ENABLED_BUTTON_H
#define ENABLED_BUTTON_H

#include "Text_button.h"

#include <array>
#include <string>

class Enabled_button : public Text_button {
public:
	Enabled_button(
			Gump* par, int selectionnum, int px, int py, int width,
			int height = 0);

	bool push(MouseButton button) override;
	void unpush(MouseButton button) override;
	bool activate(MouseButton button) override;

	int getselection() const override {
		return get_framenum();
	}

	virtual void toggle(int state) = 0;

protected:
	std::array<const char*, 2> selections;
};

template <typename Parent>
class CallbackEnabledButton : public Enabled_button {
public:
	using CallbackType = void (Parent::*)(int state);

	template <typename... Ts>
	CallbackEnabledButton(Parent* par, CallbackType&& callback, Ts&&... args)
			: Enabled_button(par, std::forward<Ts>(args)...), parent(par),
			  on_toggle(std::forward<CallbackType>(callback)) {}

	void toggle(int state) override {
		(parent->*on_toggle)(state);
		parent->paint();
	}

private:
	Parent*      parent;
	CallbackType on_toggle;
};

#endif
