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

#include "Mui_widgets.h"

#include "Mui_font.h"
#include "Mui_window.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
	const Mui_theme::Metrics& metrics(const Mui_painter& p) {
		return p.get_theme().metrics;
	}

	int radius(const Mui_painter& p) {
		return p.px(metrics(p).radius);
	}

	bool stone(const Mui_painter& p) {
		return p.get_theme().frame_stone;
	}

	// Box with the theme's look: bevelled in the stone theme, rounded and
	// outlined otherwise.
	void control_box(Mui_painter& p, const TileRect& r, uint8 fill, uint8 outline, bool sunken = false) {
		if (stone(p)) {
			p.fill_rect(r, fill);
			const uint8 light = p.col(Mui_colour::frame_light);
			const uint8 dark  = p.col(Mui_colour::frame_dark);
			p.bevel(r, std::max(1, p.px(2)), sunken ? dark : light, sunken ? light : dark);
		} else {
			p.rounded_fill(r, radius(p), fill);
			p.rounded_frame(r, radius(p), std::max(1, p.px(metrics(p).border)), outline);
		}
	}

	// Part lookups: "<id>.<n>" or "<id>.<key>".
	int part_index(const std::string& want, const std::string& id, const std::vector<std::string>& keys, size_t count) {
		if (id.empty() || want.size() <= id.size() + 1 || want.compare(0, id.size(), id) != 0 || want[id.size()] != '.') {
			return -1;
		}
		const std::string part = want.substr(id.size() + 1);
		for (size_t i = 0; i < keys.size(); i++) {
			if (keys[i] == part) {
				return int(i);
			}
		}
		if (!part.empty() && std::all_of(part.begin(), part.end(), [](char c) {
				return std::isdigit(static_cast<unsigned char>(c)) != 0;
			})) {
			const int n = std::atoi(part.c_str());
			return n >= 0 && size_t(n) < count ? n : -1;
		}
		return -1;
	}
}    // namespace

/*
 *  Mui_widget
 */

void Mui_widget::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	ignore_unused(p);
	w = avail_w;
	h = 0;
}

Mui_widget* Mui_widget::hit(int x, int y) {
	return visible && rect.has_point(x, y) ? this : nullptr;
}

void Mui_widget::collect(std::vector<Mui_widget*>& out) {
	if (visible) {
		out.push_back(this);
	}
}

Mui_widget* Mui_widget::find(const std::string& want) {
	return !id.empty() && id == want ? this : nullptr;
}

bool Mui_widget::find_part(const std::string& want, TileRect& r) {
	ignore_unused(want, r);
	return false;
}

void Mui_widget::invalidate() const {
	if (win) {
		win->set_dirty();
	}
}

bool Mui_widget::is_hover() const {
	return win && win->is_hover(this);
}

bool Mui_widget::is_focus() const {
	return win && win->is_focus(this);
}

bool Mui_widget::is_pressed() const {
	return win && win->is_pressed(this);
}

uint8 Mui_widget::surface(const Mui_painter& p) const {
	if (surface_override >= 0) {
		return uint8(surface_override);
	}
	return parent ? parent->surface(p) : p.col(Mui_colour::panel);
}

/*
 *  Mui_box
 */

Mui_widget* Mui_box::add_widget(std::unique_ptr<Mui_widget> w) {
	w->parent = this;
	w->attach(win);
	children.push_back(std::move(w));
	return children.back().get();
}

void Mui_box::clear() {
	children.clear();
	sizes.clear();
}

int Mui_box::gap_px(const Mui_painter& p) const {
	return gap < 0 ? p.px(metrics(p).gap) : p.px(gap);
}

void Mui_box::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	const int padp  = p.px(pad);
	const int g     = gap_px(p);
	const int inner = std::max(0, avail_w - 2 * padp);
	sizes.assign(children.size(), 0);
	w      = 0;
	h      = 0;
	int n  = 0;
	int st = 0;    // Stretch children (horizontal).
	for (size_t i = 0; i < children.size(); i++) {
		auto& c = children[i];
		if (!c->visible) {
			continue;
		}
		int cw = 0;
		int ch = 0;
		c->measure(p, inner, cw, ch);
		if (dir == Dir::vertical) {
			sizes[i] = ch;
			w        = std::max(w, cw);
			h += ch;
		} else {
			sizes[i] = cw;
			if (c->stretch) {
				st++;
				sizes[i] = 0;
			}
			w += sizes[i];
			h = std::max(h, ch);
		}
		n++;
	}
	if (n > 1) {
		(dir == Dir::vertical ? h : w) += g * (n - 1);
	}
	if (dir == Dir::horizontal && st > 0) {
		w = std::max(w, inner);
	}
	w += 2 * padp;
	h += 2 * padp;
}

void Mui_box::layout(Mui_painter& p) {
	const int padp = p.px(pad);
	const int g    = gap_px(p);
	int       dummy_w;
	int       dummy_h;
	measure(p, rect.w, dummy_w, dummy_h);
	const TileRect in(rect.x + padp, rect.y + padp, std::max(0, rect.w - 2 * padp), std::max(0, rect.h - 2 * padp));
	int            used      = 0;
	int            n         = 0;
	int            stretch_n = 0;
	for (size_t i = 0; i < children.size(); i++) {
		if (!children[i]->visible) {
			continue;
		}
		used += sizes[i];
		n++;
		stretch_n += children[i]->stretch ? 1 : 0;
	}
	used += n > 1 ? g * (n - 1) : 0;
	const int spare = (dir == Dir::vertical ? in.h : in.w) - used;
	int       pos   = dir == Dir::vertical ? in.y : in.x;
	if (centre && stretch_n == 0 && spare > 0) {
		pos += spare / 2;
	}
	for (size_t i = 0; i < children.size(); i++) {
		auto& c = children[i];
		if (!c->visible) {
			continue;
		}
		int cw = 0;
		int ch = 0;
		c->measure(p, in.w, cw, ch);
		if (dir == Dir::vertical) {
			int w = c->fill_w ? in.w : std::min(cw, in.w);
			int x = in.x;
			if (!c->fill_w && centre) {
				x = in.x + (in.w - w) / 2;
			}
			c->rect = TileRect(x, pos, w, sizes[i]);
			pos += sizes[i] + g;
		} else {
			int w = sizes[i];
			if (c->stretch) {
				w = std::max(0, spare / std::max(1, stretch_n));
			}
			ch      = std::min(ch, in.h);
			c->rect = TileRect(pos, in.y + (in.h - ch) / 2, w, ch);
			pos += w + g;
		}
		c->layout(p);
	}
}

void Mui_box::paint(Mui_painter& p) {
	if (panel) {
		if (bg == Mui_colour::panel && p.get_theme().panel_parchment) {
			p.texture_parchment(rect, p.col(Mui_colour::panel), p.col(Mui_colour::panel), p.col(Mui_colour::panel_alt));
		} else {
			p.rounded_fill(rect, radius(p), p.col(bg));
		}
	}
	for (auto& c : children) {
		if (c->visible && c->rect.w > 0 && c->rect.h > 0 && p.get_clip().intersects(c->rect)) {
			c->paint(p);
		}
	}
}

Mui_widget* Mui_box::hit(int x, int y) {
	if (!visible || !rect.has_point(x, y)) {
		return nullptr;
	}
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if (Mui_widget* w = (*it)->hit(x, y)) {
			return w;
		}
	}
	return this;
}

