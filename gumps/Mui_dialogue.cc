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

#include "Mui_dialogue.h"

#include "Configuration.h"
#include "Mui_font.h"
#include "Mui_painter.h"
#include "Mui_router.h"
#include "Mui_selftest.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ibuf8.h"
#include "items.h"
#include "keys.h"
#include "mouse.h"
#include "shapeid.h"
#include "tqueue.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Continue() {
			return get_text_msg(0xDC0 - msg_file_start);
		}

		static auto ContinueHint() {
			return get_text_msg(0xDC1 - msg_file_start);
		}

		static auto ChooseHint() {
			return get_text_msg(0xDC2 - msg_file_start);
		}

		static auto HistoryTitle() {
			return get_text_msg(0xDC3 - msg_file_start);
		}

		static auto HistoryHint() {
			return get_text_msg(0xDC5 - msg_file_start);
		}
	};

	// The keys that choose answers 0, 1, 2 ... (as in the classic view).
	constexpr const char option_keys[]   = "123456789abcdefghijklmnopqrstuvwxyz";
	constexpr int        num_option_keys = int(sizeof(option_keys)) - 1;

	// Reference sizes (pixels at a 720-px display height).
	constexpr float panel_ref_w  = 1180;
	constexpr float margin_ref   = 24;
	constexpr float pad_ref      = 16;
	constexpr float card_ref     = 136;
	constexpr float card_pad_ref = 6;

	Image_window* iwin() {
		return Game_window::get_instance()->get_win();
	}

	// "bye", "goodbye", "good-bye" or "farewell" (any case, trimmed).
	bool is_farewell(const std::string& s) {
		const auto b = s.find_first_not_of(" \t");
		if (b == std::string::npos) {
			return false;
		}
		const auto  e = s.find_last_not_of(" \t");
		std::string t = s.substr(b, e - b + 1);
		for (char& c : t) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return t == "bye" || t == "goodbye" || t == "good-bye" || t == "farewell";
	}

	// Text for the history: '*' and '\r' dropped, '^x' upper-cases x, trimmed.
	std::string history_text(const std::string& text) {
		std::string out;
		for (size_t i = 0; i < text.size(); i++) {
			const char c = text[i];
			if (c == '*' || c == '\r') {
				continue;
			}
			if (c == '^') {
				if (i + 1 < text.size()) {
					out += char(std::toupper(static_cast<unsigned char>(text[++i])));
				}
				continue;
			}
			out += c;
		}
		const auto b = out.find_first_not_of(" \t\n");
		if (b == std::string::npos) {
			return std::string();
		}
		return out.substr(b, out.find_last_not_of(" \t\n") - b + 1);
	}

	std::string join_lines(const std::vector<std::string>& lines, const char* sep) {
		std::string out;
		for (const auto& l : lines) {
			if (!out.empty()) {
				out += sep;
			}
			out += l;
		}
		return out;
	}
}    // namespace

/*
 *  Resolve the conversation style: config/gameplay/dialogue_style, where a
 *  missing or empty key and "follow" use the menu style. Reading never
 *  creates the key.
 */
bool Mui_dialogue::resolve_style(std::string& theme_out) {
	std::string style;
	config->value("config/gameplay/dialogue_style", style, "follow");
	if (style.empty() || style == "follow") {
		style = Mui::menu_style();
	}
	theme_out         = "classic";
	Game_window* gwin = Game_window::get_instance();
	if (style == "classic" || !gwin || gwin->is_in_exult_menu() || Mui_theme::load(style) == nullptr) {
		return false;
	}
	theme_out = style;
	return true;
}

Mui_dialogue::Mui_dialogue(std::shared_ptr<Mui_theme> theme_) : theme(std::move(theme_)) {}

Mui_dialogue::~Mui_dialogue() {
	destroy_layers();
}

/*
 *  Layers: the panel layer and, above it, the portrait layer, both the
 *  size of the display and below every gump.
 */
bool Mui_dialogue::ensure_layers() {
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
	layer      = w->create_layer("mui dlg", dw, dh, 255, 0, 0);
	face_layer = w->create_layer("mui dlg faces", dw, dh, 255, 0, 1);
	if (layer < 0 || face_layer < 0) {
		destroy_layers();
		return false;
	}
	layer_w = dw;
	layer_h = dh;
	for (const int l : {layer, face_layer}) {
		w->layer_set_ui_kind(l, Image_window::UiLayerMenus);
		w->layer_set_dest(l, 0, 0, dw, dh);
		w->layer_set_visible(l, false);
	}
	// Translucent face pixels (Guardian, serpents) become real alpha, with
	// the same halfway boost as the classic conversation layer (copied from
	// Conversation::get_conv_layer, which explains the boost).
	const uint32* base = Shape_manager::get_instance()->get_translucency_argb();
	if (base) {
		uint32 boosted[256];
		for (int i = 0; i < 256; i++) {
			const uint32 argb = base[i];
			if (argb == 0) {
				boosted[i] = 0;
				continue;
			}
			const uint32 a  = (argb >> 24) & 0xff;
			const uint32 a2 = 255 - ((255 - a) * (255 - a)) / 255;
			const uint32 af = (a + a2) / 2;
			boosted[i]      = (af << 24) | (argb & 0x00ffffffu);
		}
		w->layer_set_index_argb(face_layer, boosted);
	}
	scale = Mui_theme::ui_scale(dw, dh);
	shown = false;
	dirty = true;
	return true;
}

void Mui_dialogue::destroy_layers() {
	Image_window* w = iwin();
	if (layer >= 0) {
		w->destroy_layer(layer);
	}
	if (face_layer >= 0) {
		w->destroy_layer(face_layer);
	}
	layer = face_layer = -1;
	layer_w = layer_h = 0;
}

void Mui_dialogue::hide() {
	Image_window* w = iwin();
	if (layer >= 0) {
		w->layer_set_visible(layer, false);
	}
	if (face_layer >= 0) {
		w->layer_set_visible(face_layer, false);
	}
	shown = false;
	Game_window::get_instance()->set_all_dirty();
}

