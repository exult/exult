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

#include "Mui_window.h"

#include "Configuration.h"
#include "Mui_font.h"
#include "Mui_router.h"
#include "Mui_selftest.h"
#include "exult.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"
#include "mouse.h"
#include "touchui.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iostream>

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Ultima() {
			return get_text_msg(0xC01 - msg_file_start);
		}

		static auto Modern() {
			return get_text_msg(0xC02 - msg_file_start);
		}

		static auto Close() {
			return get_text_msg(0xC07 - msg_file_start);
		}

		static auto KeyboardHint() {
			return get_text_msg(0xC08 - msg_file_start);
		}

		static auto OK() {
			return get_text_msg(0xC09 - msg_file_start);
		}

		static auto Cancel() {
			return get_text_msg(0xC0A - msg_file_start);
		}

		static auto StyleHelp() {
			return get_text_msg(0xC1B - msg_file_start);
		}

		static auto CloseHelp() {
			return get_text_msg(0xC1C - msg_file_start);
		}
	};

	Image_window* iwin() {
		return Game_window::get_instance()->get_win();
	}
}    // namespace

Mui_window::Mui_window(std::string title_) : Modal_gump(nullptr, -1), title(std::move(title_)) {
	no_dragging = true;
	theme_ptr   = Mui_theme::load(Mui::menu_style());
	if (!theme_ptr) {
		theme_ptr = Mui_theme::load("modern");
	}
}

Mui_window::~Mui_window() {
	Mui_selftest::forget(this);
	if (text_input_on) {
		SDL_StopTextInput(iwin()->get_screen_window());
	}
	destroy_layers();
}

/*
 *  Layers: one full-display 8-bit layer for the window and a 1x1 scrim
 *  stretched over the display below it.
 */

bool Mui_window::ensure_layers() {
	Image_window* w  = iwin();
	const int     dw = w->get_display_width();
	const int     dh = w->get_display_height();
	if (dw <= 0 || dh <= 0) {
		return false;
	}
	if (layer >= 0 && dw == layer_w && dh == layer_h) {
		return true;
	}
	destroy_layers();
	layer_w = dw;
	layer_h = dh;
	layer   = w->create_layer("mui", dw, dh, 255, 0, z_hint);
	scrim   = w->create_layer("mui scrim", 1, 1, 255, 0, z_hint - 1);
	if (layer < 0 || scrim < 0) {
		destroy_layers();
		return false;
	}
	w->layer_set_ui_kind(layer, Image_window::UiLayerMenus);
	w->layer_set_dest(layer, 0, 0, dw, dh);
	w->layer_set_ui_kind(scrim, Image_window::UiLayerMenus);
	w->layer_set_dest(scrim, 0, 0, dw, dh);
	w->layer_set_alpha(scrim, static_cast<unsigned char>(theme_ptr->backdrop_alpha));
	w->get_layer_ibuf(scrim)->fill8(theme_ptr->colour(Mui_colour::backdrop));
	w->layer_set_dirty(scrim);
	scale = Mui_theme::ui_scale(dw, dh);
	dirty = true;
	// Size changed: relayout (the widget tree is kept).
	if (!need_build) {
		layout_all();
	}
	return true;
}

void Mui_window::destroy_layers() {
	Image_window* w = iwin();
	if (layer >= 0) {
		w->destroy_layer(layer);
	}
	if (scrim >= 0) {
		w->destroy_layer(scrim);
	}
	layer = scrim = -1;
	layer_w = layer_h = 0;
}

void Mui_window::hide_layers() {
	Image_window* w = iwin();
	if (layer >= 0) {
		w->layer_set_visible(layer, false);
	}
	if (scrim >= 0) {
		w->layer_set_visible(scrim, false);
	}
	gwin->set_all_dirty();
}

void Mui_window::run_hidden(const std::function<void()>& fn) {
	if (text_input_on) {
		SDL_StopTextInput(iwin()->get_screen_window());
		text_input_on = false;
	}
	hide_layers();
	gwin->paint();
	fn();
	dirty = true;
	gwin->set_all_dirty();
	update_text_input();
}

void Mui_window::set_layer_z_hint(int z) {
	z_hint = z;
	if (layer >= 0) {
		iwin()->layer_set_z(layer, z);
		iwin()->layer_set_z(scrim, z - 1);
	}
}

