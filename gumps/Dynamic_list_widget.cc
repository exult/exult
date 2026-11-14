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

#include "Dynamic_list_widget.h"

#include "Dynamic_list_layout.h"
#include "Dynamic_slider.h"
#include "Gump.h"
#include "contain.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"
#include "objiter.h"
#include "palette.h"
#include "ucmachine.h"

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

using namespace Dynamic_list_layout;

namespace {
	class Strings : public GumpStrings {
	public:
		static auto of() {
			return get_text_msg(0x6D2 - msg_file_start);
		}

		static auto Empty() {
			return get_text_msg(0x6D3 - msg_file_start);
		}
	};

	// Icon cell in each row: icon_w by row_height - 2, one pixel in.
	constexpr int icon_w = 14;
	// Font of the "N-M of T" line.
	constexpr int count_font = 4;
	// Built-in scroll controls: gap between each arrow and the track,
	// and the thumb's offset into the arrows' column.
	constexpr int builtin_pad   = 1;
	constexpr int builtin_inset = 2;

	// Shade tables, made from the palette (the game's blend tables are
	// too strong for this and differ between the games).
	Xform_palette shade70{};
	Xform_palette shade85{};
	Xform_palette shade95{};
	int           shade_pal = -1;    // Palette the shades were made for.

	void update_shades(Game_window* gwin) {
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
				table.colors[i] = static_cast<unsigned char>(
						pal->find_color(pal->get_red(i) * pct / 100, pal->get_green(i) * pct / 100, pal->get_blue(i) * pct / 100));
			}
		};
		make(shade70, 70);
		make(shade85, 85);
		make(shade95, 95);
	}

	int frame_height(int shnum, ShapeFile file) {
		const ShapeID      sid(shnum, 0, file);
		const Shape_frame* frame = sid.get_shape();
		return frame ? frame->get_height() : 8;
	}
}    // namespace

Dynamic_list_widget::Dynamic_list_widget(Gump* par, const Dynamic_list_def& d)
		: Gump_button(par, -1, d.pos_x, d.pos_y, SF_GUMPS_VGA), def(d), builtin_controls(d.up_shape < 0),
		  preview_sid(-1, 0, SF_SHAPES_VGA) {
	if (def.source != 1) {
		def.source = 0;
	}
	// The scroll controls: a vertical Dynamic_slider to the right of the
	// rows, its arrows at the top and bottom of the rows' column.
	Dynamic_slider_def sd;
	sd.field_id        = -1;
	sd.pos_x           = def.width + 3;
	sd.pos_y           = 0;
	sd.min_val         = 0;
	sd.max_val         = 0;
	sd.step            = 1;
	sd.default_val     = 0;
	sd.usecode_fn      = -1;
	sd.orientation     = 0;
	sd.hide_idle_thumb = true;
	if (builtin_controls) {
		// Exult's own: the save/load gump's arrows and slider.
		sd.shapefile   = SF_EXULT_FLX;
		sd.dec_shape   = EXULT_FLX_SAV_UP_SHP;
		sd.inc_shape   = EXULT_FLX_SAV_DOWN_SHP;
		sd.thumb_shape = EXULT_FLX_SAV_SLIDER_SHP;
		sd.track_pad   = builtin_pad;
		sd.thumb_inset = builtin_inset;
		sd.top_left    = true;
	} else {
		sd.shapefile   = SF_GUMPS_VGA;
		sd.dec_shape   = def.up_shape;
		sd.inc_shape   = def.down_shape;
		sd.thumb_shape = def.thumb_shape;
	}
	const int dec_h = frame_height(sd.dec_shape, sd.shapefile);
	const int inc_h = frame_height(sd.inc_shape, sd.shapefile);
	sd.extent       = std::max(0, def.rows * def.row_height - dec_h - inc_h - 2 * sd.track_pad);
	groove_y        = dec_h + sd.track_pad;
	groove_h        = sd.extent;
	slider          = std::make_unique<Dynamic_slider>(this, sd);
	// Thumb drags and arrows scroll the list.  The slider already shows
	// the value, so only the rows need repainting.
	slider->set_on_change([this](int val) {
		first = val;
		dirty();
	});
}

Dynamic_list_widget::~Dynamic_list_widget() = default;

Gump_widget* Dynamic_list_widget::clone(Gump* par) {
	return new Dynamic_list_widget(par, def);
}

