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

#include "Mui_options.h"

#include "Mui_router.h"
#include "gamewin.h"
#include "items.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Options() {
			return get_text_msg(0xC13 - msg_file_start);
		}

		static auto Tab(int i) {
			return get_text_msg(unsigned(0xC20 + i) - unsigned(msg_file_start));
		}

		static auto SearchPlaceholder() {
			return get_text_msg(0xC27 - msg_file_start);
		}

		static auto AppliesNow() {
			return get_text_msg(0xC28 - msg_file_start);
		}

		static auto NeedsRestart() {
			return get_text_msg(0xC29 - msg_file_start);
		}

		static auto Changed() {
			return get_text_msg(0xC2A - msg_file_start);
		}

		static auto ResetPage() {
			return get_text_msg(0xC2B - msg_file_start);
		}

		static auto Apply() {
			return get_text_msg(0xC2C - msg_file_start);
		}

		static auto NoMatches() {
			return get_text_msg(0xC2E - msg_file_start);
		}

		static auto SettingsFound() {
			return get_text_msg(0xC2F - msg_file_start);
		}

		static auto SettingFound() {
			return get_text_msg(0xD21 - msg_file_start);
		}

		static auto Open() {
			return get_text_msg(0xC30 - msg_file_start);
		}

		static auto DefaultHelp() {
			return get_text_msg(0xC33 - msg_file_start);
		}

		static auto ResetHelp() {
			return get_text_msg(0xC34 - msg_file_start);
		}

		static auto CancelHelp() {
			return get_text_msg(0xC35 - msg_file_start);
		}

		static auto ApplyHelp() {
			return get_text_msg(0xC36 - msg_file_start);
		}

		static auto SearchHelp() {
			return get_text_msg(0xC37 - msg_file_start);
		}

		static auto ClearHelp() {
			return get_text_msg(0xC38 - msg_file_start);
		}

		static auto JumpHelp() {
			return get_text_msg(0xC3A - msg_file_start);
		}

		static auto Cancel() {
			return get_text_msg(0xC0A - msg_file_start);
		}
	};

	constexpr int num_model_tabs = int(Mui_options_model::Tab::count);
	constexpr int advanced_tab   = num_model_tabs;    // The exult.cfg editor.

	// Remembered for the session.
	int last_tab = 0;

	const char* tab_key(int tab) {
		return tab == advanced_tab ? "advanced" : Mui_options_model::tab_key(Mui_options_model::Tab(tab));
	}

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	// Horizontal indent for sub-settings.
	class Indent : public Mui_widget {
	public:
		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			ignore_unused(avail_w);
			w = p.px(24);
			h = 0;
		}
	};

	// A wrapped help line with a left margin.
	class Help_label : public Mui_label {
	public:
		Help_label(std::string text_, float margin_ref)
				: Mui_label(std::move(text_), Mui_font_role::small, Mui_colour::text_dim, true), margin(margin_ref) {}

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			const int m = p.px(margin);
			Mui_label::measure(p, std::max(1, avail_w - m), w, h);
			w += m;
		}

		void paint(Mui_painter& p) override {
			const TileRect saved = rect;
			const int      m     = p.px(margin);
			rect                 = TileRect(rect.x + m, rect.y, std::max(1, rect.w - m), rect.h);
			Mui_label::paint(p);
			rect = saved;
		}

	private:
		float margin;
	};

	// Controls share a minimum width so the right column lines up.
	constexpr float control_ref_w = 280;

	class Option_dropdown : public Mui_dropdown {
	public:
		using Mui_dropdown::Mui_dropdown;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_dropdown::measure(p, avail_w, w, h);
			w = std::min(std::max(w, p.px(control_ref_w)), avail_w);
		}
	};

	class Option_segmented : public Mui_segmented {
	public:
		using Mui_segmented::Mui_segmented;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_segmented::measure(p, avail_w, w, h);
			w = std::min(std::max(w, p.px(control_ref_w)), avail_w);
		}
	};

	// Slider over choice indices; reports the end of a drag.
	class Option_slider : public Mui_slider {
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

	bool same(const std::string& a, const char* b) {
		return b && a == b;
	}

	bool is_off(const std::string& s) {
		return same(s, GumpStrings::No()) || same(s, GumpStrings::Disabled()) || s == "Off";
	}

	bool is_on(const std::string& s) {
		return same(s, GumpStrings::Yes()) || same(s, GumpStrings::Enabled()) || s == "On";
	}

	// Choices that are ordered numbers (e.g. "x2", "20 fps", "44100", "50%"),
	// optionally starting with an off value.
	bool numeric_choices(const std::vector<std::string>& choices) {
		if (choices.size() < 3) {
			return false;
		}
		long last = -1;
		for (size_t i = 0; i < choices.size(); i++) {
			const std::string& c = choices[i];
			if (i == 0 && is_off(c)) {
				last = 0;
				continue;
			}
			size_t b = 0;
			if (!c.empty() && (c[0] == 'x' || c[0] == 'X')) {
				b = 1;
			}
			if (b >= c.size() || !std::isdigit(static_cast<unsigned char>(c[b]))) {
				return false;
			}
			char*      end = nullptr;
			const long v   = std::strtol(c.c_str() + b, &end, 10);
			// Only a unit may follow the number, not "x200" of a resolution.
			if (end && *end && *end != ' ' && *end != '%') {
				return false;
			}
			if (v <= last) {
				return false;
			}
			last = v;
		}
		return true;
	}
}    // namespace

