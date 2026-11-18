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

#include "Mui_config_editor.h"

#include "Configuration.h"
#include "Face_stats.h"
#include "Gump_manager.h"
#include "Modal_gump.h"
#include "Mui_config_meta.h"
#include "Mui_theme.h"
#include "Mui_window.h"
#include "ShortcutBar_gump.h"
#include "cheat.h"
#include "combat_opts.h"
#include "exult.h"
#include "gamewin.h"
#include "items.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Intro() {
			return get_text_msg(0xC39 - msg_file_start);
		}

		static auto Title() {
			return get_text_msg(0xC2D - msg_file_start);
		}

		static auto AppliesNow() {
			return get_text_msg(0xC28 - msg_file_start);
		}

		static auto NeedsRestart() {
			return get_text_msg(0xC29 - msg_file_start);
		}

		static auto Filter() {
			return get_text_msg(0xC80 - msg_file_start);
		}

		static auto Other() {
			return get_text_msg(0xC81 - msg_file_start);
		}

		static auto NotSet() {
			return get_text_msg(0xC82 - msg_file_start);
		}

		static auto ResetDefault() {
			return get_text_msg(0xC83 - msg_file_start);
		}

		static auto ChangedDefault() {
			return get_text_msg(0xC84 - msg_file_start);
		}

		static auto Keys() {
			return get_text_msg(0xC85 - msg_file_start);
		}

		static auto Key() {
			return get_text_msg(0xC95 - msg_file_start);
		}

		static auto NoKeys() {
			return get_text_msg(0xC86 - msg_file_start);
		}

		static auto Default() {
			return get_text_msg(0xC8D - msg_file_start);
		}

		static auto Unknown() {
			return get_text_msg(0xC8E - msg_file_start);
		}

		static auto SectionsHelp() {
			return get_text_msg(0xC8F - msg_file_start);
		}

		static auto FilterHelp() {
			return get_text_msg(0xC90 - msg_file_start);
		}

		static auto ResetHelp() {
			return get_text_msg(0xC91 - msg_file_start);
		}

		static auto FixErrors() {
			return get_text_msg(0xC92 - msg_file_start);
		}

		static auto Empty() {
			return get_text_msg(0xC93 - msg_file_start);
		}

		static auto OnlySubsections() {
			return get_text_msg(0xC94 - msg_file_start);
		}
	};

	const char* const other_node = "#other";
	const char* const root       = "config";

	// Reference widths of the two columns and of the controls.
	constexpr float tree_ref_w    = 250;
	constexpr float control_ref_w = 260;

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	bool is_yes(const std::string& s) {
		return lower(s) == "yes";
	}

	std::string parent_of(const std::string& path) {
		const size_t slash = path.rfind('/');
		return slash == std::string::npos ? std::string() : path.substr(0, slash);
	}

	std::string last_segment(const std::string& path) {
		const size_t slash = path.rfind('/');
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}

	int depth_of(const std::string& path) {
		return int(std::count(path.begin(), path.end(), '/'));
	}

	// Legacy keys are listed only when they are present.
	bool is_legacy(const Mui_config_key& k) {
		return k.description.rfind("Old ", 0) == 0;
	}

	// Tree on the left (fixed width), keys on the right (the rest), each
	// measured at the width it gets. The tree stays at the top of the
	// visible part of the page while the keys scroll.
	class Columns : public Mui_box {
	public:
		Columns() : Mui_box(Dir::horizontal) {}

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			int lw = 0;
			int rw = 0;
			widths(p, avail_w, lw, rw);
			w = avail_w;
			h = 0;
			for (size_t i = 0; i < children.size() && i < 2; i++) {
				int cw = 0;
				int ch = 0;
				children[i]->measure(p, i == 0 ? lw : rw, cw, ch);
				h = std::max(h, ch);
			}
		}

		void layout(Mui_painter& p) override {
			int lw = 0;
			int rw = 0;
			widths(p, rect.w, lw, rw);
			int x = rect.x;
			for (size_t i = 0; i < children.size() && i < 2; i++) {
				const int cwant = i == 0 ? lw : rw;
				int       cw    = 0;
				int       ch    = 0;
				children[i]->measure(p, cwant, cw, ch);
				children[i]->rect = TileRect(x, rect.y, cwant, ch);
				children[i]->layout(p);
				x += cwant + gap_px(p);
			}
		}

		void paint(Mui_painter& p) override {
			if (!children.empty()) {
				Mui_widget*    tree = children[0].get();
				const TileRect clip = p.get_clip();
				const int      top  = std::max(rect.y, std::min(clip.y, rect.y + rect.h - tree->rect.h));
				const int      dy   = top - tree->rect.y;
				if (dy != 0) {
					std::vector<Mui_widget*> all;
					tree->collect(all);
					for (auto* w : all) {
						w->rect.y += dy;
					}
				}
			}
			Mui_box::paint(p);
		}

	private:
		void widths(Mui_painter& p, int avail_w, int& lw, int& rw) const {
			lw = std::min(p.px(tree_ref_w), avail_w / 3);
			rw = std::max(1, avail_w - lw - gap_px(p));
		}
	};

	// A wrapped small line.
	class Note : public Mui_label {
	public:
		explicit Note(std::string text_, Mui_colour c = Mui_colour::text_dim)
				: Mui_label(std::move(text_), Mui_font_role::small, c, true) {}
	};

	class Key_dropdown : public Mui_dropdown {
	public:
		using Mui_dropdown::Mui_dropdown;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_dropdown::measure(p, avail_w, w, h);
			w = std::min(std::max(w, p.px(control_ref_w)), avail_w);
		}
	};

	class Key_text_field : public Mui_text_field {
	public:
		using Mui_text_field::Mui_text_field;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_text_field::measure(p, avail_w, w, h);
			w = std::min(p.px(control_ref_w), avail_w);
		}
	};

	// Slider that reports the end of a drag.
	class Key_slider : public Mui_slider {
	public:
		using Mui_slider::Mui_slider;
		std::function<void()> on_release;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_slider::measure(p, avail_w, w, h);
			w = std::min(p.px(control_ref_w), avail_w);
		}

		bool on_mouse_up(int x, int y, int button, bool inside) override {
			const bool r = Mui_slider::on_mouse_up(x, y, button, inside);
			if (on_release) {
				auto cb = on_release;
				cb();
			}
			return r;
		}
	};
}    // namespace