void Mui_box::collect(std::vector<Mui_widget*>& out) {
	if (!visible) {
		return;
	}
	out.push_back(this);
	for (auto& c : children) {
		c->collect(out);
	}
}

Mui_widget* Mui_box::find(const std::string& want) {
	if (Mui_widget* w = Mui_widget::find(want)) {
		return w;
	}
	for (auto& c : children) {
		if (Mui_widget* w = c->find(want)) {
			return w;
		}
	}
	return nullptr;
}

bool Mui_box::find_part(const std::string& want, TileRect& r) {
	for (auto& c : children) {
		if (c->visible && c->find_part(want, r)) {
			return true;
		}
	}
	return false;
}

void Mui_box::attach(Mui_window* w) {
	win = w;
	for (auto& c : children) {
		c->attach(w);
	}
}

uint8 Mui_box::surface(const Mui_painter& p) const {
	if (panel && surface_override < 0) {
		return p.col(bg);
	}
	return Mui_widget::surface(p);
}

/*
 *  Mui_label
 */

Mui_label::Mui_label(std::string text_, Mui_font_role role_, Mui_colour colour_, bool wrap_)
		: text(std::move(text_)), role(role_), colour(colour_), wrap(wrap_) {}

void Mui_label::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	const Mui_text_font* f = p.font(role);
	if (!f) {
		w = h = 0;
		return;
	}
	if (wrap) {
		const auto lines = f->wrap(text, std::max(1, avail_w));
		w                = 0;
		for (const auto& l : lines) {
			w = std::max(w, f->width(l));
		}
		h = int(lines.size()) * f->height();
	} else {
		w = f->width(text);
		h = f->height();
	}
}

void Mui_label::paint(Mui_painter& p) {
	const Mui_text_font* f = p.font(role);
	if (!f) {
		return;
	}
	const uint8 fg = p.col(enabled ? colour : Mui_colour::text_dim);
	const uint8 bg = surface(p);
	if (!wrap) {
		p.text_in_idx(text, rect, role, fg, bg, align);
		return;
	}
	int y = rect.y;
	for (const auto& line : f->wrap(text, std::max(1, rect.w))) {
		TileRect r(rect.x, y, rect.w, f->height());
		p.text_in_idx(line, r, role, fg, bg, align);
		y += f->height();
	}
}

/*
 *  Mui_section_header
 */

void Mui_section_header::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	w = avail_w;
	h = p.line_height(Mui_font_role::heading) + p.px(metrics(p).gap);
}

void Mui_section_header::paint(Mui_painter& p) {
	const int lh = p.line_height(Mui_font_role::heading);
	p.text_in_idx(
			text, TileRect(rect.x, rect.y, rect.w, lh), Mui_font_role::heading, p.col(Mui_colour::text), surface(p),
			Mui_align::left);
	p.divider(rect.x, rect.y + lh + p.px(metrics(p).gap) / 2 - 1, rect.w);
}

/*
 *  Mui_badge
 */

void Mui_badge::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	ignore_unused(avail_w);
	w = p.text_width(text, Mui_font_role::small) + 2 * p.px(metrics(p).unit);
	h = p.line_height(Mui_font_role::small) + p.px(4);
}

void Mui_badge::paint(Mui_painter& p) {
	Mui_colour c = Mui_colour::badge_now;
	if (kind == Kind::restart) {
		c = Mui_colour::badge_restart;
	} else if (kind == Kind::changed) {
		c = Mui_colour::badge_changed;
	}
	p.rounded_fill(rect, rect.h / 2, p.col(c));
	// Badges are dark in both themes: light text, as on the danger colour.
	p.text_in_idx(text, rect, Mui_font_role::small, p.col(Mui_colour::text_on_danger), p.col(c), Mui_align::centre);
}

/*
 *  Mui_button
 */

Mui_button::Mui_button(std::string id_, std::string text_, Style s, std::function<void()> click)
		: Mui_widget(std::move(id_)), text(std::move(text_)), style(s), on_click(std::move(click)) {
	focusable = true;
	fill_w    = false;
}

void Mui_button::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	const int pad = p.px(metrics(p).unit * 2);
	h             = p.px(metrics(p).control_h);
	w             = text.empty() ? h : p.text_width(text, Mui_font_role::body) + 2 * pad;
	if (has_icon && !text.empty()) {
		w += h / 2;
	}
	w = std::max(w, p.px(min_w));
	w = std::min(w, std::max(h, avail_w));
}

void Mui_button::paint(Mui_painter& p) {
	const bool live = is_live();
	const bool hot  = live && is_hover();
	const bool down = live && is_pressed() && hot;
	uint8      fill = p.col(Mui_colour::panel_alt);
	uint8      ink  = p.col(Mui_colour::text);
	uint8      line = p.col(Mui_colour::frame);
	switch (style) {
	case Style::primary:
		fill = p.col(hot ? Mui_colour::accent_hover : Mui_colour::accent);
		ink  = p.col(Mui_colour::text_on_accent);
		line = p.col(Mui_colour::frame_dark);
		break;
	case Style::destructive:
		fill = p.col(Mui_colour::danger);
		ink  = p.col(Mui_colour::text_on_danger);
		line = p.col(Mui_colour::frame_dark);
		if (hot) {
			fill = Mui_theme::blend_index(fill, p.col(Mui_colour::text_on_accent), 1, 5);
		}
		break;
	case Style::flat:
		fill = hot ? p.col(Mui_colour::panel_alt) : surface(p);
		ink  = p.col(hot ? Mui_colour::text : flat_ink);
		break;
	case Style::secondary:
		if (hot) {
			fill = Mui_theme::blend_index(fill, p.col(Mui_colour::accent), 1, 4);
		}
		break;
	}
	if (!live) {
		ink = p.col(Mui_colour::text_dim);
	}
	if (style == Style::flat) {
		if (hot) {
			p.rounded_fill(rect, radius(p), fill);
		}
	} else {
		control_box(p, rect, fill, line, down);
	}
	TileRect tr = rect;
	if (down) {
		tr.x += 1;
		tr.y += 1;
	}
	if (has_icon) {
		const int is = rect.h * 3 / 5;
		if (text.empty()) {
			p.icon(icon, TileRect(tr.x + (tr.w - is) / 2, tr.y + (tr.h - is) / 2, is, is), ink);
		} else {
			p.icon(icon, TileRect(tr.x + p.px(metrics(p).unit), tr.y + (tr.h - is) / 2, is, is), ink);
			tr.x += is;
			tr.w -= is;
		}
	}
	if (!text.empty()) {
		p.text_in_idx(text, tr, Mui_font_role::body, ink, fill, Mui_align::centre);
	}
	if (is_focus()) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

bool Mui_button::on_mouse_down(int x, int y, int button) {
	ignore_unused(x, y);
	return button == 1;
}

bool Mui_button::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x, y);
	if (button == 1 && inside) {
		activate();
	}
	return true;
}

bool Mui_button::activate() {
	if (!is_live()) {
		return false;
	}
	invalidate();
	if (on_click) {
		// Copy: the callback may rebuild the widget tree (and delete this).
		auto cb = on_click;
		cb();
	}
	return true;
}

/*
 *  Mui_toggle
 */