/*
 *  The rows, the count line below them and the scroll controls.
 */

TileRect Dynamic_list_widget::get_rect() const {
	int h = def.rows * def.row_height;
	if (def.flags & DYNLIST_COUNT_LINE) {
		h += 2 + sman->get_text_height(count_font);
	}
	TileRect r(0, 0, def.width + 15, h);
	local_to_screen(r.x, r.y);
	if (slider) {
		r = r.add(slider->get_rect());
	}
	return r;
}

// Repaint the whole gump: a bound shape display may show the selection.
void Dynamic_list_widget::dirty() {
	auto* gump = dynamic_cast<Gump*>(parent);
	gwin->add_dirty(gump ? gump->get_rect() : get_rect());
}

/*
 *  Rows.
 */

void Dynamic_list_widget::sync_rows() {
	if (def.source != 0) {
		return;
	}
	auto*                  gump = dynamic_cast<Gump*>(parent);
	Container_game_object* cont = gump ? gump->get_container() : nullptr;
	std::vector<Row_sig>   sig;
	if (cont) {
		Object_iterator next(cont->get_objects());
		Game_object*    obj;
		while ((obj = next.get_next()) != nullptr) {
			if (obj->get_shape()) {
				sig.push_back(Row_sig{obj, obj->get_shapenum(), obj->get_framenum(), obj->get_quantity()});
			}
		}
	}
	if (sig != signature) {
		// Contents changed: rebuild, and let rows_fn run from run().
		signature = std::move(sig);
		objs.clear();
		rows.clear();
		for (const auto& each : signature) {
			Row row;
			row.text = display_name(each.obj);
			if (each.quantity > 1) {
				row.right = std::to_string(each.quantity);
			}
			objs.push_back(each.obj);
			rows.push_back(std::move(row));
		}
		rows_pending = true;
	}
	// Keep the selection valid.  When the selected object went away,
	// select the row now at its place.
	const int  count = static_cast<int>(objs.size());
	const auto it    = sel_obj ? std::find(objs.begin(), objs.end(), sel_obj) : objs.end();
	int        sel   = it == objs.end() ? -1 : static_cast<int>(it - objs.begin());
	if (sel < 0) {
		sel_obj = nullptr;
		if (sel_follow && count > 0) {
			sel = next_enabled(std::clamp(sel_hint, 0, count - 1) - 1, 1, count, [this](int i) {
				return row_disabled(i);
			});
			if (sel >= 0) {
				sel_obj = objs[sel];
				first   = first_showing(first, sel, count, def.rows);
			}
		}
	}
	if (sel >= 0) {
		sel_hint = sel;
	}
	first = clamp_first(first, count, def.rows);
	update_slider();
}

int Dynamic_list_widget::get_count() {
	sync_rows();
	return static_cast<int>(rows.size());
}

void Dynamic_list_widget::update_slider() {
	const int count    = static_cast<int>(rows.size());
	const int maxfirst = std::max(0, count - def.rows);
	first              = clamp_first(first, count, def.rows);
	if (slider->get_max() != maxfirst) {
		slider->set_range(0, maxfirst);
	}
	if (slider->getselection() != first && !slider->is_dragging()) {
		slider->set_val(first);
	}
}

bool Dynamic_list_widget::row_disabled(int index) const {
	return index >= 0 && index < static_cast<int>(rows.size()) && (rows[index].flags & row_flag_disabled) != 0;
}

int Dynamic_list_widget::selected_row() {
	sync_rows();
	if (def.source == 0) {
		const auto it = sel_obj ? std::find(objs.begin(), objs.end(), sel_obj) : objs.end();
		return it == objs.end() ? -1 : static_cast<int>(it - objs.begin());
	}
	return sel_index >= 0 && sel_index < static_cast<int>(rows.size()) ? sel_index : -1;
}

int Dynamic_list_widget::get_selection() {
	return selected_row() + 1;
}

/*
 *  Select a row (0-based, -1 = none) and scroll it into view.
 *  Output: true if the selection changed.
 */

bool Dynamic_list_widget::select_row(int index) {
	const int old   = selected_row();
	const int count = static_cast<int>(rows.size());
	if (index >= count || row_disabled(index)) {
		return false;
	}
	if (index < 0) {
		index = -1;
	}
	if (def.source == 0) {
		sel_obj    = index >= 0 ? objs[index] : nullptr;
		sel_follow = index >= 0;
		if (index >= 0) {
			sel_hint = index;
		}
	} else {
		sel_index = index;
	}
	if (index >= 0) {
		scroll_to(first_showing(first, index, count, def.rows));
	}
	dirty();
	return old != index;
}

