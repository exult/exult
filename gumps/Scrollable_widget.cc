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

#include "Scrollable_widget.h"

#include "Gump_button.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "span.h"

class Scrollablebutton : public CallbackButton<Scrollable_widget> {
public:
	template <typename... Ts>
	Scrollablebutton(Scrollable_widget* par, Ts&&... args)
			: CallbackButtonBase(par, std::forward<Ts>(args)...) {
		set_self_managed(true);
	}
};

Scrollable_widget::Scrollable_widget(
		Gump_Base* parent, int x, int y, int iw, int ih, int border_size,
		ScrollbarType type, bool nopagebuttons, int scrollbg)
		: Gump_widget(parent, -1, x, y), border_size(border_size), type(type),
		  slider_shape(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX),
		  sliderw(slider_shape.get_shape()->get_width()),
		  sliderh(slider_shape.get_shape()->get_height()), scrollbg(scrollbg) {
	int scrollbutton_x = iw + 2 * border_size;
	height             = ih + 2 * border_size;
	children.resize(id_count);
	// Scrolbar buttons.
	int ypos = 0;
	if (!nopagebuttons) {
		children[id_page_up] = std::make_shared<Scrollablebutton>(
				this, &Scrollable_widget::page_up, EXULT_FLX_SAV_UPUP_SHP,
				scrollbutton_x, 0, SF_EXULT_FLX);
		children[id_page_up]->set_pos(
				scrollbutton_x + children[id_page_up]->get_shape()->get_xleft(),
				ypos + children[id_page_up]->get_shape()->get_yabove());
		ypos += children[id_page_up]->get_shape()->get_height();
	}
	children[id_line_up] = std::make_shared<Scrollablebutton>(
			this, &Scrollable_widget::line_up, EXULT_FLX_SAV_UP_SHP,
			scrollbutton_x, ypos, SF_EXULT_FLX);
	children[id_line_up]->set_pos(
			scrollbutton_x + children[id_line_up]->get_shape()->get_xleft(),
			ypos + children[id_line_up]->get_shape()->get_yabove());
	int button_w       = children[id_line_up]->get_shape()->get_width();
	width              = scrollbutton_x + button_w;
	int scroll_start_x = scrollbutton_x + (button_w - sliderw) / 2;
	int scroll_start_y = ypos + children[id_line_up]->get_shape()->get_height();
	ypos               = height;
	if (!nopagebuttons) {
		children[id_page_down] = std::make_shared<Scrollablebutton>(
				this, &Scrollable_widget::page_down, EXULT_FLX_SAV_DOWNDOWN_SHP,
				scrollbutton_x, height, SF_EXULT_FLX);
		ypos = ypos - children[id_page_down]->get_shape()->get_height();
		children[id_page_down]->set_pos(
				scrollbutton_x
						+ children[id_page_down]->get_shape()->get_xleft(),
				ypos + children[id_page_down]->get_shape()->get_yabove());
	}

	children[id_line_down] = std::make_shared<Scrollablebutton>(
			this, &Scrollable_widget::line_down, EXULT_FLX_SAV_DOWN_SHP,
			scrollbutton_x, ypos, SF_EXULT_FLX);
	ypos = ypos - children[id_line_down]->get_shape()->get_height();

	children[id_line_down]->set_pos(
			scrollbutton_x + children[id_line_up]->get_shape()->get_xleft(),
			ypos + children[id_line_up]->get_shape()->get_yabove());
	children[id_scrolling] = pane
			= std::make_shared<Scrolling_pane>(this, border_size, 0);
	scrollrect = TileRect(
			scroll_start_x, scroll_start_y, sliderw, ypos - scroll_start_y);
	// Call run to update scrollbar state
	run();
}

