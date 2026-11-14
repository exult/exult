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

#include "gamedoctor.h"

#include "actors.h"
#include "chunks.h"
#include "contain.h"
#include "files/U7file.h"    // IWYU pragma: keep
#include "files/databuf.h"
#include "files/msgfile.h"
#include "files/utils.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "keyring.h"
#include "objiter.h"
#include "party.h"
#include "schedule.h"
#include "ucmachine.h"
#include "version.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <exception>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <streambuf>
#include <unordered_set>

namespace {
	std::string lower_case(std::string_view text) {
		std::string out(text);
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return out;
	}

	bool iequals(std::string_view a, std::string_view b) {
		return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
				   return std::tolower(x) == std::tolower(y);
			   });
	}

	// Parses a whole decimal or 0x hex number.
	std::optional<unsigned> parse_number(std::string_view text) {
		int base = 10;
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			base = 16;
			text.remove_prefix(2);
		}
		unsigned    value = 0;
		const auto* end   = text.data() + text.size();
		auto [ptr, ec]    = std::from_chars(text.data(), end, value, base);
		if (text.empty() || ec != std::errc() || ptr != end) {
			return std::nullopt;
		}
		return value;
	}

	// Name and description strings of one flag table, by number.
	struct Flag_strings {
		std::vector<std::optional<std::string>> names;
		std::vector<std::optional<std::string>> descriptions;
	};

	Flag_strings read_flag_table(IDataSource& in) {
		Flag_strings               out;
		const Text_msg_file_reader reader(in);
		reader.get_global_section_strings(out.names);
		reader.get_section_strings("descriptions", out.descriptions);
		return out;
	}
}    // namespace

/*
 *  A stream buffer that writes through to the original buffer of
 *  std::cout or std::cerr and hands each complete line to the doctor.
 */
class Log_tee_buf : public std::streambuf {
public:
	Log_tee_buf(std::ostream& stream, Game_doctor& doctor) : stream(stream), original(stream.rdbuf()), doctor(doctor) {
		stream.rdbuf(this);
	}

	~Log_tee_buf() override {
		if (stream.rdbuf() == this) {
			stream.rdbuf(original);
		}
		if (!partial.empty()) {
			doctor.add_log_line(std::move(partial));
		}
	}

	Log_tee_buf(const Log_tee_buf&)            = delete;
	Log_tee_buf& operator=(const Log_tee_buf&) = delete;

protected:
	int_type overflow(int_type ch) override {
		if (traits_type::eq_int_type(ch, traits_type::eof())) {
			return traits_type::not_eof(ch);
		}
		const char c = traits_type::to_char_type(ch);
		collect(&c, 1);
		return original->sputc(c);
	}

	std::streamsize xsputn(const char* s, std::streamsize n) override {
		collect(s, n);
		return original->sputn(s, n);
	}

	int sync() override {
		return original->pubsync();
	}

private:
	void collect(const char* s, std::streamsize n) {
		for (std::streamsize i = 0; i < n; i++) {
			if (s[i] == '\n') {
				if (!partial.empty() && partial.back() == '\r') {
					partial.pop_back();
				}
				doctor.add_log_line(std::move(partial));
				partial.clear();
			} else {
				partial += s[i];
			}
		}
	}

	std::ostream&   stream;
	std::streambuf* original;
	Game_doctor&    doctor;
	std::string     partial;
};

Game_doctor& Game_doctor::get() {
	static Game_doctor doctor;
	return doctor;
}

Game_doctor::~Game_doctor() {
	remove_log_capture();
}

//
// Named flags.
//

