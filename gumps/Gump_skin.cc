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

#include "Gump_skin.h"

#include "Mui_font.h"
#include "Mui_router.h"
#include "font.h"
#include "gamewin.h"
#include "gumpinf.h"
#include "ibuf8.h"
#include "shapeid.h"
#include "vgafile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace {
	// The sizes in mui_fonts.vga (see Mui_font.cc).
	constexpr std::array<int, 7> skin_font_sizes = {13, 16, 20, 24, 30, 36, 48};

	// Theme metrics are pixels at a 720-px display, where the 200-line
	// game is 3.6 display pixels per game pixel.
	constexpr float metric_game_px = 3.6f;

	// The close control's tab when the checkmark has none, in game pixels.
	constexpr int default_tab_w = 30;
	constexpr int default_tab_h = 35;

	// Frame ring outside the panel, in game pixels (inside the layer's
	// 4-pixel margin).
	constexpr int frame_ring = 3;

	int classic_height(int fontnum) {
		auto font = Shape_manager::get_instance()->get_font(fontnum);
		return font ? font->get_text_height() : 8;
	}
}    // namespace

namespace Gump_theme {
	std::shared_ptr<const Mui_theme> current() {
		Game_window* gwin = Game_window::get_instance();
		if (!gwin || gwin->is_in_exult_menu()) {
			return nullptr;
		}
		const std::string style = Mui::menu_style();
		if (style == "classic") {
			return nullptr;
		}
		// Mui_theme::load parses the theme file: only on a style change.
		static std::string                      cached_name;
		static std::shared_ptr<const Mui_theme> cached;
		if (style != cached_name) {
			cached_name = style;
			cached      = Mui_theme::load(style);
		}
		return cached;
	}

	bool wants(const Gump_info* info, bool list_view) {
		if (info && info->theme_mode == 0) {
			return false;
		}
		if (info && info->theme_mode == 1) {
			return true;
		}
		if (list_view) {
			return true;
		}
		return info && !info->has_snap_zones()
			   && (info->has_dynamic_buttons() || info->has_dynamic_texts() || info->has_dynamic_shapes()
				   || info->has_dynamic_sliders() || info->has_dynamic_lists());
	}
}    // namespace Gump_theme

/*
 *  The layer.
 */

Gump_skin::~Gump_skin() {
	// Game_window may already be gone at exit.
	Game_window* gwin = Game_window::get_instance();
	if (layer >= 0 && gwin) {
		gwin->destroy_layer(layer);
	}
}

bool Gump_skin::begin(const Mui_theme& th, const TileRect& b, int dx, int dy, int dw, int dh, int z) {
	Game_window* gwin = Game_window::get_instance();
	if (!gwin || dw <= 0 || dh <= 0 || b.w <= 0 || b.h <= 0) {
		return false;
	}
	if (layer < 0 || w != dw || h != dh) {
		if (layer >= 0) {
			gwin->destroy_layer(layer);
		}
		layer    = gwin->create_layer("gump skin", dw, dh, 255, 0, z);
		has_base = false;
		if (layer < 0) {
			w = h = 0;
			return false;
		}
		gwin->layer_set_ui_kind(layer, Image_window::UiLayerMenus);
		w = dw;
		h = dh;
	}
	gwin->layer_set_z(layer, z);
	gwin->layer_set_dest(layer, dx, dy, dw, dh);
	buf = gwin->get_layer_ibuf(layer);
	if (!buf) {
		return false;
	}
	buf->clear_clip();
	buf->fill8(255);
	bounds     = b;
	ox         = b.x;
	oy         = b.y;
	sx         = static_cast<float>(dw) / static_cast<float>(b.w);
	sy         = static_cast<float>(dh) / static_cast<float>(b.h);
	theme_ptr  = &th;
	panel_game = b;
	paint      = std::make_unique<Mui_painter>(buf, th, sy / metric_game_px);
	return true;
}

/*
 *  Paint the art over the skin, scaled like Mui_painter::blit_scaled:
 *  rows of the art with nothing in them are skipped.
 */