const Mui_text_font* Mui_dialogue::body_font() const {
	const auto& spec = theme->font(Mui_font_role::body);
	return Mui_fonts::get().pick(spec.face, int(std::lround(spec.px * scale * text_size)));
}

/*
 *  State pushed by Conversation.
 */

void Mui_dialogue::set_faces(const std::array<Face, 2>& list, int active_slot) {
	faces  = list;
	active = active_slot;
	// The text goes with the face that said it.
	if (text_slot >= 0 && (text_slot > 1 || !faces[size_t(text_slot)].used || faces[size_t(text_slot)].shape != text_shape)) {
		text_slot = -1;
		page_lines.clear();
		page_text.clear();
	}
	dirty = true;
}

void Mui_dialogue::set_answers(const std::vector<std::string>& list) {
	answers = list;
	focus   = std::clamp(focus, 0, std::max(0, int(answers.size()) - 1));
	hover   = -1;
	dirty   = true;
}

void Mui_dialogue::show_choices(bool on) {
	if (on && !choosing) {
		focus  = 0;
		scroll = 0;
	}
	choosing = on;
	hover    = -1;
	dirty    = true;
}

std::string Mui_dialogue::speaker() const {
	const int slot = text_slot >= 0 ? text_slot : active;
	if (slot < 0 || slot > 1 || !faces[size_t(slot)].used) {
		return std::string();
	}
	return faces[size_t(slot)].name;
}

/*
 *  Layout (reference pixels at 720 high, times the theme scale): a panel
 *  at the bottom centre with the portrait cards at its sides, the speaker
 *  name above the text, the answers below it and a hint line at the
 *  bottom. Choosing grows the panel upwards, at most to 60 % of the
 *  display; the answers scroll beyond that.
 */
void Mui_dialogue::layout(Mui_painter& p) {
	const auto&          m     = theme->metrics;
	const bool           stone = theme->frame_stone;
	const Mui_text_font* body  = body_font();
	Layout               L;
	L.line_h           = body ? std::max(1, body->height()) : p.px(20);
	L.gap              = p.px(m.gap);
	const int margin   = p.px(margin_ref);
	const int fb       = stone ? p.px(m.unit * 1.5f) : std::max(1, p.px(m.border));
	const int pad      = p.px(pad_ref);
	const int card     = p.px(card_ref);
	const int small_h  = p.line_height(Mui_font_role::small);
	const int plate_h  = p.line_height(Mui_font_role::heading);
	const int w        = std::max(card * 2, std::min(p.px(panel_ref_w), layer_w - 2 * margin));
	bool      any_card = false;
	for (size_t i = 0; i < 2; i++) {
		L.card_shown[i] = faces[i].used && faces[i].drawable && !faces[i].large;
		any_card        = any_card || L.card_shown[i];
	}
	L.lines            = std::max(3, std::max(4 * L.line_h, card - plate_h - L.gap) / L.line_h);
	const int text_h   = L.lines * L.line_h;
	const int cards_h  = any_card ? card + L.gap + small_h : 0;
	const int content  = std::max(cards_h, plate_h + L.gap + text_h);
	const int footer_h = std::max(small_h, p.px(20));
	const int base_h   = 2 * (fb + pad) + content + L.gap + footer_h;
	int       ans_h    = 0;
	if (choosing && !answers.empty()) {
		const int n  = int(answers.size());
		L.cols       = n <= 4 ? 1 : n <= 10 ? 2 : 3;
		L.rows       = (n + L.cols - 1) / L.cols;
		L.btn_h      = p.px(m.control_h);
		L.content_h  = L.rows * L.btn_h + (L.rows - 1) * L.gap;
		const int mx = layer_h * 6 / 10 - base_h - 2 * L.gap;
		ans_h        = std::min(L.content_h, std::max(L.btn_h, mx));
		if (ans_h < L.content_h) {
			L.bar_w = std::max(4, p.px(6));
		}
	}
	const int h = base_h + (ans_h > 0 ? ans_h + 2 * L.gap : 0);
	L.panel     = TileRect((layer_w - w) / 2, std::max(0, layer_h - margin - h), w, h);
	L.inner     = TileRect(L.panel.x + fb + pad, L.panel.y + fb + pad, w - 2 * (fb + pad), h - 2 * (fb + pad));
	int tx      = L.inner.x;
	int tr      = L.inner.x + L.inner.w;
	if (L.card_shown[0]) {
		L.cards[0] = TileRect(L.inner.x, L.inner.y, card, card);
		tx += card + pad;
	}
	if (L.card_shown[1]) {
		L.cards[1] = TileRect(tr - card, L.inner.y, card, card);
		tr -= card + pad;
	}
	L.plate  = TileRect(tx, L.inner.y, tr - tx, plate_h);
	L.text   = TileRect(tx, L.plate.y + plate_h + L.gap, tr - tx, text_h);
	L.footer = TileRect(L.inner.x, L.inner.y + L.inner.h - footer_h, L.inner.w, footer_h);
	if (ans_h > 0) {
		L.answers = TileRect(L.inner.x, L.inner.y + content + L.gap, L.inner.w, ans_h);
	}
	const int icon_w = small_h;
	const int cue_w  = icon_w + L.gap / 2 + p.text_width(Strings::Continue(), Mui_font_role::small);
	L.cue            = TileRect(L.footer.x + L.footer.w - cue_w, L.footer.y, cue_w, footer_h);
	lay              = L;
	scroll           = std::clamp(scroll, 0, std::max(0, lay.content_h - lay.answers.h));
}

/*
 *  Paging, with the rules of Shape_manager::paint_text_box: '\n' breaks
 *  the line, '\r' is dropped, '^x' upper-cases x, '*' starts a new page
 *  unless it falls on the page's first line (then it is dropped).
 */