void Game_doctor::load_flags() {
	const std::string key = Game::get_gametitle() + '|' + Game::get_modtitle();
	if (key == flag_table_key && !flag_table.empty()) {
		return;
	}
	flag_table_key = key;
	flag_table.clear();
	if (game == nullptr) {
		return;
	}
	Flag_strings base;
	{
		const str_int_pair& resource = game->get_resource("files/global_flags");
		IExultDataSource    in(resource.str, resource.num);
		if (in.good()) {
			base = read_flag_table(in);
		}
	}
	Flag_strings patch;
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_GLOBAL_FLAGS)) {
		IFileDataSource in(PATCH_GLOBAL_FLAGS, true);
		if (in.good()) {
			patch = read_flag_table(in);
		}
	}
	auto at = [](const std::vector<std::optional<std::string>>& v, size_t i) {
		return i < v.size() && v[i] ? *v[i] : std::string();
	};
	const size_t count = std::max({base.names.size(), base.descriptions.size(), patch.names.size(), patch.descriptions.size()});
	for (size_t i = 0; i < count; i++) {
		Flag_info info;
		info.num                    = static_cast<unsigned>(i);
		const std::string base_name = at(base.names, i);
		info.name                   = at(patch.names, i);
		if (info.name.empty()) {
			info.name = base_name;
		} else if (!base_name.empty() && base_name != info.name) {
			info.alias = base_name;
		}
		info.description = at(patch.descriptions, i);
		if (info.description.empty()) {
			info.description = at(base.descriptions, i);
		}
		if (!info.name.empty() || !info.description.empty()) {
			flag_table.push_back(std::move(info));
		}
	}
}

const std::vector<Game_doctor::Flag_info>& Game_doctor::flags() {
	load_flags();
	return flag_table;
}

const Game_doctor::Flag_info* Game_doctor::find_flag(unsigned num) {
	const auto& table = flags();
	auto        it    = std::lower_bound(table.begin(), table.end(), num, [](const Flag_info& f, unsigned n) {
        return f.num < n;
    });
	return it != table.end() && it->num == num ? &*it : nullptr;
}

int Game_doctor::flag_number(std::string_view name) {
	if (name.empty()) {
		return -1;
	}
	for (const auto& info : flags()) {
		if (iequals(info.name, name) || (!info.alias.empty() && iequals(info.alias, name))) {
			return static_cast<int>(info.num);
		}
	}
	return -1;
}

std::vector<Game_doctor::Flag_info> Game_doctor::search_flags(std::string_view text) {
	std::vector<Flag_info> out;
	const auto&            table  = flags();
	const auto             number = parse_number(text);
	const std::string      needle = lower_case(text);
	for (const auto& info : table) {
		if (needle.empty() || (number && info.num == *number) || lower_case(info.name).find(needle) != std::string::npos
			|| lower_case(info.alias).find(needle) != std::string::npos
			|| lower_case(info.description).find(needle) != std::string::npos) {
			out.push_back(info);
		}
	}
	// A number without a name still finds its flag.
	if (number && find_flag(*number) == nullptr) {
		auto* usecode = gwin != nullptr ? gwin->get_usecode() : nullptr;
		if (usecode != nullptr && *number < Usecode_machine::max_num_gflags) {
			Flag_info info;
			info.num = *number;
			out.insert(
					std::upper_bound(
							out.begin(), out.end(), info.num,
							[](unsigned n, const Flag_info& f) {
								return n < f.num;
							}),
					info);
		}
	}
	return out;
}

bool Game_doctor::flag_value(unsigned num) const {
	auto* usecode = gwin != nullptr ? gwin->get_usecode() : nullptr;
	return usecode != nullptr && usecode->get_global_flag_bool(static_cast<int>(num));
}

bool Game_doctor::set_flag(unsigned num, bool on) {
	auto* usecode = gwin != nullptr ? gwin->get_usecode() : nullptr;
	return usecode != nullptr && usecode->set_global_flag(static_cast<int>(num), on);
}

//
// Log capture.
//

void Game_doctor::install_log_capture() {
	if (!cout_tee) {
		cout_tee = std::make_unique<Log_tee_buf>(std::cout, *this);
	}
	if (!cerr_tee) {
		cerr_tee = std::make_unique<Log_tee_buf>(std::cerr, *this);
	}
}

void Game_doctor::remove_log_capture() {
	std::cout.flush();
	std::cerr.flush();
	cout_tee.reset();
	cerr_tee.reset();
}

void Game_doctor::add_log_line(std::string line) {
	const std::lock_guard<std::mutex> lock(log_mutex);
	log_lines.push_back(std::move(line));
	while (log_lines.size() > log_capacity) {
		log_lines.pop_front();
	}
}