bool Dynamic_list_widget::set_selection(int index) {
	const int count = get_count();
	if (index < 0 || index > count || row_disabled(index - 1)) {
		return false;
	}
	select_row(index - 1);
	return true;
}

int Dynamic_list_widget::set_rows(std::vector<Row> newrows) {
	if (def.source != 1) {
		return 0;
	}
	rows      = std::move(newrows);
	sel_index = -1;
	first     = 0;
	update_slider();
	dirty();
	return static_cast<int>(rows.size());
}

bool Dynamic_list_widget::set_row(int index, const Row& row, int given) {
	const int count = get_count();
	if (def.source == 1) {
		if (index < 1 || index > count + 1) {
			return false;
		}
		if (index == count + 1) {
			rows.push_back(row);
		} else {
			rows[index - 1] = row;
		}
		if (sel_index == index - 1 && row_disabled(sel_index)) {
			sel_index = -1;
		}
	} else {
		if (index < 1 || index > count) {
			return false;
		}
		// The object gives the name and icon.
		Row& dest = rows[index - 1];
		if (given >= 2) {
			dest.right = row.right;
		}
		if (given >= 5) {
			dest.flags = row.flags;
		}
		if (given >= 6) {
			dest.font = row.font;
		}
		if (row_disabled(index - 1) && sel_obj == objs[index - 1]) {
			sel_obj = nullptr;    // sync_rows() picks the next enabled row.
		}
	}
	update_slider();
	dirty();
	return true;
}

Game_object* Dynamic_list_widget::get_object(int index) {
	if (def.source != 0) {
		return nullptr;
	}
	const int count = get_count();
	if (index == 0) {
		const int sel = selected_row();
		return sel >= 0 ? objs[sel] : nullptr;
	}
	return index >= 1 && index <= count ? objs[index - 1] : nullptr;
}

/*
 *  Queries for the hosting gump.
 */

// Row under a screen point, 0-based, or -1.  Uses the rows as they are.
int Dynamic_list_widget::row_under(int mx, int my) const {
	int lx = mx;
	int ly = my;
	screen_to_local(lx, ly);
	return row_at(lx, ly, 0, 0, def.width, def.rows, def.row_height, first, static_cast<int>(rows.size()));
}

Game_object* Dynamic_list_widget::object_at(int mx, int my) {
	if (def.source != 0) {
		return nullptr;
	}
	sync_rows();
	const int index = row_under(mx, my);
	return index >= 0 && !row_disabled(index) ? objs[index] : nullptr;
}

/*
 *  Screen location of an object's hotspot, centred in its row's icon
 *  cell.  Output: false if the object has no visible row.
 */

bool Dynamic_list_widget::row_location(const Game_object* obj, int& ox, int& oy) const {
	const auto it = std::find(objs.begin(), objs.end(), obj);
	if (it == objs.end() || !obj->get_shape()) {
		return false;
	}
	const int index = static_cast<int>(it - objs.begin());
	if (index < first || index >= first + def.rows) {
		return false;
	}
	const Shape_frame* frame  = obj->get_shape();
	const int          icon_h = def.row_height - 2;
	int                sx     = 0;
	int                sy     = 0;
	local_to_screen(sx, sy);
	const int ry = sy + (index - first) * def.row_height;
	ox           = sx + 1 + (icon_w - frame->get_width()) / 2 + frame->get_xleft();
	oy           = ry + 1 + (icon_h - frame->get_height()) / 2 + frame->get_yabove();
	return true;
}

bool Dynamic_list_widget::is_drop_target(int mx, int my) const {
	if (def.source != 0) {
		return false;
	}
	int lx = mx;
	int ly = my;
	screen_to_local(lx, ly);
	return lx >= 0 && lx < def.width && ly >= 0 && ly < def.rows * def.row_height;
}

void Dynamic_list_widget::select_object(Game_object* obj, bool fire) {
	if (def.source != 0) {
		return;
	}
	sync_rows();
	const auto it = std::find(objs.begin(), objs.end(), obj);
	if (it == objs.end()) {
		return;
	}
	if (fire) {
		make_focused();
	}
	if (select_row(static_cast<int>(it - objs.begin())) && fire) {
		select_pending = true;    // From run(): we are inside a drag or drop.
	}
}