Mui_toggle::Mui_toggle(std::string id_, bool value_, std::function<void(bool)> change)
		: Mui_widget(std::move(id_)), value(value_), on_change(std::move(change)) {
	focusable = true;
	fill_w    = false;
}

void Mui_toggle::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	ignore_unused(avail_w);
	h = p.px(metrics(p).control_h);
	w = h * 2;
}

void Mui_toggle::paint(Mui_painter& p) {
	const int   th = rect.h * 3 / 4;
	TileRect    track(rect.x, rect.y + (rect.h - th) / 2, rect.w, th);
	const uint8 on_c  = p.col(is_hover() ? Mui_colour::accent_hover : Mui_colour::accent);
	const uint8 off_c = p.col(Mui_colour::frame);
	const int   rad   = stone(p) ? 0 : th / 2;
	if (stone(p)) {
		control_box(p, track, value ? on_c : off_c, p.col(Mui_colour::frame_dark), true);
	} else {
		p.rounded_fill(track, rad, value ? on_c : off_c);
	}
	const int k  = th - 2 * std::max(2, p.px(3));
	const int kx = value ? track.x + track.w - k - (th - k) / 2 : track.x + (th - k) / 2;
	TileRect  knob(kx, track.y + (th - k) / 2, k, k);
	if (stone(p)) {
		control_box(p, knob, p.col(Mui_colour::panel), p.col(Mui_colour::frame_dark));
	} else {
		p.rounded_fill(knob, k / 2, p.col(Mui_colour::text_on_accent));
	}
	if (!is_live()) {
		// Dim: overlay a sparse dither of the surface colour.
		for (int y = track.y; y < track.y + track.h; y += 2) {
			for (int x = track.x + (y & 2) / 2; x < track.x + track.w; x += 2) {
				p.fill_rect(TileRect(x, y, 1, 1), surface(p));
			}
		}
	}
	if (is_focus()) {
		p.focus_ring(track, rad);
	}
}

void Mui_toggle::set(bool v) {
	if (v == value) {
		return;
	}
	value = v;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(value);
	}
}

bool Mui_toggle::on_mouse_down(int x, int y, int button) {
	ignore_unused(x, y);
	return button == 1;
}

bool Mui_toggle::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x, y);
	if (button == 1 && inside && is_live()) {
		set(!value);
	}
	return true;
}

bool Mui_toggle::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	if (key == SDLK_LEFT) {
		set(false);
		return true;
	}
	if (key == SDLK_RIGHT) {
		set(true);
		return true;
	}
	return false;
}

bool Mui_toggle::activate() {
	if (!is_live()) {
		return false;
	}
	set(!value);
	return true;
}

/*
 *  Mui_segmented
 */

Mui_segmented::Mui_segmented(std::string id_, std::vector<std::string> options_, int index_, std::function<void(int)> change)
		: Mui_widget(std::move(id_)), options(std::move(options_)), index(index_), on_change(std::move(change)) {
	focusable = true;
	fill_w    = false;
}

void Mui_segmented::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	h       = p.px(metrics(p).control_h);
	int seg = h;
	for (const auto& o : options) {
		seg = std::max(seg, p.text_width(o, Mui_font_role::body) + 2 * p.px(metrics(p).unit * 1.5f));
	}
	w = std::min(seg * int(std::max<size_t>(1, options.size())), std::max(h, avail_w));
}

TileRect Mui_segmented::segment_rect(int i) const {
	const int n  = std::max<int>(1, int(options.size()));
	const int x0 = rect.x + rect.w * i / n;
	const int x1 = rect.x + rect.w * (i + 1) / n;
	return TileRect(x0, rect.y, x1 - x0, rect.h);
}

void Mui_segmented::paint(Mui_painter& p) {
	const uint8 base = p.col(Mui_colour::panel_alt);
	control_box(p, rect, base, p.col(Mui_colour::frame), true);
	const int inset = std::max(2, p.px(3));
	for (int i = 0; i < int(options.size()); i++) {
		TileRect r = segment_rect(i);
		if (i == index) {
			TileRect s = r;
			s.enlarge(-inset);
			const uint8 c = p.col(Mui_colour::accent);
			if (stone(p)) {
				control_box(p, s, c, p.col(Mui_colour::frame_dark));
			} else {
				p.rounded_fill(s, std::max(0, radius(p) - inset / 2), c);
			}
			p.text_in_idx(options[i], s, Mui_font_role::body, p.col(Mui_colour::text_on_accent), c, Mui_align::centre);
		} else {
			uint8 fill = base;
			if (is_hover()) {
				fill = base;
			}
			if (i > 0 && i - 1 != index) {
				p.vline(r.x, r.y + inset * 2, r.h - inset * 4, p.col(Mui_colour::frame));
			}
			p.text_in_idx(
					options[i], r, Mui_font_role::body, p.col(is_live() ? Mui_colour::text : Mui_colour::text_dim), fill,
					Mui_align::centre);
		}
	}
	if (is_focus()) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

void Mui_segmented::set(int i) {
	if (i < 0 || i >= int(options.size()) || i == index) {
		return;
	}
	index = i;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(index);
	}
}

bool Mui_segmented::on_mouse_down(int x, int y, int button) {
	press = -1;
	if (button != 1) {
		return false;
	}
	for (int i = 0; i < int(options.size()); i++) {
		if (segment_rect(i).has_point(x, y)) {
			press = i;
		}
	}
	return true;
}

bool Mui_segmented::on_mouse_up(int x, int y, int button, bool inside) {
	if (button == 1 && inside && press >= 0 && segment_rect(press).has_point(x, y) && is_live()) {
		set(press);
	}
	press = -1;
	return true;
}

bool Mui_segmented::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	if (key == SDLK_LEFT) {
		set(index - 1);
		return true;
	}
	if (key == SDLK_RIGHT) {
		set(index + 1);
		return true;
	}
	return false;
}

bool Mui_segmented::activate() {
	if (options.empty() || !is_live()) {
		return false;
	}
	set((index + 1) % int(options.size()));
	return true;
}

bool Mui_segmented::find_part(const std::string& want, TileRect& r) {
	const int i = part_index(want, id, keys, options.size());
	if (i < 0) {
		return false;
	}
	r = segment_rect(i);
	return true;
}

std::string Mui_segmented::value_text() const {
	if (index >= 0 && size_t(index) < keys.size()) {
		return keys[index];
	}
	return std::to_string(index);
}

/*
 *  Mui_list
 */

Mui_list::Mui_list(std::string id_, std::vector<std::string> rows_, int sel)
		: Mui_widget(std::move(id_)), rows(std::move(rows_)), selected(sel) {
	focusable = true;
}

void Mui_list::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	row_px = p.px(row_ref > 0 ? row_ref : metrics(p).row_h);
	w      = avail_w;
	h      = row_px * std::max(1, std::min(visible_rows, std::max(1, int(rows.size()))));
}

int Mui_list::rows_shown() const {
	return row_px > 0 ? std::max(1, rect.h / row_px) : 1;
}

void Mui_list::clamp_scroll() {
	scroll = std::clamp(scroll, 0, std::max(0, int(rows.size()) - rows_shown()));
}

void Mui_list::ensure_visible(int i) {
	if (i < scroll) {
		scroll = i;
	} else if (i >= scroll + rows_shown()) {
		scroll = i - rows_shown() + 1;
	}
	clamp_scroll();
}