Mui_options_window::Mui_options_window(const char* tab) : Mui_window(Strings::Options()) {
	create_models();
	editor    = std::make_unique<Mui_config_editor>(*this, [this](const std::string& focus_id) {
        rebuild(focus_id);
    });
	keys      = std::make_unique<Mui_keys_editor>(*this, [this](const std::string& focus_id) {
        rebuild(focus_id);
    });
	tab_index = last_tab;
	if (tab && *tab) {
		for (int i = 0; i <= advanced_tab; i++) {
			if (std::string(tab_key(i)) == tab) {
				tab_index = i;
			}
		}
	}
}

Mui_options_window::~Mui_options_window() {
	// Destroy the models in reverse order (the video page restores the
	// palette it forced last).
	while (!models.empty()) {
		models.pop_back();
	}
}

void Mui_options_window::create_models() {
	models.clear();
	for (int i = 0; i < num_model_tabs; i++) {
		models.push_back(std::make_unique<Model>(Model::Tab(i)));
	}
}

std::vector<std::string> Mui_options_window::tab_labels() const {
	std::vector<std::string> out;
	for (int i = 0; i <= advanced_tab; i++) {
		out.emplace_back(Strings::Tab(i));
	}
	return out;
}

std::vector<std::string> Mui_options_window::tab_keys() const {
	std::vector<std::string> out;
	for (int i = 0; i <= advanced_tab; i++) {
		out.emplace_back(tab_key(i));
	}
	return out;
}

void Mui_options_window::on_tab(int index) {
	search.clear();
	highlight.clear();
	last_tab = index;
	Mui_window::on_tab(index);
}

std::string Mui_options_window::default_help() const {
	return Strings::DefaultHelp();
}

/*
 *  Title bar: search.
 */

void Mui_options_window::build_title(Mui_box& bar) {
	auto* field        = bar.add<Mui_text_field>("options.search", search, Strings::SearchPlaceholder());
	field->search_icon = true;
	field->stretch     = true;
	field->help        = Strings::SearchHelp();
	field->on_change   = [this](const std::string& s) {
        post([this, s] {
            search = s;
            highlight.clear();
            rebuild("options.search");
        });
	};
	field->on_submit = [this](const std::string&) {
		post([this] {
			// Enter jumps to the first result.
			const std::string needle = lower(search);
			for (int t = 0; t < num_model_tabs && !needle.empty(); t++) {
				for (const auto& r : models[size_t(t)]->rows()) {
					if (matches(*models[size_t(t)], r, needle)) {
						jump_to(t, r.id);
						return;
					}
				}
			}
			bool       key_section = false;
			const auto key_rows    = keys->search(needle, key_section);
			if (key_section) {
				jump_to_key_section();
				return;
			}
			if (!key_rows.empty()) {
				jump_to_key_row(key_rows.front());
				return;
			}
			const auto adv = editor->search(needle);
			if (!adv.empty()) {
				jump_to_key(adv.front()->path);
			}
		});
	};
	if (!search.empty()) {
		auto* clear     = bar.add<Mui_button>("options.search.clear", "", Mui_button::Style::flat, [this] {
            post([this] {
                search.clear();
                rebuild("options.search");
            });
        });
		clear->has_icon = true;
		clear->icon     = Mui_icon::close;
		clear->help     = Strings::ClearHelp();
	}
}