std::vector<std::string> Game_doctor::recent_log(size_t n) const {
	const std::lock_guard<std::mutex> lock(log_mutex);
	const size_t                      count = std::min(n, log_lines.size());
	return std::vector<std::string>(log_lines.end() - static_cast<std::ptrdiff_t>(count), log_lines.end());
}

//
// Names.
//

std::string Game_doctor::schedule_name(int type) {
	// In the order of Schedule::Schedule_types.
	static const char* const names[]
			= {"combat",
			   "horiz_pace",
			   "vert_pace",
			   "talk",
			   "dance",
			   "eat",
			   "farm",
			   "tend_shop",
			   "miner",
			   "hound",
			   "stand",
			   "loiter",
			   "wander",
			   "blacksmith",
			   "sleep",
			   "wait",
			   "sit",
			   "graze",
			   "bake",
			   "sew",
			   "shy",
			   "lab",
			   "thief",
			   "waiter",
			   "special",
			   "kid_games",
			   "eat_at_inn",
			   "duel",
			   "preach",
			   "patrol",
			   "desk_work",
			   "follow_avatar",
			   "walk_to_schedule",
			   "street_maintenance",
			   "arrest_avatar"};
	static_assert(std::size(names) == Schedule::arrest_avatar + 1, "schedule names out of step");
	if (type >= 0 && static_cast<size_t>(type) < std::size(names)) {
		return names[type];
	}
	if (type >= Schedule::first_scripted_schedule) {
		return "scripted_" + std::to_string(type - Schedule::first_scripted_schedule);
	}
	return "schedule_" + std::to_string(type);
}

std::string Game_doctor::alignment_name(int align) {
	switch (align) {
	case Actor::neutral:
		return "neutral";
	case Actor::good:
		return "good";
	case Actor::evil:
		return "evil";
	case Actor::chaotic:
		return "chaotic";
	default:
		return "alignment_" + std::to_string(align);
	}
}

std::string Game_doctor::object_name(const Game_object* obj) {
	if (obj == nullptr) {
		return std::string();
	}
	if (const Actor* npc = obj->as_actor()) {
		const int num = npc->get_npc_num();
		return npc->get_npc_name() + (num >= 0 ? " (NPC " + std::to_string(num) + ")" : std::string());
	}
	std::string name = obj->get_name();
	if (name.empty()) {
		name = "shape " + std::to_string(obj->get_shapenum());
	}
	return name;
}

std::vector<std::string> Game_doctor::owner_chain(const Game_object* obj) {
	std::vector<std::string> chain;
	const Game_object*       top = obj;
	// Guard against corrupt (cyclic) ownership.
	for (int depth = 0; depth < 64 && top != nullptr && top->get_owner() != nullptr; depth++) {
		top = top->get_owner();
		chain.push_back(object_name(top));
	}
	if (top != nullptr && top->as_actor() == nullptr) {
		chain.emplace_back("ground");
	}
	return chain;
}

//
// NPC browser.
//

namespace {
	Actor* doctor_npc(int num) {
		Game_window* win = Game_window::get_instance();
		return win != nullptr ? win->get_npc(num) : nullptr;
	}

	std::string npc_label(const Actor* npc) {
		return Game_doctor::object_name(npc);
	}

	Game_doctor::Action_result fail(std::string message) {
		return Game_doctor::Action_result{false, std::move(message)};
	}

	Game_doctor::Action_result done(std::string message) {
		return Game_doctor::Action_result{true, std::move(message)};
	}

	// A free spot beside the Avatar for an object, or an invalid tile.
	Tile_coord spot_beside_avatar(Game_object* obj) {
		Game_window* win = Game_window::get_instance();
		Actor*       av  = win->get_main_actor();
		if (av == nullptr || av->get_chunk() == nullptr) {
			return Tile_coord(-1, -1, -1);
		}
		const Tile_coord pos = av->get_tile();
		for (int dist = 1; dist <= 6; dist++) {
			const Tile_coord spot = Map_chunk::find_spot(pos, dist, obj->get_shapenum(), obj->get_framenum(), 1);
			if (spot.tx >= 0 && spot != pos) {
				return spot;
			}
		}
		return Tile_coord(-1, -1, -1);
	}

