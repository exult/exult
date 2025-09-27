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
	Gump_widget()     = delete;
	Gump_Base* parent = nullptr;    // Who this is in.
	short      x, y;                // Coords. relative to parent.

public:
	friend class Gump;
	friend class Spellbook_gump;
	friend class Spellscroll_gump;

	Gump_widget(
			Gump_Base* parent, int shnum, int px, int py, int frnum = 0,
			ShapeFile shfile = SF_GUMPS_VGA)
			: Gump_Base(shnum, frnum, shfile), parent(parent), x(px), y(py) {}

	virtual Gump_widget* clone(Gump* par) {
		ignore_unused_variable_warning(par);
		return nullptr;
	}

	void set_parent(Gump_Base* newpar) {
		parent = newpar;
	}

	// Is a given point on the widget?
	virtual bool on_widget(int mx, int my) const;

	Gump_button* on_button(int mx, int my) override {
		ignore_unused_variable_warning(mx, my);
		return nullptr;
	}

	// Widget that wants first chance at input handling
	// Should call on all children.
	// A widget should only return themself if they are
	// in a state where they need to accept all input even if not directly
	// directed to it. This allows a widget to detect when a user clicks away
	// from it and also to implement focused keyboard input.
	virtual Gump_widget* Input_first() {
		return nullptr;
	}

	void paint() override;

	virtual TileRect get_rect() const;

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

	void set_pos(int newx, int newy) override;

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

	/// For a widget that has a value or selectable state, get the current value
	/// or selected state
	virtual int getselection() const {
		return 0;
	}

	/// For a widget that has a value or selectable state, set the current value
	/// or selected state
	virtual void setselection(int newsel) {
		ignore_unused_variable_warning(newsel);
	}

	//
	// Sorted Painting
	//
	// When using sorted painting Paint() should not paint children
	//
public:
	enum class Sort_Order {
		hidden = -3,    // Anything with this sort will be skipped
		lowest = -2,
		low    = -1,
		normal = 0,
		high   = 1,
		ontop  = 2,    // This is rendered last and is allowed to rnder outsuide
					   // of all parent bounds
	} sort_order = Sort_Order::normal;

	/// Paint a range of widgets and their children using their sort_order
	/// Worst case complexity O(n^2) if every widget and child has a different
	/// sort_order but paint will only ever be called once per widget so even at
	/// the worst case it shouldn't be too slow
	/// \tparam T container type. Must be container of pointer like objects of Gump_widget
	template <typename T>
	static void paintSorted(T& container) {
		Sort_Order next;
		Sort_Order highest = Sort_Order::ontop;
		for (int s = int(Sort_Order::lowest); s <= int(highest);
			 s     = int(next)) {
			next = Sort_Order(s + 1);
			for (auto& widget : container) {
				if (widget) {
					widget->paintSorted(Sort_Order(s), next, highest);
				}
			}
		}
	}

	template <typename T>
	static Gump_widget* findSorted(
			int sx, int sy, T& container, Gump_widget* before) {
		Sort_Order next;
		Sort_Order highest = Sort_Order::ontop;
		for (int s = int(Sort_Order::lowest); s <= int(highest);
			 s     = int(next)) {
			next = Sort_Order(s + 1);
			for (auto& child : container) {
				if (!child) {
					continue;
				}	
				if (&*child == before) {
					break;
				}

				Gump_widget* found = child->findSorted(
						sx, sy, Sort_Order(s), next, highest, before);
				if (found) {
					return found;
				}
			}
		}

		return nullptr;
	}

	/// Paint self if our sort order matches and call call on children
	/// Also gather next and highest sort orders
	/// see IterableGump_widget::paintSorted() for example
	virtual void paintSorted(
			Sort_Order sort, Sort_Order& next, Sort_Order& highest);

	// Rerturn self if we have the point and our sort order matches the input,
	// if wedon't match call on our children
	/// Also gather next and highest sort orders
	/// see IterableGump_widget::findSorted() for example
	virtual Gump_widget* findSorted(
			int sx, int sy, Sort_Order sort, Sort_Order& next,
			Sort_Order& highest, Gump_widget* before);
};

#endif
