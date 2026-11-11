/*
Copyright (C) 2026 The Exult Team

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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Container_list_gump.h"

#include "Container_list_layout.h"
#include "Gump_button.h"
#include "Gump_widget.h"
#include "cheat.h"
#include "contain.h"
#include "exult_flx.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"
#include "misc_buttons.h"
#include "objiter.h"
#include "palette.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL_keyboard.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <algorithm>
#include <cctype>
#include <cstdio>

using namespace Container_list_layout;

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Quantity_() {
			return get_text_msg(0x6D0 - msg_file_start);
		}

		static auto Weight_() {
			return get_text_msg(0x6D1 - msg_file_start);
		}

		static auto of() {
			return get_text_msg(0x6D2 - msg_file_start);
		}

		static auto Empty() {
			return get_text_msg(0x6D3 - msg_file_start);
		}

		static auto DoubleClickToUse() {
			return get_text_msg(0x6D4 - msg_file_start);
		}

		static auto DragToTake() {
			return get_text_msg(0x6D5 - msg_file_start);
		}
	};

	// Ink colour of the scroll art, for rules and the divider.
	constexpr unsigned char ink = 0x8C;
	// Fonts: the game's own, as the notebook and scrolls use.
	constexpr int big_font   = 2;
	constexpr int small_font = 4;
	// Icon cell in each row.
	constexpr int icon_w = 14;
	constexpr int icon_h = row_h - 2;
	// Preview box in the right panel.
	constexpr int preview_y = text_y + 12;
	constexpr int preview_h = 50;
	// Slider travel along the track.
	constexpr int track_travel = track_y1 - track_y0 - slider_h;

	using List_button = CallbackButton<Container_list_gump>;

	/*
	 *  The scroll track between the arrow buttons: a groove, and the
	 *  slider when there is more than one page.  Click above or below the
	 *  slider to page; drag the slider to scroll.
	 */
	class List_track : public Gump_widget {
		int grab = -1;    // Grab point in the slider while dragging, or -1.

		Container_list_gump* list() const {
			return static_cast<Container_list_gump*>(parent);
		}

		int screen_y() const {
			int sx = 0;
			int sy = 0;
			local_to_screen(sx, sy);
			return sy;
		}

	public:
		explicit List_track(Container_list_gump* par) : Gump_widget(par, -1, ctrl_x, track_y0, 0, SF_GUMPS_VGA) {}

		// No shape, so the rect is the track itself.
		TileRect get_rect() const override {
			int sx = 0;
			int sy = 0;
			local_to_screen(sx, sy);
			return TileRect(sx, sy, 11, track_y1 - track_y0);
		}

		void paint() override {
			int sx = 0;
			int sy = 0;
			local_to_screen(sx, sy);
			list()->paint_shade(5, track_y1 - track_y0, sx + 3, sy, 70);
			const int count = list()->get_count();
			if (count <= rows) {
				return;
			}
			const ShapeID slider(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
			Shape_frame*  frame = slider.get_shape();
			if (frame) {
				const int off = slider_offset(list()->get_first(), count, track_travel);
				slider.paint_shape(sx + 2 + frame->get_xleft(), sy + off + frame->get_yabove());
			}
		}

		bool mouse_down(int mx, int my, MouseButton button) override {
			if (button != MouseButton::Left || !on_widget(mx, my)) {
				return false;
			}
			const int count = list()->get_count();
			if (count <= rows) {
				return true;    // Nothing to scroll, but it's our click.
			}
			const int ly  = my - screen_y();
			const int off = slider_offset(list()->get_first(), count, track_travel);
			if (ly < off) {
				list()->page_up();
			} else if (ly >= off + slider_h) {
				list()->page_down();
			} else {
				grab = ly - off;
			}
			return true;
		}

		bool mouse_drag(int mx, int my) override {
			ignore_unused_variable_warning(mx);
			if (grab < 0) {
				return false;
			}
			const int count = list()->get_count();
			list()->scroll_to(first_from_slider(my - screen_y() - grab, count, track_travel));
			return true;
		}

		bool mouse_up(int mx, int my, MouseButton button) override {
			ignore_unused_variable_warning(mx, my, button);
			grab = -1;
			return true;
		}
	};

	// Top-left corner of an exult.flx button placed with its top-left at
	// (tx, ty), as a hotspot position.
	int button_x(int shnum, int tx) {
		const ShapeID      sid(shnum, 0, SF_EXULT_FLX);
		const Shape_frame* frame = sid.get_shape();
		return tx + (frame ? frame->get_xleft() : 0);
	}

	int button_y(int shnum, int ty) {
		const ShapeID      sid(shnum, 0, SF_EXULT_FLX);
		const Shape_frame* frame = sid.get_shape();
		return ty + (frame ? frame->get_yabove() : 0);
	}
}    // namespace