void Mui_list::paint(Mui_painter& p) {
	if (row_px <= 0) {
		return;
	}
	clamp_scroll();
	const uint8 bg = surface(p);
	p.push_clip(rect);
	const bool need_bar = int(rows.size()) > rows_shown();
	const int  bar      = need_bar ? std::max(4, p.px(6)) : 0;
	for (int i = scroll; i < int(rows.size()) && i < scroll + rows_shown() + 1; i++) {
		TileRect   r(rect.x, rect.y + (i - scroll) * row_px, rect.w - bar, row_px);
		const bool sel  = i == selected;
		uint8      fill = bg;
		if (sel) {
			fill = p.col(Mui_colour::accent);
			p.rounded_fill(r, radius(p), fill);
		} else if (i == press_row) {
			fill = p.col(Mui_colour::panel_alt);
			p.rounded_fill(r, radius(p), fill);
		}
		if (paint_row) {
			paint_row(p, i, r, sel);
		} else {
			TileRect tr = r;
			tr.x += p.px(metrics(p).unit * 1.5f);
			tr.w -= p.px(metrics(p).unit * 3);
			p.text_in_idx(
					rows[i], tr, Mui_font_role::body, p.col(sel ? Mui_colour::text_on_accent : Mui_colour::text), fill,
					Mui_align::left);
		}
	}
	if (need_bar) {
		const int n  = int(rows.size());
		const int th = std::max(bar * 2, rect.h * rows_shown() / n);
		const int ty = rect.y + (rect.h - th) * scroll / std::max(1, n - rows_shown());
		p.rounded_fill(TileRect(rect.x + rect.w - bar, rect.y, bar, rect.h), bar / 2, p.col(Mui_colour::panel_alt));
		p.rounded_fill(TileRect(rect.x + rect.w - bar, ty, bar, th), bar / 2, p.col(Mui_colour::frame_light));
	}
	p.pop_clip();
	if (is_focus()) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

void Mui_list::set_selected(int i, bool notify) {
	if (i < 0 || i >= int(rows.size())) {
		return;
	}
	const bool changed = i != selected;
	selected           = i;
	ensure_visible(i);
	invalidate();
	if (notify && changed && on_select) {
		auto cb = on_select;
		cb(i);
	}
}

bool Mui_list::on_mouse_down(int x, int y, int button) {
	ignore_unused(x);
	if (button != 1 || row_px <= 0) {
		return false;
	}
	press_row   = scroll + (y - rect.y) / row_px;
	drag_y      = y;
	drag_scroll = scroll;
	if (press_row >= int(rows.size())) {
		press_row = -1;
	}
	invalidate();
	return true;
}

bool Mui_list::on_mouse_move(int x, int y, bool held) {
	ignore_unused(x);
	if (!held || drag_y < 0 || row_px <= 0) {
		return false;
	}
	// Drag to scroll (touch).
	const int rows_moved = (drag_y - y) / row_px;
	if (rows_moved != 0) {
		press_row = -1;
		scroll    = drag_scroll + rows_moved;
		clamp_scroll();
		invalidate();
	}
	return true;
}

bool Mui_list::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x);
	if (button == 1 && inside && press_row >= 0 && row_px > 0) {
		const int r = scroll + (y - rect.y) / row_px;
		if (r == press_row) {
			const bool again = r == selected;
			set_selected(r);
			if (again && on_activate) {
				auto cb = on_activate;
				cb(r);
			} else if (!on_select && on_activate) {
				auto cb = on_activate;
				cb(r);
			}
		}
	}
	press_row = -1;
	drag_y    = -1;
	invalidate();
	return true;
}

bool Mui_list::on_wheel(int x, int y, int dy) {
	ignore_unused(x, y);
	scroll -= dy;
	clamp_scroll();
	invalidate();
	return true;
}

bool Mui_list::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	int i = selected;
	switch (key) {
	case SDLK_UP:
		i = std::max(0, selected - 1);
		break;
	case SDLK_DOWN:
		i = std::min(int(rows.size()) - 1, selected + 1);
		break;
	case SDLK_PAGEUP:
		i = std::max(0, selected - rows_shown());
		break;
	case SDLK_PAGEDOWN:
		i = std::min(int(rows.size()) - 1, selected + rows_shown());
		break;
	case SDLK_HOME:
		i = 0;
		break;
	case SDLK_END:
		i = int(rows.size()) - 1;
		break;
	default:
		return false;
	}
	set_selected(i);
	return true;
}

bool Mui_list::activate() {
	if (selected >= 0 && on_activate) {
		auto cb = on_activate;
		cb(selected);
		return true;
	}
	return false;
}

bool Mui_list::find_part(const std::string& want, TileRect& r) {
	const int i = part_index(want, id, {}, rows.size());
	if (i < 0) {
		return false;
	}
	ensure_visible(i);
	r = TileRect(rect.x, rect.y + (i - scroll) * row_px, rect.w, row_px);
	return true;
}

std::string Mui_list::value_text() const {
	return selected >= 0 && selected < int(rows.size()) ? rows[selected] : std::string();
}

/*
 *  Mui_dropdown
 */

Mui_dropdown::Mui_dropdown(std::string id_, std::vector<std::string> options_, int index_, std::function<void(int)> change)
		: Mui_widget(std::move(id_)), options(std::move(options_)), index(index_), on_change(std::move(change)) {
	focusable = true;
	fill_w    = false;
}

void Mui_dropdown::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	h = p.px(metrics(p).control_h);
	w = h;
	for (const auto& o : options) {
		w = std::max(w, p.text_width(o, Mui_font_role::body));
	}
	w = std::min(w + 2 * p.px(metrics(p).unit * 1.5f) + h, std::max(h, avail_w));
}

void Mui_dropdown::paint(Mui_painter& p) {
	const uint8 fill = is_hover() ? Mui_theme::blend_index(p.col(Mui_colour::panel_alt), p.col(Mui_colour::accent), 1, 5)
								  : p.col(Mui_colour::panel_alt);
	control_box(p, rect, fill, p.col(Mui_colour::frame));
	const int pad = p.px(metrics(p).unit * 1.5f);
	TileRect  tr(rect.x + pad, rect.y, rect.w - 2 * pad - rect.h / 2, rect.h);
	p.text_in_idx(
			value_text(), tr, Mui_font_role::body, p.col(is_live() ? Mui_colour::text : Mui_colour::text_dim), fill,
			Mui_align::left);
	const int is = rect.h / 2;
	p.icon(Mui_icon::chevron_down, TileRect(rect.x + rect.w - pad - is, rect.y + (rect.h - is) / 2, is, is),
		   p.col(Mui_colour::text_dim));
	if (is_focus()) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

std::string Mui_dropdown::value_text() const {
	return index >= 0 && index < int(options.size()) ? options[index] : std::string();
}

void Mui_dropdown::set(int i) {
	if (i < 0 || i >= int(options.size()) || i == index) {
		return;
	}
	index = i;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(index);
	}
}

void Mui_dropdown::open_popup() {
	if (!win || !is_live()) {
		return;
	}
	auto list          = std::make_unique<Mui_list>(id + ".list", options, index);
	list->visible_rows = std::min(8, int(options.size()));
	Mui_list* l        = list.get();
	l->on_activate     = [this](int i) {
        Mui_window* w = win;
        set(i);
        if (w) {
            w->close_overlay();
        }
	};
	// on_select only moves the highlight; a click activates.
	win->open_popup(this, std::move(list), rect);
}

