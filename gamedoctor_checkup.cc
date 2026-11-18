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

/*
 *  The game doctor's check-up: data-driven rules (doctor_rules.txt) and
 *  the generic object tree checks.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "actors.h"
#include "chunks.h"
#include "contain.h"
#include "files/U7file.h"    // IWYU pragma: keep
#include "files/databuf.h"
#include "files/utils.h"
#include "fnames.h"
#include "game.h"
#include "gamedoctor.h"
#include "gamemap.h"
#include "gamewin.h"
#include "keyring.h"
#include "objiter.h"
#include "ucmachine.h"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {
	constexpr int max_nesting       = 16;
	constexpr int max_tree_findings = 50;

	std::string_view trim(std::string_view text) {
		const auto first = text.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	std::vector<std::string_view> split(std::string_view text, char sep, size_t max_parts) {
		std::vector<std::string_view> parts;
		while (parts.size() + 1 < max_parts) {
			const auto pos = text.find(sep);
			if (pos == std::string_view::npos) {
				break;
			}
			parts.push_back(text.substr(0, pos));
			text.remove_prefix(pos + 1);
		}
		parts.push_back(text);
		return parts;
	}

	std::vector<std::string_view> words(std::string_view text, const char* seps = " \t") {
		std::vector<std::string_view> out;
		while (!text.empty()) {
			const auto start = text.find_first_not_of(seps);
			if (start == std::string_view::npos) {
				break;
			}
			text.remove_prefix(start);
			const auto end = text.find_first_of(seps);
			out.push_back(text.substr(0, end));
			text.remove_prefix(end == std::string_view::npos ? text.size() : end);
		}
		return out;
	}

	std::optional<int> parse_int(std::string_view text) {
		text          = trim(text);
		bool negative = !text.empty() && text[0] == '-';
		if (negative) {
			text.remove_prefix(1);
		}
		int base = 10;
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			base = 16;
			text.remove_prefix(2);
		}
		int         value = 0;
		const auto* end   = text.data() + text.size();
		auto [ptr, ec]    = std::from_chars(text.data(), end, value, base);
		if (text.empty() || ec != std::errc() || ptr != end) {
			return std::nullopt;
		}
		return negative ? -value : value;
	}

	std::vector<int> parse_int_list(std::string_view text) {
		std::vector<int> out;
		for (const auto item : words(text, ",")) {
			if (const auto value = parse_int(item)) {
				out.push_back(*value);
			}
		}
		return out;
	}

	int severity_rank(const std::string& severity) {
		if (severity == "error") {
			return 0;
		}
		if (severity == "warning") {
			return 1;
		}
		return 2;
	}

	std::string tile_text(const Tile_coord& t) {
		return std::to_string(t.tx) + ',' + std::to_string(t.ty) + ',' + std::to_string(t.tz);
	}

	// Parses one rule file into 'rules'; same id replaces, 'id|off' removes.
	void parse_rules(std::istream& in, const std::string& file, std::vector<Game_doctor::Rule>& rules) {
		std::string line;
		int         linenum = 0;
		while (std::getline(in, line)) {
			++linenum;
			const auto text = trim(line);
			if (text.empty() || text[0] == '#') {
				continue;
			}
			const std::string where = file + ':' + std::to_string(linenum);
			const auto        parts = split(text, '|', 6);
			const std::string id(trim(parts[0]));
			auto              existing = std::find_if(rules.begin(), rules.end(), [&](const Game_doctor::Rule& r) {
                return r.id == id;
            });
			if (parts.size() == 2 && trim(parts[1]) == "off") {
				if (existing != rules.end()) {
					rules.erase(existing);
				}
				continue;
			}
			if (parts.size() != 6 || id.empty()) {
				std::cerr << "Game doctor: " << where << ": expected id|severity|kind|when|args|message" << std::endl;
				continue;
			}
			Game_doctor::Rule rule;
			rule.id       = id;
			rule.severity = std::string(trim(parts[1]));
			rule.kind     = std::string(trim(parts[2]));
			rule.message  = std::string(trim(parts[5]));
			rule.source   = where;
			if (rule.severity != "error" && rule.severity != "warning" && rule.severity != "info") {
				std::cerr << "Game doctor: " << where << ": unknown severity '" << rule.severity << "'" << std::endl;
				continue;
			}
			if (rule.kind != "item" && rule.kind != "npc" && rule.kind != "keys" && rule.kind != "orphans") {
				std::cerr << "Game doctor: " << where << ": unknown kind '" << rule.kind << "'" << std::endl;
				continue;
			}
			bool ok = true;
			for (const auto token : words(parts[3])) {
				if (token == "always") {
					continue;
				}
				if (token.size() < 2 || (token[0] != '+' && token[0] != '-')) {
					std::cerr << "Game doctor: " << where << ": bad flag test '" << token << "'" << std::endl;
					ok = false;
					break;
				}
				rule.when.push_back(Game_doctor::Rule::Flag_test{token[0] == '+', std::string(token.substr(1))});
			}
			for (const auto token : words(parts[4])) {
				const auto eq = token.find('=');
				if (eq == std::string_view::npos || eq == 0) {
					std::cerr << "Game doctor: " << where << ": bad argument '" << token << "'" << std::endl;
					ok = false;
					break;
				}
				rule.args.emplace_back(std::string(token.substr(0, eq)), std::string(token.substr(eq + 1)));
			}
			if (!ok) {
				continue;
			}
			if (existing != rules.end()) {
				*existing = std::move(rule);
			} else {
				rules.push_back(std::move(rule));
			}
		}
	}

	// Objects the rules need, gathered in one walk of the world.
	struct World_index {
		std::unordered_map<int, std::vector<Game_object*>> by_shape;
	};
}    // namespace

std::string Game_doctor::Rule::arg(std::string_view key, std::string_view fallback) const {
	for (const auto& [name, value] : args) {
		if (name == key) {
			return value;
		}
	}
	return std::string(fallback);
}

void Game_doctor::load_rules() {
	const std::string key = Game::get_gametitle() + '|' + Game::get_modtitle();
	if (key == rule_table_key) {
		return;
	}
	rule_table_key = key;
	rule_table.clear();
	if (game == nullptr) {
		return;
	}
	{
		const str_int_pair& resource = game->get_resource("files/doctor_rules");
		IExultDataSource    in(resource.str, resource.num);
		if (in.good()) {
			std::string text;
			in.read(text, in.getAvail());
			std::istringstream stream(text);
			parse_rules(stream, DOCTOR_RULES, rule_table);
		}
	}
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_DOCTOR_RULES)) {
		IFileDataSource in(PATCH_DOCTOR_RULES, true);
		if (in.good()) {
			std::string text;
			in.read(text, in.getAvail());
			std::istringstream stream(text);
			parse_rules(stream, PATCH_DOCTOR_RULES, rule_table);
		}
	}
}

const std::vector<Game_doctor::Rule>& Game_doctor::rules() {
	load_rules();
	return rule_table;
}

std::vector<int> Game_doctor::rule_shapes() {
	std::set<int> shapes;
	for (const auto& rule : rules()) {
		if (rule.kind == "item") {
			if (const auto shape = parse_int(rule.arg("shape"))) {
				shapes.insert(*shape);
			}
		} else if (rule.kind == "keys") {
			for (const auto list : {"key", "doors", "containers"}) {
				for (const int shape : parse_int_list(rule.arg(list))) {
					shapes.insert(shape);
				}
			}
		}
	}
	return std::vector<int>(shapes.begin(), shapes.end());
}

std::vector<Game_doctor::Finding> Game_doctor::run_checkup() {
	std::vector<Finding> findings;
	if (gwin == nullptr || gwin->get_main_actor() == nullptr) {
		return findings;
	}
	const auto& table = rules();

	// Which rules apply now? Unknown flag names are findings themselves.
	std::vector<const Rule*> active;
	for (const auto& rule : table) {
		bool holds = true;
		for (const auto& test : rule.when) {
			int num = -1;
			if (const auto value = parse_int(test.flag)) {
				num = *value;
			} else {
				num = flag_number(test.flag);
			}
			if (num < 0) {
				Finding f;
				f.id       = rule.id;
				f.severity = "warning";
				f.message  = "The rule at " + rule.source + " uses an unknown flag '" + test.flag + "'.";
				findings.push_back(std::move(f));
				holds = false;
				break;
			}
			if (flag_value(static_cast<unsigned>(num)) != test.set) {
				holds = false;
				break;
			}
		}
		if (holds) {
			active.push_back(&rule);
		}
	}

	// One walk for every shape the active rules count.
	World_index index;
	{
		std::unordered_set<int> wanted;
		for (const Rule* rule : active) {
			if (rule->kind == "item") {
				if (const auto shape = parse_int(rule->arg("shape"))) {
					wanted.insert(*shape);
				}
			} else if (rule->kind == "keys") {
				for (const auto list : {"key", "doors", "containers"}) {
					for (const int shape : parse_int_list(rule->arg(list))) {
						wanted.insert(shape);
					}
				}
			}
		}
		if (!wanted.empty()) {
			walk_world([&](Game_object* obj) {
				if (wanted.count(obj->get_shapenum()) != 0) {
					index.by_shape[obj->get_shapenum()].push_back(obj);
				}
			});
		}
	}

	auto add = [&](const Rule& rule, std::string message, const Game_object* where = nullptr, int npc = -1) {
		Finding f;
		f.id       = rule.id;
		f.severity = rule.severity;
		f.message  = std::move(message);
		f.npc      = npc;
		if (where != nullptr) {
			const Game_object* top = where->get_outermost();
			if (top->get_chunk() != nullptr) {
				f.tile = top->get_tile();
				f.map  = top->get_map_num();
			}
		}
		findings.push_back(std::move(f));
	};

	for (const Rule* rule : active) {
		if (rule->kind == "item") {
			const auto shape = parse_int(rule->arg("shape"));
			if (!shape) {
				add(*rule, "The rule at " + rule->source + " has no shape=.");
				continue;
			}
			const int frame   = parse_int(rule->arg("frame")).value_or(c_any_framenum);
			const int quality = parse_int(rule->arg("quality")).value_or(c_any_qual);
			const int min     = parse_int(rule->arg("min")).value_or(1);
			int       count   = 0;
			for (const Game_object* obj : index.by_shape[*shape]) {
				if ((frame == c_any_framenum || obj->get_framenum() == frame)
					&& (quality == c_any_qual || obj->get_quality() == quality)) {
					count += std::max(1, obj->get_quantity());
				}
			}
			if (count < min) {
				add(*rule, rule->message + " (" + std::to_string(count) + " found, " + std::to_string(min) + " needed)");
			}
		} else if (rule->kind == "npc") {
			const auto num = parse_int(rule->arg("npc"));
			if (!num) {
				add(*rule, "The rule at " + rule->source + " has no npc=.");
				continue;
			}
			Actor* npc = gwin->get_npc(*num);
			if (npc == nullptr) {
				add(*rule, rule->message + " (NPC " + std::to_string(*num) + " missing)", nullptr, *num);
			} else if (npc->is_dead()) {
				add(*rule, rule->message + " (" + object_name(npc) + " is dead)", gwin->get_body(*num), *num);
			}
		} else if (rule->kind == "keys") {
			const int           key_shape = parse_int(rule->arg("key", "641")).value_or(641);
			const auto          doors     = parse_int_list(rule->arg("doors"));
			const auto          chests    = parse_int_list(rule->arg("containers"));
			const int           locked    = parse_int(rule->arg("locked", "2")).value_or(2);
			const auto          ignore_v  = parse_int_list(rule->arg("ignore"));
			const std::set<int> ignore(ignore_v.begin(), ignore_v.end());
			std::set<int>       keys;
			for (const Game_object* key : index.by_shape[key_shape]) {
				keys.insert(key->get_quality());
			}
			if (const Keyring* ring = gwin->get_usecode()->getKeyring()) {
				keys.insert(ring->get_keys().begin(), ring->get_keys().end());
			}
			// Quality -> locked objects without a key.
			std::map<int, std::vector<const Game_object*>> missing;
			auto                                           check = [&](const Game_object* obj) {
                const int q = obj->get_quality();
                if (ignore.count(q) == 0 && keys.count(q) == 0) {
                    missing[q].push_back(obj);
                }
			};
			for (const int shape : doors) {
				for (const Game_object* obj : index.by_shape[shape]) {
					if (obj->get_framenum() % 4 == locked) {
						check(obj);
					}
				}
			}
			for (const int shape : chests) {
				for (const Game_object* obj : index.by_shape[shape]) {
					check(obj);
				}
			}
			for (const auto& [quality, objs] : missing) {
				const Game_object* first = objs.front();
				const Game_object* top   = first->get_outermost();
				std::string text = rule->message + ": key quality " + std::to_string(quality) + ", " + std::to_string(objs.size())
								   + " locked object(s), first " + object_name(first);
				if (top->get_chunk() != nullptr) {
					text += " at " + tile_text(top->get_tile()) + " map " + std::to_string(top->get_map_num());
				}
				add(*rule, text, first);
			}
		} else if (rule->kind == "orphans") {
			// NPCs whose bodies the game itself places in the map while they live.
			const auto          body_ignore_v = parse_int_list(rule->arg("ignore"));
			const std::set<int> body_ignore(body_ignore_v.begin(), body_ignore_v.end());
			auto                living_body = [&](const Dead_body* body) -> Actor* {
                const int num = body->get_live_npc_num();
                Actor*    npc = num > 0 && body_ignore.count(num) == 0 ? gwin->get_npc(num) : nullptr;
                return npc != nullptr && !npc->is_dead() ? npc : nullptr;
			};
			int  count = 0;
			auto tree  = [&](const Game_object* where, const std::string& what) {
                if (++count <= max_tree_findings) {
                    add(*rule, rule->message + ": " + what, where);
                }
			};
			std::unordered_set<const Game_object*> seen;
			std::vector<const Game_object*>        path;
			// Checks the contents of a container, recursively.
			std::function<void(Container_game_object*, int)> walk = [&](Container_game_object* cont, int depth) {
				if (depth > max_nesting) {
					tree(cont, object_name(cont) + " is nested more than " + std::to_string(max_nesting) + " deep");
					return;
				}
				path.push_back(cont);
				Object_iterator next(cont->get_objects());
				while (Game_object* obj = next.get_next()) {
					if (std::find(path.begin(), path.end(), obj) != path.end()) {
						tree(cont, object_name(cont) + " contains itself (a cycle)");
						continue;
					}
					if (obj->get_owner() != cont) {
						tree(cont, object_name(obj) + " is in " + object_name(cont) + ", which is not its owner");
					}
					if (!seen.insert(obj).second) {
						continue;
					}
					if (const Dead_body* body = obj->as_body()) {
						if (Actor* npc = living_body(body)) {
							tree(obj, "a body of " + object_name(npc) + ", who is alive");
						}
					}
					if (Container_game_object* inner = obj->as_container()) {
						walk(inner, depth + 1);
					}
				}
				path.pop_back();
			};
			auto top_level = [&](Game_object* obj) {
				if (!seen.insert(obj).second) {
					return;
				}
				if (Container_game_object* owner = obj->get_owner()) {
					tree(obj, object_name(obj) + " is on the map but owned by " + object_name(owner));
				}
				const Tile_coord t = obj->get_tile();
				if (obj->get_chunk() != nullptr
					&& (t.tx < 0 || t.ty < 0 || t.tx >= c_num_tiles || t.ty >= c_num_tiles || t.tz < 0)) {
					tree(obj, object_name(obj) + " is outside the map at " + tile_text(t));
				}
				if (const Dead_body* body = obj->as_body()) {
					if (Actor* npc = living_body(body)) {
						tree(obj, "a body of " + object_name(npc) + ", who is alive");
					}
				}
				if (Container_game_object* cont = obj->as_container()) {
					walk(cont, 1);
				}
			};
			scan_world();
			for (Game_map* map : gwin->get_maps()) {
				if (map == nullptr) {
					continue;
				}
				for (int cy = 0; cy < c_num_chunks; cy++) {
					for (int cx = 0; cx < c_num_chunks; cx++) {
						if (Map_chunk* chunk = map->get_chunk_unsafe(cx, cy)) {
							Object_iterator next(chunk->get_objects());
							while (Game_object* obj = next.get_next()) {
								top_level(obj);
							}
						}
					}
				}
			}
			// NPCs who are not on the map still own their inventories.
			const int npcs = gwin->get_num_npcs();
			for (int i = 0; i < npcs; i++) {
				Actor* npc = gwin->get_npc(i);
				if (npc != nullptr && npc->get_chunk() == nullptr && npc->get_owner() == nullptr) {
					top_level(npc);
				}
			}
			if (count > max_tree_findings) {
				add(*rule, rule->message + ": " + std::to_string(count - max_tree_findings) + " more problem(s) not listed");
			}
		}
	}

	std::stable_sort(findings.begin(), findings.end(), [](const Finding& a, const Finding& b) {
		const int ra = severity_rank(a.severity);
		const int rb = severity_rank(b.severity);
		return ra != rb ? ra < rb : a.id < b.id;
	});
	return findings;
}