	// Puts an object that is not in the world on the ground beside the
	// Avatar. Returns false if there is no free spot.
	bool place_beside_avatar(Game_object* obj) {
		Game_window*     win  = Game_window::get_instance();
		const Tile_coord spot = spot_beside_avatar(obj);
		if (spot.tx < 0) {
			return false;
		}
		obj->move(spot.tx, spot.ty, spot.tz, win->get_main_actor()->get_map_num());
		return true;
	}

	// Takes an object out of its container or chunk, keeping it alive.
	Game_object_shared take_out(Game_object* obj) {
		Game_object_shared keep = obj->shared_from_this();
		obj->remove_this(&keep);
		obj->set_invalid();
		return keep;
	}
}    // namespace

Game_doctor::Npc_info Game_doctor::npc_info(int num) const {
	Npc_info info;
	Actor*   npc = doctor_npc(num);
	if (npc == nullptr) {
		return info;
	}
	info.num      = num;
	info.name     = npc->get_npc_name();
	info.alive    = !npc->is_dead();
	info.in_party = npc->is_in_party();
	info.on_map   = npc->get_chunk() != nullptr;
	if (info.on_map) {
		info.tile = npc->get_tile();
		info.map  = npc->get_map_num();
	}
	if (!info.alive && gwin->get_body(num) == nullptr) {
		// A body is only known once its superchunk has been read.
		scan_world();
	}
	if (const Game_object* body = gwin->get_body(num)) {
		info.has_body          = true;
		const Game_object* top = body->get_outermost();
		info.body_tile         = top->get_tile();
		info.body_map          = top->get_map_num();
		for (const auto& owner : owner_chain(body)) {
			info.body_where += info.body_where.empty() ? owner : " > " + owner;
		}
	}
	info.schedule                                         = npc->get_schedule_type();
	info.schedule_name                                    = schedule_name(info.schedule);
	info.alignment                                        = npc->get_alignment();
	info.alignment_name                                   = alignment_name(info.alignment);
	static const std::pair<int, const char*> flag_names[] = {
			{   Obj_flags::asleep,    "asleep"},
			{  Obj_flags::charmed,   "charmed"},
			{   Obj_flags::cursed,    "cursed"},
			{Obj_flags::invisible, "invisible"},
			{Obj_flags::paralyzed, "paralysed"},
			{ Obj_flags::poisoned,  "poisoned"},
			{      Obj_flags::met,       "met"}
    };
	if (info.in_party) {
		info.flags.emplace_back("in party");
	}
	for (const auto& [flag, name] : flag_names) {
		if (npc->get_flag(flag)) {
			info.flags.emplace_back(name);
		}
	}
	info.health   = npc->get_property(Actor::health);
	info.strength = npc->get_property(Actor::strength);
	Recursive_object_iterator next(npc->get_objects());
	while (Game_object* item = next.get_next()) {
		info.inventory.push_back(
				Inventory_row{
						item->get_shapenum(), item->get_framenum(), item->get_quality(), item->get_quantity(), object_name(item)});
	}
	return info;
}

std::vector<int> Game_doctor::find_npcs(std::string_view text) const {
	std::vector<int> out;
	if (gwin == nullptr) {
		return out;
	}
	const auto        number = parse_number(text);
	const std::string needle = lower_case(text);
	const int         count  = gwin->get_num_npcs();
	for (int i = 0; i < count; i++) {
		const Actor* npc = gwin->get_npc(i);
		if (npc == nullptr) {
			continue;
		}
		if (needle.empty() || (number && static_cast<int>(*number) == i)
			|| lower_case(npc->get_npc_name()).find(needle) != std::string::npos) {
			out.push_back(i);
		}
	}
	return out;
}

Game_doctor::Action_result Game_doctor::bring_npc(int num) {
	Actor* npc = doctor_npc(num);
	if (npc == nullptr || num == 0) {
		return fail("No such NPC.");
	}
	if (npc->is_dead()) {
		return fail(npc_label(npc) + " is dead: revive them first.");
	}
	const Tile_coord spot = spot_beside_avatar(npc);
	if (spot.tx < 0) {
		return fail("There is no free spot beside the Avatar.");
	}
	npc->move(spot.tx, spot.ty, spot.tz, gwin->get_main_actor()->get_map_num());
	return done("Brought " + npc_label(npc) + " beside the Avatar.");
}