bool Mui_window::display_to_local(int sx, int sy, int& lx, int& ly) const {
	if (layer < 0) {
		lx = sx;
		ly = sy;
		return true;
	}
	iwin()->screen_to_layer(layer, sx, sy, lx, ly);
	return true;
}

bool Mui_window::has_point(int x, int y) const {
	ignore_unused_variable_warning(x, y);
	return true;
}

TileRect Mui_window::get_rect() const {
	return TileRect(0, 0, gwin->get_width(), gwin->get_height());
}

/*
 *  Building and layout.
 */

std::string Mui_window::default_help() const {
	return Strings::KeyboardHint();
}

void Mui_window::post(std::function<void()> fn) {
	pending.push_back(std::move(fn));
}

void Mui_window::process_pending() {
	while (!pending.empty()) {
		auto todo = std::move(pending);
		pending.clear();
		for (auto& fn : todo) {
			fn();
		}
	}
	if (need_build && layer >= 0) {
		rebuild_now();
	}
}

void Mui_window::rebuild() {
	need_build = true;
	dirty      = true;
}

void Mui_window::rebuild(const std::string& focus_id) {
	requested_focus = focus_id;
	rebuild();
}

void Mui_window::clear_pointers() {
	hover   = nullptr;
	pressed = nullptr;
	if (focus) {
		focus_id_for_rebuild = focus->id;
	}
	focus = nullptr;
}

void Mui_window::rebuild_now() {
	need_build = false;
	if (!theme_adjusted) {
		theme_adjusted = true;
		auto t         = std::make_shared<Mui_theme>(*theme_ptr);
		adjust_theme(*t);
		theme_ptr = t;
	}
	clear_pointers();
	if (!requested_focus.empty()) {
		focus_id_for_rebuild = requested_focus;
		requested_focus.clear();
	}
	overlays.clear();
	// Body.
	auto box = std::make_unique<Mui_box>(Mui_box::Dir::vertical, "mui.body.box");
	box->pad = 4;    // Room for focus rings at the edges.
	body_box = box.get();
	body     = std::make_unique<Mui_scroll_view>(std::move(box), "mui.body");
	build(*body_box);
	// Tabs.
	const auto labels = tab_labels();
	tabs.reset();
	if (!labels.empty()) {
		tab_index  = std::clamp(tab_index, 0, int(labels.size()) - 1);
		tabs       = std::make_unique<Mui_tabs>("mui.tabs", labels, tab_index, theme_ptr->tabs_left, [this](int i) {
            post([this, i] {
                on_tab(i);
                tab_index = i;
                rebuild();
            });
        });
		tabs->keys = tab_keys();
	}
	// Close button in the title bar.
	close_button           = std::make_unique<Mui_button>("mui.close", "", Mui_button::Style::flat, [this] {
        post([this] {
            on_cancel();
        });
    });
	close_button->has_icon = true;
	close_button->icon     = Mui_icon::close;
	close_button->help     = Strings::CloseHelp();
	// Optional title bar widgets.
	title_bar = std::make_unique<Mui_box>(Mui_box::Dir::horizontal, "mui.titlebar");
	build_title(*title_bar);
	if (title_bar->get_children().empty()) {
		title_bar.reset();
	}
	// Footer: theme switch on the left, page buttons on the right.
	help_line = std::make_unique<Mui_help_line>();
	footer    = std::make_unique<Mui_box>(Mui_box::Dir::horizontal, "mui.footer");
	int style = -1;
	if (theme_ptr->name == "ultima") {
		style = 0;
	} else if (theme_ptr->name == "modern") {
		style = 1;
	}
	auto* sw = footer->add<Mui_segmented>(
			"mui.theme", std::vector<std::string>{Strings::Ultima(), Strings::Modern()}, style, [this](int i) {
				switch_theme(i == 0 ? "ultima" : "modern");
			});
	sw->keys = {"ultima", "modern"};
	sw->help = Strings::StyleHelp();
	footer->add<Mui_spacer>(0, true);
	build_footer(*footer);
	for (Mui_widget* w : roots()) {
		w->attach(this);
	}
	layout_all();
	// Restore focus by id, else the first focusable widget of the body.
	Mui_widget* f = focus_id_for_rebuild.empty() ? nullptr : find(focus_id_for_rebuild);
	if (!f || !f->focusable) {
		std::vector<Mui_widget*> all;
		body->collect(all);
		for (auto* w : all) {
			if (w->focusable && w->is_live()) {
				f = w;
				break;
			}
		}
	}
	focus_id_for_rebuild.clear();
	set_focus(f);
	dirty = true;
}