/*
 *  An object is leaving the container.  If it was selected, the row
 *  that takes its place (or the new last row) is selected.
 */

void Dynamic_list_widget::object_removed(Game_object* obj) {
	if (def.source != 0 || !obj || obj != sel_obj) {
		return;
	}
	const int index = selected_row();
	const int count = static_cast<int>(objs.size());
	if (index < 0) {
		return;
	}
	if (index + 1 < count) {
		sel_obj  = objs[index + 1];
		sel_hint = index;    // Its row once obj is gone.
	} else {
		sel_obj  = index > 0 ? objs[index - 1] : nullptr;
		sel_hint = std::max(index - 1, 0);
	}
	select_pending = true;
	dirty();
}

const ShapeID* Dynamic_list_widget::get_preview() {
	const int sel = selected_row();
	if (sel < 0) {
		return nullptr;
	}
	if (def.source == 0) {
		return objs[sel];
	}
	const Row& row = rows[sel];
	if (row.shape < 0) {
		return nullptr;
	}
	preview_sid.set_shape(row.shape, row.frame);
	return &preview_sid;
}

/*
 *  Scrolling.
 */

void Dynamic_list_widget::line_up() {
	scroll_to(first - 1);
}

void Dynamic_list_widget::line_down() {
	scroll_to(first + 1);
}

void Dynamic_list_widget::page_up() {
	scroll_to(first - def.rows);
}

void Dynamic_list_widget::page_down() {
	scroll_to(first + def.rows);
}

void Dynamic_list_widget::scroll_to(int newfirst) {
	newfirst = clamp_first(newfirst, get_count(), def.rows);
	if (newfirst != first) {
		first = newfirst;
		slider->set_val(first);
		dirty();
	}
}

/*
 *  Usecode.  The item is the gump's container, as for sliders and
 *  buttons.  Output: true if usecode ran; the caller must then return
 *  at once, because usecode may have closed the gump.
 */

bool Dynamic_list_widget::call_fn(int fn) {
	if (fn < 0) {
		return false;
	}
	auto* gump = dynamic_cast<Gump*>(parent);
	auto* cont = gump ? gump->get_container() : nullptr;
	auto* ucm  = gwin->get_usecode();
	if (!cont || !ucm) {
		return false;
	}
	const int fn_id = ucm->get_shape_fun(fn);
	if (fn_id < 0) {
		return false;
	}
	ucm->call_usecode(fn_id, static_cast<Game_object*>(cont), Usecode_machine::double_click);
	return true;
}

// Use the selected row: the object itself (source 0), or activate_fn.
bool Dynamic_list_widget::activate_selected() {
	const int sel = selected_row();
	if (sel < 0 || row_disabled(sel)) {
		return false;
	}
	if (def.source == 0) {
		objs[sel]->activate();
		return true;
	}
	return call_fn(def.activate_fn);
}

/*
 *  Keyboard focus between several lists on one gump: the last one
 *  clicked, else the first.
 */

void Dynamic_list_widget::make_focused() {
	focused    = true;
	auto* gump = dynamic_cast<Gump*>(parent);
	if (!gump) {
		return;
	}
	for (auto* w : gump->get_elems()) {
		auto* other = dynamic_cast<Dynamic_list_widget*>(w);
		if (other && other != this) {
			other->focused = false;
		}
	}
}

bool Dynamic_list_widget::other_list_focused() const {
	auto* gump = dynamic_cast<Gump*>(parent);
	if (!gump) {
		return false;
	}
	for (auto* w : gump->get_elems()) {
		auto* other = dynamic_cast<Dynamic_list_widget*>(w);
		if (other && other != this && other->focused) {
			return true;
		}
	}
	return false;
}

/*
 *  Mouse.  Gump::on_button() finds the list only over a row; the scroll
 *  controls get mouse_down() through Gump::forward_mouse_down().
 */

Gump_button* Dynamic_list_widget::on_button(int mx, int my) {
	sync_rows();
	pressed_row = row_under(mx, my);
	return pressed_row >= 0 ? this : nullptr;
}

bool Dynamic_list_widget::push(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	make_focused();
	// No usecode here: the caller still uses the button afterwards.
	if (pressed_row >= 0 && select_row(pressed_row)) {
		select_pending = true;
	}
	return true;
}