Game_doctor::Action_result Game_doctor::goto_npc(int num) {
	Actor* npc = doctor_npc(num);
	if (npc == nullptr || num == 0) {
		return fail("No such NPC.");
	}
	const Game_object* target = npc;
	if (npc->is_dead() && gwin->get_body(num) == nullptr) {
		scan_world();    // The body's superchunk may not be read yet.
	}
	if (npc->is_dead() && gwin->get_body(num) != nullptr) {
		target = gwin->get_body(num)->get_outermost();
	}
	if (target->get_chunk() == nullptr) {
		return fail(npc_label(npc) + " is not on the map.");
	}
	gwin->teleport_party(target->get_tile(), false, target->get_map_num());
	return done("Went to " + npc_label(npc) + ".");
}

Game_doctor::Action_result Game_doctor::revive_npc(int num) {
	Actor* npc = doctor_npc(num);
	if (npc == nullptr) {
		return fail("No such NPC.");
	}
	if (num == 0) {
		return fail("The Avatar cannot be revived from here.");
	}
	if (!npc->is_dead()) {
		return fail(npc_label(npc) + " is not dead.");
	}
	if (gwin->get_body(num) == nullptr) {
		scan_world();    // The body's superchunk may not be read yet.
	}
	Dead_body* body = gwin->get_body(num);
	if (body != nullptr && body->get_owner() != nullptr) {
		// Actor::resurrect() needs the body on the ground.
		const Tile_coord spot = spot_beside_avatar(body);
		if (spot.tx < 0) {
			return fail("There is no free spot beside the Avatar for the body.");
		}
		const Game_object_shared keep = take_out(body);
		body->move(spot.tx, spot.ty, spot.tz, gwin->get_main_actor()->get_map_num());
	}
	if (npc->resurrect(body) == nullptr) {
		return fail("Could not revive " + npc_label(npc) + ".");
	}
	if (body == nullptr && !place_beside_avatar(npc)) {
		return fail("Revived " + npc_label(npc) + ", but there is no free spot beside the Avatar.");
	}
	npc->set_alignment(Actor::neutral);
	npc->reset_effective_alignment();
	if (!npc->is_in_party()) {
		gwin->revert_schedules(npc);
		npc->update_schedule(gwin->get_clock()->get_hour() / 3);
	}
	return done("Revived " + npc_label(npc) + ".");
}

Game_doctor::Action_result Game_doctor::restore_schedule(int num) {
	Actor* npc = doctor_npc(num);
	if (npc == nullptr || num == 0) {
		return fail("No such NPC.");
	}
	if (npc->is_dead()) {
		return fail(npc_label(npc) + " is dead.");
	}
	if (npc->is_in_party()) {
		return fail(npc_label(npc) + " is in the party.");
	}
	gwin->revert_schedules(npc);
	npc->update_schedule(gwin->get_clock()->get_hour() / 3);
	return done("Restored the schedule of " + npc_label(npc) + ": " + schedule_name(npc->get_schedule_type()) + ".");
}

Game_doctor::Action_result Game_doctor::heal_npc(int num) {
	Actor* npc = doctor_npc(num);
	if (npc == nullptr) {
		return fail("No such NPC.");
	}
	if (npc->is_dead()) {
		return fail(npc_label(npc) + " is dead: revive them first.");
	}
	npc->set_property(Actor::health, npc->get_property(Actor::strength));
	npc->clear_flag(Obj_flags::poisoned);
	npc->clear_flag(Obj_flags::paralyzed);
	return done("Healed " + npc_label(npc) + ".");
}

//
// Item finder.
//

void Game_doctor::scan_world() const {
	if (gwin == nullptr) {
		return;
	}
	for (Game_map* map : gwin->get_maps()) {
		if (map == nullptr) {
			continue;
		}
		for (int cy = 0; cy < c_num_chunks; cy += c_chunks_per_schunk) {
			for (int cx = 0; cx < c_num_chunks; cx += c_chunks_per_schunk) {
				map->ensure_chunk_read(cx, cy);
			}
		}
	}
}