bool Mui_dropdown::on_mouse_down(int x, int y, int button) {
	ignore_unused(x, y);
	return button == 1;
}

bool Mui_dropdown::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x, y);
	if (button == 1 && inside) {
		open_popup();
	}
	return true;
}

bool Mui_dropdown::on_key(SDL_Keycode key, int unicode) {
	if (key == SDLK_UP || key == SDLK_LEFT) {
		set(index - 1);
		return true;
	}
	if (key == SDLK_DOWN || key == SDLK_RIGHT) {
		set(index + 1);
		return true;
	}
	// Type-ahead: next option starting with the letter.
	if (unicode > ' ' && unicode < 127 && !options.empty()) {
		const char c = char(std::tolower(unicode));
		for (int k = 1; k <= int(options.size()); k++) {
			const int i = (index + k) % int(options.size());
			if (!options[i].empty() && std::tolower(static_cast<unsigned char>(options[i][0])) == c) {
				set(i);
				return true;
			}
		}
	}
	return false;
}

bool Mui_dropdown::activate() {
	open_popup();
	return true;
}

/*
 *  Mui_slider
 */

Mui_slider::Mui_slider(std::string id_, int min_, int max_, int step_, int value_, std::function<void(int)> change)
		: Mui_widget(std::move(id_)), minv(min_), maxv(std::max(min_, max_)), step(std::max(1, step_)), value(value_),
		  on_change(std::move(change)) {
	focusable = true;
}

void Mui_slider::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	h        = p.px(metrics(p).control_h);
	knob_px  = h * 2 / 3;
	auto fmt = [this](int v) {
		return format ? format(v) : std::to_string(v);
	};
	value_w = std::max(p.text_width(fmt(minv), Mui_font_role::body), p.text_width(fmt(maxv), Mui_font_role::body))
			  + p.px(metrics(p).unit * 2);
	w = std::min(avail_w, p.px(320));
}

TileRect Mui_slider::track_rect(const Mui_painter* p) const {
	ignore_unused(p);
	const int k = std::max(1, knob_px);
	return TileRect(rect.x + k / 2, rect.y, std::max(1, rect.w - value_w - k), rect.h);
}

void Mui_slider::paint(Mui_painter& p) {
	const TileRect t    = track_rect(&p);
	const int      th   = std::max(3, p.px(4));
	const int      cy   = rect.y + rect.h / 2;
	const int      span = std::max(1, maxv - minv);
	const int      kx   = t.x + (value - minv) * t.w / span;
	p.rounded_fill(TileRect(t.x, cy - th / 2, t.w, th), th / 2, p.col(Mui_colour::frame));
	p.rounded_fill(TileRect(t.x, cy - th / 2, kx - t.x, th), th / 2, p.col(Mui_colour::accent));
	const int k = knob_px;
	TileRect  knob(kx - k / 2, cy - k / 2, k, k);
	if (stone(p)) {
		control_box(p, knob, p.col(Mui_colour::accent), p.col(Mui_colour::frame_dark));
	} else {
		p.rounded_fill(knob, k / 2, p.col(is_hover() || dragging ? Mui_colour::accent_hover : Mui_colour::accent));
	}
	const std::string text = format ? format(value) : std::to_string(value);
	p.text_in_idx(
			text, TileRect(rect.x + rect.w - value_w, rect.y, value_w, rect.h), Mui_font_role::body, p.col(Mui_colour::text),
			surface(p), Mui_align::right);
	if (is_focus()) {
		p.focus_ring(knob, k / 2);
	}
}

void Mui_slider::set(int v) {
	v = std::clamp(minv + (v - minv + step / 2) / step * step, minv, maxv);
	if (v == value) {
		return;
	}
	value = v;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(value);
	}
}

void Mui_slider::set_from_x(int x) {
	const TileRect t = track_rect();
	set(minv + (x - t.x) * (maxv - minv) / std::max(1, t.w));
}

bool Mui_slider::on_mouse_down(int x, int y, int button) {
	ignore_unused(y);
	if (button != 1 || !is_live()) {
		return false;
	}
	dragging = true;
	set_from_x(x);
	return true;
}

bool Mui_slider::on_mouse_move(int x, int y, bool held) {
	ignore_unused(y);
	if (dragging && held) {
		set_from_x(x);
		return true;
	}
	return false;
}

bool Mui_slider::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x, y, button, inside);
	dragging = false;
	invalidate();
	return true;
}

bool Mui_slider::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	if (key == SDLK_LEFT || key == SDLK_DOWN) {
		set(value - step);
		return true;
	}
	if (key == SDLK_RIGHT || key == SDLK_UP) {
		set(value + step);
		return true;
	}
	if (key == SDLK_HOME) {
		set(minv);
		return true;
	}
	if (key == SDLK_END) {
		set(maxv);
		return true;
	}
	return false;
}

bool Mui_slider::on_wheel(int x, int y, int dy) {
	ignore_unused(x, y);
	set(value + dy * step);
	return true;
}

/*
 *  Mui_number_field
 */

Mui_number_field::Mui_number_field(std::string id_, int min_, int max_, int step_, int value_, std::function<void(int)> change)
		: Mui_widget(std::move(id_)), minv(min_), maxv(std::max(min_, max_)), step(std::max(1, step_)), value(value_),
		  on_change(std::move(change)) {
	focusable = true;
	fill_w    = false;
}

void Mui_number_field::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	h            = p.px(metrics(p).control_h);
	const int tw = std::max(
			p.text_width(std::to_string(minv), Mui_font_role::body), p.text_width(std::to_string(maxv), Mui_font_role::body));
	w = std::min(avail_w, tw + 2 * h + 2 * p.px(metrics(p).unit * 2));
}

TileRect Mui_number_field::minus_rect() const {
	return TileRect(rect.x, rect.y, rect.h, rect.h);
}

TileRect Mui_number_field::plus_rect() const {
	return TileRect(rect.x + rect.w - rect.h, rect.y, rect.h, rect.h);
}

void Mui_number_field::paint(Mui_painter& p) {
	const uint8 fill = p.col(Mui_colour::panel_alt);
	control_box(p, rect, fill, p.col(Mui_colour::frame), true);
	const uint8 ink = p.col(is_live() ? Mui_colour::text : Mui_colour::text_dim);
	TileRect    mr  = minus_rect();
	TileRect    pr  = plus_rect();
	if (press < 0) {
		p.fill_rect(TileRect(mr.x + 1, mr.y + 1, mr.w - 2, mr.h - 2), p.col(Mui_colour::frame_light));
	}
	if (press > 0) {
		p.fill_rect(TileRect(pr.x + 1, pr.y + 1, pr.w - 2, pr.h - 2), p.col(Mui_colour::frame_light));
	}
	mr.enlarge(-rect.h / 4);
	pr.enlarge(-rect.h / 4);
	p.icon(Mui_icon::minus, mr, ink);
	p.icon(Mui_icon::plus, pr, ink);
	p.vline(rect.x + rect.h, rect.y, rect.h, p.col(Mui_colour::frame));
	p.vline(rect.x + rect.w - rect.h - 1, rect.y, rect.h, p.col(Mui_colour::frame));
	const std::string text = typed.empty() ? std::to_string(value) : typed;
	p.text_in_idx(
			text, TileRect(rect.x + rect.h, rect.y, rect.w - 2 * rect.h, rect.h), Mui_font_role::body,
			typed.empty() ? ink : p.col(Mui_colour::accent), fill, Mui_align::centre);
	if (is_focus()) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

void Mui_number_field::set(int v) {
	v = std::clamp(v, minv, maxv);
	typed.clear();
	if (v == value) {
		invalidate();
		return;
	}
	value = v;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(value);
	}
}