bool Scrollable_widget::run() {
	bool repaint = arrange_children();

	// Work out how much scroling we cando
	scroll_max = std::max(0, Get_ChildHeight() - GetUsableArea().h);

	// Hide/show scrolbar elements as needed
	if ((scroll_max == 0 && type == ScrollbarType::Auto)
		|| type == ScrollbarType::None) {
		for (int i = id_first_button; i <= id_last_button; i++) {
			auto& button = children[i];
			if (button) {
				repaint |= std::exchange(button->sort_order, Sort_Order::hidden)
						   != Sort_Order::hidden;
			}
		}
		repaint |= std::exchange(pane->scroll_offset, 0) != 0;
		// Recalculate width for no scrollbar
		width = scrollrect.x;
		scrollrect.w = 0;
	} else {
		for (int i = id_first_button; i <= id_last_button; i++) {
			auto& button = children[i];
			if (button) {
				repaint |= std::exchange(button->sort_order, Sort_Order::normal)
						   != Sort_Order::normal;
			}
		}
		// recalculate width for scrollbar
		width = scrollrect.x + children[id_line_up]->get_shape()->get_width();
		scrollrect.w = sliderw;
	}
	for (auto& child : children) {
		if (child) {
			repaint |= child->run();
		}
	}

	// automatically update our sort order to match the  higest child
	Sort_Order n{}, h{};
	pane->findSorted(0, 0, Sort_Order::lowest, n, h, nullptr);
	repaint |= std::exchange(sort_order, h) != h;
	return repaint;
	;
}

bool Scrollable_widget::mouse_down(int mx, int my, MouseButton button) {
	// only if in ourbounds
	if (!get_rect().has_point(mx, my)) {
		return false;
	}
	Gump_widget* found = nullptr;
	while ((found = Gump_widget::findSorted(mx, my, children, found))) {
		if (found->mouse_down(mx, my, button)) {
			gwin->set_all_dirty();
			return true;
		}
	}
	screen_to_local(mx, my);
	// Check for scroller
	if (scroll_enabled() && mx >= scrollrect.x
		&& mx < scrollrect.x + scrollrect.w && my >= scrollrect.y
		&& my < scrollrect.y + scrollrect.h) {
		// Now work out the position
		const int pos = ((scrollrect.h - sliderh) * get_scroll_offset())
						/ std::max(1, GetScrollMax());
		;

		// Pressed above it
		if (my < pos + scrollrect.y) {
			scroll_page(-1);
			gwin->set_all_dirty();
			return true;
		}
		// Pressed below it
		else if (my >= pos + scrollrect.y + sliderh) {
			scroll_page(1);
			gwin->set_all_dirty();
			return true;
		}
		// Pressed on it
		else {
			drag_start = my;
			return true;
		}
	}
	return false;
}

bool Scrollable_widget::mouse_up(int mx, int my, MouseButton button) {
	if (drag_start == -1) {
		// only if in ourbounds
		if (!get_rect().has_point(mx, my)) {
			return false;
		}

		Gump_widget* found = nullptr;
		while ((found = Gump_widget::findSorted(mx, my, children, found))) {
			if (found->mouse_up(mx, my, button)) {
				gwin->set_all_dirty();
				return true;
			}
		}
	} else {
		// end drag in progress
		drag_start = -1;
	}
	return false;
}

bool Scrollable_widget::mouse_drag(int mx, int my) {
	// If not sliding try children then let parent class try to deal with it
	if (drag_start == -1) {
		// only if in ourbounds
		if (!get_rect().has_point(mx, my)) {
			return false;
		}

		Gump_widget* found = nullptr;
		while ((found = Gump_widget::findSorted(mx, my, children, found))) {
			if (found->mouse_drag(mx, my)) {
				return true;
			}
		}
		return Gump_widget::mouse_drag(mx, my);
	}

	screen_to_local(mx, my);

	// First if the position is too far away from the slider
	// We'll clamp it to the extremes
	int sy = my - scrollrect.y;
	if (mx < scrollrect.x - 20 || mx > scrollrect.x + sliderw + 20) {
		sy = drag_start - scrollrect.y;
	}

	if (sy < sliderh / 2) {
		sy = sliderh / 2;
	}
	if (sy > scrollrect.h) {
		sy = scrollrect.h;
	}

	//
	const int new_offset = sy * scroll_max / (scrollrect.h - sliderh);

	if (new_offset != get_scroll_offset()) {
		set_scroll_offset(new_offset);
		gwin->set_all_dirty();
	}
	return true;
}

bool Scrollable_widget::mousewheel_up(int mx, int my) {
	Gump_widget* found = nullptr;
	while ((found = Gump_widget::findSorted(mx, my, children, found))) {
		if (found->mousewheel_up(mx, my)) {
			return true;
		}
	}
	if (scroll_enabled()) {
		line_up();
		return true;
	}
	return false;
}

