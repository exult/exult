/*
Copyright (C) 2025 The Exult Team

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

#ifndef SCROLLABLE_WIDGET_H
#define SCROLLABLE_WIDGET_H

#include "IterableGump_widget.h"

// A widget that can verically scroll it's contents
class Scrollable_widget : public Gump_widget {
	int width;
	int height;
	int border_size;

	std::vector<std::shared_ptr<Gump_widget>> children;

	class Scrolling_pane : public IterableGump_widget<
								   std::vector<std::shared_ptr<Gump_widget>>> {
	public:
		std::vector<std::shared_ptr<Gump_widget>> children;

		Scrolling_pane(Gump_Base* parent, int x, int y);

		// Inherited via IterableGump_widget
		iterator begin() override {
			return children.begin();
		}

		iterator end() override {
			return children.end();
		}

		short& scroll_offset;

		TileRect get_rect() const override {
			auto p = static_cast<Scrollable_widget*>(parent);
			return p->GetUsableArea(true);
		}
	};

	std::shared_ptr<Scrolling_pane> pane;

public:
	enum class ScrollbarType {
		None,
		Auto,
		Always,
	};

	Scrollable_widget(
			Gump_Base* parent, int x, int y, int inner_width, int inner_height,
			int border_size, ScrollbarType type, bool nopagebuttons,
			int scrollbg);

	TileRect get_rect() const override {
		TileRect rect(0, 0, width, height);
		local_to_screen(rect.x, rect.y);
		return rect;
	}

	void add_child(std::shared_ptr<Gump_widget>&& child) {
		child->set_parent(pane.get());
		pane->children.push_back(std::move(child));
		arrange_children();
	}

	IterableGump_widget<std::vector<std::shared_ptr<Gump_widget>>>&
			get_children_iterable() {
		return *pane;
	}

	bool run() override;

	Gump_widget* Input_first() override {
		Gump_widget* found = Gump_widget::Input_first();
		if (sort_order == Sort_Order::hidden) {
			return nullptr;
		}
		if (!found) {
			for (auto& child : children) {
				if (!child) {
					continue;
				}
				found = child->Input_first();
				if (found) {
					break;
				}
			}
		}

		return found;
	}

	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;

	void paint() override;

	// void paintSorted(
	//		Sort_Order sort, Sort_Order& next, Sort_Order& highest) override;
	// Scrolling support
private:
	ScrollbarType type;

	int  line_height = 12;
	bool snap        = false;
	enum class HAlign {
		none,
		left,
		centre,
		right
	} halign = HAlign::none;

	ShapeID  slider_shape;
	short    sliderw;    // Width of Slider
	short    sliderh;    // Height of Slider
	TileRect scrollrect;
	int      drag_start = -1;    // Pixel (v) where a slide started
	int      scroll_max = 0;

	int scrollbg;

	enum button_ids {
		id_first        = 0,
		id_page_up      = id_first,
		id_first_button = id_page_up,
		id_line_up,
		id_line_down,
		id_page_down,
		id_last_button = id_page_down,
		id_scrolling,
		id_count,
		id_button_count = id_scrolling - id_first_button,
	};

public:
	int GetScrollMax() const {
		return scroll_max;
	}

	void scroll_line(int dir);    // Scroll Line Button Pressed
	void scroll_page(int dir);    // Scroll Page Button Pressed.

	void line_up() {
		scroll_line(-1);
	}

	void line_down() {
		scroll_line(1);
	}

	void page_up() {
		scroll_page(-1);
	}

	void page_down() {
		scroll_page(1);
	}

	int get_scroll_offset() {
		return -pane->scroll_offset;
	}

	void set_scroll_offset(int newpos, bool ignore_snap = false);
	void set_line_height(int newsize, bool snap);

	TileRect GetUsableArea(bool screencoords = false) const {
		int x = border_size, y = border_size;
		if (screencoords) {
			Gump_widget::local_to_screen(x, y);
		}

		return TileRect(
				x, y, scrollrect.x - 2 * border_size, height - 2 * border_size);
	}

	int Get_ChildHeight() const;

	bool scroll_enabled() {
		return children[id_line_up]
			   && children[id_line_up]->sort_order != Sort_Order::hidden;
	}

	// Vertically stack children with given halign
	bool arrange_children();

	void expand(int deltax, int deltay);
};

#endif
