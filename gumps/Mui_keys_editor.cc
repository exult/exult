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

#include "Mui_keys_editor.h"

#include "Mui_theme.h"
#include "Mui_window.h"
#include "items.h"
#include "utils.h"

#include <algorithm>
#include <cctype>

namespace {
	using Model = Mui_keys_model;

	std::string tr(unsigned id) {
		return Model::text(id);
	}

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	std::string group_name(Model::Group g) {
		switch (g) {
		case Model::Group::cheat:
			return "cheat";
		case Model::Group::mapedit:
			return "mapedit";
		default:
			return "game";
		}
	}

	std::string group_title(Model::Group g) {
		switch (g) {
		case Model::Group::cheat:
			return tr(0xDD3);
		case Model::Group::mapedit:
			return tr(0xDD4);
		default:
			return tr(0xDD2);
		}
	}

	// A wrapped small line.
	class Note : public Mui_label {
	public:
		explicit Note(std::string text_, Mui_colour c = Mui_colour::text_dim)
				: Mui_label(std::move(text_), Mui_font_role::small, c, true) {}
	};

	constexpr float filter_ref_w = 280;

	class Filter_field : public Mui_text_field {
	public:
		using Mui_text_field::Mui_text_field;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_text_field::measure(p, avail_w, w, h);
			w = std::min(p.px(filter_ref_w), avail_w);
		}
	};

	// A key chip that waits for the next key press. It sees every key
	// (Tab and Escape too) as SDL delivered it, so the keypad and the
	// modifiers are exactly what the key bindings will match.
	class Key_capture : public Mui_button {
	public:
		Key_capture(std::string id_, std::string text_) : Mui_button(std::move(id_), std::move(text_), Style::primary) {
			focusable = true;
		}

		std::function<void(ExultKey)> on_captured;
		std::function<void()>         on_cancel;

		bool wants_raw_keys() const override {
			return true;
		}

		bool on_key(SDL_Keycode key, int unicode) override {
			ignore_unused(unicode);
			const Mui_window* w = window();
			if (!w) {
				return true;
			}
			const SDL_Event& ev = w->last_raw_key();
			if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat) {
				return true;
			}
			const SDL_Keycode raw = ev.key.key;
			if (raw == SDLK_ESCAPE || key == SDLK_ESCAPE) {
				if (on_cancel) {
					auto cb = on_cancel;
					cb();
				}
				return true;
			}
			switch (raw) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
			case SDLK_LCTRL:
			case SDLK_RCTRL:
			case SDLK_LALT:
			case SDLK_RALT:
			case SDLK_LGUI:
			case SDLK_RGUI:
			case SDLK_MODE:
			case SDLK_CAPSLOCK:
			case SDLK_NUMLOCKCLEAR:
			case SDLK_SCROLLLOCK:
			case SDLK_UNKNOWN:
				return true;    // A modifier on its own is not a key.
			default:
				break;
			}
			ExultKey k{raw, SDL_KMOD_NONE};
			if (KeyBinder::NormaliseEvent(ev, k) && on_captured) {
				auto cb = on_captured;
				cb(k);
			}
			return true;
		}

		void on_blur() override {
			if (on_cancel) {
				auto cb = on_cancel;
				cb();
			}
		}
	};
}    // namespace

Mui_keys_editor::Mui_keys_editor(Mui_window& w, std::function<void(const std::string&)> rebuild_)
		: win(w), rebuild(std::move(rebuild_)) {
	expanded.insert(int(Model::Group::game));
}

std::string Mui_keys_editor::key_text(ExultKey key) {
	return KeyBinder::DescribeKey(key);
}

void Mui_keys_editor::say(const std::string& text, bool error) {
	status       = text;
	status_error = error;
}

void Mui_keys_editor::reload() {
	if (!model.changed()) {
		model.load();
	}
}

void Mui_keys_editor::revert() {
	model.revert();
	end_capture();
	status.clear();
}

void Mui_keys_editor::end_capture() {
	capture_row.clear();
	capture_index = -1;
}

/*
 *  Search.
 */