/*
 *  Create the list view for a container.  shnum is the container's own
 *  gump shape.
 */

Container_list_gump::Container_list_gump(Container_game_object* cont, int initx, int inity, int shnum)
		: Gump(cont, initx, inity, shnum), scroll(game->get_shape("gumps/scroll"), 0, SF_GUMPS_VGA) {
	add_elem(new Checkmark_button(this, check_x, check_y));
	add_elem(new List_button(
			this, &Container_list_gump::line_up, EXULT_FLX_SAV_UP_SHP, button_x(EXULT_FLX_SAV_UP_SHP, ctrl_x),
			button_y(EXULT_FLX_SAV_UP_SHP, up_y), SF_EXULT_FLX));
	add_elem(new List_button(
			this, &Container_list_gump::line_down, EXULT_FLX_SAV_DOWN_SHP, button_x(EXULT_FLX_SAV_DOWN_SHP, ctrl_x),
			button_y(EXULT_FLX_SAV_DOWN_SHP, down_y), SF_EXULT_FLX));
	add_elem(new List_track(this));
	collect_rows();
	if (!items.empty()) {
		selected = items.front();
	}
}

/*
 *  The layout needs the scroll frame it was made for.  A mod that
 *  replaces the scroll with different art gets the picture view.
 */

bool Container_list_gump::is_available() {
	const int shnum = game->get_shape("gumps/scroll");
	if (shnum < 0) {
		return false;
	}
	const ShapeID      sid(shnum, 0, SF_GUMPS_VGA);
	const Shape_frame* frame = sid.get_shape();
	return frame && frame->get_width() == scroll_w && frame->get_height() == scroll_h && frame->get_xleft() == 0
		   && frame->get_yabove() == 0;
}

Gump* Container_list_gump::clone(Container_game_object* cont, int initx, int inity) {
	return new Container_list_gump(cont, initx, inity, get_shapenum());
}

/*
 *  Screen area: the widened scroll.  (The base class measures the
 *  container's gump shape, which isn't drawn here.)
 */

TileRect Container_list_gump::get_rect() const {
	return TileRect(x, y, scroll_w + extra_w, scroll_h);
}

bool Container_list_gump::has_point(int sx, int sy) const {
	const int lx = sx - x;
	const int ly = sy - y;
	if (lx >= 0 && lx < scroll_w + extra_w && ly >= 0 && ly < scroll_h) {
		const Shape_frame* frame = scroll.get_shape();
		if (frame && frame->has_point(source_column(lx), ly)) {
			return true;
		}
	}
	for (const auto* w : elems) {
		if (w && w->on_widget(sx, sy)) {
			return true;
		}
	}
	return false;
}

TileRect Container_list_gump::get_dirty() {
	return get_rect();
}

/*
 *  Where an object is drawn: centred in its row's icon cell, or the
 *  middle of the rows when it is scrolled out of view.
 */

void Container_list_gump::get_shape_location(const Game_object* obj, int& ox, int& oy) const {
	const Shape_frame* frame = obj->get_shape();
	const int          index = index_of(obj);
	if (frame && index >= first && index < first + rows) {
		const int ry = row_y0 + (index - first) * row_h;
		ox           = x + text_x + 1 + (icon_w - frame->get_width()) / 2 + frame->get_xleft();
		oy           = y + ry + 1 + (icon_h - frame->get_height()) / 2 + frame->get_yabove();
	} else {
		ox = x + text_x + row_w / 2;
		oy = y + row_y0 + rows * row_h / 2;
	}
}