void Dynamic_list_widget::unpush(MouseButton button) {
	ignore_unused_variable_warning(button);
}

bool Dynamic_list_widget::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	if (select_pending) {
		select_pending = false;
		call_fn(def.select_fn);    // May close the gump: return at once.
	}
	return true;
}

void Dynamic_list_widget::double_clicked(int x, int y) {
	if (def.source != 1) {
		return;    // Source 0: Gump_manager uses the object itself.
	}
	const int index = row_under(x, y);
	if (index >= 0 && index == selected_row()) {
		activate_selected();    // May close the gump.
	}
}

bool Dynamic_list_widget::mouse_down(int mx, int my, MouseButton button) {
	if (button != MouseButton::Left || !slider) {
		return false;
	}
	switch (slider->hit_part(mx, my)) {
	case Dynamic_slider::Part::before:
		make_focused();
		page_up();
		return true;
	case Dynamic_slider::Part::after:
		make_focused();
		page_down();
		return true;
	case Dynamic_slider::Part::dec:
	case Dynamic_slider::Part::inc:
	case Dynamic_slider::Part::thumb:
		make_focused();
		return slider->mouse_down(mx, my, button);
	case Dynamic_slider::Part::none:
		break;
	}
	// On the controls but nothing to scroll: still our click.
	return slider->get_rect().has_point(mx, my);
}

bool Dynamic_list_widget::mouse_up(int mx, int my, MouseButton button) {
	return slider && slider->mouse_up(mx, my, button);
}

bool Dynamic_list_widget::mouse_drag(int mx, int my) {
	return slider && slider->mouse_drag(mx, my);
}

Gump_widget* Dynamic_list_widget::Input_first() {
	return slider && slider->Input_first() ? this : nullptr;
}

bool Dynamic_list_widget::mousewheel_up(int mx, int my) {
	if (!get_rect().has_point(mx, my)) {
		return false;
	}
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		page_up();
	} else {
		line_up();
	}
	return true;
}

bool Dynamic_list_widget::mousewheel_down(int mx, int my) {
	if (!get_rect().has_point(mx, my)) {
		return false;
	}
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		page_down();
	} else {
		line_down();
	}
	return true;
}

/*
 *  Keys: Up/Down a row, Page Up/Down a page, Home/End, Enter activates.
 *  Disabled rows are skipped.
 */

bool Dynamic_list_widget::move_selection(int dir) {
	const int count  = get_count();
	const int target = next_enabled(selected_row(), dir, count, [this](int i) {
		return row_disabled(i);
	});
	if (target >= 0 && select_row(target)) {
		make_focused();
		call_fn(def.select_fn);    // May close the gump.
		return true;
	}
	return true;
}

bool Dynamic_list_widget::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	ignore_unused_variable_warning(unicode);
	if (other_list_focused()) {
		return false;
	}
	switch (chr) {
	case SDLK_UP:
		return move_selection(-1);
	case SDLK_DOWN:
		return move_selection(1);
	case SDLK_PAGEUP:
		return move_selection(-def.rows);
	case SDLK_PAGEDOWN:
		return move_selection(def.rows);
	case SDLK_HOME:
		return move_selection(-get_count());
	case SDLK_END:
		return move_selection(get_count());
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		activate_selected();    // May close the gump.
		return true;
	default:
		return false;
	}
}

/*
 *  Each frame: the slider's arrow auto-repeat, then any callback that
 *  could not run where it was triggered.
 */

bool Dynamic_list_widget::run() {
	const bool changed = slider && slider->run();
	sync_rows();
	if (select_pending) {
		select_pending = false;
		if (call_fn(def.select_fn)) {
			return true;    // `this` may be gone.
		}
	}
	if (rows_pending) {
		rows_pending = false;
		if (def.source == 0 && call_fn(def.rows_fn)) {
			return true;    // `this` may be gone.
		}
	}
	return changed;
}

/*
 *  Paint.
 */

bool Dynamic_list_widget::has_icons() const {
	if (def.source == 0) {
		return true;
	}
	return std::any_of(rows.begin(), rows.end(), [](const Row& row) {
		return row.shape >= 0;
	});
}

int Dynamic_list_widget::text_x() const {
	return has_icons() ? icon_w + 4 : 2;
}