std::vector<std::string> Mui_keys_editor::search(const std::string& needle, bool& section) const {
	std::vector<std::string> out;
	section = model.section_matches(needle);
	if (!model.available() || needle.empty()) {
		return out;
	}
	for (const auto& r : model.rows()) {
		if (model.row_matches(r, needle)) {
			out.push_back(r.id);
		}
	}
	return out;
}

void Mui_keys_editor::reveal(const std::string& id) {
	filter.clear();
	highlight = id;
	if (const auto* r = model.find(id)) {
		expanded.insert(int(r->group));
	}
}

/*
 *  Widgets.
 */

void Mui_keys_editor::build(Mui_box& body) {
	if (!model.available()) {
		return;
	}
	if (!takeover.empty()) {
		win.post([this] {
			if (!takeover.empty()) {
				ask_now();
			}
		});
	}
	auto* head = body.add<Mui_section_header>(tr(0xDD0));
	head->id   = "keys.header";
	// Which keys file is in use.
	const std::string cfg = model.configured_path();
	const std::string own = model.own_path();
	std::string       note;
	if (cfg.empty() || cfg == "(default)") {
		note = Model::format(0xDED, own);
	} else if (get_system_path(cfg) == own) {
		note = Model::format(0xDEC, own);
	} else {
		note = Model::format(0xDF6, cfg);
	}
	body.add<Note>(note)->id                        = "keys.file";
	body.add<Note>(tr(0xDEE) + " " + tr(0xDF5))->id = "keys.note";
	auto* bar                                       = body.add<Mui_box>(Mui_box::Dir::horizontal);
	auto* field                                     = bar->add<Filter_field>("keys.filter", filter, tr(0xDDE));
	field->search_icon                              = true;
	field->help                                     = tr(0xDF4);
	field->on_change                                = [this](const std::string& s) {
        win.post([this, s] {
            filter = s;
            highlight.clear();
            rebuild("keys.filter");
        });
	};
	auto* all    = bar->add<Mui_button>("keys.reset_all", tr(0xDD9), Mui_button::Style::secondary, [this] {
        win.post([this] {
            end_capture();
            win.show_confirm(tr(0xDE8), true, [this](bool yes) {
                if (yes) {
                    model.reset_all();
                    say(std::string(), false);
                }
                rebuild("keys.reset_all");
            });
        });
    });
	all->help    = tr(0xDE3);
	all->enabled = !model.is_default();
	if (!status.empty()) {
		body.add<Note>(status, status_error ? Mui_colour::danger : Mui_colour::text)->id = "keys.status";
	}
	const std::string needle = lower(filter);
	for (const auto g : {Model::Group::game, Model::Group::cheat, Model::Group::mapedit}) {
		std::vector<const Model::Row*> rows;
		for (const auto& r : model.rows()) {
			if (r.group == g && (needle.empty() || model.row_matches(r, needle))) {
				rows.push_back(&r);
			}
		}
		if (rows.empty()) {
			continue;
		}
		const bool open = !needle.empty() || expanded.count(int(g)) != 0;
		auto*      gh   = body.add<Mui_button>(
                "keys.group." + group_name(g),
                group_title(g) + "  (" + std::to_string(rows.size()) + ")  " + (open ? tr(0xDF2) : tr(0xDF1)),
                Mui_button::Style::flat, [this, g] {
                    win.post([this, g] {
                        if (expanded.count(int(g))) {
                            expanded.erase(int(g));
                        } else {
                            expanded.insert(int(g));
                        }
                        rebuild("keys.group." + group_name(g));
                    });
                });
		gh->fill_w = true;
		gh->help   = group_title(g);
		if (!open) {
			continue;
		}
		if (g == Model::Group::cheat) {
			body.add<Note>(tr(0xDEF))->id = "keys.cheatnote";
		}
		for (const auto* r : rows) {
			build_row(body, *r);
		}
	}
}