Mui_config_editor::Mui_config_editor(Mui_window& w, std::function<void(const std::string&)> rebuild_)
		: win(w), rebuild(std::move(rebuild_)) {
	reload();
	select_section("config/gameplay");
}

/*
 *  Model.
 */

void Mui_config_editor::reload() {
	const Mui_config_meta& meta = Mui_config_meta::get();
	// Every key under <config> with its value; leaves are keys that no
	// other key continues.
	Configuration::KeyTypeList ktl;
	config->getsubkeys(ktl, root);
	std::map<std::string, std::string> live;
	std::set<std::string>              inner;
	for (const auto& kv : ktl) {
		const std::string path = std::string(root) + "/" + kv.first;
		if (path.find("/!") != std::string::npos) {
			continue;    // XML comments.
		}
		live.emplace(path, kv.second);
		for (std::string p = parent_of(path); !p.empty(); p = parent_of(p)) {
			inner.insert(p);
		}
	}
	entries.clear();
	std::set<std::string> listed;
	for (const auto& kv : live) {
		if (inner.count(kv.first)) {
			continue;
		}
		Entry e;
		e.path    = kv.first;
		e.meta    = meta.find(e.path);
		e.present = true;
		e.live    = kv.second;
		entries.push_back(std::move(e));
		listed.insert(kv.first);
	}
	// Known keys that are not set, shown with their default.
	for (const auto& k : meta.keys()) {
		if (k.is_pattern() || listed.count(k.path) || inner.count(k.path) || is_legacy(k)) {
			continue;
		}
		Entry e;
		e.path = k.path;
		e.meta = &k;
		entries.push_back(std::move(e));
		listed.insert(k.path);
	}
	std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
		return a.path < b.path;
	});
	// Sections from the paths of known keys; unknown keys go to "Other".
	std::map<std::string, int> counts;
	std::set<std::string>      sections;
	int                        others = 0;
	for (auto& e : entries) {
		if (!e.meta) {
			e.section = other_node;
			others++;
			continue;
		}
		e.section = parent_of(e.path);
		counts[e.section]++;
		for (std::string p = e.section; depth_of(p) >= 1; p = parent_of(p)) {
			sections.insert(p);
		}
	}
	nodes.clear();
	for (const auto& s : sections) {
		Node n;
		n.path         = s;
		n.label        = last_segment(s);
		n.depth        = depth_of(s);
		n.keys         = counts.count(s) ? counts[s] : 0;
		auto it        = sections.upper_bound(s);
		n.has_children = it != sections.end() && it->rfind(s + "/", 0) == 0;
		nodes.push_back(std::move(n));
	}
	if (others) {
		Node n;
		n.path  = other_node;
		n.label = Strings::Other();
		n.keys  = others;
		nodes.push_back(std::move(n));
	}
	// Drop edits of keys that went away, and edits that match again.
	for (auto it = pending.begin(); it != pending.end();) {
		auto e = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) {
			return x.path == it->first;
		});
		if (e == entries.end()) {
			it = pending.erase(it);
		} else {
			++it;
		}
	}
	if (std::none_of(nodes.begin(), nodes.end(), [this](const Node& n) {
			return n.path == section;
		})) {
		section = nodes.empty() ? std::string() : nodes.front().path;
	}
}