void Mui_window::layout_all() {
	if (layer < 0 || !body) {
		return;
	}
	auto*       buf = static_cast<Image_buffer8*>(iwin()->get_layer_ibuf(layer));
	Mui_painter p(buf, *theme_ptr, scale);
	const auto& m      = theme_ptr->metrics;
	const bool  stone  = theme_ptr->frame_stone;
	const int   margin = p.px(16);
	float       wref;
	float       href;
	preferred_size(wref, href);
	const int  fb        = stone ? p.px(m.unit * 1.5f) : p.px(m.border);
	const int  title_h   = p.px(m.title_h);
	const int  foot_h    = p.px(m.footer_h);
	const int  help_h    = p.line_height(Mui_font_role::small) + p.px(m.gap);
	const int  pad       = p.px(m.gap * 2);
	const bool left_tabs = tabs && theme_ptr->tabs_left;
	const int  tabs_h    = tabs && !left_tabs ? p.px(m.tab_h) : 0;
	const int  side_w    = left_tabs ? p.px(theme_ptr->sidebar_width) : 0;
	int        w         = std::min(p.px(wref), layer_w - 2 * margin);
	int        h;
	if (href > 0) {
		h = std::min(p.px(href), layer_h - 2 * margin);
	} else {
		// Fit the content.
		int cw;
		int ch;
		body->measure(p, w - 2 * fb - side_w - 2 * pad, cw, ch);
		h = std::min(2 * fb + title_h + tabs_h + ch + 2 * pad + help_h + foot_h, layer_h - 2 * margin);
	}
	frame_rect = TileRect((layer_w - w) / 2, (layer_h - h) / 2, w, h);
	const TileRect in(frame_rect.x + fb, frame_rect.y + fb, frame_rect.w - 2 * fb, frame_rect.h - 2 * fb);
	title_rect  = TileRect(in.x, in.y, in.w, title_h);
	footer_rect = TileRect(in.x, in.y + in.h - foot_h, in.w, foot_h);
	help_rect   = TileRect(in.x, footer_rect.y - help_h, in.w, help_h);
	int top     = title_rect.y + title_h;
	if (tabs) {
		if (left_tabs) {
			tabs_rect = TileRect(in.x + pad / 2, top + pad / 2, side_w - pad, help_rect.y - top - pad);
		} else {
			tabs_rect = TileRect(in.x + pad, top, in.w - 2 * pad, tabs_h);
			top += tabs_h;
		}
		tabs->rect = tabs_rect;
		tabs->layout(p);
		int tw;
		int th;
		tabs->measure(p, tabs_rect.w, tw, th);
	} else {
		tabs_rect = TileRect(0, 0, 0, 0);
	}
	const int bx = in.x + side_w + pad;
	body_rect    = TileRect(bx, top + pad, in.x + in.w - pad - bx, help_rect.y - top - pad - pad / 2);
	body->rect   = body_rect;
	body->layout(p);
	// Surfaces below the chrome widgets (for anti-aliased text).
	const uint8 chrome_bg = stone ? theme_ptr->colour(Mui_colour::frame) : theme_ptr->colour(Mui_colour::window);
	const uint8 page_bg   = theme_ptr->colour(Mui_colour::panel);
	if (tabs) {
		tabs->surface_override = chrome_bg;
	}
	close_button->surface_override = chrome_bg;
	close_button->flat_ink         = stone ? Mui_colour::title_text : Mui_colour::text;
	footer->surface_override       = stone ? page_bg : chrome_bg;
	help_line->surface_override    = stone ? page_bg : chrome_bg;
	body->surface_override         = page_bg;
	const int cb                   = p.px(m.control_h);
	close_button->rect = TileRect(title_rect.x + title_rect.w - cb - pad / 2, title_rect.y + (title_h - cb) / 2, cb, cb);
	if (title_bar) {
		const int tw                = std::min(p.px(title_bar_width()), title_rect.w / 2);
		title_bar->surface_override = chrome_bg;
		title_bar->rect             = TileRect(close_button->rect.x - pad / 2 - tw, title_rect.y + (title_h - cb) / 2, tw, cb);
		title_bar->layout(p);
	}
	help_line->rect = TileRect(help_rect.x + pad, help_rect.y, help_rect.w - 2 * pad, help_rect.h);
	footer->rect    = TileRect(footer_rect.x + pad, footer_rect.y, footer_rect.w - 2 * pad, footer_rect.h);
	footer->layout(p);
	for (auto& o : overlays) {
		if (o.dialog) {
			int ow;
			int oh;
			o.root->measure(p, p.px(460), ow, oh);
			o.root->rect = TileRect(
					frame_rect.x + (frame_rect.w - p.px(460)) / 2, frame_rect.y + (frame_rect.h - oh) / 2, p.px(460), oh);
			o.root->layout(p);
		}
	}
	dirty = true;
}

