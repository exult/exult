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

#ifndef GUMP_BUTTON_H
#define GUMP_BUTTON_H

#include "Gump_widget.h"

#include <tuple>
#include <type_traits>

/*
 *  A pushable button on a gump:
 */
class Gump_button : public Gump_widget {
private:
	MouseButton pushed_button;    // MouseButton::Unknown if in unpushed state.

	bool self_managed;                              // Self managed button handles it's own input.
													// on_button will return nullptr if self managed
	MouseButton dragging = MouseButton::Unknown;    // Button beingg held while
													// dragging mouse

public:
	friend class Gump;

	Gump_button(Gump_Base* par, int shnum, int px, int py, ShapeFile shfile = SF_GUMPS_VGA, bool self_managed = false)
			: Gump_widget(par, shnum, px, py, 0, shfile), pushed_button(MouseButton::Unknown), self_managed(self_managed) {}

	// Only respond to this is we are not self managed
	Gump_button* on_button(int mx, int my) override {
		return !self_managed && on_widget(mx, my) ? this : nullptr;
	}

	// Want input focus if self managed and pushed or dragging
	Gump_widget* Input_first() override {
		return self_managed && (pushed_button != MouseButton::Unknown || dragging != MouseButton::Unknown) ? this : nullptr;
	}

	// What to do when 'clicked':
	virtual bool activate(MouseButton button);
	// Or double-clicked.
	virtual void double_clicked(int x, int y);
	virtual bool push(MouseButton button);    // Redisplay as pushed.
	virtual void unpush(MouseButton button);
	void         paint() override;

	// Self managed input handling
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;

	bool is_self_managed() const {
		return self_managed;
	}

	void set_self_managed(bool set) {
		self_managed = set;
	}

	MouseButton get_pushed() {
		return pushed_button;
	}

	bool is_pushed() const {
		return pushed_button != MouseButton::Unknown;
	}

	void set_pushed(MouseButton button);

	void set_pushed(bool set);

	virtual bool is_checkmark() const {
		return false;
	}

	Gump_button* as_button() override {
		return this;
	}
};

// A Basic Recolorable Button class to be used by Subclasses to draw a button in the style of Text_button
//
class Basic_button : public Gump_button {
public:
	uint8 OuterBorder;
	uint8 OuterBorderCorner;
	uint8 PushedEdgeCorner;
	uint8 PushedEdgeTop;
	uint8 PushedEdgeLeft;

	uint8 BevelHighlight;
	uint8 BevelLowlight;
	uint8 BevelCornerBoth;
	uint8 BevelDoubleHighlight;
	uint8 InnerBorderTRCorner;
	uint8 BevelDoubleLowlight;

	uint8 Background;
	uint8 BGHighlight;

	//! @brief Recolor the button
	//! @param newBackground Palette index of the background colour to derive all other colours from. Must have 4 palette indices
	//! available above this getting darker and 3 palette indices below getting lighter
	void Recolor(int newBackground = 140, int newOuterBorder = 0) {
		Background        = newBackground;
		OuterBorder       = newOuterBorder;
		OuterBorderCorner = 0xff;

		PushedEdgeCorner = newBackground + 4;
		PushedEdgeTop    = newBackground + 3;
		PushedEdgeLeft   = newBackground + 3;

		BevelHighlight       = newBackground - 2;
		BevelLowlight        = newBackground + 3;
		BevelCornerBoth      = newBackground + 1;
		BevelDoubleHighlight = newBackground - 3;
		InnerBorderTRCorner  = newBackground - 2;
		BevelDoubleLowlight  = newBackground + 4;

		BGHighlight = newBackground - 1;
	}

protected:
	int width;
	int height;

public:
	Basic_button(Gump_Base* par, int px, int py, int w, int h, bool self_managed = false)
			: Gump_button(par, -1, px, py, SF_OTHER, self_managed), width(w), height(h) {
		// need to set frame to -1 to disable the shape
		set_frame(-1);
		// Set the default color
		Recolor();
	}

	void paint() override;

	//! @brief  Get the area subclasses are allowed to draw into. The draw area is only the top surface of the button excluding the
	//! bevel
	//! @return draw area rectangle in local coordinates
	virtual TileRect get_draw_area(std::optional<bool> pushed = std::nullopt) const;

	TileRect get_rect() const override;
	bool     on_widget(int mx, int my) const override;
};

template <class Callable, class Tuple, size_t... Is>
inline auto call(Callable&& func, Tuple&& args, std::index_sequence<Is...>) {
	return func(std::get<Is>(args)...);
}

template <typename Parent, typename Base, typename... Args>
class CallbackButtonBase : public Base {
public:
	using CallbackType   = void (Parent::*)(Args...);
	using CallbackParams = std::tuple<Args...>;

	template <typename... Ts>
	CallbackButtonBase(Parent* par, CallbackType&& callback, CallbackParams&& params, Ts&&... args)
			: Base(par, std::forward<Ts>(args)...), parent(par), on_click(std::forward<CallbackType>(callback)),
			  parameters(std::forward<CallbackParams>(params)) {}

	bool activate(Gump_Base::MouseButton button) override {
		if (button != Gump_Base::MouseButton::Left) {
			return false;
		}
		call(
				[this](Args... args) {
					(parent->*on_click)(args...);
				},
				parameters, std::make_index_sequence<sizeof...(Args)>{});
		return true;
	}

private:
	Parent*        parent;
	CallbackType   on_click;
	CallbackParams parameters;
};

template <typename Parent, typename Base>
class CallbackButtonBase<Parent, Base> : public Base {
public:
	using CallbackType   = void (Parent::*)();
	using CallbackType2  = void (Parent::*)(Gump_widget* sender, Gump_Base::MouseButton button);
	using CallbackParams = std::tuple<>;

	template <typename... Ts>
	CallbackButtonBase(Parent* par, CallbackType&& callback, Ts&&... args)
			: Base(par, std::forward<Ts>(args)...), parent(par), on_click(std::forward<CallbackType>(callback)) {}

	// Construct with a callback that has arguments for sender and MouseButton
	template <typename... Ts>
	CallbackButtonBase(Parent* par, CallbackType2&& callback, Ts&&... args)
			: Base(par, std::forward<Ts>(args)...), parent(par), on_click2(std::forward<CallbackType2>(callback)) {}

	bool activate(Gump_Base::MouseButton button) override {
		if (on_click && button == Gump_Base::MouseButton::Left) {
			(parent->*on_click)();
			return true;
		} else if (on_click2) {
			(parent->*on_click2)(this, button);
			return true;
		}
		return false;
	}

private:
	Parent*       parent;
	CallbackType  on_click  = nullptr;
	CallbackType2 on_click2 = nullptr;
};

template <typename Parent, typename... Args>
using CallbackButton = CallbackButtonBase<Parent, Gump_button, Args...>;

// A wrapper class to make a button self managed
// A self managed button expects to handle it's own mouse button events and on_button will return
// nullptr
template <typename Base>
class SelfManaged : public Base {
public:
	template <typename... Ts>
	SelfManaged(Ts&&... args) : Base(std::forward<Ts>(args)...) {
		this->set_self_managed(true);
	}
};

#endif