std::string Mui_config_editor::value_of(const Entry& e) const {
	auto it = pending.find(e.path);
	if (it != pending.end()) {
		return it->second;
	}
	if (e.present) {
		return e.live;
	}
	return e.meta ? e.meta->def : std::string();
}

std::string Mui_config_editor::description(const Entry& e) const {
	if (!e.meta) {
		return Strings::Unknown();
	}
	std::string d = e.meta->description;
	if (!e.meta->def.empty()) {
		d += std::string("  ") + Strings::Default() + " " + e.meta->def;
	}
	return d;
}

void Mui_config_editor::set_pending(const Entry& e, const std::string& value, bool now) {
	const bool same = e.present ? value == e.live : (e.meta && value == e.meta->def);
	if (same) {
		pending.erase(e.path);
	} else {
		pending[e.path] = value;
	}
	errors.erase(e.path);
	if (now) {
		rebuild(control_id(e.path));
	}
}

void Mui_config_editor::revert() {
	pending.clear();
	errors.clear();
}

bool Mui_config_editor::apply(bool& wrote) {
	wrote = false;
	errors.clear();
	for (const auto& kv : pending) {
		auto e = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) {
			return x.path == kv.first;
		});
		if (e != entries.end() && e->meta) {
			const std::string why = e->meta->validate(kv.second);
			if (!why.empty()) {
				errors[kv.first] = why;
			}
		}
	}
	if (!errors.empty()) {
		// Show the first bad key.
		reveal(errors.begin()->first);
		return false;
	}
	std::vector<std::pair<std::string, std::string>> now;
	for (const auto& kv : pending) {
		config->set(kv.first, kv.second, false);
		wrote                   = true;
		const Mui_config_key* k = Mui_config_meta::get().find(kv.first);
		if (k && k->apply == Mui_config_key::Apply::now) {
			now.emplace_back(kv.first, kv.second);
		}
	}
	if (wrote) {
		config->write_back();
	}
	for (const auto& kv : now) {
		apply_now(kv.first, kv.second);
	}
	pending.clear();
	reload();
	return true;
}