void Mui_number_field::commit() {
	if (!typed.empty()) {
		const int v = std::atoi(typed.c_str());
		typed.clear();
		set(v);
	}
}

void Mui_number_field::on_blur() {
	commit();
}

bool Mui_number_field::on_mouse_down(int x, int y, int button) {
	press = 0;
	if (button != 1 || !is_live()) {
		return false;
	}
	if (minus_rect().has_point(x, y)) {
		press = -1;
	} else if (plus_rect().has_point(x, y)) {
		press = 1;
	}
	invalidate();
	return true;
}

bool Mui_number_field::on_mouse_up(int x, int y, int button, bool inside) {
	if (button == 1 && inside) {
		if (press < 0 && minus_rect().has_point(x, y)) {
			commit();
			set(value - step);
		} else if (press > 0 && plus_rect().has_point(x, y)) {
			commit();
			set(value + step);
		}
	}
	press = 0;
	invalidate();
	return true;
}

bool Mui_number_field::on_key(SDL_Keycode key, int unicode) {
	if (key == SDLK_LEFT || key == SDLK_DOWN) {
		commit();
		set(value - step);
		return true;
	}
	if (key == SDLK_RIGHT || key == SDLK_UP) {
		commit();
		set(value + step);
		return true;
	}
	if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
		if (!typed.empty()) {
			commit();
			return true;
		}
		return false;
	}
	if (key == SDLK_BACKSPACE) {
		if (!typed.empty()) {
			typed.pop_back();
			invalidate();
		}
		return true;
	}
	if (key == SDLK_ESCAPE && !typed.empty()) {
		typed.clear();
		invalidate();
		return true;
	}
	if ((unicode >= '0' && unicode <= '9') || (unicode == '-' && typed.empty() && minv < 0)) {
		if (typed.size() < 9) {
			typed.push_back(char(unicode));
			invalidate();
		}
		return true;
	}
	return false;
}

bool Mui_number_field::on_wheel(int x, int y, int dy) {
	ignore_unused(x, y);
	commit();
	set(value + dy * step);
	return true;
}

/*
 *  Mui_text_field
 */

Mui_text_field::Mui_text_field(
		std::string id_, std::string text_, std::string placeholder_, std::function<void(const std::string&)> change)
		: Mui_widget(std::move(id_)), text(std::move(text_)), placeholder(std::move(placeholder_)), on_change(std::move(change)) {
	focusable = true;
}

void Mui_text_field::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	h = p.px(metrics(p).control_h);
	w = std::min(avail_w, p.px(360));
}

void Mui_text_field::paint(Mui_painter& p) {
	const uint8 fill = p.col(Mui_colour::panel_alt);
	const bool  foc  = is_focus();
	control_box(p, rect, fill, p.col(foc ? Mui_colour::accent : Mui_colour::frame), true);
	const int pad = p.px(metrics(p).unit * 1.5f);
	int       x   = rect.x + pad;
	if (search_icon) {
		const int is = rect.h / 2;
		p.icon(Mui_icon::search, TileRect(x, rect.y + (rect.h - is) / 2, is, is), p.col(Mui_colour::text_dim));
		x += is + pad / 2;
	}
	const TileRect tr(x, rect.y, rect.x + rect.w - pad - x, rect.h);
	const auto*    f = p.font(Mui_font_role::body);
	if (!f) {
		return;
	}
	const size_t cur = cursor == std::string::npos ? text.size() : std::min(cursor, text.size());
	if (text.empty()) {
		p.text_in_idx(placeholder, tr, Mui_font_role::body, p.col(Mui_colour::text_dim), fill, Mui_align::left);
	}
	// Scroll the text so the cursor stays visible.
	int       offset = 0;
	const int cx     = f->width(text.data(), cur);
	if (cx > tr.w - 2) {
		offset = cx - tr.w + 2;
	}
	p.push_clip(tr);
	if (!text.empty()) {
		f->draw(p.get_buf(), text, tr.x - offset, tr.y + (tr.h - f->height()) / 2, p.col(Mui_colour::text), fill);
	}
	if (foc && (SDL_GetTicks() / 500) % 2 == 0) {
		p.fill_rect(
				TileRect(tr.x - offset + cx, tr.y + (tr.h - f->height()) / 2, std::max(1, p.px(2)), f->height()),
				p.col(Mui_colour::text));
	}
	p.pop_clip();
	if (foc) {
		p.focus_ring(rect, stone(p) ? 0 : radius(p));
	}
}

void Mui_text_field::set_text(const std::string& t, bool notify) {
	text   = t.substr(0, max_len);
	cursor = std::string::npos;
	invalidate();
	if (notify && on_change) {
		auto cb = on_change;
		cb(text);
	}
}

bool Mui_text_field::insert(const std::string& s) {
	size_t      cur = cursor == std::string::npos ? text.size() : std::min(cursor, text.size());
	std::string add;
	for (const char c : s) {
		if (static_cast<unsigned char>(c) >= ' ' && text.size() + add.size() < max_len) {
			add.push_back(c);
		}
	}
	if (add.empty()) {
		return false;
	}
	text.insert(cur, add);
	cursor = cur + add.size();
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(text);
	}
	return true;
}

bool Mui_text_field::on_mouse_down(int x, int y, int button) {
	ignore_unused(x, y);
	if (button == 1 && win) {
		win->set_focus(this);
		cursor = std::string::npos;
		return true;
	}
	return false;
}

bool Mui_text_field::on_text(const char* t) {
	return t && insert(t);
}

bool Mui_text_field::on_key(SDL_Keycode key, int unicode) {
	size_t cur = cursor == std::string::npos ? text.size() : std::min(cursor, text.size());
	switch (key) {
	case SDLK_BACKSPACE:
		if (cur > 0) {
			text.erase(cur - 1, 1);
			cursor = cur - 1;
			invalidate();
			if (on_change) {
				auto cb = on_change;
				cb(text);
			}
		}
		return true;
	case SDLK_DELETE:
		if (cur < text.size()) {
			text.erase(cur, 1);
			cursor = cur;
			invalidate();
			if (on_change) {
				auto cb = on_change;
				cb(text);
			}
		}
		return true;
	case SDLK_LEFT:
		cursor = cur > 0 ? cur - 1 : 0;
		invalidate();
		return true;
	case SDLK_RIGHT:
		cursor = std::min(text.size(), cur + 1);
		invalidate();
		return true;
	case SDLK_HOME:
		cursor = 0;
		invalidate();
		return true;
	case SDLK_END:
		cursor = std::string::npos;
		invalidate();
		return true;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (on_submit) {
			auto cb = on_submit;
			cb(text);
			return true;
		}
		return false;
	default:
		break;
	}
	// Printable keys arrive as SDL_EVENT_TEXT_INPUT while text input is on
	// (chr == SDLK_UNKNOWN); a plain key press only counts when it is off.
	if (unicode >= ' ' && unicode < 256 && unicode != 127) {
		if (key != SDLK_UNKNOWN && win && SDL_TextInputActive(SDL_GetKeyboardFocus())) {
			return true;
		}
		const char c = char(unicode);
		return insert(std::string(1, c));
	}
	return false;
}

