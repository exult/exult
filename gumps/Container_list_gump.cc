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
#include "Dynamic_container_gump.h"
#include "Dynamic_list_widget.h"
#include "Dynamic_shape_widget.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "cheat.h"
#include "contain.h"
#include "exult_flx.h"
#include "frflags.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"
#include "misc_buttons.h"
#include "objiter.h"
#include "shapeinf.h"

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

		static auto DoubleClickToUse() {
			return get_text_msg(0x6D4 - msg_file_start);
		}

		static auto DragToTake() {
			return get_text_msg(0x6D5 - msg_file_start);
		}
	};

	// Ink colour of the scroll art, for rules and the divider.
	constexpr unsigned char ink = 0x8C;

	using List_button = CallbackButton<Container_list_gump>;

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

// The background's gumps.vga shape: the layout's, else the game's scroll.
int Container_list_gump::background_shape(const List_layout& L) {
	return L.bg_shape >= 0 ? L.bg_shape : game->get_shape("gumps/scroll");
}

/*
 *  Create the list view for a container.  shnum is the container's own
 *  gump shape.
 */

Container_list_gump::Container_list_gump(Container_game_object* cont, int initx, int inity, int shnum)
		: Gump(cont, initx, inity, shnum), layout(Gump_info::get_list_layout(shnum)),
		  background(background_shape(layout), 0, SF_GUMPS_VGA) {
	if (!can_widen(layout)) {
		layout.extra_w = 0;    // Draw the background as it is.
	}
	check = new Checkmark_button(this, 0, 0);
	add_elem(check);
	set_picture();
	// The base constructor placed the container's own picture (at its
	// saved spot, if any).  Keep it there and hang the background above it.
	x -= pic_x;
	y -= pic_y;
	// The rows and their scroll controls, and the selected item enlarged.
	list = new Dynamic_list_widget(this, layout.list);
	add_elem(list);
	Dynamic_shape_def preview;
	preview.field_id  = -1;
	preview.pos_x     = layout.preview_x;
	preview.pos_y     = layout.preview_y;
	preview.shape_num = 0;    // Nothing when nothing is selected.
	preview.list_id   = layout.list.field_id;
	preview.box_w     = layout.preview_w;
	preview.box_h     = layout.preview_h;
	add_elem(new Dynamic_shape_widget(this, preview));
	if (layout.nav_left_x >= 0 && layout.nav_right_x >= 0 && layout.nav_y >= 0) {
		add_elem(new List_button(
				this, &Container_list_gump::prev_container, EXULT_FLX_SCROLL_LEFT_SHP,
				button_x(EXULT_FLX_SCROLL_LEFT_SHP, layout.nav_left_x), button_y(EXULT_FLX_SCROLL_LEFT_SHP, layout.nav_y),
				SF_EXULT_FLX));
		add_elem(new List_button(
				this, &Container_list_gump::next_container, EXULT_FLX_SCROLL_RIGHT_SHP,
				button_x(EXULT_FLX_SCROLL_RIGHT_SHP, layout.nav_right_x), button_y(EXULT_FLX_SCROLL_RIGHT_SHP, layout.nav_y),
				SF_EXULT_FLX));
	}
	if (cont) {
		root = cont->weak_from_this();
	}
}

/*
 *  Gump::~Gump saves the position of the container's picture from x, y,
 *  so move x, y back to the picture first.
 */

Container_list_gump::~Container_list_gump() {
	x += pic_x;
	y += pic_y;
}

/*
 *  Lay out the current container's picture: its hotspot, object area
 *  and checkmark, as Container_gump would, shifted to hang off the
 *  background's bottom-left.
 */

void Container_list_gump::set_picture() {
	const int shnum = get_shapenum();
	TileRect  area{};
	int       checkx = 0;
	int       checky = 0;
	Container_gump::get_layout(shnum, area, checkx, checky);
	if (shnum >= 0 && get_shapefile() == SF_GUMPS_VGA) {
		read_gump_info(shnum, area, checkx, checky);
	}
	const Shape_frame* frame = get_shape();
	const int          w     = frame ? frame->get_width() : 0;
	pic_x                    = picture_left(layout, w) + (frame ? frame->get_xleft() : 0);
	pic_y                    = layout.pic_top + (frame ? frame->get_yabove() : 0);
	area.shift(pic_x, pic_y);
	object_area = area;
	check->set_pos(pic_x + checkx, pic_y + checky);
}

// Is a screen point on the picture's art?
bool Container_list_gump::on_picture(int sx, int sy) const {
	const Shape_frame* frame = get_shape();
	return frame && frame->has_point(sx - x - pic_x, sy - y - pic_y);
}