// The engine side of keys marked "now", as the classic option screens do.
void Mui_config_editor::apply_now(const std::string& path, const std::string& value) {
	const std::string v   = lower(value);
	const bool        yes = is_yes(value);
	const int         n   = std::atoi(value.c_str());
	const std::string key = path.substr(std::string("config/").size());
	if (key == "video/fps") {
		gwin->set_std_delay(1000 / (n > 0 ? n : 5));
	} else if (key == "gameplay/menu_style") {
		if (v != "classic" && Mui_theme::load(value)) {
			win.switch_theme(value);
		}
	} else if (key == "gameplay/cheat") {
		cheat.set_enabled(yes);
	} else if (key == "gameplay/feeding") {
		cheat.SetFoodUse(
				v == "automatic"  ? Cheat::FoodUse::Automatic
				: v == "disabled" ? Cheat::FoodUse::Disabled
								  : Cheat::FoodUse::Manual,
				false);
	} else if (key == "gameplay/extended_intro") {
		gwin->set_extended_intro(yes);
	} else if (key == "gameplay/facestats") {
		Face_stats::load_config(config);
	} else if (key == "gameplay/textbackground") {
		gwin->set_text_bg(n);
	} else if (key == "gameplay/smooth_scrolling") {
		gwin->set_lerping_enabled(n);
	} else if (key == "gameplay/alternate_drop") {
		gwin->set_alternate_drop(yes);
	} else if (key == "gameplay/allow_autonotes") {
		gwin->set_allow_autonotes(yes);
	} else if (key == "gameplay/enhancements") {
		gwin->set_allow_enhancements(yes);
	} else if (key == "gameplay/gumps_dont_pause_game") {
		gumpman->set_gumps_dont_pause_game(yes);
	} else if (key == "gameplay/modal_gump_dragging") {
		Modal_gump::SetDragType(
				v == "always"  ? Modal_gump::DragType::Always
				: v == "never" ? Modal_gump::DragType::Never
							   : Modal_gump::DragType::Offscreen);
	} else if (key == "gameplay/double_click_closes_gumps") {
		gwin->set_double_click_closes_gumps(yes);
	} else if (key == "gameplay/right_click_closes_gumps") {
		gumpman->set_right_click_close(v != "no");
	} else if (key == "gameplay/allow_right_pathfind") {
		gwin->set_allow_right_pathfind(v == "no" ? 0 : v == "single" ? 1 : 2);
	} else if (key == "gameplay/scroll_with_mouse") {
		gwin->set_mouse_with_scroll(yes);
	} else if (key == "gameplay/mouse3rd") {
		gwin->set_mouse3rd(yes);
	} else if (key == "gameplay/fastmouse") {
		gwin->set_fastmouse(yes);
	} else if (key == "gameplay/combat/difficulty") {
		Combat::difficulty = n;
	} else if (key == "gameplay/combat/charmDifficulty") {
		Combat::charmed_more_difficult = v == "hard";
	} else if (key == "gameplay/combat/mode") {
		Combat::mode = v == "keypause" ? Combat::keypause : Combat::original;
	} else if (key == "gameplay/combat/show_hits") {
		Combat::show_hits = yes;
	} else if (key == "shortcutbar/hide_missing_items") {
		gwin->set_sb_hide_missing_items(v != "no");
		if (g_shortcutBar) {
			g_shortcutBar->set_changed();
		}
	} else if (key == "touch/item_menu") {
		gwin->set_item_menu(yes);
	} else if (key == "touch/touch_pathfind") {
		gwin->set_touch_pathfind(yes);
	}
	// Others (audio/disablepause, audio/effects/midi) are read when used.
	gwin->set_all_dirty();
}

std::vector<const Mui_config_editor::Entry*> Mui_config_editor::search(const std::string& needle) const {
	std::vector<const Entry*> out;
	if (needle.empty()) {
		return out;
	}
	for (const auto& e : entries) {
		if (lower(e.path).find(needle) != std::string::npos
			|| (e.meta && lower(e.meta->description).find(needle) != std::string::npos)) {
			out.push_back(&e);
		}
	}
	return out;
}

void Mui_config_editor::select_section(const std::string& path) {
	section = path;
	// Open the node and every node above it.
	for (std::string p = path; depth_of(p) >= 1; p = parent_of(p)) {
		expanded.insert(p);
	}
}

void Mui_config_editor::reveal(const std::string& path) {
	auto e = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) {
		return x.path == path;
	});
	if (e == entries.end()) {
		return;
	}
	filter.clear();
	select_section(e->section);
	highlight = path;
}