std::vector<std::vector<std::string>> Mui_dialogue::paginate(const std::string& text, const Mui_text_font* font) const {
	std::vector<std::string> chunks(1);
	for (size_t i = 0; i < text.size(); i++) {
		const char c = text[i];
		if (c == '\r') {
			continue;
		}
		if (c == '^') {
			if (i + 1 < text.size()) {
				chunks.back() += char(std::toupper(static_cast<unsigned char>(text[++i])));
			}
			continue;
		}
		if (c == '*') {
			chunks.emplace_back();
			continue;
		}
		chunks.back() += c;
	}
	std::vector<std::vector<std::string>> pages;
	const size_t                          per   = size_t(std::max(3, lay.lines));
	const int                             max_w = std::max(1, lay.text.w);
	auto                                  wrap  = [&](const std::string& s) {
        return font ? font->wrap(s, max_w) : std::vector<std::string>{s};
	};
	// Move whole pages from the front of lines to pages.
	auto take_full = [&](std::vector<std::string>& lines) {
		while (lines.size() > per) {
			pages.emplace_back(lines.begin(), lines.begin() + long(per));
			lines.erase(lines.begin(), lines.begin() + long(per));
		}
	};
	std::string cur;    // Text of the page being filled.
	for (size_t k = 0; k < chunks.size(); k++) {
		if (k > 0) {    // A '*' came before this chunk.
			std::vector<std::string> lines = wrap(cur);
			take_full(lines);
			if (lines.size() > 1) {
				pages.push_back(lines);    // Past the first line: new page.
				cur.clear();
			} else {
				cur = lines.empty() ? std::string() : lines[0];    // Dropped.
			}
		}
		cur += chunks[k];
	}
	std::vector<std::string> lines = wrap(cur);
	take_full(lines);
	if (!lines.empty() && !(lines.size() == 1 && lines[0].empty())) {
		pages.push_back(lines);
	}
	return pages;
}

void Mui_dialogue::show_page(const std::vector<std::string>& lines) {
	page_lines = lines;
	page_text  = join_lines(lines, " ");
	if (Mui_selftest::active()) {
		log.push_back(page_text);
	}
	dirty = true;
	paint();
}

void Mui_dialogue::say(const std::string& text, bool large) {
	text_large       = large;
	text_slot        = active >= 0 && active <= 1 ? active : -1;
	text_shape       = text_slot >= 0 ? faces[size_t(text_slot)].shape : -1;
	std::string said = history_text(text);
	if (!said.empty()) {
		history.push_back(Entry{speaker(), std::move(said), false});
	}
	std::vector<std::vector<std::string>> pages;
	if (ensure_layers()) {
		auto*       buf = static_cast<Image_buffer8*>(iwin()->get_layer_ibuf(layer));
		Mui_painter p(buf, *theme, scale);
		layout(p);
		pages = paginate(text, body_font());
	}
	if (pages.empty()) {
		pages.emplace_back();
	}
	for (size_t k = 0; k < pages.size(); k++) {
		show_page(pages[k]);
		if (k + 1 < pages.size()) {
			wait_input(Wait::page);
		}
	}
}

void Mui_dialogue::add_choice(const std::string& who, const std::string& text) {
	history.push_back(Entry{who, text, true});
	dirty = true;
}

void Mui_dialogue::clear_history() {
	history.clear();
	hist_open   = false;
	hist_drag   = false;
	hist_scroll = 0;
	dirty       = true;
}

void Mui_dialogue::wait_continue() {
	wait_input(Wait::cont);
}

int Mui_dialogue::choose() {
	if (answers.empty()) {
		return -2;
	}
	return wait_input(Wait::choice);
}

/*
 *  Answers: column-major grid, so keys 1, 2, 3 ... run down the columns.
 */

TileRect Mui_dialogue::answer_rect(int i) const {
	if (lay.rows <= 0) {
		return TileRect(0, 0, 0, 0);
	}
	const int col = i / lay.rows;
	const int row = i % lay.rows;
	const int aw  = lay.answers.w - (lay.bar_w > 0 ? lay.bar_w + lay.gap : 0);
	const int bw  = (aw - (lay.cols - 1) * lay.gap) / lay.cols;
	return TileRect(lay.answers.x + col * (bw + lay.gap), lay.answers.y + row * (lay.btn_h + lay.gap) - scroll, bw, lay.btn_h);
}

int Mui_dialogue::answer_at(int x, int y) const {
	if (!choosing || !lay.answers.has_point(x, y)) {
		return -1;
	}
	for (int i = 0; i < int(answers.size()); i++) {
		if (answer_rect(i).has_point(x, y)) {
			return i;
		}
	}
	return -1;
}

void Mui_dialogue::set_focus(int i) {
	if (answers.empty()) {
		return;
	}
	focus            = std::clamp(i, 0, int(answers.size()) - 1);
	const TileRect r = answer_rect(focus);
	if (r.y < lay.answers.y) {
		scroll_by(r.y - lay.answers.y);
	} else if (r.y + r.h > lay.answers.y + lay.answers.h) {
		scroll_by(r.y + r.h - lay.answers.y - lay.answers.h);
	}
	dirty = true;
}

void Mui_dialogue::scroll_by(int dy) {
	scroll = std::clamp(scroll + dy, 0, std::max(0, lay.content_h - lay.answers.h));
	dirty  = true;
}

/*
 *  Painting.
 */