/*
 *  Mui_grid
 */

Mui_grid::Mui_grid(std::string id_, int count_, float card_w_ref, float card_h_ref)
		: Mui_widget(std::move(id_)), count(count_), card_w(card_w_ref), card_h(card_h_ref) {
	focusable = true;
}

void Mui_grid::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	cw             = p.px(card_w);
	ch             = p.px(card_h);
	gapx           = p.px(metrics(p).gap * 2);
	cols           = std::max(1, (avail_w + gapx) / std::max(1, cw + gapx));
	const int rows = (count + cols - 1) / cols;
	w              = avail_w;
	h              = rows > 0 ? rows * (ch + gapx) - gapx : 0;
}

TileRect Mui_grid::card_rect(int i) const {
	const int c = i % cols;
	const int r = i / cols;
	return TileRect(rect.x + c * (cw + gapx), rect.y + r * (ch + gapx), cw, ch);
}

void Mui_grid::paint(Mui_painter& p) {
	for (int i = 0; i < count; i++) {
		const TileRect r = card_rect(i);
		if (!p.get_clip().intersects(r)) {
			continue;
		}
		const bool sel = i == selected;
		if (paint_card) {
			paint_card(p, i, r, sel);
		} else {
			control_box(p, r, p.col(sel ? Mui_colour::accent : Mui_colour::panel_alt), p.col(Mui_colour::frame));
		}
		if (sel && is_focus()) {
			p.focus_ring(r, stone(p) ? 0 : radius(p));
		}
	}
}

void Mui_grid::set_selected(int i, bool notify) {
	if (i < 0 || i >= count) {
		return;
	}
	const bool changed = i != selected;
	selected           = i;
	invalidate();
	if (notify && changed && on_select) {
		auto cb = on_select;
		cb(i);
	}
}

bool Mui_grid::on_mouse_down(int x, int y, int button) {
	press = -1;
	if (button != 1) {
		return false;
	}
	for (int i = 0; i < count; i++) {
		if (card_rect(i).has_point(x, y)) {
			press = i;
		}
	}
	return true;
}

bool Mui_grid::on_mouse_up(int x, int y, int button, bool inside) {
	if (button == 1 && inside && press >= 0 && card_rect(press).has_point(x, y)) {
		const bool again = press == selected;
		set_selected(press);
		if (again && on_activate) {
			auto cb = on_activate;
			cb(press);
		}
	}
	press = -1;
	return true;
}

bool Mui_grid::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	int i = selected < 0 ? 0 : selected;
	switch (key) {
	case SDLK_LEFT:
		i--;
		break;
	case SDLK_RIGHT:
		i++;
		break;
	case SDLK_UP:
		i -= cols;
		break;
	case SDLK_DOWN:
		i += cols;
		break;
	default:
		return false;
	}
	set_selected(std::clamp(i, 0, count - 1));
	return true;
}

bool Mui_grid::activate() {
	if (selected >= 0 && on_activate) {
		auto cb = on_activate;
		cb(selected);
		return true;
	}
	return false;
}

bool Mui_grid::find_part(const std::string& want, TileRect& r) {
	const int i = part_index(want, id, {}, size_t(std::max(0, count)));
	if (i < 0) {
		return false;
	}
	r = card_rect(i);
	return true;
}

/*
 *  Mui_tabs
 */

Mui_tabs::Mui_tabs(std::string id_, std::vector<std::string> labels_, int index_, bool vertical_, std::function<void(int)> change)
		: Mui_widget(std::move(id_)), labels(std::move(labels_)), index(index_), vertical(vertical_), on_change(std::move(change)) {
	focusable = true;
}

void Mui_tabs::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	tab_px = p.px(metrics(p).tab_h);
	widths.clear();
	const int pad = p.px(metrics(p).unit * 2);
	for (const auto& l : labels) {
		widths.push_back(p.text_width(l, Mui_font_role::body) + 2 * pad);
	}
	if (vertical) {
		w = avail_w;
		h = tab_px * int(labels.size());
	} else {
		w = avail_w;
		h = tab_px;
	}
}

TileRect Mui_tabs::tab_rect(int i) const {
	if (vertical) {
		return TileRect(rect.x, rect.y + i * tab_px, rect.w, tab_px);
	}
	int x = rect.x;
	for (int k = 0; k < i && k < int(widths.size()); k++) {
		x += widths[k];
	}
	return TileRect(x, rect.y, i < int(widths.size()) ? widths[i] : 0, rect.h);
}

void Mui_tabs::paint(Mui_painter& p) {
	const uint8 bg = surface(p);
	for (int i = 0; i < int(labels.size()); i++) {
		const TileRect r    = tab_rect(i);
		const bool     sel  = i == index;
		uint8          fill = bg;
		if (vertical) {
			if (sel) {
				fill = p.col(Mui_colour::panel_alt);
				p.rounded_fill(r, radius(p), fill);
				p.fill_rect(TileRect(r.x, r.y + r.h / 5, std::max(3, p.px(4)), r.h * 3 / 5), p.col(Mui_colour::accent));
			}
			TileRect tr = r;
			tr.x += p.px(metrics(p).unit * 2);
			tr.w -= p.px(metrics(p).unit * 3);
			p.text_in_idx(
					labels[i], tr, Mui_font_role::body, p.col(sel ? Mui_colour::text : Mui_colour::text_dim), fill,
					Mui_align::left);
		} else {
			if (sel) {
				if (stone(p)) {
					// Parchment tab joined to the body below.
					TileRect t = r;
					t.h += 2;
					p.texture_parchment(t, p.col(Mui_colour::panel), p.col(Mui_colour::panel), p.col(Mui_colour::panel_alt));
					p.vline(t.x, t.y, t.h, p.col(Mui_colour::frame_light));
					p.hline(t.x, t.y, t.w, p.col(Mui_colour::frame_light));
					p.vline(t.x + t.w - 1, t.y, t.h, p.col(Mui_colour::frame_dark));
					fill = p.col(Mui_colour::panel);
				} else {
					p.fill_rect(
							TileRect(r.x, r.y + r.h - std::max(3, p.px(3)), r.w, std::max(3, p.px(3))), p.col(Mui_colour::accent));
				}
			}
			const Mui_colour ink = sel ? (stone(p) ? Mui_colour::text : Mui_colour::text)
									   : (stone(p) ? Mui_colour::title_text : Mui_colour::text_dim);
			p.text_in_idx(labels[i], r, Mui_font_role::body, p.col(ink), fill, Mui_align::centre);
		}
	}
	if (is_focus() && index >= 0) {
		p.focus_ring(tab_rect(index), stone(p) ? 0 : radius(p));
	}
}

void Mui_tabs::set(int i) {
	if (i < 0 || i >= int(labels.size()) || i == index) {
		return;
	}
	index = i;
	invalidate();
	if (on_change) {
		auto cb = on_change;
		cb(index);
	}
}

bool Mui_tabs::on_mouse_down(int x, int y, int button) {
	press = -1;
	if (button != 1) {
		return false;
	}
	for (int i = 0; i < int(labels.size()); i++) {
		if (tab_rect(i).has_point(x, y)) {
			press = i;
		}
	}
	return true;
}