void Gump_skin::compose(Image_buffer8* art) {
	if (!art || !buf || w <= 0 || h <= 0) {
		return;
	}
	const int            aw     = static_cast<int>(art->get_width());
	const int            ah     = static_cast<int>(art->get_height());
	const int            apitch = static_cast<int>(art->get_line_width());
	const int            dpitch = static_cast<int>(buf->get_line_width());
	const unsigned char* src    = art->get_bits();
	unsigned char*       dst    = buf->get_bits();
	if (aw <= 0 || ah <= 0 || !src || !dst) {
		return;
	}
	std::vector<char> used(static_cast<size_t>(ah), 0);
	for (int y = 0; y < ah; y++) {
		const unsigned char* row     = src + y * apitch;
		used[static_cast<size_t>(y)] = std::any_of(row, row + aw, [](unsigned char pix) {
			return pix != 255;
		});
	}
	std::vector<int> xmap(static_cast<size_t>(w));
	for (int x = 0; x < w; x++) {
		xmap[static_cast<size_t>(x)] = x * aw / w;
	}
	for (int y = 0; y < h; y++) {
		const int srcy = y * ah / h;
		if (!used[static_cast<size_t>(srcy)]) {
			continue;
		}
		const unsigned char* row  = src + srcy * apitch;
		unsigned char*       drow = dst + y * dpitch;
		for (int x = 0; x < w; x++) {
			const unsigned char pix = row[xmap[static_cast<size_t>(x)]];
			if (pix != 255) {
				drow[x] = pix;
			}
		}
	}
}

void Gump_skin::end() {
	Game_window* gwin = Game_window::get_instance();
	if (layer >= 0 && gwin) {
		if (buf) {
			buf->clear_clip();
		}
		gwin->layer_set_visible(layer, true);
		gwin->layer_set_dirty(layer);
	}
}

void Gump_skin::hide() {
	Game_window* gwin = Game_window::get_instance();
	if (layer >= 0 && gwin) {
		gwin->layer_set_visible(layer, false);
	}
}

/*
 *  Geometry.
 */

int Gump_skin::gx(int x) const {
	return static_cast<int>(std::lround(static_cast<float>(x - ox) * sx));
}

int Gump_skin::gy(int y) const {
	return static_cast<int>(std::lround(static_cast<float>(y - oy) * sy));
}

TileRect Gump_skin::to_skin(const TileRect& game) const {
	const int x0 = gx(game.x);
	const int y0 = gy(game.y);
	return TileRect(x0, y0, gx(game.x + game.w) - x0, gy(game.y + game.h) - y0);
}

TileRect Gump_skin::tab_rect(const TileRect& check_rect, const TileRect& check_tab) const {
	if (check_tab.w > 0 && check_tab.h > 0) {
		return check_tab;
	}
	if (check_rect.w <= 0 || check_rect.h <= 0) {
		return TileRect(0, 0, 0, 0);
	}
	const TileRect tab(
			check_rect.x + check_rect.w / 2 - default_tab_w / 2, check_rect.y + check_rect.h / 2 - default_tab_h / 2, default_tab_w,
			default_tab_h);
	return tab.intersect(bounds);
}

/*
 *  The panel cache.
 */

bool Gump_skin::base_valid(const TileRect& panel_rect, const TileRect& tab_rect, const TileRect& well_rect) {
	panel_game        = panel_rect;
	pending_key.theme = theme_ptr;
	pending_key.name  = theme_ptr ? theme_ptr->name : std::string();
	pending_key.w     = w;
	pending_key.h     = h;
	pending_key.panel = to_skin(panel_rect);
	pending_key.tab   = to_skin(tab_rect);
	pending_key.well  = to_skin(well_rect);
	if (!buf || !has_base || !(base_key == pending_key) || base.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) {
		return false;
	}
	const int pitch = static_cast<int>(buf->get_line_width());
	for (int y = 0; y < h; y++) {
		std::memcpy(
				buf->get_bits() + y * pitch, base.data() + static_cast<size_t>(y) * static_cast<size_t>(w), static_cast<size_t>(w));
	}
	return true;
}