/*
 *  The layout needs the background frame it was made for (one frame of
 *  its size, hotspot at its top-left).  A mod that replaces the art
 *  without a layout to match gets the picture view, as does a gump
 *  whose layout is off.
 */

bool Container_list_gump::is_available(int shnum) {
	const List_layout L = Gump_info::get_list_layout(shnum);
	if (L.off) {
		return false;
	}
	const int bg = background_shape(L);
	if (bg < 0) {
		return false;
	}
	const ShapeID      sid(bg, 0, SF_GUMPS_VGA);
	const Shape_frame* frame = sid.get_shape();
	return frame && frame->get_width() == L.bg_width && frame->get_height() == L.bg_height && frame->get_xleft() == 0
		   && frame->get_yabove() == 0;
}

Gump* Container_list_gump::clone(Container_game_object* cont, int initx, int inity) {
	return new Container_list_gump(cont, initx, inity, get_shapenum());
}

/*
 *  Screen area: the widened background and the container's picture.
 */

TileRect Container_list_gump::get_rect() const {
	TileRect           rect(x, y, total_w(layout), layout.bg_height);
	const Shape_frame* frame = get_shape();
	if (frame) {
		rect = rect.add(
				TileRect(x + pic_x - frame->get_xleft(), y + pic_y - frame->get_yabove(), frame->get_width(), frame->get_height()));
	}
	return rect;
}

bool Container_list_gump::has_point(int sx, int sy) const {
	const int lx = sx - x;
	const int ly = sy - y;
	if (lx >= 0 && lx < total_w(layout) && ly >= 0 && ly < layout.bg_height) {
		const Shape_frame* frame = background.get_shape();
		if (frame && frame->has_point(source_column(layout, lx), ly)) {
			return true;
		}
	}
	if (on_picture(sx, sy)) {
		return true;
	}
	for (const auto* w : elems) {
		if (w && w->on_widget(sx, sy)) {
			return true;
		}
	}
	return false;
}

/*
 *  The gump and the picture's objects (which may stick out of it).
 */

TileRect Container_list_gump::get_dirty() {
	TileRect rect = get_rect();
	if (!container) {
		return rect;
	}
	Object_iterator next(container->get_objects());
	Game_object*    obj;
	while ((obj = next.get_next()) != nullptr) {
		const TileRect orect = Gump::get_shape_rect(obj);
		if (orect.w > 0 && orect.h > 0) {
			rect = rect.add(orect);
		}
	}
	return rect;
}

/*
 *  Where an object is drawn: in the picture if it was found there, or
 *  if its row is scrolled out of view; else centred in its row's icon
 *  cell.
 */

