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

#ifndef GAMEDOCTOR_H
#define GAMEDOCTOR_H

#include "exult_constants.h"
#include "singles.h"
#include "tiles.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Actor;
class Game_object;
class Log_tee_buf;

/*
 *  The game state doctor: tools for players and mod authors to look at
 *  and repair a running game (named global flags, NPCs, items, check-up
 *  rules, state dumps). One instance for the whole program.
 */
class Game_doctor : public Game_singletons {
public:
	// One global flag with its name from the flag tables.
	struct Flag_info {
		unsigned    num = 0;
		std::string name;           // Mod name if the mod renames it.
		std::string alias;          // Base game name when the mod renames it.
		std::string description;    // From the 'descriptions' section.
	};

	// Outcome of a doctor action, with a message for the player.
	struct Action_result {
		bool        ok = false;
		std::string message;
	};

	// One item in an inventory listing.
	struct Inventory_row {
		int         shape    = 0;
		int         frame    = 0;
		int         quality  = 0;
		int         quantity = 0;
		std::string name;
	};

	// What the NPC browser shows about one NPC.
	struct Npc_info {
		int                        num = -1;
		std::string                name;
		bool                       alive    = true;
		bool                       on_map   = false;
		bool                       in_party = false;
		Tile_coord                 tile;    // Valid when on_map.
		int                        map      = -1;
		bool                       has_body = false;    // Dead with a body somewhere.
		Tile_coord                 body_tile;
		int                        body_map = -1;
		std::string                body_where;    // Containers holding the body, if any.
		int                        schedule = 0;
		std::string                schedule_name;
		int                        alignment = 0;
		std::string                alignment_name;
		std::vector<std::string>   flags;    // asleep, poisoned, ...
		int                        health   = 0;
		int                        strength = 0;
		std::vector<Inventory_row> inventory;
	};

	// Item finder query; c_any_* (-359) means any.
	struct Item_query {
		int         shape   = c_any_shapenum;
		int         frame   = c_any_framenum;
		int         quality = c_any_qual;
		std::string name;    // Case-insensitive substring of the item name.
	};

	// One item found in the world.
	struct Item_hit {
		std::weak_ptr<Game_object> obj;
		int                        shape    = 0;
		int                        frame    = 0;
		int                        quality  = 0;
		int                        quantity = 0;
		std::string                name;
		Tile_coord                 tile;    // Of the outermost owner.
		int                        map = -1;
		// Owners from the innermost outwards, ending with the NPC holding
		// it or "ground".
		std::vector<std::string> chain;
	};

	// One check-up rule: 'id|severity|kind|when|args|message'.
	struct Rule {
		struct Flag_test {
			bool        set = true;    // '+' or '-'.
			std::string flag;          // Number or name as written.
		};

		std::string                                      id;
		std::string                                      severity;    // error, warning, info.
		std::string                                      kind;        // item, npc, keys, orphans.
		std::vector<Flag_test>                           when;        // Empty means always.
		std::vector<std::pair<std::string, std::string>> args;
		std::string                                      message;
		std::string                                      source;    // file:line.

		std::string arg(std::string_view key, std::string_view fallback = {}) const;
	};

	// One check-up result.
	struct Finding {
		std::string id;
		std::string severity;
		std::string message;
		Tile_coord  tile{-1, -1, -1};    // Where to look, if known.
		int         map = -1;
		int         npc = -1;
	};

	static Game_doctor& get();
	~Game_doctor();
	Game_doctor(const Game_doctor&)            = delete;
	Game_doctor& operator=(const Game_doctor&) = delete;

	// Named flags. The table is loaded on first use for the current game
	// and mod: the game's files/global_flags resource, then the mod's
	// <PATCH>/global_flags.txt overlaid per number.
	const std::vector<Flag_info>& flags();
	const Flag_info*              find_flag(unsigned num);
	// Case-insensitive exact match on a name or alias; -1 if unknown.
	int flag_number(std::string_view name);
	// Flags whose number (decimal or 0x hex) equals the text, or whose
	// name, alias or description contains it (case-insensitive). An empty
	// text matches every named flag.
	std::vector<Flag_info> search_flags(std::string_view text);
	bool                   flag_value(unsigned num) const;
	bool                   set_flag(unsigned num, bool on);

	// Log capture: keeps the last lines written to std::cout and std::cerr.
	void                     install_log_capture();
	void                     remove_log_capture();
	std::vector<std::string> recent_log(size_t n = log_capacity) const;
	void                     add_log_line(std::string line);

	// NPC browser.
	Npc_info         npc_info(int num) const;
	std::vector<int> find_npcs(std::string_view text) const;
	Action_result    bring_npc(int num);           // Beside the Avatar.
	Action_result    goto_npc(int num);            // Teleport the party there.
	Action_result    revive_npc(int num);          // Resurrect a dead NPC.
	Action_result    restore_schedule(int num);    // Back to the usual schedule.
	Action_result    heal_npc(int num);            // Full health, no poison.

	// Item finder. scan_world() reads every superchunk of every loaded map
	// once; walk_world() then visits every object in them (going into
	// containers) and every NPC's inventory, each object once.
	void                  scan_world() const;
	void                  walk_world(const std::function<void(Game_object*)>& visit);
	std::vector<Item_hit> find_items(const Item_query& query, size_t limit = 500);
	Action_result         goto_item(const Item_hit& hit);
	Action_result         fetch_item(const Item_hit& hit);    // Into the Avatar's pack.

	// Check-up (gamedoctor_checkup.cc). Rules come from the game's
	// files/doctor_rules resource, then <PATCH>/doctor_rules.txt.
	const std::vector<Rule>& rules();
	std::vector<int>         rule_shapes();    // Every shape a rule names.
	std::vector<Finding>     run_checkup();

	static std::string              schedule_name(int type);
	static std::string              alignment_name(int align);
	static std::string              object_name(const Game_object* obj);
	static std::vector<std::string> owner_chain(const Game_object* obj);

	// Called when a game has been restored.
	void on_game_restored();

	constexpr static size_t log_capacity = 500;

private:
	Game_doctor() = default;
	void load_flags();

	void load_rules();

	std::vector<Flag_info> flag_table;
	std::string            flag_table_key;    // Game and mod it was loaded for.
	std::vector<Rule>      rule_table;
	std::string            rule_table_key;

	mutable std::mutex           log_mutex;
	std::deque<std::string>      log_lines;
	std::unique_ptr<Log_tee_buf> cout_tee;
	std::unique_ptr<Log_tee_buf> cerr_tee;
};

#endif