std::vector<const Mui_config_editor::Node*> Mui_config_editor::visible_nodes() const {
	std::vector<const Node*> out;
	for (const auto& n : nodes) {
		bool shown = true;
		if (n.path != other_node) {
			for (std::string p = parent_of(n.path); depth_of(p) >= 1; p = parent_of(p)) {
				if (!expanded.count(p)) {
					shown = false;
					break;
				}
			}
		}
		if (shown) {
			out.push_back(&n);
		}
	}
	return out;
}

/*
 *  Widgets.
 */

void Mui_config_editor::build(Mui_box& body) {
	body.gap = 6;
	body.add<Mui_section_header>(Strings::Title());
	body.add<Note>(Strings::Intro())->id = "adv.intro";
	auto* bar                            = body.add<Mui_box>(Mui_box::Dir::horizontal);
	auto* field                          = bar->add<Key_text_field>("adv.filter", filter, Strings::Filter());
	field->search_icon                   = true;
	field->help                          = Strings::FilterHelp();
	field->on_change                     = [this](const std::string& s) {
        win.post([this, s] {
            filter = s;
            highlight.clear();
            rebuild("adv.filter");
        });
	};
	int shown = 0;
	if (filter.empty()) {
		shown = int(std::count_if(entries.begin(), entries.end(), [this](const Entry& e) {
			return e.section == section;
		}));
	} else {
		shown = int(search(lower(filter)).size());
	}
	auto* count = bar->add<Mui_label>(
			std::to_string(shown) + " " + (shown == 1 ? Strings::Key() : Strings::Keys()), Mui_font_role::body,
			Mui_colour::text_dim);
	count->id      = "adv.count";
	count->stretch = true;
	if (!errors.empty()) {
		body.add<Note>(Strings::FixErrors(), Mui_colour::danger)->id = "adv.errors";
	}
	auto* cols = body.add<Columns>();
	auto* left = cols->add<Mui_box>(Mui_box::Dir::vertical);
	build_tree(*left);
	auto* right = cols->add<Mui_box>(Mui_box::Dir::vertical);
	right->gap  = 4;
	build_keys(*right);
}

void Mui_config_editor::build_tree(Mui_box& col) {
	const auto               shown = visible_nodes();
	std::vector<std::string> labels;
	int                      sel = -1;
	for (size_t i = 0; i < shown.size(); i++) {
		labels.push_back(shown[i]->label);
		if (filter.empty() && shown[i]->path == section) {
			sel = int(i);
		}
	}
	auto* list         = col.add<Mui_list>("adv.sections", labels, sel);
	list->visible_rows = std::clamp(int(labels.size()), 4, 16);
	list->row_ref      = 30;
	list->help         = Strings::SectionsHelp();
	std::vector<const Node*> nodes_shown(shown.begin(), shown.end());
	list->paint_row = [this, nodes_shown](Mui_painter& p, int i, const TileRect& r, bool sel) {
		const Node& n    = *nodes_shown[size_t(i)];
		const uint8 fill = sel ? p.col(Mui_colour::accent) : p.col(Mui_colour::panel);
		const uint8 ink  = p.col(sel ? Mui_colour::text_on_accent : Mui_colour::text);
		const uint8 dim  = p.col(sel ? Mui_colour::text_on_accent : Mui_colour::text_dim);
		const int   unit = p.px(8);
		int         x    = r.x + unit + (n.depth - 1) * p.px(16);
		const int   is   = r.h / 3;
		if (n.has_children) {
			const bool open = expanded.count(n.path) != 0;
			p.icon(open ? Mui_icon::chevron_down : Mui_icon::arrow_right, TileRect(x, r.y + (r.h - is) / 2, is, is), dim);
		}
		x += is + unit / 2;
		const std::string count = n.keys ? std::to_string(n.keys) : std::string();
		const int         cw    = count.empty() ? 0 : p.text_width(count, Mui_font_role::small) + unit;
		p.text_in_idx(n.label, TileRect(x, r.y, r.x + r.w - x - cw, r.h), Mui_font_role::body, ink, fill, Mui_align::left);
		if (!count.empty()) {
			p.text_in_idx(
					count, TileRect(r.x + r.w - cw - unit / 2, r.y, cw, r.h), Mui_font_role::small, dim, fill, Mui_align::right);
		}
	};
	list->on_select = [this, nodes_shown](int i) {
		const std::string path = nodes_shown[size_t(i)]->path;
		win.post([this, path] {
			filter.clear();
			highlight.clear();
			select_section(path);
			rebuild("adv.sections");
		});
	};
	// Choosing the selected node again opens or closes it.
	list->on_activate = [this, nodes_shown](int i) {
		const std::string path = nodes_shown[size_t(i)]->path;
		win.post([this, path] {
			if (expanded.count(path) && section == path) {
				expanded.erase(path);
			} else {
				select_section(path);
			}
			filter.clear();
			rebuild("adv.sections");
		});
	};
}