void Container_list_gump::get_shape_location(const Game_object* obj, int& ox, int& oy) const {
	if (obj != picture_hit && obj->get_shape()) {
		list->sync_rows();
		if (list->row_location(obj, ox, oy)) {
			return;
		}
	}
	Gump::get_shape_location(obj, ox, oy);
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
 *  In the list, the whole row is the hit target.  In the picture, the
 *  top-most object under the point, as in the picture view.
 */

Game_object* Container_list_gump::find_object(int mx, int my) {
	picture_hit      = nullptr;
	Game_object* row = list->object_at(mx, my);
	if (row) {
		return row;
	}
	if (!container) {
		return nullptr;
	}
	Game_object*    found = nullptr;
	Object_iterator next(container->get_objects());
	Game_object*    obj;
	while ((obj = next.get_next()) != nullptr) {
		const Shape_frame* frame = obj->get_shape();
		if (!frame || !Gump::get_shape_rect(obj).has_point(mx, my)) {
			continue;
		}
		int ox;
		int oy;
		Gump::get_shape_location(obj, ox, oy);
		if (frame->has_point(mx - ox, my - oy)) {
			found = obj;    // Keep the last: it's painted on top.
		}
	}
	picture_hit = found;
	return found;
}

void Container_list_gump::object_pressed(Game_object* obj) {
	list->select_object(obj, true);
}

/*
 *  Add an object.  A drop on a row or on an object in the picture goes
 *  to that object, as in the picture view (into a bag, or onto a
 *  matching stack).  A drop elsewhere on the picture stays where it was
 *  dropped.  Anything else goes into the container with tx/ty unset, so
 *  the picture places it.  If mx, my, sx, sy are all -2, tx/ty are
 *  already correct.
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
		list->select_object(onobj, true);
		return true;
	}
	if (!container->add(obj, dont_check)) {    // DON'T combine here.
		return false;
	}
	const bool keep_pos = mx == -2 && my == -2 && sx == -2 && sy == -2;
	if (!keep_pos) {
		if (sx != -1 && sy != -1 && on_picture(mx, my)) {
			place_object(obj, sx, sy);
		} else {
			obj->set_shape_pos(255, 255);
		}
	}
	list->select_object(obj, true);
	return true;
}

/*
 *  Remove an object.  If it was selected, the row that takes its place
 *  (or the new last row) is selected.
 */

void Container_list_gump::remove(Game_object* obj) {
	if (obj == picture_hit) {
		picture_hit = nullptr;
	}
	list->object_removed(obj);
	Gump::remove(obj);
}

/*
 *  Paint the background, the list and the selected item.
 */

void Container_list_gump::paint() {
	paint_background();
	// The container's picture, over the bottom roller's left end.
	if (get_shape()) {
		paint_shape(x + pic_x, y + pic_y);
	}
	gwin->set_painted();

	list->sync_rows();
	paint_title();
	paint_panel();
	// The rows and scroll controls, the preview and the arrows.
	paint_elems();
	// The picture's objects, placed and drawn as in the picture view.
	paint_objects();

	// Outline selections in this gump, in the list and in the picture.
	const Game_object_shared_vector& sel_objs = cheat.get_selected();
	for (const auto& it : sel_objs) {
		Game_object* obj = it.get();
		if (container != obj->get_owner()) {
			continue;
		}
		int ox;
		int oy;
		if (list->row_location(obj, ox, oy)) {
			obj->ShapeID::paint_outline(ox, oy, HIT_PIXEL);
		}
		Gump::get_shape_location(obj, ox, oy);
		obj->ShapeID::paint_outline(ox, oy, HIT_PIXEL);
	}
}

/*
 *  Run the widgets each frame: the list's callbacks and the scroll
 *  arrows' auto-repeat.  Called from Gump_manager::update_gumps().
 */

void Container_list_gump::update_gump() {
	const unsigned changes = gumpman->get_chain_changes();
	for (auto* w : elems) {
		if (w && w->run()) {
			gwin->set_all_dirty();
			// Usecode run by the widget may have closed this gump (or
			// opened another): touch nothing more of `this`.
			if (gumpman->get_chain_changes() != changes) {
				return;
			}
		}
	}
}

/*
 *  Paint the background in three slices: its left part, the plain strip
 *  repeated, and its right part shifted right.
 */

void Container_list_gump::paint_background() {
	Image_buffer8*  ib   = gwin->get_win()->get_ib8();
	auto            save = ib->SaveClip();
	const TileRect& clip = save.Rect();

	auto draw = [&](int dest_x, int w, int shift) {
		const TileRect r = TileRect(x + dest_x, y, w, layout.bg_height).intersect(clip);
		if (r.w > 0 && r.h > 0) {
			ib->set_clip(r.x, r.y, r.w, r.h);
			background.paint_shape(x + shift, y);
		}
	};
	if (layout.extra_w <= 0) {
		draw(0, layout.bg_width, 0);
		return;
	}
	const int strip_x0 = layout.strip_x0;
	const int strip_x1 = layout.strip_x1;
	const int extra_w  = layout.extra_w;
	draw(0, strip_x1, 0);
	const int strip_w = strip_x1 - strip_x0;
	for (int d = strip_x1; d < strip_x1 + extra_w; d += strip_w) {
		draw(d, std::min(strip_w, strip_x1 + extra_w - d), d - strip_x0);
	}
	draw(strip_x1 + extra_w, layout.bg_width - strip_x1, extra_w);
}

/*
 *  The container's name over the list, with a rule under it, and the
 *  dotted divider between the columns.
 */

void Container_list_gump::paint_title() {
	Image_buffer8* ib   = gwin->get_win()->get_ib8();
	const int      tx   = x + layout.title_x;
	const int      ty   = y + layout.title_y;
	const int      tw   = layout.title_w;
	const int      font = layout.title_font;

	// Centred between the arrows.
	const std::string title = Dynamic_list_widget::fit_text(
			font, container ? Dynamic_list_widget::display_name(container) : std::string(), tw - 28);
	sman->paint_text(font, title.c_str(), tx + (tw - sman->get_text_width(font, title.c_str())) / 2, ty - 2);
	if (tw > 8) {
		ib->fill_hline8(ink, tw - 8, tx + 4, ty + 8);
	}

	// A faint dotted divider between the two columns.
	for (int dy = layout.divider_y0; dy < layout.divider_y1; dy++) {
		if (dy % 3) {
			ib->put_pixel8(ink, x + layout.divider_x, y + dy);
		}
	}
}

/*
 *  Right column: the selected item's name and details.  The bound shape
 *  widget paints it enlarged.
 */

void Container_list_gump::paint_panel() {
	Game_object* obj = list->get_object(0);
	if (!obj) {
		return;    // Empty container: plain parchment.
	}
	Image_buffer8* ib    = gwin->get_win()->get_ib8();
	const int      px    = x + layout.panel_x;
	const int      ty    = y + layout.panel_y;
	const int      pw    = layout.panel_w;
	const int      font  = layout.panel_font;
	const int      small = layout.panel_small_font;

	const std::string name = Dynamic_list_widget::fit_text(font, Dynamic_list_widget::display_name(obj), pw - 2);
	sman->paint_text(font, name.c_str(), px + (pw - sman->get_text_width(font, name.c_str())) / 2, ty - 2);
	if (pw > 16) {
		ib->fill_hline8(ink, pw - 16, px + 8, ty + 8);
	}

	char      quantity[48];
	char      weight[48];
	const int wt = obj->get_weight();    // In tenths of a stone.
	snprintf(quantity, sizeof(quantity), "%s %d", Strings::Quantity_(), obj->get_quantity());
	snprintf(weight, sizeof(weight), "%s %d.%d", Strings::Weight_(), wt / 10, wt % 10);
	const char* lines[] = {quantity, weight, "", Strings::DoubleClickToUse(), Strings::DragToTake()};
	int         ly      = ty + 66;
	for (const char* line : lines) {
		if (*line) {
			const std::string s = Dynamic_list_widget::fit_text(small, line, pw);
			sman->paint_text(small, s.c_str(), px + (pw - sman->get_text_width(small, s.c_str())) / 2, ly);
		}
		ly += 8;
	}
}

/*
 *  Scrolling: the wheel anywhere on the gump scrolls the list.
 */

bool Container_list_gump::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		list->page_up();
	} else {
		list->line_up();
	}
	return true;
}