void Gump_skin::store_base() {
	if (!buf) {
		return;
	}
	base.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
	const int pitch = static_cast<int>(buf->get_line_width());
	for (int y = 0; y < h; y++) {
		std::memcpy(
				base.data() + static_cast<size_t>(y) * static_cast<size_t>(w), buf->get_bits() + y * pitch, static_cast<size_t>(w));
	}
	base_key = pending_key;
	has_base = true;
}

/*
 *  Painting helpers.
 */

void Gump_skin::panel(const TileRect& r, const TileRect& tab) {
	panel_game = r;
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	Mui_painter&   p     = *paint;
	const auto&    th    = *theme_ptr;
	const TileRect page  = to_skin(r);
	TileRect       outer = page;
	outer.enlarge(static_cast<int>(std::lround(frame_ring * std::min(sx, sy))));
	const int bev = std::max(1, p.px(2));
	if (th.frame_stone) {
		const uint8 base_c = th.colour(Mui_colour::frame);
		const uint8 light  = th.colour(Mui_colour::frame_light);
		const uint8 dark   = th.colour(Mui_colour::frame_dark);
		const uint8 mortar = th.colour(Mui_colour::window);
		// The tab hangs out from under the frame.
		if (tab.w > 0 && tab.h > 0) {
			const TileRect t = to_skin(tab);
			p.texture_stone(t, base_c, light, dark, mortar);
			p.bevel(t, bev, light, dark);
		}
		p.texture_stone(outer, base_c, light, dark, mortar);
		p.bevel(outer, bev, light, dark);
		// Gold rule round the page, as Mui_window::paint_frame.
		TileRect rule = page;
		rule.enlarge(bev);
		p.frame_rect(rule, std::max(1, p.px(1.5f)), th.colour(Mui_colour::accent));
		p.texture_parchment(page, th.colour(Mui_colour::panel), th.colour(Mui_colour::panel), th.colour(Mui_colour::panel_alt));
		p.bevel(page, bev, dark, light);
	} else {
		const int rad = p.px(th.metrics.radius * 2);
		p.rounded_fill(outer, rad, th.colour(Mui_colour::window));
		p.rounded_frame(outer, rad, std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame));
		p.rounded_fill(page, p.px(th.metrics.radius), th.colour(Mui_colour::panel));
	}
}

void Gump_skin::well(const TileRect& r) {
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	Mui_painter&   p  = *paint;
	const auto&    th = *theme_ptr;
	const TileRect t  = to_skin(r);
	if (th.frame_stone) {
		p.fill_rect(t, th.colour(Mui_colour::panel_alt));
		p.bevel(t, std::max(1, p.px(2)), th.colour(Mui_colour::frame_dark), th.colour(Mui_colour::frame_light));
	} else {
		p.rounded_fill(t, p.px(th.metrics.radius), th.colour(Mui_colour::panel_alt));
		p.rounded_frame(t, p.px(th.metrics.radius), std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame));
	}
}

void Gump_skin::fill(const TileRect& r, Mui_colour c) {
	paint->fill_rect(to_skin(r), theme_ptr->colour(c));
}

/*
 *  The theme face for a classic font's role (taller than font 2 =
 *  heading, shorter = small, else body), in the largest size that is no
 *  taller than the classic font on the display.
 */

const Mui_text_font* Gump_skin::font_for(int classic_font) const {
	const int     ch     = classic_height(classic_font);
	const int     body_h = classic_height(2);
	Mui_font_role role   = Mui_font_role::body;
	if (ch > body_h) {
		role = Mui_font_role::heading;
	} else if (ch < body_h) {
		role = Mui_font_role::small;
	}
	const Mui_face       face  = theme_ptr->font(role).face;
	const int            limit = static_cast<int>(static_cast<float>(ch) * sy);
	const Mui_text_font* best  = nullptr;
	const Mui_text_font* small = nullptr;
	for (const int size : skin_font_sizes) {
		const Mui_text_font* f = Mui_fonts::get().pick(face, size);
		if (!f) {
			continue;
		}
		if (!small) {
			small = f;
		}
		if (f->height() <= limit) {
			best = f;
		}
	}
	return best ? best : small;
}