TileRect Container_list_gump::get_shape_rect(const Game_object* obj) const {
	const Shape_frame* frame = obj->get_shape();
	if (!frame) {
		return TileRect(0, 0, 0, 0);
	}
	int ox;
	int oy;
	get_shape_location(obj, ox, oy);
	return TileRect(ox - frame->get_xleft(), oy - frame->get_yabove(), frame->get_width(), frame->get_height());
}

/*
 *  The whole row is the hit target.
 */

Game_object* Container_list_gump::find_object(int mx, int my) {
	collect_rows();
	const int count = static_cast<int>(items.size());
	const int index = row_at(mx - x, my - y, clamp_first(first, count), count);
	return index >= 0 ? items[index] : nullptr;
}

void Container_list_gump::object_pressed(Game_object* obj) {
	select(obj);
}

/*
 *  Add an object.  A drop on a row goes to that row's object, as in the
 *  picture view (into a bag, or onto a matching stack).  Anything else
 *  goes into the container with tx/ty unset, so the picture view places
 *  it later.  If mx, my, sx, sy are all -2, tx/ty are already correct.
 *
 *  Output: false if cannot add it.
 */

bool Container_list_gump::add(Game_object* obj, int mx, int my, int sx, int sy, bool dont_check, bool combine) {
	ignore_unused_variable_warning(combine);
	if (!container || (!cheat.in_hack_mover() && !dont_check && !container->has_room(obj))) {
		return false;    // Full.
	}
	Game_object* onobj = find_object(mx, my);
	if (onobj && onobj != obj && onobj->drop(obj)) {
		select(onobj);
		return true;
	}
	if (!container->add(obj, dont_check)) {    // DON'T combine here.
		return false;
	}
	if (mx != -2 || my != -2 || sx != -2 || sy != -2) {
		obj->set_shape_pos(255, 255);
	}
	select(obj);
	return true;
}

/*
 *  Remove an object.  If it was selected, the row that takes its place
 *  (or the new last row) is selected.
 */

void Container_list_gump::remove(Game_object* obj) {
	collect_rows();
	if (obj == selected) {
		const int index = selected_index();
		const int count = static_cast<int>(items.size());
		if (index + 1 < count) {
			selected = items[index + 1];
		} else {
			selected = index > 0 ? items[index - 1] : nullptr;
		}
	}
	Gump::remove(obj);
}

/*
 *  Paint the scroll, the list and the selected item.
 */

void Container_list_gump::paint() {
	paint_scroll();
	gwin->set_painted();

	// Rebuild the rows and keep the selection valid.  When the selected
	// object went away without passing through remove() (used up by
	// usecode, say), select the row now at its place.
	collect_rows();
	const int count = static_cast<int>(items.size());
	int       sel   = selected_index();
	if (sel < 0) {
		if (count > 0) {
			sel      = std::clamp(sel_hint, 0, count - 1);
			selected = items[sel];
			first    = first_showing(first, sel, count);
		} else {
			selected = nullptr;
		}
	}
	sel_hint = std::max(sel, 0);
	first    = clamp_first(first, count);

	paint_list();
	paint_panel();
	paint_elems();

	// Outline selections in this gump.
	const Game_object_shared_vector& sel_objs = cheat.get_selected();
	for (const auto& it : sel_objs) {
		Game_object* obj   = it.get();
		const int    index = index_of(obj);
		if (container == obj->get_owner() && index >= first && index < first + rows) {
			int ox;
			int oy;
			get_shape_location(obj, ox, oy);
			obj->ShapeID::paint_outline(ox, oy, HIT_PIXEL);
		}
	}
}

/*
 *  Paint the scroll in three slices: its left part, the plain strip
 *  repeated, and its right part shifted right.
 */

