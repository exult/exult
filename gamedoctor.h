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

#include "singles.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

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

	// Called when a game has been restored.
	void on_game_restored();

	constexpr static size_t log_capacity = 500;

private:
	Game_doctor() = default;
	void load_flags();

	std::vector<Flag_info> flag_table;
	std::string            flag_table_key;    // Game and mod it was loaded for.

	mutable std::mutex           log_mutex;
	std::deque<std::string>      log_lines;
	std::unique_ptr<Log_tee_buf> cout_tee;
	std::unique_ptr<Log_tee_buf> cerr_tee;
};

#endif