void Mui_dialogue::paint_card(Mui_painter& p, int slot) {
	const auto&    th   = *theme;
	const TileRect r    = lay.cards[slot];
	const int      rad  = p.px(th.metrics.radius);
	const int      line = std::max(2, p.px(2));
	if (th.frame_stone) {
		p.fill_rect(r, th.colour(Mui_colour::panel_alt));
		p.bevel(r, line, th.colour(Mui_colour::frame_dark), th.colour(Mui_colour::frame_light));
		if (slot == active) {
			TileRect o = r;
			o.enlarge(line);
			p.frame_rect(o, line, th.colour(Mui_colour::accent));
		}
	} else {
		p.rounded_fill(r, rad, th.colour(Mui_colour::panel_alt));
		p.rounded_frame(
				r, rad, slot == active ? line : std::max(1, p.px(th.metrics.border)),
				th.colour(slot == active ? Mui_colour::accent : Mui_colour::frame));
	}
	// The name of a card that is not speaking goes below it.
	const int spk = text_slot >= 0 ? text_slot : active;
	if (slot != spk && !faces[size_t(slot)].name.empty()) {
		const uint8 surf = th.colour(th.frame_stone ? Mui_colour::panel : Mui_colour::window);
		const int   pad  = p.px(pad_ref);
		TileRect    nr(r.x - pad / 2, r.y + r.h + lay.gap, r.w + pad, p.line_height(Mui_font_role::small));
		p.text_in_idx(faces[size_t(slot)].name, nr, Mui_font_role::small, th.colour(Mui_colour::text_dim), surf, Mui_align::centre);
	}
}

void Mui_dialogue::paint_answers(Mui_painter& p) {
	const auto& th   = *theme;
	const uint8 surf = th.colour(th.frame_stone ? Mui_colour::panel : Mui_colour::window);
	const int   rad  = p.px(th.metrics.radius);
	const int   bord = std::max(1, p.px(th.metrics.border));
	// Room for the focus ring around the edge buttons.
	TileRect clip = lay.answers;
	clip.enlarge(std::max(2, p.px(2)) + 2);
	p.push_clip(clip);
	for (int i = 0; i < int(answers.size()); i++) {
		const TileRect r = answer_rect(i);
		if (r.y + r.h < lay.answers.y || r.y > lay.answers.y + lay.answers.h) {
			continue;
		}
		const bool farewell = is_farewell(answers[size_t(i)]);
		const bool hot      = i == hover;
		uint8      fill     = th.colour(farewell ? Mui_colour::panel : Mui_colour::panel_alt);
		uint8      ink      = th.colour(farewell ? Mui_colour::text_dim : Mui_colour::text);
		uint8      outline  = th.colour(Mui_colour::frame);
		if (farewell && !th.frame_stone) {
			fill = surf;    // Secondary: outline only.
		}
		if (hot) {
			fill = th.colour(Mui_colour::accent_hover);
			ink  = th.colour(Mui_colour::text_on_accent);
		}
		if (th.frame_stone) {
			p.fill_rect(r, fill);
			if (!farewell || hot) {
				p.bevel(r, std::max(1, p.px(2)), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark));
			} else {
				p.frame_rect(r, bord, th.colour(Mui_colour::frame_dark));
			}
		} else {
			p.rounded_fill(r, rad, fill);
			p.rounded_frame(r, rad, bord, outline);
		}
		// Key badge, then the text.
		const int bs = r.h - 2 * std::max(2, p.px(5));
		TileRect  badge(r.x + (r.h - bs) / 2, r.y + (r.h - bs) / 2, bs, bs);
		if (i < num_option_keys) {
			const uint8 bfill = th.colour(farewell && !hot ? Mui_colour::frame : Mui_colour::accent);
			if (th.frame_stone) {
				p.fill_rect(badge, bfill);
			} else {
				p.rounded_fill(badge, std::max(1, rad / 2), bfill);
			}
			const char key[2] = {option_keys[i], 0};
			p.text_in_idx(key, badge, Mui_font_role::small, th.colour(Mui_colour::text_on_accent), bfill, Mui_align::centre);
		}
		const int tx = badge.x + badge.w + p.px(8);
		p.text_in_idx(
				answers[size_t(i)], TileRect(tx, r.y, r.x + r.w - tx - p.px(8), r.h), Mui_font_role::body, ink, fill,
				Mui_align::left);
		if (i == focus) {
			p.focus_ring(r, th.frame_stone ? 0 : rad);
		}
	}
	p.pop_clip();
	if (lay.bar_w > 0) {
		const TileRect track(lay.answers.x + lay.answers.w - lay.bar_w, lay.answers.y, lay.bar_w, lay.answers.h);
		const int      th_h = std::max(lay.bar_w * 3, track.h * track.h / std::max(1, lay.content_h));
		const int      th_y = track.y + (track.h - th_h) * scroll / std::max(1, lay.content_h - track.h);
		p.rounded_fill(track, lay.bar_w / 2, th.colour(Mui_colour::panel_alt));
		p.rounded_fill(TileRect(track.x, th_y, track.w, th_h), lay.bar_w / 2, th.colour(Mui_colour::frame_light));
	}
}

/*
 *  The panel background: stone frame and parchment page for stone themes,
 *  a rounded flat panel otherwise (as Mui_window::paint_frame).
 */
void Mui_dialogue::paint_frame(Mui_painter& p, const TileRect& panel) {
	const auto& th    = *theme;
	const auto& m     = th.metrics;
	const bool  stone = th.frame_stone;
	if (stone) {
		p.texture_stone(
				panel, th.colour(Mui_colour::frame), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark),
				th.colour(Mui_colour::window));
		p.bevel(panel, std::max(1, p.px(2)), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark));
		const int fb = p.px(m.unit * 1.5f);
		TileRect  in(panel.x + fb, panel.y + fb, panel.w - 2 * fb, panel.h - 2 * fb);
		TileRect  rule = in;
		rule.enlarge(std::max(1, p.px(2)));
		p.frame_rect(rule, std::max(1, p.px(1.5f)), th.colour(Mui_colour::accent));
		p.texture_parchment(in, th.colour(Mui_colour::panel), th.colour(Mui_colour::panel), th.colour(Mui_colour::panel_alt));
		p.bevel(in, std::max(1, p.px(2)), th.colour(Mui_colour::frame_dark), th.colour(Mui_colour::frame_light));
	} else {
		const int rad = p.px(m.radius * 2);
		p.rounded_fill(panel, rad, th.colour(Mui_colour::window));
		p.rounded_frame(panel, rad, std::max(1, p.px(m.border)), th.colour(Mui_colour::frame));
	}
}