void Game_doctor::walk_world(const std::function<void(Game_object*)>& visit) {
	if (gwin == nullptr) {
		return;
	}
	scan_world();
	std::unordered_set<const Game_object*> seen;
	auto                                   walk_list = [&](Object_list& list) {
        Recursive_object_iterator next(list);
        while (Game_object* obj = next.get_next()) {
            if (seen.insert(obj).second) {
                visit(obj);
            }
        }
	};
	for (Game_map* map : gwin->get_maps()) {
		if (map == nullptr) {
			continue;
		}
		for (int cy = 0; cy < c_num_chunks; cy++) {
			for (int cx = 0; cx < c_num_chunks; cx++) {
				if (Map_chunk* chunk = map->get_chunk_unsafe(cx, cy)) {
					walk_list(chunk->get_objects());
				}
			}
		}
	}
	// NPCs off the map (and anything the chunks missed).
	const int count = gwin->get_num_npcs();
	for (int i = 0; i < count; i++) {
		Actor* npc = gwin->get_npc(i);
		if (npc == nullptr) {
			continue;
		}
		if (seen.insert(npc).second) {
			visit(npc);
		}
		walk_list(npc->get_objects());
	}
}

std::vector<Game_doctor::Item_hit> Game_doctor::find_items(const Item_query& query, size_t limit) {
	std::vector<Item_hit> hits;
	const std::string     needle = lower_case(query.name);
	walk_world([&](Game_object* obj) {
		if (hits.size() >= limit || obj->as_actor() != nullptr) {
			return;
		}
		if ((query.shape != c_any_shapenum && obj->get_shapenum() != query.shape)
			|| (query.frame != c_any_framenum && obj->get_framenum() != query.frame)
			|| (query.quality != c_any_qual && obj->get_quality() != query.quality)) {
			return;
		}
		std::string name = object_name(obj);
		if (!needle.empty() && lower_case(name).find(needle) == std::string::npos) {
			return;
		}
		Item_hit hit;
		hit.obj                = obj->weak_from_this();
		hit.shape              = obj->get_shapenum();
		hit.frame              = obj->get_framenum();
		hit.quality            = obj->get_quality();
		hit.quantity           = obj->get_quantity();
		hit.name               = std::move(name);
		const Game_object* top = obj->get_outermost();
		if (top->get_chunk() != nullptr) {
			hit.tile = top->get_tile();
			hit.map  = top->get_map_num();
		} else {
			hit.tile = Tile_coord(-1, -1, -1);
		}
		hit.chain = owner_chain(obj);
		hits.push_back(std::move(hit));
	});
	return hits;
}

Game_doctor::Action_result Game_doctor::goto_item(const Item_hit& hit) {
	const Game_object_shared obj = hit.obj.lock();
	if (!obj) {
		return fail("That item no longer exists.");
	}
	const Game_object* top = obj->get_outermost();
	if (top->get_chunk() == nullptr) {
		return fail("That item is not on the map.");
	}
	gwin->teleport_party(top->get_tile(), false, top->get_map_num());
	return done("Went to " + object_name(obj.get()) + ".");
}

Game_doctor::Action_result Game_doctor::fetch_item(const Item_hit& hit) {
	const Game_object_shared obj = hit.obj.lock();
	Actor*                   av  = gwin != nullptr ? gwin->get_main_actor() : nullptr;
	if (!obj || av == nullptr) {
		return fail("That item no longer exists.");
	}
	if (obj->as_actor() != nullptr) {
		return fail("Use the NPC browser to move NPCs.");
	}
	if (obj->get_outermost() == av) {
		return fail("The Avatar already has it.");
	}
	if (obj->get_owner() == nullptr && !obj->is_dragable()) {
		return fail(object_name(obj.get()) + " is fixed in place.");
	}
	const std::string  name = object_name(obj.get());
	Game_object_shared keep = take_out(obj.get());
	if (av->add(obj.get(), false, true)) {
		return done("Put " + name + " in the Avatar's pack.");
	}
	if (place_beside_avatar(obj.get())) {
		return done("Dropped " + name + " at the Avatar's feet.");
	}
	// Nowhere to go: put it at the Avatar's tile.
	const Tile_coord pos = av->get_tile();
	obj->move(pos.tx, pos.ty, pos.tz, av->get_map_num());
	return done("Dropped " + name + " on the Avatar's tile.");
}

