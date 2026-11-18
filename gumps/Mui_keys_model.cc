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

#include "Mui_keys_model.h"

#include "Configuration.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"
#include "utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace {
	constexpr unsigned msg_title       = 0xDD0;
	constexpr unsigned msg_search_keys = 0xDD1;

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	std::string humanise(const std::string& action) {
		std::string s = action;
		for (auto& c : s) {
			if (c == '_') {
				c = ' ';
			}
		}
		if (!s.empty()) {
			s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
		}
		return s;
	}

	std::string config_key() {
		return "config/disk/game/" + Game::get_gametitle() + "/keys";
	}

	// Number of parameters that matter: trailing -1 is the same as none.
	int trim_params(const int* params, int nparams) {
		int n = std::min(nparams, c_maxparams);
		while (n > 0 && params[n - 1] == -1) {
			n--;
		}
		return n;
	}

	bool same_params(const int* a, const int* b) {
		for (int i = 0; i < c_maxparams; i++) {
			if (a[i] != b[i]) {
				return false;
			}
		}
		return true;
	}

	bool has_marker_line(const std::string& path) {
		auto in = U7open_in(path.c_str(), true);
		if (!in) {
			return false;
		}
		std::string first;
		std::getline(*in, first);
		return first.rfind(Mui_keys_model::marker(), 0) == 0;
	}
}    // namespace

const char* Mui_keys_model::marker() {
	return "# exult-keys-editor v1";
}

std::string Mui_keys_model::text(unsigned msg_id) {
	const char* t = get_text_msg(msg_id - msg_file_start);
	return t ? t : "";
}

std::string Mui_keys_model::format(unsigned msg_id, const std::string& a, const std::string& b, const std::string& c) {
	const std::string in = text(msg_id);
	std::string       out;
	for (size_t i = 0; i < in.size(); i++) {
		if (in[i] == '%' && i + 1 < in.size() && in[i + 1] >= '1' && in[i + 1] <= '3') {
			out += in[i + 1] == '1' ? a : in[i + 1] == '2' ? b : c;
			i++;
		} else {
			out += in[i];
		}
	}
	return out;
}

Mui_keys_model::Mui_keys_model() {
	load();
}

/*
 *  Reading the live bindings.
 */

std::string Mui_keys_model::row_id(const std::string& action, const int* params, int nparams, int& trimmed) {
	trimmed        = trim_params(params, nparams);
	std::string id = action;
	for (int i = 0; i < trimmed; i++) {
		id += (i == 0 ? "." : "_") + std::to_string(params[i]);
	}
	return id;
}

Mui_keys_model::Entry Mui_keys_model::make_entry(const KeyBinder::Binding& b) {
	Entry e;
	e.key     = b.key;
	e.action  = b.action;
	e.nparams = b.nparams;
	for (int i = 0; i < c_maxparams; i++) {
		e.params[i] = b.params[i];
	}
	int trimmed = 0;
	e.row       = row_id(e.action, e.params, e.nparams, trimmed);
	e.comment   = b.comment;
	e.hidden    = b.hidden;
	return e;
}

// The user's lines (not the mod's); a later line for a key replaces an earlier one.
std::vector<Mui_keys_model::Entry> Mui_keys_model::user_entries(const std::vector<KeyBinder::Binding>& all) {
	std::vector<Entry> out;
	for (const auto& b : all) {
		if (b.source == KeyBinder::Source::patch) {
			continue;
		}
		Entry e = make_entry(b);
		out.erase(
				std::remove_if(
						out.begin(), out.end(),
						[&](const Entry& x) {
							return key_eq(x.key, e.key);
						}),
				out.end());
		out.push_back(std::move(e));
	}
	return out;
}

bool Mui_keys_model::same_command(const Entry& a, const Entry& b) {
	return a.action == b.action && same_params(a.params, b.params);
}

bool Mui_keys_model::same_set(const std::vector<Entry>& a, const std::vector<Entry>& b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (const auto& x : a) {
		const auto it = std::find_if(b.begin(), b.end(), [&](const Entry& y) {
			return key_eq(x.key, y.key);
		});
		if (it == b.end() || !same_command(x, *it)) {
			return false;
		}
	}
	return true;
}