/*
 *  Painting.
 */

void Mui_window::paint_frame(Mui_painter& p) {
	const auto& th    = *theme_ptr;
	const bool  stone = th.frame_stone;
	const auto& m     = th.metrics;
	if (stone) {
		p.texture_stone(
				frame_rect, th.colour(Mui_colour::frame), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark),
				th.colour(Mui_colour::window));
		p.bevel(frame_rect, std::max(1, p.px(2)), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark));
		// Gold rule around the inner edge.
		const int fb = p.px(m.unit * 1.5f);
		TileRect  in(frame_rect.x + fb, frame_rect.y + fb, frame_rect.w - 2 * fb, frame_rect.h - 2 * fb);
		TileRect  rule = in;
		rule.enlarge(std::max(1, p.px(2)));
		p.frame_rect(rule, std::max(1, p.px(1.5f)), th.colour(Mui_colour::accent));
		// Body page and footer strip on parchment.
		TileRect page(in.x, title_rect.y + title_rect.h + (tabs && !th.tabs_left ? tabs_rect.h : 0), in.w, 0);
		page.h = in.y + in.h - page.y;
		p.texture_parchment(page, th.colour(Mui_colour::panel), th.colour(Mui_colour::panel), th.colour(Mui_colour::panel_alt));
		p.bevel(page, std::max(1, p.px(2)), th.colour(Mui_colour::frame_dark), th.colour(Mui_colour::frame_light));
		p.divider(help_rect.x + p.px(m.gap * 2), help_rect.y - p.px(m.gap) / 2, help_rect.w - p.px(m.gap * 4));
		if (tabs && th.tabs_left) {
			p.vline(body_rect.x - p.px(m.gap), page.y + p.px(m.gap), body_rect.h, th.colour(Mui_colour::frame_dark));
		}
	} else {
		const int rad = p.px(m.radius * 2);
		p.rounded_fill(frame_rect, rad, th.colour(Mui_colour::window));
		p.rounded_frame(frame_rect, rad, std::max(1, p.px(m.border)), th.colour(Mui_colour::frame));
		p.hline(frame_rect.x + rad, title_rect.y + title_rect.h - 1, frame_rect.w - 2 * rad, th.colour(Mui_colour::frame));
		// Body panel.
		TileRect page = body_rect;
		page.enlarge(p.px(m.gap));
		p.rounded_fill(page, p.px(m.radius), th.colour(Mui_colour::panel));
		p.hline(frame_rect.x + rad, footer_rect.y, frame_rect.w - 2 * rad, th.colour(Mui_colour::frame));
	}
	// Title.
	const uint8 title_bg = stone ? th.colour(Mui_colour::frame) : th.colour(Mui_colour::window);
	TileRect    tr(
            title_rect.x + p.px(m.gap * 2), title_rect.y,
            title_rect.w - p.px(m.gap * 4) - close_button->rect.w - (title_bar ? title_bar->rect.w + p.px(m.gap * 2) : 0),
            title_rect.h);
	p.text_in_idx(title, tr, Mui_font_role::title, th.colour(Mui_colour::title_text), title_bg, Mui_align::left);
}