void Mui_keys_editor::build_row(Mui_box& col, const Mui_keys_model::Row& r) {
	const std::string rid = "keys.row." + r.id;
	const std::string id  = r.id;
	auto*             row = col.add<Mui_box>(Mui_box::Dir::vertical, rid);
	row->gap              = 2;
	row->pad              = 4;
	if (highlight == r.id) {
		row->panel = true;
		row->bg    = Mui_colour::panel_alt;
	}
	const auto keys    = model.keys_of(r.id);
	const bool changed = model.row_changed(r.id);
	// Name and badges.
	auto* line            = row->add<Mui_box>(Mui_box::Dir::horizontal);
	auto* label           = line->add<Mui_label>(r.label);
	label->stretch        = true;
	label->id             = rid + ".label";
	label->help           = tr(0xDDF);
	const bool from_mod   = std::any_of(keys.begin(), keys.end(), [](const Model::Key& k) {
        return k.from_mod;
    });
	const bool overridden = std::any_of(keys.begin(), keys.end(), [](const Model::Key& k) {
		return k.overridden;
	});
	if (from_mod) {
		line->add<Mui_badge>(Mui_badge::Kind::restart, tr(0xDDC))->id = rid + ".mod";
	}
	if (overridden) {
		line->add<Mui_badge>(Mui_badge::Kind::restart, tr(0xDDD))->id = rid + ".overridden";
	}
	if (changed) {
		line->add<Mui_badge>(Mui_badge::Kind::now, get_text_msg(0xC28 - msg_file_start))->id     = rid + ".now";
		line->add<Mui_badge>(Mui_badge::Kind::changed, get_text_msg(0xC2A - msg_file_start))->id = rid + ".changed";
	}
	// Keys and buttons.
	auto* edit   = row->add<Mui_box>(Mui_box::Dir::horizontal);
	int   n      = 0;
	int   user_n = 0;
	for (const auto& k : keys) {
		const std::string cid = rid + ".key." + std::to_string(n);
		if (!k.from_mod && capture_row == r.id && capture_index == user_n) {
			auto* cap        = edit->add<Key_capture>("keys.capture", tr(0xDDA));
			cap->help        = tr(0xDDF);
			cap->on_captured = [this, id, user_n](ExultKey key) {
				win.post([this, id, user_n, key] {
					captured(id, user_n, key);
				});
			};
			cap->on_cancel = [this] {
				win.post([this] {
					if (!capture_row.empty()) {
						end_capture();
						rebuild(std::string());
					}
				});
			};
		} else {
			auto* chip = edit->add<Mui_button>(cid, key_text(k.key), Mui_button::Style::secondary);
			if (k.from_mod) {
				chip->enabled = false;
				chip->help    = tr(0xDF3);
			} else {
				chip->enabled   = !k.overridden;
				chip->help      = k.overridden ? tr(0xDDD) : tr(0xDDF);
				const int index = user_n;
				chip->on_click  = [this, id, index] {
                    win.post([this, id, index] {
                        start_capture(id, index);
                    });
				};
			}
		}
		n++;
		user_n += k.from_mod ? 0 : 1;
	}
	if (keys.empty()) {
		edit->add<Mui_label>(tr(0xDDB), Mui_font_role::small, Mui_colour::text_dim)->id = rid + ".notset";
	}
	edit->add<Mui_spacer>(0, true);
	if (capture_row == r.id && capture_index < 0) {
		auto* cap        = edit->add<Key_capture>("keys.capture", tr(0xDDA));
		cap->help        = tr(0xDDF);
		cap->on_captured = [this, id](ExultKey key) {
			win.post([this, id, key] {
				captured(id, -1, key);
			});
		};
		cap->on_cancel = [this] {
			win.post([this] {
				if (!capture_row.empty()) {
					end_capture();
					rebuild(std::string());
				}
			});
		};
	}
	auto* add       = edit->add<Mui_button>(rid + ".add", tr(0xDD6), Mui_button::Style::flat, [this, id] {
        win.post([this, id] {
            start_capture(id, -1);
        });
    });
	add->help       = tr(0xDE0);
	auto* unbind    = edit->add<Mui_button>(rid + ".unbind", tr(0xDD7), Mui_button::Style::flat, [this, id] {
        win.post([this, id] {
            end_capture();
            const std::string name = model.label_of(id);
            model.unbind(id);
            say(Model::format(0xDF7, name), false);
            rebuild(std::string());
        });
    });
	unbind->help    = tr(0xDE1);
	unbind->enabled = std::any_of(keys.begin(), keys.end(), [](const Model::Key& k) {
		return !k.from_mod;
	});
	auto* reset     = edit->add<Mui_button>(rid + ".reset", tr(0xDD8), Mui_button::Style::flat, [this, id] {
        win.post([this, id] {
            end_capture();
            std::vector<ExultKey> taken;
            const auto            conflicts = model.reset_conflicts(id, &taken);
            if (conflicts.empty()) {
                model.reset_row(id);
                rebuild(std::string());
                return;
            }
            // The default key now belongs to another action.
            win.show_confirm(
                    Model::format(0xDF0, model.label_of(id), key_text(taken.front()), conflicts.front().other_label), false,
                    [this, id](bool yes) {
                        if (yes) {
                            model.reset_row(id);
                        }
                        rebuild(std::string());
                    });
        });
    });
	reset->help     = Model::format(0xDF8, r.label);
	reset->enabled  = !model.row_is_default(r.id);
}