void Container_list_gump::paint_scroll() {
	Image_buffer8*  ib   = gwin->get_win()->get_ib8();
	auto            save = ib->SaveClip();
	const TileRect& clip = save.Rect();

	auto draw = [&](int dest_x, int w, int shift) {
		const TileRect r = TileRect(x + dest_x, y, w, scroll_h).intersect(clip);
		if (r.w > 0 && r.h > 0) {
			ib->set_clip(r.x, r.y, r.w, r.h);
			scroll.paint_shape(x + shift, y);
		}
	};
	draw(0, strip_x1, 0);
	constexpr int strip_w = strip_x1 - strip_x0;
	for (int d = strip_x1; d < strip_x1 + extra_w; d += strip_w) {
		draw(d, std::min(strip_w, strip_x1 + extra_w - d), d - strip_x0);
	}
	draw(strip_x1 + extra_w, scroll_w - strip_x1, extra_w);
}

/*
 *  Left column: title, rows, count and the divider.
 */

void Container_list_gump::paint_list() {
	Image_buffer8* ib    = gwin->get_win()->get_ib8();
	const int      lx    = x + text_x;
	const int      ty    = y + text_y;
	const int      count = static_cast<int>(items.size());
	const int      sel   = selected_index();

	const std::string title = fit_text(big_font, container ? display_name(container) : std::string(), list_w - 4);
	sman->paint_text(big_font, title.c_str(), lx + (list_w - sman->get_text_width(big_font, title.c_str())) / 2, ty - 2);
	ib->fill_hline8(ink, list_w - 8, lx + 4, ty + 8);

	// A faint dotted divider between the two columns.
	for (int dy = text_y + 2; dy < text_y + text_h - 4; dy++) {
		if (dy % 3) {
			ib->put_pixel8(ink, lx + list_w + 4, y + dy);
		}
	}

	if (count == 0) {
		const char* empty = Strings::Empty();
		sman->paint_text(
				big_font, empty, lx + (list_w - sman->get_text_width(big_font, empty)) / 2,
				y + row_y0 + (rows * row_h - sman->get_text_height(big_font)) / 2);
		return;
	}

	for (int i = 0; i < rows && first + i < count; i++) {
		const int    n   = first + i;
		Game_object* obj = items[n];
		const int    ry  = y + row_y0 + i * row_h;
		if (n == sel) {
			paint_shade(row_w, row_h, lx, ry, 85);
		} else if (n % 2) {
			paint_shade(row_w, row_h, lx, ry, 95);
		}
		// The item, centred in its cell and clipped to it.
		Shape_frame* frame = obj->get_shape();
		if (frame) {
			auto           save = ib->SaveClip();
			const TileRect cell = TileRect(lx + 1, ry + 1, icon_w, icon_h).intersect(save.Rect());
			if (cell.w > 0 && cell.h > 0) {
				ib->set_clip(cell.x, cell.y, cell.w, cell.h);
				obj->paint_shape(
						lx + 1 + (icon_w - frame->get_width()) / 2 + frame->get_xleft(),
						ry + 1 + (icon_h - frame->get_height()) / 2 + frame->get_yabove());
			}
		}
		// Name, then the quantity right-aligned.
		int       name_w   = row_w - 20;
		const int quantity = obj->get_quantity();
		if (quantity > 1) {
			char buf[16];
			snprintf(buf, sizeof(buf), "%d", quantity);
			const int qw = sman->get_text_width(big_font, buf);
			sman->paint_text(big_font, buf, lx + row_w - 2 - qw, ry + 2);
			name_w -= qw + 4;
		}
		const std::string name = fit_text(big_font, display_name(obj), name_w);
		sman->paint_text(big_font, name.c_str(), lx + 18, ry + 2);
	}

	char buf[64];
	snprintf(buf, sizeof(buf), "%d-%d %s %d", first + 1, std::min(first + rows, count), Strings::of(), count);
	sman->paint_text(small_font, buf, lx + (row_w - sman->get_text_width(small_font, buf)) / 2, y + count_y);
}

/*
 *  Right column: the selected item, enlarged, with its details.
 */

