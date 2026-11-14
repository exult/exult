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

#include "files/U7file.h"    // IWYU pragma: keep
#include "files/databuf.h"
#include "files/msgfile.h"
#include "files/utils.h"
#include "fnames.h"
#include "game.h"
#include "gamewin.h"
#include "ucmachine.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iostream>
#include <map>
#include <optional>
#include <streambuf>

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
// Sessions.
//

void Game_doctor::on_game_restored() {}
