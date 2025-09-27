/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef GUMP_TOGGLEBUTTON_H
#define GUMP_TOGGLEBUTTON_H

#include "Gump_button.h"
#include "Text_button.h"

#include <string>
#include <vector>

/*
 * A button that toggles shape when pushed
 */

class Gump_ToggleButton : public Gump_button {
public:
	Gump_ToggleButton(
			Gump* par, int px, int py, int shapenum, int selectionnum,
			int numsel)
			: Gump_button(par, shapenum, px, py, SF_EXULT_FLX),
			  numselections(numsel) {
		set_frame(2 * selectionnum);
	}

	bool push(MouseButton button) override;
	void unpush(MouseButton button) override;
	bool activate(MouseButton button) override;

	int getselection() const override {
		return get_framenum() / 2;
	}

	virtual void toggle(int state) = 0;

private:
	int numselections;
};

/*
 * A text button that toggles shape when pushed
 */

class Gump_ToggleTextButton : public Text_button {
public:
	Gump_ToggleTextButton(
			Gump_Base* par, const std::vector<std::string>& s, int selectionnum,
			int px, int py, int width, int height = 0)
			: Text_button(par, "", px, py, width, height),
			  selections(s) {
		set_frame(selectionnum);

		// call init for all of the strings to ensure the widget is wide enough
		// for all of them
		for (auto& selection : selections) {
			text = selection;
			init();
		}

		// Set the text to the actual default selection
		text = selections[selectionnum];
		init();
	}

	Gump_ToggleTextButton(
			Gump_Base* par, std::vector<std::string>&& s, int selectionnum,
			int px, int py, int width, int height = 0)
			: Text_button(par, "", px, py, width, height),
			  selections(std::move(s)) {
		set_frame(selectionnum);
		// call init for all of the strings to ensure the widget is wide enough
		// for all of them
		for (auto& selection : selections) {
			text = selection;
			init();
		}
		text = selections[selectionnum];
		init();
	}

	bool push(MouseButton button) override;
	void unpush(MouseButton button) override;
	bool activate(MouseButton button) override;

	int getselection() const override {
		return get_framenum();
	}

	void setselection(int newsel) override;

	virtual void toggle(int state) = 0;

private:
	std::vector<std::string> selections;
};

template <typename Parent>
class CallbackToggleTextButton : public Gump_ToggleTextButton {
public:
	using CallbackType  = void (Parent::*)(int state);
	using CallbackType2 = void (Parent::*)(Gump_widget*);

	template <typename... Ts>
	CallbackToggleTextButton(Parent* par, CallbackType&& callback, Ts&&... args)
			: Gump_ToggleTextButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle(std::forward<CallbackType>(callback)) {}

	template <typename... Ts>
	CallbackToggleTextButton(
			Parent* par, CallbackType2&& callback, Ts&&... args)
			: Gump_ToggleTextButton(par, std::forward<Ts>(args)...),
			  parent(par), on_toggle2(std::forward<CallbackType2>(callback)) {}

	void toggle(int state) override {
		if (on_toggle) {
			(parent->*on_toggle)(state);
		}
		if (on_toggle2) {
			(parent->*on_toggle2)(this);
		}
		parent->paint();
	}

private:
	Parent*       parent;
	CallbackType  on_toggle  = nullptr;
	CallbackType2 on_toggle2 = nullptr;
};

template <typename Parent>
class SelfManagedCallbackToggleTextButton
		: public CallbackToggleTextButton<Parent> {
public:
	template <typename... Ts>
	SelfManagedCallbackToggleTextButton(Ts&&... args)
			: CallbackToggleTextButton<Parent>(std::forward<Ts>(args)...) {
		Gump_button::set_self_managed(true);
	}
};
#endif