int Gump_skin::text(
		const std::string& s, int ax, int ay, int halign, int valign, int classic_font, Mui_colour fg, Mui_colour bg,
		const TileRect* lim) {
	const Mui_text_font* f = s.empty() ? nullptr : font_for(classic_font);
	if (!f) {
		return 0;
	}
	const TileRect& r = lim ? *lim : panel_game;
	int             avail;
	if (halign == 1) {
		avail = ax - r.x;
	} else if (halign == 2) {
		avail = 2 * std::min(ax - r.x, r.x + r.w - ax);
	} else {
		avail = r.x + r.w - ax;
	}
	const int         max_w = std::max(0, static_cast<int>(static_cast<float>(avail) * sx));
	const std::string t     = f->width(s) <= max_w ? s : f->elide(s, max_w);
	const int         tw    = f->width(t);
	int               x     = gx(ax);
	int               y     = gy(ay);
	if (halign == 1) {
		x -= tw;
	} else if (halign == 2) {
		x -= tw / 2;
	}
	if (valign == 1) {
		y -= f->height();
	} else if (valign == 2) {
		y -= f->height() / 2;
	}
	f->draw(buf, t, x, y, theme_ptr->colour(fg), theme_ptr->colour(bg));
	return static_cast<int>(std::ceil(static_cast<float>(tw) / sx));
}

void Gump_skin::plate(const TileRect& r, bool pushed) {
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	Mui_painter&   p  = *paint;
	const auto&    th = *theme_ptr;
	const TileRect t  = to_skin(r);
	if (th.frame_stone) {
		const uint8 light = th.colour(Mui_colour::frame_light);
		const uint8 dark  = th.colour(Mui_colour::frame_dark);
		p.fill_rect(t, th.colour(pushed ? Mui_colour::accent_hover : Mui_colour::frame));
		p.bevel(t, std::max(1, p.px(2)), pushed ? dark : light, pushed ? light : dark);
	} else {
		const int rad = p.px(th.metrics.radius);
		p.rounded_fill(t, rad, th.colour(pushed ? Mui_colour::accent_hover : Mui_colour::panel_alt));
		p.rounded_frame(t, rad, std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame));
	}
}

void Gump_skin::arrow(const TileRect& r, Mui_icon which, bool pushed) {
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	plate(r, pushed);
	const auto& th  = *theme_ptr;
	Mui_colour  ink = Mui_colour::text;
	if (th.frame_stone) {
		ink = pushed ? Mui_colour::text_on_accent : Mui_colour::title_text;
	}
	paint->icon(which, to_skin(r), th.colour(ink));
}

void Gump_skin::groove(const TileRect& r, bool vertical) {
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	Mui_painter& p  = *paint;
	const auto&  th = *theme_ptr;
	TileRect     t  = to_skin(r);
	// A thin groove along the middle of the track.
	if (vertical) {
		const int thick = std::min(t.w, std::max(p.px(6), t.w / 3));
		t               = TileRect(t.x + (t.w - thick) / 2, t.y, thick, t.h);
	} else {
		const int thick = std::min(t.h, std::max(p.px(6), t.h / 3));
		t               = TileRect(t.x, t.y + (t.h - thick) / 2, t.w, thick);
	}
	if (th.frame_stone) {
		p.fill_rect(t, th.colour(Mui_colour::panel_alt));
		p.bevel(t, std::max(1, p.px(1)), th.colour(Mui_colour::frame_dark), th.colour(Mui_colour::frame_light));
	} else {
		const int rad = std::min(t.w, t.h) / 2;
		p.rounded_fill(t, rad, th.colour(Mui_colour::panel_alt));
		p.rounded_frame(t, rad, std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame_dark));
	}
}