void Mui_dialogue::paint_panel(Mui_painter& p) {
	const auto& th = *theme;
	paint_frame(p, lay.panel);
	const uint8 surf = th.colour(th.frame_stone ? Mui_colour::panel : Mui_colour::window);
	for (int i = 0; i < 2; i++) {
		if (lay.card_shown[i]) {
			paint_card(p, i);
		}
	}
	// Speaker name and text.
	const std::string who = speaker();
	if (!who.empty()) {
		p.text_in_idx(who, lay.plate, Mui_font_role::heading, th.colour(Mui_colour::text), surf, Mui_align::left);
	}
	if (const Mui_text_font* body = body_font()) {
		const uint8 ink = th.colour(text_large ? Mui_colour::danger : Mui_colour::text);
		p.push_clip(lay.text);
		for (size_t i = 0; i < page_lines.size(); i++) {
			body->draw(p.get_buf(), page_lines[i], lay.text.x, lay.text.y + int(i) * lay.line_h, ink, surf);
		}
		p.pop_clip();
	}
	if (choosing && lay.answers.h > 0) {
		paint_answers(p);
	}
	// Footer: hint on the left, continue cue on the right.
	const bool  cont = wait == Wait::page || wait == Wait::cont;
	const char* hint = choosing ? Strings::ChooseHint() : Strings::ContinueHint();
	TileRect    hr   = lay.footer;
	if (cont) {
		hr.w         = lay.cue.x - hr.x - lay.gap;
		const int is = lay.cue.h * 4 / 5;
		p.icon(Mui_icon::arrow_down, TileRect(lay.cue.x, lay.cue.y + (lay.cue.h - is) / 2, is, is), th.colour(Mui_colour::accent));
		TileRect ct(lay.cue.x + is + lay.gap / 2, lay.cue.y, lay.cue.w - is - lay.gap / 2, lay.cue.h);
		p.text_in_idx(Strings::Continue(), ct, Mui_font_role::small, th.colour(Mui_colour::text), surf, Mui_align::left);
	}
	p.text_in_idx(hint, hr, Mui_font_role::small, th.colour(Mui_colour::text_dim), surf, Mui_align::left);
}

/*
 *  History mode: the panel grows to 70 % of the display, with a header and
 *  close button, the entries (one speaker label per run of lines of the
 *  same speaker; chosen answers start with "> ") and a scrollbar.
 */
void Mui_dialogue::layout_history(Mui_painter& p) {
	const auto&          m    = theme->metrics;
	const Mui_text_font* body = body_font();
	Hist_layout          H;
	H.line_h         = body ? std::max(1, body->height()) : p.px(20);
	H.label_h        = p.line_height(Mui_font_role::heading);
	const int margin = p.px(margin_ref);
	const int fb     = theme->frame_stone ? p.px(m.unit * 1.5f) : std::max(1, p.px(m.border));
	const int pad    = p.px(pad_ref);
	const int gap    = p.px(m.gap);
	const int w      = lay.panel.w;
	const int h      = std::min(layer_h - margin, std::max(lay.panel.h, layer_h * 7 / 10));
	H.panel          = TileRect((layer_w - w) / 2, std::max(0, layer_h - margin - h), w, h);
	const TileRect inner(H.panel.x + fb + pad, H.panel.y + fb + pad, w - 2 * (fb + pad), h - 2 * (fb + pad));
	const int      head_h   = std::max(H.label_h, p.px(m.control_h));
	const int      footer_h = std::max(p.line_height(Mui_font_role::small), p.px(20));
	const int      bar_w    = std::max(6, p.px(10));
	H.header                = TileRect(inner.x, inner.y, inner.w - head_h - gap, head_h);
	H.close                 = TileRect(inner.x + inner.w - head_h, inner.y, head_h, head_h);
	H.footer                = TileRect(inner.x, inner.y + inner.h - footer_h, inner.w, footer_h);
	const int vy            = inner.y + head_h + gap;
	const int vh            = std::max(H.line_h, H.footer.y - gap - vy);
	H.track                 = TileRect(inner.x + inner.w - bar_w, vy, bar_w, vh);
	H.view                  = TileRect(inner.x, vy, inner.w - bar_w - gap, vh);
	int         y           = 0;
	bool        first       = true;
	std::string prev;
	for (const Entry& e : history) {
		if (first || e.speaker != prev) {
			if (!first) {
				y += gap;
			}
			if (!e.speaker.empty()) {
				H.lines.push_back(Hist_line{e.speaker, y, true, e.choice});
				y += H.label_h;
			}
			prev = e.speaker;
		} else {
			y += gap / 2;
		}
		first                                = false;
		const std::string              t     = e.choice ? "> " + e.text : e.text;
		const std::vector<std::string> lines = body ? body->wrap(t, std::max(1, H.view.w)) : std::vector<std::string>{t};
		for (const auto& l : lines) {
			H.lines.push_back(Hist_line{l, y, false, e.choice});
			y += H.line_h;
		}
	}
	H.content_h       = y;
	hl                = std::move(H);
	const int max_s   = hist_max_scroll();
	hist_scroll       = std::clamp(hist_scroll, 0, max_s);
	const int thumb_h = max_s > 0 ? std::max(bar_w * 3, hl.track.h * hl.view.h / std::max(1, hl.content_h)) : hl.track.h;
	const int th_h    = std::min(thumb_h, hl.track.h);
	const int th_y    = hl.track.y + (max_s > 0 ? (hl.track.h - th_h) * hist_scroll / max_s : 0);
	hl.thumb          = TileRect(hl.track.x, th_y, hl.track.w, th_h);
}