void Mui_keys_model::load() {
	rows_.clear();
	cur.clear();
	orig.clear();
	defaults.clear();
	mod.clear();
	if (!keybinder) {
		return;
	}
	const auto all = keybinder->ListBindings();
	cur            = user_entries(all);
	orig           = cur;
	for (const auto& b : all) {
		if (b.source == KeyBinder::Source::patch && b.effective) {
			mod.push_back(make_entry(b));
		}
	}
	{
		KeyBinder scratch;
		scratch.LoadDefaults(true);
		defaults = user_entries(scratch.ListBindings());
	}
	// One row per command: first the table's actions, then every command
	// any set binds (with its parameters).
	const Exult_Game                       running = Game::get_game_type();
	std::vector<const std::vector<Entry>*> sets{&defaults, &cur, &mod};
	auto                                   bound_anywhere = [&](const std::string& id) {
        for (const auto* s : sets) {
            for (const auto& e : *s) {
                if (e.row == id) {
                    return true;
                }
            }
        }
        return false;
	};
	for (const auto& info : KeyBinder::ListActions()) {
		std::vector<std::string> ids;
		auto                     add_id = [&](const std::string& id) {
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
                ids.push_back(id);
            }
		};
		const bool wrong_game = info.game != NONE && info.game != running;
		if (!info.needs_params && info.type != KeyBinder::ActionInfo::Type::hidden && (!wrong_game || bound_anywhere(info.name))) {
			add_id(info.name);
		}
		for (const auto* s : sets) {
			for (const auto& e : *s) {
				if (e.action == info.name) {
					add_id(e.row);
				}
			}
		}
		for (const auto& id : ids) {
			Row r;
			r.id     = id;
			r.action = info.name;
			// Parameters come from an entry of the row, if there is one.
			bool found = false;
			for (const auto* s : sets) {
				for (const auto& e : *s) {
					if (!found && e.row == id) {
						r.nparams = trim_params(e.params, e.nparams);
						for (int i = 0; i < c_maxparams; i++) {
							r.params[i] = e.params[i];
						}
						found = true;
					}
				}
			}
			switch (info.type) {
			case KeyBinder::ActionInfo::Type::cheat:
				r.group = Group::cheat;
				break;
			case KeyBinder::ActionInfo::Type::mapedit:
				r.group = Group::mapedit;
				break;
			default:
				r.group = Group::game;
				break;
			}
			r.mod_keys = std::any_of(mod.begin(), mod.end(), [&](const Entry& e) {
				return e.row == id;
			});
			rows_.push_back(std::move(r));
		}
	}
	for (auto& r : rows_) {
		r.label = label_for(r);
	}
}

std::string Mui_keys_model::label_for(const Row& r) const {
	for (const auto* s : {&cur, &defaults, &mod}) {
		for (const auto& e : *s) {
			if (e.row == r.id && !e.comment.empty()) {
				return e.comment;
			}
		}
	}
	std::string base;
	for (const auto& info : KeyBinder::ListActions()) {
		if (info.name == r.action) {
			base = info.label;
			break;
		}
	}
	if (base.empty()) {
		base = humanise(r.action);
	}
	if (r.nparams > 0) {
		base += " (";
		for (int i = 0; i < r.nparams; i++) {
			base += (i ? " " : "") + std::to_string(r.params[i]);
		}
		base += ")";
	}
	return base;
}

const Mui_keys_model::Row* Mui_keys_model::find(const std::string& id) const {
	const auto it = std::find_if(rows_.begin(), rows_.end(), [&](const Row& r) {
		return r.id == id;
	});
	return it == rows_.end() ? nullptr : &*it;
}

std::string Mui_keys_model::label_of(const std::string& id) const {
	const Row* r = find(id);
	return r ? r->label : id;
}

const Mui_keys_model::Entry* Mui_keys_model::find_cur(ExultKey key) const {
	for (const auto& e : cur) {
		if (key_eq(e.key, key)) {
			return &e;
		}
	}
	return nullptr;
}

const Mui_keys_model::Entry* Mui_keys_model::find_mod(ExultKey key) const {
	for (const auto& e : mod) {
		if (key_eq(e.key, key)) {
			return &e;
		}
	}
	return nullptr;
}

std::vector<Mui_keys_model::Key> Mui_keys_model::keys_of(const std::string& id) const {
	std::vector<Key> out;
	for (const auto& e : cur) {
		if (e.row == id) {
			Key k;
			k.key        = e.key;
			k.overridden = find_mod(e.key) != nullptr;
			out.push_back(k);
		}
	}
	for (const auto& e : mod) {
		if (e.row == id) {
			Key k;
			k.key      = e.key;
			k.from_mod = true;
			out.push_back(k);
		}
	}
	return out;
}

/*
 *  Edits.
 */

bool Mui_keys_model::changed() const {
	return !same_set(cur, orig);
}

bool Mui_keys_model::is_default() const {
	return same_set(cur, defaults);
}

std::vector<ExultKey> Mui_keys_model::keys_in(const std::vector<Entry>& set, const std::string& id) {
	std::vector<ExultKey> out;
	for (const auto& e : set) {
		if (e.row == id) {
			out.push_back(e.key);
		}
	}
	return out;
}