/*
 *  Footer: Reset page, Cancel, Apply.
 */

void Mui_options_window::build_footer(Mui_box& footer) {
	auto* reset    = footer.add<Mui_button>("options.reset", Strings::ResetPage(), Mui_button::Style::secondary, [this] {
        post([this] {
            reset_page();
        });
    });
	reset->help    = Strings::ResetHelp();
	reset->enabled = search.empty()
					 && (tab_index < num_model_tabs
								 ? models[size_t(tab_index)]->changed() || (tab_index == int(Model::Tab::input) && keys->changed())
								 : editor->changed());
	auto* cancel   = footer.add<Mui_button>("options.cancel", Strings::Cancel(), Mui_button::Style::secondary, [this] {
        post([this] {
            on_cancel();
        });
    });
	cancel->help   = Strings::CancelHelp();
	auto* apply    = footer.add<Mui_button>("options.apply", Strings::Apply(), Mui_button::Style::primary, [this] {
        post([this] {
            apply_all();
        });
    });
	apply->help    = Strings::ApplyHelp();
}

void Mui_options_window::on_cancel() {
	// Unapplied changes are dropped with the models; anything a classic
	// sub-screen (mixer, MIDI settings) already applied stays, as in the
	// classic screens.
	finish();
}

/*
 *  Body.
 */

void Mui_options_window::build(Mui_box& body) {
	if (!search.empty()) {
		build_results(body);
	} else if (tab_index >= num_model_tabs) {
		build_advanced(body);
	} else {
		build_page(body, tab_index);
	}
}

void Mui_options_window::build_advanced(Mui_box& body) {
	editor->build(body);
}

void Mui_options_window::build_page(Mui_box& body, int tab) {
	body.gap = 6;
	for (const auto& r : models[size_t(tab)]->rows()) {
		if (r.visible) {
			build_row(body, tab, r);
		}
	}
	if (tab == int(Model::Tab::input)) {
		keys->build(body);
	}
}

void Mui_options_window::build_row(Mui_box& body, int tab, const Row& r) {
	Model&            m    = *models[size_t(tab)];
	const std::string key  = tab_key(tab);
	const std::string cid  = "opt." + key + "." + r.id;
	const std::string help = m.help(r);
	auto*             row  = body.add<Mui_box>(Mui_box::Dir::vertical, "row." + key + "." + r.id);
	row->gap               = 2;
	row->pad               = 4;
	if (highlight == cid) {
		row->panel = true;
		row->bg    = Mui_colour::panel_alt;
	}
	auto* line = row->add<Mui_box>(Mui_box::Dir::horizontal);
	if (r.indent) {
		line->add<Indent>();
	}
	auto* label    = line->add<Mui_label>(m.label(r));
	label->stretch = true;
	label->id      = cid + ".label";
	// Badges.
	const bool changed = m.row_changed(r);
	if (r.apply == Model::Apply::restart) {
		line->add<Mui_badge>(Mui_badge::Kind::restart, Strings::NeedsRestart())->id = cid + ".restart";
	}
	if (changed) {
		if (r.apply == Model::Apply::now) {
			line->add<Mui_badge>(Mui_badge::Kind::now, Strings::AppliesNow())->id = cid + ".now";
		}
		line->add<Mui_badge>(Mui_badge::Kind::changed, Strings::Changed())->id = cid + ".changed";
	}
	// Control.
	Mui_widget* control = nullptr;
	if (r.kind == Model::Kind::action) {
		const std::string id = r.id;
		control              = line->add<Mui_button>(cid, Strings::Open(), Mui_button::Style::secondary, [this, tab, id] {
            post([this, tab, id] {
                models[size_t(tab)]->activate(id);
                rebuild();
            });
        });
	} else {
		const std::string id        = r.id;
		auto              on_change = [this, tab, id](int i) {
            post([this, tab, id, i] {
                change(tab, id, i, false);
            });
		};
		const auto& ch = r.choices;
		if (r.id == "game_area") {
			// A list of sizes, like Window Size.
			control = line->add<Option_dropdown>(cid, ch, r.value, on_change);
		} else if (ch.size() == 2 && is_off(ch[0]) && is_on(ch[1])) {
			control = line->add<Mui_toggle>(cid, r.value == 1, [on_change](bool v) {
				on_change(v ? 1 : 0);
			});
		} else if (r.value >= 0 && numeric_choices(ch)) {
			auto* s   = line->add<Option_slider>(cid, 0, int(ch.size()) - 1, 1, r.value);
			s->format = [ch](int v) {
				return v >= 0 && v < int(ch.size()) ? ch[size_t(v)] : std::string();
			};
			s->on_change = [this, tab, id, s](int i) {
				// Keep dragging: rebuild once the knob is released.
				const bool dragging = get_pressed() == s;
				post([this, tab, id, i, dragging] {
					change(tab, id, i, dragging);
				});
			};
			s->on_release = [this] {
				post([this] {
					rebuild();
				});
			};
			control = s;
		} else if (ch.size() == 2) {
			control = line->add<Option_segmented>(cid, ch, r.value, on_change);
		} else {
			control = line->add<Option_dropdown>(cid, ch, r.value, on_change);
		}
	}
	control->help = help;
	label->help   = help;
	if (!help.empty()) {
		// Line up with the label: indent plus the theme gap of the row.
		const float margin                     = r.indent ? 24 + theme().metrics.gap : 0;
		row->add<Help_label>(help, margin)->id = cid + ".help";
	}
}