void Mui_dialogue::paint_history(Mui_painter& p) {
	const auto& th   = *theme;
	const uint8 surf = th.colour(th.frame_stone ? Mui_colour::panel : Mui_colour::window);
	const int   rad  = p.px(th.metrics.radius);
	paint_frame(p, hl.panel);
	p.text_in_idx(Strings::HistoryTitle(), hl.header, Mui_font_role::heading, th.colour(Mui_colour::text), surf, Mui_align::left);
	// Close button.
	const uint8 cfill = th.colour(Mui_colour::panel_alt);
	if (th.frame_stone) {
		p.fill_rect(hl.close, cfill);
		p.bevel(hl.close, std::max(1, p.px(2)), th.colour(Mui_colour::frame_light), th.colour(Mui_colour::frame_dark));
	} else {
		p.rounded_fill(hl.close, rad, cfill);
		p.rounded_frame(hl.close, rad, std::max(1, p.px(th.metrics.border)), th.colour(Mui_colour::frame));
	}
	TileRect ci = hl.close;
	ci.enlarge(-hl.close.h / 4);
	p.icon(Mui_icon::close, ci, th.colour(Mui_colour::text));
	// Entries.
	const Mui_text_font* body = body_font();
	p.push_clip(hl.view);
	for (const Hist_line& l : hl.lines) {
		const int y = hl.view.y + l.y - hist_scroll;
		if (y + std::max(hl.line_h, hl.label_h) < hl.view.y || y > hl.view.y + hl.view.h) {
			continue;
		}
		if (l.label) {
			p.text_idx(l.text, hl.view.x, y, Mui_font_role::heading, th.colour(Mui_colour::accent), surf);
		} else if (body) {
			body->draw(p.get_buf(), l.text, hl.view.x, y, th.colour(l.choice ? Mui_colour::text_dim : Mui_colour::text), surf);
		}
	}
	p.pop_clip();
	// Scrollbar.
	const int bw = hl.track.w;
	p.rounded_fill(hl.track, bw / 2, th.colour(Mui_colour::panel_alt));
	p.rounded_fill(hl.thumb, bw / 2, th.colour(hist_drag ? Mui_colour::accent : Mui_colour::frame_light));
	p.text_in_idx(Strings::HistoryHint(), hl.footer, Mui_font_role::small, th.colour(Mui_colour::text_dim), surf, Mui_align::left);
}

int Mui_dialogue::hist_max_scroll() const {
	return std::max(0, hl.content_h - hl.view.h);
}

void Mui_dialogue::hist_scroll_to(int y) {
	hist_scroll = std::clamp(y, 0, hist_max_scroll());
	dirty       = true;
}

// Opens scrolled to the bottom (the newest lines).
void Mui_dialogue::open_history() {
	if (hist_open) {
		return;
	}
	hist_open   = true;
	hist_drag   = false;
	hist_scroll = std::numeric_limits<int>::max() / 2;
	hover       = -1;
	dirty       = true;
	paint();    // Layout must be current for the next input.
}

void Mui_dialogue::close_history() {
	hist_open = false;
	hist_drag = false;
	dirty     = true;
}

/*
 *  Keys while the history is open: Escape and H close it, End goes to the
 *  bottom and closes it when already there, Home/PgUp/PgDn/Up/Down
 *  scroll. Every other key is ignored (it must not continue or choose).
 */
bool Mui_dialogue::hist_key(int key) {
	const int page = std::max(hl.line_h, hl.view.h - hl.line_h);
	switch (key) {
	case SDLK_ESCAPE:
	case SDLK_H:
		close_history();
		break;
	case SDLK_END:
		if (hist_scroll >= hist_max_scroll()) {
			close_history();
		} else {
			hist_scroll_to(hist_max_scroll());
		}
		break;
	case SDLK_HOME:
		hist_scroll_to(0);
		break;
	case SDLK_PAGEUP:
		hist_scroll_to(hist_scroll - page);
		break;
	case SDLK_PAGEDOWN:
		hist_scroll_to(hist_scroll + page);
		break;
	case SDLK_UP:
		hist_scroll_to(hist_scroll - hl.line_h);
		break;
	case SDLK_DOWN:
		hist_scroll_to(hist_scroll + hl.line_h);
		break;
	default:
		return false;
	}
	return true;
}

void Mui_dialogue::paint_faces(Mui_painter& p) {
	const int pad    = p.px(card_pad_ref);
	const int margin = p.px(margin_ref);
	for (size_t i = 0; i < 2; i++) {
		const Face& f = faces[i];
		if (!f.used || !f.drawable) {
			continue;
		}
		ShapeID      sid(f.shape, f.frame, SF_FACES_VGA);
		Shape_frame* fr = sid.get_shape();
		if (!fr || fr->get_width() <= 0 || fr->get_height() <= 0) {
			continue;
		}
		const int fw = fr->get_width();
		const int fh = fr->get_height();
		if (f.large) {
			// Centred above the panel at the largest integer scale.
			const int aw = layer_w - 2 * margin;
			const int ah = lay.panel.y - margin;
			const int s  = std::max(1, std::min(aw / fw, ah / fh));
			const int w  = fw * s;
			const int h  = fh * s;
			p.blit_shape(fr, TileRect((layer_w - w) / 2, std::max(0, lay.panel.y - lay.gap - h), w, h), false);
			continue;
		}
		if (!lay.card_shown[i]) {
			continue;
		}
		TileRect in = lay.cards[i];
		in.enlarge(-pad);
		const int s = std::min(in.w / fw, in.h / fh);
		if (s < 1) {
			p.blit_shape(fr, in, true);    // Too big for the card: shrink.
			continue;
		}
		const int w = fw * s;
		const int h = fh * s;
		p.blit_shape(fr, TileRect(in.x + (in.w - w) / 2, in.y + (in.h - h) / 2, w, h), false);
	}
}