static bool same_keys(const std::vector<ExultKey>& a, const std::vector<ExultKey>& b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (const auto& k : a) {
		if (std::none_of(b.begin(), b.end(), [&](const ExultKey& o) {
				return o.key == k.key && o.mod == k.mod;
			})) {
			return false;
		}
	}
	return true;
}

bool Mui_keys_model::row_changed(const std::string& id) const {
	return !same_keys(keys_in(cur, id), keys_in(orig, id));
}

bool Mui_keys_model::row_is_default(const std::string& id) const {
	return same_keys(keys_in(cur, id), keys_in(defaults, id));
}

Mui_keys_model::Check_result Mui_keys_model::check_key(const std::string& id, ExultKey key) const {
	Check_result r;
	if (key.key == SDLK_UNKNOWN || KeyBinder::FormatKeyToken(key).empty()) {
		r.status = Check::unsupported;
		return r;
	}
	if (const Entry* m = find_mod(key)) {
		r.other_id    = m->row;
		r.other_label = label_of(m->row);
		r.status      = m->row == id ? Check::same_row : Check::taken_by_mod;
		return r;
	}
	if (const Entry* e = find_cur(key)) {
		r.other_id    = e->row;
		r.other_label = label_of(e->row);
		r.status      = e->row == id ? Check::same_row : Check::taken_by_row;
		return r;
	}
	return r;
}

void Mui_keys_model::add_entry(const std::string& id, ExultKey key) {
	const Row* row = find(id);
	if (!row) {
		return;
	}
	Entry e;
	e.key     = key;
	e.row     = id;
	e.action  = row->action;
	e.nparams = row->nparams;
	for (int i = 0; i < c_maxparams; i++) {
		e.params[i] = row->params[i];
	}
	// Same help text as the row's other keys.
	for (const auto* s : {&cur, &defaults}) {
		for (const auto& x : *s) {
			if (x.row == id && e.comment.empty() && !e.hidden) {
				e.comment = x.comment;
				e.hidden  = x.hidden;
			}
		}
	}
	cur.push_back(std::move(e));
}

void Mui_keys_model::set_key(const std::string& id, ExultKey old_key, ExultKey new_key) {
	// The new key goes to this row only.
	cur.erase(
			std::remove_if(
					cur.begin(), cur.end(),
					[&](const Entry& e) {
						return key_eq(e.key, new_key);
					}),
			cur.end());
	if (old_key.key != SDLK_UNKNOWN) {
		for (auto& e : cur) {
			if (e.row == id && key_eq(e.key, old_key)) {
				e.key = new_key;
				return;
			}
		}
	}
	add_entry(id, new_key);
}

void Mui_keys_model::unbind(const std::string& id) {
	cur.erase(
			std::remove_if(
					cur.begin(), cur.end(),
					[&](const Entry& e) {
						return e.row == id;
					}),
			cur.end());
}

std::vector<Mui_keys_model::Check_result> Mui_keys_model::reset_conflicts(
		const std::string& id, std::vector<ExultKey>* keys) const {
	std::vector<Check_result> out;
	for (const auto& d : defaults) {
		if (d.row != id || find_mod(d.key)) {
			continue;
		}
		const Entry* e = find_cur(d.key);
		if (e && e->row != id) {
			Check_result r;
			r.status      = Check::taken_by_row;
			r.other_id    = e->row;
			r.other_label = label_of(e->row);
			out.push_back(r);
			if (keys) {
				keys->push_back(d.key);
			}
		}
	}
	return out;
}

void Mui_keys_model::reset_row(const std::string& id) {
	unbind(id);
	for (const auto& d : defaults) {
		if (d.row != id || find_mod(d.key)) {
			continue;
		}
		cur.erase(
				std::remove_if(
						cur.begin(), cur.end(),
						[&](const Entry& e) {
							return key_eq(e.key, d.key);
						}),
				cur.end());
		cur.push_back(d);
	}
}

void Mui_keys_model::reset_all() {
	cur = defaults;
}

void Mui_keys_model::revert() {
	cur = orig;
}

/*
 *  Search.
 */

bool Mui_keys_model::section_matches(const std::string& needle) const {
	if (needle.empty() || rows_.empty()) {
		return false;
	}
	if (lower(text(msg_title)).find(needle) != std::string::npos) {
		return true;
	}
	std::istringstream words(lower(text(msg_search_keys)));
	std::string        w;
	while (words >> w) {
		if (w.find(needle) != std::string::npos) {
			return true;
		}
	}
	return false;
}