void Gump_skin::knob(const TileRect& r) {
	if (r.w <= 0 || r.h <= 0) {
		return;
	}
	Mui_painter&   p  = *paint;
	const auto&    th = *theme_ptr;
	const TileRect t  = to_skin(r);
	if (th.frame_stone) {
		p.fill_rect(t, th.colour(Mui_colour::accent));
		p.bevel(t, std::max(1, p.px(2)), th.colour(Mui_colour::accent_hover), th.colour(Mui_colour::frame_dark));
	} else {
		p.rounded_fill(t, std::min(t.w, t.h) / 2, th.colour(Mui_colour::accent));
	}
}

void Gump_skin::close_control(const TileRect& check_rect, bool pushed) {
	if (check_rect.w <= 0 || check_rect.h <= 0) {
		return;
	}
	Mui_painter&   p    = *paint;
	const auto&    th   = *theme_ptr;
	const TileRect t    = to_skin(check_rect);
	const int      side = std::max(t.w, t.h);
	const TileRect sq(t.x + (t.w - side) / 2, t.y + (t.h - side) / 2, side, side);
	if (th.frame_stone) {
		const uint8 light = th.colour(Mui_colour::frame_light);
		const uint8 dark  = th.colour(Mui_colour::frame_dark);
		p.fill_rect(sq, th.colour(Mui_colour::frame));
		p.bevel(sq, std::max(1, p.px(2)), pushed ? dark : light, pushed ? light : dark);
		p.icon(Mui_icon::check, sq, th.colour(pushed ? Mui_colour::accent_hover : Mui_colour::accent));
	} else {
		p.rounded_fill(sq, side / 2, th.colour(pushed ? Mui_colour::accent_hover : Mui_colour::panel_alt));
		p.rounded_frame(sq, side / 2, std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame));
		p.icon(Mui_icon::close, sq, th.colour(Mui_colour::text));
	}
}

/*
 *  The opaque pixels' bounding box of a frame: painted into a scratch
 *  buffer filled with the transparent index, then scanned.
 */

TileRect Gump_skin::art_bounds(const ShapeID& sid) {
	static std::map<std::tuple<int, int, int>, TileRect> cache;
	const auto key = std::make_tuple(static_cast<int>(sid.get_shapefile()), sid.get_shapenum(), sid.get_framenum());
	auto       it  = cache.find(key);
	if (it != cache.end()) {
		return it->second;
	}
	TileRect     r(0, 0, 0, 0);
	Shape_frame* frame = sid.get_shape();
	const int    fw    = frame ? frame->get_width() : 0;
	const int    fh    = frame ? frame->get_height() : 0;
	if (fw > 0 && fh > 0) {
		Image_buffer8 tmp{static_cast<unsigned>(fw), static_cast<unsigned>(fh)};
		tmp.fill8(255);
		if (frame->is_rle()) {
			frame->paint_rle(&tmp, frame->get_xleft(), frame->get_yabove());
		} else {
			frame->paint(&tmp, frame->get_xleft(), frame->get_yabove());
		}
		const unsigned char* bits  = tmp.get_bits();
		const int            pitch = static_cast<int>(tmp.get_line_width());
		int                  x0    = fw;
		int                  y0    = fh;
		int                  x1    = -1;
		int                  y1    = -1;
		for (int y = 0; y < fh; y++) {
			for (int x = 0; x < fw; x++) {
				if (bits[y * pitch + x] != 255) {
					x0 = std::min(x0, x);
					x1 = std::max(x1, x);
					y0 = std::min(y0, y);
					y1 = std::max(y1, y);
				}
			}
		}
		if (x1 >= 0) {
			r = TileRect(x0 - frame->get_xleft(), y0 - frame->get_yabove(), x1 - x0 + 1, y1 - y0 + 1);
		}
	}
	cache[key] = r;
	return r;
}