void Mui_dialogue::paint() {
	if (!ensure_layers()) {
		return;
	}
	const bool visible = choosing || hist_open || faces[0].used || faces[1].used;
	if (!visible) {
		if (shown) {
			hide();
		}
		return;
	}
	Image_window* w    = iwin();
	auto*         buf  = static_cast<Image_buffer8*>(w->get_layer_ibuf(layer));
	auto*         fbuf = static_cast<Image_buffer8*>(w->get_layer_ibuf(face_layer));
	buf->clear_clip();
	buf->fill8(255);
	fbuf->clear_clip();
	fbuf->fill8(255);
	Mui_painter p(buf, *theme, scale);
	layout(p);
	if (hist_open) {
		layout_history(p);
		paint_history(p);
	} else {
		paint_panel(p);
	}
	buf->clear_clip();
	Mui_painter fp(fbuf, *theme, scale);
	if (!hist_open) {    // The portraits are hidden while the history is open.
		paint_faces(fp);
	}
	fbuf->clear_clip();
	w->layer_set_dirty(layer);
	w->layer_set_dirty(face_layer);
	w->layer_set_visible(layer, true);
	w->layer_set_visible(face_layer, !hist_open);
	shown = true;
	dirty = false;
	Game_window::get_instance()->set_painted();
}

bool Mui_dialogue::part_rect(const std::string& part, TileRect& r) {
	if (!ensure_layers()) {
		return false;
	}
	if (dirty) {
		paint();    // Layout must be current.
	}
	if (part == "text") {
		r = lay.text;
		return true;
	}
	if (part == "panel") {
		r = hist_open ? hl.panel : lay.panel;
		return true;
	}
	if (part.compare(0, 8, "history.") == 0) {
		const std::string what = part.substr(8);
		r                      = what == "close" ? hl.close : what == "thumb" ? hl.thumb : hl.track;
		return hist_open && (what == "close" || what == "thumb" || what == "track");
	}
	if (part == "continue") {
		r = lay.cue;
		return wait == Wait::page || wait == Wait::cont;
	}
	if (part == "face.0" || part == "face.1") {
		const int i = part[5] - '0';
		r           = lay.cards[i];
		return lay.card_shown[i];
	}
	if (part.compare(0, 7, "answer.") == 0 && choosing) {
		const int i = std::atoi(part.c_str() + 7);
		if (i < 0 || i >= int(answers.size())) {
			return false;
		}
		set_focus(i);    // Scroll it into view.
		if (dirty) {
			paint();
		}
		r = answer_rect(i);
		return true;
	}
	return false;
}

bool Mui_dialogue::to_layer(int sx, int sy, int& lx, int& ly) const {
	return layer >= 0 && iwin()->screen_to_layer(layer, sx, sy, lx, ly);
}

/*
 *  Wait for input, modelled on the static Get_click loop in exult.cc:
 *  palette rotation, paused timer queue, focus events and the Ctrl+Alt+S
 *  screenshot. Continue: left or right click, Space, Enter, Escape or any
 *  printable key. Choice: the keys 1-9, a-z, arrows/Tab and Enter/Space,
 *  a click on an answer; Escape or right click return -2. Wheel up over
 *  the panel, PgUp, Home, H or Up (not choosing) open the history; while
 *  it is open no input continues or chooses.
 */