//
// State dump.
//

namespace {
	// Standard CRC-32 (as zlib's crc32()), so tools can check it.
	uint32 standard_crc32(const unsigned char* data, size_t len) {
		static const auto table = [] {
			std::array<uint32, 256> t{};
			for (uint32 i = 0; i < 256; i++) {
				uint32 c = i;
				for (int k = 0; k < 8; k++) {
					c = (c & 1) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
				}
				t[i] = c;
			}
			return t;
		}();
		uint32 crc = 0xFFFFFFFFU;
		for (size_t i = 0; i < len; i++) {
			crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
		}
		return crc ^ 0xFFFFFFFFU;
	}

	std::string hex(unsigned value, int width) {
		std::ostringstream out;
		out << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
		return out.str();
	}

	std::string dec(int value, int width) {
		std::ostringstream out;
		out << std::setfill('0') << std::setw(width) << value;
		return out.str();
	}

	std::string place_text(const Tile_coord& t, int map) {
		return std::to_string(t.tx) + ',' + std::to_string(t.ty) + ',' + std::to_string(t.tz) + " map " + std::to_string(map);
	}

	// Where an object is: its outermost owner's tile, or the NPC holding it.
	std::string object_place(const Game_object* obj) {
		const Game_object* top = obj->get_outermost();
		if (top->get_chunk() != nullptr) {
			return place_text(top->get_tile(), top->get_map_num());
		}
		if (const Actor* npc = top->as_actor()) {
			return "npc " + std::to_string(npc->get_npc_num()) + " not on map";
		}
		return "not on map";
	}

	// One-line text without line breaks (names come from game data).
	std::string one_line(std::string text) {
		std::replace(text.begin(), text.end(), '\n', ' ');
		std::replace(text.begin(), text.end(), '\r', ' ');
		return text;
	}
}    // namespace