void Mui_options_window::change(int tab, const std::string& id, int index, bool defer_rebuild) {
	const bool structural = models[size_t(tab)]->set_value(id, index);
	if (!defer_rebuild || structural) {
		rebuild();
	}
}

/*
 *  Search.
 */

bool Mui_options_window::matches(const Model& m, const Row& r, const std::string& needle) const {
	if (!r.visible || needle.empty()) {
		return false;
	}
	return lower(m.label(r)).find(needle) != std::string::npos || lower(m.help(r)).find(needle) != std::string::npos
		   || lower(r.keys).find(needle) != std::string::npos;
}

void Mui_options_window::build_results(Mui_box& body) {
	const std::string needle = lower(search);
	int               count  = 0;
	for (int t = 0; t < num_model_tabs; t++) {
		for (const auto& r : models[size_t(t)]->rows()) {
			count += matches(*models[size_t(t)], r, needle) ? 1 : 0;
		}
	}
	bool       key_section = false;
	const auto key_rows    = keys->search(needle, key_section);
	count += int(key_rows.size()) + (key_section ? 1 : 0);
	const auto adv = editor->search(needle);
	count += int(adv.size());
	auto* found = body.add<Mui_label>(
			count ? std::to_string(count) + " " + (count == 1 ? Strings::SettingFound() : Strings::SettingsFound())
				  : std::string(Strings::NoMatches()),
			Mui_font_role::body, Mui_colour::text_dim);
	found->id = "options.search.count";
	for (int t = 0; t < num_model_tabs; t++) {
		const Model& m     = *models[size_t(t)];
		bool         first = true;
		auto         header = [&] {
            if (first) {
                body.add<Mui_section_header>(Strings::Tab(t));
                first = false;
            }
		};
		for (const auto& r : m.rows()) {
			if (!matches(m, r, needle)) {
				continue;
			}
			header();
			const std::string id = r.id;
			auto*             b  = body.add<Mui_button>(
                    std::string("options.result.") + tab_key(t) + "." + r.id, m.label(r), Mui_button::Style::secondary,
                    [this, t, id] {
                        post([this, t, id] {
                            jump_to(t, id);
                        });
                    });
			b->fill_w              = true;
			b->help                = Strings::JumpHelp();
			const std::string help = m.help(r);
			if (!help.empty()) {
				body.add<Help_label>(help, 8);
			}
		}
		if (t == int(Model::Tab::input)) {
			// The key bindings: the section itself, or the matching actions.
			if (key_section) {
				header();
				auto* b = body.add<Mui_button>(
						"options.result.keys", Mui_keys_model::text(0xDD0), Mui_button::Style::secondary, [this] {
							post([this] {
								jump_to_key_section();
							});
						});
				b->fill_w = true;
				b->help   = Strings::JumpHelp();
			}
			for (const auto& id : key_rows) {
				header();
				const std::string rid = id;
				auto*             b   = body.add<Mui_button>(
                        "options.result.keys." + id, Mui_keys_model::text(0xDD0) + ": " + keys->row_label(id),
                        Mui_button::Style::secondary, [this, rid] {
                            post([this, rid] {
                                jump_to_key_row(rid);
                            });
                        });
				b->fill_w = true;
				b->help   = Strings::JumpHelp();
			}
		}
	}
	// exult.cfg keys of the Advanced tab.
	if (!adv.empty()) {
		body.add<Mui_section_header>(Strings::Tab(advanced_tab));
	}
	for (const auto* e : adv) {
		const std::string path = e->path;
		auto* b   = body.add<Mui_button>("options.result.advanced." + path, path, Mui_button::Style::secondary, [this, path] {
            post([this, path] {
                jump_to_key(path);
            });
        });
		b->fill_w = true;
		b->help   = Strings::JumpHelp();
		body.add<Help_label>(editor->description(*e), 8);
	}
}