bool Mui_tabs::on_mouse_up(int x, int y, int button, bool inside) {
	if (button == 1 && inside && press >= 0 && tab_rect(press).has_point(x, y)) {
		const int i = press;
		press       = -1;
		set(i);
		return true;
	}
	press = -1;
	return true;
}

bool Mui_tabs::on_key(SDL_Keycode key, int unicode) {
	ignore_unused(unicode);
	const SDL_Keycode prev = vertical ? SDLK_UP : SDLK_LEFT;
	const SDL_Keycode next = vertical ? SDLK_DOWN : SDLK_RIGHT;
	if (key == prev) {
		set(index - 1);
		return true;
	}
	if (key == next) {
		set(index + 1);
		return true;
	}
	return false;
}

bool Mui_tabs::find_part(const std::string& want, TileRect& r) {
	const int i = part_index(want, id, keys, labels.size());
	if (i < 0) {
		return false;
	}
	r = tab_rect(i);
	return true;
}

std::string Mui_tabs::value_text() const {
	if (index >= 0 && size_t(index) < keys.size()) {
		return keys[index];
	}
	return std::to_string(index);
}

/*
 *  Mui_scroll_view
 */

Mui_scroll_view::Mui_scroll_view(std::unique_ptr<Mui_widget> child_, std::string id_)
		: Mui_widget(std::move(id_)), child(std::move(child_)) {
	child->parent = this;
}

void Mui_scroll_view::attach(Mui_window* w) {
	win = w;
	child->attach(w);
}

void Mui_scroll_view::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	bar_w = std::max(4, p.px(6));
	child->measure(p, avail_w, w, h);
	w = avail_w;
}

int Mui_scroll_view::content_w() const {
	// Room for the scrollbar only when the content overflows.
	return std::max(0, content_h > rect.h ? rect.w - bar_w * 2 : rect.w);
}

void Mui_scroll_view::place_child() {
	child->rect = TileRect(rect.x, rect.y - scroll, content_w(), content_h);
}

void Mui_scroll_view::layout(Mui_painter& p) {
	bar_w  = std::max(4, p.px(6));
	int cw = 0;
	child->measure(p, rect.w, cw, content_h);
	if (content_h > rect.h) {
		child->measure(p, rect.w - bar_w * 2, cw, content_h);
	}
	scroll = std::clamp(scroll, 0, std::max(0, content_h - rect.h));
	place_child();
	child->layout(p);
}

void Mui_scroll_view::set_scroll(int s) {
	const int ns = std::clamp(s, 0, std::max(0, content_h - rect.h));
	if (ns == scroll) {
		return;
	}
	const int dy = scroll - ns;
	scroll       = ns;
	// Shift the laid-out tree instead of relaying it out.
	std::vector<Mui_widget*> all;
	child->collect(all);
	for (auto* w : all) {
		w->rect.y += dy;
	}
	invalidate();
}

TileRect Mui_scroll_view::thumb_rect() const {
	if (content_h <= rect.h || content_h <= 0) {
		return TileRect(0, 0, 0, 0);
	}
	const int th = std::max(bar_w * 3, rect.h * rect.h / content_h);
	const int ty = rect.y + (rect.h - th) * scroll / std::max(1, content_h - rect.h);
	return TileRect(rect.x + rect.w - bar_w, ty, bar_w, th);
}

void Mui_scroll_view::paint(Mui_painter& p) {
	p.push_clip(rect);
	child->paint(p);
	p.pop_clip();
	const TileRect t = thumb_rect();
	if (t.w > 0) {
		p.rounded_fill(TileRect(t.x, rect.y, t.w, rect.h), t.w / 2, p.col(Mui_colour::panel_alt));
		p.rounded_fill(t, t.w / 2, p.col(drag_y >= 0 ? Mui_colour::accent : Mui_colour::frame_light));
	}
}

Mui_widget* Mui_scroll_view::hit(int x, int y) {
	if (!visible || !rect.has_point(x, y)) {
		return nullptr;
	}
	if (x >= rect.x + rect.w - bar_w * 2 && content_h > rect.h) {
		return this;
	}
	Mui_widget* w = child->hit(x, y);
	return w ? w : this;
}

void Mui_scroll_view::collect(std::vector<Mui_widget*>& out) {
	if (!visible) {
		return;
	}
	out.push_back(this);
	child->collect(out);
}

Mui_widget* Mui_scroll_view::find(const std::string& want) {
	if (Mui_widget* w = Mui_widget::find(want)) {
		return w;
	}
	return child->find(want);
}

bool Mui_scroll_view::find_part(const std::string& want, TileRect& r) {
	return child->find_part(want, r);
}

uint8 Mui_scroll_view::surface(const Mui_painter& p) const {
	return Mui_widget::surface(p);
}

void Mui_scroll_view::ensure_visible(const TileRect& r) {
	if (r.y < rect.y) {
		set_scroll(scroll - (rect.y - r.y));
	} else if (r.y + r.h > rect.y + rect.h) {
		set_scroll(scroll + (r.y + r.h - rect.y - rect.h));
	}
}

bool Mui_scroll_view::on_wheel(int x, int y, int dy) {
	ignore_unused(x, y);
	if (content_h <= rect.h) {
		return false;
	}
	set_scroll(scroll - dy * std::max(8, rect.h / 8));
	return true;
}

bool Mui_scroll_view::on_mouse_down(int x, int y, int button) {
	if (button != 1 || content_h <= rect.h) {
		return false;
	}
	const TileRect t = thumb_rect();
	if (x >= rect.x + rect.w - bar_w * 2) {
		if (!t.has_point(t.x, y)) {
			// Page towards the click.
			set_scroll(scroll + (y < t.y ? -rect.h : rect.h));
		}
		drag_y = y;
		drag_s = scroll;
		return true;
	}
	// Drag on empty body space scrolls (touch).
	drag_y = y;
	drag_s = scroll;
	return true;
}

bool Mui_scroll_view::on_mouse_move(int x, int y, bool held) {
	ignore_unused(x);
	if (!held || drag_y < 0) {
		return false;
	}
	const TileRect t = thumb_rect();
	if (t.h > 0 && x >= rect.x + rect.w - bar_w * 2) {
		set_scroll(drag_s + (y - drag_y) * (content_h - rect.h) / std::max(1, rect.h - t.h));
	} else {
		set_scroll(drag_s - (y - drag_y));
	}
	return true;
}

bool Mui_scroll_view::on_mouse_up(int x, int y, int button, bool inside) {
	ignore_unused(x, y, button, inside);
	drag_y = -1;
	invalidate();
	return true;
}

/*
 *  Mui_help_line
 */

void Mui_help_line::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	w = avail_w;
	h = p.line_height(Mui_font_role::small);
}

std::string Mui_help_line::value_text() const {
	return win ? win->current_help() : std::string();
}

void Mui_help_line::paint(Mui_painter& p) {
	p.text_in_idx(value_text(), rect, Mui_font_role::small, p.col(Mui_colour::text_dim), surface(p), Mui_align::left);
}

/*
 *  Mui_spacer
 */

void Mui_spacer::measure(Mui_painter& p, int avail_w, int& w, int& h) {
	ignore_unused(avail_w);
	w = 0;
	h = p.px(h_ref);
}