bool Mui_keys_model::row_matches(const Row& r, const std::string& needle) const {
	if (needle.empty()) {
		return false;
	}
	std::string action_words = r.action;
	if (lower(r.label).find(needle) != std::string::npos || action_words.find(needle) != std::string::npos) {
		return true;
	}
	for (auto& c : action_words) {
		if (c == '_') {
			c = ' ';
		}
	}
	if (action_words.find(needle) != std::string::npos) {
		return true;
	}
	for (const auto& k : keys_of(r.id)) {
		if (lower(KeyBinder::DescribeKey(k.key)).find(needle) != std::string::npos
			|| lower(KeyBinder::FormatKeyToken(k.key)).find(needle) != std::string::npos) {
			return true;
		}
	}
	return false;
}

/*
 *  Writing.
 */

std::string Mui_keys_model::snapshot() const {
	std::ostringstream out;
	out << marker() << "\n";
	out << "# Written by Options > Input > Key bindings. This file replaces Exult's built-in keys,\n";
	out << "# so it lists every key. Edit it here or in the Options window.\n";
	for (const auto& e : cur) {
		const std::string token = KeyBinder::FormatKeyToken(e.key);
		if (token.empty()) {
			continue;
		}
		out << token << "\t" << e.action;
		const int n = std::max(e.nparams, trim_params(e.params, c_maxparams));
		for (int i = 0; i < n && i < c_maxparams; i++) {
			out << " " << e.params[i];
		}
		if (e.hidden) {
			out << "\t#-";
		} else if (!e.comment.empty()) {
			out << "\t# " << e.comment;
		}
		out << "\n";
	}
	return out.str();
}

std::string Mui_keys_model::own_path() const {
	std::string name = Game::get_gametitle();
	for (auto& c : name) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
			c = '_';
		}
	}
	return get_system_path("<SAVEGAME>/keys-" + name + ".txt");
}

std::string Mui_keys_model::configured_path() const {
	std::string v;
	config->value(config_key().c_str(), v, "(default)");
	return v;
}

Mui_keys_model::Apply_result Mui_keys_model::apply(bool takeover) {
	Apply_result res;
	res.path = own_path();
	if (!changed() || !keybinder) {
		return res;
	}
	const std::string cfg         = configured_path();
	const bool        cfg_default = cfg.empty() || cfg == "(default)";
	bool              cfg_own     = false;
	if (!cfg_default) {
		std::error_code ec;
		cfg_own = get_system_path(cfg) == res.path
				  || (U7exists(res.path) && U7exists(cfg) && std::filesystem::equivalent(get_system_path(cfg), res.path, ec));
	}
	if (is_default() && (cfg_default || cfg_own)) {
		// Back to Exult's own keys; any file of the editor stays, unused.
		config->set(config_key().c_str(), "(default)", true);
	} else {
		if (!cfg_default && !cfg_own && !takeover) {
			res.status  = Apply_status::needs_takeover;
			res.foreign = cfg;
			return res;
		}
		if (U7exists(res.path) && !has_marker_line(res.path)) {
			res.status  = Apply_status::refused;
			res.foreign = res.path;
			return res;
		}
		const std::string text = snapshot();
		// The text must read back as exactly the pending set.
		{
			KeyBinder check;
			check.LoadFromText(text, KeyBinder::Source::file);
			if (!same_set(user_entries(check.ListBindings()), cur)) {
				res.status = Apply_status::failed;
				return res;
			}
		}
		const std::string tmp = res.path + ".tmp";
		{
			auto out = U7open_out(tmp.c_str(), true);
			if (!out) {
				res.status = Apply_status::failed;
				return res;
			}
			*out << text;
			out->flush();
			if (!*out) {
				res.status = Apply_status::failed;
				return res;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tmp, res.path, ec);
		if (ec) {
			U7remove(tmp.c_str());
			res.status = Apply_status::failed;
			return res;
		}
		config->set(config_key().c_str(), res.path, true);
	}
	keybinder->LoadConfigured();
	// A held walk key's release no longer matches after a change.
	Game_window::get_instance()->stop_actor();
	load();
	res.status = Apply_status::ok;
	return res;
}

bool Mui_keys_model::same_bindings(const KeyBinder& a, const KeyBinder& b) {
	auto flatten = [](const KeyBinder& k) {
		std::map<std::pair<long, long>, std::string> m;
		for (const auto& x : k.ListBindings()) {
			if (!x.effective) {
				continue;
			}
			std::string v = x.action;
			for (int i = 0; i < c_maxparams; i++) {
				v += " " + std::to_string(x.params[i]);
			}
			m[{long(x.key.key), long(x.key.mod)}] = v;
		}
		return m;
	};
	return flatten(a) == flatten(b);
}