bool Container_list_gump::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	if (SDL_GetModState() & SDL_KMOD_ALT) {
		list->page_down();
	} else {
		list->line_down();
	}
	return true;
}

/*
 *  Could the arrows step into this object?  Only if double-clicking it
 *  would open a plain container gump (the checks of
 *  Container_game_object::show_gump and Gump_manager::add_gump) that may
 *  be shown as a list.
 */

bool Container_list_gump::can_step_into(Game_object* obj) {
	if (!obj->as_container() || obj->as_actor()) {
		return false;
	}
	const Shape_info& inf = obj->get_info();
	if (inf.has_object_flag(obj->get_framenum(), inf.has_quality() ? obj->get_quality() : -1, Frame_flags::fp_force_usecode)) {
		return false;    // Locked, or usecode decides.
	}
	const int gump = inf.get_gump_shape();
	if (gump < 0 || Dynamic_container_gump::has_config(gump) || Gump_info::get_list_layout(gump).off) {
		return false;
	}
	for (const char* special : {"gumps/spellbook", "gumps/jawbone", "gumps/statsdisplay", "gumps/spell_scroll"}) {
		if (gump == game->get_shape(special)) {
			return false;
		}
	}
	return true;
}

// The containers inside cont, depth-first, in object order.
void Container_list_gump::collect_nested(Container_game_object* cont, std::vector<Container_game_object*>& out) {
	Object_iterator next(cont->get_objects());
	Game_object*    obj;
	while ((obj = next.get_next()) != nullptr) {
		if (can_step_into(obj)) {
			Container_game_object* inner = obj->as_container();
			out.push_back(inner);
			collect_nested(inner, out);
		}
	}
}

void Container_list_gump::prev_container() {
	step(-1);
}

void Container_list_gump::next_container() {
	step(1);
}

/*
 *  Step through the gump's own container followed by the containers
 *  inside it.  The ends stop.  If the shown container is no longer
 *  among them, go back to the gump's own.
 */

void Container_list_gump::step(int delta) {
	const auto             locked = root.lock();
	Container_game_object* top    = locked ? locked->as_container() : nullptr;
	if (!top) {
		return;
	}
	std::vector<Container_game_object*> nested{top};
	collect_nested(top, nested);
	const auto it     = std::find(nested.begin(), nested.end(), container);
	const int  last   = static_cast<int>(nested.size()) - 1;
	const int  target = it == nested.end() ? 0 : std::clamp(static_cast<int>(it - nested.begin()) + delta, 0, last);
	if (nested[target] != container) {
		show_container(nested[target]);
	}
}

/*
 *  Show another container: its contents, title and picture.  The
 *  background and the layout stay as they are.
 */

void Container_list_gump::show_container(Container_game_object* cont) {
	gwin->add_dirty(get_dirty());
	container = cont;
	set_shape(cont->get_info().get_gump_shape(), 0);
	set_picture();
	picture_hit = nullptr;
	list->reset();
	gwin->set_all_dirty();
}