void Mui_window::paint_overlay(Mui_painter& p, Overlay& o) {
	const auto& th    = *theme_ptr;
	TileRect    r     = o.root->rect;
	const int   b     = std::max(1, p.px(th.metrics.border));
	TileRect    outer = r;
	outer.enlarge(b + (o.dialog ? p.px(th.metrics.gap) : 0));
	if (o.dialog) {
		// Dim the window below.
		for (int y = frame_rect.y; y < frame_rect.y + frame_rect.h; y += 2) {
			for (int x = frame_rect.x + (y / 2 & 1); x < frame_rect.x + frame_rect.w; x += 2) {
				p.fill_rect(TileRect(x, y, 1, 1), th.colour(Mui_colour::frame_dark));
			}
		}
	}
	if (th.frame_stone) {
		TileRect s = outer;
		s.enlarge(p.px(th.metrics.unit));
		p.texture_stone(
				s, th.colour(Mui_colour::frame), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark),
				th.colour(Mui_colour::window));
		p.texture_parchment(outer, th.colour(Mui_colour::panel), th.colour(Mui_colour::panel), th.colour(Mui_colour::panel_alt));
	} else {
		p.rounded_fill(outer, p.px(th.metrics.radius), th.colour(Mui_colour::panel_alt));
		p.rounded_frame(outer, p.px(th.metrics.radius), b, th.colour(Mui_colour::frame_light));
	}
	o.root->surface_override = th.colour(th.frame_stone ? Mui_colour::panel : Mui_colour::panel_alt);
	o.root->paint(p);
}

void Mui_window::paint() {
	if (done) {
		return;
	}
	if (!ensure_layers()) {
		return;
	}
	Image_window* w = iwin();
	w->layer_set_visible(layer, true);
	w->layer_set_visible(scrim, true);
	if (need_build) {
		rebuild_now();
	}
	if (!dirty) {
		return;
	}
	dirty     = false;
	auto* buf = static_cast<Image_buffer8*>(w->get_layer_ibuf(layer));
	buf->clear_clip();
	buf->fill8(255);
	Mui_painter p(buf, *theme_ptr, scale);
	paint_frame(p);
	if (tabs) {
		tabs->paint(p);
	}
	body->paint(p);
	close_button->paint(p);
	if (title_bar) {
		title_bar->paint(p);
	}
	help_line->paint(p);
	footer->paint(p);
	for (auto& o : overlays) {
		paint_overlay(p, o);
	}
	buf->clear_clip();
	w->layer_set_dirty(layer);
}

void Mui_window::force_paint() {
	dirty = true;
	gwin->set_all_dirty();
	gwin->paint();
}

bool Mui_window::run() {
	if (done) {
		hide_layers();
		return false;
	}
	ensure_layers();
	process_pending();
	if (done) {
		hide_layers();
		return false;
	}
	// Hover follows the mouse (modal gumps only get motion while dragging).
	if (layer >= 0 && !Mouse::use_touch_input) {
		float         fx;
		float         fy;
		float         rx;
		float         ry;
		SDL_Renderer* r = SDL_GetRenderer(iwin()->get_screen_window());
		SDL_GetMouseState(&fx, &fy);
		if (r && SDL_RenderCoordinatesFromWindow(r, fx, fy, &rx, &ry)) {
			int lx;
			int ly;
			display_to_local(int(rx), int(ry), lx, ly);
			Mui_widget* h = pressed ? hover : hit_widget(lx, ly);
			if (h != hover) {
				hover = h;
				dirty = true;
			}
		}
	}
	Mui_selftest::on_window_frame(this);
	if (done) {
		hide_layers();
		return false;
	}
	// Caret blink.
	if (focus && focus->wants_text()) {
		const uint32 phase = SDL_GetTicks() / 500;
		if (phase != caret_ticks) {
			caret_ticks = phase;
			dirty       = true;
		}
	}
	return dirty;
}

void Mui_window::close() {
	finish();
}

void Mui_window::finish() {
	done = true;
	if (text_input_on) {
		SDL_StopTextInput(iwin()->get_screen_window());
		text_input_on = false;
	}
	hide_layers();
}

/*
 *  Theme.
 */

void Mui_window::switch_theme(const std::string& name) {
	post([this, name] {
		auto t = Mui_theme::load(name);
		if (!t) {
			return;
		}
		config->set("config/gameplay/menu_style", name, true);
		theme_ptr      = t;
		theme_adjusted = false;
		if (scrim >= 0) {
			iwin()->layer_set_alpha(scrim, static_cast<unsigned char>(theme_ptr->backdrop_alpha));
			iwin()->get_layer_ibuf(scrim)->fill8(theme_ptr->colour(Mui_colour::backdrop));
			iwin()->layer_set_dirty(scrim);
		}
		if (focus) {
			focus_id_for_rebuild = focus->id;
		}
		rebuild_now();
	});
}