void Mui_keys_editor::start_capture(const std::string& id, int index) {
	capture_row   = id;
	capture_index = index;
	say(std::string(), false);
	rebuild("keys.capture");
}

void Mui_keys_editor::captured(const std::string& id, int index, ExultKey key) {
	end_capture();
	const auto keys = model.keys_of(id);
	ExultKey   old{SDLK_UNKNOWN, SDL_KMOD_NONE};
	int        n = 0;
	for (const auto& k : keys) {
		if (!k.from_mod && n++ == index) {
			old = k.key;
		}
	}
	const std::string name = key_text(key);
	const auto        c    = model.check_key(id, key);
	switch (c.status) {
	case Model::Check::unsupported:
		say(Model::format(0xDE6, name), true);
		break;
	case Model::Check::same_row:
		say(Model::format(0xDE7, name, model.label_of(id)), true);
		break;
	case Model::Check::taken_by_mod:
		say(Model::format(0xDE5, name, c.other_label), true);
		break;
	case Model::Check::taken_by_row: {
		// Moving a key takes it from the other action.
		const std::string other_id    = c.other_id;
		const std::string other_label = c.other_label;
		win.show_confirm(
				Model::format(0xDE4, name, other_label, model.label_of(id)), false,
				[this, id, old, key, other_id, other_label](bool yes) {
					if (yes) {
						model.set_key(id, old, key);
						say(model.keys_of(other_id).empty() ? Model::format(0xDF7, other_label) : std::string(), false);
					}
					rebuild(std::string());
				});
		return;
	}
	case Model::Check::ok:
		model.set_key(id, old, key);
		say(std::string(), false);
		break;
	}
	rebuild(std::string());
}

/*
 *  Apply.
 */

void Mui_keys_editor::report(const Mui_keys_model::Apply_result& r, bool& wrote) {
	switch (r.status) {
	case Model::Apply_status::ok:
		wrote = true;
		say(Model::format(0xDEC, r.path), false);
		break;
	case Model::Apply_status::refused:
		say(Model::format(0xDEA, r.foreign), true);
		break;
	case Model::Apply_status::failed:
		say(Model::format(0xDEB, r.path), true);
		break;
	case Model::Apply_status::needs_takeover:
		ask_takeover(r.foreign);
		break;
	case Model::Apply_status::nothing:
		break;
	}
}

void Mui_keys_editor::ask_takeover(const std::string& foreign) {
	// Asked once the window has rebuilt (a rebuild closes any question).
	takeover = foreign;
}

void Mui_keys_editor::ask_now() {
	const std::string foreign = takeover;
	takeover.clear();
	win.show_confirm(Model::format(0xDE9, foreign, model.own_path()), false, [this](bool yes) {
		if (yes) {
			bool wrote = false;
			report(model.apply(true), wrote);
		}
		rebuild(std::string());
	});
}

bool Mui_keys_editor::apply(bool& wrote) {
	if (!model.available() || !model.changed()) {
		return true;
	}
	end_capture();
	const auto r = model.apply(false);
	report(r, wrote);
	return r.status == Model::Apply_status::ok || r.status == Model::Apply_status::nothing;
}