bool Scrollable_widget::mousewheel_down(int mx, int my) {
	Gump_widget* found = nullptr;
	while ((found = Gump_widget::findSorted(mx, my, children, found))) {
		if (found->mousewheel_down(mx, my)) {
			return true;
		}
	}
	if (scroll_enabled()) {
		line_down();
		return true;
	}

	return false;
}

void Scrollable_widget::paint() {
	int sx, sy;
	// Save clipping rect
	auto ibuf = gwin->get_win()->get_ibuf();

	Image_buffer::ClipRectSave clip(ibuf);

	// Make sure children are clipped to us
	auto new_clip = this->get_rect().intersect(clip);
	ibuf->set_clip(new_clip.x, new_clip.y, new_clip.w, new_clip.h);

	if (scroll_enabled()) {
		// paint scrollbar children
		tcb::span<std::shared_ptr<Gump_widget>> sbchildren(
				children.data() + id_first, id_button_count);
		Gump_widget::paintSorted(sbchildren);

		// paint scroll background and slider
		sx = scrollrect.x;
		sy = scrollrect.y;
		local_to_screen(sx, sy);
		ibuf->draw_box(sx, sy, scrollrect.w, scrollrect.h, 0, scrollbg, 0xff);

		// Now work out the position
		const int pos = ((scrollrect.h - sliderh) * get_scroll_offset())
						/ std::max(1, GetScrollMax());

		slider_shape.paint_shape(sx, sy + pos);

		// Update clip to scrollable region
		auto usable = GetUsableArea(true);
		new_clip    = usable.intersect(new_clip);
		pane->set_pos(border_size, pane->get_y());
		ibuf->set_clip(new_clip.x, new_clip.y, new_clip.w, new_clip.h);
	}

	// paint scrolling pane
	Gump_widget::paintSorted(*pane);
}

void Scrollable_widget::scroll_line(int dir) {
	set_scroll_offset(get_scroll_offset() + line_height * dir);
}

void Scrollable_widget::scroll_page(int dir) {
	set_scroll_offset((get_scroll_offset() + height * dir));
}

void Scrollable_widget::set_scroll_offset(int newpos, bool ignore_snap) {
	// if not scolling, no chagimg offset
	if (!scroll_enabled()) {
		return;
	}

	if (snap && !ignore_snap) {
		newpos = ((newpos + line_height / 2) / line_height) * line_height;
	}
	pane->scroll_offset = -std::max(0, std::min(newpos, GetScrollMax()));
	gwin->set_all_dirty();
}

void Scrollable_widget::set_line_height(int newsize, bool snap) {
	line_height = newsize;
	this->snap  = snap;
	set_scroll_offset(get_scroll_offset());
}

int Scrollable_widget::Get_ChildHeight() const {
	int maxy = 0;
	for (auto& child : *pane) {
		if (!child) {
			continue;
		}
		maxy += child->get_rect().h + border_size;
	}
	return maxy - border_size;
}

bool Scrollable_widget::arrange_children() {
	TileRect usablearea = GetUsableArea();
	bool     changed    = false;
	int      voffset    = usablearea.y;
	for (auto& child : *pane) {
		if (!child) {
			continue;
		}
		int y = child->get_y();
		int x = usablearea.x;

		auto rect = child->get_rect();
		if (halign == HAlign::right) {
			x += usablearea.w - rect.w;
		}
		if (halign == HAlign::centre) {
			x += (usablearea.w - rect.w) / 2;
		}
		if (halign == HAlign::none) {
			x = child->get_x();
		}

		y = voffset;
		voffset += rect.h + border_size;

		changed |= child->get_x() != x || child->get_y() != y;
		child->set_pos(x, y);
	}
	return changed;
}

void Scrollable_widget::expand(int deltax, int deltay) {
	width += deltax;
	height += deltay;
	scrollrect.x += deltax;
	scrollrect.h += deltay;
	
	// Move scrolbar widgets

	for (int index = id_first_button; index <= id_last_button; index++)
	{
		auto widget = children[index];
		if (widget) {		
		widget->set_pos(widget->get_x() + deltax, widget->get_y()+(index>=id_page_down?deltay:0));
		}
	}	
}

Scrollable_widget::Scrolling_pane::Scrolling_pane(
		Gump_Base* parent, int px, int py)
		: IterableGump_widget(parent, -1, px, py, 0), scroll_offset(y) {}