void Mui_config_editor::build_keys(Mui_box& col) {
	if (filter.empty()) {
		const std::string title                               = section == other_node ? std::string(Strings::Other()) : section;
		col.add<Mui_label>(title, Mui_font_role::heading)->id = "adv.section";
		if (section == other_node) {
			col.add<Note>(Strings::Unknown());
		}
		int n = 0;
		for (const auto& e : entries) {
			if (e.section == section) {
				build_row(col, e);
				n++;
			}
		}
		if (n == 0) {
			col.add<Note>(Strings::OnlySubsections());
		}
		return;
	}
	const auto found = search(lower(filter));
	if (found.empty()) {
		col.add<Note>(Strings::NoKeys())->id = "adv.nokeys";
		return;
	}
	std::string last;
	for (const Entry* e : found) {
		if (e->section != last) {
			last = e->section;
			col.add<Mui_label>(last == other_node ? std::string(Strings::Other()) : last, Mui_font_role::heading);
		}
		build_row(col, *e);
	}
}

void Mui_config_editor::build_row(Mui_box& col, const Entry& e) {
	const std::string cid   = control_id(e.path);
	const std::string value = value_of(e);
	auto*             row   = col.add<Mui_box>(Mui_box::Dir::vertical, "adv.row." + e.path);
	row->gap                = 2;
	row->pad                = 4;
	if (highlight == e.path) {
		row->panel = true;
		row->bg    = Mui_colour::panel_alt;
	}
	// Name and badges, then the control and Reset, then path and text.
	auto* line = row->add<Mui_box>(Mui_box::Dir::horizontal);
	auto* name
			= line->add<Mui_label>(last_segment(e.path), Mui_font_role::body, e.present ? Mui_colour::text : Mui_colour::text_dim);
	name->stretch          = true;
	name->id               = cid + ".name";
	const std::string help = description(e);
	name->help             = help;
	if (!e.present && !pending.count(e.path)) {
		line->add<Mui_label>(Strings::NotSet(), Mui_font_role::small, Mui_colour::text_dim)->id = cid + ".notset";
	}
	if (e.meta) {
		if (!e.meta->def.empty() && !e.meta->is_default(value)) {
			line->add<Mui_badge>(Mui_badge::Kind::changed, Strings::ChangedDefault())->id = cid + ".changed";
		}
		if (e.meta->apply == Mui_config_key::Apply::now) {
			line->add<Mui_badge>(Mui_badge::Kind::now, Strings::AppliesNow())->id = cid + ".now";
		} else {
			line->add<Mui_badge>(Mui_badge::Kind::restart, Strings::NeedsRestart())->id = cid + ".restart";
		}
	}
	auto* edit = row->add<Mui_box>(Mui_box::Dir::horizontal);
	add_control(*edit, e);
	if (e.meta) {
		const std::string path = e.path;
		auto* reset    = edit->add<Mui_button>(cid + ".reset", Strings::ResetDefault(), Mui_button::Style::flat, [this, path] {
            win.post([this, path] {
                auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) {
                    return x.path == path;
                });
                if (it != entries.end() && it->meta) {
                    set_pending(*it, it->meta->def, true);
                }
            });
        });
		reset->help    = Strings::ResetHelp();
		reset->enabled = !e.meta->def.empty() && !e.meta->is_default(value);
	}
	edit->add<Mui_spacer>(0, true);
	row->add<Mui_label>(e.path, Mui_font_role::mono, Mui_colour::text_dim)->id = cid + ".path";
	row->add<Note>(help)->id                                                   = cid + ".desc";
	auto err                                                                   = errors.find(e.path);
	if (err != errors.end()) {
		row->add<Note>(err->second, Mui_colour::danger)->id = cid + ".error";
	}
}