/*
 *  Widget lookup and focus.
 */

std::vector<Mui_widget*> Mui_window::roots() const {
	std::vector<Mui_widget*> out;
	for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
		out.push_back(it->root.get());
	}
	if (close_button) {
		out.push_back(close_button.get());
	}
	if (title_bar) {
		out.push_back(title_bar.get());
	}
	if (footer) {
		out.push_back(footer.get());
	}
	if (help_line) {
		out.push_back(help_line.get());
	}
	if (tabs) {
		out.push_back(tabs.get());
	}
	if (body) {
		out.push_back(body.get());
	}
	return out;
}

Mui_widget* Mui_window::hit_widget(int x, int y) const {
	if (!overlays.empty()) {
		return overlays.back().root->hit(x, y);
	}
	for (Mui_widget* r : roots()) {
		if (Mui_widget* w = r->hit(x, y)) {
			return w;
		}
	}
	return nullptr;
}

Mui_widget* Mui_window::find(const std::string& want) {
	for (Mui_widget* r : roots()) {
		if (Mui_widget* w = r->find(want)) {
			return w;
		}
	}
	return nullptr;
}

bool Mui_window::widget_rect(const std::string& want, TileRect& r) {
	if (Mui_widget* w = find(want)) {
		if (body && w != body.get()) {
			// Scroll body content into view first.
			Mui_widget* p = w->parent;
			while (p && p != body.get()) {
				p = p->parent;
			}
			if (p == body.get()) {
				body->ensure_visible(w->rect);
			}
		}
		r = w->rect;
		return true;
	}
	for (Mui_widget* root : roots()) {
		if (root->find_part(want, r)) {
			return true;
		}
	}
	return false;
}

std::vector<Mui_widget*> Mui_window::focus_chain() const {
	std::vector<Mui_widget*> all;
	if (!overlays.empty()) {
		overlays.back().root->collect(all);
	} else {
		if (title_bar) {
			title_bar->collect(all);
		}
		if (tabs) {
			tabs->collect(all);
		}
		if (body) {
			body->collect(all);
		}
		if (footer) {
			footer->collect(all);
		}
		if (close_button) {
			close_button->collect(all);
		}
	}
	std::vector<Mui_widget*> out;
	for (auto* w : all) {
		if (w->focusable && w->is_live()) {
			out.push_back(w);
		}
	}
	return out;
}

void Mui_window::move_focus(int dir) {
	const auto chain = focus_chain();
	if (chain.empty()) {
		return;
	}
	auto it = std::find(chain.begin(), chain.end(), focus);
	int  i  = it == chain.end() ? (dir > 0 ? -1 : 0) : int(it - chain.begin());
	i       = (i + dir + int(chain.size())) % int(chain.size());
	set_focus(chain[size_t(i)]);
}

void Mui_window::set_focus(Mui_widget* w) {
	if (w == focus) {
		return;
	}
	Mui_widget* old = focus;
	focus           = w;
	if (old) {
		old->on_blur();
	}
	if (focus && body) {
		Mui_widget* p = focus->parent;
		while (p && p != body.get()) {
			p = p->parent;
		}
		if (p == body.get()) {
			body->ensure_visible(focus->rect);
		}
	}
	update_text_input();
	dirty = true;
}

void Mui_window::update_text_input() {
	SDL_Window* sw   = iwin()->get_screen_window();
	const bool  want = focus && focus->wants_text();
	if (want && !text_input_on) {
		if (touchui != nullptr) {
			int gx1;
			int gy1;
			int gx2;
			int gy2;
			int sx;
			int sy;
			iwin()->layer_to_screen(layer, focus->rect.x, focus->rect.y, sx, sy);
			iwin()->screen_to_game(sx, sy, gwin->get_fastmouse(), gx1, gy1);
			iwin()->layer_to_screen(layer, focus->rect.x + focus->rect.w, focus->rect.y + focus->rect.h, sx, sy);
			iwin()->screen_to_game(sx, sy, gwin->get_fastmouse(), gx2, gy2);
			TouchUI::setTextInputArea(sw, gx1, gy1, gx2, gy2);
			TouchUI::startTextInput(sw);
		} else {
			SDL_StartTextInput(sw);
		}
		text_input_on = true;
	} else if (!want && text_input_on) {
		SDL_StopTextInput(sw);
		text_input_on = false;
	}
}