void Container_list_gump::paint_panel() {
	const int sel = selected_index();
	if (sel < 0) {
		return;    // Empty container: plain parchment.
	}
	Game_object*   obj = items[sel];
	Image_buffer8* ib  = gwin->get_win()->get_ib8();
	const int      px  = x + panel_x;
	const int      ty  = y + text_y;

	const std::string name = fit_text(big_font, display_name(obj), panel_w - 2);
	sman->paint_text(big_font, name.c_str(), px + (panel_w - sman->get_text_width(big_font, name.c_str())) / 2, ty - 2);
	ib->fill_hline8(ink, panel_w - 16, px + 8, ty + 8);

	paint_enlarged(obj, px, y + preview_y, panel_w, preview_h);

	char      quantity[48];
	char      weight[48];
	const int wt = obj->get_weight();    // In tenths of a stone.
	snprintf(quantity, sizeof(quantity), "%s %d", Strings::Quantity_(), obj->get_quantity());
	snprintf(weight, sizeof(weight), "%s %d.%d", Strings::Weight_(), wt / 10, wt % 10);
	const char* lines[] = {quantity, weight, "", Strings::DoubleClickToUse(), Strings::DragToTake()};
	int         ly      = ty + 66;
	for (const char* line : lines) {
		if (*line) {
			const std::string s = fit_text(small_font, line, panel_w);
			sman->paint_text(small_font, s.c_str(), px + (panel_w - sman->get_text_width(small_font, s.c_str())) / 2, ly);
		}
		ly += 8;
	}
}

/*
 *  Paint an object k times its size, centred in a box.  It is painted
 *  first into two scratch buffers, one filled with the parchment colour
 *  and one with 0xff: a pixel is transparent only where they still read
 *  those.  Translucent pixels come out blended with the parchment.
 */

void Container_list_gump::paint_enlarged(Game_object* obj, int bx, int by, int bw, int bh) {
	const Shape_frame* frame = obj->get_shape();
	if (!frame) {
		return;
	}
	const int w = frame->get_width();
	const int h = frame->get_height();
	if (w <= 0 || h <= 0) {
		return;
	}
	Image_buffer8* ib = gwin->get_win()->get_ib8();
	// The parchment colour under the box.
	unsigned char back = 0;
	{
		const auto save = ib->SaveClip();
		const int  sx   = bx + 4;
		const int  sy   = by + bh / 2;
		if (save.Rect().has_point(sx, sy)) {
			back = ib->get_pixel8(sx, sy);
		}
		if (back == 0xff) {
			back = 0;
		}
	}
	Image_buffer8 lo(w, h);
	Image_buffer8 hi(w, h);
	lo.fill8(back);
	hi.fill8(0xff);
	for (Image_buffer8* buf : {&lo, &hi}) {
		Image_buffer8* prev = gwin->push_render_target(buf);
		obj->paint_shape(frame->get_xleft(), frame->get_yabove());
		gwin->pop_render_target(prev);
	}
	const int k  = preview_scale(w, h, bw, bh);
	const int ox = bx + (bw - w * k) / 2;
	const int oy = by + (bh - h * k) / 2;
	for (int py = 0; py < h; py++) {
		for (int px = 0; px < w; px++) {
			const unsigned char pix = lo.get_pixel8(px, py);
			if (pix == back && hi.get_pixel8(px, py) == 0xff) {
				continue;    // Transparent.
			}
			for (int dy = 0; dy < k; dy++) {
				for (int dx = 0; dx < k; dx++) {
					ib->put_pixel8(pix, ox + px * k + dx, oy + py * k + dy);
				}
			}
		}
	}
}

/*
 *  Darken a rectangle, keeping to the art's own tones.  The game's blend
 *  tables are too strong for this and differ between the games, so the
 *  tables are made from the palette.
 */

void Container_list_gump::paint_shade(int w, int h, int sx, int sy, int pct) {
	update_shades();
	const Xform_palette& table = pct <= 70 ? shade70 : (pct <= 85 ? shade85 : shade95);
	gwin->get_win()->get_ib8()->fill_translucent8(0, w, h, sx, sy, table);
}