int Mui_dialogue::wait_input(Wait mode) {
	Game_window*              gwin      = Game_window::get_instance();
	Image_window*             w         = gwin->get_win();
	const Mouse::Mouse_shapes saveshape = Mouse::mouse()->get_shape();
	Mouse::mouse()->set_shape(Mouse::hand);
	wait  = mode;
	hover = -1;
	if (mode == Wait::choice) {
		set_focus(focus);
	}
	dirty = true;
	paint();
	Mouse::mouse()->show();
	gwin->show(true);
	gwin->get_tqueue()->pause(Game::get_ticks());
	g_waiting_for_click       = true;
	SDL_Renderer* renderer    = SDL_GetRenderer(w->get_screen_window());
	uint32        last_rotate = 0;
	float         last_mx     = -1;
	float         last_my     = -1;
	bool          rightclick  = false;
	bool          bar_press   = false;    // Left button went down on the history scrollbar.
	int           result      = -1;
	bool          done        = false;
	auto          hover_at    = [&](int sx, int sy) {
        int lx;
        int ly;
        if (mode == Wait::choice && !hist_open && to_layer(sx, sy, lx, ly)) {
            const int h = answer_at(lx, ly);
            if (h != hover) {
                hover = h;
                dirty = true;
            }
        }
	};
	while (!done) {
		Delay();
		const uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		Mui_selftest::on_dialogue_frame();
		Mouse::mouse()->hide();
		Mouse::mouse_update = false;
		const int rot_speed = 100 << (w->fast_palette_rotate() ? 0 : 1);
		if (ticks > last_rotate + rot_speed) {
			w->rotate_colors(0xfc, 3, 0);
			w->rotate_colors(0xf8, 4, 0);
			w->rotate_colors(0xf4, 4, 0);
			w->rotate_colors(0xf0, 4, 0);
			w->rotate_colors(0xe8, 8, 0);
			w->rotate_colors(0xe0, 8, 1);
			while (ticks > last_rotate + rot_speed) {
				last_rotate += rot_speed;
			}
			if (!w->is_palettized()) {
				gwin->set_painted();
			}
		}
		// Hover follows the real mouse when it moves.
		if (!Mouse::use_touch_input && renderer) {
			float fx;
			float fy;
			float rx;
			float ry;
			SDL_GetMouseState(&fx, &fy);
			if ((fx != last_mx || fy != last_my) && SDL_RenderCoordinatesFromWindow(renderer, fx, fy, &rx, &ry)) {
				last_mx = fx;
				last_my = fy;
				hover_at(int(rx), int(ry));
			}
		}
		SDL_Event event;
		while (!done && SDL_PollEvent(&event)) {
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			switch (event.type) {
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				if (event.button.button == 3) {
					rightclick = true;
				}
				int lx;
				int ly;
				if (hist_open && event.button.button == 1 && to_layer(int(event.button.x), int(event.button.y), lx, ly)) {
					bar_press = hl.track.has_point(lx, ly);
					if (hl.thumb.has_point(lx, ly)) {
						hist_drag = true;
						hist_grab = ly - hl.thumb.y;
						dirty     = true;
					} else if (bar_press) {    // Track: a page towards the click.
						const int page = std::max(hl.line_h, hl.view.h - hl.line_h);
						hist_scroll_to(hist_scroll + (ly < hl.thumb.y ? -page : page));
					}
				}
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				if (hist_open) {
					// A click outside the scrollbar closes the history.
					if (event.button.button == 3) {
						rightclick = false;
						close_history();
					} else if (event.button.button == 1) {
						if (!hist_drag && !bar_press) {
							close_history();
						}
						hist_drag = false;
						bar_press = false;
						dirty     = true;
					}
					break;
				}
				if (event.button.button == 3) {
					if (rightclick) {
						rightclick = false;
						result     = -2;
						done       = true;
					}
					break;
				}
				if (event.button.button != 1) {
					break;
				}
				if (mode != Wait::choice) {
					done = true;
					break;
				}
				int lx;
				int ly;
				if (to_layer(int(event.button.x), int(event.button.y), lx, ly)) {
					const int i = answer_at(lx, ly);
					if (i >= 0) {
						result = i;
						done   = true;
					}
				}
				break;
			}
			case SDL_EVENT_MOUSE_MOTION: {
				int mx;
				int my;
				w->screen_to_game(int(event.motion.x), int(event.motion.y), gwin->get_fastmouse(), mx, my);
				Mouse::mouse()->move(mx, my);
				Mouse::mouse_update = true;
				hover_at(int(event.motion.x), int(event.motion.y));
				int lx;
				int ly;
				if (hist_drag && to_layer(int(event.motion.x), int(event.motion.y), lx, ly)) {
					const int range = hl.track.h - hl.thumb.h;
					if (range > 0) {
						hist_scroll_to((ly - hist_grab - hl.track.y) * hist_max_scroll() / range);
					}
				}
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL: {
				int lx;
				int ly;
				if (!to_layer(int(event.wheel.mouse_x), int(event.wheel.mouse_y), lx, ly)) {
					break;
				}
				const float dy = event.wheel.y;
				if (hist_open) {
					const int lines = dy > 0 ? -3 : dy < 0 ? 3 : 0;
					hist_scroll_to(hist_scroll + lines * hl.line_h);
				} else if (mode == Wait::choice && lay.answers.has_point(lx, ly) && (lay.bar_w > 0 || dy < 0)) {    // Scrollable
																													// answers.
					scroll_by(-int(dy) * std::max(lay.btn_h, lay.answers.h / 4));
				} else if (dy > 0 && lay.panel.has_point(lx, ly)) {
					open_history();
				}
				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				const SDL_Keycode c   = event.key.key;
				const SDL_Keymod  mod = event.key.mod;
				switch (c) {
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
				case SDLK_RCTRL:
				case SDLK_LCTRL:
				case SDLK_RALT:
				case SDLK_LALT:
				case SDLK_RGUI:
				case SDLK_LGUI:
				case SDLK_NUMLOCKCLEAR:
				case SDLK_CAPSLOCK:
				case SDLK_SCROLLLOCK:
					continue;
				default:
					break;
				}
				if (c == SDLK_S && (mod & SDL_KMOD_ALT) && (mod & SDL_KMOD_CTRL)) {
					make_screenshot(true);
					break;
				}
				if (hist_open) {
					hist_key(int(c));
					break;
				}
				// History keys: PgUp, Home, H (unless it picks an answer)
				// and Up while not choosing.
				const bool h_answers
						= mode == Wait::choice && int(std::strchr(option_keys, 'h') - option_keys) < int(answers.size());
				if (c == SDLK_PAGEUP || c == SDLK_HOME || (c == SDLK_UP && mode != Wait::choice) || (c == SDLK_H && !h_answers)) {
					open_history();
					break;
				}
				if (mode == Wait::choice) {
					const int n    = int(answers.size());
					const int rows = std::max(1, lay.rows);
					switch (c) {
					case SDLK_ESCAPE:
						result = -2;
						done   = true;
						break;
					case SDLK_UP:
						set_focus(focus - 1);
						break;
					case SDLK_DOWN:
						set_focus(focus + 1);
						break;
					case SDLK_LEFT:
						if (focus - rows >= 0) {
							set_focus(focus - rows);
						}
						break;
					case SDLK_RIGHT:
						if (focus + rows < n) {
							set_focus(focus + rows);
						}
						break;
					case SDLK_TAB:
						set_focus((focus + ((mod & SDL_KMOD_SHIFT) ? n - 1 : 1)) % std::max(1, n));
						break;
					case SDLK_RETURN:
					case SDLK_KP_ENTER:
					case SDLK_SPACE:
						if (focus >= 0 && focus < n) {
							result = focus;
							done   = true;
						}
						break;
					default:
						if (!(mod & SDL_KMOD_SHIFT) && c < 128) {
							const char* k = std::strchr(option_keys, int(c));
							if (c != 0 && k && int(k - option_keys) < n) {
								result = int(k - option_keys);
								done   = true;
							}
						}
						break;
					}
					break;
				}
				// Continue.
				if (c == SDLK_ESCAPE || c == SDLK_RETURN || c == SDLK_KP_ENTER || (c >= ' ' && c < 127)) {
					done = true;
				}
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				gwin->get_focus();
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				gwin->lose_focus();
				break;
			default:
				break;
			}
		}
		if (dirty && !done) {
			paint();
		}
		Mouse::mouse()->show();
		if (!gwin->show() && Mouse::mouse_update) {
			Mouse::mouse()->blit_dirty();
		}
	}
	g_waiting_for_click = false;
	gwin->get_tqueue()->resume(Game::get_ticks());
	Mouse::mouse()->set_shape(saveshape);
	wait  = Wait::none;
	hover = -1;
	dirty = true;
	paint();
	return result;
}