void Mui_options_window::jump_to(int tab, const std::string& id) {
	search.clear();
	tab_index = tab;
	last_tab  = tab;
	highlight = std::string("opt.") + tab_key(tab) + "." + id;
	rebuild(highlight);
}

void Mui_options_window::jump_to_key(const std::string& path) {
	search.clear();
	highlight.clear();
	tab_index = advanced_tab;
	last_tab  = advanced_tab;
	editor->reveal(path);
	rebuild(Mui_config_editor::control_id(path));
}

void Mui_options_window::jump_to_key_row(const std::string& id) {
	search.clear();
	highlight.clear();
	tab_index = int(Model::Tab::input);
	last_tab  = tab_index;
	keys->reveal(id);
	rebuild(Mui_keys_editor::focus_id(id));
}

void Mui_options_window::jump_to_key_section() {
	search.clear();
	highlight.clear();
	tab_index = int(Model::Tab::input);
	last_tab  = tab_index;
	rebuild("keys.filter");
}

/*
 *  Apply and reset.
 */

void Mui_options_window::apply_all() {
	const std::string old_style       = Mui::menu_style();
	bool              display_applied = false;
	for (int t = 0; t < num_model_tabs; t++) {
		Model& m = *models[size_t(t)];
		if (!m.changed()) {
			continue;
		}
		m.apply();
		display_applied |= m.get_tab() == Model::Tab::display;
	}
	// The key bindings write their own keys file and use it at once.
	bool keys_wrote = false;
	if (keys->changed()) {
		keys->apply(keys_wrote);
	}
	// The pages may have written keys the Advanced tab shows.
	editor->reload();
	bool editor_wrote = false;
	if (editor->changed() && !editor->apply(editor_wrote)) {
		// Show the values that need fixing; nothing of the tab was written.
		search.clear();
		tab_index = advanced_tab;
		last_tab  = advanced_tab;
	}
	if (display_applied || editor_wrote) {
		// The display page reloads the game text (language, fonts): every
		// classic gump holds strings of the old text, so build them again.
		// They also load their settings again after an Advanced edit.
		create_models();
		title = Strings::Options();
	}
	keys->reload();
	rebuild();
	gwin->set_all_dirty();
	// A new menu style (UI page) applies to every themed window; classic
	// closes them and opens the classic screens instead.
	const std::string style = Mui::menu_style();
	if (style != old_style) {
		if (Mui::themed()) {
			switch_theme(style);
		} else {
			finish();
		}
	}
}

void Mui_options_window::reset_page() {
	if (tab_index < num_model_tabs) {
		models[size_t(tab_index)]->revert();
		if (tab_index == int(Model::Tab::input)) {
			keys->revert();
		}
	} else {
		editor->revert();
	}
	rebuild();
}