std::string Mui_window::current_help() const {
	if (hover && !hover->help.empty()) {
		return hover->help;
	}
	if (focus && !focus->help.empty()) {
		return focus->help;
	}
	return default_help();
}

/*
 *  Overlays.
 */

void Mui_window::open_popup(Mui_widget* owner, std::unique_ptr<Mui_widget> content, const TileRect& r) {
	if (layer < 0) {
		return;
	}
	auto*       buf = static_cast<Image_buffer8*>(iwin()->get_layer_ibuf(layer));
	Mui_painter p(buf, *theme_ptr, scale);
	int         w;
	int         h;
	content->measure(p, r.w, w, h);
	TileRect pr(r.x, r.y + r.h + p.px(2), std::max(r.w, w), h);
	if (pr.y + pr.h > layer_h - p.px(8)) {
		pr.y = r.y - p.px(2) - h;    // Not enough room below: open upwards.
	}
	pr.y          = std::max(0, pr.y);
	content->rect = pr;
	content->attach(this);
	content->layout(p);
	Overlay o;
	o.root            = std::move(content);
	o.owner           = owner;
	o.dialog          = false;
	o.prev_focus      = owner ? owner->id : std::string();
	Mui_widget* first = o.root.get();
	overlays.push_back(std::move(o));
	set_focus(first->focusable ? first : nullptr);
	dirty = true;
}

void Mui_window::close_overlay() {
	post([this] {
		if (overlays.empty()) {
			return;
		}
		const std::string prev = overlays.back().prev_focus;
		hover                  = nullptr;
		pressed                = nullptr;
		focus                  = nullptr;
		overlays.pop_back();
		Mui_widget* f = prev.empty() ? nullptr : find(prev);
		set_focus(f);
		dirty = true;
	});
}

void Mui_window::show_confirm(const std::string& msg, bool destructive, std::function<void(bool)> done_cb) {
	auto box   = std::make_unique<Mui_box>(Mui_box::Dir::vertical, "mui.dialog");
	box->pad   = 8;
	auto* text = box->add<Mui_label>(msg, Mui_font_role::body, Mui_colour::text, true);
	text->id   = "mui.dialog.text";
	auto* row  = box->add<Mui_box>(Mui_box::Dir::horizontal, "mui.dialog.buttons");
	row->add<Mui_spacer>(0, true);
	auto finish_with = [this, done_cb](bool ok) {
		close_overlay();
		post([done_cb, ok] {
			if (done_cb) {
				done_cb(ok);
			}
		});
	};
	row->add<Mui_button>("mui.dialog.cancel", Strings::Cancel(), Mui_button::Style::secondary, [finish_with] {
		finish_with(false);
	});
	auto* ok = row->add<Mui_button>(
			"mui.dialog.ok", Strings::OK(), destructive ? Mui_button::Style::destructive : Mui_button::Style::primary,
			[finish_with] {
				finish_with(true);
			});
	Overlay o;
	o.root       = std::move(box);
	o.dialog     = true;
	o.prev_focus = focus ? focus->id : std::string();
	o.root->attach(this);
	overlays.push_back(std::move(o));
	layout_all();
	set_focus(ok);
}

void Mui_window::show_prompt(
		const std::string& msg, const std::string& initial, std::function<void(bool, const std::string&)> done_cb) {
	auto box = std::make_unique<Mui_box>(Mui_box::Dir::vertical, "mui.dialog");
	box->pad = 8;
	box->add<Mui_label>(msg, Mui_font_role::body, Mui_colour::text, true)->id = "mui.dialog.text";
	auto* field = box->add<Mui_text_field>("mui.dialog.field", initial);
	auto* row   = box->add<Mui_box>(Mui_box::Dir::horizontal, "mui.dialog.buttons");
	row->add<Mui_spacer>(0, true);
	auto finish_with = [this, done_cb, field](bool ok) {
		const std::string value = field->text;
		close_overlay();
		post([done_cb, ok, value] {
			if (done_cb) {
				done_cb(ok, value);
			}
		});
	};
	field->on_submit = [finish_with](const std::string&) {
		finish_with(true);
	};
	row->add<Mui_button>("mui.dialog.cancel", Strings::Cancel(), Mui_button::Style::secondary, [finish_with] {
		finish_with(false);
	});
	row->add<Mui_button>("mui.dialog.ok", Strings::OK(), Mui_button::Style::primary, [finish_with] {
		finish_with(true);
	});
	Overlay o;
	o.root       = std::move(box);
	o.dialog     = true;
	o.prev_focus = focus ? focus->id : std::string();
	o.root->attach(this);
	overlays.push_back(std::move(o));
	layout_all();
	set_focus(field);
}