void Mui_config_editor::add_control(Mui_box& line, const Entry& e) {
	using Type              = Mui_config_key::Type;
	const std::string cid   = control_id(e.path);
	const std::string value = value_of(e);
	const std::string path  = e.path;
	const std::string help  = description(e);
	// Look the entry up again when a callback runs (entries may reload).
	auto with_entry = [this, path](const std::function<void(const Entry&)>& fn) {
		auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) {
			return x.path == path;
		});
		if (it != entries.end()) {
			fn(*it);
		}
	};
	const Type  type    = e.meta ? e.meta->type : Type::string;
	Mui_widget* control = nullptr;
	if (type == Type::boolean) {
		control = line.add<Mui_toggle>(cid, is_yes(value), [this, with_entry](bool v) {
			win.post([this, with_entry, v] {
				with_entry([this, v](const Entry& x) {
					set_pending(x, v ? "yes" : "no", true);
				});
			});
		});
	} else if (type == Type::enumeration) {
		std::vector<std::string> values = e.meta->choices;
		int                      index  = e.meta->choice_index(value);
		if (index < 0) {
			// Keep a value the table does not list (e.g. a mod theme).
			values.push_back(value);
			index = int(values.size()) - 1;
		}
		std::vector<std::string> labels;
		for (const auto& c : values) {
			labels.push_back(c.empty() ? std::string(Strings::Empty()) : c);
		}
		control = line.add<Key_dropdown>(cid, labels, index, [this, with_entry, values](int i) {
			const std::string v = values[size_t(i)];
			win.post([this, with_entry, v] {
				with_entry([this, v](const Entry& x) {
					set_pending(x, v, true);
				});
			});
		});
	} else if (type == Type::integer && e.meta->has_range && e.meta->maxv - e.meta->minv <= 200) {
		const int minv = int(e.meta->minv);
		const int maxv = int(e.meta->maxv);
		auto*     s    = line.add<Key_slider>(cid, minv, maxv, 1, std::clamp(std::atoi(value.c_str()), minv, maxv));
		s->on_change   = [this, with_entry, s](int v) {
            // While dragging, rebuild only when the knob is released.
            const bool dragging = win.get_pressed() == s;
            win.post([this, with_entry, v, dragging] {
                with_entry([this, v, dragging](const Entry& x) {
                    set_pending(x, std::to_string(v), !dragging);
                });
            });
		};
		s->on_release = [this, cid] {
			win.post([this, cid] {
				rebuild(cid);
			});
		};
		control = s;
	} else if (type == Type::integer) {
		const int minv = e.meta->has_range ? int(e.meta->minv) : -999999;
		const int maxv = e.meta->has_range ? int(e.meta->maxv) : 999999;
		control        = line.add<Mui_number_field>(
                cid, minv, maxv, 1, std::clamp(std::atoi(value.c_str()), minv, maxv), [this, with_entry](int v) {
                    win.post([this, with_entry, v] {
                        with_entry([this, v](const Entry& x) {
                            set_pending(x, std::to_string(v), true);
                        });
                    });
                });
	} else {
		// Paths, text, numbers with a fraction and unknown keys.
		auto* f      = line.add<Key_text_field>(cid, value);
		f->max_len   = 1024;
		f->on_change = [this, with_entry](const std::string& v) {
			win.post([this, with_entry, v] {
				with_entry([this, v](const Entry& x) {
					set_pending(x, v, true);
				});
			});
		};
		control = f;
	}
	control->help = help;
}