void Container_list_gump::update_shades() {
	const Palette* pal = gwin->get_pal();
	if (!pal || pal->get_palette_index() == shade_pal) {
		return;
	}
	shade_pal = pal->get_palette_index();
	auto make = [pal](Xform_palette& table, int pct) {
		for (int i = 0; i < 256; i++) {
			if (i >= 0xe0) {    // Leave cycling and special colours alone.
				table.colors[i] = static_cast<unsigned char>(i);
				continue;
			}
			table.colors[i] = static_cast<unsigned char>(pal->find_color(
					pal->get_red(i) * pct / 100, pal->get_green(i) * pct / 100, pal->get_blue(i) * pct / 100));
		}
	};
	make(shade70, 70);
	make(shade85, 85);
	make(shade95, 95);
}

/*
 *  Scrolling.
 */

bool Container_list_gump::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		page_up();
	} else {
		line_up();
	}
	return true;
}

bool Container_list_gump::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		page_down();
	} else {
		line_down();
	}
	return true;
}

void Container_list_gump::line_up() {
	scroll_to(first - 1);
}

void Container_list_gump::line_down() {
	scroll_to(first + 1);
}

void Container_list_gump::page_up() {
	scroll_to(first - rows);
}

void Container_list_gump::page_down() {
	scroll_to(first + rows);
}

void Container_list_gump::scroll_to(int newfirst) {
	newfirst = clamp_first(newfirst, get_count());
	if (newfirst != first) {
		first = newfirst;
		gwin->add_dirty(get_rect());
	}
}

int Container_list_gump::get_count() {
	collect_rows();
	return static_cast<int>(items.size());
}

/*
 *  Rows: the container's objects that have a shape, in order.
 */

void Container_list_gump::collect_rows() {
	items.clear();
	if (!container) {
		return;
	}
	Object_iterator next(container->get_objects());
	Game_object*    obj;
	while ((obj = next.get_next()) != nullptr) {
		if (obj->get_shape()) {
			items.push_back(obj);
		}
	}
}

// Row index of an object, counted as collect_rows() does, or -1.
int Container_list_gump::index_of(const Game_object* obj) const {
	if (!container || !obj) {
		return -1;
	}
	int             index = 0;
	Object_iterator next(container->get_objects());
	Game_object*    each;
	while ((each = next.get_next()) != nullptr) {
		if (!each->get_shape()) {
			continue;
		}
		if (each == obj) {
			return index;
		}
		index++;
	}
	return -1;
}

int Container_list_gump::selected_index() const {
	if (!selected) {
		return -1;
	}
	const auto it = std::find(items.begin(), items.end(), selected);
	return it == items.end() ? -1 : static_cast<int>(it - items.begin());
}

/*
 *  Select an object and scroll it into view.
 */

void Container_list_gump::select(Game_object* obj) {
	selected = obj;
	collect_rows();
	const int sel = selected_index();
	if (sel >= 0) {
		sel_hint = sel;
		first    = first_showing(first, sel, static_cast<int>(items.size()));
	}
	gwin->add_dirty(get_rect());
}

/*
 *  Name as shown: the game's name without the "<quantity> " that stacked
 *  items' names start with, and with an initial capital.
 */

std::string Container_list_gump::display_name(const Game_object* obj) {
	std::string name = obj->get_name();
	size_t      i    = 0;
	while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i]))) {
		i++;
	}
	if (i > 0 && i < name.size() && name[i] == ' ') {
		name.erase(0, i + 1);
	}
	if (!name.empty() && name[0] >= 'a' && name[0] <= 'z') {
		name[0] = static_cast<char>(name[0] - 'a' + 'A');
	}
	return name;
}

// Trim a string to fit a width.
std::string Container_list_gump::fit_text(int fontnum, const std::string& s, int w) {
	std::string out = s;
	while (!out.empty() && sman->get_text_width(fontnum, out.c_str()) > w) {
		out.pop_back();
	}
	return out;
}