/*
 *  Input (coordinates are layer pixels, see display_to_local()).
 */

bool Mui_window::mouse_down(int mx, int my, MouseButton button) {
	process_pending();
	const int b = int(button);
	if (!overlays.empty()) {
		Overlay& o = overlays.back();
		if (!o.root->rect.has_point(mx, my)) {
			if (!o.dialog) {
				close_overlay();    // Click outside a popup closes it.
			}
			return true;
		}
	}
	if (button == MouseButton::Right) {
		return true;
	}
	Mui_widget* w = hit_widget(mx, my);
	hover         = w;
	pressed       = nullptr;
	// Bubble to the first widget that takes the press.
	for (Mui_widget* t = w; t; t = t->parent) {
		if (t->is_live() && t->on_mouse_down(mx, my, b)) {
			pressed = t;
			if (t->focusable) {
				set_focus(t);
			}
			break;
		}
	}
	dirty = true;
	return true;
}

bool Mui_window::mouse_up(int mx, int my, MouseButton button) {
	process_pending();
	if (button == MouseButton::Right) {
		if (!overlays.empty()) {
			close_overlay();
			return true;
		}
		// Right-click outside the window closes it (classic behaviour).
		if (!frame_rect.has_point(mx, my)) {
			finish();
			return false;
		}
		return true;
	}
	if (pressed) {
		Mui_widget* p = pressed;
		pressed       = nullptr;
		p->on_mouse_up(mx, my, int(button), p->rect.has_point(mx, my));
	}
	dirty = true;
	return true;
}

bool Mui_window::mouse_drag(int mx, int my) {
	process_pending();
	if (pressed) {
		pressed->on_mouse_move(mx, my, true);
	} else {
		Mui_widget* h = hit_widget(mx, my);
		if (h != hover) {
			hover = h;
			dirty = true;
		}
	}
	return true;
}

bool Mui_window::mousewheel_up(int mx, int my) {
	process_pending();
	for (Mui_widget* t = hit_widget(mx, my); t; t = t->parent) {
		if (t->is_live() && t->on_wheel(mx, my, 1)) {
			break;
		}
	}
	dirty = true;
	return true;
}

bool Mui_window::mousewheel_down(int mx, int my) {
	process_pending();
	for (Mui_widget* t = hit_widget(mx, my); t; t = t->parent) {
		if (t->is_live() && t->on_wheel(mx, my, -1)) {
			break;
		}
	}
	dirty = true;
	return true;
}

bool Mui_window::text_input(const char* text) {
	process_pending();
	if (focus && focus->wants_text()) {
		focus->on_text(text);
		dirty = true;
	}
	return true;
}

bool Mui_window::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	process_pending();
	dirty = true;
	// Text input events arrive with chr == SDLK_UNKNOWN.
	if (chr == SDLK_UNKNOWN) {
		if (focus && focus->wants_text() && unicode >= ' ' && unicode < 127) {
			const char s[2] = {char(unicode), 0};
			focus->on_text(s);
		}
		return true;
	}
	if (chr == SDLK_ESCAPE) {
		if (focus && focus->on_key(chr, int(unicode))) {
			return true;
		}
		if (!overlays.empty()) {
			close_overlay();
		} else {
			post([this] {
				on_cancel();
			});
		}
		return true;
	}
	if (chr == SDLK_TAB) {
		move_focus((SDL_GetModState() & SDL_KMOD_SHIFT) ? -1 : 1);
		return true;
	}
	if (focus && focus->is_live() && focus->on_key(chr, int(unicode))) {
		return true;
	}
	switch (chr) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
	case SDLK_SPACE:
		if (focus && focus->is_live()) {
			focus->activate();
		}
		return true;
	case SDLK_UP:
		move_focus(-1);
		return true;
	case SDLK_DOWN:
		move_focus(1);
		return true;
	default:
		break;
	}
	return true;
}