std::string Game_doctor::dump_text(bool with_checkup) {
	std::ostringstream out;
	out << "# exult-doctor-dump 1\n";
	{
		std::ostringstream version;
		getVersionInfo(version);
		std::string first;
		std::getline(std::istringstream(version.str()), first);
		out << "meta.exult: " << one_line(first) << '\n';
		out << "meta.git: " << one_line(VersionGetGitInfo(true)) << '\n';
	}
	out << "meta.game: " << Game::get_gametitle() << '\n';
	out << "meta.mod: " << Game::get_modtitle() << '\n';
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_USECODE)) {
		IFileDataSource in(PATCH_USECODE);
		if (in.good()) {
			std::string data;
			in.read(data, in.getAvail());
			out << "meta.usecode_crc32: 0x"
				<< hex(standard_crc32(reinterpret_cast<const unsigned char*>(data.data()), data.size()), 8) << '\n';
			out << "meta.usecode_size: " << data.size() << '\n';
		}
	}
	if (gwin == nullptr || gwin->get_main_actor() == nullptr) {
		out << "# no game loaded\n";
		return out.str();
	}
	const Game_clock* clock = gwin->get_clock();
	out << "clock: day " << clock->get_day() << ' ' << dec(clock->get_hour(), 2) << ':' << dec(clock->get_minute(), 2) << '\n';
	const Actor* av = gwin->get_main_actor();
	out << "avatar: " << (av->get_chunk() != nullptr ? place_text(av->get_tile(), av->get_map_num()) : "not on map") << '\n';
	out << "party:";
	for (int i = 0; i < partyman->get_count(); i++) {
		const int    num = partyman->get_member(i);
		const Actor* npc = gwin->get_npc(num);
		out << (i == 0 ? " " : ", ") << num << ' ' << (npc != nullptr ? one_line(npc->get_npc_name()) : std::string("?"));
	}
	out << '\n';

	// Set flags only.
	Usecode_machine* usecode = gwin->get_usecode();
	const size_t     nflags  = usecode->get_num_global_flags();
	for (size_t i = 0; i < nflags; i++) {
		if (usecode->get_global_flag_bool(static_cast<int>(i))) {
			const Flag_info* info = find_flag(static_cast<unsigned>(i));
			out << "flag.0x" << hex(static_cast<unsigned>(i), 4) << ": 1";
			if (info != nullptr && !info->name.empty()) {
				out << ' ' << info->name;
			}
			out << '\n';
		}
	}
	if (GAME_SI) {
		out << "keyring:";
		if (const Keyring* ring = usecode->getKeyring()) {
			for (const int q : ring->get_keys()) {
				out << ' ' << q;
			}
		}
		out << '\n';
	}

	// NPCs.
	const int count = gwin->get_num_npcs();
	for (int i = 0; i < count; i++) {
		if (gwin->get_npc(i) == nullptr) {
			continue;
		}
		const Npc_info    info = npc_info(i);
		const std::string key  = "npc." + dec(i, 4);
		out << key << ": " << (info.alive ? "alive" : "dead") << " | " << one_line(info.name) << " | ";
		if (info.on_map) {
			out << place_text(info.tile, info.map);
		} else if (info.has_body) {
			out << "body " << place_text(info.body_tile, info.body_map);
		} else {
			out << "not on map";
		}
		out << " | " << info.schedule_name << " | " << info.alignment_name << " | ";
		if (info.flags.empty()) {
			out << '-';
		}
		for (size_t f = 0; f < info.flags.size(); f++) {
			out << (f != 0 ? "," : "") << info.flags[f];
		}
		out << " | hp " << info.health << '/' << info.strength << " | items " << info.inventory.size() << '\n';
		if (!info.inventory.empty()) {
			std::vector<std::string> items;
			for (const auto& row : info.inventory) {
				items.push_back(
						std::to_string(row.shape) + ':' + std::to_string(row.frame) + ':' + std::to_string(row.quality) + 'x'
						+ std::to_string(row.quantity));
			}
			std::sort(items.begin(), items.end());
			out << key << ".items:";
			for (const auto& item : items) {
				out << ' ' << item;
			}
			out << '\n';
		}
	}

	// Shapes the check-up rules name.
	const std::vector<int> shapes = rule_shapes();
	if (!shapes.empty()) {
		std::map<int, std::vector<std::string>> places;
		for (const int shape : shapes) {
			places[shape];
		}
		walk_world([&](Game_object* obj) {
			auto it = places.find(obj->get_shapenum());
			if (it != places.end() && obj->as_actor() == nullptr) {
				it->second.push_back(object_place(obj));
			}
		});
		for (auto& [shape, where] : places) {
			std::sort(where.begin(), where.end());
			out << "item." << dec(shape, 4) << ": " << where.size();
			for (size_t i = 0; i < where.size() && i < 20; i++) {
				out << (i == 0 ? " | " : "; ") << where[i];
			}
			out << '\n';
		}
	}

	if (with_checkup) {
		std::map<std::string, int> seen;
		for (const auto& finding : run_checkup()) {
			const int n = ++seen[finding.id];
			out << "finding." << finding.id << (n > 1 ? "." + std::to_string(n) : std::string()) << ": " << finding.severity << ' '
				<< one_line(finding.message) << '\n';
		}
	}
	return out.str();
}

bool Game_doctor::write_dump(const std::string& path, std::string* err, bool with_checkup) {
	const std::string text = dump_text(with_checkup);
	try {
		auto out = U7open_out(path.c_str(), true);
		if (!out || !out->good()) {
			if (err != nullptr) {
				*err = "Cannot write " + path;
			}
			return false;
		}
		*out << text;
		out->flush();
		if (!out->good()) {
			if (err != nullptr) {
				*err = "Error writing " + path;
			}
			return false;
		}
	} catch (const std::exception& e) {
		if (err != nullptr) {
			*err = std::string("Cannot write ") + path + ": " + e.what();
		}
		return false;
	}
	return true;
}

//
// Sessions.
//

void Game_doctor::on_game_restored() {}