void Dynamic_list_widget::paint() {
	sync_rows();
	int sx = 0;
	int sy = 0;
	local_to_screen(sx, sy);
	const int count = static_cast<int>(rows.size());
	const int sel   = selected_row();

	if (count == 0) {
		if (def.flags & DYNLIST_EMPTY_TEXT) {
			const char* empty = Strings::Empty();
			sman->paint_text(
					def.font, empty, sx + (def.width + 15 - sman->get_text_width(def.font, empty)) / 2,
					sy + (def.rows * def.row_height - sman->get_text_height(def.font)) / 2);
		}
	} else {
		for (int i = 0; i < def.rows && first + i < count; i++) {
			const int n  = first + i;
			const int ry = sy + i * def.row_height;
			if (n == sel) {
				paint_shade(def.width, def.row_height, sx, ry, 85);
			} else if (n % 2) {
				paint_shade(def.width, def.row_height, sx, ry, 95);
			}
			paint_row(n, sx, ry);
			if (row_disabled(n)) {
				paint_shade(def.width, def.row_height, sx, ry, 70);
			}
		}
		if (def.flags & DYNLIST_COUNT_LINE) {
			char buf[64];
			snprintf(buf, sizeof(buf), "%d-%d %s %d", first + 1, std::min(first + def.rows, count), Strings::of(), count);
			sman->paint_text(
					count_font, buf, sx + (def.width - sman->get_text_width(count_font, buf)) / 2,
					sy + def.rows * def.row_height + 2);
		}
	}

	// The built-in controls' groove, then the controls.
	if (builtin_controls && groove_h > 0) {
		paint_shade(5, groove_h, sx + def.width + 3 + 3, sy + groove_y, 70);
	}
	if (slider) {
		slider->paint();
	}
}

// One row's icon and text; ry is the row's top on screen.
void Dynamic_list_widget::paint_row(int index, int sx, int ry) {
	const Row&     row = rows[index];
	Image_buffer8* ib  = gwin->get_win()->get_ib8();
	// The icon, centred in its cell and clipped to it.
	const ShapeID* icon = nullptr;
	ShapeID        row_icon(row.shape, row.frame, SF_SHAPES_VGA);
	if (def.source == 0) {
		icon = objs[index];
	} else if (row.shape >= 0) {
		icon = &row_icon;
	}
	const Shape_frame* frame = icon ? icon->get_shape() : nullptr;
	if (frame) {
		const int      icon_h = def.row_height - 2;
		auto           save   = ib->SaveClip();
		const TileRect cell   = TileRect(sx + 1, ry + 1, icon_w, icon_h).intersect(save.Rect());
		if (cell.w > 0 && cell.h > 0) {
			ib->set_clip(cell.x, cell.y, cell.w, cell.h);
			icon->paint_shape(
					sx + 1 + (icon_w - frame->get_width()) / 2 + frame->get_xleft(),
					ry + 1 + (icon_h - frame->get_height()) / 2 + frame->get_yabove());
		}
	}
	// Text, then the right column right-aligned.  The text sits two
	// pixels down in a 12-pixel row, and is centred the same way in
	// other heights.
	const int font   = row.font >= 0 ? row.font : def.font;
	const int ty     = ry + (def.row_height - 12) / 2 + 2;
	const int name_x = text_x();
	int       name_w = def.width - name_x - 2;
	if (!row.right.empty()) {
		const int rw = sman->get_text_width(font, row.right.c_str());
		sman->paint_text(font, row.right.c_str(), sx + def.width - 2 - rw, ty);
		name_w -= rw + 4;
	}
	const std::string text = fit_text(font, row.text, name_w);
	sman->paint_text(font, text.c_str(), sx + name_x, ty);
}

void Dynamic_list_widget::paint_shade(int w, int h, int sx, int sy, int pct) {
	update_shades(gwin);
	const Xform_palette& table = pct <= 70 ? shade70 : (pct <= 85 ? shade85 : shade95);
	gwin->get_win()->get_ib8()->fill_translucent8(0, w, h, sx, sy, table);
}

std::string Dynamic_list_widget::display_name(const Game_object* obj) {
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

std::string Dynamic_list_widget::fit_text(int fontnum, const std::string& s, int w) {
	return fit_words(s, w, [fontnum](const std::string& t) {
		return sman->get_text_width(fontnum, t.c_str());
	});
}
